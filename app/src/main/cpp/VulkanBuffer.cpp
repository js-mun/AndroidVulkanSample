#include "VulkanBuffer.h"
#include "Log.h"
#include <cstring>

namespace {
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOGE("failed to find suitable memory type!");
    return 0;
}
}

VulkanBuffer::VulkanBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                           VkDeviceSize size, VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags properties)
        : mDevice(device), mSize(size) {
    // 1. VkBuffer 생성
    VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(mDevice, &bufferInfo, nullptr, &mBuffer) != VK_SUCCESS) {
        LOGE("Failed to create VkBuffer");
    }
    // 2. 메모리 요구사항 확인 및 할당
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(mDevice, mBuffer, &memRequirements);
    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);
    if (vkAllocateMemory(mDevice, &allocInfo, nullptr, &mMemory) != VK_SUCCESS) {
        LOGE("Failed to allocate buffer memory");
    }
    // 3. 버퍼와 메모리 연결
    vkBindBufferMemory(mDevice, mBuffer, mMemory, 0);
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
        memcpy(target, data, (size_t) (size > mSize ? mSize : size));
        if (!alreadyMapped) unmap();
    }
}