#include <jni.h>

#include <game-activity/native_app_glue/android_native_app_glue.h>

#include "AndroidAssetProvider.h"
#include "AndroidInputProvider.h"
#include "AndroidSurfaceProvider.h"
#include "IInputProvider.h"
#include "Renderer.h"
#include "Log.h"

#include <memory>



extern "C" {

struct AppRuntime {
    std::unique_ptr<AndroidSurfaceProvider> surfaceProvider;
    std::unique_ptr<AndroidAssetProvider> assetProvider;
    std::unique_ptr<IInputProvider> inputProvider;
    std::unique_ptr<Renderer> renderer;
};

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
                auto runtime = std::make_unique<AppRuntime>();
                runtime->surfaceProvider = std::make_unique<AndroidSurfaceProvider>(pApp);
                runtime->assetProvider = std::make_unique<AndroidAssetProvider>(
                        pApp->activity->assetManager);
                runtime->renderer = std::make_unique<Renderer>(
                        *runtime->surfaceProvider, *runtime->assetProvider);
                runtime->inputProvider = std::make_unique<AndroidInputProvider>(pApp);
                if (runtime->renderer->initialize()) {
                    pApp->userData = runtime.release();
                } else {
                    LOGE("Renderer initialization failed");
                }
            }
            break;
        case APP_CMD_TERM_WINDOW:
            LOGI("APP_CMD_TERM_WINDOW");
            // The window is being destroyed. Use this to clean up your userData to avoid leaking
            // resources.
            //
            // We have to check if userData is assigned just in case this comes in really quickly
            if (pApp->userData) {
                auto *runtime = reinterpret_cast<AppRuntime *>(pApp->userData);
                pApp->userData = nullptr;
                delete runtime;
            }
            break;
        case APP_CMD_CONFIG_CHANGED:
            LOGI("APP_CMD_CONFIG_CHANGED");
            if (pApp->userData) {
                auto *runtime = reinterpret_cast<AppRuntime *>(pApp->userData);
                runtime->renderer->mFramebufferResized = true;
            }
            break;
        case APP_CMD_WINDOW_RESIZED:
            LOGI("APP_CMD_WINDOW_RESIZED");
            if (pApp->userData) {
                auto *runtime = reinterpret_cast<AppRuntime *>(pApp->userData);
                runtime->renderer->mFramebufferResized = true;
            }
            break;
        default:
            break;
    }
}

/*!
 * This the main entry point for a native activity
 */
void android_main(struct android_app *pApp) {
    // Can be removed, useful to ensure your code is running
    LOGI("Welcome to android_main");

    // Register an event handler for Android events
    pApp->onAppCmd = handle_cmd;

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
            auto *runtime = reinterpret_cast<AppRuntime *>(pApp->userData);
            auto *pRenderer = runtime->renderer.get();

            if (runtime->inputProvider) {
                runtime->inputProvider->processInput(*pRenderer);
            }

            pRenderer->render();
        }
    } while (!pApp->destroyRequested);
}
}
