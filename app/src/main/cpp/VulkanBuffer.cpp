#include "VulkanBuffer.h"
#include "Log.h"
#include <cstring>

VulkanBuffer::VulkanBuffer(VmaAllocator allocator,
                           VkDeviceSize size,
                           VkBufferUsageFlags usage,
                           VmaMemoryUsage vmaUsage)
        : mAllocator(allocator), mSize(size) {
    VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = vmaUsage;
    allocInfo.flags = 0; // 명시적 0 초기화 (VMA 버전이나 컴파일러에 따라서 쓰레기 값 가능성 제거)

    // 버퍼 생성과 메모리 할당을 한 번에 처리
    if (vmaCreateBuffer(mAllocator, &bufferInfo, &allocInfo, &mBuffer,
                        &mAllocation, nullptr) != VK_SUCCESS) {
        LOGE("Failed to create buffer using VMA");
    }
}

VulkanBuffer::~VulkanBuffer() {
    if (mMappedData != nullptr) {
        unmap();
    }
    if (mBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(mAllocator, mBuffer, mAllocation);
    }
}

void* VulkanBuffer::map() {
    if (mMappedData == nullptr) {
        if (vmaMapMemory(mAllocator, mAllocation, &mMappedData) != VK_SUCCESS) {
            LOGE("Failed to map buffer memory using VMA");
            return nullptr;
        }
    }
    return mMappedData;
}

void VulkanBuffer::unmap() {
    if (mMappedData != nullptr) {
        vmaUnmapMemory(mAllocator, mAllocation);
        mMappedData = nullptr;
    }
}

void VulkanBuffer::copyTo(const void* data, VkDeviceSize size) {
    if (data == nullptr) {
        LOGE("VulkanBuffer::copyTo received null data");
        return;
    }
    if (size > mSize) {
        LOGE("VulkanBuffer::copyTo overflow (requested=%llu, capacity=%llu)",
             static_cast<unsigned long long>(size),
             static_cast<unsigned long long>(mSize));
        return;
    }

    bool alreadyMapped = (mMappedData != nullptr);
    void* target = alreadyMapped ? mMappedData : map();

    if (target != nullptr) {
        memcpy(target, data, static_cast<size_t>(size));
        if (size > 0 && vmaFlushAllocation(mAllocator, mAllocation, 0, size) != VK_SUCCESS) {
            LOGE("Failed to flush VMA allocation");
        }
        if (!alreadyMapped) unmap();
    }
}
