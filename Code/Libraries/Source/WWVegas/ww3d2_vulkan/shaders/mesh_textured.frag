#version 450

layout(constant_id = 0) const uint ALPHA_TEST_ENABLE = 0;

#include "frame_ubo.glsl"

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec2 v_uv2;
layout(location = 3) in float v_fog_factor;
layout(location = 4) in vec3 v_normal_cs;
layout(location = 5) in vec2 v_shroud_uv;

layout(set = 0, binding = 1) uniform sampler2D diffuse_tex;
layout(set = 0, binding = 2) uniform sampler2D detail_tex;
layout(set = 0, binding = 3) uniform sampler2D shroud_tex;

layout(location = 0) out vec4 out_color;

const uint FLAG_TEXTURING = 4u;
const uint FLAG_COLOR1_UNLIT_MODULATE = 8u;
const uint FLAG_FOG = 16u;
const uint FLAG_SCREEN_BLEND_UNLIT = 32u;
const uint FLAG_SCREEN_BLEND_LIT = 64u;
const uint FLAG_SCREEN_BLEND_EVALOGO = 128u;
const uint FLAG_SCREEN_BLEND_GIZMO_DIM = 256u;
const uint FLAG_PARTICLE_SIMPLE = 16384u;
const uint FLAG_SHADOW_DECAL = 32768u;
const uint FLAG_ROAD_LIGHTMAP_MASK = 262144u;
const uint FLAG_ROAD_CLOUD_MOD = 524288u;
const uint FLAG_TERRAIN_FOW_SHROUD = 1048576u;
const uint FLAG_MESH_SHROUD = 2097152u;
const uint FLAG_MESH_SHROUD_ROAD_MASK = 4194304u;
const uint FLAG_MESH_INLINE_SHROUD = 8388608u;

vec2 Apply_Bump_Offset(vec2 uv, vec4 bump_texel)
{
	vec2 bump = bump_texel.rg * 2.0 - 1.0;
	float du = bump.x * ubo.bump_mat[0] + bump.y * ubo.bump_mat[1];
	float dv = bump.x * ubo.bump_mat[2] + bump.y * ubo.bump_mat[3];
	return uv + vec2(du, dv);
}

vec4 Apply_Stage0(vec4 base_color, vec4 texel, vec2 uv)
{
	float mode = ubo.tex_stage0_mode;
	if (mode < 0.5) {
		return vec4(texel.rgb, texel.a * base_color.a);
	}
	if (mode < 1.5) {
		vec3 rgb = base_color.rgb;
		if ((uint(ubo.flags) & FLAG_COLOR1_UNLIT_MODULATE) != 0u && dot(rgb, rgb) < 1e-8) {
			rgb = vec3(1.0);
		}
		return vec4(texel.rgb * rgb, texel.a * base_color.a);
	}
	if (mode < 2.5) {
		return vec4(texel.rgb + base_color.rgb, texel.a * base_color.a);
	}
	if (mode < 3.5) {
		vec2 env_uv = Apply_Bump_Offset(uv, texel);
		vec4 env = texture(detail_tex, env_uv);
		return vec4(env.rgb * base_color.rgb, base_color.a);
	}
	if (mode < 4.5) {
		vec2 env_uv = Apply_Bump_Offset(uv, texel);
		vec4 env = texture(detail_tex, env_uv);
		float lum = texel.r * ubo.bump_l_scale + ubo.bump_l_offset;
		return vec4(env.rgb * lum * base_color.rgb, base_color.a);
	}
	vec3 perturbed = normalize(texel.rgb * 2.0 - 1.0);
	float dp = dot(perturbed, normalize(v_normal_cs));
	return vec4(base_color.rgb * dp, base_color.a);
}

vec4 Apply_Stage1Color(vec4 current, vec4 detail)
{
	float mode = ubo.tex_stage1_color_mode;
	if (mode < 0.5) {
		return current;
	}
	if (mode < 1.5) {
		return vec4(detail.rgb, current.a);
	}
	if (mode < 2.5) {
		return current * detail;
	}
	if (mode < 3.5) {
		return current + (vec4(1.0) - current) * detail;
	}
	if (mode < 4.5) {
		return current + detail;
	}
	if (mode < 5.5) {
		return current - detail;
	}
	if (mode < 6.5) {
		return detail - current;
	}
	if (mode < 7.5) {
		return detail.a * detail + (1.0 - detail.a) * current;
	}
	return detail.a * current + (1.0 - detail.a) * detail;
}

float Apply_Stage1Alpha(float current, float detail_a)
{
	float mode = ubo.tex_stage1_alpha_mode;
	if (mode < 0.5) {
		return current;
	}
	if (mode < 1.5) {
		return detail_a;
	}
	if (mode < 2.5) {
		return current * detail_a;
	}
	return current + (1.0 - current) * detail_a;
}

