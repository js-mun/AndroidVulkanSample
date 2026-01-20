#pragma once

#include "volk.h"
#include "VulkanContext.h"
#include "VulkanBuffer.h"
#include "vulkan_types.h"
#include <vector>
#include <memory>

class VulkanMesh {
public:
    VulkanMesh(VulkanContext* context, const std::vector<Vertex>& vertices);
    ~VulkanMesh() = default;

    // 복사 방지
    VulkanMesh(const VulkanMesh&) = delete;
    VulkanMesh& operator=(const VulkanMesh&) = delete;

    void draw(VkCommandBuffer commandBuffer);

private:
    std::unique_ptr<VulkanBuffer> mVertexBuffer;
    uint32_t mVertexCount;
};
