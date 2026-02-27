#pragma once

#include "volk.h"
#include "vk_mem_alloc.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <vector>

class VulkanContext {
public:
    explicit VulkanContext(struct android_app* app);
    ~VulkanContext();

    // Disable copying
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    bool initialize();

    VkInstance getInstance() const { return mInstance; }
    VkSurfaceKHR getSurface() const { return mSurface; }
    VkPhysicalDevice getPhysicalDevice() const { return mPhysicalDevice; }
    VkDevice getDevice() const { return mDevice; }
    VkQueue getGraphicsQueue() const { return mGraphicsQueue; }
    uint32_t getGraphicsQueueFamilyIndex() const { return mGraphicsQueueFamilyIndex; }

    // VMA
    VmaAllocator getAllocator() const { return mAllocator; }

    // Utilities
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    
    // Buffer Utils
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    
    // Image Utils
    bool createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VmaMemoryUsage vmaUsage,
                     VkImage& image, VmaAllocation& allocation);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

private:
    struct android_app* mApp;

    VkInstance mInstance = VK_NULL_HANDLE;
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mDevice = VK_NULL_HANDLE;
    VkQueue mGraphicsQueue = VK_NULL_HANDLE;
    uint32_t mGraphicsQueueFamilyIndex = 0;

    VkCommandPool mTransferCommandPool = VK_NULL_HANDLE;

    // VMA
    VmaAllocator mAllocator = VK_NULL_HANDLE;

    bool createInstance();
    bool createSurface();
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool createTransferCommandPool();
    
    // VMA
    bool createAllocator();
};
