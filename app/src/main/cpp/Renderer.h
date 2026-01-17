//
// Created by mj on 1/9/26.
//

#ifndef MYGAME_RENDERER_H
#define MYGAME_RENDERER_H

//#ifndef VK_USE_PLATFORM_ANDROID_KHR
//#define VK_USE_PLATFORM_ANDROID_KHR
//#endif
//#ifndef VK_NO_PROTOTYPES
//#define VK_NO_PROTOTYPES
//#endif

#include "volk.h"
#include "vulkan_context.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <vector>
#include <android/asset_manager.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "vulkan_buffer.h"

struct UniformBufferObject {
    glm::mat4 mvp;
};

struct Vertex {
    glm::vec2 pos;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(1);
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        return attributeDescriptions;
    }
};

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

    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
    std::vector<VkImage> mSwapchainImages;
    VkFormat mSwapchainImageFormat;
    VkExtent2D mSwapchainExtent;
    VkSurfaceTransformFlagBitsKHR mSwapchainTransform; // 스왑체인 회전 상태 저장
    std::vector<VkImageView> mSwapchainImageViews;

    VkRenderPass mRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> mSwapchainFramebuffers;

    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mGraphicsPipeline = VK_NULL_HANDLE;

    VkCommandPool mCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mCommandBuffers;

    std::vector<VkSemaphore> mImageAvailableSemaphores;
    std::vector<VkSemaphore> mRenderFinishedSemaphores;
    std::vector<VkFence> mInFlightFences;
    uint32_t mCurrentFrame = 0;
    const int MAX_FRAMES_IN_FLIGHT = 2;

    VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
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

#endif //MYGAME_RENDERER_H
