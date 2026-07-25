#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec2 vUV2;
layout(location = 2) in vec3 vColor;
layout(location = 3) in float vBlend;

layout(location = 0) out vec4 outColor;

void main()
{
    /* Vertex color: white = textured terrain, otherwise solid border lines. */
    if (vColor.r < 0.99 || vColor.g < 0.99 || vColor.b < 0.99)
    {
        outColor = vec4(vColor, 1.0);
        return;
    }

    /* Original HeightMap: stage0=base UV, stage1=blend UV, diffuse.a = blend weight. */
    vec3 base = texture(texSampler, vUV).rgb;
    vec3 blend = texture(texSampler, vUV2).rgb;
    outColor = vec4(mix(base, blend, clamp(vBlend, 0.0, 1.0)), 1.0);
}
