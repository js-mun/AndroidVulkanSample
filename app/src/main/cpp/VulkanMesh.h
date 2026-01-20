#pragma once

#include "volk.h"
#include "VulkanContext.h"
#include "VulkanBuffer.h"
#include "vulkan_types.h"
#include <vector>
#include <memory>

class VulkanMesh {
public:
    template<typename T>
    VulkanMesh(VulkanContext* context,
               const std::vector<Vertex>& vertices,
               const std::vector<T>& indices) {
        VkIndexType indexType = (sizeof(T) == sizeof(uint32_t))
                ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;

        initialize(context, vertices, indices.data(),
                   static_cast<uint32_t>(indices.size()), indexType);
    }
    ~VulkanMesh() = default;

    // 복사 방지
    VulkanMesh(const VulkanMesh&) = delete;
    VulkanMesh& operator=(const VulkanMesh&) = delete;

    void draw(VkCommandBuffer commandBuffer);

private:
    void initialize(VulkanContext* context,
                    const std::vector<Vertex>& vertices,
                    const void* indexData,
                    uint32_t indexCount,
                    VkIndexType indexType);

    std::unique_ptr<VulkanBuffer> mVertexBuffer;
    std::unique_ptr<VulkanBuffer> mIndexBuffer;
    uint32_t mVertexCount;
    uint32_t mIndexCount;
    VkIndexType mIndexType; // UINT16 or UINT32
};
