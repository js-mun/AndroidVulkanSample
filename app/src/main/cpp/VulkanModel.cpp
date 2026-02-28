#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// [중요] 안드로이드 에셋 로딩 활성화 매크로를 헤더 포함 전에 정의합니다.
#ifndef TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif
#include "tiny_gltf.h"

#include "VulkanModel.h"
#include "Log.h"
#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace {
void logNodeRecursive(const tinygltf::Model& model, int nodeIndex, int depth) {
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= model.nodes.size()) {
        return;
    }

    const auto& node = model.nodes[nodeIndex];
    std::string indent(static_cast<size_t>(depth) * 2, ' ');

    LOGI("[glTF] %sNode[%d] name='%s' mesh=%d children=%zu matrixSize=%zu",
         indent.c_str(),
         nodeIndex,
         node.name.c_str(),
         node.mesh,
         node.children.size(),
         node.matrix.size());

    if (!node.translation.empty()) {
        LOGI("[glTF] %s  translation=(%.3f, %.3f, %.3f)",
             indent.c_str(),
             static_cast<float>(node.translation[0]),
             static_cast<float>(node.translation[1]),
             static_cast<float>(node.translation[2]));
    }
    if (!node.rotation.empty()) {
        LOGI("[glTF] %s  rotation(quat xyzw)=(%.3f, %.3f, %.3f, %.3f)",
             indent.c_str(),
             static_cast<float>(node.rotation[0]),
             static_cast<float>(node.rotation[1]),
             static_cast<float>(node.rotation[2]),
             static_cast<float>(node.rotation[3]));
    }
    if (!node.scale.empty()) {
        LOGI("[glTF] %s  scale=(%.3f, %.3f, %.3f)",
             indent.c_str(),
             static_cast<float>(node.scale[0]),
             static_cast<float>(node.scale[1]),
             static_cast<float>(node.scale[2]));
    }
    if (!node.matrix.empty()) {
        LOGI("[glTF] %s  matrix[0..3]=(%.3f, %.3f, %.3f, %.3f)",
             indent.c_str(),
             static_cast<float>(node.matrix[0]),
             static_cast<float>(node.matrix[1]),
             static_cast<float>(node.matrix[2]),
             static_cast<float>(node.matrix[3]));
    }

    for (int childIndex : node.children) {
        logNodeRecursive(model, childIndex, depth + 1);
    }
}

glm::mat4 getNodeLocalTransform(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        glm::mat4 matrix(1.0f);
        // glTF matrix는 column-major 순서입니다.
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                matrix[col][row] = static_cast<float>(node.matrix[col * 4 + row]);
            }
        }
        return matrix;
    }

    glm::mat4 translation(1.0f);
    glm::mat4 rotation(1.0f);
    glm::mat4 scale(1.0f);

    if (node.translation.size() == 3) {
        translation = glm::translate(glm::mat4(1.0f), glm::vec3(
                static_cast<float>(node.translation[0]),
                static_cast<float>(node.translation[1]),
                static_cast<float>(node.translation[2])));
    }

    if (node.rotation.size() == 4) {
        // glTF 회전은 x,y,z,w / glm::quat 생성자는 w,x,y,z
        glm::quat quaternion(
                static_cast<float>(node.rotation[3]),
                static_cast<float>(node.rotation[0]),
                static_cast<float>(node.rotation[1]),
                static_cast<float>(node.rotation[2]));
        rotation = glm::toMat4(quaternion);
    }

    if (node.scale.size() == 3) {
        scale = glm::scale(glm::mat4(1.0f), glm::vec3(
                static_cast<float>(node.scale[0]),
                static_cast<float>(node.scale[1]),
                static_cast<float>(node.scale[2])));
    }

    return translation * rotation * scale;
}
} // namespace

VulkanModel::VulkanModel(VulkanContext* context) : mContext(context) {
}

