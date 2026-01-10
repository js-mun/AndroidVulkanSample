//
// Created by mj on 1/9/26.
//

#include "Renderer.h"
#include "Log.h"
#include <vector>

Renderer::Renderer(struct android_app *app) : mApp(app) {
}

bool Renderer::initialize() {
    // 1. volk 초기화 (Vulkan 로더 로드)
    if (volkInitialize() != VK_SUCCESS) {
        LOGE("Failed to initialize volk");
        return false;
    }

    // 2. Vulkan Instance 생성
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "MyGame";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

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

    // 3. volk에 Instance 로드
    volkLoadInstance(mInstance);
    LOGI("Vulkan Initialized successfully up to Instance creation!");

    LOGV("Vulkan Instance: %p", (void*)mInstance);
    LOGV("vkCreateAndroidSurfaceKHR pointer: %p", (void*)vkCreateAndroidSurfaceKHR);
    if (mApp->window == nullptr) {
        LOGE("ERROR: Native window is NULL. Cannot create surface.");
        return false;
    }
    if (vkCreateAndroidSurfaceKHR == nullptr) {
        LOGE("ERROR: vkCreateAndroidSurfaceKHR function pointer is NULL. Volk failed to load it.");
        return false;
    }

    // 4. Surface 생성 (Android App Glue 사용)
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.window = mApp->window;
    if (vkCreateAndroidSurfaceKHR(mInstance, &surfaceCreateInfo,
                                  nullptr, &mSurface) != VK_SUCCESS) {
        LOGI("Failed to create VkAndroidSurface");
        return false;
    }
    LOGI("Vulkan Initialized successfully up to Surface creation!");

    // 5. Physical Device 선택
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOGE("No Vulkan supported GPU found");
        return false;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());
    mPhysicalDevice = devices[0]; // 첫 번째 GPU 선택 (실제로는 적합성 검사 권장)

    LOGI("Found %u physical device(s):", deviceCount);
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

    // 6. Graphics Queue Family 찾기
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, queueFamilies.data());

    LOGI("Found %u queue families for selected device:", queueFamilyCount);
    bool found = false;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        LOGV("Queue Family [%u]:", i);
        LOGV("  - Queue Count: %u", queueFamilies[i].queueCount);
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) LOGV("    - GRAPHICS bit set");
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)  LOGV("    - COMPUTE bit set");
        if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) LOGV("    - TRANSFER bit set");
        if (queueFamilies[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) LOGV("    - SPARSE_BINDING bit set");

        if (!found && (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            mGraphicsQueueFamilyIndex = i;
            found = true;
            LOGI("  -> Selected as Graphics Queue Family Index: %u", i);
        }
    }
    if (!found) {
        LOGE("No Graphics Queue Family found!");
        return false;
    }

    // 7. Logical Device 생성
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
    // 장치 레벨의 함수 로드 및 큐 가져오기
    volkLoadDevice(mDevice);
    vkGetDeviceQueue(mDevice, mGraphicsQueueFamilyIndex, 0, &mGraphicsQueue);
    LOGI("Vulkan Logical Device created successfully!");

    // 8. Swapchain 생성
    // Surface 성능 및 포맷 확인
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &capabilities);
    // 해상도 설정 (window 크기에 맞춤)
    mSwapchainExtent = capabilities.currentExtent;
    LOGI("Surface Current Extent: %u x %u", mSwapchainExtent.width, mSwapchainExtent.height);
    // 최소 이미지 개수 설정 (Double Buffering 이상)
    LOGI("Surface Capabilities: minImageCount=%u, maxImageCount=%u", capabilities.minImageCount, capabilities.maxImageCount);
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    LOGI("Selected image count: %d", imageCount);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, formats.data());

    // 최적의 포맷 선택 (보통 B8G8R8A8_UNORM 또는 R8G8B8A8_UNORM)
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    LOGI("Found %zu surface format(s):", formats.size());
    for (const auto& availableFormat : formats) {
        LOGI("  - Format: %d, ColorSpace: %d", availableFormat.format, availableFormat.colorSpace);
        if ((availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM ||
             availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM) &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = availableFormat;
            break;
        }
    }
    mSwapchainImageFormat = surfaceFormat.format;
    LOGI("Selected final format: %d", surfaceFormat.format);

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = mSurface;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = mSwapchainExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.preTransform = capabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // V-Sync 활성화
    swapchainCreateInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(mDevice, &swapchainCreateInfo, nullptr, &mSwapchain) != VK_SUCCESS) {
        LOGE("Failed to create Swapchain");
        return false;
    }

    // 스왑체인 이미지 가져오기
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, nullptr);
    mSwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, mSwapchainImages.data());

    // 9. Image Views 생성 (이미지에 접근하기 위한 통로)
    mSwapchainImageViews.resize(mSwapchainImages.size());
    for (size_t i = 0; i < mSwapchainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = mSwapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = mSwapchainImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(mDevice, &viewInfo, nullptr, &mSwapchainImageViews[i]) != VK_SUCCESS) {
            LOGE("Failed to create Swapchain ImageView [%zu]", i);
            return false;
        }
    }
    LOGI("Vulkan Swapchain and ImageViews created successfully!");

    // 10. Render Pass 생성
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = mSwapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // 그리기 전 화면 지움
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 그린 후 메모리에 저장
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // 출력을 위한 레이아웃

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // 스왑체인 이미지가 준비될 때까지 기다리도록 종속성 설정
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS) {
        LOGE("Failed to create Render Pass");
        return false;
    }
    LOGI("Vulkan Render Pass created successfully!");

    // 11. Framebuffers 생성
    mSwapchainFramebuffers.resize(mSwapchainImageViews.size());

    for (size_t i = 0; i < mSwapchainImageViews.size(); i++) {
        VkImageView attachments[] = {
                mSwapchainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = mRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = mSwapchainExtent.width;
        framebufferInfo.height = mSwapchainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mSwapchainFramebuffers[i]) != VK_SUCCESS) {
            LOGE("Failed to create Framebuffer [%zu]", i);
            return false;
        }
    }
    LOGI("Vulkan Framebuffers created successfully!");

    //12. Graphics Pipeline 생성
    // --- Shader Modules ---
    auto vertCode= loadSpirvFromAssets(
            mApp->activity->assetManager,"shaders/vert.spv");

    auto fragCode= loadSpirvFromAssets(
            mApp->activity->assetManager,"shaders/frag.spv");

    VkShaderModule vertShader = createShaderModule(vertCode);
    VkShaderModule fragShader = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertShader;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragShader;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStage, fragStage };

