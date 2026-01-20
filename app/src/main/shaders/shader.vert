#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 mvp;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

void main() {
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
}