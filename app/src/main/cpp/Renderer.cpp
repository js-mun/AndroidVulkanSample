#include "Renderer.h"
#include "Log.h"
#include "vulkan_types.h"

#include <array>
#include <vector>

#ifndef DEBUG_LOG_RG
#define DEBUG_LOG_RG DEBUG_LOG
#endif

Renderer::Renderer(ISurfaceProvider& surfaceProvider, IAssetProvider& assetProvider)
        : mSurfaceProvider(surfaceProvider), mAssetProvider(assetProvider) {
}

bool Renderer::initialize() {
    // 1. volk 초기화 (Vulkan 로더 로드)
    if (volkInitialize() != VK_SUCCESS) {
        LOGE("Failed to initialize volk");
        return false;
    }

    mContext = std::make_unique<VulkanContext>(mSurfaceProvider);
    if (!mContext->initialize()) {
        LOGE("Failed to initialize VulkanContext");
        return false;
    }

    mSwapchain = std::make_unique<VulkanSwapchain>(mContext.get());
    if (!mSwapchain->createSwapchainAndViews()) {
        LOGE("Failed to initialize VulkanSwapchain(Swapchain and Views)");
        return false;
    }

    mDescriptorLayouts = std::make_unique<DescriptorLayouts>(mContext->getDevice());
    if (!mDescriptorLayouts->initialize()) {
        LOGE("Failed to initialize descriptor layouts");
        return false;
    }

    PipelineConfig mainConfig;
    mainConfig.vertShaderPath = "shaders/main.vert.spv";
    mainConfig.fragShaderPath = "shaders/main.frag.spv";
    mainConfig.depthOnly = false;
    mainConfig.cullMode = VK_CULL_MODE_BACK_BIT;
    mainConfig.depthBiasConstant = 0.0f;
    mainConfig.depthBiasSlope = 0.0f;
    mainConfig.setProfile = PipelineConfig::SetProfile::Main;

    mMainPipeline = std::make_unique<VulkanPipeline>(mContext->getDevice());
    if (!mMainPipeline->initialize(mSwapchain->getImageFormat(), mSwapchain->getDepthFormat(),
                               mAssetProvider,
                               mDescriptorLayouts->getMainGlobalSetLayout(),
                               mDescriptorLayouts->getMainMaterialSetLayout(),
                               mainConfig)) {
        LOGE("Failed to initialize Vulkan Pipeline");
        return false;
    }

    if (!mSwapchain->createFramebuffers(mMainPipeline->getRenderPass())) {
        LOGE("Failed to initialize VulkanSwapchain(Framebuffers)");
        return false;
    }

    PipelineConfig shadowConfig;
    shadowConfig.vertShaderPath = "shaders/shadow.vert.spv";
    shadowConfig.depthOnly = true;
    shadowConfig.cullMode = VK_CULL_MODE_FRONT_BIT;
    shadowConfig.depthBiasConstant = 1.25f;
    shadowConfig.depthBiasSlope = 1.75f;
    shadowConfig.setProfile = PipelineConfig::SetProfile::Shadow;

    mShadowPipeline = std::make_unique<VulkanPipeline>(mContext->getDevice());
    if (!mShadowPipeline->initialize(mSwapchain->getImageFormat(),
            mSwapchain->getDepthFormat(), mAssetProvider,
            mDescriptorLayouts->getShadowGlobalSetLayout(),
            VK_NULL_HANDLE,
            shadowConfig)) {
        LOGE("Failed to initialize Vulkan Pipeline");
        return false;
    }

    mShadowResources = std::make_unique<ShadowResources>(mContext.get());
    if (!mShadowResources->initialize(mShadowPipeline->getRenderPass(),
            mSwapchain->getDepthFormat())) {
        LOGE("Failed to initialize shadow resources");
        return false;
    }

    mSync = std::make_unique<VulkanSync>(
            mContext->getDevice(), MAX_FRAMES_IN_FLIGHT);
    if (!mSync->initialize()) {
        LOGE("Failed to initialize VulkanSync");
        return false;
    }

    mCommand = std::make_unique<VulkanCommand>(
            mContext->getDevice(), mContext->getGraphicsQueueFamilyIndex());
    if (!mCommand->initialize(MAX_FRAMES_IN_FLIGHT)) {
        LOGE("Failed to initialize VulkanCommand");
        return false;
    }

    // 16. Uniform Buffers 생성
    mUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        mUniformBuffers[i] = std::make_unique<VulkanBuffer>(
                mContext->getAllocator(), sizeof(UniformBufferObject),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VMA_MEMORY_USAGE_CPU_TO_GPU
        );
        mUniformBuffers[i]->map();
    }

    mMainGlobalDescriptor = std::make_unique<GlobalDescriptor>(
            mContext->getDevice(), MAX_FRAMES_IN_FLIGHT, true);
    if (!mMainGlobalDescriptor->initialize(
            mDescriptorLayouts->getMainGlobalSetLayout(),
            mUniformBuffers,
            mShadowResources->getDepthView(),
            mShadowResources->getSampler())) {
        LOGE("Failed to initialize main global descriptor");
        return false;
    }

    mShadowGlobalDescriptor = std::make_unique<GlobalDescriptor>(
            mContext->getDevice(), MAX_FRAMES_IN_FLIGHT, false);
    if (!mShadowGlobalDescriptor->initialize(
            mDescriptorLayouts->getShadowGlobalSetLayout(),
            mUniformBuffers,
            VK_NULL_HANDLE,
            VK_NULL_HANDLE)) {
        LOGE("Failed to initialize shadow global descriptor");
        return false;
    }

    // 모델들을 로드하고 각 모델 디스크립터를 초기화합니다.
    const std::vector<std::string> modelPaths = {
            "glTF/plane.glb",
            "glTF/AnimatedColorsCube.glb",
    };
    for (const auto& path : modelPaths) {
        auto model = std::make_unique<VulkanModel>(mContext.get());
        if (!model->loadFromFile(mAssetProvider, path)) {
            LOGE("Failed to load model: %s", path.c_str());
            return false;
        }
        if (!model->initializeDescriptor(mDescriptorLayouts->getMainMaterialSetLayout(),
                MAX_FRAMES_IN_FLIGHT)) {
            LOGE("Failed to initialize model descriptor: %s", path.c_str());
            return false;
        }
        mModels.push_back(std::move(model));
        mModelTransforms.push_back(glm::mat4(1.0f));
    }

    // 예시: 큐브를 바닥 위로 살짝 올립니다.
    for (size_t i = 0; i < modelPaths.size(); ++i) {
        if (modelPaths[i].find("AnimatedColorsCube") != std::string::npos) {
            mModelTransforms[i] = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, 0.0f));
        }
    }

    mCamera = std::make_unique<Camera>();

    mRenderGraph = std::make_unique<RenderGraph>();
    resetTrackedLayouts();
    mStartTime = std::chrono::steady_clock::now();
    buildFrameGraph();

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
    mActiveImageIndex = imageIndex;
    if (!mFrameGraphReady) {
        buildFrameGraph();
    }
    updateFrameGraphResources();
    executeFrameGraph(commandBuffer);
}

