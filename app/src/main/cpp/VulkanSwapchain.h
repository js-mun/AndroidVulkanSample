//
// Created by mj on 1/11/26.
//

#ifndef MYGAME_VULKANSWAPCHAIN_H
#define MYGAME_VULKANSWAPCHAIN_H

#include "volk.h"
#include "VulkanContext.h"
#include <vector>

class VulkanSwapchain {
public:
    VulkanSwapchain(VulkanContext* context);
    ~VulkanSwapchain();

    // Disable copying
    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    // 1단계: 스왑체인, 이미지뷰 및 깊이 버퍼 생성
    bool createSwapchainAndViews();
    // 2단계: 파이프라인의 렌더패스를 받아 프레임버퍼 생성
    bool createFramebuffers(VkRenderPass renderPass);

    void recreate(VkRenderPass renderPass);
    void cleanup();

    VkSwapchainKHR getSwapchain() const { return mSwapchain; }
    VkExtent2D getExtent() const { return mSwapchainExtent; }
    VkFormat getImageFormat() const { return mSwapchainImageFormat; }
    VkFormat getDepthFormat() const { return mDepthFormat; }
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

    // 깊이 버퍼 관련 변수
    VkImage mDepthImage = VK_NULL_HANDLE;
    VmaAllocation mDepthImageAllocation = VK_NULL_HANDLE;
    VkImageView mDepthImageView = VK_NULL_HANDLE;
    VkFormat mDepthFormat;

    bool createSwapchain();
    bool createImageViews();
    bool createDepthResources();
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
};

#endif //MYGAME_VULKANSWAPCHAIN_H
