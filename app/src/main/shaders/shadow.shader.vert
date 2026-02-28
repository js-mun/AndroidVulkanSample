#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 viewProj;
    mat4 lightViewProj;
    vec4 lightPos;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = ubo.lightViewProj * (pc.model * vec4(inPosition, 1.0));
}
