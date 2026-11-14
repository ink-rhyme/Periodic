// 元素周期表 — C++ / OpenGL 3.3 / GLFW / FreeType
// 视觉风格：深蓝科技星空底 + 半透明磨砂玻璃圆角卡片 + 细亮边框辉光
// 数据由 assets/elements_cn.csv (+ elements_en.csv) 运行时加载
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "ElementData.h"
#include "TextRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <tuple>
#include <vector>

// ---------------- 布局常量 ----------------
static const int   COLS = 18, ROWS = 10;
static const float CELL_W = 66, CELL_H = 64, GAP = 3;
static const float PITCH_X = CELL_W + GAP, PITCH_Y = CELL_H + GAP;
static const float TABLE_X = 30, TABLE_Y = 60;
static const float TABLE_W = COLS * PITCH_X - GAP;
static const float TABLE_H = ROWS * PITCH_Y - GAP;
static const float PANEL_X = TABLE_X + TABLE_W + 30;
static const float PANEL_W = 400;
static const float WIN_W   = PANEL_X + PANEL_W + 30;
static const float WIN_H   = 820;
static const float HEADER_H = 48;   // 顶部标题栏：标题与窗口按钮都在两个框上面

// ---------------- 主题色（深蓝科技 / 半透明磨砂玻璃） ----------------
static const float BG_TOP[3]    = {0.055f, 0.088f, 0.160f};
static const float BG_BOT[3]    = {0.010f, 0.020f, 0.046f};
static const float PANEL_TOP[4] = {0.105f, 0.150f, 0.252f, 0.74f};
static const float PANEL_BOT[4] = {0.042f, 0.070f, 0.135f, 0.62f};
static const float BORDER[4]    = {0.42f, 0.62f, 0.92f, 0.42f};
static const float GLOW[3]      = {0.28f, 0.56f, 1.00f};
static const float TEXT_HI[3]   = {0.94f, 0.96f, 1.00f};
static const float TEXT_MID[3]  = {0.62f, 0.72f, 0.90f};
static const float TEXT_DIM[3]  = {0.40f, 0.49f, 0.64f};
static const float GOLD[3]      = {1.00f, 0.78f, 0.36f};
static const float CYAN[3]      = {0.42f, 0.80f, 0.98f};

// ---------------- 着色器（片元内建磨砂噪点） ----------------
// 注意：屏幕坐标 y 向下，NDC 需要翻转 y
static const char* kVS = R"(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec4 aColor;
uniform vec2 uView;
out vec4 vColor;
void main(){
    vec2 ndc = vec2(aPos.x / uView.x * 2.0 - 1.0,
                    1.0 - aPos.y / uView.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vColor = aColor;
}
)";

static const char* kFS = R"(#version 330 core
in vec4 vColor;
out vec4 FragColor;
float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
void main() {
    float n = hash(gl_FragCoord.xy * 0.7) * 0.065 - 0.0325;
    vec4 c = vColor;
    c.rgb += n * min(c.a * 1.6, 1.0);
    FragColor = c;
}
)";

// ---------------- 全局状态 ----------------
static std::vector<float> g_batch;                 // pos2 + rgba4
static GLuint g_prog = 0, g_vao = 0, g_vbo = 0;
static GLint  g_locView = -1;
static float  g_viewW = WIN_W, g_viewH = WIN_H;
static float  g_LH    = WIN_H;   // 当前布局高度（最大化时窗口更高，多余高度给面板）
static TextRenderer g_text;

static int    g_selected = 1;   // 默认选中 H
static bool   g_iconMode = false;   // 图标导出模式：只画深空底 + 单个原子
static int    g_hoverCell = 0;
static ElementDB g_db;

// 窗口拖拽
static bool   g_dragging = false;
static double g_dragAbsX = 0.0, g_dragAbsY = 0.0;      // 按下时的全局光标位置
static int    g_dragWinX = 0, g_dragWinY = 0;          // 按下时的窗口位置

// 原子核屏幕位置（供悬浮窗命中判定）
static float  g_nucCx = 0.0f, g_nucCy = 0.0f, g_nucR = 0.0f;

// 悬浮窗触发配置：0 = 鼠标悬停，1 = 长按左键
static int    g_nucTriggerMode = 0;
static float  g_nucLongPressThresh = 0.5f;
static bool   g_nucMouseDown = false;
static double g_nucPressTime = 0.0;

static double g_lastCursorX = 0.0, g_lastCursorY = 0.0;

// ---------------- 基础绘图 ----------------
static inline void vtx(float x, float y, float r, float g, float b, float a)
{
    g_batch.push_back(x);
    g_batch.push_back(y);
    g_batch.push_back(r);
    g_batch.push_back(g);
    g_batch.push_back(b);
    g_batch.push_back(a);
}

static void drawQuad(float x, float y, float w, float h,
                     float r, float g, float b, float a)
{    vtx(x,     y,     r, g, b, a);
    vtx(x + w, y,     r, g, b, a);
    vtx(x + w, y + h, r, g, b, a);

    vtx(x,     y,     r, g, b, a);
    vtx(x + w, y + h, r, g, b, a);
    vtx(x,     y + h, r, g, b, a);
}

// 任意方向线段（画成细四边形，用于按钮图标）
static void drawLine(float x1, float y1, float x2, float y2, float wdt,
                     float r, float g, float b, float a)
{
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.0001f) return;
    float nx = -dy / len * wdt * 0.5f, ny = dx / len * wdt * 0.5f;
    vtx(x1 + nx, y1 + ny, r, g, b, a); vtx(x2 + nx, y2 + ny, r, g, b, a); vtx(x2 - nx, y2 - ny, r, g, b, a);
    vtx(x1 + nx, y1 + ny, r, g, b, a); vtx(x2 - nx, y2 - ny, r, g, b, a); vtx(x1 - nx, y1 - ny, r, g, b, a);
}

static void drawQuadGrad(float x, float y, float w, float h,
                         float r1, float g1, float b1, float a1,
                         float r2, float g2, float b2, float a2)
{
    vtx(x,     y,     r1, g1, b1, a1);
    vtx(x + w, y,     r1, g1, b1, a1);
    vtx(x + w, y + h, r2, g2, b2, a2);

    vtx(x,     y,     r1, g1, b1, a1);
    vtx(x + w, y + h, r2, g2, b2, a2);
    vtx(x,     y + h, r2, g2, b2, a2);
}

static void drawDisc(float cx, float cy, float rad,
                     float r, float g, float b, float a, int segs = 28)
{
    for (int i = 0; i < segs; ++i) {
        float a0 = (float)i / segs * 6.2831853f;
        float a1 = (float)(i + 1) / segs * 6.2831853f;
        vtx(cx, cy, r, g, b, a);
        vtx(cx + cosf(a0) * rad, cy + sinf(a0) * rad, r, g, b, a);
        vtx(cx + cosf(a1) * rad, cy + sinf(a1) * rad, r, g, b, a);
    }
}

