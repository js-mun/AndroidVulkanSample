#pragma once

#include "VulkanMesh.h"
#include "VulkanContext.h"
#include "VulkanTexture.h"

#include <string>
#include <vector>
#include <memory>

#include <android/asset_manager.h>
#include <glm/gtc/type_ptr.hpp>

namespace tinygltf {
    class Model;
}

struct AnimationData {
    std::vector<float> times;          // 키프레임 시간 (초)
    std::vector<glm::quat> rotations; // 각 시간의 회전값 (Quaternion)
};

class VulkanModel {
public:
    VulkanModel(VulkanContext* context);
    ~VulkanModel() = default;

    // glTF 파일을 로드하고 VulkanMesh들을 생성
    bool loadFromFile(AAssetManager* assetManager, const std::string& filename);

    // 모든 메시를 순회하며 그리기
    void draw(VkCommandBuffer commandBuffer);

    // 텍스처에 접근하기 위한 인터페이스
    const std::vector<std::unique_ptr<VulkanTexture>>& getTextures() const { return mTextures; }

    // 현재 시간에 맞는 회전 행렬 계산
    glm::mat4 getAnimationTransform(float time);

private:
    VulkanContext* mContext;
    std::vector<std::unique_ptr<VulkanMesh>> mMeshes;
    std::vector<std::unique_ptr<VulkanTexture>> mTextures;
    AnimationData mRotationAnim;

    void processModel(const tinygltf::Model& model); // tinygltf 모델 -> VulkanMesh 변환
    void loadTextures(const tinygltf::Model& model);
    void loadAnimations(const tinygltf::Model& model);
};
