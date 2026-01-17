#pragma once

#include "volk.h"
#include "vulkan_buffer.h"
#include "vulkan_command.h"
#include "vulkan_context.h"
#include "vulkan_pipeline.h"
#include "vulkan_swapchain.h"
#include "vulkan_sync.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

    uint32_t mCurrentFrame = 0;
    const int MAX_FRAMES_IN_FLIGHT = 2;

    VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> mDescriptorSets;

    std::unique_ptr<VulkanBuffer> mVertexBuffer;
    std::vector<std::unique_ptr<VulkanBuffer>> mUniformBuffers;

private:
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void updateUniformBuffer(uint32_t currentImage);
    void createVertexBuffer();
};
