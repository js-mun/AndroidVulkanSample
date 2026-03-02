#include "RenderGraph.h"
#include "Log.h"
#include <cstdint>
#include <unordered_set>

void RenderGraph::reset() {
    mPasses.clear();
    mResources.clear();
}

void RenderGraph::addPass(Pass pass) {
    mPasses.push_back(std::move(pass));
}

void RenderGraph::registerResource(const std::string& name,
                                   VkImage image,
                                   VkFormat format,
                                   VkImageLayout initialLayout,
                                   VkImageAspectFlags aspectMask) {
    mResources[name] = {image, format, initialLayout, aspectMask};
}

bool RenderGraph::compile(const std::vector<std::string>& externalResources) {
    std::unordered_set<std::string> produced;
    std::unordered_set<std::string> external(externalResources.begin(), externalResources.end());

    for (const auto& pass : mPasses) {
        for (const auto& r : pass.reads) {
            if (produced.find(r.name) == produced.end() && external.find(r.name) == external.end()) {
                LOGW("[RG] Pass '%s' reads resource '%s' with no producer (treated as external?)",
                     pass.name.c_str(), r.name.c_str());
            }
            if (mResources.find(r.name) == mResources.end()) {
                LOGE("[RG] Pass '%s' reads unknown resource '%s'", pass.name.c_str(), r.name.c_str());
                return false;
            }
        }
        for (const auto& w : pass.writes) {
            produced.insert(w.name);
            if (mResources.find(w.name) == mResources.end()) {
                LOGE("[RG] Pass '%s' writes unknown resource '%s'", pass.name.c_str(), w.name.c_str());
                return false;
            }
        }
        for (const auto& kv : pass.postLayouts) {
            if (mResources.find(kv.first) == mResources.end()) {
                LOGE("[RG] Pass '%s' post-layout for unknown resource '%s'",
                     pass.name.c_str(), kv.first.c_str());
                return false;
            }
        }
    }

    if (DEBUG_LOG) {
        for (size_t i = 0; i < mPasses.size(); ++i) {
            LOGV("[RG] pass %zu: %s", i, mPasses[i].name.c_str());
        }
    }
    return true;
}

namespace {
void inferBarrierParams(VkImageLayout oldLayout,
                        VkImageLayout newLayout,
                        VkImageAspectFlags aspectMask,
                        VkAccessFlags& srcAccess,
                        VkAccessFlags& dstAccess,
                        VkPipelineStageFlags& srcStage,
                        VkPipelineStageFlags& dstStage) {
    (void)aspectMask;
    auto isDepth = (aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;

    auto srcFromLayout = [&](VkImageLayout layout) {
        switch (layout) {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                srcAccess = 0;
                srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                break;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                srcAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                srcStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
                srcAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                           VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                break;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                srcAccess = VK_ACCESS_SHADER_READ_BIT;
                srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                break;
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                srcAccess = 0;
                srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                break;
            default:
                srcAccess = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
                srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                break;
        }
    };

    auto dstFromLayout = [&](VkImageLayout layout) {
        switch (layout) {
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                dstAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                dstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
                dstAccess = isDepth ? (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
                                    : VK_ACCESS_SHADER_READ_BIT;
                dstStage = isDepth ? (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)
                                   : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                break;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                dstAccess = VK_ACCESS_SHADER_READ_BIT;
                dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                break;
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                dstAccess = 0;
                dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                break;
            default:
                dstAccess = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
                dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                break;
        }
    };

    srcFromLayout(oldLayout);
    dstFromLayout(newLayout);
}
} // namespace

bool RenderGraph::transitionResource(VkCommandBuffer cmd,
                                     const std::string& name,
                                     VkImageLayout newLayout) {
    auto it = mResources.find(name);
    if (it == mResources.end()) {
        LOGE("[RG] transition requested for unknown resource '%s'", name.c_str());
        return false;
    }

    auto& res = it->second;
    if (res.currentLayout == newLayout) {
        return true;
    }
    if (res.image == VK_NULL_HANDLE || res.aspectMask == 0) {
        LOGE("[RG] resource '%s' is not valid for barrier (image=%p, aspect=0x%x)",
             name.c_str(),
             reinterpret_cast<void*>(res.image),
             res.aspectMask);
        return false;
    }

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = res.currentLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = res.image;
    barrier.subresourceRange.aspectMask = res.aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage = 0;
    VkPipelineStageFlags dstStage = 0;
    inferBarrierParams(res.currentLayout, newLayout, res.aspectMask,
                       barrier.srcAccessMask, barrier.dstAccessMask, srcStage, dstStage);

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    res.currentLayout = newLayout;
    return true;
}

void RenderGraph::applyPostPassLayouts(const Pass& pass) {
    for (const auto& kv : pass.postLayouts) {
        auto it = mResources.find(kv.first);
        if (it == mResources.end()) {
            LOGW("[RG] post layout for unknown resource '%s' ignored", kv.first.c_str());
            continue;
        }
        it->second.currentLayout = kv.second;
    }
}

void RenderGraph::execute(VkCommandBuffer cmd) {
    for (const auto& pass : mPasses) {
        for (const auto& w : pass.writes) {
            if (!transitionResource(cmd, w.name, w.layout)) {
                LOGE("[RG] failed to transition write resource '%s' in pass '%s'",
                     w.name.c_str(), pass.name.c_str());
            }
        }
        for (const auto& r : pass.reads) {
            if (!transitionResource(cmd, r.name, r.layout)) {
                LOGE("[RG] failed to transition read resource '%s' in pass '%s'",
                     r.name.c_str(), pass.name.c_str());
            }
        }
        pass.record(cmd);
        applyPostPassLayouts(pass);
    }
}
