#include "VulkanTexture.h"
#include "VulkanBuffer.h"
#include "Log.h"

VulkanTexture::VulkanTexture(VulkanContext* context) : mContext(context) {
}

VulkanTexture::~VulkanTexture() {
    VkDevice device = mContext->getDevice();
    VmaAllocator allocator = mContext->getAllocator();
    if (mTextureSampler != VK_NULL_HANDLE) vkDestroySampler(device, mTextureSampler, nullptr);
    if (mTextureImageView != VK_NULL_HANDLE) vkDestroyImageView(device, mTextureImageView, nullptr);
    if (mTextureImage != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, mTextureImage, mTextureAllocation);
        mTextureImage = VK_NULL_HANDLE;
        mTextureAllocation = VK_NULL_HANDLE;
    }
}

bool VulkanTexture::loadFromMemory(const unsigned char* pixels, uint32_t width, uint32_t height, VkFormat format) {
    VkDeviceSize imageSize = width * height * 4; // RGBA 기준

    // 1. 스테이징 버퍼 생성 및 데이터 복사
    VulkanBuffer stagingBuffer(
        mContext->getAllocator(), imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );
    stagingBuffer.copyTo(pixels, imageSize);

    // 2. GPU 이미지 생성
    mContext->createImage(
        width, height, format, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        mTextureImage, mTextureAllocation
    );

    // 3. 레이아웃 전환: UNDEFINED -> TRANSFER_DST
    mContext->transitionImageLayout(mTextureImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 4. 버퍼에서 이미지로 복사
    mContext->copyBufferToImage(stagingBuffer.getBuffer(), mTextureImage, width, height);

    // 5. 레이아웃 전환: TRANSFER_DST -> SHADER_READ_ONLY
    mContext->transitionImageLayout(mTextureImage, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // 6. 이미지 뷰 및 샘플러 생성
    createTextureImageView();
    createTextureSampler();

    return true;
}

void VulkanTexture::createTextureImageView() {
    VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image = mTextureImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB; // AnimatedCube 기준
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(mContext->getDevice(), &viewInfo, nullptr, &mTextureImageView) != VK_SUCCESS) {
        LOGE("Failed to create texture image view");
    }
}

void VulkanTexture::createTextureSampler() {
    VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE; // 일단 비활성화
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(mContext->getDevice(), &samplerInfo, nullptr, &mTextureSampler) != VK_SUCCESS) {
        LOGE("Failed to create texture sampler");
    }
}
