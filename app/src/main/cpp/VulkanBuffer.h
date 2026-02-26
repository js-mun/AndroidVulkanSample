#pragma once

#include "volk.h"
#include "vk_mem_alloc.h"

class VulkanBuffer {
public:
    VulkanBuffer(VmaAllocator allocator,
                 VkDeviceSize size,
                 VkBufferUsageFlags usage,
                 VmaMemoryUsage vmaUsage);
    ~VulkanBuffer();

    // 복사 방지 (Vulkan 자원 중복 해제 방지)
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    VkBuffer getBuffer() const { return mBuffer; }
    VkDeviceSize getSize() const { return mSize; }

    void copyTo(const void* data, VkDeviceSize size);
    void* map();
    void unmap();

private:
    VmaAllocator mAllocator;
    VkBuffer mBuffer = VK_NULL_HANDLE;
    VmaAllocation mAllocation = VK_NULL_HANDLE;
    VkDeviceSize mSize;
    void* mMappedData = nullptr;
};
