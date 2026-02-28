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

    // 2. LoadASCIIFromFile 사용
    // 이 함수는 filename을 기반으로 base_dir를 자동 계산하며,
    // TINYGLTF_ANDROID_LOAD_FROM_ASSETS 덕분에 에셋 폴더에서 .bin 파일도 자동으로 찾습니다.
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);

    if (!warn.empty()) LOGI("glTF Warning: %s", warn.c_str());
    if (!err.empty()) LOGE("glTF Error: %s", err.c_str());
    if (!ret) {
        LOGE("Failed to parse glTF: %s", filename.c_str());
        return false;
    }
    LOGI("Successfully loaded glTF model: %s", filename.c_str());

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
}

void VulkanModel::processModel(const tinygltf::Model& model) {
    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            if (primitive.attributes.find("POSITION") == primitive.attributes.end()) continue;

            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            // 1. POSITION 추출
            const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
            const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];
            const float* positions = reinterpret_cast<const float*>(&posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);

            vertices.resize(posAccessor.count);
            for (size_t i = 0; i < posAccessor.count; i++) {
                vertices[i].pos = glm::vec3(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);
                vertices[i].color = glm::vec3(1.0f, 1.0f, 1.0f); // 기본 색상 (white)
                vertices[i].texCoord = glm::vec2(0.0f, 0.0f);       // UV 초기화
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
                    vertices[i].color = glm::vec3(rgba[0], rgba[1], rgba[2]);
                }
                LOGI("Extracted COLOR_0 data for %zu vertices", colorAccessor.count);
            }

            // 1.2 TEXCOORD_0 추출 (추가됨)
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

            // Debugging: 처음 10개의 정점 데이터 출력
            LOGV("Mesh Primitive: Vertex Count = %zu, Index Count = %zu", vertices.size(), indices.size());
            for (size_t i = 0; i < std::min(vertices.size(), size_t(10)); ++i) {
                LOGV("  Vertex[%zu]: pos(%.2f, %.2f, %.2f), color(%.2f, %.2f, %.2f), uv(%.2f, %.2f)",
                     i,
                     vertices[i].pos.x, vertices[i].pos.y, vertices[i].pos.z,
                     vertices[i].color.r, vertices[i].color.g, vertices[i].color.b,
                     vertices[i].texCoord.x, vertices[i].texCoord.y);
            }
        }
    }
}

void VulkanModel::draw(VkCommandBuffer commandBuffer) {
    for (const auto& mesh : mMeshes) {
        mesh->draw(commandBuffer);
    }
}