// --- Vertex Input (없음) ---
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

// --- Input Assembly ---
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

// --- Viewport & Scissor ---
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width  = (float)mSwapchainExtent.width;
    viewport.height = (float)mSwapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = mSwapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

// --- Rasterizer ---
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

// --- Multisampling ---
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

// --- Color Blend ---
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

// --- Pipeline Layout ---
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (vkCreatePipelineLayout(mDevice, &layoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS) {
        LOGE("Failed to create pipeline layout");
        return false;
    }

// --- Graphics Pipeline ---
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = mPipelineLayout;
    pipelineInfo.renderPass = mRenderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(
            mDevice, VK_NULL_HANDLE, 1, &pipelineInfo,
            nullptr, &mGraphicsPipeline) != VK_SUCCESS) {
        LOGE("Failed to create graphics pipeline");
        return false;
    }

    vkDestroyShaderModule(mDevice, fragShader, nullptr);
    vkDestroyShaderModule(mDevice, vertShader, nullptr);

    LOGI("Vulkan Graphics Pipeline created successfully!");

    return true;
}

Renderer::~Renderer() {
    // Device 레벨 객체들 해제
    if (mDevice != VK_NULL_HANDLE) {
        if (mGraphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
        }
        if (mPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
        }

        for (auto framebuffer : mSwapchainFramebuffers) {
            vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
        }
        mSwapchainFramebuffers.clear();

        if (mRenderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        }

        for (auto imageView : mSwapchainImageViews) {
            vkDestroyImageView(mDevice, imageView, nullptr);
        }
        mSwapchainImageViews.clear();

        if (mSwapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        }
        vkDestroyDevice(mDevice, nullptr);
    }

    // Instance 레벨 객체들 해제
    if (mInstance != VK_NULL_HANDLE) {
        if (mSurface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        }
        vkDestroyInstance(mInstance, nullptr);
    }
}

VkShaderModule Renderer::createShaderModule(const std::vector<uint32_t>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(
            mDevice,
            &createInfo,
            nullptr,
            &shaderModule) != VK_SUCCESS) {

        LOGE("Failed to create shader module");
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}


std::vector<char> Renderer::readFile(const char* filename) {
    FILE* file = fopen(filename, "rb");
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    std::vector<char> buffer(size);
    fread(buffer.data(), 1, size, file);
    fclose(file);

    return buffer;
}

std::vector<uint32_t> Renderer::loadSpirvFromAssets(
        AAssetManager* assetManager,
        const char* filename) {

    AAsset* asset = AAssetManager_open(
            assetManager,
            filename,
            AASSET_MODE_BUFFER);

    if (!asset) {
        LOGE("Failed to open asset: %s", filename);
        return {};
    }

    size_t size = AAsset_getLength(asset);

    if (size % 4 != 0) {
        LOGE("SPIR-V file size is not multiple of 4: %s", filename);
    }

    std::vector<uint32_t> buffer(size / 4);
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);

    return buffer;
}
