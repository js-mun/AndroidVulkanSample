#include "AndroidInputProvider.h"

#include <game-activity/GameActivity.h>
#include <cmath>

#include "Renderer.h"

namespace {
bool motionEventFilter(const GameActivityMotionEvent* motionEvent) {
    const auto sourceClass = motionEvent->source & AINPUT_SOURCE_CLASS_MASK;
    return (sourceClass == AINPUT_SOURCE_CLASS_POINTER ||
            sourceClass == AINPUT_SOURCE_CLASS_JOYSTICK);
}
}

AndroidInputProvider::AndroidInputProvider(android_app* app) : mApp(app) {
    if (mApp) {
        android_app_set_motion_event_filter(mApp, motionEventFilter);
    }
}

void AndroidInputProvider::processInput(Renderer& renderer) {
    if (!mApp) {
        return;
    }

    auto* inputBuffer = android_app_swap_input_buffers(mApp);
    if (!inputBuffer) {
        return;
    }

    for (size_t i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto& motionEvent = inputBuffer->motionEvents[i];
        const int32_t action = motionEvent.action & AMOTION_EVENT_ACTION_MASK;
        const uint32_t pointerCount = motionEvent.pointerCount;

        if (pointerCount >= 2) {
            const float x0 = GameActivityPointerAxes_getX(&motionEvent.pointers[0]);
            const float y0 = GameActivityPointerAxes_getY(&motionEvent.pointers[0]);
            const float x1 = GameActivityPointerAxes_getX(&motionEvent.pointers[1]);
            const float y1 = GameActivityPointerAxes_getY(&motionEvent.pointers[1]);
            const float dist = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));

            if (action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                mLastPinchDist = dist;
            } else if (action == AMOTION_EVENT_ACTION_MOVE) {
                renderer.handlePinchZoom(dist - mLastPinchDist);
                mLastPinchDist = dist;
            }
        } else if (pointerCount == 1) {
            const float x = GameActivityPointerAxes_getX(&motionEvent.pointers[0]);
            const float y = GameActivityPointerAxes_getY(&motionEvent.pointers[0]);
            switch (action) {
                case AMOTION_EVENT_ACTION_DOWN:
                    mLastX = x;
                    mLastY = y;
                    break;
                case AMOTION_EVENT_ACTION_MOVE:
                    renderer.handleTouchDrag(x - mLastX, y - mLastY);
                    mLastX = x;
                    mLastY = y;
                    break;
                case AMOTION_EVENT_ACTION_UP:
                    mLastX = x;
                    mLastY = y;
                    break;
                default:
                    break;
            }
        }
    }

    android_app_clear_motion_events(inputBuffer);
    android_app_clear_key_events(inputBuffer);
}

