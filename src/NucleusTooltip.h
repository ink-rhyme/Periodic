#pragma once
#include "UIControl.h"
#include "ElementData.h"

// 原子核Tooltip触发模式
enum class NucleusTriggerMode {
    Hover,      // 默认：鼠标悬停
    LongPress   // 长按左键触发
};

// 原子核悬浮提示控件
class NucleusTooltip : public UIControl {
public:
    using Ptr = std::shared_ptr<NucleusTooltip>;

    NucleusTooltip() {
        type = "nucleus_tooltip";
        width = 200;
        height = 140;
        bgTop = UIColor(0.12f, 0.12f, 0.15f, 0.92f);
        bgBot = UIColor(0.08f, 0.08f, 0.10f, 0.92f);
        borderColor = UIColor(0.9f, 0.7f, 0.2f, 1.0f);
        borderWidth = 1.0f;
        fontSize = 13;
        visible = false;
    }

    // 绑定当前选中元素
    void bindElement(const Element* elem) {
        currentElem = elem;
    }

    // 设置触发模式
    void setTriggerMode(NucleusTriggerMode mode) {
        triggerMode = mode;
    }

    // 设置长按阈值（秒）
    void setLongPressThreshold(double sec) {
        longPressThresh = sec;
    }

    // 设置原子核在屏幕上的中心坐标和判定半径
    void setNucleusScreenPos(float cx, float cy, float hitR) {
        nucCx = cx;
        nucCy = cy;
        hitRadius = hitR;
    }

    // 每帧更新交互（主循环里调用）
    void update(float mouseX, float mouseY, double nowTime) {
        // 命中检测
        float dx = mouseX - nucCx;
        float dy = mouseY - nucCy;
        isHovering = (dx*dx + dy*dy <= hitRadius*hitRadius);

        switch (triggerMode) {
            case NucleusTriggerMode::Hover:
                visible = isHovering;
                break;
            case NucleusTriggerMode::LongPress:
                if (mouseLeftDown && isHovering) {
                    double hold = nowTime - pressStartTime;
                    visible = (hold >= longPressThresh);
                } else {
                    visible = false;
                    pressStartTime = 0;
                }
                break;
        }

        // 悬浮窗跟随鼠标，自动避让窗口边缘
        if (visible) {
            x = mouseX + 20;
            y = mouseY + 20;
            // 右边超出，往左弹
            if (x + width > UIRenderCtx->winW) x = mouseX - width - 20;
            // 下边超出，往上弹
            if (y + height > UIRenderCtx->winH) y = mouseY - height - 20;
        }
    }

    // GLFW鼠标按键回调转发
    void onMouseButton(int button, int action) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                mouseLeftDown = true;
                pressStartTime = glfwGetTime();
            } else if (action == GLFW_RELEASE) {
                mouseLeftDown = false;
            }
        }
    }

    // 传入全局渲染上下文指针
    void setRenderCtx(UIRenderContext* ctx) {
        UIRenderCtx = ctx;
    }

protected:
    // 重写绘制，展示原子核详细信息
    void onDraw(UIRenderContext& ctx) override {
        if (!currentElem) return;
        UIControl::onDraw(ctx);

        float curY = y + 20;
        // 标题
        ctx.drawText("原子核信息", x + 12, curY, fontSize + 1, UIColor(1.0f, 0.8f, 0.2f, 1.0f));
        curY += 25;

        char buf[128];
        // 质子数
        snprintf(buf, sizeof(buf), "质子：%d", currentElem->number);
        ctx.drawText(buf, x + 12, curY, fontSize, UIColor(1.0f, 0.3f, 0.3f, 1.0f));
        curY += 20;

        // 中子数
        snprintf(buf, sizeof(buf), "中子：%d", currentElem->neutrons);
        ctx.drawText(buf, x + 12, curY, fontSize, UIColor(0.6f, 0.6f, 0.6f, 1.0f));
        curY += 20;

        // 核子总数
        snprintf(buf, sizeof(buf), "核子总数：%d", currentElem->number + currentElem->neutrons);
        ctx.drawText(buf, x + 12, curY, fontSize, UIColor(0.9f, 0.9f, 0.9f, 1.0f));
    }

private:
    const Element* currentElem = nullptr;
    NucleusTriggerMode triggerMode = NucleusTriggerMode::Hover;
    double longPressThresh = 0.5;

    float nucCx = 0, nucCy = 0, hitRadius = 30;
    bool isHovering = false;
    bool mouseLeftDown = false;
    double pressStartTime = 0;

    UIRenderContext* UIRenderCtx = nullptr;
};
