#include "VulkanMesh.h"
//
//VulkanMesh::VulkanMesh(VulkanContext* context,
//                       const std::vector<Vertex>& vertices,
//                       const std::vector<T>& indices) {
//    // 스테이징 버퍼(CPU용) 생성 없이 간단히 직접 생성 (학습용)
//    mVertexCount = static_cast<uint32_t>(vertices.size());
//    VkDeviceSize bufferSize = sizeof(Vertex) * mVertexCount;
//    mVertexBuffer = std::make_unique<VulkanBuffer>(
//            context->getDevice(),
//            context->getPhysicalDevice(),
//            bufferSize,
//            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
//            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
//    );
//    mVertexBuffer->copyTo(vertices.data(), bufferSize); // map->copy->unmap
//
//    mIndexCount = static_cast<uint32_t>(indices.size());
//    VkDeviceSize indexBufferSize = sizeof(uint32_t) * mIndexCount;
//    mIndexBuffer = std::make_unique<VulkanBuffer>(
//            context->getDevice(), context->getPhysicalDevice(), indexBufferSize,
//            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
//            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
//    );
//    mIndexBuffer->copyTo(indices.data(), indexBufferSize);
//}

void VulkanMesh::initialize(VulkanContext* context,
                const std::vector<Vertex>& vertices,
                const void* indexData,
                uint32_t indexCount,
                VkIndexType indexType) {
    mIndexCount = indexCount;
    mIndexType = indexType;

    // 1. Vertex Buffer 생성
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
    mVertexBuffer = std::make_unique<VulkanBuffer>(
            context->getDevice(), context->getPhysicalDevice(), vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mVertexBuffer->copyTo(vertices.data(), vertexBufferSize);

    // 2. Index Buffer 생성 (인덱스 타입에 따른 크기 계산)
    VkDeviceSize indexElementSize = (mIndexType == VK_INDEX_TYPE_UINT32) ? sizeof(uint32_t) : sizeof(uint16_t);
    VkDeviceSize indexBufferSize = indexElementSize * mIndexCount;
    mIndexBuffer = std::make_unique<VulkanBuffer>(
            context->getDevice(), context->getPhysicalDevice(), indexBufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mIndexBuffer->copyTo(indexData, indexBufferSize);
}

void VulkanMesh::draw(VkCommandBuffer commandBuffer) {
    VkBuffer vertexBuffers[] = { mVertexBuffer->getBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(commandBuffer, mIndexCount, 1, 0, 0, 0);
}
