// VMA_IMPLEMENTATION는 vma header를 사용하는
// 단 하나의 cpp에서 define해야 함 (vma는 header only로 구현)
// 헤더에 추가하면, 그 헤더를 include하는 모든 파일에서 중복 생성되어 심볼 중복 발생
#define VMA_IMPLEMENTATION

#include "VulkanContext.h"
#include "Log.h"

VulkanContext::VulkanContext(struct android_app* app) : mApp(app) {
}

VulkanContext::~VulkanContext() {
    if (mAllocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(mAllocator);
    }
    if (mTransferCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(mDevice, mTransferCommandPool, nullptr);
    }
    if (mDevice != VK_NULL_HANDLE) {
        vkDestroyDevice(mDevice, nullptr);
    }
    if (mInstance != VK_NULL_HANDLE) {
        if (mSurface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        }
        vkDestroyInstance(mInstance, nullptr);
    }
}

bool VulkanContext::initialize() {
    if (!createInstance()) return false;
    if (!createSurface()) return false;
    if (!selectPhysicalDevice()) return false;
    if (!createLogicalDevice()) return false;
    if (!createTransferCommandPool()) return false;
    if (!createAllocator()) return false;

    return true;
}

bool VulkanContext::createAllocator() {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
    allocatorInfo.physicalDevice = mPhysicalDevice;
    allocatorInfo.device = mDevice;
    allocatorInfo.instance = mInstance;

    // VMA가 Volk를 통해 로드된 함수 포인터들을 사용하도록 명시적으로 설정
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
    vulkanFunctions.vkFreeMemory = vkFreeMemory;
    vulkanFunctions.vkMapMemory = vkMapMemory;
    vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
    vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
    vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
    vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
    vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
    vulkanFunctions.vkCreateImage = vkCreateImage;
    vulkanFunctions.vkDestroyImage = vkDestroyImage;
    vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;

    allocatorInfo.pVulkanFunctions = &vulkanFunctions;

    if (vmaCreateAllocator(&allocatorInfo, &mAllocator) != VK_SUCCESS) {
        LOGE("Failed to create VMA Allocator");
        return false;
    }
    return true;
}

bool VulkanContext::createInstance() {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "MyGame";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    const char* extensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = extensions;

    if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS) {
        LOGE("Failed to create vkInstance");
        return false;
    }

    volkLoadInstance(mInstance);
    return true;
}

bool VulkanContext::createSurface() {
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.window = mApp->window;
    if (vkCreateAndroidSurfaceKHR(mInstance, &surfaceCreateInfo, nullptr, &mSurface) != VK_SUCCESS) {
        LOGE("Failed to create VkAndroidSurface");
        return false;
    }
    return true;
}

bool VulkanContext::selectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOGE("No Vulkan supported GPU found");
        return false;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());
    mPhysicalDevice = devices[0];

    // Debug logs
    LOGV("Found %u physical device(s):", deviceCount);
    for (uint32_t i = 0; i < deviceCount; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        LOGV("Device [%u]: %s", i, props.deviceName);
        LOGV("  - Type: %d (1:Integrated, 2:Discrete, 3:Virtual)", props.deviceType);
        LOGV("  - API Version: %d.%d.%d",
             VK_VERSION_MAJOR(props.apiVersion),
             VK_VERSION_MINOR(props.apiVersion),
             VK_VERSION_PATCH(props.apiVersion));
        LOGV("  - Driver Version: %u", props.driverVersion);
    }

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, queueFamilies.data());

    // Debug logs
    LOGV("Found %u queue families for selected device:", queueFamilyCount);
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        LOGV("Queue Family [%u]:", i);
        LOGV("  - Queue Count: %u", queueFamilies[i].queueCount);
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) LOGV("    - GRAPHICS bit set");
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) LOGV("    - COMPUTE bit set");
        if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) LOGV("    - TRANSFER bit set");
        if (queueFamilies[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) LOGV("    - SPARSE_BINDING bit set");
    }

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            mGraphicsQueueFamilyIndex = i;
            break;
        }
    }

    return true;
}

bool VulkanContext::createLogicalDevice() {
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = mGraphicsQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(mPhysicalDevice, &deviceCreateInfo, nullptr, &mDevice) != VK_SUCCESS) {
        LOGE("Failed to create Logical Device");
        return false;
    }
    volkLoadDevice(mDevice);
    vkGetDeviceQueue(mDevice, mGraphicsQueueFamilyIndex, 0, &mGraphicsQueue);
    return true;
}

bool VulkanContext::createTransferCommandPool() {
    VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolInfo.queueFamilyIndex = mGraphicsQueueFamilyIndex;
    // 짧은 수명의 커맨드 버퍼를 위한 플래그 설정
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    return vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mTransferCommandPool) == VK_SUCCESS;
}

VkCommandBuffer VulkanContext::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = mTransferCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(mDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VulkanContext::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // 큐에 제출하고 작업이 끝날 때까지 CPU가 기다립니다.
    vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(mGraphicsQueue);

    vkFreeCommandBuffers(mDevice, mTransferCommandPool, 1, &commandBuffer);
}

void VulkanContext::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion = { 0, 0, size };
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

bool VulkanContext::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                 VkImageUsageFlags usage, VmaMemoryUsage vmaUsage,
                 VkImage& image, VmaAllocation& allocation) {
    VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = vmaUsage;
    allocInfo.flags = 0;

    // 기존 vkCreateImage, findMemoryType, vkAllocateMemory, vkBindImageMemory를 이 하나로 대체
    if (vmaCreateImage(mAllocator, &imageInfo, &allocInfo, &image, &allocation, nullptr) != VK_SUCCESS) {
        LOGE("Failed to create VMA image");
        return false;
    }
    return true;
}

void VulkanContext::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        LOGE("Unsupported layout transition");
        return;
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

void VulkanContext::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}
