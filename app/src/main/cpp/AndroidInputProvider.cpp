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

    const auto now = std::chrono::steady_clock::now();
    constexpr auto kSingleTouchCooldown = std::chrono::milliseconds(100);

    auto* inputBuffer = android_app_swap_input_buffers(mApp);
    if (!inputBuffer) {
        return;
    }

    for (size_t i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto& motionEvent = inputBuffer->motionEvents[i];
        const int32_t action = motionEvent.action & AMOTION_EVENT_ACTION_MASK;
        const uint32_t pointerCount = motionEvent.pointerCount;

        if (pointerCount >= 2) {
            mWasPinching = true;
            mLastPinchTime = now;
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

            // 핀치가 끝나고 1손가락으로 전환되는 첫 프레임은 드래그 delta를 무시해
            // 카메라가 갑자기 튀는 현상을 막습니다.
            if (mWasPinching) {
                mLastX = x;
                mLastY = y;
                mWasPinching = false;
                continue;
            }

            if ((now - mLastPinchTime) < kSingleTouchCooldown) {
                mLastX = x;
                mLastY = y;
                continue;
            }

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
