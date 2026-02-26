#pragma once

#include "volk.h"
#include "VulkanContext.h"

class VulkanTexture {
public:
    VulkanTexture(VulkanContext* context);
    ~VulkanTexture();

    // raw 이미지 데이터(tinygltf에서 읽은 것)를 GPU로 업로드
    bool loadFromMemory(const unsigned char* pixels, uint32_t width, uint32_t height, VkFormat format);

    VkImageView getImageView() const { return mTextureImageView; }
    VkSampler getSampler() const { return mTextureSampler; }

private:
    VulkanContext* mContext;
    
    VkImage mTextureImage = VK_NULL_HANDLE;
    VmaAllocation mTextureAllocation = VK_NULL_HANDLE;
    VkImageView mTextureImageView = VK_NULL_HANDLE;
    VkSampler mTextureSampler = VK_NULL_HANDLE;

    void createTextureImageView();
    void createTextureSampler();
};
