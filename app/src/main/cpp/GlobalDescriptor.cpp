#include "GlobalDescriptor.h"
#include "vulkan_types.h"

GlobalDescriptor::GlobalDescriptor(VkDevice device, uint32_t maxFramesInFlight)
    : mDevice(device), mMaxFramesInFlight(maxFramesInFlight) {}

GlobalDescriptor::~GlobalDescriptor() {
    if (mDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
    }
}

bool GlobalDescriptor::initialize(VkDescriptorSetLayout globalLayout,
                                  const std::vector<std::unique_ptr<VulkanBuffer>>& uniformBuffers,
                                  VkImageView shadowView, VkSampler shadowSampler) {
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = mMaxFramesInFlight;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = mMaxFramesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = mMaxFramesInFlight;

    if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
        return false;
    }

    std::vector<VkDescriptorSetLayout> layouts(mMaxFramesInFlight, globalLayout);
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = mDescriptorPool;
    allocInfo.descriptorSetCount = mMaxFramesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    mDescriptorSets.resize(mMaxFramesInFlight);
    if (vkAllocateDescriptorSets(mDevice, &allocInfo, mDescriptorSets.data()) != VK_SUCCESS) {
        return false;
    }

    for (uint32_t i = 0; i < mMaxFramesInFlight; ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i]->getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo shadowInfo{};
        shadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        shadowInfo.imageView = shadowView;
        shadowInfo.sampler = shadowSampler;

        VkWriteDescriptorSet writes[2]{};

        writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[0].dstSet = mDescriptorSets[i];
        writes[0].dstBinding = 0; // UBO
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &bufferInfo;

        writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[1].dstSet = mDescriptorSets[i];
        writes[1].dstBinding = 2; // Shadow map
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &shadowInfo;

        vkUpdateDescriptorSets(mDevice, 2, writes, 0, nullptr);
    }

    return true;
}

void GlobalDescriptor::updateShadowMap(VkImageView shadowView, VkSampler shadowSampler) {
    for (uint32_t i = 0; i < mMaxFramesInFlight; ++i) {
        VkDescriptorImageInfo shadowInfo{};
        shadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        shadowInfo.imageView = shadowView;
        shadowInfo.sampler = shadowSampler;

        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = mDescriptorSets[i];
        write.dstBinding = 2;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &shadowInfo;

        vkUpdateDescriptorSets(mDevice, 1, &write, 0, nullptr);
    }
}
