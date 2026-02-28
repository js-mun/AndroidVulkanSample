#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 viewProj;
    mat4 lightViewProj;
    vec4 lightPos;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform sampler2D shadowMap;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 fragShadowCoord;
layout(location = 3) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 base = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);

    vec3 proj = fragShadowCoord.xyz / fragShadowCoord.w;
    proj.xy = proj.xy * 0.5 + 0.5;
    float currentDepth = proj.z;

    if (proj.x < 0.0 || proj.x > 1.0 ||
        proj.y < 0.0 || proj.y > 1.0 ||
        currentDepth < 0.0 || currentDepth > 1.0) {
        outColor = base;
        return;
    }

    // screen-space normal 근사
    vec3 dx = dFdx(fragWorldPos);
    vec3 dy = dFdy(fragWorldPos);
    vec3 N = normalize(cross(dx, dy));

    vec3 L = normalize(ubo.lightPos.xyz - fragWorldPos);
    float ndotl = max(dot(N, L), 0.0);

    // 각도 기반 bias
    float bias = max(0.0006 * (1.0 - ndotl), 0.0002);
    bias = min(bias, 0.0025);

    // 3x3 PCF
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float shadow = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 uv = proj.xy + vec2(float(x), float(y)) * texelSize;
            float pcfDepth = texture(shadowMap, uv).r;
            shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    float visibility = mix(1.0, 0.45, shadow);
    outColor = vec4(base.rgb * visibility, base.a);
}
