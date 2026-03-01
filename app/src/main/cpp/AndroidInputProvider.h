#pragma once

#include "IInputProvider.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>

class AndroidInputProvider final : public IInputProvider {
public:
    explicit AndroidInputProvider(android_app* app);

    void processInput(Renderer& renderer) override;

private:
    android_app* mApp = nullptr;
    float mLastX = 0.0f;
    float mLastY = 0.0f;
    float mLastPinchDist = 0.0f;
};

