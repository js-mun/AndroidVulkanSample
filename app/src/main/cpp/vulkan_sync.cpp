#include "vulkan_sync.h"
#include "Log.h"

VulkanSync::VulkanSync(VkDevice device, uint32_t maxFramesInFlight)
    : mDevice(device), mMaxFramesInFlight(maxFramesInFlight) {
}

VulkanSync::~VulkanSync() {
    for (size_t i = 0; i < mMaxFramesInFlight; i++) {
        if (mImageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
        }
        if (mRenderFinishedSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
        }
        if (mInFlightFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
        }
    }
}

bool VulkanSync::initialize() {
    mImageAvailableSemaphores.resize(mMaxFramesInFlight);
    mRenderFinishedSemaphores.resize(mMaxFramesInFlight);
    mInFlightFences.resize(mMaxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    
    VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < mMaxFramesInFlight; i++) {
        if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS) {
            LOGE("Failed to create synchronization objects for frame %zu", i);
            return false;
        }
    }

    return true;
}
