#include "ShadowResources.h"
#include "Log.h"

ShadowResources::ShadowResources(VulkanContext* context) : mContext(context) {
}

ShadowResources::~ShadowResources() {
    cleanup();
}

bool ShadowResources::initialize(VkRenderPass shadowRenderPass, VkFormat depthFormat,
        uint32_t width, uint32_t height) {
    mExtent.width = width;
    mExtent.height = height;

    // 1. Shadow Map용 Depth Image 생성
    // 샘플링(SAMPLED_BIT)이 가능해야 나중에 메인 패스에서 읽을 수 있음
    if (!mContext->createImage(width, height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            mDepthImage, mDepthAlloc)) {
        LOGE("Failed to create Image");
        return false;
    }

    // 2. Depth Image View 생성
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = mDepthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(mContext->getDevice(), &viewInfo, nullptr, &mDepthView) != VK_SUCCESS) {
        LOGE("Failed to create shadow depth image view");
        cleanup();
        return false;
    }

    // 3. Shadow Framebuffer 생성
    // 섀도우 패스는 컬러 어태치먼트가 없으므로 깊이 이미지 뷰 하나만 연결
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = shadowRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &mDepthView;
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(mContext->getDevice(), &framebufferInfo,
                            nullptr, &mFramebuffer) != VK_SUCCESS) {
        LOGE("Failed to create shadow framebuffer");
        cleanup();
        return false;
    }

    // 4. Shadow Sampler 생성 (섀도우 비교 전용 설정)
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;

    if (vkCreateSampler(mContext->getDevice(), &samplerInfo, nullptr, &mSampler) != VK_SUCCESS) {
        LOGE("Failed to create shadow sampler");
        cleanup();
        return false;
    }

    return true;
}

void ShadowResources::cleanup() {
    VkDevice device = mContext->getDevice();
    if (mSampler) {
        vkDestroySampler(device, mSampler, nullptr);
        mSampler = VK_NULL_HANDLE;
    }
    if (mFramebuffer) {
        vkDestroyFramebuffer(device, mFramebuffer, nullptr);
        mFramebuffer = VK_NULL_HANDLE;
    }
    if (mDepthView) {
        vkDestroyImageView(device, mDepthView, nullptr);
        mDepthView = VK_NULL_HANDLE;
    }
    if (mDepthImage) {
        vmaDestroyImage(mContext->getAllocator(), mDepthImage, mDepthAlloc);
        mDepthImage = VK_NULL_HANDLE;
        mDepthAlloc = VK_NULL_HANDLE;
    }
}

bool ShadowResources::recreate(VkRenderPass shadowRenderPass, VkFormat depthFormat,
                  uint32_t width, uint32_t height) {
    cleanup();
    return initialize(shadowRenderPass, depthFormat, width, height);
}
