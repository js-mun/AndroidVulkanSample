#include "Renderer.h"
#include "Log.h"
#include "asset_utils.h"
#include "vulkan_types.h"

#include <array>
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

    PipelineConfig mainConfig;
    mainConfig.vertShaderPath = "shaders/main.vert.spv";
    mainConfig.fragShaderPath = "shaders/main.frag.spv";
    mainConfig.depthOnly = false;
    mainConfig.cullMode = VK_CULL_MODE_BACK_BIT;
    mainConfig.depthBiasConstant = 0.0f;
    mainConfig.depthBiasSlope = 0.0f;

    mMainPipeline = std::make_unique<VulkanPipeline>(mContext->getDevice());
    if (!mMainPipeline->initialize(mSwapchain->getImageFormat(), mSwapchain->getDepthFormat(),
                               mApp->activity->assetManager, mainConfig)) {
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
    shadowConfig.cullMode = VK_CULL_MODE_BACK_BIT;
    shadowConfig.depthBiasConstant = 1.25f;
    shadowConfig.depthBiasSlope = 1.75f;

    mShadowPipeline = std::make_unique<VulkanPipeline>(mContext->getDevice());
    if (!mShadowPipeline->initialize(mSwapchain->getImageFormat(), 
            mSwapchain->getDepthFormat(), mApp->activity->assetManager, shadowConfig)) {
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

    // 모델을 먼저 로드하여 텍스처를 확보한 뒤 각 모델 디스크립터를 초기화합니다.
    {
        auto model = std::make_unique<VulkanModel>(mContext.get());
        if (!model->loadFromFile(mApp->activity->assetManager, "glTF/AnimatedCube/AnimatedCube.gltf")) {
            LOGE("Failed to load glTF model!");
            return false;
        }
        if (!model->initializeDescriptor(mMainPipeline->getDescriptorSetLayout(),
                mUniformBuffers,
                MAX_FRAMES_IN_FLIGHT,
                mShadowResources->getDepthView(),
                mShadowResources->getSampler())) {
            LOGE("Failed to initialize model descriptor");
            return false;
        }
        mModels.push_back(std::move(model));
    }

    mCamera = std::make_unique<Camera>();

    mRenderGraph = std::make_unique<RenderGraph>();

    // Ground plane (y = -0.5)
    std::vector<Vertex> groundVertices = {
        {{-5.0f, -1.5f, -5.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 5.0f, -1.5f, -5.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 5.0f, -1.5f,  5.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{-5.0f, -1.5f,  5.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
    };
    std::vector<uint32_t> groundIndices = {
        0, 2, 1,
        2, 0, 3
    };
    mGroundMesh = std::make_unique<VulkanMesh>(mContext.get(), groundVertices, groundIndices);

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
    buildFrameGraph(imageIndex);
    executeFrameGraph(commandBuffer);
}

void Renderer::buildFrameGraph(uint32_t imageIndex) {
    mRenderGraph->reset();

    // 1) Shadow Pass
    mRenderGraph->addPass({
        "ShadowPass",
        {},                 // reads
        {"shadow_depth"},   // writes
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

            vkCmdSetDepthBias(commandBuffer, 0.0f, 0.0f, 0.0f);

            // Ground는 첫 모델의 디스크립터를 재사용합니다.
            if (mGroundMesh && !mModels.empty()) {
                VkDescriptorSet groundSet = mModels[0]->getDescriptorSet(mCurrentFrame);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        mShadowPipeline->getPipelineLayout(),
                                        0, 1, &groundSet, 0, nullptr);
                mGroundMesh->draw(commandBuffer);
            }
            for (const auto& model : mModels) {
                VkDescriptorSet set = model->getDescriptorSet(mCurrentFrame);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        mShadowPipeline->getPipelineLayout(),
                                        0, 1, &set, 0, nullptr);
                model->draw(commandBuffer);
            }

            vkCmdEndRenderPass(commandBuffer);
        }
    });

    // 2) Main Pass
    mRenderGraph->addPass({
        "MainScene",
        {"shadow_depth"},
        {"swapchain_color", "swapchain_depth"},
        [this, imageIndex](VkCommandBuffer commandBuffer) {
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = mMainPipeline->getRenderPass();
            renderPassInfo.framebuffer = mSwapchain->getFramebuffers()[imageIndex];
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
            
            // Ground는 첫 모델의 디스크립터를 재사용합니다.
            if (mGroundMesh && !mModels.empty()) {
                VkDescriptorSet groundSet = mModels[0]->getDescriptorSet(mCurrentFrame);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        mMainPipeline->getPipelineLayout(), 0, 1, &groundSet, 0, nullptr);
                mGroundMesh->draw(commandBuffer);
            }
            for (const auto& model : mModels) {
                VkDescriptorSet set = model->getDescriptorSet(mCurrentFrame);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        mMainPipeline->getPipelineLayout(), 0, 1, &set, 0, nullptr);
                model->draw(commandBuffer);
            }

            vkCmdEndRenderPass(commandBuffer);
        }
    });

    if (!mRenderGraph->compile({"swapchain_color", "swapchain_depth"})) {
        LOGE("Failed to compile Render Graph");
    }
}

