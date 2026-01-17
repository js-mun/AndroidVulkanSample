#include "vulkan_command.h"
#include "Log.h"

VulkanCommand::VulkanCommand(VkDevice device, uint32_t queueFamilyIndex)
    : mDevice(device), mQueueFamilyIndex(queueFamilyIndex) {
}

VulkanCommand::~VulkanCommand() {
    if (mCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
    }
}

bool VulkanCommand::initialize(uint32_t bufferCount) {
    VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolInfo.queueFamilyIndex = mQueueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS) {
        LOGE("Failed to create command pool");
        return false;
    }

    mCommandBuffers.resize(bufferCount);
    VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.commandPool = mCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = bufferCount;

    if (vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS) {
        LOGE("Failed to allocate command buffers");
        return false;
    }

    return true;
}

void VulkanCommand::begin(uint32_t index, VkCommandBufferUsageFlags flags) {
    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = flags;
    if (vkBeginCommandBuffer(mCommandBuffers[index], &beginInfo) != VK_SUCCESS) {
        LOGE("Failed to begin recording command buffer %u", index);
    }
}

void VulkanCommand::end(uint32_t index) {
    if (vkEndCommandBuffer(mCommandBuffers[index]) != VK_SUCCESS) {
        LOGE("Failed to record command buffer %u", index);
    }
}

void VulkanCommand::reset(uint32_t index) {
    vkResetCommandBuffer(mCommandBuffers[index], 0);
}
