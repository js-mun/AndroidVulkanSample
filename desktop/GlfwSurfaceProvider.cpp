#include "GlfwSurfaceProvider.h"

#include <GLFW/glfw3.h>

GlfwSurfaceProvider::GlfwSurfaceProvider(GLFWwindow* window) : mWindow(window) {
}

std::vector<const char*> GlfwSurfaceProvider::getRequiredInstanceExtensions() const {
    uint32_t count = 0;
    const char** ext = glfwGetRequiredInstanceExtensions(&count);
    if (!ext || count == 0) {
        return {};
    }
    return std::vector<const char*>(ext, ext + count);
}

bool GlfwSurfaceProvider::createVulkanSurface(VkInstance instance, VkSurfaceKHR& outSurface) const {
    if (!mWindow) {
        return false;
    }
    return glfwCreateWindowSurface(instance, mWindow, nullptr, &outSurface) == VK_SUCCESS;
}

