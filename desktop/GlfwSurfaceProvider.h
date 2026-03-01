#pragma once

#include "ISurfaceProvider.h"

struct GLFWwindow;

class GlfwSurfaceProvider final : public ISurfaceProvider {
public:
    explicit GlfwSurfaceProvider(GLFWwindow* window);

    std::vector<const char*> getRequiredInstanceExtensions() const override;
    bool createVulkanSurface(VkInstance instance, VkSurfaceKHR& outSurface) const override;

private:
    GLFWwindow* mWindow = nullptr;
};

