#pragma once

#include "volk.h"
#include "vulkan_types.h"
#include "asset_utils.h"

#include <vector>
#include <cstdint>

struct PipelineConfig {
    std::string vertShaderPath;
    std::string fragShaderPath;
    bool depthOnly = false;         // Shadow용 (Color 출력 없음)
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    float depthBiasConstant = 0.0f; // Shadow Acne 방지용
    float depthBiasSlope = 0.0f;
};

class VulkanPipeline {
public:
    explicit VulkanPipeline(VkDevice device);
    ~VulkanPipeline();

    // Disable copying
    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    bool initialize(VkFormat swapchainImageFormat, VkFormat depthFormat,
                    AAssetManager* assetManager, const PipelineConfig& config);

    VkRenderPass getRenderPass() const { return mRenderPass; }
    VkPipelineLayout getPipelineLayout() const { return mPipelineLayout; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return mDescriptorSetLayout; }
    VkPipeline getGraphicsPipeline() const { return mGraphicsPipeline; }

private:
    VkDevice mDevice;

    VkRenderPass mRenderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mGraphicsPipeline = VK_NULL_HANDLE;

    PipelineConfig mConfig;

    bool createRenderPass(VkFormat imageFormat, VkFormat depthFormat);
    bool createDescriptorSetLayout();
    bool createGraphicsPipeline(AAssetManager* assetManager);
};
