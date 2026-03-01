#pragma once

#include "volk.h"

class DescriptorLayouts {
public:
    explicit DescriptorLayouts(VkDevice device);
    ~DescriptorLayouts();

    DescriptorLayouts(const DescriptorLayouts&) = delete;
    DescriptorLayouts& operator=(const DescriptorLayouts&) = delete;

    bool initialize();

    VkDescriptorSetLayout getGlobalSetLayout() const { return mGlobalSetLayout; }     // set = 0
    VkDescriptorSetLayout getMaterialSetLayout() const { return mMaterialSetLayout; } // set = 1

private:
    VkDevice mDevice;
    VkDescriptorSetLayout mGlobalSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout mMaterialSetLayout = VK_NULL_HANDLE;
};

