#version 450
layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    // [수정] 텍스처에서 색상을 읽어온 후 정점 색상과 곱합니다.
    // texture() 함수가 UV 좌표(fragTexCoord)를 이용해 픽셀을 가져옵니다.
    outColor = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);
}
