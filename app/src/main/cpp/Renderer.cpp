//
// Created by mj on 1/9/26.
//

#include "Renderer.h"
#include "Log.h"
#include <vector>
#include <chrono>

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
    mSwapchainTransform = capabilities.currentTransform; // 회전 상태 저장 추가

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

    // 11.2 Descriptor Set Layout 생성
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = 1;
    descriptorLayoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(mDevice, &descriptorLayoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
        LOGE("Failed to create descriptor set layout");
        return false;
    }

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

    // --- Depth Stencil (명시적 비활성화) ---
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    // --- Dynamic State (뷰포트 반전을 위해 필수) ---
    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateInfo.pDynamicStates = dynamicStates.data();


// --- Pipeline Layout ---
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &mDescriptorSetLayout;

    if (vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS) {
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
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
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

    // 13. Command Pool 생성
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = mGraphicsQueueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS) {
        LOGE("Failed to create command pool");
        return false;
    }

    // 14. Command Buffers 할당
    mCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = mCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)mCommandBuffers.size();

    if (vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS) {
        LOGE("Failed to allocate command buffers");
        return false;
    }

    // 15. 동기화 객체 생성 (Semaphores & Fences)
    mImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    mRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    mInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS) {
            LOGE("Failed to create synchronization objects for a frame");
            return false;
        }
    }

    // 16. Uniform Buffers 생성
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    mUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    mUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    mUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     mUniformBuffers[i], mUniformBuffersMemory[i]);
        vkMapMemory(mDevice, mUniformBuffersMemory[i], 0, bufferSize, 0, &mUniformBuffersMapped[i]);
    }

    // 17. Descriptor Pool 생성
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfoDesc{};
    poolInfoDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfoDesc.poolSizeCount = 1;
    poolInfoDesc.pPoolSizes = &poolSize;
    poolInfoDesc.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(mDevice, &poolInfoDesc, nullptr, &mDescriptorPool) != VK_SUCCESS) {
        LOGE("Failed to create descriptor pool");
        return false;
    }

    // 18. Descriptor Sets 할당 및 업데이트
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, mDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfoDesc{};
    allocInfoDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfoDesc.descriptorPool = mDescriptorPool;
    allocInfoDesc.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfoDesc.pSetLayouts = layouts.data();

    mDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(mDevice, &allocInfoDesc, mDescriptorSets.data()) != VK_SUCCESS) {
        LOGE("Failed to allocate descriptor sets");
        return false;
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfoUbo{};
        bufferInfoUbo.buffer = mUniformBuffers[i];
        bufferInfoUbo.offset = 0;
        bufferInfoUbo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = mDescriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfoUbo;

        vkUpdateDescriptorSets(mDevice, 1, &descriptorWrite, 0, nullptr);
    }

    LOGI("Vulkan Initialization Wrap-up Successful!");

    return true;
}

Renderer::~Renderer() {
    // Device 레벨 객체들 해제
    if (mDevice != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(mDevice); // 모든 작업이 끝날 때까지 대기

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
            vkDestroyFence(mDevice, mInFlightFences[i], nullptr);

            vkUnmapMemory(mDevice, mUniformBuffersMemory[i]);
            vkDestroyBuffer(mDevice, mUniformBuffers[i], nullptr);
            vkFreeMemory(mDevice, mUniformBuffersMemory[i], nullptr);
        }

        if (mDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
        }

        if (mDescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);
        }

        if (mCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
        }

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

void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOGE("Failed to begin recording command buffer");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = mRenderPass;
    renderPassInfo.framebuffer = mSwapchainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = mSwapchainExtent;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

    // Descriptor Set 바인딩 (UBO 데이터 연결)
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSets[mCurrentFrame], 0, nullptr);

    // Dynamic State이므로 렌더링 시점에 뷰포트/시저 설정 필요
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = (float)mSwapchainExtent.height;
    viewport.width = (float)mSwapchainExtent.width;
    viewport.height = -(float)mSwapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = mSwapchainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // 삼각형 그리기 (정점 3개)
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOGE("Failed to record command buffer");
    }
}

