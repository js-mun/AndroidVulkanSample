#include "DescriptorLayouts.h"
#include "Log.h"

#include <vector>

DescriptorLayouts::DescriptorLayouts(VkDevice device) : mDevice(device) {
}

DescriptorLayouts::~DescriptorLayouts() {
    if (mMaterialSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(mDevice, mMaterialSetLayout, nullptr);
    }
    if (mGlobalSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(mDevice, mGlobalSetLayout, nullptr);
    }
}

bool DescriptorLayouts::initialize() {
    // set = 0 : global (UBO + shadow)
    std::vector<VkDescriptorSetLayoutBinding> globalBindings;
    globalBindings.push_back({
            0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
    globalBindings.push_back({
            2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
            VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});

    VkDescriptorSetLayoutCreateInfo globalInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    globalInfo.bindingCount = static_cast<uint32_t>(globalBindings.size());
    globalInfo.pBindings = globalBindings.data();

    if (vkCreateDescriptorSetLayout(mDevice, &globalInfo, nullptr, &mGlobalSetLayout) != VK_SUCCESS) {
        LOGE("Failed to create global descriptor set layout");
        return false;
    }

    // set = 1 : material (base texture)
    std::vector<VkDescriptorSetLayoutBinding> materialBindings;
    materialBindings.push_back({
            1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
            VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});

    VkDescriptorSetLayoutCreateInfo materialInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    materialInfo.bindingCount = static_cast<uint32_t>(materialBindings.size());
    materialInfo.pBindings = materialBindings.data();

    if (vkCreateDescriptorSetLayout(mDevice, &materialInfo, nullptr, &mMaterialSetLayout) != VK_SUCCESS) {
        LOGE("Failed to create material descriptor set layout");
        vkDestroyDescriptorSetLayout(mDevice, mGlobalSetLayout, nullptr);
        mGlobalSetLayout = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

