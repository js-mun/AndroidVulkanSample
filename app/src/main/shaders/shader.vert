#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 mvp;
} ubo;

vec2 positions[3] = vec2[](
    vec2(0.0, 0.5),
    vec2(-0.5, -0.5),
    vec2(0.5, -0.5)
);

void main() {
    gl_Position = ubo.mvp * vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
