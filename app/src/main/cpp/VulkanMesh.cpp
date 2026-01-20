#include "VulkanMesh.h"

VulkanMesh::VulkanMesh(VulkanContext* context, const std::vector<Vertex>& vertices) {
    // 스테이징 버퍼(CPU용) 생성 없이 간단히 직접 생성 (학습용)
    mVertexCount = static_cast<uint32_t>(vertices.size());
    VkDeviceSize bufferSize = sizeof(Vertex) * mVertexCount;

    mVertexBuffer = std::make_unique<VulkanBuffer>(
            context->getDevice(), 
            context->getPhysicalDevice(), 
            bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    // 내부에서 map() -> copy -> unmap() 수행
    mVertexBuffer->copyTo(vertices.data(), bufferSize);
}

void VulkanMesh::draw(VkCommandBuffer commandBuffer) {
    VkBuffer vertexBuffers[] = { mVertexBuffer->getBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdDraw(commandBuffer, mVertexCount, 1, 0, 0);
}
