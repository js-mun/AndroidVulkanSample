#pragma once

#include "volk.h"
#include "VulkanContext.h"

class ShadowResources {
public:
    explicit ShadowResources(VulkanContext* context);
    ~ShadowResources();

    bool initialize(VkRenderPass shadowRenderPass, VkFormat depthFormat,
                    uint32_t width = kDefaultWidth, uint32_t height = kDefaultHeight);
    void cleanup();
    bool recreate(VkRenderPass shadowRenderPass, VkFormat depthFormat,
                  uint32_t width = kDefaultWidth, uint32_t height = kDefaultHeight);

    VkFramebuffer getFramebuffer() const { return mFramebuffer; }
    VkImageView getDepthView() const { return mDepthView; }
    VkSampler getSampler() const { return mSampler; }
    VkExtent2D getExtent() const { return mExtent; }

private:
    static constexpr uint32_t kDefaultWidth = 1024;
    static constexpr uint32_t kDefaultHeight = 1024;

    VulkanContext* mContext = nullptr;
    VkExtent2D mExtent{};

    VkImage mDepthImage = VK_NULL_HANDLE;
    VmaAllocation mDepthAlloc = VK_NULL_HANDLE;
    VkImageView mDepthView = VK_NULL_HANDLE;
    VkSampler mSampler = VK_NULL_HANDLE;
    VkFramebuffer mFramebuffer = VK_NULL_HANDLE;
};