void Renderer::executeFrameGraph(VkCommandBuffer commandBuffer) {
    mRenderGraph->execute(commandBuffer);
}

void Renderer::render() {
    if (mFramebufferResized) {
        LOGI("Buffer resized");
        mFramebufferResized = false;
        mSwapchain->recreate(mMainPipeline->getRenderPass());
        mShadowResources->recreate(mShadowPipeline->getRenderPass(),
                mSwapchain->getDepthFormat());
        for (auto& model : mModels) {
            model->updateShadowMap(mShadowResources->getDepthView(),
                    mShadowResources->getSampler());
        }
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
    VkResult result = vkAcquireNextImageKHR(mContext->getDevice(), mSwapchain->getSwapchain(),
        UINT64_MAX, mSync->getImageAvailableSemaphore(mCurrentFrame),
        VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        LOGI("Failed to acquire next image by VK_ERROR_OUT_OF_DATE_KHR");
        mSwapchain->recreate(mMainPipeline->getRenderPass());
        mShadowResources->recreate(mShadowPipeline->getRenderPass(),
                mSwapchain->getDepthFormat());
        for (auto& model : mModels) {
            model->updateShadowMap(mShadowResources->getDepthView(),
                    mShadowResources->getSampler());
        }
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
        for (auto& model : mModels) {
            model->updateShadowMap(mShadowResources->getDepthView(),
                    mShadowResources->getSampler());
        }
    }

    // 다음 프레임 인덱스로 교체
    mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::updateUniformBuffer(uint32_t currentImage) {
    // 1. 앱 시작 후 경과 시간 계산
    static auto startTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(
            currentTime - startTime).count();

    // 2. 카메라 업데이트 (VP 행렬 계산)
    mCamera->update(static_cast<float>(mSwapchain->getExtent().width),
                    static_cast<float>(mSwapchain->getExtent().height),
                    mSwapchain->getTransform());

    // 3. [핵심] 모델에게 현재 시간에 맞는 변환 행렬을 가져옴 -> 터치로 카메라 회전하도록 변경하여 주석처리.
    // glm::mat4 modelMatrix = mModel->getAnimationTransform(time);

    glm::mat4 modelMatrix = glm::mat4(1.0f);

    // 4. 최종 MVP 조합 (VP * M)
    UniformBufferObject ubo{};
    ubo.mvp = mCamera->getViewProjectionMatrix() * modelMatrix;

    // lightMVP 업데이트
    glm::mat4 lightView = glm::lookAt(
        glm::vec3(2.5f, 4.0f, 2.5f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    float near_plane = 1.0f;  // 이 값을 조절 (너무 작으면 정밀도 하락)
    float far_plane = 11.0f;  // 이 값을 조절 (너무 크면 정밀도 하락)
    // 조명이 태양광(지향성 광원)이라면 ortho를 사용
    glm::mat4 lightProj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f,
                                           near_plane, far_plane);
    // lightProj[1][1] *= -1.0f; // Vulkan clip correction

    ubo.lightMVP = lightProj * lightView * modelMatrix;


    // 5. GPU 전송
    mUniformBuffers[currentImage]->copyTo(&ubo, sizeof(ubo));
}

void Renderer::handleTouchDrag(float dx, float dy) {
    float sensitivity = 0.00003f;
    mCamera->rotate(dx * sensitivity, dy * sensitivity);
}

void Renderer::handlePinchZoom(float delta) {
    float zoomSensitivity = 0.01f;
    mCamera->zoom(delta * zoomSensitivity);
}
