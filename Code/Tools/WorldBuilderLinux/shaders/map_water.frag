#version 450

layout(binding = 1) uniform sampler2D texWater;
layout(binding = 2) uniform sampler2D texAlphaEdge;

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec2 vUV2;
layout(location = 2) in vec4 vColor;
layout(location = 3) flat in int vIsRiver;
layout(location = 0) out vec4 outColor;

void main()
{
    vec4 c = texture(texWater, vUV) * vColor;
    if (vIsRiver != 0)
        c.a *= texture(texAlphaEdge, vUV2).a;
    if (c.a < 0.01)
        discard;
    outColor = c;
}