static void drawRing(float cx, float cy, float rad, float th,
                     float r, float g, float b, float a, int segs = 64)
{
    float ri = rad - th * 0.5f;
    float ro = rad + th * 0.5f;
    for (int i = 0; i < segs; ++i) {
        float a0 = (float)i / segs * 6.2831853f;
        float a1 = (float)(i + 1) / segs * 6.2831853f;
        float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
        vtx(cx + c0 * ri, cy + s0 * ri, r, g, b, a);
        vtx(cx + c0 * ro, cy + s0 * ro, r, g, b, a);
        vtx(cx + c1 * ro, cy + s1 * ro, r, g, b, a);

        vtx(cx + c0 * ri, cy + s0 * ri, r, g, b, a);
        vtx(cx + c1 * ro, cy + s1 * ro, r, g, b, a);
        vtx(cx + c1 * ri, cy + s1 * ri, r, g, b, a);
    }
}

// 椭圆轨道环（rot 弧度）
static void drawEllipseRing(float cx, float cy, float ea, float eb,
                            float rot, float th,
                            float r, float g, float b, float a, int segs = 72)
{
    float cr = cosf(rot), sr = sinf(rot);
    float px = 0.0f, py = 0.0f;
    for (int i = 0; i <= segs; ++i) {
        float t = (float)i / segs * 6.2831853f;
        float lx = cosf(t) * ea, ly = sinf(t) * eb;
        float x = cx + lx * cr - ly * sr;
        float y = cy + lx * sr + ly * cr;

        if (i > 0) {
            float dx = x - px, dy = y - py;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 1e-4f) {
                float nx = -dy / len * th * 0.5f;
                float ny = dx / len * th * 0.5f;
                vtx(px + nx, py + ny, r, g, b, a);
                vtx(x + nx,  y + ny,  r, g, b, a);
                vtx(x - nx,  y - ny,  r, g, b, a);

                vtx(px + nx, py + ny, r, g, b, a);
                vtx(x - nx,  y - ny,  r, g, b, a);
                vtx(px - nx, py - ny, r, g, b, a);
            }
        }
        px = x; py = y;
    }
}

// 圆角矩形边界采样
static void roundPath(float x, float y, float w, float h, float rr,
                      std::vector<float>& pts, int seg)
{
    rr = std::min(rr, std::min(w, h) * 0.5f);
    if (rr < 0.0f) rr = 0.0f;
    pts.clear();
    if (rr <= 0.01f) {
        pts.push_back(x);     pts.push_back(y);
        pts.push_back(x + w); pts.push_back(y);
        pts.push_back(x + w); pts.push_back(y + h);
        pts.push_back(x);     pts.push_back(y + h);
        return;
    }
    pts.push_back(x + w - rr); pts.push_back(y);

    auto arc = [&](float ccx, float ccy, float a0, float a1) {
        for (int i = 0; i <= seg; ++i) {
            float t = a0 + (a1 - a0) * (float)i / seg;
            pts.push_back(ccx + cosf(t) * rr);
            pts.push_back(ccy + sinf(t) * rr);
        }
    };
    arc(x + w - rr, y + rr,     -1.5707963f, 0.0f);
    arc(x + w - rr, y + h - rr,  0.0f,       1.5707963f);
    arc(x + rr,     y + h - rr,  1.5707963f, 3.1415926f);
    arc(x + rr,     y + rr,      3.1415926f, 4.7123890f);
}

// 圆角矩形竖向渐变填充
static void fillRoundGrad(float x, float y, float w, float h, float rr,
                          float r1, float g1, float b1, float a1,
                          float r2, float g2, float b2, float a2)
{
    static std::vector<float> p;
    roundPath(x, y, w, h, rr, p, 6);

    auto mixCol = [&](float yy) {
        float t = (h > 0.0f) ? (yy - y) / h : 0.0f;
        t = std::max(0.0f, std::min(1.0f, t));
        return std::make_tuple(r1 + (r2 - r1) * t,
                               g1 + (g2 - g1) * t,
                               b1 + (b2 - b1) * t,
                               a1 + (a2 - a1) * t);
    };

    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    auto cc = mixCol(cy);
    float cr = std::get<0>(cc), cg = std::get<1>(cc), cb = std::get<2>(cc), ca = std::get<3>(cc);

    int n = (int)p.size() / 2;
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        auto c0 = mixCol(p[2 * i + 1]);
        auto c1 = mixCol(p[2 * j + 1]);
        vtx(cx, cy, cr, cg, cb, ca);
        vtx(p[2 * i], p[2 * i + 1], std::get<0>(c0), std::get<1>(c0), std::get<2>(c0), std::get<3>(c0));
        vtx(p[2 * j], p[2 * j + 1], std::get<0>(c1), std::get<1>(c1), std::get<2>(c1), std::get<3>(c1));
    }
}

static void fillRound(float x, float y, float w, float h, float rr,
                      float r, float g, float b, float a)
{
    fillRoundGrad(x, y, w, h, rr, r, g, b, a, r, g, b, a);
}

// 圆角矩形描边
static void strokeRound(float x, float y, float w, float h, float rr, float th,
                        float r, float g, float b, float a, int seg)
{
    static std::vector<float> p;
    roundPath(x, y, w, h, rr, p, seg);
    int n = (int)p.size() / 2;
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        float x0 = p[2 * i], y0 = p[2 * i + 1];
        float x1 = p[2 * j], y1 = p[2 * j + 1];
        float dx = x1 - x0, dy = y1 - y0;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1e-4f) continue;
        float nx = -dy / len * th * 0.5f;
        float ny = dx / len * th * 0.5f;

        vtx(x0 + nx, y0 + ny, r, g, b, a);
        vtx(x1 + nx, y1 + ny, r, g, b, a);
        vtx(x1 - nx, y1 - ny, r, g, b, a);

        vtx(x0 + nx, y0 + ny, r, g, b, a);
        vtx(x1 - nx, y1 - ny, r, g, b, a);
        vtx(x0 - nx, y0 - ny, r, g, b, a);
    }
}

// 外发光（多层描边）
static void glowRound(float x, float y, float w, float h, float rr,
                      float r, float g, float b, float strength)
{
    for (int i = 3; i >= 1; --i) {
        float sp = (float)i * 3.0f;
        float th = (float)i * 3.2f;
        strokeRound(x - sp, y - sp, w + sp * 2.0f, h + sp * 2.0f, rr + sp, th,
                    r, g, b, strength * (0.30f / (float)i), 10);
    }
}

// 立体感小球（质子 / 中子 / 电子通用）
static void drawSphere(float cx, float cy, float rad,
                       float r0, float g0, float b0, float alpha,
                       int segs = 20)
{
    drawDisc(cx, cy, rad * 1.35f, r0 * 0.9f, g0 * 0.9f, b0 * 0.9f, alpha * 0.16f, segs);

    float lx = cx - rad * 0.34f, ly = cy - rad * 0.34f;
    float hi_r = r0 + (1.0f - r0) * 0.42f;
    float hi_g = g0 + (1.0f - g0) * 0.42f;
    float hi_b = b0 + (1.0f - b0) * 0.42f;
    float dn_r = r0 * 0.42f, dn_g = g0 * 0.42f, dn_b = b0 * 0.42f;

    for (int i = 0; i < segs; ++i) {
        float a0 = (float)i / segs * 6.2831853f;
        float a1 = (float)(i + 1) / segs * 6.2831853f;
        vtx(lx, ly, hi_r, hi_g, hi_b, alpha);
        vtx(cx + cosf(a0) * rad, cy + sinf(a0) * rad, dn_r, dn_g, dn_b, alpha);
        vtx(cx + cosf(a1) * rad, cy + sinf(a1) * rad, dn_r, dn_g, dn_b, alpha);
    }
    drawRing(cx, cy, rad + 0.6f, 1.4f,
             std::min(1.0f, r0 * 1.2f), std::min(1.0f, g0 * 1.2f), std::min(1.0f, b0 * 1.2f),
             alpha * 0.35f, segs);
}

