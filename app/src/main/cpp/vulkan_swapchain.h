//
// Created by mj on 1/11/26.
//

#ifndef MYGAME_VULKAN_SWAPCHAIN_H
#define MYGAME_VULKAN_SWAPCHAIN_H

#include "volk.h"
#include "vulkan_context.h"
#include <vector>

class VulkanSwapchain {
public:
    VulkanSwapchain(VulkanContext* context);
    ~VulkanSwapchain();

    // Disable copying
    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    // 1단계: 스왑체인과 이미지뷰 생성 (포맷 결정됨)
    bool createSwapchainAndViews();
    // 2단계: 파이프라인의 렌더패스를 받아 프레임버퍼 생성
    bool createFramebuffers(VkRenderPass renderPass);

    void recreate(VkRenderPass renderPass);
    void cleanup();

    VkSwapchainKHR getSwapchain() const { return mSwapchain; }
    VkExtent2D getExtent() const { return mSwapchainExtent; }
    VkFormat getImageFormat() const { return mSwapchainImageFormat; }
    VkSurfaceTransformFlagBitsKHR getTransform() const { return mSwapchainTransform; }
    const std::vector<VkFramebuffer>& getFramebuffers() const { return mSwapchainFramebuffers; }
    uint32_t getImageCount() const { return static_cast<uint32_t>(mSwapchainImages.size()); }

private:
    VulkanContext* mContext;

    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
    VkFormat mSwapchainImageFormat;
    VkExtent2D mSwapchainExtent;
    VkSurfaceTransformFlagBitsKHR mSwapchainTransform;

    std::vector<VkImage> mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;
    std::vector<VkFramebuffer> mSwapchainFramebuffers;

    bool createSwapchain();
    bool createImageViews();
};

#endif //MYGAME_VULKAN_SWAPCHAIN_H
