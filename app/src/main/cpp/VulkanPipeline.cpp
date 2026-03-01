#include "VulkanPipeline.h"
#include "Log.h"
#include "vulkan_types.h"
#include <cstring>

namespace {
VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        LOGE("Failed to create shader module");
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}
} // namespace


VulkanPipeline::VulkanPipeline(VkDevice device) : mDevice(device) {
}

VulkanPipeline::~VulkanPipeline() {
    if (mGraphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
    }
    if (mPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
    }
    if (mRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
    }
}

bool VulkanPipeline::initialize(VkFormat swapchainImageFormat, VkFormat depthFormat,
                                const AssetProvider& assetProvider,
                                VkDescriptorSetLayout globalSetLayout,
                                VkDescriptorSetLayout materialSetLayout,
                                const PipelineConfig& config) {
    mConfig = config;
    mGlobalSetLayout = globalSetLayout;
    mMaterialSetLayout = materialSetLayout;
    if (mGlobalSetLayout == VK_NULL_HANDLE) {
        LOGE("Global descriptor set layout must not be null");
        return false;
    }
    if (mConfig.setProfile == PipelineConfig::SetProfile::Main &&
            mMaterialSetLayout == VK_NULL_HANDLE) {
        LOGE("Main pipeline requires material descriptor set layout");
        return false;
    }
    if (!createRenderPass(swapchainImageFormat, depthFormat)) return false;
    if (!createGraphicsPipeline(assetProvider)) return false;
    return true;
}

bool VulkanPipeline::createRenderPass(VkFormat imageFormat, VkFormat depthFormat) {
    std::vector<VkAttachmentDescription> attachments;
    VkAttachmentReference colorAttachmentRef = {};

    // 1. Color Attachment
    if (!mConfig.depthOnly) {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = imageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments.push_back(colorAttachment);

        colorAttachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    }

    // 2. Depth Attachment
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = mConfig.depthOnly ?
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL :
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments.push_back(depthAttachment);

    uint32_t depthIndex = mConfig.depthOnly ? 0 : 1;
    VkAttachmentReference depthAttachmentRef = { depthIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    if (!mConfig.depthOnly) {
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
    }
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency = {};
    if (mConfig.depthOnly) {
        // Shadow depth를 이 렌더패스에서 쓰고, 이후(main pass fragment)에서 읽을 수 있게 보장
        dependency.srcSubpass = 0;
        dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    } else {
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS) {
        LOGE("Failed to create Render Pass");
        return false;
    }
    return true;
}

bool VulkanPipeline::createGraphicsPipeline(const AssetProvider& assetProvider) {
    auto loadSpirv = [&assetProvider](const std::string& path, std::vector<uint32_t>& outWords) -> bool {
        std::vector<uint8_t> bytes;
        if (!assetProvider.readBinaryFile(path, bytes)) {
            LOGE("Failed to load shader asset: %s", path.c_str());
            return false;
        }
        if (bytes.empty() || (bytes.size() % sizeof(uint32_t)) != 0) {
            LOGE("Invalid SPIR-V size for shader: %s", path.c_str());
            return false;
        }
        outWords.resize(bytes.size() / sizeof(uint32_t));
        memcpy(outWords.data(), bytes.data(), bytes.size());
        return true;
    };

    // 1. Shader Modules
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    std::vector<uint32_t> vertCode;
    if (!loadSpirv(mConfig.vertShaderPath, vertCode)) {
        return false;
    }
    VkShaderModule vertShader = createShaderModule(mDevice, vertCode);
    if (vertShader == VK_NULL_HANDLE) {
        return false;
    }
    shaderStages.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr,
                            0, VK_SHADER_STAGE_VERTEX_BIT, vertShader, "main"});

    VkShaderModule fragShader = VK_NULL_HANDLE;
    if (!mConfig.depthOnly && !mConfig.fragShaderPath.empty()) {
        std::vector<uint32_t> fragCode;
        if (!loadSpirv(mConfig.fragShaderPath, fragCode)) {
            vkDestroyShaderModule(mDevice, vertShader, nullptr);
            return false;
        }
        fragShader = createShaderModule(mDevice, fragCode);
        if (fragShader == VK_NULL_HANDLE) {
            vkDestroyShaderModule(mDevice, vertShader, nullptr);
            return false;
        }
        shaderStages.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr,
                                0, VK_SHADER_STAGE_FRAGMENT_BIT, fragShader, "main"});
    }

    // 2. Vertex Input
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDescription;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

    // 3. Fixed Functions
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = mConfig.cullMode;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = mConfig.depthOnly ? VK_TRUE : VK_FALSE;
    rasterizer.depthBiasConstantFactor = mConfig.depthBiasConstant;
    rasterizer.depthBiasSlopeFactor = mConfig.depthBiasSlope;

    VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cbAtt = {};
    cbAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cbAtt.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    if (mConfig.depthOnly) {
        colorBlending.attachmentCount = 0;
        colorBlending.pAttachments = nullptr;
    } else {
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &cbAtt;
    }

    VkPipelineDepthStencilStateCreateInfo depthStencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS; // 작을수록 앞에 있음
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    if (mConfig.depthOnly) {
        dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS); // For vkCmdSetDepthBias()
    }
    VkPipelineDynamicStateCreateInfo dynamicStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateInfo.pDynamicStates = dynamicStates.data();

    // 4. Pipeline Layout
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);

    VkDescriptorSetLayout mainSetLayouts[] = {
        mGlobalSetLayout,   // set = 0
        mMaterialSetLayout  // set = 1
    };

    VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    if (mConfig.setProfile == PipelineConfig::SetProfile::Shadow) {
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &mGlobalSetLayout; // set = 0 only
    } else {
        layoutInfo.setLayoutCount = 2;
        layoutInfo.pSetLayouts = mainSetLayouts;
    }
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(mDevice, &layoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(mDevice, vertShader, nullptr);
        if (fragShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(mDevice, fragShader, nullptr);
        }
        return false;
    }

    // 5. Final Creation
    VkGraphicsPipelineCreateInfo pipeInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipeInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipeInfo.pStages = shaderStages.data();
    pipeInfo.pVertexInputState = &vertexInput;
    pipeInfo.pInputAssemblyState = &inputAssembly;
    pipeInfo.pViewportState = &viewportState;
    pipeInfo.pRasterizationState = &rasterizer;
    pipeInfo.pMultisampleState = &multisampling;
    pipeInfo.pColorBlendState = &colorBlending;
    pipeInfo.pDepthStencilState = &depthStencil;
    pipeInfo.pDynamicState = &dynamicStateInfo;
    pipeInfo.layout = mPipelineLayout;
    pipeInfo.renderPass = mRenderPass;
    pipeInfo.subpass = 0;

    VkResult res = vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &mGraphicsPipeline);

    vkDestroyShaderModule(mDevice, vertShader, nullptr);
    if (fragShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(mDevice, fragShader, nullptr);
    }
    return res == VK_SUCCESS;
}