void Renderer::buildFrameGraph() {
    mRenderGraph->reset();

    // 1) Shadow Pass
    mRenderGraph->addPass({
        "ShadowPass",
        {},                 // reads
        {{ "shadow_depth", VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }},   // writes
        {{"shadow_depth", VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL}},
        [this](VkCommandBuffer commandBuffer) {
            VkRenderPassBeginInfo rpInfo{};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.renderPass = mShadowPipeline->getRenderPass();
            rpInfo.framebuffer = mShadowResources->getFramebuffer();
            rpInfo.renderArea.offset = {0, 0};
            rpInfo.renderArea.extent = mShadowResources->getExtent();

            VkClearValue clear{};
            clear.depthStencil = {1.0f, 0};
            rpInfo.clearValueCount = 1;
            rpInfo.pClearValues = &clear;

            vkCmdBeginRenderPass(commandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              mShadowPipeline->getGraphicsPipeline());

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(mShadowResources->getExtent().width);
            viewport.height = static_cast<float>(mShadowResources->getExtent().height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = mShadowResources->getExtent();
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Shadow acne 완화를 위해 shadow pipeline 설정값과 동일한 bias를 사용
            vkCmdSetDepthBias(commandBuffer, 1.25f, 0.0f, 1.75f);

            VkDescriptorSet globalSet = mShadowGlobalDescriptor->getSet(mCurrentFrame);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    mShadowPipeline->getPipelineLayout(),
                                    0, 1, &globalSet, 0, nullptr);

            for (size_t i = 0; i < mModels.size(); ++i) {
                const auto& model = mModels[i];
                model->draw(commandBuffer,
                            mShadowPipeline->getPipelineLayout(),
                            mModelTransforms[i],
                            mElapsedTimeSec);
            }

            vkCmdEndRenderPass(commandBuffer);
        }
    });

    // 2) Main Pass
    mRenderGraph->addPass({
        "MainScene",
        {{ "shadow_depth", VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL }},
        {
            { "swapchain_color", VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
            { "swapchain_depth", VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }
        },
        {
            {"swapchain_color", VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
            {"swapchain_depth", VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}
        },
        [this](VkCommandBuffer commandBuffer) {
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = mMainPipeline->getRenderPass();
            renderPassInfo.framebuffer = mSwapchain->getFramebuffers()[mActiveImageIndex];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = mSwapchain->getExtent();

            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = {{0.2f, 0.2f, 0.2f, 1.0f}}; // 어두운 회색 클리어
            clearValues[1].depthStencil = {1.0f, 0};                     // 가장 먼 깊이(1.0)로 클리어
            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mMainPipeline->getGraphicsPipeline());

            // Descriptor Set 바인딩 (UBO 데이터 연결)
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

            VkDescriptorSet globalSet = mMainGlobalDescriptor->getSet(mCurrentFrame);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    mMainPipeline->getPipelineLayout(),
                                    0, 1, &globalSet, 0, nullptr);

            for (size_t i = 0; i < mModels.size(); ++i) {
                const auto& model = mModels[i];
                VkDescriptorSet set = model->getDescriptorSet();
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        mMainPipeline->getPipelineLayout(), 1, 1, &set, 0, nullptr);
                model->draw(commandBuffer,
                            mMainPipeline->getPipelineLayout(),
                            mModelTransforms[i],
                            mElapsedTimeSec);
            }

            vkCmdEndRenderPass(commandBuffer);
        }
    });

    if (!mRenderGraph->compile()) {
        LOGE("Failed to compile Render Graph");
        mFrameGraphReady = false;
        return;
    }
    mFrameGraphReady = true;
}

