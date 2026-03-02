#pragma once

#include "volk.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class RenderGraph {
public:
    struct ResourceUsage {
        std::string name;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct Pass {
        std::string name;
        std::vector<ResourceUsage> reads;
        std::vector<ResourceUsage> writes;
        std::unordered_map<std::string, VkImageLayout> postLayouts;
        std::function<void(VkCommandBuffer)> record;
    };

    struct Resource {
        VkImage image = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageAspectFlags aspectMask = 0;
    };

    void reset();
    void addPass(Pass pass);

    void registerResource(const std::string& name,
                          VkImage image,
                          VkFormat format,
                          VkImageLayout initialLayout,
                          VkImageAspectFlags aspectMask);

    bool compile(const std::vector<std::string>& externalResources = {});
    void execute(VkCommandBuffer cmd);

private:
    std::vector<Pass> mPasses;
    std::unordered_map<std::string, Resource> mResources;

    bool transitionResource(VkCommandBuffer cmd, const std::string& name, VkImageLayout newLayout);
    void applyPostPassLayouts(const Pass& pass);
};
