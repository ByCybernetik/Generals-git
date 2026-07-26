#version 450

layout(binding = 0) uniform UBO {
    mat4 mvp;
    vec2 _unusedScroll;
    vec2 waterAnimation; // x: riverVOrigin, y: MAP_XY_FACTOR
} ubo;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec2 inUV2;
layout(location = 3) in vec4 inColor;
layout(location = 4) in float inIsRiver;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec2 vUV2;
layout(location = 2) out vec4 vColor;
layout(location = 3) flat out int vIsRiver;

void main()
{
    float t = ubo.waterAnimation.x;
    if (inIsRiver > 0.5) {
        float flowV = -t + inUV.y
            + sin(6.28318530718 * inUV.y - 3.0 * t) / 22.0;
        vUV = vec2(inUV.x, flowV);
        vUV2 = vec2(inUV2.x, flowV);
    } else {
        float mapCoeff = 3.14159265359 / (4.0 * ubo.waterAnimation.y);
        vUV.x = inUV.x + 0.02 * cos(11.0 * t)
            * sin(25.0 * t + inPos.x * mapCoeff);
        vUV.y = inUV.y + 0.02 * cos(5.0 * t)
            * sin(25.0 * t + inPos.y * mapCoeff);
        vUV2 = inUV2;
    }
    vColor = inColor;
    vIsRiver = inIsRiver > 0.5 ? 1 : 0;
    gl_Position = ubo.mvp * vec4(inPos, 1.0);
}