void Renderer::updateFrameGraphResources() {
    // 프레임마다 달라질 수 있는 실제 이미지 핸들과 현재 레이아웃을 갱신
    mRenderGraph->registerResource(
            "shadow_depth",
            mShadowResources->getDepthImage(),
            mSwapchain->getDepthFormat(),
            mShadowDepthLayout,
            VK_IMAGE_ASPECT_DEPTH_BIT);

    mRenderGraph->registerResource(
            "swapchain_color",
            mSwapchain->getImage(mActiveImageIndex),
            mSwapchain->getImageFormat(),
            mSwapchainColorLayouts[mActiveImageIndex],
            VK_IMAGE_ASPECT_COLOR_BIT);

    mRenderGraph->registerResource(
            "swapchain_depth",
            mSwapchain->getDepthImage(),
            mSwapchain->getDepthFormat(),
            mSwapchainDepthLayout,
            VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Renderer::executeFrameGraph(VkCommandBuffer commandBuffer) {
    if (DEBUG_LOG_RG) {
        LOGV("[RG] Frame execute: currentFrame=%u activeImageIndex=%u", mCurrentFrame, mActiveImageIndex);
    }
    mRenderGraph->execute(commandBuffer);

    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (mRenderGraph->tryGetResourceLayout("shadow_depth", layout)) {
        mShadowDepthLayout = layout;
    }
    if (mRenderGraph->tryGetResourceLayout("swapchain_depth", layout)) {
        mSwapchainDepthLayout = layout;
    }
    if (mRenderGraph->tryGetResourceLayout("swapchain_color", layout)) {
        mSwapchainColorLayouts[mActiveImageIndex] = layout;
    }
}

void Renderer::render() {
    if (mFramebufferResized) {
        LOGI("Buffer resized");
        mFramebufferResized = false;
        mSwapchain->recreate(mMainPipeline->getRenderPass());
        mShadowResources->recreate(mShadowPipeline->getRenderPass(),
                mSwapchain->getDepthFormat());
        mMainGlobalDescriptor->updateShadowMap(
                mShadowResources->getDepthView(),
                mShadowResources->getSampler());
        resetTrackedLayouts();
        mFrameGraphReady = false;
        return;
    }

    // 이전 프레임 작업이 끝날 때까지 대기
    VkFence inFlightFence = mSync->getInFlightFence(mCurrentFrame);
    vkWaitForFences(mContext->getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(mContext->getDevice(), 1, &inFlightFence);

    const auto now = std::chrono::steady_clock::now();
    mElapsedTimeSec = std::chrono::duration<float>(now - mStartTime).count();

    // Uniform Buffer 업데이트 (회전 및 종횡비 계산)
    updateUniformBuffer(mCurrentFrame);

    // 스왑체인에서 이미지 가져오기
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(mContext->getDevice(), mSwapchain->getSwapchain(),
        UINT64_MAX, mSync->getImageAvailableSemaphore(mCurrentFrame),
        VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        LOGI("Failed to acquire next image by VK_ERROR_OUT_OF_DATE_KHR");
        mSwapchain->recreate(mMainPipeline->getRenderPass());
        mShadowResources->recreate(mShadowPipeline->getRenderPass(),
                mSwapchain->getDepthFormat());
        mMainGlobalDescriptor->updateShadowMap(
                mShadowResources->getDepthView(),
                mShadowResources->getSampler());
        resetTrackedLayouts();
        mFrameGraphReady = false;
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

    if (vkQueueSubmit(mContext->getGraphicsQueue(), 1,
                      &submitInfo, inFlightFence) != VK_SUCCESS) {
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
        mSwapchain->recreate(mMainPipeline->getRenderPass());
        mShadowResources->recreate(mShadowPipeline->getRenderPass(),
                mSwapchain->getDepthFormat());
        mMainGlobalDescriptor->updateShadowMap(
                mShadowResources->getDepthView(),
                mShadowResources->getSampler());
        resetTrackedLayouts();
        mFrameGraphReady = false;
    }

    // 다음 프레임 인덱스로 교체
    mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::updateUniformBuffer(uint32_t currentImage) {
    // 1. 카메라 업데이트 (VP 행렬 계산)
    mCamera->update(static_cast<float>(mSwapchain->getExtent().width),
                    static_cast<float>(mSwapchain->getExtent().height),
                    mSwapchain->getTransform());

    // 2. 프레임 공통 UBO 작성 (모델 행렬은 push constant로 per-draw 전달)
    UniformBufferObject ubo{};
    ubo.viewProj = mCamera->getViewProjectionMatrix();
    ubo.lightDir = glm::vec4(glm::normalize(glm::vec3(-0.6f, -1.0f, -0.5f)), 0.0f);

    // 3. 라이트 VP
    const glm::vec3 lightTarget(0.0f, 0.0f, 0.0f);
    const float lightDistance = 12.0f;
    glm::vec3 lightPosition = lightTarget - glm::vec3(ubo.lightDir) * lightDistance;
    glm::mat4 lightView = glm::lookAtRH(
        lightPosition,
        lightTarget,
        glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 lightProj = glm::orthoRH_ZO(
        -8.0f, 8.0f,
        -8.0f, 8.0f,
        0.5f, 30.0f);

    // Vulkan Y 보정
    lightProj[1][1] *= -1.0f;

    ubo.lightViewProj = lightProj * lightView;


    // 4. GPU 전송
    mUniformBuffers[currentImage]->copyTo(&ubo, sizeof(ubo));
}

void Renderer::resetTrackedLayouts() {
    mShadowDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    mSwapchainDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    mSwapchainColorLayouts.assign(mSwapchain->getImageCount(), VK_IMAGE_LAYOUT_UNDEFINED);
}

void Renderer::handleTouchDrag(float dx, float dy) {
    // AndroidInputProvider에서 move delta를 프레임별 증분으로 넘기므로
    // 누적 델타 기준보다 더 큰 감도가 필요합니다.
    float sensitivity = 0.001f;
    mCamera->rotate(dx * sensitivity, dy * sensitivity);
}

void Renderer::handlePinchZoom(float delta) {
    float zoomSensitivity = 0.03f;
    mCamera->zoom(delta * zoomSensitivity);
}
