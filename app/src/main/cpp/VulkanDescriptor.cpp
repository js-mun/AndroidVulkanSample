#include "VulkanDescriptor.h"
#include "Log.h"

VulkanDescriptor::VulkanDescriptor(VkDevice device, uint32_t maxFramesInFlight)
    : mDevice(device), mMaxFramesInFlight(maxFramesInFlight) {}

VulkanDescriptor::~VulkanDescriptor() {
    if (mDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
    }
}

bool VulkanDescriptor::initialize(VkDescriptorSetLayout materialLayout,
                                  const std::vector<std::unique_ptr<VulkanTexture>>& textures) {
    if (!createDescriptorPool()) return false;
    if (!allocateDescriptorSets(materialLayout)) return false;
    updateDescriptorSets(textures);
    return true;
}

bool VulkanDescriptor::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
        LOGE("Failed to create material descriptor pool");
        return false;
    }
    return true;
}

bool VulkanDescriptor::allocateDescriptorSets(VkDescriptorSetLayout layout) {
    VkDescriptorSetLayout materialLayout = layout;

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = mDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &materialLayout;

    if (vkAllocateDescriptorSets(mDevice, &allocInfo, &mDescriptorSet) != VK_SUCCESS) {
        LOGE("Failed to allocate material descriptor sets");
        return false;
    }
    return true;
}

void VulkanDescriptor::updateDescriptorSets(const std::vector<std::unique_ptr<VulkanTexture>>& textures) {
    VkDescriptorImageInfo imageInfo{};
    if (!textures.empty()) {
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textures[0]->getImageView();
        imageInfo.sampler = textures[0]->getSampler();
    } else {
        LOGE("Material descriptor update failed: no texture");
        return;
    }

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = mDescriptorSet;
    write.dstBinding = 1; // material base texture
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(mDevice, 1, &write, 0, nullptr);
}
