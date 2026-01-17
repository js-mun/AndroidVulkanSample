#include "Renderer.h"
#include "Log.h"
#include "asset_utils.h"
#include "vulkan_types.h"

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

    mContext = std::make_unique<VulkanContext>(mApp);
    if (!mContext->initialize()) {
        LOGE("Failed to initialize VulkanContext");
        return false;
    }

    mSwapchain = std::make_unique<VulkanSwapchain>(mContext.get());
    if (!mSwapchain->createSwapchainAndViews()) {
        LOGE("Failed to initialize VulkanSwapchain(Swapchain and Views)");
        return false;
    }

    mPipeline = std::make_unique<VulkanPipeline>(mContext->getDevice());
    if (!mPipeline->initialize(mSwapchain->getImageFormat(), mApp->activity->assetManager)) {
        LOGE("Failed to initialize Vulkan Pipeline");
        return false;
    }

    if (!mSwapchain->createFramebuffers(mPipeline->getRenderPass())) {
        LOGE("Failed to initialize VulkanSwapchain(Framebuffers)");
        return false;
    }

    // 13. Command Pool 생성
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = mContext->getGraphicsQueueFamilyIndex();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(mContext->getDevice(), &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS) {
        LOGE("Failed to create command pool");
        return false;
    }

    mSync = std::make_unique<VulkanSync>(mContext->getDevice(), MAX_FRAMES_IN_FLIGHT);
    if (!mSync->initialize()) {
        LOGE("Failed to initialize VulkanSync");
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
    if (mContext && mContext->getDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(mContext->getDevice()); // 모든 작업(GPU)이 끝날 때까지 대기

        if (mDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(mContext->getDevice(), mDescriptorPool, nullptr);
        }
        if (mCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(mContext->getDevice(), mCommandPool, nullptr);
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
    renderPassInfo.framebuffer = mSwapchain->getFramebuffers()[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = mSwapchain->getExtent();

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
    viewport.y = (float)mSwapchain->getExtent().height;
    viewport.width = (float)mSwapchain->getExtent().width;
    viewport.height = -(float)mSwapchain->getExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = mSwapchain->getExtent();
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
        mSwapchain->recreate(mPipeline->getRenderPass());
        return;
    }

    // 이전 프레임 작업이 끝날 때까지 대기
    VkFence inFlightFence = mSync->getInFlightFence(mCurrentFrame);
    vkWaitForFences(mContext->getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(mContext->getDevice(), 1, &inFlightFence);

    // Uniform Buffer 업데이트 (회전 및 종횡비 계산)
    updateUniformBuffer(mCurrentFrame);

    // 스왑체인에서 이미지 가져오기
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(mContext->getDevice(), mSwapchain->getSwapchain(), UINT64_MAX, mSync->getImageAvailableSemaphore(mCurrentFrame), VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        LOGI("Failed to acquire next image by VK_ERROR_OUT_OF_DATE_KHR");
        mSwapchain->recreate(mPipeline->getRenderPass());
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

    VkSemaphore waitSemaphores[] = {mSync->getImageAvailableSemaphore(mCurrentFrame)};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &mCommandBuffers[mCurrentFrame];

    VkSemaphore signalSemaphores[] = {mSync->getRenderFinishedSemaphore(mCurrentFrame)};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(mContext->getGraphicsQueue(), 1, &submitInfo, mSync->getInFlightFence(mCurrentFrame)) != VK_SUCCESS) {
        LOGE("Failed to submit draw command buffer");
    }

    // 5. 화면에 표시 (Present)
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {mSwapchain->getSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(mContext->getGraphicsQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        LOGI("Failed to queue present by VK_ERROR_OUT_OF_DATE_KHR");
        mSwapchain->recreate(mPipeline->getRenderPass());
    }

    // 6. 다음 프레임 인덱스로 교체
    mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::updateUniformBuffer(uint32_t currentImage) {
    UniformBufferObject ubo{};
    ubo.mvp = glm::mat4(1.0f);

    // 1. 기기 회전(Surface Transform) 보정
    // 기기가 물리적으로 돌아간 만큼 반대 방향으로 회전 행렬을 적용합니다.
    if (mSwapchain->getTransform() == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR) {
        ubo.mvp = glm::rotate(ubo.mvp, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    } else if (mSwapchain->getTransform() == VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR) {
        ubo.mvp = glm::rotate(ubo.mvp, glm::radians(-180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    } else if (mSwapchain->getTransform() == VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) {
        ubo.mvp = glm::rotate(ubo.mvp, glm::radians(-270.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    }

    // 2. 종횡비 보정 (가장 안정적인 'Short-side' 기준 방식)
    // 물리적 해상도를 가져옵니다.
    float width = static_cast<float>(mSwapchain->getExtent().width);
    float height = static_cast<float>(mSwapchain->getExtent().height);

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
