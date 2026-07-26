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
    vec4 texel = texture(texWater, vUV);
    vec4 c;
    /* Retail TWWater01 can be nearly black: original PS adds sparkle/noise
       stages afterwards. Preserve the Water.ini tint while using the base
       texture as detail modulation in this reduced Vulkan path. */
    vec3 waterTint = max(vColor.rgb, vec3(0.18, 0.28, 0.34));
    c.rgb = clamp(waterTint * (0.55 + 0.65 * texel.rgb), 0.0, 1.0);
    /* Original setupFlatWaterShader/setupJbaWaterShader uses
       D3DTOP_ADD for stage-0 alpha, not MODULATE. */
    c.a = min(texel.a + vColor.a, 1.0);
    if (vIsRiver != 0)
        c.a *= texture(texAlphaEdge, vUV2).a;
    if (c.a < 0.01)
        discard;
    outColor = c;
}
