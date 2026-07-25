#version 450

layout(binding = 0) uniform UBO {
    mat4 mvp;
} ubo;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec2 inUV2;
layout(location = 3) in vec3 inColor;
layout(location = 4) in float inBlend;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec2 vUV2;
layout(location = 2) out vec3 vColor;
layout(location = 3) out float vBlend;

void main()
{
    vUV = inUV;
    vUV2 = inUV2;
    vColor = inColor;
    vBlend = inBlend;
    gl_Position = ubo.mvp * vec4(inPos, 1.0);
}
