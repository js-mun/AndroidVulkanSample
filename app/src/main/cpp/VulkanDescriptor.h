#pragma once

#include "volk.h"
#include "VulkanTexture.h"
#include <vector>
#include <memory>

class VulkanDescriptor {
public:
    VulkanDescriptor(VkDevice device, uint32_t maxFramesInFlight);
    ~VulkanDescriptor();

    VulkanDescriptor(const VulkanDescriptor&) = delete;
    VulkanDescriptor& operator=(const VulkanDescriptor&) = delete;

    // material set only: binding 1 (base texture)
    bool initialize(VkDescriptorSetLayout materialLayout,
                    const std::vector<std::unique_ptr<VulkanTexture>>& textures);

    VkDescriptorSet getSet() const { return mDescriptorSet; }

private:
    VkDevice mDevice;
    uint32_t mMaxFramesInFlight;

    VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet mDescriptorSet = VK_NULL_HANDLE;

    bool createDescriptorPool();
    bool allocateDescriptorSets(VkDescriptorSetLayout layout);
    void updateDescriptorSets(const std::vector<std::unique_ptr<VulkanTexture>>& textures);
};
