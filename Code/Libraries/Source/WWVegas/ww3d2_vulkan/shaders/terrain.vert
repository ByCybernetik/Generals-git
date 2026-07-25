#version 450

#include "frame_ubo.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 2) in uint in_diffuse;
layout(location = 3) in vec2 in_uv0;
layout(location = 4) in vec2 in_uv1;

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_uv0;
layout(location = 2) out vec2 v_uv1;
layout(location = 3) out float v_fog_factor;
layout(location = 5) out vec2 v_shroud_uv;
layout(location = 6) out vec2 v_cloud_uv;
layout(location = 7) out vec2 v_noise_uv;

const uint FLAG_FOG = 16u;

vec4 Unpack_Diffuse(uint c)
{
	float a = float((c >> 24u) & 0xffu) / 255.0;
	float r = float((c >> 16u) & 0xffu) / 255.0;
	float g = float((c >> 8u) & 0xffu) / 255.0;
	float b = float(c & 0xffu) / 255.0;
	return vec4(r, g, b, a);
}

void main()
{
	vec4 pos = vec4(in_position, 1.0);
	gl_Position = pos * ubo.world * ubo.view_proj;

	/* Terrain uses PRELIT_DIFFUSE: lighting baked into COLOR1, alpha holds blend weight. */
	v_color = Unpack_Diffuse(in_diffuse);

	v_uv0 = in_uv0;
	v_uv1 = in_uv1;

	vec3 world_pos = (pos * ubo.world).xyz;
	v_shroud_uv = (vec4(world_pos, 1.0) * ubo.shroud_transform).xy;

	/*
	 * D3D8 cloud/noise: CAMERASPACEPOSITION * inv(view) * stretch * scroll
	 * == world XY * stretch + scroll offset.
	 */
	float stretch = ubo.terrain_cloud_params.z;
	if (stretch <= 0.0) {
		stretch = 1.0 / 315.0; /* 1/(63*MAP_XY_FACTOR/2), MAP_XY_FACTOR=10 */
	}
	vec2 scroll = ubo.terrain_cloud_params.xy;
	v_cloud_uv = world_pos.xy * stretch + scroll;
	v_noise_uv = world_pos.xy * stretch;

	v_fog_factor = 1.0;
	if ((uint(ubo.flags) & FLAG_FOG) != 0u && ubo.fog_end > ubo.fog_start) {
		vec4 view_pos = pos * ubo.world * ubo.view;
		float depth = abs(view_pos.z);
		v_fog_factor = clamp((ubo.fog_end - depth) / (ubo.fog_end - ubo.fog_start), 0.0, 1.0);
	}
}
