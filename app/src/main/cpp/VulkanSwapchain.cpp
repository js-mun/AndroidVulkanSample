//
// Created by mj on 1/11/26.
//

#include "VulkanSwapchain.h"
#include "Log.h"
#include <algorithm>

VulkanSwapchain::VulkanSwapchain(VulkanContext* context) : mContext(context) {
}

VulkanSwapchain::~VulkanSwapchain() {
    cleanup();
}

bool VulkanSwapchain::createSwapchainAndViews() {
    if (!createSwapchain()) return false;
    if (!createImageViews()) return false;
    if (!createDepthResources()) return false;
    return true;
}

void VulkanSwapchain::cleanup() {
    VkDevice device = mContext->getDevice();

    if (mDepthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, mDepthImageView, nullptr);
        mDepthImageView = VK_NULL_HANDLE;
    }
    if (mDepthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, mDepthImage, nullptr);
        mDepthImage = VK_NULL_HANDLE;
    }
    if (mDepthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, mDepthImageMemory, nullptr);
        mDepthImageMemory = VK_NULL_HANDLE;
    }

    for (auto framebuffer : mSwapchainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    mSwapchainFramebuffers.clear();

    for (auto imageView : mSwapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    mSwapchainImageViews.clear();

    if (mSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }
}

void VulkanSwapchain::recreate(VkRenderPass renderPass) {
    vkDeviceWaitIdle(mContext->getDevice());
    cleanup();
    createSwapchainAndViews();
    createFramebuffers(renderPass);
}

bool VulkanSwapchain::createSwapchain() {
    VkPhysicalDevice physicalDevice = mContext->getPhysicalDevice();
    VkSurfaceKHR surface = mContext->getSurface();

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    if (capabilities.currentExtent.width == 0 || capabilities.currentExtent.height == 0) return false;

    mSwapchainExtent = capabilities.currentExtent;
    mSwapchainTransform = capabilities.currentTransform;


    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

    LOGV("Found %zu surface format(s):", formats.size());
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& f : formats) {
        LOGV("  - Format: %d, ColorSpace: %d", f.format, f.colorSpace);
        if ((f.format == VK_FORMAT_R8G8B8A8_UNORM || f.format == VK_FORMAT_B8G8R8A8_UNORM) &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = f;
            break;
        }
    }
    mSwapchainImageFormat = surfaceFormat.format;
    LOGV("Selected final format: %d", mSwapchainImageFormat);

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    LOGV("Surface Current Extent: %u x %u", mSwapchainExtent.width, mSwapchainExtent.height);
    LOGV("Surface Capabilities: minImageCount=%u, maxImageCount=%u", capabilities.minImageCount, capabilities.maxImageCount);
    LOGV("Selected image count: %d", imageCount);

    VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = mSwapchainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = mSwapchainTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(mContext->getDevice(), &createInfo, nullptr, &mSwapchain) != VK_SUCCESS) {
        LOGE("Failed to create Swapchain");
        return false;
    }

    vkGetSwapchainImagesKHR(mContext->getDevice(), mSwapchain, &imageCount, nullptr);
    mSwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(mContext->getDevice(), mSwapchain, &imageCount, mSwapchainImages.data());

    return true;
}

bool VulkanSwapchain::createImageViews() {
    mSwapchainImageViews.resize(mSwapchainImages.size());
    for (size_t i = 0; i < mSwapchainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image = mSwapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = mSwapchainImageFormat;
        viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        if (vkCreateImageView(mContext->getDevice(), &viewInfo, nullptr, &mSwapchainImageViews[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

bool VulkanSwapchain::createDepthResources() {
    mDepthFormat = findDepthFormat();
    VkDevice device = mContext->getDevice();

    VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = mSwapchainExtent.width;
    imageInfo.extent.height = mSwapchainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = mDepthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &mDepthImage) != VK_SUCCESS) {
        LOGE("Failed to create depth image");
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, mDepthImage, &memRequirements);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(mContext->getPhysicalDevice(), &memProperties);

    uint32_t memoryTypeIndex = uint32_t(-1);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            memoryTypeIndex = i;
            break;
        }
    }

    VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &mDepthImageMemory) != VK_SUCCESS) {
        LOGE("Failed to allocate depth image memory");
        return false;
    }

    vkBindImageMemory(device, mDepthImage, mDepthImageMemory, 0);

    VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image = mDepthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = mDepthFormat;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

    if (vkCreateImageView(device, &viewInfo, nullptr, &mDepthImageView) != VK_SUCCESS) {
        LOGE("Failed to create depth image view");
        return false;
    }
    return true;
}

bool VulkanSwapchain::createFramebuffers(VkRenderPass renderPass) {
    mSwapchainFramebuffers.resize(mSwapchainImageViews.size());
    for (size_t i = 0; i < mSwapchainImageViews.size(); i++) {
        VkImageView attachments[] = {
                mSwapchainImageViews[i],
                mDepthImageView
        };

        VkFramebufferCreateInfo fbInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbInfo.renderPass = renderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = attachments;
        fbInfo.width = mSwapchainExtent.width;
        fbInfo.height = mSwapchainExtent.height;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(mContext->getDevice(), &fbInfo, nullptr, &mSwapchainFramebuffers[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

VkFormat VulkanSwapchain::findDepthFormat() {
    return findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkFormat VulkanSwapchain::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(mContext->getPhysicalDevice(), format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    return VK_FORMAT_D32_SFLOAT;
}