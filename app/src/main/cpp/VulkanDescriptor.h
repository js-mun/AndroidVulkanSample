#pragma once

#include "volk.h"
#include "VulkanBuffer.h"
#include "VulkanTexture.h"
#include <vector>
#include <memory>

class VulkanDescriptor {
public:
    VulkanDescriptor(VkDevice device, uint32_t maxFramesInFlight);
    ~VulkanDescriptor();

    // Disable copying
    VulkanDescriptor(const VulkanDescriptor&) = delete;
    VulkanDescriptor& operator=(const VulkanDescriptor&) = delete;

    bool initialize(VkDescriptorSetLayout layout,
                    const std::vector<std::unique_ptr<VulkanBuffer>>& uniformBuffers,
                    const std::vector<std::unique_ptr<VulkanTexture>>& textures);

    VkDescriptorSet getSet(uint32_t index) const { return mDescriptorSets[index]; }

private:
    VkDevice mDevice;
    uint32_t mMaxFramesInFlight;

    VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> mDescriptorSets;

    bool createDescriptorPool(uint32_t textureCount);
    bool allocateDescriptorSets(VkDescriptorSetLayout layout);
    void updateDescriptorSets(const std::vector<std::unique_ptr<VulkanBuffer>>& uniformBuffers,
                              const std::vector<std::unique_ptr<VulkanTexture>>& textures);
};
