#include "Camera.h"
#include <algorithm>
#include "Log.h"

Camera::Camera() : mVPMatrix(1.0f) {
    mYaw = glm::radians(45.0f);
    mPitch = glm::radians(30.0f);
    mRadius = 15.0f;
}

void Camera::update(float width, float height, VkSurfaceTransformFlagBitsKHR transform) {
    // 1. 카메라 위치 고정
    float camX = mRadius * cos(mPitch) * sin(mYaw);
    float camY = mRadius * sin(mPitch);
    float camZ = mRadius * cos(mPitch) * cos(mYaw);

    // 2. 뷰 행렬
    glm::mat4 view = glm::lookAt(glm::vec3(camX, camY, camZ),
                                 glm::vec3(0.0f, 0.0f, 0.0f), // 원점을 바라봄
                                 glm::vec3(0.0f, 1.0f, 0.0f));

    // 3. 투영 행렬: 원근법 적용
    float aspect = width / height;
    glm::mat4 proj = glm::perspective(
            glm::radians(45.0f), aspect, 0.1f, 100.0f); // 가시거리 0.1 ~ 100

    // 4. 기기 회전 보정
    glm::mat4 deviceRotation = calculateRotation(transform);

    // 5. 조합
    mVPMatrix = deviceRotation * proj * view;
}

glm::mat4 Camera::calculateRotation(VkSurfaceTransformFlagBitsKHR transform) {
    glm::mat4 rotation = glm::mat4(1.0f);
    
    if (transform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR) {
        rotation = glm::rotate(rotation, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    } else if (transform == VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR) {
        rotation = glm::rotate(rotation, glm::radians(-180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    } else if (transform == VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) {
        rotation = glm::rotate(rotation, glm::radians(-270.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    }

    return rotation;
}

void Camera::rotate(float deltaYaw, float deltaPitch) {
    mYaw -= deltaYaw;
    mPitch += deltaPitch;

    // [중요] Pitch 제한: 89도는 lookAt이 깨질 수 있으므로 80도 정도로 안전하게 제한
    const float limit = glm::radians(80.0f);
    if (mPitch > limit) mPitch = limit;
    if (mPitch < -limit) mPitch = -limit;
}

void Camera::zoom(float delta) {
    mRadius -= delta;

    // 줌 제한 (너무 가깝거나 너무 멀지 않게)
    if (mRadius < 2.0f) mRadius = 2.0f;
    if (mRadius > 50.0f) mRadius = 50.0f;
}