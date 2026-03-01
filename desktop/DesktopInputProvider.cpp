#include "DesktopInputProvider.h"

#include <GLFW/glfw3.h>

#include "Renderer.h"

namespace {
constexpr float kMouseDragScale = 4.0f;
constexpr float kWheelZoomScale = 30.0f;
}

DesktopInputProvider::DesktopInputProvider(GLFWwindow* window) : mWindow(window) {
}

void DesktopInputProvider::processInput(Renderer& renderer) {
    if (!mWindow) {
        return;
    }

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(mWindow, &x, &y);

    const int leftPressed = glfwGetMouseButton(mWindow, GLFW_MOUSE_BUTTON_LEFT);
    if (leftPressed == GLFW_PRESS) {
        if (!mDragging) {
            mDragging = true;
            mLastX = x;
            mLastY = y;
        } else {
            const float dx = static_cast<float>(x - mLastX) * kMouseDragScale;
            const float dy = static_cast<float>(y - mLastY) * kMouseDragScale;
            renderer.handleTouchDrag(dx, dy);
            mLastX = x;
            mLastY = y;
        }
    } else {
        mDragging = false;
    }

    if (mPendingScroll != 0.0) {
        renderer.handlePinchZoom(static_cast<float>(mPendingScroll) * kWheelZoomScale);
        mPendingScroll = 0.0;
    }
}

void DesktopInputProvider::onScroll(double yoffset) {
    mPendingScroll += yoffset;
}

