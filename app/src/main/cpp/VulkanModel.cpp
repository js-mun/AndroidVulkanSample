#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include "VulkanModel.h"
#include "Log.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace {
bool hasGlbExtension(const std::string& path) {
    if (path.size() < 4) {
        return false;
    }
    return path.substr(path.size() - 4) == ".glb";
}

void logNodeRecursive(const tinygltf::Model& model, int nodeIndex, int depth) {
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= model.nodes.size()) {
        return;
    }

    const auto& node = model.nodes[nodeIndex];
    std::string indent(static_cast<size_t>(depth) * 2, ' ');

    if (DEBUG_LOG) {
        LOGI("[glTF] %sNode[%d] name='%s' mesh=%d children=%zu matrixSize=%zu",
                indent.c_str(),
                nodeIndex,
                node.name.c_str(),
                node.mesh,
                node.children.size(),
                node.matrix.size());
    }

    if (!node.translation.empty()) {
        if (DEBUG_LOG) {
            LOGI("[glTF] %s  translation=(%.3f, %.3f, %.3f)",
                indent.c_str(),
                static_cast<float>(node.translation[0]),
                static_cast<float>(node.translation[1]),
                static_cast<float>(node.translation[2]));
        }
    }
    if (!node.rotation.empty()) {
        if (DEBUG_LOG) {
            LOGI("[glTF] %s  rotation(quat xyzw)=(%.3f, %.3f, %.3f, %.3f)",
                indent.c_str(),
                static_cast<float>(node.rotation[0]),
                static_cast<float>(node.rotation[1]),
                static_cast<float>(node.rotation[2]),
                static_cast<float>(node.rotation[3]));
        }
    }
    if (!node.scale.empty()) {
        if (DEBUG_LOG) {
            LOGI("[glTF] %s  scale=(%.3f, %.3f, %.3f)",
                indent.c_str(),
                static_cast<float>(node.scale[0]),
                static_cast<float>(node.scale[1]),
                static_cast<float>(node.scale[2]));
        }
    }
    if (!node.matrix.empty()) {
        if (DEBUG_LOG) {
            LOGI("[glTF] %s  matrix[0..3]=(%.3f, %.3f, %.3f, %.3f)",
                indent.c_str(),
                static_cast<float>(node.matrix[0]),
                static_cast<float>(node.matrix[1]),
                static_cast<float>(node.matrix[2]),
                static_cast<float>(node.matrix[3]));
        }
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

glm::quat VulkanModel::sampleNodeRotation(int nodeIndex, float timeSec) const {
    const auto it = mNodeRotationAnims.find(nodeIndex);
    if (it == mNodeRotationAnims.end()) {
        return mNodeBaseRotation[nodeIndex];
    }
    const RotationAnimData& anim = it->second;
    if (anim.times.empty() || anim.rotations.empty() || anim.times.size() != anim.rotations.size()) {
        return mNodeBaseRotation[nodeIndex];
    }
    if (anim.times.size() == 1) {
        return glm::normalize(anim.rotations[0]);
    }

    const float duration = anim.times.back();
    if (duration <= 0.0f) {
        return glm::normalize(anim.rotations[0]);
    }

    float t = fmod(timeSec, duration);
    if (t < 0.0f) {
        t += duration;
    }

    size_t i = 0;
    while ((i + 1) < anim.times.size() && t > anim.times[i + 1]) {
        i++;
    }
    if ((i + 1) >= anim.times.size()) {
        return glm::normalize(anim.rotations.back());
    }

    const float t1 = anim.times[i];
    const float t2 = anim.times[i + 1];
    float alpha = 0.0f;
    if (t2 > t1) {
        alpha = (t - t1) / (t2 - t1);
    }
    return glm::normalize(glm::slerp(glm::normalize(anim.rotations[i]),
                                     glm::normalize(anim.rotations[i + 1]),
                                     alpha));
}

glm::vec3 VulkanModel::sampleNodeTranslation(int nodeIndex, float timeSec) const {
    const auto it = mNodeTranslationAnims.find(nodeIndex);
    if (it == mNodeTranslationAnims.end()) {
        return mNodeBaseTranslation[nodeIndex];
    }
    const Vec3AnimData& anim = it->second;
    if (anim.times.empty() || anim.values.empty() || anim.times.size() != anim.values.size()) {
        return mNodeBaseTranslation[nodeIndex];
    }
    if (anim.times.size() == 1) {
        return anim.values[0];
    }

    const float duration = anim.times.back();
    if (duration <= 0.0f) {
        return anim.values[0];
    }

    float t = fmod(timeSec, duration);
    if (t < 0.0f) {
        t += duration;
    }

    size_t i = 0;
    while ((i + 1) < anim.times.size() && t > anim.times[i + 1]) {
        i++;
    }
    if ((i + 1) >= anim.times.size()) {
        return anim.values.back();
    }

    const float t1 = anim.times[i];
    const float t2 = anim.times[i + 1];
    float alpha = 0.0f;
    if (t2 > t1) {
        alpha = (t - t1) / (t2 - t1);
    }
    return glm::mix(anim.values[i], anim.values[i + 1], alpha);
}

glm::vec3 VulkanModel::sampleNodeScale(int nodeIndex, float timeSec) const {
    const auto it = mNodeScaleAnims.find(nodeIndex);
    if (it == mNodeScaleAnims.end()) {
        return mNodeBaseScale[nodeIndex];
    }
    const Vec3AnimData& anim = it->second;
    if (anim.times.empty() || anim.values.empty() || anim.times.size() != anim.values.size()) {
        return mNodeBaseScale[nodeIndex];
    }
    if (anim.times.size() == 1) {
        return anim.values[0];
    }

    const float duration = anim.times.back();
    if (duration <= 0.0f) {
        return anim.values[0];
    }

    float t = fmod(timeSec, duration);
    if (t < 0.0f) {
        t += duration;
    }

    size_t i = 0;
    while ((i + 1) < anim.times.size() && t > anim.times[i + 1]) {
        i++;
    }
    if ((i + 1) >= anim.times.size()) {
        return anim.values.back();
    }

    const float t1 = anim.times[i];
    const float t2 = anim.times[i + 1];
    float alpha = 0.0f;
    if (t2 > t1) {
        alpha = (t - t1) / (t2 - t1);
    }
    return glm::mix(anim.values[i], anim.values[i + 1], alpha);
}

glm::mat4 VulkanModel::getNodeLocalTransformAtTime(int nodeIndex, float timeSec) const {
    if (mNodeUseMatrix[nodeIndex]) {
        // matrix 노드는 그대로 사용 (rotation 채널 대체는 지원하지 않음)
        return mNodeBaseMatrix[nodeIndex];
    }

    const glm::vec3 t = sampleNodeTranslation(nodeIndex, timeSec);
    const glm::vec3 s = sampleNodeScale(nodeIndex, timeSec);
    const glm::quat r = sampleNodeRotation(nodeIndex, timeSec);
    return glm::translate(glm::mat4(1.0f), t) * glm::toMat4(r) * glm::scale(glm::mat4(1.0f), s);
}

void VulkanModel::buildNodeWorldRecursive(int nodeIndex, const glm::mat4& parentWorld, float timeSec) {
    const glm::mat4 local = getNodeLocalTransformAtTime(nodeIndex, timeSec);
    const glm::mat4 world = parentWorld * local;
    mNodeWorldCache[nodeIndex] = world;

    for (size_t i = 0; i < mNodeParents.size(); ++i) {
        if (mNodeParents[i] == nodeIndex) {
            buildNodeWorldRecursive(static_cast<int>(i), world, timeSec);
        }
    }
}

void VulkanModel::updateNodeWorldCache(float timeSec) {
    if (mNodeWorldCache.size() != mNodeParents.size()) {
        mNodeWorldCache.assign(mNodeParents.size(), glm::mat4(1.0f));
    }
    for (int root : mSceneRootNodes) {
        buildNodeWorldRecursive(root, glm::mat4(1.0f), timeSec);
    }
}

void VulkanModel::extractNodeBaseTransforms(const tinygltf::Model& model) {
    const size_t n = model.nodes.size();
    mNodeParents.assign(n, -1);
    mNodeBaseTranslation.assign(n, glm::vec3(0.0f));
    mNodeBaseRotation.assign(n, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    mNodeBaseScale.assign(n, glm::vec3(1.0f));
    mNodeUseMatrix.assign(n, false);
    mNodeBaseMatrix.assign(n, glm::mat4(1.0f));
    mNodeWorldCache.assign(n, glm::mat4(1.0f));

    for (size_t i = 0; i < n; ++i) {
        const auto& node = model.nodes[i];
        if (node.matrix.size() == 16) {
            mNodeUseMatrix[i] = true;
            mNodeBaseMatrix[i] = getNodeLocalTransform(node);
        } else {
            if (node.translation.size() == 3) {
                mNodeBaseTranslation[i] = glm::vec3(
                        static_cast<float>(node.translation[0]),
                        static_cast<float>(node.translation[1]),
                        static_cast<float>(node.translation[2]));
            }
            if (node.rotation.size() == 4) {
                mNodeBaseRotation[i] = glm::normalize(glm::quat(
                        static_cast<float>(node.rotation[3]),
                        static_cast<float>(node.rotation[0]),
                        static_cast<float>(node.rotation[1]),
                        static_cast<float>(node.rotation[2])));
            }
            if (node.scale.size() == 3) {
                mNodeBaseScale[i] = glm::vec3(
                        static_cast<float>(node.scale[0]),
                        static_cast<float>(node.scale[1]),
                        static_cast<float>(node.scale[2]));
            }
        }

        for (int child : node.children) {
            if (child >= 0 && static_cast<size_t>(child) < n) {
                mNodeParents[child] = static_cast<int>(i);
            }
        }
    }
}

void VulkanModel::loadAnimations(const tinygltf::Model& model) {
    mNodeRotationAnims.clear();
    mNodeTranslationAnims.clear();
    mNodeScaleAnims.clear();
    if (model.animations.empty()) return;

    const auto& anim = model.animations[0];
    for (const auto& channel : anim.channels) {
        if (channel.target_node < 0 ||
            static_cast<size_t>(channel.target_node) >= model.nodes.size()) {
            continue;
        }

        const auto& sampler = anim.samplers[channel.sampler];
        const auto& inputAccessor = model.accessors[sampler.input];
        const auto& inputView = model.bufferViews[inputAccessor.bufferView];
        const auto& inputBuffer = model.buffers[inputView.buffer];
        const float* times = reinterpret_cast<const float*>(
                &inputBuffer.data[inputView.byteOffset + inputAccessor.byteOffset]);

        const auto& outputAccessor = model.accessors[sampler.output];
        const auto& outputView = model.bufferViews[outputAccessor.bufferView];
        const auto& outputBuffer = model.buffers[outputView.buffer];
        const float* rotations = reinterpret_cast<const float*>(
                &outputBuffer.data[outputView.byteOffset + outputAccessor.byteOffset]);

        if (channel.target_path == "rotation") {
            RotationAnimData data;
            data.times.assign(times, times + inputAccessor.count);
            data.rotations.reserve(outputAccessor.count);
            for (size_t i = 0; i < outputAccessor.count; ++i) {
                const float x = rotations[i * 4 + 0];
                const float y = rotations[i * 4 + 1];
                const float z = rotations[i * 4 + 2];
                const float w = rotations[i * 4 + 3];
                data.rotations.push_back(glm::normalize(glm::quat(w, x, y, z)));
            }
            mNodeRotationAnims[channel.target_node] = std::move(data);
        } else if (channel.target_path == "translation" || channel.target_path == "scale") {
            Vec3AnimData data;
            data.times.assign(times, times + inputAccessor.count);
            data.values.reserve(outputAccessor.count);
            for (size_t i = 0; i < outputAccessor.count; ++i) {
                const float x = rotations[i * 3 + 0];
                const float y = rotations[i * 3 + 1];
                const float z = rotations[i * 3 + 2];
                data.values.emplace_back(x, y, z);
            }
            if (channel.target_path == "translation") {
                mNodeTranslationAnims[channel.target_node] = std::move(data);
            } else {
                mNodeScaleAnims[channel.target_node] = std::move(data);
            }
        }
    }
}

bool VulkanModel::loadFromFile(const IAssetProvider& assetProvider, const std::string& filename) {
    if (!hasGlbExtension(filename)) {
        LOGE("Only .glb is supported now: %s", filename.c_str());
        return false;
    }

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    std::vector<uint8_t> bytes;
    if (!assetProvider.readBinaryFile(filename, bytes)) {
        LOGE("Failed to read glb asset: %s", filename.c_str());
        return false;
    }
    bool ret = loader.LoadBinaryFromMemory(&model, &err, &warn, bytes.data(), bytes.size(), "");

    if (!warn.empty()) LOGI("glTF Warning: %s", warn.c_str());
    if (!err.empty()) LOGE("glTF Error: %s", err.c_str());
    if (!ret) {
        LOGE("Failed to parse glTF: %s", filename.c_str());
        return false;
    }
    LOGI("Successfully loaded glTF model: %s", filename.c_str());

    // Node/scene transform 정보 로깅: 실제 월드 변환(TRS/matrix) 확인용
    if (DEBUG_LOG) {
        LOGI("[glTF] scenes=%zu defaultScene=%d nodes=%zu meshes=%zu materials=%zu",
            model.scenes.size(), model.defaultScene, model.nodes.size(),
            model.meshes.size(), model.materials.size());
    }
    int sceneIndex = model.defaultScene;
    if (sceneIndex < 0 && !model.scenes.empty()) {
        sceneIndex = 0;
    }
    if (sceneIndex >= 0 && static_cast<size_t>(sceneIndex) < model.scenes.size()) {
        const auto& scene = model.scenes[sceneIndex];
        if (DEBUG_LOG) {
            LOGI("[glTF] Scene[%d] rootNodes=%zu", sceneIndex, scene.nodes.size());
        }
        for (int rootNode : scene.nodes) {
            logNodeRecursive(model, rootNode, 0);
        }
    }

    loadTextures(model);
    extractNodeBaseTransforms(model);
    processModel(model);
    loadAnimations(model);

    return true;
}

bool VulkanModel::initializeDescriptor(VkDescriptorSetLayout materialLayout,
                                       uint32_t maxFramesInFlight) {
    mDescriptor = std::make_unique<VulkanDescriptor>(mContext->getDevice(), maxFramesInFlight);
    return mDescriptor->initialize(materialLayout, mTextures);
}

VkDescriptorSet VulkanModel::getDescriptorSet() const {
    if (!mDescriptor) return VK_NULL_HANDLE;
    return mDescriptor->getSet();
}

void VulkanModel::loadTextures(const tinygltf::Model& model) {
    for (const auto& image : model.images) {
        auto texture = std::make_unique<VulkanTexture>(mContext);
        // tinygltf는 이미지를 로드하여 image.image(vector<unsigned char>)에 담아둡니다.
        if (texture->loadFromMemory(image.image.data(), image.width, image.height, VK_FORMAT_R8G8B8A8_SRGB)) {
            mTextures.push_back(std::move(texture));
            if (DEBUG_LOG) {
                LOGI("Loaded glTF texture: %s (%dx%d)", image.name.c_str(), 
                        image.width, image.height);
            }
        }
    }

    // 텍스처가 전혀 없는 모델도 shader의 sampler2D(binding=1)를 안전하게 사용하도록
    // 1x1 흰색 텍스처를 기본 텍스처로 생성합니다.
    if (mTextures.empty()) {
        static constexpr std::array<unsigned char, 4> kWhitePixel = {255, 255, 255, 255};
        auto fallback = std::make_unique<VulkanTexture>(mContext);
        if (fallback->loadFromMemory(kWhitePixel.data(), 1, 1, VK_FORMAT_R8G8B8A8_SRGB)) {
            mTextures.push_back(std::move(fallback));
            if (DEBUG_LOG) LOGI("Created fallback white texture for non-textured model");
        } else {
            LOGE("Failed to create fallback white texture");
        }
    }
}

void VulkanModel::processModel(const tinygltf::Model& model) {
    mPrimitiveDraws.clear();
    mSceneRootNodes.clear();

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
        mSceneRootNodes.push_back(rootNode);
        processNode(model, rootNode, -1);
    }
}

void VulkanModel::processNode(const tinygltf::Model& model, int nodeIndex, int parentNode) {
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= model.nodes.size()) {
        return;
    }

    mNodeParents[nodeIndex] = parentNode;
    const auto& node = model.nodes[nodeIndex];
    if (node.mesh >= 0 && static_cast<size_t>(node.mesh) < model.meshes.size()) {
        const auto& mesh = model.meshes[node.mesh];
        for (const auto& primitive : mesh.primitives) {
            processPrimitive(model, primitive, nodeIndex);
        }
    }

    for (int childIndex : node.children) {
        processNode(model, childIndex, nodeIndex);
    }
}

