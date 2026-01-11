// VulkanBuffer.h
#ifndef MYGAME_VULKAN_BUFFER_H
#define MYGAME_VULKAN_BUFFER_H

#include "volk.h"
#include <vector>

class VulkanBuffer {
public:
    VulkanBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                 VkDeviceSize size, VkBufferUsageFlags usage,
                 VkMemoryPropertyFlags properties);
    ~VulkanBuffer();

    // 복사 방지 (Vulkan 자원 중복 해제 방지)
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    VkBuffer getBuffer() const { return mBuffer; }
    VkDeviceMemory getMemory() const { return mMemory; }
    VkDeviceSize getSize() const { return mSize; }

    void copyTo(const void* data, VkDeviceSize size);
    void* map();
    void unmap();

private:
    VkDevice mDevice;
    VkBuffer mBuffer = VK_NULL_HANDLE;
    VkDeviceMemory mMemory = VK_NULL_HANDLE;
    VkDeviceSize mSize;
    void* mMappedData = nullptr;
};

#endif