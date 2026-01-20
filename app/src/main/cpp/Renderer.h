#pragma once

#include "volk.h"
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Camera.h"
#include "VulkanBuffer.h"
#include "VulkanCommand.h"
#include "VulkanContext.h"
#include "VulkanDescriptor.h"
#include "VulkanMesh.h"
#include "VulkanPipeline.h"
#include "VulkanSwapchain.h"
#include "VulkanSync.h"

class Renderer {
public:
    explicit Renderer(android_app* app);
    virtual ~Renderer();

    bool initialize();
    void render();
    bool mFramebufferResized = false;


private:
    android_app* mApp;
    std::unique_ptr<VulkanContext> mContext;
    std::unique_ptr<VulkanSwapchain> mSwapchain;
    std::unique_ptr<VulkanPipeline> mPipeline;
    std::unique_ptr<VulkanSync> mSync;
    std::unique_ptr<VulkanCommand> mCommand;
    std::unique_ptr<VulkanDescriptor> mDescriptor;

    std::unique_ptr<VulkanMesh> mMesh;

    std::unique_ptr<Camera> mCamera;

    uint32_t mCurrentFrame = 0;
    const int MAX_FRAMES_IN_FLIGHT = 2;

    std::vector<std::unique_ptr<VulkanBuffer>> mUniformBuffers;

private:
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void updateUniformBuffer(uint32_t currentImage);
    void createVertexBuffer();
};
