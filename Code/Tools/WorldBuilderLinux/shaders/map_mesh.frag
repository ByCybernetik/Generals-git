#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec2 vUV2;
layout(location = 2) in vec3 vColor;
layout(location = 3) in float vBlend;

layout(location = 0) out vec4 outColor;

void main()
{
    /* blend < 0: solid border/playable lines (unlit editor overlays). */
    if (vBlend < 0.0)
    {
        outColor = vec4(vColor, 1.0);
        return;
    }

    /* Original HeightMap PRELIT: stage0=base UV, stage1=blend UV, modulate by baked diffuse. */
    vec3 base = texture(texSampler, vUV).rgb;
    vec3 blend = texture(texSampler, vUV2).rgb;
    vec3 albedo = mix(base, blend, clamp(vBlend, 0.0, 1.0));
    outColor = vec4(albedo * vColor, 1.0);
}