glm::mat4 VulkanModel::getAnimationTransform(float time) {
    if (mRotationAnim.times.empty()) return glm::mat4(1.0f);

    // 1. 전체 애니메이션 시간을 넘어가면 반복(Loop) 처리
    float animTime = fmod(time, mRotationAnim.times.back());

    // 2. 현재 시간에 해당하는 키프레임 구간 찾기
    size_t i = 0;
    while (i < mRotationAnim.times.size() - 1 && animTime > mRotationAnim.times[i + 1]) {
        i++;
    }

    // 3. 두 키프레임 사이의 보간 비율(0.0 ~ 1.0) 계산
    float t1 = mRotationAnim.times[i];
    float t2 = mRotationAnim.times[i + 1];
    float alpha = (animTime - t1) / (t2 - t1);

    // 4. 회전 보간 (Spherical Linear Interpolation - SLERP)
    glm::quat q1 = mRotationAnim.rotations[i];
    glm::quat q2 = mRotationAnim.rotations[i + 1];
    glm::quat blendedQuat = glm::slerp(q1, q2, alpha);

    // 5. 최종 회전 행렬로 변환하여 반환
    return glm::mat4_cast(blendedQuat);
}

void VulkanModel::loadAnimations(const tinygltf::Model& model) {
    if (model.animations.empty()) return;

    // AnimatedCube는 첫 번째 애니메이션의 첫 번째 채널이 회전입니다.
    const auto& anim = model.animations[0];
    for (const auto& channel : anim.channels) {
        if (channel.target_path == "rotation") {
            const auto& sampler = anim.samplers[channel.sampler];

            // 1. 시간 데이터 추출
            const auto& inputAccessor = model.accessors[sampler.input];
            const auto& inputView = model.bufferViews[inputAccessor.bufferView];
            const auto& inputBuffer = model.buffers[inputView.buffer];
            const float* times = reinterpret_cast<const float*>(&inputBuffer.data[inputView.byteOffset + inputAccessor.byteOffset]);
            mRotationAnim.times.assign(times, times + inputAccessor.count);

            // 2. 회전 데이터(Quaternion) 추출
            const auto& outputAccessor = model.accessors[sampler.output];
            const auto& outputView = model.bufferViews[outputAccessor.bufferView];
            const auto& outputBuffer = model.buffers[outputView.buffer];
            const float* rotations = reinterpret_cast<const float*>(&outputBuffer.data[outputView.byteOffset + outputAccessor.byteOffset]);

            for (size_t i = 0; i < outputAccessor.count; ++i) {
                mRotationAnim.rotations.push_back(glm::make_quat(&rotations[i * 4]));
            }
        }
    }
}

bool VulkanModel::loadFromFile(AAssetManager* assetManager, const std::string& filename) {
    // 1. tinygltf 전역 에셋 매니저 설정 (내부 로더가 사용)
    tinygltf::asset_manager = assetManager;

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    // 2. 확장자에 따라 glTF(.gltf)/GLB(.glb) 로드를 분기합니다.
    bool ret = false;
    if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".glb") {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    } else {
        // 이 함수는 filename을 기반으로 base_dir를 자동 계산하며,
        // TINYGLTF_ANDROID_LOAD_FROM_ASSETS 덕분에 에셋 폴더에서 .bin 파일도 자동으로 찾습니다.
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
    }

    if (!warn.empty()) LOGI("glTF Warning: %s", warn.c_str());
    if (!err.empty()) LOGE("glTF Error: %s", err.c_str());
    if (!ret) {
        LOGE("Failed to parse glTF: %s", filename.c_str());
        return false;
    }
    LOGI("Successfully loaded glTF model: %s", filename.c_str());

    // Node/scene transform 정보 로깅: 실제 월드 변환(TRS/matrix) 확인용
    LOGI("[glTF] scenes=%zu defaultScene=%d nodes=%zu meshes=%zu materials=%zu",
         model.scenes.size(), model.defaultScene, model.nodes.size(),
         model.meshes.size(), model.materials.size());

    int sceneIndex = model.defaultScene;
    if (sceneIndex < 0 && !model.scenes.empty()) {
        sceneIndex = 0;
    }
    if (sceneIndex >= 0 && static_cast<size_t>(sceneIndex) < model.scenes.size()) {
        const auto& scene = model.scenes[sceneIndex];
        LOGI("[glTF] Scene[%d] rootNodes=%zu", sceneIndex, scene.nodes.size());
        for (int rootNode : scene.nodes) {
            logNodeRecursive(model, rootNode, 0);
        }
    }

    loadTextures(model);
    processModel(model);
    loadAnimations(model);

    return true;
}

