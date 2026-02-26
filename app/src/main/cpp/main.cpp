#include <jni.h>

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <game-activity/GameActivity.h>

#include "Renderer.h"
#include "Log.h"

#include <vector>



extern "C" {

/*!
 * Handles commands sent to this Android application
 * @param pApp the app the commands are coming from
 * @param cmd the command to handle
 */
void handle_cmd(android_app *pApp, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            LOGI("APP_CMD_INIT_WINDOW");
            // A new window is created, associate a renderer with it. You may replace this with a
            // "game" class if that suits your needs. Remember to change all instances of userData
            // if you change the class here as a reinterpret_cast is dangerous this in the
            // android_main function and the APP_CMD_TERM_WINDOW handler case.
            if (!pApp->userData) {
                auto* renderer = new Renderer(pApp);
                renderer->initialize();
                pApp->userData = renderer;
            }
            break;
        case APP_CMD_TERM_WINDOW:
            LOGI("APP_CMD_TERM_WINDOW");
            // The window is being destroyed. Use this to clean up your userData to avoid leaking
            // resources.
            //
            // We have to check if userData is assigned just in case this comes in really quickly
            if (pApp->userData) {
                //
                auto *pRenderer = reinterpret_cast<Renderer *>(pApp->userData);
                pApp->userData = nullptr;
                delete pRenderer;
            }
            break;
        case APP_CMD_CONFIG_CHANGED:
            LOGI("APP_CMD_CONFIG_CHANGED");
            if (pApp->userData) {
                auto *pRenderer = reinterpret_cast<Renderer *>(pApp->userData);
                pRenderer->mFramebufferResized = true;
            }
            break;
        case APP_CMD_WINDOW_RESIZED:
            LOGI("APP_CMD_WINDOW_RESIZED");
            if (pApp->userData) {
                auto *pRenderer = reinterpret_cast<Renderer *>(pApp->userData);
                pRenderer->mFramebufferResized = true;
            }
            break;
        default:
            break;
    }
}

/*!
 * Enable the motion events you want to handle; not handled events are
 * passed back to OS for further processing. For this example case,
 * only pointer and joystick devices are enabled.
 *
 * @param motionEvent the newly arrived GameActivityMotionEvent.
 * @return true if the event is from a pointer or joystick device,
 *         false for all other input devices.
 */
bool motion_event_filter_func(const GameActivityMotionEvent *motionEvent) {
    auto sourceClass = motionEvent->source & AINPUT_SOURCE_CLASS_MASK;
    return (sourceClass == AINPUT_SOURCE_CLASS_POINTER ||
            sourceClass == AINPUT_SOURCE_CLASS_JOYSTICK);
}

/*!
 * This the main entry point for a native activity
 */
void android_main(struct android_app *pApp) {
    // Can be removed, useful to ensure your code is running
    LOGI("Welcome to android_main");

    // Register an event handler for Android events
    pApp->onAppCmd = handle_cmd;

    // Set input event filters (set it to NULL if the app wants to process all inputs).
    // Note that for key inputs, this example uses the default default_key_filter()
    // implemented in android_native_app_glue.c.
    android_app_set_motion_event_filter(pApp, motion_event_filter_func);

    float lastX = 0.0f, lastY = 0.0f;
    float lastPinchDist = 0.0f;

    // This sets up a typical game/event loop. It will run until the app is destroyed.
    do {
        // Process all pending events before running game logic.
        bool done = false;
        while (!done) {
            // 0 is non-blocking.
            int timeout = 0;
            int events;
            android_poll_source *pSource;
            int result = ALooper_pollOnce(timeout, nullptr, &events,
                                          reinterpret_cast<void**>(&pSource));
            switch (result) {
                case ALOOPER_POLL_TIMEOUT:
                    [[clang::fallthrough]];
                case ALOOPER_POLL_WAKE:
                    // No events occurred before the timeout or explicit wake. Stop checking for events.
                    done = true;
                    break;
                case ALOOPER_EVENT_ERROR:
                    LOGI("Looper event error");
                    break;
                case ALOOPER_POLL_CALLBACK:
                    break;
                default:
                    if (pSource) {
                        pSource->process(pApp, pSource);
                    }
            }
        }

        // Check if any user data is associated. This is assigned in handle_cmd
        if (pApp->userData) {
            // We know that our user data is a Renderer, so reinterpret cast it. If you change your
            // user data remember to change it here
            auto *pRenderer = reinterpret_cast<Renderer *>(pApp->userData);

            auto* inputBuffer = android_app_swap_input_buffers(pApp);
            if (inputBuffer) {
                // 1. 모션 이벤트(터치) 처리
                for (size_t i = 0; i < inputBuffer->motionEventsCount; i++) {
                    auto& motionEvent = inputBuffer->motionEvents[i];
                    int32_t action = motionEvent.action & AMOTION_EVENT_ACTION_MASK;
                    uint32_t pointerCount = motionEvent.pointerCount;
                    if (pointerCount >= 2) {
                        float x0 = GameActivityPointerAxes_getX(&motionEvent.pointers[0]);
                        float y0 = GameActivityPointerAxes_getY(&motionEvent.pointers[0]);
                        float x1 = GameActivityPointerAxes_getX(&motionEvent.pointers[1]);
                        float y1 = GameActivityPointerAxes_getY(&motionEvent.pointers[1]);

                        // 피타고라스 정리로 두 점 사이의 거리 계산
                        float dist = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));

                        if (action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                            lastPinchDist = dist;
                        } else if (action == AMOTION_EVENT_ACTION_MOVE) {
                            float delta = dist - lastPinchDist;
                            pRenderer->handlePinchZoom(delta);
                            lastPinchDist = dist;
                        }
                    } else if (pointerCount == 1) {
                        float x = GameActivityPointerAxes_getX(&motionEvent.pointers[0]);
                        float y = GameActivityPointerAxes_getY(&motionEvent.pointers[0]);
                        switch (action) {
                            case AMOTION_EVENT_ACTION_DOWN:
                                lastX = x;
                                lastY = y;
                                break;
                            case AMOTION_EVENT_ACTION_MOVE:
                                pRenderer->handleTouchDrag(x - lastX, y - lastY);
                                break;
                            case AMOTION_EVENT_ACTION_UP:
                                lastX = x;
                                lastY = y;
                                break;
                        }

                    }
                }
                android_app_clear_motion_events(inputBuffer); // 모션 이벤트 처리 완료
                android_app_clear_key_events(inputBuffer); // 키 이벤트 처리 완료
            }

            pRenderer->render();
        }
    } while (!pApp->destroyRequested);
}
}