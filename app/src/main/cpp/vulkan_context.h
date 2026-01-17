//
// Created by mj on 1/11/26.
//

#ifndef MYGAME_VULKAN_CONTEXT_H
#define MYGAME_VULKAN_CONTEXT_H

//// 안드로이드 전용 확장 기능을 활성화 (volk.h 전에 선언)
//#ifndef VK_USE_PLATFORM_ANDROID_KHR
//#define VK_USE_PLATFORM_ANDROID_KHR
//#endif
//// Vulkan 함수를 직접(정적) 호출하지 않고, volk를 통해 동적 로드 (volk.h 전에 선언)
//#ifndef VK_NO_PROTOTYPES
//#define VK_NO_PROTOTYPES
//#endif

#include "volk.h"
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <vector>

class VulkanContext {
public:
    explicit VulkanContext(struct android_app* app);
    ~VulkanContext();

    // Disable copying
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    bool initialize();

    VkInstance getInstance() const { return mInstance; }
    VkSurfaceKHR getSurface() const { return mSurface; }
    VkPhysicalDevice getPhysicalDevice() const { return mPhysicalDevice; }
    VkDevice getDevice() const { return mDevice; }
    VkQueue getGraphicsQueue() const { return mGraphicsQueue; }
    uint32_t getGraphicsQueueFamilyIndex() const { return mGraphicsQueueFamilyIndex; }

private:
    struct android_app* mApp;

    VkInstance mInstance = VK_NULL_HANDLE;
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mDevice = VK_NULL_HANDLE;
    VkQueue mGraphicsQueue = VK_NULL_HANDLE;
    uint32_t mGraphicsQueueFamilyIndex = 0;

    bool createInstance();
    bool createSurface();
    bool selectPhysicalDevice();
    bool createLogicalDevice();
};

#endif //MYGAME_VULKAN_CONTEXT_H
