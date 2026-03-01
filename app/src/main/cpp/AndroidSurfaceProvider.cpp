#include "AndroidSurfaceProvider.h"

#include "Log.h"

AndroidSurfaceProvider::AndroidSurfaceProvider(android_app* app) : mApp(app) {
}

std::vector<const char*> AndroidSurfaceProvider::getRequiredInstanceExtensions() const {
    return {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };
}

bool AndroidSurfaceProvider::createVulkanSurface(VkInstance instance, VkSurfaceKHR& outSurface) const {
    if (!mApp || !mApp->window) {
        LOGE("Android window is null");
        return false;
    }

    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.window = mApp->window;
    if (vkCreateAndroidSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &outSurface) != VK_SUCCESS) {
        LOGE("Failed to create VkAndroidSurface");
        return false;
    }
    return true;
}

