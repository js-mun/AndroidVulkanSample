//
// Created by mj on 1/11/26.
//

#include "vulkan_swapchain.h"
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
    return true;
}

void VulkanSwapchain::cleanup() {
    VkDevice device = mContext->getDevice();
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

bool VulkanSwapchain::createFramebuffers(VkRenderPass renderPass) {
    mSwapchainFramebuffers.resize(mSwapchainImageViews.size());
    for (size_t i = 0; i < mSwapchainImageViews.size(); i++) {
        VkImageView attachments[] = { mSwapchainImageViews[i] };
        VkFramebufferCreateInfo fbInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbInfo.renderPass = renderPass;
        fbInfo.attachmentCount = 1;
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
