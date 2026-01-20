#include "VulkanMesh.h"

void VulkanMesh::initialize(VulkanContext* context,
                const std::vector<Vertex>& vertices,
                const void* indexData,
                uint32_t indexCount,
                VkIndexType indexType) {
    mIndexCount = indexCount;
    mIndexType = indexType;

    // 1. Vertex
    // Staging Buffer 생성
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
    VulkanBuffer stagingBufferVertex(
            context->getDevice(), context->getPhysicalDevice(), vertexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    stagingBufferVertex.copyTo(vertices.data(), vertexBufferSize);
    // Device Local Memory (GPU) 버퍼 생성
    mVertexBuffer = std::make_unique<VulkanBuffer>(
            context->getDevice(), context->getPhysicalDevice(), vertexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    // GPU 내부에서 복사 실행
    context->copyBuffer(stagingBufferVertex.getBuffer(), mVertexBuffer->getBuffer(), vertexBufferSize);


    // 2. Index
    // Staging Buffer 생성
    VkDeviceSize indexElementSize = (mIndexType == VK_INDEX_TYPE_UINT32) ? sizeof(uint32_t) : sizeof(uint16_t);
    VkDeviceSize indexBufferSize = indexElementSize * mIndexCount;
    VulkanBuffer stagingBufferIndex(
            context->getDevice(), context->getPhysicalDevice(), indexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    stagingBufferIndex.copyTo(indexData, indexBufferSize);
    // Device Local Memory (GPU) 버퍼 생성
    mIndexBuffer = std::make_unique<VulkanBuffer>(
            context->getDevice(), context->getPhysicalDevice(), indexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // GPU 내부에서 복사 실행
    context->copyBuffer(stagingBufferIndex.getBuffer(), mIndexBuffer->getBuffer(), indexBufferSize);
}

void VulkanMesh::draw(VkCommandBuffer commandBuffer) {
    VkBuffer vertexBuffers[] = { mVertexBuffer->getBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer->getBuffer(), 0, mIndexType);

    vkCmdDrawIndexed(commandBuffer, mIndexCount, 1, 0, 0, 0);
}
