#include "camera.h"
#include <algorithm>

Camera::Camera() : mMVPMatrix(1.0f) {
}

void Camera::update(float width, float height, VkSurfaceTransformFlagBitsKHR transform) {
    // 1. 기본 모델 행렬 (여기서는 고정)
    glm::mat4 model = glm::mat4(1.0f);

    // 2. 종횡비 및 투영 보정 행렬 계산
    glm::mat4 projection = calculateProjection(width, height);

    // 3. 기기 회전 보정 행렬 계산
    glm::mat4 rotation = calculateRotation(transform);


    // 4. 최종 결합 (오른쪽부터 적용: Model -> Projection -> Rotation)
    mMVPMatrix = rotation * projection * model;
    
    // 전체 크기 조정
    mMVPMatrix = glm::scale(mMVPMatrix, glm::vec3(1.0f, 1.0f, 1.0f));
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
