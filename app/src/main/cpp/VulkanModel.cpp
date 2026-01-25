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
    processModel(model);
    return true;
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

            // 2. INDICES 추출 (uint8, uint16, uint32 대응)
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
                LOGV("  Vertex[%zu]: pos(%.2f, %.2f, %.2f), color(%.2f, %.2f, %.2f)",
                     i,
                     vertices[i].pos.x, vertices[i].pos.y, vertices[i].pos.z,
                     vertices[i].color.r, vertices[i].color.g, vertices[i].color.b);
            }

        }
    }
}

void VulkanModel::draw(VkCommandBuffer commandBuffer) {
    for (const auto& mesh : mMeshes) {
        mesh->draw(commandBuffer);
    }
}