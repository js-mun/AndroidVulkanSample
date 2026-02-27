#include "VulkanDescriptor.h"
#include "vulkan_types.h"
#include "Log.h"
#include <array>

VulkanDescriptor::VulkanDescriptor(VkDevice device, uint32_t maxFramesInFlight)
    : mDevice(device), mMaxFramesInFlight(maxFramesInFlight) {
}

VulkanDescriptor::~VulkanDescriptor() {
    if (mDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
    }
}

bool VulkanDescriptor::initialize(VkDescriptorSetLayout layout,
        const std::vector<std::unique_ptr<VulkanBuffer>>& uniformBuffers,
        const std::vector<std::unique_ptr<VulkanTexture>>& textures,
        VkImageView shadowView, VkSampler shadowSampler) {
    if (!createDescriptorPool(static_cast<uint32_t>(textures.size()))) return false;
    if (!allocateDescriptorSets(layout)) return false;
    updateDescriptorSets(uniformBuffers, textures, shadowView, shadowSampler);
    return true;
}

bool VulkanDescriptor::createDescriptorPool(uint32_t textureCount) {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    
    // 1. UBO용 풀 사이즈
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = mMaxFramesInFlight;
    
    // 2. 텍스처 샘플러용 풀 사이즈 (텍스처 개수만큼)
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = mMaxFramesInFlight * (std::max(1u, textureCount) + 1u);

    VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = mMaxFramesInFlight;

    if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
        LOGE("Failed to create descriptor pool");
        return false;
    }
    return true;
}

bool VulkanDescriptor::allocateDescriptorSets(VkDescriptorSetLayout layout) {
    std::vector<VkDescriptorSetLayout> layouts(mMaxFramesInFlight, layout);
    VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = mDescriptorPool;
    allocInfo.descriptorSetCount = mMaxFramesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    mDescriptorSets.resize(mMaxFramesInFlight);
    if (vkAllocateDescriptorSets(mDevice, &allocInfo, mDescriptorSets.data()) != VK_SUCCESS) {
        LOGE("Failed to allocate descriptor sets");
        return false;
    }
    return true;
}

void VulkanDescriptor::updateDescriptorSets(
        const std::vector<std::unique_ptr<VulkanBuffer>>& uniformBuffers,
        const std::vector<std::unique_ptr<VulkanTexture>>& textures,
        VkImageView shadowView, VkSampler shadowSampler) {
    for (size_t i = 0; i < mMaxFramesInFlight; i++) {
        std::vector<VkWriteDescriptorSet> descriptorWrites;

        // 1. UBO 업데이트 (Binding 0)
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = uniformBuffers[i]->getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet uboWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        uboWrite.dstSet = mDescriptorSets[i];
        uboWrite.dstBinding = 0;
        uboWrite.dstArrayElement = 0;
        uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo = &bufferInfo;
        descriptorWrites.push_back(uboWrite);

        // 2. 텍스처 업데이트 (Binding 1)
        // glTF 모델에 텍스처가 있는 경우 첫 번째 텍스처를 바인딩합니다.
        VkDescriptorImageInfo imageInfo = {};
        if (!textures.empty()) {
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = textures[0]->getImageView();
            imageInfo.sampler = textures[0]->getSampler();
        }

        VkWriteDescriptorSet samplerWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        samplerWrite.dstSet = mDescriptorSets[i];
        samplerWrite.dstBinding = 1;
        samplerWrite.dstArrayElement = 0;
        samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerWrite.descriptorCount = 1;
        samplerWrite.pImageInfo = &imageInfo;
        descriptorWrites.push_back(samplerWrite);

        // 3. Shadow 업데이트 (Binding 2)
        VkDescriptorImageInfo shadowInfo{};
        shadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        shadowInfo.imageView = shadowView;
        shadowInfo.sampler = shadowSampler;

        VkWriteDescriptorSet shadowWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        shadowWrite.dstSet = mDescriptorSets[i];
        shadowWrite.dstBinding = 2;
        shadowWrite.dstArrayElement = 0;
        shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        shadowWrite.descriptorCount = 1;
        shadowWrite.pImageInfo = &shadowInfo;
        descriptorWrites.push_back(shadowWrite);

        vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(descriptorWrites.size()),
                descriptorWrites.data(), 0, nullptr);
    }
}

void VulkanDescriptor::updateShadowMap(VkImageView shadowView, VkSampler shadowSampler) {
    for (size_t i = 0; i < mMaxFramesInFlight; i++) {
        VkDescriptorImageInfo shadowInfo{};
        shadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        shadowInfo.imageView = shadowView;
        shadowInfo.sampler = shadowSampler;

        VkWriteDescriptorSet shadowWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        shadowWrite.dstSet = mDescriptorSets[i];
        shadowWrite.dstBinding = 2;
        shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        shadowWrite.descriptorCount = 1;
        shadowWrite.pImageInfo = &shadowInfo;

        vkUpdateDescriptorSets(mDevice, 1, &shadowWrite, 0, nullptr);
    }
}

