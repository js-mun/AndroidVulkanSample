#pragma once

#include "volk.h"

#include <vector>

class ISurfaceProvider {
public:
    virtual ~ISurfaceProvider() = default;

    virtual std::vector<const char*> getRequiredInstanceExtensions() const = 0;
    virtual bool createVulkanSurface(VkInstance instance, VkSurfaceKHR& outSurface) const = 0;
};