bool VulkanModel::initializeDescriptor(VkDescriptorSetLayout layout,
                                       const std::vector<std::unique_ptr<VulkanBuffer>>& uniformBuffers,
                                       uint32_t maxFramesInFlight,
                                       VkImageView shadowView,
                                       VkSampler shadowSampler) {
    mDescriptor = std::make_unique<VulkanDescriptor>(mContext->getDevice(), maxFramesInFlight);
    return mDescriptor->initialize(layout, uniformBuffers, mTextures, shadowView, shadowSampler);
}

void VulkanModel::updateShadowMap(VkImageView shadowView, VkSampler shadowSampler) {
    if (mDescriptor) {
        mDescriptor->updateShadowMap(shadowView, shadowSampler);
    }
}

VkDescriptorSet VulkanModel::getDescriptorSet(uint32_t frameIndex) const {
    if (!mDescriptor) return VK_NULL_HANDLE;
    return mDescriptor->getSet(frameIndex);
}

void VulkanModel::loadTextures(const tinygltf::Model& model) {
    for (const auto& image : model.images) {
        auto texture = std::make_unique<VulkanTexture>(mContext);
        // tinygltf는 이미지를 로드하여 image.image(vector<unsigned char>)에 담아둡니다.
        if (texture->loadFromMemory(image.image.data(), image.width, image.height, VK_FORMAT_R8G8B8A8_SRGB)) {
            mTextures.push_back(std::move(texture));
            LOGI("Loaded glTF texture: %s (%dx%d)", image.name.c_str(), image.width, image.height);
        }
    }

    // 텍스처가 전혀 없는 모델도 shader의 sampler2D(binding=1)를 안전하게 사용하도록
    // 1x1 흰색 텍스처를 기본 텍스처로 생성합니다.
    if (mTextures.empty()) {
        static constexpr std::array<unsigned char, 4> kWhitePixel = {255, 255, 255, 255};
        auto fallback = std::make_unique<VulkanTexture>(mContext);
        if (fallback->loadFromMemory(kWhitePixel.data(), 1, 1, VK_FORMAT_R8G8B8A8_SRGB)) {
            mTextures.push_back(std::move(fallback));
            LOGI("Created fallback white texture for non-textured model");
        } else {
            LOGE("Failed to create fallback white texture");
        }
    }
}

void VulkanModel::processModel(const tinygltf::Model& model) {
    mMeshes.clear();

    int sceneIndex = model.defaultScene;
    if (sceneIndex < 0 && !model.scenes.empty()) {
        sceneIndex = 0;
    }
    if (sceneIndex < 0 || static_cast<size_t>(sceneIndex) >= model.scenes.size()) {
        LOGE("No valid glTF scene to process");
        return;
    }

    const auto& scene = model.scenes[sceneIndex];
    for (int rootNode : scene.nodes) {
        processNode(model, rootNode, glm::mat4(1.0f));
    }
}

void VulkanModel::processNode(const tinygltf::Model& model, int nodeIndex, const glm::mat4& parentWorld) {
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= model.nodes.size()) {
        return;
    }

    const auto& node = model.nodes[nodeIndex];
    const glm::mat4 localTransform = getNodeLocalTransform(node);
    const glm::mat4 worldTransform = parentWorld * localTransform;

    if (node.mesh >= 0 && static_cast<size_t>(node.mesh) < model.meshes.size()) {
        const auto& mesh = model.meshes[node.mesh];
        for (const auto& primitive : mesh.primitives) {
            processPrimitive(model, primitive, worldTransform);
        }
    }

    for (int childIndex : node.children) {
        processNode(model, childIndex, worldTransform);
    }
}

