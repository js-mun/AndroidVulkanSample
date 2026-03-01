#pragma once

#include "VulkanMesh.h"
#include "VulkanContext.h"
#include "VulkanTexture.h"
#include "VulkanDescriptor.h"
#include "IAssetProvider.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include <glm/gtc/type_ptr.hpp>

namespace tinygltf {
    class Model;
    class Primitive;
}

struct RotationAnimData {
    std::vector<float> times;          // 키프레임 시간 (초)
    std::vector<glm::quat> rotations; // 각 시간의 회전값 (Quaternion)
};

struct Vec3AnimData {
    std::vector<float> times;
    std::vector<glm::vec3> values;
};

struct PrimitiveDrawItem {
    std::unique_ptr<VulkanMesh> mesh;
    int nodeIndex = -1;
};

class VulkanModel {
public:
    VulkanModel(VulkanContext* context);
    ~VulkanModel() = default;

    // glTF 파일을 로드하고 VulkanMesh들을 생성
    bool loadFromFile(const IAssetProvider& assetProvider, const std::string& filename);
    bool initializeDescriptor(VkDescriptorSetLayout materialLayout,
                            uint32_t maxFramesInFlight);

    VkDescriptorSet getDescriptorSet() const;

    // 모든 primitive를 노드 transform(애니메이션 포함)으로 그리기
    void draw(VkCommandBuffer commandBuffer,
              VkPipelineLayout pipelineLayout,
              const glm::mat4& modelBase,
              float timeSec);

    // 텍스처에 접근하기 위한 인터페이스
    const std::vector<std::unique_ptr<VulkanTexture>>& getTextures() const { return mTextures; }

private:
    VulkanContext* mContext;
    std::vector<PrimitiveDrawItem> mPrimitiveDraws;
    std::vector<std::unique_ptr<VulkanTexture>> mTextures;
    std::unique_ptr<VulkanDescriptor> mDescriptor;
    std::unordered_map<int, RotationAnimData> mNodeRotationAnims; // key: node index
    std::unordered_map<int, Vec3AnimData> mNodeTranslationAnims;
    std::unordered_map<int, Vec3AnimData> mNodeScaleAnims;

    std::vector<int> mNodeParents;
    std::vector<glm::vec3> mNodeBaseTranslation;
    std::vector<glm::quat> mNodeBaseRotation;
    std::vector<glm::vec3> mNodeBaseScale;
    std::vector<bool> mNodeUseMatrix;
    std::vector<glm::mat4> mNodeBaseMatrix;
    std::vector<glm::mat4> mNodeWorldCache;
    std::vector<int> mSceneRootNodes;

    void processModel(const tinygltf::Model& model); // tinygltf 모델 -> VulkanMesh 변환
    void processNode(const tinygltf::Model& model, int nodeIndex, int parentNode);
    void processPrimitive(const tinygltf::Model& model,
                          const tinygltf::Primitive& primitive,
                          int nodeIndex);
    void loadTextures(const tinygltf::Model& model);
    void loadAnimations(const tinygltf::Model& model);

    void extractNodeBaseTransforms(const tinygltf::Model& model);
    glm::vec3 sampleNodeTranslation(int nodeIndex, float timeSec) const;
    glm::quat sampleNodeRotation(int nodeIndex, float timeSec) const;
    glm::vec3 sampleNodeScale(int nodeIndex, float timeSec) const;
    glm::mat4 getNodeLocalTransformAtTime(int nodeIndex, float timeSec) const;
    void buildNodeWorldRecursive(int nodeIndex, const glm::mat4& parentWorld, float timeSec);
    void updateNodeWorldCache(float timeSec);
};
