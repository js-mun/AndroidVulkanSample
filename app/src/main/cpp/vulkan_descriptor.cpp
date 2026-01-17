#include "vulkan_descriptor.h"
#include "vulkan_types.h"
#include "Log.h"

VulkanDescriptor::VulkanDescriptor(VkDevice device, uint32_t maxFramesInFlight)
    : mDevice(device), mMaxFramesInFlight(maxFramesInFlight) {
}

VulkanDescriptor::~VulkanDescriptor() {
    if (mDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
    }
}

bool VulkanDescriptor::initialize(VkDescriptorSetLayout layout, const std::vector<std::unique_ptr<VulkanBuffer>>& uniformBuffers) {
    if (!createDescriptorPool()) return false;
    if (!allocateDescriptorSets(layout)) return false;
    updateDescriptorSets(uniformBuffers);
    return true;
}

bool VulkanDescriptor::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = mMaxFramesInFlight;

    VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
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

void VulkanDescriptor::updateDescriptorSets(const std::vector<std::unique_ptr<VulkanBuffer>>& uniformBuffers) {
    for (size_t i = 0; i < mMaxFramesInFlight; i++) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = uniformBuffers[i]->getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        descriptorWrite.dstSet = mDescriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(mDevice, 1, &descriptorWrite, 0, nullptr);
    }
}
