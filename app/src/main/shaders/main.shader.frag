#version 450
layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform sampler2D shadowMap;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 fragShadowCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 base = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);

    if (fragShadowCoord.w <= 0.0) {
        outColor = base;
        return;
    }

    vec3 proj = fragShadowCoord.xyz / fragShadowCoord.w;
    // XY는 -1~1을 0~1로 바꿔야 하지만 (Vulkan NDC),
    // Z는 이미 0~1이므로 다시 0.5를 곱할 필요가 없습니다.
    proj.xy = proj.xy * 0.5 + 0.5;

    // float depthRef = proj.z * 0.5 + 0.5; <-- 이 줄을 삭제하거나 아래처럼 수정
    float currentDepth = proj.z;

    // 섀도우 맵 경계 체크
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
        outColor = base;
        return;
    }

    float shadowDepth = texture(shadowMap, proj.xy).r;

    // Bias는 0.001 ~ 0.005 사이에서 테스트
    float bias = 0.002;

    // 최종 판정
    float visibility = (currentDepth - bias > shadowDepth) ? 0.4 : 1.0;
    outColor = vec4(base.rgb * visibility, base.a);
}
