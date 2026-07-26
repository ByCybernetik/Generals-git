#version 450

layout(binding = 0) uniform UBO {
    mat4 mvp;
} ubo;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec3 vColor;

void main()
{
    vUV = inUV;
    vColor = inColor;
    gl_Position = ubo.mvp * vec4(inPos, 1.0);
}
