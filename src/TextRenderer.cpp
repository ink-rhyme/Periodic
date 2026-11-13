#include "TextRenderer.h"

#include <GL/glew.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

const char* kVS = R"(#version 330 core
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 color;
uniform mat4 proj;
out vec2 vUv;
out vec4 vColor;
void main() {
    gl_Position = proj * vec4(pos, 0.0, 1.0);
    vUv = uv;
    vColor = color;
}
)";

const char* kFS = R"(#version 330 core
in vec2 vUv;
in vec4 vColor;
uniform sampler2D tex;
out vec4 fragColor;
void main() {
    float a = texture(tex, vUv).r;
    fragColor = vec4(vColor.rgb, vColor.a * a);
}
)";

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        fprintf(stderr, "[TextRenderer] shader error: %s\n", log);
    }
    return s;
}

// UTF-8 -> codepoints
std::vector<unsigned> decodeUtf8(const std::string& s) {
    std::vector<unsigned> out;
    size_t i = 0, n = s.size();
    while (i < n) {
        unsigned char c = s[i];
        unsigned cp = 0;
        int len = 1;
        if (c < 0x80)            { cp = c; len = 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
        else { ++i; continue; }
        if (i + len > n) break;
        for (int k = 1; k < len; ++k) {
            unsigned char cc = s[i + k];
            if ((cc & 0xC0) != 0x80) { len = 1; cp = c; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        out.push_back(cp);
        i += len;
    }
    return out;
}

void orthoMatrix(float w, float h, float m[16]) {
    std::memset(m, 0, 16 * sizeof(float));
    m[0] = 2.0f / w;
    m[5] = -2.0f / h;
    m[10] = -1.0f;
    m[12] = -1.0f;
    m[13] = 1.0f;
    m[15] = 1.0f;
}

} // namespace

TextRenderer::TextRenderer() = default;

TextRenderer::~TextRenderer() { shutdown(); }

std::string TextRenderer::findDefaultFont() {
    static const char* kCandidates[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJKsc-Regular.otf",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/usr/share/fonts/truetype/arphic/uming.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };
    for (const char* p : kCandidates) {
        FILE* f = fopen(p, "rb");
        if (f) { fclose(f); return p; }
    }
    return {};
}

bool TextRenderer::init(const std::string& fontPath) {
    FT_Library lib;
    if (FT_Init_FreeType(&lib)) {
        fprintf(stderr, "[TextRenderer] FT_Init_FreeType 失败\n");
        return false;
    }
    FT_Face face;
    if (FT_New_Face(lib, fontPath.c_str(), 0, &face)) {
        fprintf(stderr, "[TextRenderer] 无法加载字体: %s\n", fontPath.c_str());
        FT_Done_FreeType(lib);
        return false;
    }
    ftLib = lib;
    ftFace = face;

    GLuint vs = compileShader(GL_VERTEX_SHADER, kVS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFS);
    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        fprintf(stderr, "[TextRenderer] link error: %s\n", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    locProj = glGetUniformLocation(prog, "proj");
    locTex  = glGetUniformLocation(prog, "tex");

    // 共享字形图集
    glGenTextures(1, &atlas);
    glBindTexture(GL_TEXTURE_2D, atlas);
    std::vector<unsigned char> zero((size_t)kAtlas * kAtlas, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, kAtlas, kAtlas, 0, GL_RED, GL_UNSIGNED_BYTE, zero.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8 * 6 * 8192, nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
    glBindVertexArray(0);
    return true;
}

void TextRenderer::shutdown() {
    if (atlas) { glDeleteTextures(1, &atlas); atlas = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (prog) { glDeleteProgram(prog); prog = 0; }
    if (ftFace) { FT_Done_Face((FT_Face)ftFace); ftFace = nullptr; }
    if (ftLib) { FT_Done_FreeType((FT_Library)ftLib); ftLib = nullptr; }
    glyphs.clear();
    verts.clear();
}

void TextRenderer::beginFrame(int fbW, int fbH) {
    verts.clear();
    if (fbW == vpW && fbH == vpH && proj[0] != 0.0f) return;
    vpW = fbW; vpH = fbH;
    orthoMatrix((float)fbW, (float)fbH, proj);
}

TextRenderer::Glyph& TextRenderer::getGlyph(unsigned cp, int px) {
    uint64_t key = ((uint64_t)(unsigned)px << 32) | (uint64_t)cp;
    auto it = glyphs.find(key);
    if (it != glyphs.end()) return it->second;

    Glyph g;
    FT_Face face = (FT_Face)ftFace;
    FT_Set_Pixel_Sizes(face, 0, px);
    if (!FT_Get_Char_Index(face, cp) || FT_Load_Char(face, cp, FT_LOAD_RENDER)) {
        g.valid = false; g.advance = px * 0.5f;
        return glyphs.emplace(key, g).first->second;
    }
    FT_GlyphSlot slot = face->glyph;
    g.w = (int)slot->bitmap.width;
    g.h = (int)slot->bitmap.rows;
    g.bearingX = (float)slot->bitmap_left;
    g.bearingY = (float)slot->bitmap_top;
    g.advance = (float)(slot->advance.x >> 6);

    if (g.w > 0 && g.h > 0) {
        // 图集装不下时重置（正常体量远用不到 2048²）
        if (curX + g.w + 2 >= kAtlas) { curX = 2; curY += rowH + 2; rowH = 0; }
        if (curY + g.h + 2 >= kAtlas) { curX = 2; curY = 2; rowH = 0; }
        g.ax = curX; g.ay = curY;
        glBindTexture(GL_TEXTURE_2D, atlas);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (slot->bitmap.pitch == (int)slot->bitmap.width) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, curX, curY, g.w, g.h,
                            GL_RED, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
        } else {
            std::vector<unsigned char> tmp((size_t)g.w * g.h);
            for (int r0 = 0; r0 < g.h; ++r0)
                std::memcpy(&tmp[(size_t)r0 * g.w],
                            slot->bitmap.buffer + (size_t)r0 * slot->bitmap.pitch, (size_t)g.w);
            glTexSubImage2D(GL_TEXTURE_2D, 0, curX, curY, g.w, g.h,
                            GL_RED, GL_UNSIGNED_BYTE, tmp.data());
        }
        curX += g.w + 2;
        if (g.h > rowH) rowH = g.h;
    }
    return glyphs.emplace(key, g).first->second;
}

float TextRenderer::width(const std::string& utf8, int px) {
    float w = 0;
    for (unsigned cp : decodeUtf8(utf8)) w += getGlyph(cp, px).advance;
    return w;
}

void TextRenderer::draw(const std::string& utf8, float x, float y, int px,
                        float r, float g, float b, float a) {
    if (utf8.empty()) return;
    FT_Face face = (FT_Face)ftFace;
    FT_Set_Pixel_Sizes(face, 0, px);
    float asc = (float)face->size->metrics.ascender / 64.0f;
    float baseline = y + asc;

    float penX = x;
    for (unsigned cp : decodeUtf8(utf8)) {
        Glyph& gl = getGlyph(cp, px);
        if (!gl.valid) continue;
        if (gl.w > 0 && gl.h > 0) {
            float x0 = penX + gl.bearingX;
            float y0 = baseline - gl.bearingY;
            float x1 = x0 + (float)gl.w, y1 = y0 + (float)gl.h;
            float u0 = (float)gl.ax / kAtlas, v0 = (float)gl.ay / kAtlas;
            float u1 = (float)(gl.ax + gl.w) / kAtlas, v1 = (float)(gl.ay + gl.h) / kAtlas;
            float q[48] = {
                x0, y0, u0, v0, r, g, b, a,
                x1, y0, u1, v0, r, g, b, a,
                x1, y1, u1, v1, r, g, b, a,
                x0, y0, u0, v0, r, g, b, a,
                x1, y1, u1, v1, r, g, b, a,
                x0, y1, u0, v1, r, g, b, a,
            };
            verts.insert(verts.end(), q, q + 48);
        }
        penX += gl.advance;
    }
}

void TextRenderer::drawCentered(const std::string& utf8, float cx, float cy, int px,
                                float r, float g, float b, float a) {
    float w = width(utf8, px);
    FT_Face face = (FT_Face)ftFace;
    FT_Set_Pixel_Sizes(face, 0, px);
    float h = (float)(face->size->metrics.ascender - face->size->metrics.descender) / 64.0f;
    draw(utf8, cx - w * 0.5f, cy - h * 0.5f, px, r, g, b, a);
}

void TextRenderer::flush() {
    if (verts.empty()) return;
    size_t n = verts.size() / 8;
    glUseProgram(prog);
    glUniformMatrix4fv(locProj, 1, GL_FALSE, proj);
    glUniform1i(locTex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data());
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)n);
    glBindVertexArray(0);
    verts.clear();
}
