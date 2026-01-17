#include "Renderer.h"
#include "Log.h"
#include "asset_utils.h"
#include "vulkan_types.h"

#include <vector>
#include <chrono>

Renderer::Renderer(struct android_app *app) : mApp(app) {
    mContext = std::make_unique<VulkanContext>(mApp);
}

bool Renderer::initialize() {
    // 1. volk 초기화 (Vulkan 로더 로드)
    if (volkInitialize() != VK_SUCCESS) {
        LOGE("Failed to initialize volk");
        return false;
    }

    if (!mContext->initialize()) {
        LOGE("Failed to initialize VulkanContext");
        return false;
    }

    // 8. Swapchain 생성
    // Surface 성능 및 포맷 확인
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            mContext->getPhysicalDevice(), mContext->getSurface(), &capabilities);
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
    vkGetPhysicalDeviceSurfaceFormatsKHR(mContext->getPhysicalDevice(), mContext->getSurface(),&formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(mContext->getPhysicalDevice(), mContext->getSurface(),
        &formatCount, formats.data());

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
    swapchainCreateInfo.surface = mContext->getSurface();
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

    if (vkCreateSwapchainKHR(mContext->getDevice(), &swapchainCreateInfo, nullptr, &mSwapchain) != VK_SUCCESS) {
        LOGE("Failed to create Swapchain");
        return false;
    }

    // 스왑체인 이미지 가져오기
    vkGetSwapchainImagesKHR(mContext->getDevice(), mSwapchain, &imageCount, nullptr);
    mSwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(mContext->getDevice(), mSwapchain, &imageCount, mSwapchainImages.data());

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

        if (vkCreateImageView(mContext->getDevice(), &viewInfo, nullptr, &mSwapchainImageViews[i]) != VK_SUCCESS) {
            LOGE("Failed to create Swapchain ImageView [%zu]", i);
            return false;
        }
    }
    LOGI("Vulkan Swapchain and ImageViews created successfully!");

    mPipeline = std::make_unique<VulkanPipeline>(mContext->getDevice());
    if (!mPipeline->initialize(mSwapchainImageFormat, mApp->activity->assetManager)) {
        LOGE("Failed to initialize Vulkan Pipeline");
        return false;
    }

    // 11. Framebuffers 생성
    mSwapchainFramebuffers.resize(mSwapchainImageViews.size());

    for (size_t i = 0; i < mSwapchainImageViews.size(); i++) {
        VkImageView attachments[] = {
                mSwapchainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = mPipeline->getRenderPass();
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = mSwapchainExtent.width;
        framebufferInfo.height = mSwapchainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(mContext->getDevice(), &framebufferInfo, nullptr, &mSwapchainFramebuffers[i]) != VK_SUCCESS) {
            LOGE("Failed to create Framebuffer [%zu]", i);
            return false;
        }
    }
    LOGI("Vulkan Framebuffers created successfully!");

    // 13. Command Pool 생성
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = mContext->getGraphicsQueueFamilyIndex();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(mContext->getDevice(), &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS) {
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

    if (vkAllocateCommandBuffers(mContext->getDevice(), &allocInfo, mCommandBuffers.data()) != VK_SUCCESS) {
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
        if (vkCreateSemaphore(mContext->getDevice(), &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mContext->getDevice(), &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(mContext->getDevice(), &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS) {
            LOGE("Failed to create synchronization objects for a frame");
            return false;
        }
    }

    // 16. Uniform Buffers 생성
    mUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        mUniformBuffers[i] = std::make_unique<VulkanBuffer>(
                mContext->getDevice(), mContext->getPhysicalDevice(), sizeof(UniformBufferObject),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        mUniformBuffers[i]->map();
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

    if (vkCreateDescriptorPool(mContext->getDevice(), &poolInfoDesc, nullptr, &mDescriptorPool) != VK_SUCCESS) {
        LOGE("Failed to create descriptor pool");
        return false;
    }

    // 18. Descriptor Sets 할당 및 업데이트
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, mPipeline->getDescriptorSetLayout());
    VkDescriptorSetAllocateInfo allocInfoDesc{};
    allocInfoDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfoDesc.descriptorPool = mDescriptorPool;
    allocInfoDesc.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfoDesc.pSetLayouts = layouts.data();

    mDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(mContext->getDevice(), &allocInfoDesc, mDescriptorSets.data()) != VK_SUCCESS) {
        LOGE("Failed to allocate descriptor sets");
        return false;
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfoUbo{};
        bufferInfoUbo.buffer = mUniformBuffers[i]->getBuffer();
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

        vkUpdateDescriptorSets(mContext->getDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    createVertexBuffer();

    LOGI("Vulkan Initialization Wrap-up Successful!");

    return true;
}

Renderer::~Renderer() {
    // Device 레벨 객체들 해제
    if (mContext->getDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(mContext->getDevice()); // 모든 작업이 끝날 때까지 대기

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(mContext->getDevice(), mRenderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(mContext->getDevice(), mImageAvailableSemaphores[i], nullptr);
            vkDestroyFence(mContext->getDevice(), mInFlightFences[i], nullptr);
        }

        if (mDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(mContext->getDevice(), mDescriptorPool, nullptr);
        }

        if (mCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(mContext->getDevice(), mCommandPool, nullptr);
        }

        for (auto framebuffer : mSwapchainFramebuffers) {
            vkDestroyFramebuffer(mContext->getDevice(), framebuffer, nullptr);
        }
        mSwapchainFramebuffers.clear();

        for (auto imageView : mSwapchainImageViews) {
            vkDestroyImageView(mContext->getDevice(), imageView, nullptr);
        }
        mSwapchainImageViews.clear();

        if (mSwapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(mContext->getDevice(), mSwapchain, nullptr);
        }
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
    renderPassInfo.renderPass = mPipeline->getRenderPass();
    renderPassInfo.framebuffer = mSwapchainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = mSwapchainExtent;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline->getGraphicsPipeline());

    VkBuffer vertexBuffers[] = { mVertexBuffer->getBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Descriptor Set 바인딩 (UBO 데이터 연결)
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline->getPipelineLayout(), 0, 1, &mDescriptorSets[mCurrentFrame], 0, nullptr);

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

void Renderer::createVertexBuffer() {
    std::vector<Vertex> vertices = {
            {{0.0f, 0.5f}},   // 위
            {{-0.5f, -0.5f}}, // 왼쪽 아래
            {{0.5f, -0.5f}}   // 오른쪽 아래
    };

    // 1. 스테이징 버퍼(CPU용) 생성 없이 간단히 직접 생성 (학습용)
    VkDeviceSize size = sizeof(Vertex) * vertices.size();
    mVertexBuffer = std::make_unique<VulkanBuffer>(
            mContext->getDevice(), mContext->getPhysicalDevice(), size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    // 1회성 정점 데이터이며 내부에서 map() -> copy -> unmap() 수행
    mVertexBuffer->copyTo(vertices.data(), size);
}

void Renderer::render() {
    if (mFramebufferResized) {
        LOGI("Buffer resized");
        mFramebufferResized = false;
        recreateSwapchain();
        return;
    }

    // 1. 이전 프레임 작업이 끝날 때까지 대기
    vkWaitForFences(mContext->getDevice(), 1, &mInFlightFences[mCurrentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(mContext->getDevice(), 1, &mInFlightFences[mCurrentFrame]);

    // Uniform Buffer 업데이트 (회전 및 종횡비 계산)
    updateUniformBuffer(mCurrentFrame);

    // 2. 스왑체인에서 이미지 가져오기
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(mContext->getDevice(), mSwapchain, UINT64_MAX, mImageAvailableSemaphores[mCurrentFrame], VK_NULL_HANDLE, &imageIndex);
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

    if (vkQueueSubmit(mContext->getGraphicsQueue(), 1, &submitInfo, mInFlightFences[mCurrentFrame]) != VK_SUCCESS) {
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

    result = vkQueuePresentKHR(mContext->getGraphicsQueue(), &presentInfo);
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
        vkDestroyFramebuffer(mContext->getDevice(), framebuffer, nullptr);
    }
    mSwapchainFramebuffers.clear();

    // 2. 이미지 뷰 해제
    for (auto imageView : mSwapchainImageViews) {
        vkDestroyImageView(mContext->getDevice(), imageView, nullptr);
    }
    mSwapchainImageViews.clear();

    // 3. 스왑체인 파괴
    if (mSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(mContext->getDevice(), mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }
}

void Renderer::recreateSwapchain() {
    // 윈도우가 최소화되어 있거나 크기가 0이면 대기
    if (mApp->window == nullptr) return;

    vkDeviceWaitIdle(mContext->getDevice());

    // 기존 자원 정리
    cleanupSwapchain();

    // 1. 스왑체인 다시 생성 (initialize()의 8번 로직 재사용)
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mContext->getPhysicalDevice(), mContext->getSurface(), &capabilities);
    mSwapchainExtent = capabilities.currentExtent;
    mSwapchainTransform = capabilities.currentTransform; // 회전 상태 업데이트

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(mContext->getPhysicalDevice(), mContext->getSurface(), &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(mContext->getPhysicalDevice(), mContext->getSurface(), &formatCount, formats.data());
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == mSwapchainImageFormat) { // 기존 포맷 유지
            surfaceFormat = f; break;
        }
    }

    VkSwapchainCreateInfoKHR swapchainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchainInfo.surface = mContext->getSurface();
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

    if (vkCreateSwapchainKHR(mContext->getDevice(), &swapchainInfo, nullptr, &mSwapchain) != VK_SUCCESS) {
        LOGE("Failed to recreate Swapchain");
        return;
    }

    // 2. 이미지 및 이미지 뷰 다시 생성 (initialize()의 9번 로직)
    uint32_t imageCount;
    vkGetSwapchainImagesKHR(mContext->getDevice(), mSwapchain, &imageCount, nullptr);
    mSwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(mContext->getDevice(), mSwapchain, &imageCount, mSwapchainImages.data());

    mSwapchainImageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image = mSwapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = mSwapchainImageFormat;
        viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCreateImageView(mContext->getDevice(), &viewInfo, nullptr, &mSwapchainImageViews[i]);
    }

    // 3. 프레임버퍼 다시 생성 (initialize()의 11번 로직)
    mSwapchainFramebuffers.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageView attachments[] = { mSwapchainImageViews[i] };
        VkFramebufferCreateInfo fbInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbInfo.renderPass = mPipeline->getRenderPass();
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = attachments;
        fbInfo.width = mSwapchainExtent.width;
        fbInfo.height = mSwapchainExtent.height;
        fbInfo.layers = 1;
        vkCreateFramebuffer(mContext->getDevice(), &fbInfo, nullptr, &mSwapchainFramebuffers[i]);
    }

    LOGI("Vulkan Swapchain recreated successfully (New size: %ux%u)", mSwapchainExtent.width, mSwapchainExtent.height);
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
    mUniformBuffers[currentImage]->copyTo(&ubo, sizeof(ubo));
}
