#pragma once

#include "VulkanMesh.h"
#include "VulkanContext.h"
#include "VulkanTexture.h"

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

    // 텍스처에 접근하기 위한 인터페이스 (나중에 디스크립터 연결 시 사용)
    const std::vector<std::unique_ptr<VulkanTexture>>& getTextures() const { return mTextures; }

private:
    VulkanContext* mContext;
    std::vector<std::unique_ptr<VulkanMesh>> mMeshes;
    std::vector<std::unique_ptr<VulkanTexture>> mTextures;


    void processModel(const tinygltf::Model& model); // tinygltf 모델 -> VulkanMesh 변환
    void loadTextures(const tinygltf::Model& model);
};
