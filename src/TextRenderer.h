#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// 基于 FreeType 的 UTF-8 文本渲染
// 字形一次光栅化进共享图集，整帧文本批量合并为一次 GL 绘制（切换元素不重复光栅化）
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    bool init(const std::string& fontPath);
    void shutdown();

    // 每帧开始时设置帧缓冲尺寸（缓存投影矩阵）
    void beginFrame(int fbW, int fbH);

    // 在 (x, y) 左上角绘制文本，px 为像素高度（只入队，不立即绘制）
    void draw(const std::string& utf8, float x, float y, int px,
              float r, float g, float b, float a);
    void drawCentered(const std::string& utf8, float cx, float cy, int px,
                      float r, float g, float b, float a);

    // 文本宽度（像素）
    float width(const std::string& utf8, int px);

    // 提交本帧所有文本（一次绘制调用）
    void flush();

    static std::string findDefaultFont();

private:
    struct Glyph {
        int ax = 0, ay = 0;        // 图集内位置
        int w = 0, h = 0;          // 位图尺寸
        float bearingX = 0, bearingY = 0;
        float advance = 0;
        bool valid = true;
    };

    Glyph& getGlyph(unsigned cp, int px);

    void* ftLib = nullptr;    // FT_Library
    void* ftFace = nullptr;   // FT_Face
    unsigned prog = 0, vao = 0, vbo = 0, atlas = 0;
    int locProj = -1, locTex = -1;
    int vpW = 0, vpH = 0;
    float proj[16] = {0};

    // 图集（整帧共享一个纹理，文本只绘制一次）
    static const int kAtlas = 2048;
    int curX = 2, curY = 2, rowH = 0;
    std::unordered_map<uint64_t, Glyph> glyphs;   // key = px<<32 | codepoint

    // 批量顶点: pos(2) + uv(2) + rgba(4)
    std::vector<float> verts;
};