static float hash11(int i)
{
    float x = sinf((float)i * 127.1f + 311.7f) * 43758.5453f;
    return x - floorf(x);
}

// 星空背景
static void drawStarfield(float w, float h)
{
    for (int i = 0; i < 230; ++i) {
        float fx = hash11(i * 3 + 1);
        float fy = hash11(i * 3 + 2);
        float fs = hash11(i * 3 + 3);
        float x = fx * w;
        float y = fy * h;
        float a = 0.05f + fs * fs * 0.30f;
        float sz = (fs > 0.86f) ? 2.0f : 1.0f;
        drawQuad(x, y, sz, sz, 0.72f, 0.85f, 1.0f, a);
    }
    drawDisc(w * 0.46f, 40.0f, w * 0.30f, 0.25f, 0.48f, 0.95f, 0.030f, 40);
    drawDisc(w * 0.72f, 90.0f, w * 0.18f, 0.35f, 0.55f, 1.00f, 0.022f, 40);
}

// ---------------- 批处理提交 ----------------
static void flushBatch()
{
    if (g_batch.empty()) return;
    glUseProgram(g_prog);
    glUniform2f(g_locView, g_viewW, g_viewH);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(g_batch.size() * sizeof(float)),
                 g_batch.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(g_batch.size() / 6));
    g_batch.clear();
}

// ---------------- 布局 ----------------
static float cellX(int col) { return TABLE_X + col * PITCH_X; }
static float cellY(int row) { return TABLE_Y + row * PITCH_Y; }

static bool cellPlace(const Element& e, int& col, int& row)
{
    if (e.group >= 1 && e.group <= 18 && e.period >= 1 && e.period <= 7) {
        col = e.group - 1;
        row = e.period - 1;
        return true;
    }
    if (e.category == ElemCategory::Lanthanide) {
        col = (e.number - 57) + 2;
        row = 8;
        return true;
    }
    if (e.category == ElemCategory::Actinide) {
        col = (e.number - 89) + 2;
        row = 9;
        return true;
    }
    return false;
}

