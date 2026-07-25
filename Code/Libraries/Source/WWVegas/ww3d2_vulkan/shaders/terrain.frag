#version 450

#include "frame_ubo.glsl"

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv0;
layout(location = 2) in vec2 v_uv1;
layout(location = 3) in float v_fog_factor;
layout(location = 5) in vec2 v_shroud_uv;
layout(location = 6) in vec2 v_cloud_uv;
layout(location = 7) in vec2 v_noise_uv;

layout(set = 0, binding = 1) uniform sampler2D terrain_tex;
layout(set = 0, binding = 2) uniform sampler2D shroud_tex;
layout(set = 0, binding = 3) uniform sampler2D cloud_tex;
layout(set = 0, binding = 4) uniform sampler2D noise_tex;

layout(location = 0) out vec4 out_color;

const uint FLAG_FOG = 16u;
const uint FLAG_TERRAIN_CLOUD = 65536u;
const uint FLAG_TERRAIN_NOISE = 131072u;
const uint FLAG_TERRAIN_FOW_SHROUD = 1048576u;

void main()
{
	/* terrain.pso: lrp(t0, t1, v0.a); mul(v0) — single-pass equivalent of 2Stage blend. */
	vec4 t0 = texture(terrain_tex, v_uv0);
	vec4 t1 = texture(terrain_tex, v_uv1);
	/* terrain.nvp: lrp r0, v0.a, t1, t0 — blend weight is vertex alpha only. */
	float blend = v_color.a;
	vec3 rgb = mix(t0.rgb, t1.rgb, blend) * v_color.rgb;
	vec4 color = vec4(rgb, 1.0);

	uint flags = uint(ubo.flags);
	/* D3D8 order: base → cloud/noise modulate pass → shroud overlay.
	 * Keep cloud/noise before shroud so alpha FOW does not leave lit noise. */
	if ((flags & FLAG_TERRAIN_CLOUD) != 0u) {
		color.rgb *= texture(cloud_tex, v_cloud_uv).rgb;
	}
	if ((flags & FLAG_TERRAIN_NOISE) != 0u) {
		color.rgb *= texture(noise_tex, v_noise_uv).rgb;
	}

	vec4 shroud = texture(shroud_tex, v_shroud_uv);
	/*
	 * FOW (A4R4G4B4): AlphaSprite-style mix toward shroud.rgb by shroud.a.
	 * Classic (R5G6B5, a forced to 1): brightness is in rgb — multiply.
	 */
	if ((flags & FLAG_TERRAIN_FOW_SHROUD) != 0u) {
		color.rgb = mix(color.rgb, shroud.rgb, shroud.a);
	} else {
		color.rgb *= shroud.rgb;
	}

	if ((flags & FLAG_FOG) != 0u) {
		float f = v_fog_factor;
		if (ubo.fog_mode < 0.5) {
			color.rgb = mix(ubo.fog_color.rgb, color.rgb, f);
		} else if (ubo.fog_mode < 1.5) {
			color.rgb *= f;
		} else {
			color.rgb = mix(color.rgb, ubo.fog_color.rgb, 1.0 - f);
		}
	}

	out_color = color;
}