void main()
{
	uint flags = uint(ubo.flags);

	if ((flags & FLAG_PARTICLE_SIMPLE) != 0u) {
		vec4 texel = texture(diffuse_tex, v_uv);
		out_color = vec4(texel.rgb * v_color.rgb, texel.a * v_color.a);
		return;
	}

	if ((flags & FLAG_SHADOW_DECAL) != 0u) {
		vec4 texel = texture(diffuse_tex, v_uv);
		out_color = vec4(texel.rgb * v_color.rgb, texel.a * v_color.a);
		return;
	}

	/*
	 * Mesh shroud overlay. Roads: multiply factor masked by road alpha.
	 * Objects/bridges: FOW uses dest*=(1-a); classic multiplies by shroud.rgb.
	 */
	if ((flags & FLAG_MESH_SHROUD) != 0u) {
		if ((flags & FLAG_MESH_SHROUD_ROAD_MASK) != 0u) {
			float road_a = texture(diffuse_tex, v_uv).a;
			vec4 shroud = texture(detail_tex, v_uv2);
			if ((flags & FLAG_TERRAIN_FOW_SHROUD) != 0u) {
				float f = 1.0 - shroud.a * road_a;
				out_color = vec4(vec3(f), 1.0);
			} else {
				out_color = vec4(mix(vec3(1.0), shroud.rgb, road_a), 1.0);
			}
		} else if ((flags & FLAG_TERRAIN_FOW_SHROUD) != 0u) {
			/* FOW texels are black RGB + alpha; AlphaSprite is unreliable here.
			 * Match terrain darkening: Frame *= (1 - shroud.a).
			 * Clamp so a bad UV/border sample cannot wipe the model to black. */
			float a = texture(diffuse_tex, v_uv).a;
			float f = 1.0 - a;
			f = max(f, 0.35);
			out_color = vec4(vec3(f), 1.0);
		} else {
			vec4 s = texture(diffuse_tex, v_uv);
			/* FOW texture without FOW flag still has black RGB — never multiply
			 * that straight into the framebuffer. */
			if (dot(s.rgb, s.rgb) < 0.02) {
				float f = max(1.0 - s.a, 0.35);
				out_color = vec4(vec3(f), 1.0);
			} else {
				out_color = s;
			}
		}
		return;
	}

	/* Road NOISE12 pass 1: Frame *= mix(1, lightmap, roadAlpha). */
	if ((flags & FLAG_ROAD_LIGHTMAP_MASK) != 0u) {
		float road_a = texture(diffuse_tex, v_uv).a;
		vec3 noise = texture(detail_tex, v_uv2).rgb;
		out_color = vec4(mix(vec3(1.0), noise, road_a), 1.0);
		return;
	}

	vec4 color = v_color;

	if ((flags & FLAG_TEXTURING) != 0u) {
		vec4 texel = texture(diffuse_tex, v_uv);
		color = Apply_Stage0(color, texel, v_uv);
	}

	/* Road cloud: always modulate; do not rely on ShaderClass PostDetail bits. */
	if ((flags & FLAG_ROAD_CLOUD_MOD) != 0u) {
		color.rgb *= texture(detail_tex, v_uv2).rgb;
	} else if ((flags & FLAG_TEXTURING) != 0u &&
			(ubo.tex_stage1_color_mode > 0.5 || ubo.tex_stage1_alpha_mode > 0.5)) {
		vec4 detail = texture(detail_tex, v_uv2);
		color = Apply_Stage1Color(color, detail);
		color.a = Apply_Stage1Alpha(color.a, detail.a);
	}

	if ((flags & FLAG_SCREEN_BLEND_UNLIT) != 0u) {
		color.rgb *= ubo.material_diffuse.rgb;
	}

	if ((flags & FLAG_SCREEN_BLEND_LIT) != 0u) {
		bool is_evalogo = (flags & FLAG_SCREEN_BLEND_EVALOGO) != 0u;
		bool is_gizmo_dim = (flags & FLAG_SCREEN_BLEND_GIZMO_DIM) != 0u;
		float mod_max = max(max(color.r, color.g), color.b);
		float dark_thresh = is_evalogo ? 0.12 : 0.06;
		float dark_scale = is_evalogo ? 0.55 : 0.45;
		if (mod_max < dark_thresh) {
			color.rgb = max(color.rgb, v_color.rgb * dark_scale);
		}
		float md = max(max(ubo.material_diffuse.r, ubo.material_diffuse.g), ubo.material_diffuse.b);
		float gain_scale = is_evalogo ? 1.0 : (is_gizmo_dim ? 0.15 : 0.5);
		float gain = 1.0 + gain_scale * max(0.0, 1.0 - md);
		color.rgb = min(color.rgb * gain, vec3(1.0));
	}

	if ((flags & FLAG_SCREEN_BLEND_GIZMO_DIM) != 0u) {
		color.rgb *= 0.45;
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

	/* Same FOW/classic shroud as terrain — once per pixel, no multiply stacking. */
	if ((flags & FLAG_MESH_INLINE_SHROUD) != 0u) {
		vec4 shroud = texture(shroud_tex, v_shroud_uv);
		if ((flags & FLAG_TERRAIN_FOW_SHROUD) != 0u) {
			color.rgb = mix(color.rgb, shroud.rgb, shroud.a);
		} else {
			color.rgb *= shroud.rgb;
		}
	}

	out_color = color;

	/*
	 * D3D scales ALPHAREF by material opacity for placement ghosts
	 * (0x60 * alphaOverride). Scale the cutout the same way so translucent
	 * builds are not discarded when opacity < 0.5.
	 */
	if (ALPHA_TEST_ENABLE != 0u) {
		float ref = 0.5 * max(ubo.material_diffuse.a, 1e-5);
		if (out_color.a < ref) {
			discard;
		}
	}
}
