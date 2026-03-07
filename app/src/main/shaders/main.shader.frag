#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 viewProj;
    mat4 lightViewProj;
    vec4 lightDir;
} ubo;

layout(set = 1, binding = 1) uniform sampler2D texSampler;
layout(set = 0, binding = 2) uniform sampler2D shadowMap;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 fragShadowCoord;
layout(location = 3) in vec3 fragWorldPos;
layout(location = 4) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

float sampleShadowPCF(vec2 uv, float compareDepth, float bias) {
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float shadow = 0.0;

    for (int y = 0; y <= 1; ++y) {
        for (int x = 0; x <= 1; ++x) {
            vec2 offset = (vec2(x, y) - vec2(0.5)) * texelSize;
            float shadowDepth = texture(shadowMap, uv + offset).r;
            shadow += (compareDepth - bias > shadowDepth) ? 1.0 : 0.0;
        }
    }

    return shadow / 4.0;
}

void main() {
    vec4 base = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);

    vec3 proj = fragShadowCoord.xyz / fragShadowCoord.w;
    proj.xy = proj.xy * 0.5 + 0.5;
    float currentDepth = proj.z;
    bool inShadowMap = proj.x >= 0.0 && proj.x <= 1.0 &&
                       proj.y >= 0.0 && proj.y <= 1.0 &&
                       currentDepth >= 0.0 && currentDepth <= 1.0;

    vec3 N = normalize(fragNormal);
    vec3 L = normalize(-ubo.lightDir.xyz);
    float ndotl = max(dot(N, L), 0.0);

    // 각도 기반 bias
    float bias = max(0.0060 * (1.0 - ndotl), 0.0020);
    bias = min(bias, 0.0150);

    float shadow = (inShadowMap && ndotl > 0.0) ? sampleShadowPCF(proj.xy, currentDepth, bias) : 0.0;

    float ambient = 0.24;
    float diffuse = ndotl;

    // 그림자는 직접광에만 적용하고, 약한 주변광은 유지한다.
    float directLight = 1.10 * diffuse * mix(1.0, 0.58, shadow);
    float lighting = min(ambient + directLight, 1.28);

    outColor = vec4(base.rgb * lighting, base.a);
}