void VulkanModel::processPrimitive(const tinygltf::Model& model,
                                   const tinygltf::Primitive& primitive,
                                   const glm::mat4& worldTransform) {
    if (primitive.attributes.find("POSITION") == primitive.attributes.end()) {
        return;
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Material의 baseColorFactor를 기본 정점 색상으로 사용합니다.
    glm::vec3 baseColorFactor(1.0f, 1.0f, 1.0f);
    if (primitive.material >= 0 &&
        static_cast<size_t>(primitive.material) < model.materials.size()) {
        const auto& factor = model.materials[primitive.material]
                .pbrMetallicRoughness.baseColorFactor;
        if (factor.size() >= 3) {
            baseColorFactor = glm::vec3(
                    static_cast<float>(factor[0]),
                    static_cast<float>(factor[1]),
                    static_cast<float>(factor[2]));
        }
    }

    // 1. POSITION 추출 + node world transform 적용
    const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
    const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
    const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];
    const float* positions = reinterpret_cast<const float*>(&posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);

    vertices.resize(posAccessor.count);
    for (size_t i = 0; i < posAccessor.count; i++) {
        const glm::vec3 localPos(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);
        const glm::vec4 worldPos4 = worldTransform * glm::vec4(localPos, 1.0f);
        vertices[i].pos = glm::vec3(worldPos4);
        vertices[i].color = baseColorFactor; // material baseColor 기본값
        vertices[i].texCoord = glm::vec2(0.0f, 0.0f); // UV 초기화
    }

    // 1.1 COLOR_0 추출 (존재하는 경우에만)
    if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
        const tinygltf::Accessor& colorAccessor = model.accessors[primitive.attributes.at("COLOR_0")];
        const tinygltf::BufferView& colorView = model.bufferViews[colorAccessor.bufferView];
        const tinygltf::Buffer& colorBuffer = model.buffers[colorView.buffer];
        const unsigned char* colorData = &colorBuffer.data[colorView.byteOffset + colorAccessor.byteOffset];
        int stride = colorAccessor.ByteStride(colorView);
        for (size_t i = 0; i < colorAccessor.count; i++) {
            const float* rgba = reinterpret_cast<const float*>(colorData + i * stride);
            // glTF는 VEC3 또는 VEC4 색상을 가질 수 있습니다.
            vertices[i].color = glm::vec3(rgba[0], rgba[1], rgba[2]) * baseColorFactor;
        }
        LOGI("Extracted COLOR_0 data for %zu vertices", colorAccessor.count);
    }

    // 1.2 TEXCOORD_0 추출
    if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
        const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
        const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
        const tinygltf::Buffer& uvBuffer = model.buffers[uvView.buffer];
        const unsigned char* uvData = &uvBuffer.data[uvView.byteOffset + uvAccessor.byteOffset];
        int stride = uvAccessor.ByteStride(uvView);
        for (size_t i = 0; i < uvAccessor.count; i++) {
            const float* uvs = reinterpret_cast<const float*>(uvData + i * stride);
            vertices[i].texCoord = glm::vec2(uvs[0], uvs[1]);
        }
        LOGI("Extracted TEXCOORD_0 data for %zu vertices", uvAccessor.count);
    }

    // 2. INDICES 추출
    if (primitive.indices >= 0) {
        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];
        const unsigned char* indexData = &indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset];

        indices.resize(indexAccessor.count);
        if (indexAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT) {
            const uint32_t* buf = reinterpret_cast<const uint32_t*>(indexData);
            for (size_t i = 0; i < indexAccessor.count; i++) indices[i] = buf[i];
        } else if (indexAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT) {
            const uint16_t* buf = reinterpret_cast<const uint16_t*>(indexData);
            for (size_t i = 0; i < indexAccessor.count; i++) indices[i] = buf[i];
        } else if (indexAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE) {
            const uint8_t* buf = reinterpret_cast<const uint8_t*>(indexData);
            for (size_t i = 0; i < indexAccessor.count; i++) indices[i] = buf[i];
        }
    }

    // 3. VulkanMesh 생성
    mMeshes.push_back(std::make_unique<VulkanMesh>(mContext, vertices, indices));

    // Debugging: transformed vertex 확인
    LOGV("Mesh Primitive: Vertex Count = %zu, Index Count = %zu", vertices.size(), indices.size());
    if (!vertices.empty()) {
        LOGI("[glTF] transformed v0=(%.3f, %.3f, %.3f)",
             vertices[0].pos.x, vertices[0].pos.y, vertices[0].pos.z);
    }
    for (size_t i = 0; i < std::min(vertices.size(), size_t(10)); ++i) {
        LOGV("  Vertex[%zu]: pos(%.2f, %.2f, %.2f), color(%.2f, %.2f, %.2f), uv(%.2f, %.2f)",
             i,
             vertices[i].pos.x, vertices[i].pos.y, vertices[i].pos.z,
             vertices[i].color.r, vertices[i].color.g, vertices[i].color.b,
             vertices[i].texCoord.x, vertices[i].texCoord.y);
    }
}

void VulkanModel::draw(VkCommandBuffer commandBuffer) {
    for (const auto& mesh : mMeshes) {
        mesh->draw(commandBuffer);
    }
}
