#include "RenderGraph.h"
#include "Log.h"
#include <cstdint>
#include <string>
#include <unordered_set>

#ifndef DEBUG_LOG_RG
#define DEBUG_LOG_RG 0
#endif

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
        }
        for (const auto& w : pass.writes) {
            produced.insert(w.name);
        }
    }

    if (DEBUG_LOG_RG) {
        for (size_t i = 0; i < mPasses.size(); ++i) {
            LOGV("[RG] pass %zu: %s", i, mPasses[i].name.c_str());
        }
    }
    return true;
}

namespace {
const char* toLayoutName(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED: return "UNDEFINED";
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return "COLOR_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return "DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return "DEPTH_STENCIL_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return "SHADER_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return "PRESENT_SRC_KHR";
        default: return "OTHER_LAYOUT";
    }
}

std::string stageFlagsToString(VkPipelineStageFlags flags) {
    if (flags == 0) return "0";

    std::string out;
    auto append = [&](VkPipelineStageFlags bit, const char* name) {
        if ((flags & bit) == 0) return;
        if (!out.empty()) out += "|";
        out += name;
    };

    append(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "TOP_OF_PIPE");
    append(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, "BOTTOM_OF_PIPE");
    append(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, "COLOR_ATTACHMENT_OUTPUT");
    append(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, "EARLY_FRAGMENT_TESTS");
    append(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, "LATE_FRAGMENT_TESTS");
    append(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, "FRAGMENT_SHADER");
    append(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, "ALL_COMMANDS");

    return out.empty() ? "UNKNOWN_STAGE_BITS" : out;
}

std::string accessFlagsToString(VkAccessFlags flags) {
    if (flags == 0) return "0";

    std::string out;
    auto append = [&](VkAccessFlags bit, const char* name) {
        if ((flags & bit) == 0) return;
        if (!out.empty()) out += "|";
        out += name;
    };

    append(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, "COLOR_ATTACHMENT_READ");
    append(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "COLOR_ATTACHMENT_WRITE");
    append(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, "DEPTH_STENCIL_READ");
    append(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, "DEPTH_STENCIL_WRITE");
    append(VK_ACCESS_SHADER_READ_BIT, "SHADER_READ");
    append(VK_ACCESS_MEMORY_READ_BIT, "MEMORY_READ");
    append(VK_ACCESS_MEMORY_WRITE_BIT, "MEMORY_WRITE");

    return out.empty() ? "UNKNOWN_ACCESS_BITS" : out;
}

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
                                     const std::string& passName,
                                     const std::string& usageKind,
                                     const std::string& name,
                                     VkImageLayout newLayout) {
    auto it = mResources.find(name);
    if (it == mResources.end()) {
        LOGE("[RG] transition requested for unknown resource '%s'", name.c_str());
        return false;
    }

    auto& res = it->second;
    if (res.currentLayout == newLayout) {
        if (DEBUG_LOG_RG) {
            LOGV("[RG][%s] %s '%s': already %s, barrier skipped",
                 passName.c_str(),
                 usageKind.c_str(),
                 name.c_str(),
                 toLayoutName(newLayout));
        }
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

    if (DEBUG_LOG_RG) {
        LOGV("[RG][%s] %s '%s' (image=%p): %s -> %s",
             passName.c_str(),
             usageKind.c_str(),
             name.c_str(),
             reinterpret_cast<void*>(res.image),
             toLayoutName(barrier.oldLayout),
             toLayoutName(barrier.newLayout));
        LOGV("[RG][%s]   barrier srcStage=%s srcAccess=%s",
             passName.c_str(),
             stageFlagsToString(srcStage).c_str(),
             accessFlagsToString(barrier.srcAccessMask).c_str());
        LOGV("[RG][%s]   barrier dstStage=%s dstAccess=%s",
             passName.c_str(),
             stageFlagsToString(dstStage).c_str(),
             accessFlagsToString(barrier.dstAccessMask).c_str());
    }

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
        if (DEBUG_LOG_RG) {
            LOGV("[RG][%s] post-layout '%s': %s -> %s",
                 pass.name.c_str(),
                 kv.first.c_str(),
                 toLayoutName(it->second.currentLayout),
                 toLayoutName(kv.second));
        }
        it->second.currentLayout = kv.second;
    }
}

void RenderGraph::execute(VkCommandBuffer cmd) {
    for (size_t passIndex = 0; passIndex < mPasses.size(); ++passIndex) {
        const auto& pass = mPasses[passIndex];
        if (DEBUG_LOG_RG) {
            LOGV("[RG] ---- Pass %zu begin: %s ----", passIndex, pass.name.c_str());
        }
        for (const auto& w : pass.writes) {
            if (!transitionResource(cmd, pass.name, "write", w.name, w.layout)) {
                LOGE("[RG] failed to transition write resource '%s' in pass '%s'",
                     w.name.c_str(), pass.name.c_str());
            }
        }
        for (const auto& r : pass.reads) {
            if (!transitionResource(cmd, pass.name, "read", r.name, r.layout)) {
                LOGE("[RG] failed to transition read resource '%s' in pass '%s'",
                     r.name.c_str(), pass.name.c_str());
            }
        }
        if (DEBUG_LOG_RG) {
            LOGV("[RG][%s] record() begin", pass.name.c_str());
        }
        pass.record(cmd);
        if (DEBUG_LOG_RG) {
            LOGV("[RG][%s] record() end", pass.name.c_str());
        }
        applyPostPassLayouts(pass);
        if (DEBUG_LOG_RG) {
            LOGV("[RG] ---- Pass %zu end: %s ----", passIndex, pass.name.c_str());
        }
    }
}

bool RenderGraph::tryGetResourceLayout(const std::string& name, VkImageLayout& outLayout) const {
    auto it = mResources.find(name);
    if (it == mResources.end()) {
        return false;
    }
    outLayout = it->second.currentLayout;
    return true;
}
