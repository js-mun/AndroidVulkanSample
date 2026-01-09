//
// Created by mj on 1/9/26.
//

#ifndef MYGAME_RENDERER_H
#define MYGAME_RENDERER_H

#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif

#include "volk.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>


class Renderer {
public:
    explicit Renderer(android_app* app);
    virtual ~Renderer();

    bool initialize();


private:
    android_app* mApp;
    VkInstance mInstance;
    VkSurfaceKHR mSurface;

    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mDevice = VK_NULL_HANDLE;
    VkQueue mGraphicsQueue = VK_NULL_HANDLE;
    uint32_t mGraphicsQueueFamilyIndex = 0;
};

#endif //MYGAME_RENDERER_H
