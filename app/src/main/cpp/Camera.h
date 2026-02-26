#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "volk.h"

class Camera {
public:
    Camera();
    ~Camera() = default;

    void update(float width, float height, VkSurfaceTransformFlagBitsKHR transform);

    // 최종 View-Projection 조합 행렬 반환 (Model 제외)
    glm::mat4 getViewProjectionMatrix() const { return mVPMatrix; }

    void rotate(float deltaYaw, float deltaPitch);
    void zoom(float delta);
private:
    glm::mat4 mVPMatrix;
    float mYaw;   // 좌우 회전 (라디안)
    float mPitch; // 상하 회전 (라디안)
    float mRadius;

    // 내부 보정 로직
    glm::mat4 calculateRotation(VkSurfaceTransformFlagBitsKHR transform);
};
