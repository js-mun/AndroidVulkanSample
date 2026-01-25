#pragma once

#include "VulkanMesh.h"
#include "VulkanContext.h"
#include <string>
#include <vector>
#include <memory>
#include <android/asset_manager.h>

namespace tinygltf {
    class Model;
}

class VulkanModel {
public:
    VulkanModel(VulkanContext* context);
    ~VulkanModel() = default;

    // glTF 파일을 로드하고 VulkanMesh들을 생성
    bool loadFromFile(AAssetManager* assetManager, const std::string& filename);

    // 모든 메시를 순회하며 그리기
    void draw(VkCommandBuffer commandBuffer);

private:
    VulkanContext* mContext;
    std::vector<std::unique_ptr<VulkanMesh>> mMeshes;

    // tinygltf 모델 데이터를 VulkanMesh로 변환하는 내부 로직
    void processModel(const tinygltf::Model& model);
};
