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

    mSync = std::make_unique<VulkanSync>(mContext->getDevice(), MAX_FRAMES_IN_FLIGHT);
    if (!mSync->initialize()) {
        LOGE("Failed to initialize VulkanSync");
        return false;
    }

    mCommand = std::make_unique<VulkanCommand>(mContext->getDevice(), mContext->getGraphicsQueueFamilyIndex());
    if (!mCommand->initialize(MAX_FRAMES_IN_FLIGHT)) {
        LOGE("Failed to initialize VulkanCommand");
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

    mDescriptor = std::make_unique<VulkanDescriptor>(mContext->getDevice(), MAX_FRAMES_IN_FLIGHT);
    if (!mDescriptor->initialize(mPipeline->getDescriptorSetLayout(), mUniformBuffers)) {
        LOGE("Failed to initialize VulkanDescriptor");
        return false;
    }

    mCamera = std::make_unique<Camera>();

    createVertexBuffer();

    LOGI("Vulkan Initialization Wrap-up Successful!");

    return true;
}

Renderer::~Renderer() {
    // Device 레벨 객체들 해제
    if (mContext && mContext->getDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(mContext->getDevice()); // 모든 작업(GPU)이 끝날 때까지 대기
    }
}

void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
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

    // Descriptor Set 바인딩 (UBO 데이터 연결)
    VkDescriptorSet set = mDescriptor->getSet(mCurrentFrame);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline->getPipelineLayout(), 0, 1, &set, 0, nullptr);

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
    mMesh->draw(commandBuffer);

    vkCmdEndRenderPass(commandBuffer);
}

void Renderer::createVertexBuffer() {
    std::vector<Vertex> vertices = {
            {{0.0f, 0.5f}},   // 위
            {{-0.5f, -0.5f}}, // 왼쪽 아래
            {{0.5f, -0.5f}}   // 오른쪽 아래
    };

    mMesh = std::make_unique<VulkanMesh>(mContext.get(), vertices);
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

    // 커맨드 버퍼 기록
    mCommand->reset(mCurrentFrame);
    mCommand->begin(mCurrentFrame, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    recordCommandBuffer(mCommand->getBuffer(mCurrentFrame), imageIndex);
    mCommand->end(mCurrentFrame);

    // GPU 큐에 제출
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {mSync->getImageAvailableSemaphore(mCurrentFrame)};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    VkCommandBuffer commandBuffer = mCommand->getBuffer(mCurrentFrame);
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkSemaphore signalSemaphores[] = {mSync->getRenderFinishedSemaphore(mCurrentFrame)};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(mContext->getGraphicsQueue(), 1, &submitInfo, mSync->getInFlightFence(mCurrentFrame)) != VK_SUCCESS) {
        LOGE("Failed to submit draw command buffer");
    }

    // 화면에 표시 (Present)
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

    // 다음 프레임 인덱스로 교체
    mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::updateUniformBuffer(uint32_t currentImage) {
    mCamera->update(mSwapchain->getExtent().width,
                    mSwapchain->getExtent().height,
                    mSwapchain->getTransform());

    UniformBufferObject ubo{};
    ubo.mvp = mCamera->getMVPMatrix();
    // GPU 메모리로 복사
    mUniformBuffers[currentImage]->copyTo(&ubo, sizeof(ubo));
}
