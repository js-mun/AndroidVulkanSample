#pragma once

#include "volk.h"
#include "vulkan_types.h"
#include "IAssetProvider.h"

#include <vector>
#include <cstdint>

struct PipelineConfig {
    enum class SetProfile {
        Main,
        Shadow
    };

    std::string vertShaderPath;
    std::string fragShaderPath;
    bool depthOnly = false;         // Shadow용 (Color 출력 없음)
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    float depthBiasConstant = 0.0f; // Shadow Acne 방지용
    float depthBiasSlope = 0.0f;
    SetProfile setProfile = SetProfile::Main;
};

class VulkanPipeline {
public:
    explicit VulkanPipeline(VkDevice device);
    ~VulkanPipeline();

    // Disable copying
    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    bool initialize(VkFormat swapchainImageFormat, VkFormat depthFormat,
                    const IAssetProvider& assetProvider,
                    VkDescriptorSetLayout globalSetLayout,
                    VkDescriptorSetLayout materialSetLayout,
                    const PipelineConfig& config);

    VkRenderPass getRenderPass() const { return mRenderPass; }
    VkPipelineLayout getPipelineLayout() const { return mPipelineLayout; }
    VkDescriptorSetLayout getGlobalSetLayout() const { return mGlobalSetLayout; }
    VkDescriptorSetLayout getMaterialSetLayout() const { return mMaterialSetLayout; }
    VkPipeline getGraphicsPipeline() const { return mGraphicsPipeline; }

private:
    VkDevice mDevice;

    VkRenderPass mRenderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout mGlobalSetLayout = VK_NULL_HANDLE;    // set = 0
    VkDescriptorSetLayout mMaterialSetLayout = VK_NULL_HANDLE;  // set = 1
    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mGraphicsPipeline = VK_NULL_HANDLE;

    PipelineConfig mConfig;

    bool createRenderPass(VkFormat imageFormat, VkFormat depthFormat);
    bool createGraphicsPipeline(const IAssetProvider& assetProvider);
};
