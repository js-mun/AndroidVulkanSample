#pragma once

#include "volk.h"
#include "VulkanBuffer.h"
#include <memory>
#include <vector>

class GlobalDescriptor {
public:
    GlobalDescriptor(VkDevice device, uint32_t maxFramesInFlight, bool includeShadowBinding);
    ~GlobalDescriptor();

    bool initialize(VkDescriptorSetLayout globalLayout,
                    const std::vector<std::unique_ptr<VulkanBuffer>>& uniformBuffers,
                    VkImageView shadowView, VkSampler shadowSampler);

    void updateShadowMap(VkImageView shadowView, VkSampler shadowSampler);

    VkDescriptorSet getSet(uint32_t frameIndex) const { return mDescriptorSets[frameIndex]; }

private:
    VkDevice mDevice;
    uint32_t mMaxFramesInFlight;
    bool mIncludeShadowBinding;
    VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> mDescriptorSets;
};
