#pragma once

#include "IInputProvider.h"

struct GLFWwindow;

class DesktopInputProvider final : public IInputProvider {
public:
    explicit DesktopInputProvider(GLFWwindow* window);

    void processInput(Renderer& renderer) override;
    void onScroll(double yoffset);

private:
    GLFWwindow* mWindow = nullptr;
    bool mDragging = false;
    double mLastX = 0.0;
    double mLastY = 0.0;
    double mPendingScroll = 0.0;
};

