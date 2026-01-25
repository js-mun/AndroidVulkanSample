#include "Camera.h"
#include <algorithm>

Camera::Camera() : mVPMatrix(1.0f) {
}

void Camera::update(float width, float height, VkSurfaceTransformFlagBitsKHR transform) {
    // 1. 카메라 위치 고정
    float camX = 0.0f;
    float camZ = 10.0f; // 큐브로부터 X만큼 떨어진 정면

    // 2. 뷰 행렬: 회전하는 카메라 위치 적용
    glm::mat4 view = glm::lookAt(glm::vec3(camX, 2.0f, camZ), // 위아래로 2.0만큼 띄움
                                 glm::vec3(0.0f, 0.0f, 0.0f), // 원점을 바라봄
                                 glm::vec3(0.0f, 1.0f, 0.0f));

    // 3. 투영 행렬: 원근법 적용
    float aspect = width / height;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);

    // 4. 기기 회전 보정
    glm::mat4 deviceRotation = calculateRotation(transform);

    // 5. [수정] View-Projection 결합 (Model 제외)
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
