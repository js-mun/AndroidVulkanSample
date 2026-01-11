#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 mvp;
} ubo;

layout(location = 0) in vec2 inPosition; // 외부에서 주입받는 좌표

void main() {
    gl_Position = ubo.mvp * vec4(inPosition, 0.0, 1.0);
}