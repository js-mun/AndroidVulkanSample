// VulkanBuffer.cpp
#include "vulkan_buffer.h"
#include "vulkan_utils.h"
#include "Log.h"
#include <cstring>

VulkanBuffer::VulkanBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                           VkDeviceSize size, VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags properties)
        : mDevice(device), mSize(size) {

    VulkanUtils::createBuffer(mDevice, physicalDevice, mSize, usage, properties, mBuffer, mMemory);
}

VulkanBuffer::~VulkanBuffer() {
    if (mMappedData) unmap();
    if (mBuffer != VK_NULL_HANDLE) vkDestroyBuffer(mDevice, mBuffer, nullptr);
    if (mMemory != VK_NULL_HANDLE) vkFreeMemory(mDevice, mMemory, nullptr);
}

void* VulkanBuffer::map() {
    if (!mMappedData) {
        VkResult result = vkMapMemory(mDevice, mMemory, 0, mSize, 0, &mMappedData);
        if (result != VK_SUCCESS) {
            LOGE("Failed to map buffer memory! (Result: %d)", result);
            return nullptr;
        }
    }
    return mMappedData;
}

void VulkanBuffer::unmap() {
    if (mMappedData) {
        vkUnmapMemory(mDevice, mMemory);
        mMappedData = nullptr;
    }
}

void VulkanBuffer::copyTo(const void* data, VkDeviceSize size) {
    bool alreadyMapped = (mMappedData != nullptr);
    void *target = alreadyMapped ? mMappedData : map();

    if (target != nullptr) {
        memcpy(target, data, (size_t) size);
        if (!alreadyMapped) unmap();
    }
}