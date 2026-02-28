#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 viewProj;
    mat4 lightViewProj;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 fragShadowCoord;
layout(location = 3) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = ubo.viewProj * worldPos;

    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragShadowCoord = ubo.lightViewProj * worldPos;
    fragWorldPos = worldPos.xyz;
}
