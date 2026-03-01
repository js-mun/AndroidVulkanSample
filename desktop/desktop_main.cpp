#include <iostream>

#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "GlfwSurfaceProvider.h"
#include "DesktopAssetProvider.h"

namespace {
void framebufferResizeCallback(GLFWwindow* window, int, int) {
    auto* renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if (renderer) {
        renderer->mFramebufferResized = true;
    }
}
}

int main(int argc, char** argv) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }
    if (!glfwVulkanSupported()) {
        std::cerr << "GLFW Vulkan is not supported\n";
        glfwTerminate();
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "MyGame Desktop", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    const std::string assetRoot = (argc > 1) ? argv[1] : "../app/src/main/assets";
    DesktopAssetProvider assetProvider(assetRoot);
    GlfwSurfaceProvider surfaceProvider(window);
    Renderer renderer(surfaceProvider, assetProvider);
    glfwSetWindowUserPointer(window, &renderer);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    if (!renderer.initialize()) {
        std::cerr << "Renderer initialization failed\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        renderer.render();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

