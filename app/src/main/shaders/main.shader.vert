#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 mvp;
    mat4 lightMVP;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 fragShadowCoord;

void main() {
    vec4 worldPos = vec4(inPosition, 1.0);
    gl_Position = ubo.mvp * worldPos;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragShadowCoord = ubo.lightMVP * worldPos;
}