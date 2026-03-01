#pragma once

#include "ISurfaceProvider.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>

class AndroidSurfaceProvider final : public ISurfaceProvider {
public:
    explicit AndroidSurfaceProvider(android_app* app);

    std::vector<const char*> getRequiredInstanceExtensions() const override;
    bool createVulkanSurface(VkInstance instance, VkSurfaceKHR& outSurface) const override;

private:
    android_app* mApp = nullptr;
};

