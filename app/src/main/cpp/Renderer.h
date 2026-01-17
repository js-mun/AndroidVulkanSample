#pragma once

#include "volk.h"
#include "vulkan_context.h"
#include "vulkan_buffer.h"
#include "vulkan_pipeline.h"

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
    std::unique_ptr<VulkanPipeline> mPipeline;

    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
    std::vector<VkImage> mSwapchainImages;
    VkFormat mSwapchainImageFormat;
    VkExtent2D mSwapchainExtent;
    VkSurfaceTransformFlagBitsKHR mSwapchainTransform; // 스왑체인 회전 상태 저장
    std::vector<VkImageView> mSwapchainImageViews;

    std::vector<VkFramebuffer> mSwapchainFramebuffers;

    VkCommandPool mCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mCommandBuffers;

    std::vector<VkSemaphore> mImageAvailableSemaphores;
    std::vector<VkSemaphore> mRenderFinishedSemaphores;
    std::vector<VkFence> mInFlightFences;
    uint32_t mCurrentFrame = 0;
    const int MAX_FRAMES_IN_FLIGHT = 2;

    VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> mDescriptorSets;

    std::unique_ptr<VulkanBuffer> mVertexBuffer;
    std::vector<std::unique_ptr<VulkanBuffer>> mUniformBuffers;

private:
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void cleanupSwapchain();
    void recreateSwapchain();

    void updateUniformBuffer(uint32_t currentImage);
    void createVertexBuffer();
};
