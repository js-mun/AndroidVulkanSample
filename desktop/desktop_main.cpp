#include <iostream>
#include <memory>

#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "GlfwSurfaceProvider.h"
#include "DesktopAssetProvider.h"
#include "DesktopInputProvider.h"

namespace {
struct DesktopRuntime {
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<DesktopInputProvider> inputProvider;
};

void framebufferResizeCallback(GLFWwindow* window, int, int) {
    auto* runtime = reinterpret_cast<DesktopRuntime*>(glfwGetWindowUserPointer(window));
    if (runtime && runtime->renderer) {
        runtime->renderer->mFramebufferResized = true;
    }
}

void scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    auto* runtime = reinterpret_cast<DesktopRuntime*>(glfwGetWindowUserPointer(window));
    if (runtime && runtime->inputProvider) {
        runtime->inputProvider->onScroll(yoffset);
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

    DesktopRuntime runtime;
    runtime.renderer = std::make_unique<Renderer>(surfaceProvider, assetProvider);
    runtime.inputProvider = std::make_unique<DesktopInputProvider>(window);

    glfwSetWindowUserPointer(window, &runtime);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetScrollCallback(window, scrollCallback);

    if (!runtime.renderer->initialize()) {
        std::cerr << "Renderer initialization failed\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        runtime.inputProvider->processInput(*runtime.renderer);
        runtime.renderer->render();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