void Renderer::render() {
    if (mFramebufferResized) {
        LOGI("Buffer resized");
        mFramebufferResized = false;
        recreateSwapchain();
        return;
    }

    // 1. 이전 프레임 작업이 끝날 때까지 대기
    vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(mDevice, 1, &mInFlightFences[mCurrentFrame]);

    // Uniform Buffer 업데이트 (회전 및 종횡비 계산)
    updateUniformBuffer(mCurrentFrame);

    // 2. 스왑체인에서 이미지 가져오기
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(mDevice, mSwapchain, UINT64_MAX, mImageAvailableSemaphores[mCurrentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        LOGI("Failed to acquire next image by VK_ERROR_OUT_OF_DATE_KHR");
        recreateSwapchain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOGE("Failed to acquire swapchain image!");
        return;
    }

    // 3. 커맨드 버퍼 기록
    vkResetCommandBuffer(mCommandBuffers[mCurrentFrame], 0);
    recordCommandBuffer(mCommandBuffers[mCurrentFrame], imageIndex);

    // 4. GPU 큐에 제출
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {mImageAvailableSemaphores[mCurrentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &mCommandBuffers[mCurrentFrame];

    VkSemaphore signalSemaphores[] = {mRenderFinishedSemaphores[mCurrentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mInFlightFences[mCurrentFrame]) != VK_SUCCESS) {
        LOGE("Failed to submit draw command buffer");
    }

    // 5. 화면에 표시 (Present)
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {mSwapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(mGraphicsQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        LOGI("Failed to queue present by VK_ERROR_OUT_OF_DATE_KHR");
        recreateSwapchain();
    }

    // 6. 다음 프레임 인덱스로 교체
    mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::cleanupSwapchain() {
    // 1. 프레임버퍼 해제
    for (auto framebuffer : mSwapchainFramebuffers) {
        vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    }
    mSwapchainFramebuffers.clear();

    // 2. 이미지 뷰 해제
    for (auto imageView : mSwapchainImageViews) {
        vkDestroyImageView(mDevice, imageView, nullptr);
    }
    mSwapchainImageViews.clear();

    // 3. 스왑체인 파괴
    if (mSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }
}

void Renderer::recreateSwapchain() {
    // 윈도우가 최소화되어 있거나 크기가 0이면 대기
    if (mApp->window == nullptr) return;

    vkDeviceWaitIdle(mDevice);

    // 기존 자원 정리
    cleanupSwapchain();

    // 1. 스왑체인 다시 생성 (initialize()의 8번 로직 재사용)
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &capabilities);
    mSwapchainExtent = capabilities.currentExtent;
    mSwapchainTransform = capabilities.currentTransform; // 회전 상태 업데이트

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, formats.data());
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == mSwapchainImageFormat) { // 기존 포맷 유지
            surfaceFormat = f; break;
        }
    }

    VkSwapchainCreateInfoKHR swapchainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchainInfo.surface = mSurface;
    swapchainInfo.minImageCount = capabilities.minImageCount + 1;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = mSwapchainExtent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = capabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(mDevice, &swapchainInfo, nullptr, &mSwapchain) != VK_SUCCESS) {
        LOGE("Failed to recreate Swapchain");
        return;
    }

    // 2. 이미지 및 이미지 뷰 다시 생성 (initialize()의 9번 로직)
    uint32_t imageCount;
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, nullptr);
    mSwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, mSwapchainImages.data());

    mSwapchainImageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image = mSwapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = mSwapchainImageFormat;
        viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCreateImageView(mDevice, &viewInfo, nullptr, &mSwapchainImageViews[i]);
    }

    // 3. 프레임버퍼 다시 생성 (initialize()의 11번 로직)
    mSwapchainFramebuffers.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageView attachments[] = { mSwapchainImageViews[i] };
        VkFramebufferCreateInfo fbInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbInfo.renderPass = mRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = attachments;
        fbInfo.width = mSwapchainExtent.width;
        fbInfo.height = mSwapchainExtent.height;
        fbInfo.layers = 1;
        vkCreateFramebuffer(mDevice, &fbInfo, nullptr, &mSwapchainFramebuffers[i]);
    }

    LOGI("Vulkan Swapchain recreated successfully (New size: %ux%u)", mSwapchainExtent.width, mSwapchainExtent.height);
}

void Renderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(mDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        LOGE("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(mDevice, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(mDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        LOGE("Failed to allocate buffer memory");
    }
    vkBindBufferMemory(mDevice, buffer, bufferMemory, 0);
}

uint32_t Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOGE("failed to find suitable memory type!");
    return 0;
}

void Renderer::updateUniformBuffer(uint32_t currentImage) {
    UniformBufferObject ubo{};
    ubo.mvp = glm::mat4(1.0f);

    // 1. 기기 회전(Surface Transform) 보정
    // 기기가 물리적으로 돌아간 만큼 반대 방향으로 회전 행렬을 적용합니다.
    if (mSwapchainTransform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR) {
        ubo.mvp = glm::rotate(ubo.mvp, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    } else if (mSwapchainTransform == VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR) {
        ubo.mvp = glm::rotate(ubo.mvp, glm::radians(-180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    } else if (mSwapchainTransform == VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) {
        ubo.mvp = glm::rotate(ubo.mvp, glm::radians(-270.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    }

    // 2. 종횡비 보정 (가장 안정적인 'Short-side' 기준 방식)
    // 물리적 해상도를 가져옵니다.
    float width = static_cast<float>(mSwapchainExtent.width);
    float height = static_cast<float>(mSwapchainExtent.height);

    // 화면의 가로/세로 중 짧은 쪽 길이를 기준으로 잡습니다.
    float minDim = std::min(width, height);

    // [핵심] 짧은 쪽 길이에 맞춰 전체 좌표계를 정사각형 박스로 만듭니다.
    // 이렇게 하면 어떤 회전 상태에서도 삼각형이 찌그러지지 않고 동일한 크기를 유지합니다.
    ubo.mvp = glm::scale(ubo.mvp, glm::vec3(minDim / width, minDim / height, 1.0f));

    // 3. 전체 크기 조정
    ubo.mvp = glm::scale(ubo.mvp, glm::vec3(1.0f, 1.0f, 1.0f));

    // GPU 메모리로 복사
    if (mUniformBuffersMapped[currentImage] != nullptr) {
        memcpy(mUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
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