// ---------------- 元素格 ----------------
static void drawCell(float x, float y, const Element& e, bool selected, bool hovered)
{
    float c[3] = {0.5f, 0.5f, 0.5f};
    categoryColor(e.category, c);

    float top[3], bot[3];
    for (int i = 0; i < 3; ++i) {
        top[i] = c[i] + (1.0f - c[i]) * 0.26f;
        bot[i] = c[i] * 0.52f;
    }
    if (hovered) {
        top[0] += 0.14f; top[1] += 0.14f; top[2] += 0.14f;
        bot[0] += 0.10f; bot[1] += 0.10f; bot[2] += 0.10f;
    }

    fillRoundGrad(x, y, CELL_W, CELL_H, 5.0f,
                  std::min(1.0f, top[0]), std::min(1.0f, top[1]), std::min(1.0f, top[2]), 0.94f,
                  std::min(1.0f, bot[0]), std::min(1.0f, bot[1]), std::min(1.0f, bot[2]), 0.86f);

    strokeRound(x, y, CELL_W, CELL_H, 5.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.30f, 3);

    if (selected) {
        glowRound(x, y, CELL_W, CELL_H, 5.0f, GOLD[0], GOLD[1], GOLD[2], 0.85f);
        strokeRound(x, y, CELL_W, CELL_H, 5.0f, 2.0f, GOLD[0], GOLD[1], GOLD[2], 0.95f, 4);
    } else if (hovered) {
        strokeRound(x, y, CELL_W, CELL_H, 5.0f, 1.6f, 1.0f, 1.0f, 1.0f, 0.70f, 3);
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", e.number);
    g_text.draw(buf, x + 6.0f, y + 3.0f, 9, TEXT_HI[0], TEXT_HI[1], TEXT_HI[2], 0.88f);

    g_text.drawCentered(e.symbol, x + CELL_W * 0.5f, y + 26.0f, 18,
                        TEXT_HI[0], TEXT_HI[1], TEXT_HI[2], 1.0f);

    // 中文名与拼音合并成一行：氢(qīng)
    if (!e.nameCn.empty()) {
        char nb[96];
        if (!e.pinyin.empty()) snprintf(nb, sizeof(nb), "%s(%s)", e.nameCn.c_str(), e.pinyin.c_str());
        else snprintf(nb, sizeof(nb), "%s", e.nameCn.c_str());
        int px = g_text.width(nb, 10) > CELL_W - 6.0f ? 8 : 10;
        g_text.drawCentered(nb, x + CELL_W * 0.5f, y + 52.0f, px,
                            1.0f, 1.0f, 1.0f, 0.94f);
    }
}

static void drawPlaceholderCell(float x, float y, const char* label)
{
    fillRoundGrad(x, y, CELL_W, CELL_H, 5.0f,
                  0.16f, 0.20f, 0.30f, 0.55f,
                  0.09f, 0.12f, 0.20f, 0.45f);
    strokeRound(x, y, CELL_W, CELL_H, 5.0f, 1.0f, BORDER[0], BORDER[1], BORDER[2], 0.35f, 3);
    g_text.drawCentered(label, x + CELL_W * 0.5f, y + CELL_H * 0.5f, 10,
                        TEXT_MID[0], TEXT_MID[1], TEXT_MID[2], 0.95f);
}

// ---------------- 原子核：默认 1 质子 + 1 中子 ----------------
static void drawNucleus(float cx, float cy, float radius)
{
    g_nucCx = cx; g_nucCy = cy; g_nucR = radius;

    drawDisc(cx, cy, radius * 1.55f, 0.35f, 0.55f, 0.95f, 0.10f, 30);
    drawRing(cx, cy, radius, 1.4f, 0.55f, 0.72f, 1.0f, 0.20f, 40);

    float sr = radius * 0.46f;
    // 左：红质子    右：灰中子
    drawSphere(cx - sr * 0.66f, cy, sr, 0.94f, 0.24f, 0.20f, 1.0f);
    drawSphere(cx + sr * 0.66f, cy, sr, 0.70f, 0.71f, 0.76f, 1.0f);
}

// ---------------- 玻尔模型 ----------------
static void drawBohrModel(float cx, float cy, float R, const Element& e)
{
    std::vector<int> shells;
    int v = 0;
    bool inNum = false;
    for (char ch : e.shells) {
        if (ch >= '0' && ch <= '9') { v = v * 10 + (ch - '0'); inNum = true; }
        else if (inNum) { shells.push_back(v); v = 0; inNum = false; }
    }
    if (inNum) shells.push_back(v);
    if (shells.empty()) shells.push_back(2);

    int maxShown = std::min((int)shells.size(), 3);

    drawDisc(cx, cy, R * 1.18f, 0.20f, 0.38f, 0.78f, 0.10f, 40);

    for (int s = 0; s < maxShown; ++s) {
        float rot = (float)s * 1.0471976f + 0.35f;
        float ea = R * (1.0f - s * 0.17f);
        float eb = ea * 0.40f;
        float col[3];
        for (int k = 0; k < 3; ++k) col[k] = (s % 2 == 0) ? GOLD[k] : CYAN[k];

        drawEllipseRing(cx, cy, ea, eb, rot, 7.0f, col[0], col[1], col[2], 0.10f, 72);
        drawEllipseRing(cx, cy, ea, eb, rot, 2.4f, col[0], col[1], col[2], 0.90f, 72);

        int n = std::min(shells[s], 12);
        float crr = cosf(rot), srr = sinf(rot);
        for (int i = 0; i < n; ++i) {
            float t = (float)i / n * 6.2831853f + (float)s * 1.1f;
            float lx = cosf(t) * ea, ly = sinf(t) * eb;
            float x = cx + lx * crr - ly * srr;
            float y = cy + lx * srr + ly * crr;
            drawDisc(x, y, 4.2f, col[0], col[1], col[2], 0.28f, 12);
            drawDisc(x, y, 2.2f, 1.0f, 1.0f, 1.0f, 0.95f, 12);
        }
    }

    drawNucleus(cx, cy, R * 0.30f);

    if (g_iconMode) return;   // 图标模式不画电子排布文字，避免干扰图形

    // 电子排布文字放在原子模型正下方（暗底胶囊），避免压住电子轨道
    if (!e.shells.empty()) {
        float tw = g_text.width(e.shells, 11) + 16.0f;
        float tx = cx - tw * 0.5f, ty = cy + R * 0.72f;
        fillRound(tx, ty, tw, 17.0f, 8.5f, 0.02f, 0.05f, 0.12f, 0.62f);
        g_text.drawCentered(e.shells, cx, ty + 8.5f, 11,
                            TEXT_MID[0], TEXT_MID[1], TEXT_MID[2], 0.95f);
    }
}

// ---------------- 原子核悬浮窗（整层统一半透明） ----------------
static bool nucTooltipVisible(float mx, float my)
{
    if (g_nucR <= 0.0f) return false;
    float dx = mx - g_nucCx, dy = my - g_nucCy;
    bool onNucleus = (dx * dx + dy * dy) <= (g_nucR * 1.7f) * (g_nucR * 1.7f);

    if (g_nucTriggerMode == 0)
        return onNucleus;

    double now = glfwGetTime();
    return (g_nucMouseDown && onNucleus &&
            (now - g_nucPressTime) >= (double)g_nucLongPressThresh);
}

static void drawNucleusTooltip(const Element* e)
{
    if (!e) return;
    if (!nucTooltipVisible((float)g_lastCursorX, (float)g_lastCursorY)) return;

    // 先把已入队的内容（含文本批次）结算掉，让悬浮窗真正浮在最上层
    flushBatch();
    g_text.flush();

    const float O = 0.92f;               // 整层统一透明度
    const float TW = 272.0f, TH = 190.0f, RR = 14.0f;

    float tx = g_nucCx + g_nucR + 18.0f;
    float ty = g_nucCy - TH * 0.5f;

    // 原子核位于右侧详情面板内时，悬浮窗贴到面板左侧，避免遮挡面板文字
    if (g_nucCx > PANEL_X + 10.0f)
        tx = PANEL_X - 16.0f - TW;
    else if (tx + TW > WIN_W - 14.0f)
        tx = g_nucCx - g_nucR - 18.0f - TW;

    if (tx < 12.0f) tx = 12.0f;
    if (ty < 14.0f) ty = 14.0f;
    if (ty + TH > g_LH - 44.0f) ty = g_LH - 44.0f - TH;

    // 玻璃底：先压一层暗底（模拟磨砂背景虚化），再叠半透明玻璃面
    fillRound(tx, ty, TW, TH, RR, 0.020f, 0.035f, 0.070f, 0.62f * O);
    fillRoundGrad(tx, ty, TW, TH, RR,
                  0.125f, 0.165f, 0.265f, 0.94f * O,
                  0.050f, 0.075f, 0.140f, 0.88f * O);
    fillRoundGrad(tx + 1.0f, ty + 1.0f, TW - 2.0f, TH * 0.40f, RR - 2.0f,
                  1.0f, 1.0f, 1.0f, 0.060f * O,
                  1.0f, 1.0f, 1.0f, 0.0f);

    glowRound(tx, ty, TW, TH, RR, GOLD[0], GOLD[1], GOLD[2], 0.60f * O);
    strokeRound(tx, ty, TW, TH, RR, 1.6f, 0.96f, 0.80f, 0.45f, 0.82f * O, 8);

    // 左侧核子排列
    float ncx = tx + 68.0f, ncy = ty + TH * 0.56f;
    float nrad = 46.0f;
    drawDisc(ncx, ncy, nrad * 1.12f, 0.18f, 0.30f, 0.55f, 0.34f * O, 28);
    drawRing(ncx, ncy, nrad * 1.02f, 1.2f, 0.58f, 0.72f, 1.0f, 0.32f * O, 40);

    int protons = e->number;
    int neutrons = e->neutrons;
    int total = protons + neutrons;
    int shown = std::min(total, 26);

    std::vector<char> isProton(shown, 0);
    int pShown = std::min(shown, (int)((float)shown * (float)protons / (float)std::max(1, total)));
    for (int i = 0; i < pShown; ++i) isProton[i] = 1;
    for (int i = shown - 1; i > 0; --i) {
        int j = (int)(hash11(i * 13 + 7) * (float)(i + 1));
        if (j > i) j = i;
        std::swap(isProton[i], isProton[j]);
    }

    const float golden = 2.39996323f;
    float sr = 6.0f;
    for (int i = 0; i < shown; ++i) {
        float t = (float)i + 0.5f;
        float rr = nrad * 0.72f * sqrtf(t / (float)shown);
        float a = t * golden;
        float px = ncx + cosf(a) * rr;
        float py = ncy + sinf(a) * rr * 0.96f;
        if (isProton[i])
            drawSphere(px, py, sr, 0.96f, 0.20f, 0.17f, 0.97f * O, 14);
        else
            drawSphere(px, py, sr, 0.72f, 0.73f, 0.78f, 0.97f * O, 14);
    }

    // 右侧数值
    float lx = tx + 128.0f;
    auto row = [&](float yy, const char* label, int value,
                   float cr, float cg, float cb) {
        g_text.draw(label, lx, yy, 14, TEXT_MID[0], TEXT_MID[1], TEXT_MID[2], 0.92f * O);
        char vb[16];
        snprintf(vb, sizeof(vb), "%d", value);
        g_text.draw(vb, lx + 66.0f, yy, 17, 1.0f, 1.0f, 1.0f, 1.0f * O);
        drawSphere(lx + 128.0f, yy + 8.0f, 4.0f, cr, cg, cb, 0.92f * O, 12);
    };
    row(ty + 44.0f, "质子", protons, 0.96f, 0.20f, 0.17f);
    row(ty + 78.0f, "中子", neutrons, 0.72f, 0.73f, 0.78f);

    drawQuad(tx + 124.0f, ty + 112.0f, TW - 42.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.16f * O);

    g_text.draw("核子总数", lx, ty + 128.0f, 14, TEXT_MID[0], TEXT_MID[1], TEXT_MID[2], 0.92f * O);
    char tb[16];
    snprintf(tb, sizeof(tb), "%d", total);
    g_text.draw(tb, lx + 66.0f, ty + 126.0f, 20, GOLD[0], GOLD[1], GOLD[2], 1.0f * O);

    char title[32];
    snprintf(title, sizeof(title), "%s · 原子核", e->symbol.c_str());
    g_text.draw(title, tx + 16.0f, ty + 10.0f, 12, 1.0f, 1.0f, 1.0f, 0.62f * O);
    drawQuad(tx + 14.0f, ty + 28.0f, TW - 28.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.10f * O);

    // 结算悬浮窗自身内容
    flushBatch();
    g_text.flush();
}

// ---------------- 右侧详情面板 ----------------
static void drawDetails(const Element& e)
{
    float x = PANEL_X, y = HEADER_H;
    float w = PANEL_W;
    float h = (g_LH - 14.0f) - y;   // 与左侧主表底板上下对齐

    glowRound(x, y, w, h, 16.0f, GLOW[0], GLOW[1], GLOW[2], 0.55f);
    fillRoundGrad(x, y, w, h, 16.0f,
                  PANEL_TOP[0], PANEL_TOP[1], PANEL_TOP[2], PANEL_TOP[3],
                  PANEL_BOT[0], PANEL_BOT[1], PANEL_BOT[2], PANEL_BOT[3]);
    strokeRound(x, y, w, h, 16.0f, 1.4f, BORDER[0], BORDER[1], BORDER[2], BORDER[3], 8);

    float c[3] = {0.5f, 0.5f, 0.5f};
    categoryColor(e.category, c);
    fillRoundGrad(x + 18.0f, y + 12.0f, w - 36.0f, 3.0f, 1.5f,
                  c[0], c[1], c[2], 0.95f, c[0], c[1], c[2], 0.25f);

    g_text.draw(e.symbol, x + 24.0f, y + 30.0f, 52, TEXT_HI[0], TEXT_HI[1], TEXT_HI[2], 1.0f);
    // 中文名 + 拼音同一行：金(jīn)
    {
        g_text.draw(e.nameCn, x + 28.0f, y + 98.0f, 26,
                    TEXT_HI[0], TEXT_HI[1], TEXT_HI[2], 1.0f);
        if (!e.pinyin.empty()) {
            char pb[64];
            snprintf(pb, sizeof(pb), "(%s)", e.pinyin.c_str());
            float nw = g_text.width(e.nameCn, 26);
            g_text.draw(pb, x + 28.0f + nw + 7.0f, y + 105.0f, 14,
                        0.72f, 0.82f, 0.96f, 0.95f);
        }
    }
    g_text.draw(e.nameEn, x + 28.0f, y + 140.0f, 16, TEXT_MID[0], TEXT_MID[1], TEXT_MID[2], 0.95f);

    char buf[128];
    snprintf(buf, sizeof(buf), "%d  ·  %s", e.number, e.mass.c_str());
    g_text.draw(buf, x + 28.0f, y + 166.0f, 17, TEXT_MID[0], TEXT_MID[1], TEXT_MID[2], 0.92f);

    drawBohrModel(x + w - 132.0f, y + 122.0f, 92.0f, e);

    drawQuad(x + 22.0f, y + 224.0f, w - 44.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.14f);

    // 双列属性（贴近参考图的信息密度）
    float py = y + 242.0f;
    auto infoPair = [&](const char* k1, const char* val1,
                        const char* k2, const char* val2) {
        g_text.draw(k1, x + 26.0f, py, 15, TEXT_DIM[0], TEXT_DIM[1], TEXT_DIM[2], 0.95f);
        // 过长的值自动降一档字号，避免撞到右侧那一列
        int s1 = strlen(val1) > 12 ? 12 : 16;
        g_text.draw(val1, x + 104.0f, py, s1, TEXT_HI[0], TEXT_HI[1], TEXT_HI[2], 0.92f);
        g_text.draw(k2, x + 224.0f, py, 15, TEXT_DIM[0], TEXT_DIM[1], TEXT_DIM[2], 0.95f);
        g_text.draw(val2, x + 298.0f, py, 16, TEXT_HI[0], TEXT_HI[1], TEXT_HI[2], 0.92f);
        py += 52.0f;
    };

    char v1[96], v2[96], v3[96], v4[96], v5[96], v6[96], v7[96];
    infoPair("类别", categoryChinese(e.category), "区", e.block.empty() ? "-" : e.block.c_str());

    if (e.group == 0) snprintf(v1, sizeof(v1), "%d 周期 / f 区", e.period);
    else snprintf(v1, sizeof(v1), "%d 周期 / %d 族", e.period, e.group);
    infoPair("周期 · 族", v1, "常温状态", phaseChinese(e.phase));

    infoPair("电子排布", e.shells.empty() ? "-" : e.shells.c_str(),
             "电负性", (snprintf(v2, sizeof(v2), "%.2f", e.electronegativity), v2));

    infoPair("原子半径", (snprintf(v3, sizeof(v3), "%.0f pm", e.atomicRadius), v3),
             "密度", (snprintf(v4, sizeof(v4), "%.2f g/cm³", e.density), v4));

    infoPair("熔点", (snprintf(v5, sizeof(v5), "%.0f K", e.meltingK), v5),
             "沸点", (snprintf(v6, sizeof(v6), "%.0f K", e.boilingK), v6));

    infoPair("原子量", e.mass.c_str(),
             "中子数", (snprintf(v7, sizeof(v7), "%d", e.neutrons), v7));

    // 电子排布条（K L M N ...）——铺满面板下半部
    {
        float bx0 = x + 26.0f;
        float baseY = (g_LH - 14.0f) - 34.0f;   // 柱底基线（贴面板底部）
        float by0 = baseY - 176.0f;              // 小标题
        std::vector<int> sh;
        int vv = 0;
        bool inn = false;
        for (char ch : e.shells) {
            if (ch >= '0' && ch <= '9') { vv = vv * 10 + (ch - '0'); inn = true; }
            else if (inn) { sh.push_back(vv); vv = 0; inn = false; }
        }
        if (inn) sh.push_back(vv);

        g_text.draw("电子层分布", bx0, by0, 16,
                    TEXT_DIM[0], TEXT_DIM[1], TEXT_DIM[2], 0.95f);
        drawQuad(bx0, by0 + 24.0f, w - 60.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.10f);

        int maxCap = 0;
        for (int c : sh) maxCap = std::max(maxCap, c);

        int cnt = (int)sh.size() > 7 ? 7 : (int)sh.size();
        float usable = w - 60.0f;
        float step = cnt > 0 ? std::min(84.0f, usable / (float)cnt) : 84.0f;
        float barW = std::max(22.0f, step * 0.52f);
        float startX = bx0 + (usable - step * (float)cnt) * 0.5f;

        for (size_t i = 0; i < sh.size() && i < 7; ++i) {
            float cx0 = startX + (float)i * step;
            const char* names[] = {"K", "L", "M", "N", "O", "P", "Q"};
            g_text.draw(names[i], cx0, by0 + 40.0f, 14, TEXT_MID[0], TEXT_MID[1], TEXT_MID[2], 0.9f);

            float ratio = maxCap > 0 ? (float)sh[i] / (float)maxCap : 0.0f;
            float bh = 34.0f + 104.0f * ratio;
            float bx = cx0 + (step - barW) * 0.5f;
            fillRound(bx, baseY - bh, barW, bh, 5.0f, 0.42f, 0.72f, 0.98f, 0.55f);
            strokeRound(bx, baseY - bh, barW, bh, 5.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.28f, 3);

            char cb[8];
            snprintf(cb, sizeof(cb), "%d", sh[i]);
            g_text.drawCentered(cb, cx0 + step * 0.5f, baseY + 14.0f, 15,
                                TEXT_HI[0], TEXT_HI[1], TEXT_HI[2], 0.95f);
        }
    }
}

// ---------------- 自绘窗口按钮（最小化 / 最大化 / 关闭） ----------------
static const float WB_Y = 9.0f, WB_SIZE = 26.0f, WB_GAP = 6.0f;
static int g_btnHot = -1;

static float wbX(int i)
{
    return WIN_W - 16.0f - 3.0f * WB_SIZE - 2.0f * WB_GAP + (float)i * (WB_SIZE + WB_GAP);
}

static int winBtnHit(float mx, float my)
{
    if (my < WB_Y - 4.0f || my > WB_Y + WB_SIZE + 4.0f) return -1;
    for (int i = 0; i < 3; ++i) {
        if (mx >= wbX(i) - 2.0f && mx <= wbX(i) + WB_SIZE + 2.0f) return i;
    }
    return -1;
}

static void drawWindowButtons()
{
    for (int i = 0; i < 3; ++i) {
        float x = wbX(i), y = WB_Y;
        bool hot = (g_btnHot == i);

        if (hot && i == 2)
            fillRound(x, y, WB_SIZE, WB_SIZE, 6.0f, 0.72f, 0.18f, 0.18f, 0.85f);
        else
            fillRound(x, y, WB_SIZE, WB_SIZE, 6.0f,
                      hot ? 0.24f : 0.16f, hot ? 0.34f : 0.24f, hot ? 0.50f : 0.36f,
                      hot ? 0.85f : 0.55f);
        strokeRound(x, y, WB_SIZE, WB_SIZE, 6.0f, 1.0f, 1.0f, 1.0f, 1.0f, hot ? 0.55f : 0.22f, 3);

        float cx = x + WB_SIZE * 0.5f, cy = y + WB_SIZE * 0.5f;
        if (i == 0) {                                   // 最小化
            drawLine(cx - 5.0f, cy + 3.0f, cx + 5.0f, cy + 3.0f, 1.8f, 1, 1, 1, 0.92f);
        } else if (i == 1) {                            // 最大化
            strokeRound(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f, 1.5f, 1.5f, 1, 1, 1, 0.92f, 4);
        } else {                                        // 关闭
            drawLine(cx - 4.5f, cy - 4.5f, cx + 4.5f, cy + 4.5f, 1.8f, 1, 1, 1, 0.95f);
            drawLine(cx + 4.5f, cy - 4.5f, cx - 4.5f, cy + 4.5f, 1.8f, 1, 1, 1, 0.95f);
        }
    }
}

// ---------------- 图例 ----------------
static void drawLegend()
{
    struct Item { const char* name; ElemCategory cat; };
    Item items[] = {
        {"碱金属", ElemCategory::AlkaliMetal},
        {"碱土金属", ElemCategory::AlkalineEarth},
        {"过渡金属", ElemCategory::TransitionMetal},
        {"主族金属", ElemCategory::PostTransition},
        {"准金属", ElemCategory::Metalloid},
        {"非金属", ElemCategory::Nonmetal},
        {"卤素", ElemCategory::Halogen},
        {"稀有气体", ElemCategory::NobleGas},
        {"镧系", ElemCategory::Lanthanide},
        {"锕系", ElemCategory::Actinide},
    };

    float x = TABLE_X + 4.0f;
    float y = g_LH - 78.0f;   // 图例收进玻璃底板底部，留足内边距
    float cw = CELL_W * 2.0f - 4.0f;

    for (int i = 0; i < 10; ++i) {
        int colI = i % 5;
        int rowI = i / 5;
        float bx = x + colI * cw;
        float by = y + rowI * 18.0f;
        float cc[3];
        categoryColor(items[i].cat, cc);
        fillRound(bx, by, 11.0f, 11.0f, 3.0f, cc[0], cc[1], cc[2], 0.90f);
        strokeRound(bx, by, 11.0f, 11.0f, 3.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.28f, 3);
        g_text.draw(items[i].name, bx + 16.0f, by - 2.0f, 11,
                    TEXT_MID[0], TEXT_MID[1], TEXT_MID[2], 0.85f);
    }
}

// ---------------- 整帧 ----------------
static bool g_debugMarkers = false;

static void renderFrame()
{
    g_text.beginFrame((int)WIN_W, (int)g_LH);

    if (g_iconMode) {
        const float S = 512.0f, cx = S * 0.5f, cy = S * 0.5f, R = S * 0.5f;
        drawQuadGrad(0.0f, 0.0f, S, S,
                     BG_TOP[0], BG_TOP[1], BG_TOP[2], 1.0f,
                     BG_BOT[0], BG_BOT[1], BG_BOT[2], 1.0f);
        drawStarfield(S, S);
        // 细微冷光晕
        for (int i = 0; i < 8; ++i) {
            float t = (float)i / 8.0f;
            drawDisc(cx, cy, R * (1.0f - t * 0.55f), GLOW[0], GLOW[1], GLOW[2], 0.03f);
        }
        if (const Element* e = g_db.get(79))
            drawBohrModel(cx, cy, 150.0f, *e);
        flushBatch();
        g_text.flush();
        return;
    }

    if (g_debugMarkers) {
        drawQuad(0, 0, 30, 30, 1, 0, 0, 1);          // 左上 红
        drawQuad(WIN_W - 30, 0, 30, 30, 0, 1, 0, 1);  // 右上 绿
        drawQuad(0, WIN_H - 30, 30, 30, 0, 0, 1, 1);  // 左下 蓝
        drawQuad(WIN_W - 30, WIN_H - 30, 30, 30, 1, 1, 0, 1); // 右下 黄
        drawQuad(30, 48, CELL_W, CELL_H, 1, 1, 1, 1); // (30,48) 起点 白
        return;
    }


    // 标题：位于两个面板上方的独立标题栏正中
    float headCx = (TABLE_X - 14.0f + PANEL_X + PANEL_W) * 0.5f;
    g_text.drawCentered("元素周期表", headCx, 24.0f, 22,
                        TEXT_HI[0], TEXT_HI[1], TEXT_HI[2], 1.0f);
    g_text.drawCentered("PERIODIC TABLE OF ELEMENTS", headCx, 41.0f, 9,
                        TEXT_DIM[0], TEXT_DIM[1], TEXT_DIM[2], 0.85f);

    // 主表玻璃底板
    float bx = TABLE_X - 14.0f;
    float by = HEADER_H;
    float bw = TABLE_W + 28.0f;
    float bh = (g_LH - 14.0f) - by;   // 顶部留标题栏，底部留 14px
    glowRound(bx, by, bw, bh, 16.0f, GLOW[0], GLOW[1], GLOW[2], 0.45f);
    fillRoundGrad(bx, by, bw, bh, 16.0f,
                  0.085f, 0.125f, 0.215f, 0.70f,
                  0.030f, 0.052f, 0.100f, 0.58f);
    strokeRound(bx, by, bw, bh, 16.0f, 1.2f, BORDER[0], BORDER[1], BORDER[2], 0.35f, 8);

    // 周期编号
    for (int r = 0; r < 7; ++r) {
        char b[8];
        snprintf(b, sizeof(b), "%d", r + 1);
        g_text.draw(b, bx + 5.0f, cellY(r) + 24.0f, 10,
                    TEXT_DIM[0], TEXT_DIM[1], TEXT_DIM[2], 0.75f);
    }

    for (const Element& e : g_db.elements) {
        int col = 0, row = 0;
        if (!cellPlace(e, col, row)) continue;
        if (row >= ROWS) continue;
        drawCell(cellX(col), cellY(row), e,
                 e.number == g_selected, e.number == g_hoverCell);
    }

    drawPlaceholderCell(cellX(2), cellY(5), "57-71");
    drawPlaceholderCell(cellX(2), cellY(6), "89-103");

    const Element* cur = g_db.get(g_selected);
    if (cur) drawDetails(*cur);
    drawNucleusTooltip(cur);

    drawLegend();
    drawWindowButtons();

    char hint[256];
    snprintf(hint, sizeof(hint),
             "拖拽移动窗口  ·  ESC 取消选择  ·  Tab 切换原子核悬浮窗触发方式（当前：%s）  ·  Ctrl+Q 退出",
             (g_nucTriggerMode == 0 ? "鼠标悬停" : "长按左键 0.5s"));
    g_text.draw(hint, TABLE_X + 10.0f, g_LH - 36.0f, 11,
                TEXT_DIM[0], TEXT_DIM[1], TEXT_DIM[2], 0.85f);
}

// ---------------- 输入 ----------------
static int cellAt(float mx, float my)
{
    for (const Element& e : g_db.elements) {
        int col = 0, row = 0;
        if (!cellPlace(e, col, row)) continue;
        float x = cellX(col), y = cellY(row);
        if (mx >= x && mx <= x + CELL_W && my >= y && my <= y + CELL_H)
            return e.number;
    }
    return 0;
}

static void onCursorPos(GLFWwindow* win, double x, double y)
{
    int ww = 0, wh = 0;
    glfwGetWindowSize(win, &ww, &wh);
    // 与主循环的等比视口保持一致：按最小缩放铺满并居中，含黑边偏移
    float scX = (ww > 0) ? (float)ww / WIN_W : 1.0f;
    float scY = (wh > 0) ? (float)wh / WIN_H : 1.0f;
    float sc = scX < scY ? scX : scY;
    if (sc <= 0.0f) sc = 1.0f;
    float offX = (scY <= scX) ? (ww - WIN_W * sc) * 0.5f : 0.0f;
    float offY = 0.0f;   // 内容顶部对齐

    float lx = ((float)x - offX) / sc;
    float ly = ((float)y - offY) / sc;
    g_lastCursorX = lx;
    g_lastCursorY = ly;

    int hovered = cellAt(lx, ly);
    // 悬停只做高亮，不改变选中（选中只由鼠标左键点击决定）
    g_hoverCell = hovered;

    g_btnHot = winBtnHit(lx, ly);
}

static void onMouseButton(GLFWwindow* win, int button, int action, int mods)
{
    (void)mods;
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    if (action == GLFW_PRESS) {
        g_nucMouseDown = true;
        g_nucPressTime = glfwGetTime();

        // 窗口按钮：最小化 / 最大化(还原) / 关闭
        int btn = winBtnHit((float)g_lastCursorX, (float)g_lastCursorY);
        if (btn >= 0) {
            if (btn == 0) {
                glfwIconifyWindow(win);
            } else if (btn == 1) {
                // 最大化交给窗口管理器完成（自行计算铺满尺寸在 WSLg 下会算错屏
                // 幕尺寸导致窗口跑到屏幕外），画面比例由主循环的等比视口保证
                static bool maxed = false;
                if (!maxed) {
                    glfwSetWindowAspectRatio(win, GLFW_DONT_CARE, GLFW_DONT_CARE);
                    glfwMaximizeWindow(win);
                    maxed = true;
                } else {
                    glfwRestoreWindow(win);
                    glfwSetWindowAspectRatio(win, (int)WIN_W, (int)WIN_H);
                    maxed = false;
                }
            } else {
                glfwSetWindowShouldClose(win, 1);
            }
            return;
        }

        int hit = cellAt((float)g_lastCursorX, (float)g_lastCursorY);
        if (hit != 0) g_selected = hit;

        // 只有按在顶部标题条上才允许拖窗，避免每次点击都移动窗口；最大化时禁止拖动
        if (g_lastCursorY <= HEADER_H && !glfwGetWindowAttrib(win, GLFW_MAXIMIZED)) {
            int wwx = 0, wwy = 0;
            glfwGetWindowPos(win, &wwx, &wwy);
            double cx = 0, cy = 0;
            glfwGetCursorPos(win, &cx, &cy);
            g_dragWinX = wwx;
            g_dragWinY = wwy;
            g_dragAbsX = (double)wwx + cx;
            g_dragAbsY = (double)wwy + cy;
            g_dragging = true;
        }
    } else if (action == GLFW_RELEASE) {
        g_nucMouseDown = false;
        g_dragging = false;
    }
}

static void onKey(GLFWwindow* win, int key, int scancode, int action, int mods)
{
    (void)scancode;
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_ESCAPE) {
        g_selected = 0;
        g_hoverCell = 0;
    } else if (key == GLFW_KEY_TAB) {
        g_nucTriggerMode = (g_nucTriggerMode == 0) ? 1 : 0;
        printf("[Tooltip] 原子核悬浮窗触发方式 -> %s\n",
               (g_nucTriggerMode == 0 ? "鼠标悬停" : "长按左键"));
    } else if (key == GLFW_KEY_Q && (mods & GLFW_MOD_CONTROL)) {
        glfwSetWindowShouldClose(win, 1);
    }
}

