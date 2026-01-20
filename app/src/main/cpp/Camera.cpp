#include "Camera.h"
#include <algorithm>

Camera::Camera() : mMVPMatrix(1.0f) {
}

void Camera::update(float width, float height, VkSurfaceTransformFlagBitsKHR transform) {
    // 1. 모델 행렬 (큐브를 약간 돌려놓음)
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(1.0f, 1.0f, 0.0f));

    // 2. 뷰 행렬 (카메라를 뒤로 2만큼 뺌)
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 6.0f), // 이 값을 키울수록 뒤로 갑니다.
                                 glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    // 3. 투영 행렬 (원근법 적용)
    float aspect = width / height;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);

    // Vulkan은 Y축이 아래로 향하므로 보정 (음수 Viewport 사용 중이라면 불필요)
    // proj[1][1] *= -1;

    // 4. 기기 회전 보정
    glm::mat4 rotation = calculateRotation(transform);

    // 최종 조합: Rotation * Projection * View * Model
    mMVPMatrix = rotation * proj * view * model;
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

glm::mat4 Camera::calculateProjection(float width, float height) {
    // 뷰포트 반전(Negative Viewport)을 고려한 정사영(Orthographic) 또는 종횡비 보정
    float minDim = std::min(width, height);
    return glm::scale(glm::mat4(1.0f), glm::vec3(minDim / width, minDim / height, 1.0f));
}
