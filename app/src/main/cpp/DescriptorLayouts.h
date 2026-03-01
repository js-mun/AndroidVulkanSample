#pragma once

#include "volk.h"

class DescriptorLayouts {
public:
    explicit DescriptorLayouts(VkDevice device);
    ~DescriptorLayouts();

    DescriptorLayouts(const DescriptorLayouts&) = delete;
    DescriptorLayouts& operator=(const DescriptorLayouts&) = delete;

    bool initialize();

    VkDescriptorSetLayout getMainGlobalSetLayout() const { return mMainGlobalSetLayout; }   // set = 0 (main)
    VkDescriptorSetLayout getMainMaterialSetLayout() const { return mMainMaterialSetLayout; } // set = 1 (main)
    VkDescriptorSetLayout getShadowGlobalSetLayout() const { return mShadowGlobalSetLayout; } // set = 0 (shadow)

private:
    VkDevice mDevice;
    VkDescriptorSetLayout mMainGlobalSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout mMainMaterialSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout mShadowGlobalSetLayout = VK_NULL_HANDLE;
};