// ---------------- shader 辅助 ----------------
static GLuint compileShader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        fprintf(stderr, "shader 编译失败: %s\n", log);
    }
    return s;
}

static void saveBMP(const char* path, int w, int h)
{
    std::vector<unsigned char> px((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_BGR, GL_UNSIGNED_BYTE, px.data());

    int rowSize = w * 3;
    int pad = (4 - (rowSize % 4)) % 4;
    int dataSize = (rowSize + pad) * h;
    FILE* f = fopen(path, "wb");
    if (!f) return;

    unsigned char hdr[54] = {0};
    hdr[0] = 'B'; hdr[1] = 'M';
    int fileSize = 54 + dataSize;
    memcpy(&hdr[2], &fileSize, 4);
    int off = 54; memcpy(&hdr[10], &off, 4);
    int dib = 40; memcpy(&hdr[14], &dib, 4);
    memcpy(&hdr[18], &w, 4);
    memcpy(&hdr[22], &h, 4);
    short planes = 1; memcpy(&hdr[26], &planes, 2);
    short bpp = 24; memcpy(&hdr[28], &bpp, 2);
    memcpy(&hdr[34], &dataSize, 4);
    fwrite(hdr, 1, 54, f);

    unsigned char zero[3] = {0, 0, 0};
    for (int y = 0; y < h; ++y) {
        fwrite(&px[(size_t)y * w * 3], 1, rowSize, f);
        if (pad > 0) fwrite(zero, 1, pad, f);
    }
    fclose(f);
}

// ---------------- main ----------------
#ifndef PERIODIC_VERSION
#define PERIODIC_VERSION "dev"
#endif
#ifndef PERIODIC_GIT_HASH
#define PERIODIC_GIT_HASH "nogit"
#endif

int main(int argc, char** argv)
{
    std::string shotPath;
    bool bench = false;
    bool demoTooltip = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v")) {
            printf("Periodic v%s (%s)\n", PERIODIC_VERSION, PERIODIC_GIT_HASH);
            return 0;
        }
        else if (!strcmp(argv[i], "--select") && i + 1 < argc) g_selected = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shotPath = argv[++i];
        else if (!strcmp(argv[i], "--bench")) bench = true;
        else if (!strcmp(argv[i], "--debug")) g_debugMarkers = true;
        else if (!strcmp(argv[i], "--tooltip")) demoTooltip = true;
        else if (!strcmp(argv[i], "--icon") && i + 1 < argc) {
            g_iconMode = true;
            shotPath = argv[++i];
        }
    }

    std::string err;
    if (!g_db.load("assets/elements_cn.csv", "assets/elements_en.csv", err) &&
        !g_db.load("../assets/elements_cn.csv", "../assets/elements_en.csv", err)) {
        fprintf(stderr, "加载元素数据失败: %s\n", err.c_str());
        return 1;
    }
    printf("Periodic v%s (%s) · 已加载 %zu 个元素\n",
           PERIODIC_VERSION, PERIODIC_GIT_HASH, g_db.elements.size());

    if (!glfwInit()) {
        fprintf(stderr, "glfwInit 失败\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // 先带边框创建（WSLg 软件渲染模式下无边框窗口无法被映射到 Windows），
    // 映射成功后立刻在运行时去掉边框，界面上仍然没有原生标题栏
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

    GLFWwindow* win = glfwCreateWindow((int)WIN_W, (int)WIN_H,
                                       "Periodic - 元素周期表", nullptr, nullptr);
    if (!win) {
        fprintf(stderr, "创建窗口失败\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(win);
    // WSLg（软件渲染 COPY MODE）无法映射无边框窗口，故先用带边框创建，
    // 待窗口被映射后再在运行时去掉边框；同时显式归位到可见区
    glfwShowWindow(win);
    glfwPollEvents();
    glfwSetWindowAttrib(win, GLFW_DECORATED, GLFW_FALSE);
    glfwSetWindowPos(win, 60, 60);
    glfwSwapInterval(1);

    // 只允许等比缩放，禁止拖成非原始比例
    glfwSetWindowAspectRatio(win, (int)WIN_W, (int)WIN_H);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "GLEW 初始化失败\n");
        return 1;
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    g_prog = glCreateProgram();
    glAttachShader(g_prog, compileShader(GL_VERTEX_SHADER, kVS));
    glAttachShader(g_prog, compileShader(GL_FRAGMENT_SHADER, kFS));
    glLinkProgram(g_prog);
    g_locView = glGetUniformLocation(g_prog, "uView");

    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    std::string font = TextRenderer::findDefaultFont();
    if (!g_text.init(font)) {
        fprintf(stderr, "字体加载失败: %s\n", font.c_str());
        return 1;
    }

    glfwSetCursorPosCallback(win, onCursorPos);
    glfwSetMouseButtonCallback(win, onMouseButton);
    glfwSetKeyCallback(win, onKey);

    int frames = 0;
    double t0 = glfwGetTime();
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();

        if (g_dragging && glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            double cx = 0, cy = 0;
            int wx = 0, wy = 0;
            glfwGetCursorPos(win, &cx, &cy);
            glfwGetWindowPos(win, &wx, &wy);
            // 用“当前窗位 + 光标”得到真实全局光标，再相对按下时的起点算总位移；
            // 位置始终由起点推出（不是逐帧累加），窗口移动不会反过来影响位移量，避免抖动
            double absX = wx + cx, absY = wy + cy;
            int nx = g_dragWinX + (int)llround(absX - g_dragAbsX);
            int ny = g_dragWinY + (int)llround(absY - g_dragAbsY);
            if (nx != wx || ny != wy)
                glfwSetWindowPos(win, nx, ny);
        }

        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(win, &fbW, &fbH);
        static bool printed = false;
        if (g_debugMarkers && !printed) {
            int ww = 0, wh = 0;
            glfwGetWindowSize(win, &ww, &wh);
            printf("WIN=%.0f,%.0f TABLE_W=%.0f PANEL_X=%.0f  window=%dx%d fb=%dx%d\n",
                   WIN_W, WIN_H, TABLE_W, PANEL_X, ww, wh, fbW, fbH);
            printed = true;
        }
        glViewport(0, 0, fbW, fbH);
        glClearColor(BG_BOT[0], BG_BOT[1], BG_BOT[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 背景铺满整个窗口
        g_viewW = (float)fbW; g_viewH = (float)fbH;
        drawQuadGrad(0.0f, 0.0f, (float)fbW, (float)fbH,
                     BG_TOP[0], BG_TOP[1], BG_TOP[2], 1.0f,
                     BG_BOT[0], BG_BOT[1], BG_BOT[2], 1.0f);
        drawStarfield((float)fbW, (float)fbH);
        flushBatch();

        // 等比缩放：元素尺寸按最小比例保持一致，多余的高度留给布局
        // （面板、图例随 g_LH 下移，窗口被拉高时界面铺满而不是留黑边）
        float scX = (float)fbW / WIN_W, scY = (float)fbH / WIN_H;
        float sc = scX < scY ? scX : scY;
        int vpW = (int)(WIN_W * sc), vpH = fbH;
        int vpX = 0;
        if (scY <= scX) {                 // 高度先到边：铺满高度，左右居中
            vpW = (int)(WIN_W * sc);
            vpX = (fbW - vpW) / 2;
            g_LH = WIN_H;
        } else {                          // 宽度先到边：铺满宽度，多余高度给布局
            vpW = fbW;
            vpX = 0;
            g_LH = (float)fbH / sc;
        }
        glViewport(vpX, 0, vpW, vpH);
        g_viewW = (float)vpW / sc; g_viewH = g_LH;

        renderFrame();
        flushBatch();
        g_text.flush();

        // 首帧时窗口可能刚被改过尺寸，先多渲染几帧再截图，避免读到旧帧缓冲
        static int shotWarmup = 0;
        if (!shotPath.empty() && shotWarmup < 2) {
            ++shotWarmup;
            glfwSwapBuffers(win);
            continue;
        }
        if (!shotPath.empty()) {
            if (demoTooltip && frames == 0) {
                frames = 1;
                g_lastCursorX = g_nucCx;
                g_lastCursorY = g_nucCy;
                glfwSwapBuffers(win);
                continue;
            }
            glFinish();
            saveBMP(shotPath.c_str(), fbW, fbH);
            glfwSwapBuffers(win);
            break;
        }

        glfwSwapBuffers(win);

        if (bench) {
            ++frames;
            if (frames >= 200) {
                double t1 = glfwGetTime();
                printf("200 帧 %.3f s, 平均 %.2f ms/帧 (%.1f FPS)\n",
                       t1 - t0, (t1 - t0) * 1000.0 / 200.0, 200.0 / (t1 - t0));
                break;
            }
        }
    }

    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
