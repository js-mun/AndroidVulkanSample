#pragma once

#include "volk.h"
#include <vector>

class VulkanCommand {
public:
    VulkanCommand(VkDevice device, uint32_t queueFamilyIndex);
    ~VulkanCommand();

    // 복사 방지
    VulkanCommand(const VulkanCommand&) = delete;
    VulkanCommand& operator=(const VulkanCommand&) = delete;

    bool initialize(uint32_t bufferCount);

    VkCommandBuffer getBuffer(uint32_t index) const { return mCommandBuffers[index]; }
    VkCommandPool getPool() const { return mCommandPool; }

    // 편의를 위한 인터페이스
    void begin(uint32_t index, VkCommandBufferUsageFlags flags = 0);
    void end(uint32_t index);
    void reset(uint32_t index);

private:
    VkDevice mDevice;
    uint32_t mQueueFamilyIndex;
    VkCommandPool mCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mCommandBuffers;
};