void VulkanModel::processPrimitive(const tinygltf::Model& model,
                                   const tinygltf::Primitive& primitive,
                                   int nodeIndex) {
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

    // 1. POSITION 추출 (mesh local space)
    const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
    const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
    const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];
    const float* positions = reinterpret_cast<const float*>(&posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);

    vertices.resize(posAccessor.count);
    for (size_t i = 0; i < posAccessor.count; i++) {
        vertices[i].pos = glm::vec3(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);
        vertices[i].color = baseColorFactor; // material baseColor 기본값
        vertices[i].texCoord = glm::vec2(0.0f, 0.0f); // UV 초기화
        vertices[i].normal = glm::vec3(0.0f, 1.0f, 0.0f); // NORMAL 없을 때 기본값
    }

    // 1.1 COLOR_0 추출 (존재하는 경우에만)
    if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
        const tinygltf::Accessor& colorAccessor = model.accessors[primitive.attributes.at("COLOR_0")];
        const tinygltf::BufferView& colorView = model.bufferViews[colorAccessor.bufferView];
        const tinygltf::Buffer& colorBuffer = model.buffers[colorView.buffer];
        const unsigned char* colorData = &colorBuffer.data[colorView.byteOffset + colorAccessor.byteOffset];
        int stride = colorAccessor.ByteStride(colorView);
        const int componentCount = (colorAccessor.type == TINYGLTF_TYPE_VEC4) ? 4 : 3;

        auto readNormalizedColor = [&](const unsigned char* src, int componentType, int count) -> glm::vec3 {
            glm::vec3 color(1.0f);
            if (componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                const float* v = reinterpret_cast<const float*>(src);
                color.r = (count > 0) ? v[0] : 1.0f;
                color.g = (count > 1) ? v[1] : 1.0f;
                color.b = (count > 2) ? v[2] : 1.0f;
            } else if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                const uint8_t* v = reinterpret_cast<const uint8_t*>(src);
                color.r = (count > 0) ? static_cast<float>(v[0]) / 255.0f : 1.0f;
                color.g = (count > 1) ? static_cast<float>(v[1]) / 255.0f : 1.0f;
                color.b = (count > 2) ? static_cast<float>(v[2]) / 255.0f : 1.0f;
            } else if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                const uint16_t* v = reinterpret_cast<const uint16_t*>(src);
                color.r = (count > 0) ? static_cast<float>(v[0]) / 65535.0f : 1.0f;
                color.g = (count > 1) ? static_cast<float>(v[1]) / 65535.0f : 1.0f;
                color.b = (count > 2) ? static_cast<float>(v[2]) / 65535.0f : 1.0f;
            } else {
                // 지원하지 않는 타입이면 기본 흰색 유지
                LOGW("Unsupported COLOR_0 componentType: %d", componentType);
            }
            return color;
        };

        for (size_t i = 0; i < colorAccessor.count; i++) {
            const unsigned char* src = colorData + i * stride;
            // glTF는 VEC3/VEC4 + 다양한 componentType을 허용합니다.
            vertices[i].color = readNormalizedColor(src, colorAccessor.componentType, componentCount) * baseColorFactor;
        }
        if (DEBUG_LOG) LOGI("Extracted COLOR_0 data for %zu vertices", colorAccessor.count);
    }

    // 1.2 NORMAL 추출
    if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
        const tinygltf::Accessor& normalAccessor = model.accessors[primitive.attributes.at("NORMAL")];
        const tinygltf::BufferView& normalView = model.bufferViews[normalAccessor.bufferView];
        const tinygltf::Buffer& normalBuffer = model.buffers[normalView.buffer];
        const unsigned char* normalData = &normalBuffer.data[normalView.byteOffset + normalAccessor.byteOffset];
        int stride = normalAccessor.ByteStride(normalView);
        for (size_t i = 0; i < normalAccessor.count; i++) {
            const float* n = reinterpret_cast<const float*>(normalData + i * stride);
            vertices[i].normal = glm::normalize(glm::vec3(n[0], n[1], n[2]));
        }
        if (DEBUG_LOG) LOGI("Extracted NORMAL data for %zu vertices", normalAccessor.count);
    }

    // 1.3 TEXCOORD_0 추출
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
        if (DEBUG_LOG) LOGI("Extracted TEXCOORD_0 data for %zu vertices", uvAccessor.count);
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

    // 3. VulkanMesh 생성 + node mapping
    PrimitiveDrawItem drawItem{};
    drawItem.mesh = std::make_unique<VulkanMesh>(mContext, vertices, indices);
    drawItem.nodeIndex = nodeIndex;
    mPrimitiveDraws.push_back(std::move(drawItem));

    // Debugging: transformed vertex 확인
    if (DEBUG_LOG) {
        LOGV("Mesh Primitive: Vertex Count = %zu, Index Count = %zu", vertices.size(), indices.size());
        if (!vertices.empty()) {
            LOGI("[glTF] local v0=(%.3f, %.3f, %.3f)",
                vertices[0].pos.x, vertices[0].pos.y, vertices[0].pos.z);
        }
    for (size_t i = 0; i < std::min(vertices.size(), size_t(10)); ++i) {
        LOGV("  Vertex[%zu]: pos(%.2f, %.2f, %.2f), color(%.2f, %.2f, %.2f), uv(%.2f, %.2f), n(%.2f, %.2f, %.2f)",
             i,
             vertices[i].pos.x, vertices[i].pos.y, vertices[i].pos.z,
             vertices[i].color.r, vertices[i].color.g, vertices[i].color.b,
             vertices[i].texCoord.x, vertices[i].texCoord.y,
             vertices[i].normal.x, vertices[i].normal.y, vertices[i].normal.z);
    }
}
}

void VulkanModel::draw(VkCommandBuffer commandBuffer,
                       VkPipelineLayout pipelineLayout,
                       const glm::mat4& modelBase,
                       float timeSec) {
    updateNodeWorldCache(timeSec);

    for (const auto& item : mPrimitiveDraws) {
        if (!item.mesh) {
            continue;
        }
        const int nodeIndex = item.nodeIndex;
        if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= mNodeWorldCache.size()) {
            continue;
        }
        const glm::mat4 model = modelBase * mNodeWorldCache[nodeIndex];
        vkCmdPushConstants(commandBuffer,
                           pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(glm::mat4),
                           &model);
        item.mesh->draw(commandBuffer);
    }
}
