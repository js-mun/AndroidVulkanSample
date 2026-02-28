#pragma once

#include "volk.h"
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Camera.h"
#include "RenderGraph.h"
#include "ShadowResources.h"
#include "VulkanBuffer.h"
#include "VulkanCommand.h"
#include "VulkanContext.h"
#include "VulkanMesh.h"
#include "VulkanModel.h"
#include "VulkanPipeline.h"
#include "VulkanSwapchain.h"
#include "VulkanSync.h"

class Renderer {
public:
    bool mFramebufferResized = false;
    
    explicit Renderer(android_app* app);
    virtual ~Renderer();

    bool initialize();
    void render();

    void handleTouchDrag(float dx, float dy);
    void handlePinchZoom(float delta);

    void buildFrameGraph(uint32_t imageIndex);
    void executeFrameGraph(VkCommandBuffer commandBuffer);

private:
    android_app* mApp;
    std::unique_ptr<VulkanContext> mContext;
    std::unique_ptr<VulkanSwapchain> mSwapchain;
    std::unique_ptr<VulkanPipeline> mMainPipeline;
    std::unique_ptr<VulkanPipeline> mShadowPipeline;
    std::unique_ptr<ShadowResources> mShadowResources;
    std::unique_ptr<VulkanSync> mSync;
    std::unique_ptr<VulkanCommand> mCommand;

    std::vector<std::unique_ptr<VulkanModel>> mModels;

    std::unique_ptr<Camera> mCamera;

    std::unique_ptr<RenderGraph> mRenderGraph;

    uint32_t mCurrentFrame = 0;
    const int MAX_FRAMES_IN_FLIGHT = 2;

    std::vector<std::unique_ptr<VulkanBuffer>> mUniformBuffers;

    std::unique_ptr<VulkanMesh> mGroundMesh;

private:
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void updateUniformBuffer(uint32_t currentImage);
};
