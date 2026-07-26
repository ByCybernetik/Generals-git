#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec3 vColor;
layout(location = 0) out vec4 outColor;

void main()
{
    vec4 c = texture(texSampler, vUV);
    if (c.a < 0.01)
        discard;
    outColor = vec4(c.rgb * vColor, c.a);
}
