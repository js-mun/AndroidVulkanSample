#pragma once

#include "volk.h"
#include <vector>

class VulkanSync {
public:
    VulkanSync(VkDevice device, uint32_t maxFramesInFlight);
    ~VulkanSync();

    // Disable copying
    VulkanSync(const VulkanSync&) = delete;
    VulkanSync& operator=(const VulkanSync&) = delete;

    bool initialize();

    VkSemaphore getImageAvailableSemaphore(uint32_t index) const { return mImageAvailableSemaphores[index]; }
    VkSemaphore getRenderFinishedSemaphore(uint32_t index) const { return mRenderFinishedSemaphores[index]; }
    VkFence getInFlightFence(uint32_t index) const { return mInFlightFences[index]; }

private:
    VkDevice mDevice;
    uint32_t mMaxFramesInFlight;

    std::vector<VkSemaphore> mImageAvailableSemaphores;
    std::vector<VkSemaphore> mRenderFinishedSemaphores;
    std::vector<VkFence> mInFlightFences;
};
