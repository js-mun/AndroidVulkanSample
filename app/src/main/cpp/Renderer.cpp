//
// Created by mj on 1/9/26.
//

#include "Renderer.h"
#include "Log.h"

Renderer::Renderer(struct android_app *app) : mApp(app) {
}

bool Renderer::initialize() {
    // 1. volk 초기화 (Vulkan 로더 로드)
    if (volkInitialize() != VK_SUCCESS) {
        LOGE("Failed to initialize volk");
        return false;
    }

    // 2. Vulkan Instance 생성
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "MyGame";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    const char* extensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = extensions;

    if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS) {
        LOGE("Failed to create vkInstance");
        return false;
    }

    // 3. volk에 Instance 로드
    volkLoadInstance(mInstance);
    LOGI("Vulkan Initialized successfully up to Instance creation!");

    // 디버깅 로그
    LOGI("Vulkan Instance: %p", (void*)mInstance);
    LOGI("vkCreateAndroidSurfaceKHR pointer: %p", (void*)vkCreateAndroidSurfaceKHR);
    if (mApp->window == nullptr) {
        LOGE("ERROR: Native window is NULL. Cannot create surface.");
        return false;
    }
    if (vkCreateAndroidSurfaceKHR == nullptr) {
        LOGE("ERROR: vkCreateAndroidSurfaceKHR function pointer is NULL. Volk failed to load it.");
        return false;
    }

    // 4. Surface 생성 (Android App Glue 사용)
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.window = mApp->window;
    if (vkCreateAndroidSurfaceKHR(mInstance, &surfaceCreateInfo,
                                  nullptr, &mSurface) != VK_SUCCESS) {
        LOGI("Failed to create VkAndroidSurface");
        return false;
    }

    // 이후 단계 (간략화):
    // 5. Physical Device 선택 (vkEnumeratePhysicalDevices)
    // 6. Logical Device & Queue 생성 (vkCreateDevice)
    // 7. Swapchain 생성 (vkCreateSwapchainKHR)
    // 8. Render Pass & Pipeline 생성 (삼각형 쉐이더 포함)
    // 9. Command Buffer 기록 및 루프 시작

    LOGI("Vulkan Initialized successfully up to Surface creation!");
    return true;
}

Renderer::~Renderer() = default;

