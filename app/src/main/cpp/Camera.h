#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "volk.h"

class Camera {
public:
    Camera();
    ~Camera() = default;

    // 화면 해상도와 Vulkan의 회전 상태를 업데이트
    void update(float width, float height, VkSurfaceTransformFlagBitsKHR transform);
    
    // 최종 MVP 행렬 반환
    glm::mat4 getMVPMatrix() const { return mMVPMatrix; }

private:
    glm::mat4 mMVPMatrix;
    
    // 내부 보정 로직
    glm::mat4 calculateRotation(VkSurfaceTransformFlagBitsKHR transform);
    glm::mat4 calculateProjection(float width, float height);
};
