#include "DescriptorLayouts.h"
#include "Log.h"

#include <vector>

DescriptorLayouts::DescriptorLayouts(VkDevice device) : mDevice(device) {
}

DescriptorLayouts::~DescriptorLayouts() {
    if (mShadowGlobalSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(mDevice, mShadowGlobalSetLayout, nullptr);
    }
    if (mMainMaterialSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(mDevice, mMainMaterialSetLayout, nullptr);
    }
    if (mMainGlobalSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(mDevice, mMainGlobalSetLayout, nullptr);
    }
}

bool DescriptorLayouts::initialize() {
    // set = 0 (main): global (UBO + shadow)
    std::vector<VkDescriptorSetLayoutBinding> mainGlobalBindings;
    mainGlobalBindings.push_back({
            0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
    mainGlobalBindings.push_back({
            2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
            VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});

    VkDescriptorSetLayoutCreateInfo mainGlobalInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    mainGlobalInfo.bindingCount = static_cast<uint32_t>(mainGlobalBindings.size());
    mainGlobalInfo.pBindings = mainGlobalBindings.data();

    if (vkCreateDescriptorSetLayout(mDevice, &mainGlobalInfo, nullptr, &mMainGlobalSetLayout) != VK_SUCCESS) {
        LOGE("Failed to create main global descriptor set layout");
        return false;
    }

    // set = 1 (main): material (base texture)
    std::vector<VkDescriptorSetLayoutBinding> materialBindings;
    materialBindings.push_back({
            1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
            VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});

    VkDescriptorSetLayoutCreateInfo materialInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    materialInfo.bindingCount = static_cast<uint32_t>(materialBindings.size());
    materialInfo.pBindings = materialBindings.data();

    if (vkCreateDescriptorSetLayout(mDevice, &materialInfo, nullptr, &mMainMaterialSetLayout) != VK_SUCCESS) {
        LOGE("Failed to create main material descriptor set layout");
        vkDestroyDescriptorSetLayout(mDevice, mMainGlobalSetLayout, nullptr);
        mMainGlobalSetLayout = VK_NULL_HANDLE;
        return false;
    }

    // set = 0 (shadow): global (UBO only)
    std::vector<VkDescriptorSetLayoutBinding> shadowGlobalBindings;
    shadowGlobalBindings.push_back({
            0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
            VK_SHADER_STAGE_VERTEX_BIT, nullptr});

    VkDescriptorSetLayoutCreateInfo shadowGlobalInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    shadowGlobalInfo.bindingCount = static_cast<uint32_t>(shadowGlobalBindings.size());
    shadowGlobalInfo.pBindings = shadowGlobalBindings.data();

    if (vkCreateDescriptorSetLayout(mDevice, &shadowGlobalInfo, nullptr, &mShadowGlobalSetLayout) != VK_SUCCESS) {
        LOGE("Failed to create shadow global descriptor set layout");
        vkDestroyDescriptorSetLayout(mDevice, mMainMaterialSetLayout, nullptr);
        mMainMaterialSetLayout = VK_NULL_HANDLE;
        vkDestroyDescriptorSetLayout(mDevice, mMainGlobalSetLayout, nullptr);
        mMainGlobalSetLayout = VK_NULL_HANDLE;
        return false;
    }

    return true;
}
