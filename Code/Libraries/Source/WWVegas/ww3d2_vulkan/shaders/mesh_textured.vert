#version 450

#include "frame_ubo.glsl"

layout(constant_id = 1) const uint HAS_NORMAL = 1;
layout(constant_id = 2) const uint HAS_DIFFUSE = 1;
layout(constant_id = 3) const uint TEX_LAYER_COUNT = 2;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in uint in_diffuse;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec2 in_uv2;

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out vec2 v_uv2;
layout(location = 3) out float v_fog_factor;
layout(location = 4) out vec3 v_normal_cs;
layout(location = 5) out vec2 v_shroud_uv;

const uint FLAG_LIGHTING = 1u;
const uint FLAG_DIFFUSE_FROM_VERTEX = 2u;
const uint FLAG_AMBIENT_FROM_VERTEX = 512u;
const uint FLAG_FOG = 16u;
const uint FLAG_SHADOW_DECAL = 32768u;
const uint FLAG_ROAD_CLOUD_MOD = 524288u;
const uint FLAG_MESH_SHROUD = 2097152u;
const uint FLAG_MESH_SHROUD_ROAD_MASK = 4194304u;
const uint FLAG_MESH_INLINE_SHROUD = 8388608u;

vec4 Unpack_Diffuse(uint c)
{
	float a = float((c >> 24u) & 0xffu) / 255.0;
	float r = float((c >> 16u) & 0xffu) / 255.0;
	float g = float((c >> 8u) & 0xffu) / 255.0;
	float b = float(c & 0xffu) / 255.0;
	return vec4(r, g, b, a);
}

/*
 * D3D8 fixed-function lighting (per light, then summed):
 *   emissive + material_ambient * scene_ambient
 *   + material_diffuse * sum(light_diffuse * max(N·L,0) * atten)
 *   + material_specular * sum(light_specular * pow(max(R·V,0), shininess) * atten)
 *
 * Directional: L = -Direction (same convention as D3DLIGHT8).
 * Point attenuation: 1 / (Att0 + Att1*d + Att2*d²).
 * light_params: directional yzw = specular rgb; point yzw = Att0, Att1, Att2.
 */
float D3d8_Light_Attenuation(int i, float dist)
{
	return 1.0 / max(
		ubo.light_params[i].y +
		ubo.light_params[i].z * dist +
		ubo.light_params[i].w * dist * dist,
		0.0001);
}

vec3 D3d8_Light_To_Surface(int i, vec3 world_pos)
{
	float light_type = ubo.light_params[i].x;
	if (light_type < 1.5) {
		/* D3D8 uses the opposite of the Direction member for diffuse. */
		return -normalize(ubo.light_dir_or_pos[i].xyz);
	}
	vec3 to_light = ubo.light_dir_or_pos[i].xyz - world_pos;
	float dist = length(to_light);
	if (dist < 0.0001) {
		return vec3(0.0);
	}
	return to_light / dist;
}

vec3 Evaluate_Diffuse_Light(vec3 normal_ws, vec3 world_pos)
{
	vec3 diffuse_acc = vec3(0.0);

	for (int i = 0; i < 4; ++i) {
		float light_type = ubo.light_params[i].x;
		if (light_type < 0.5) {
			continue;
		}
		vec3 light_dir = D3d8_Light_To_Surface(i, world_pos);
		if (dot(light_dir, light_dir) < 1e-8) {
			continue;
		}
		float atten = 1.0;
		if (light_type >= 1.5) {
			float dist = length(ubo.light_dir_or_pos[i].xyz - world_pos);
			atten = D3d8_Light_Attenuation(i, dist);
		}
		float ndotl = max(dot(normal_ws, light_dir), 0.0);
		diffuse_acc += ubo.light_diffuse[i].rgb * ndotl * atten;
	}
	return diffuse_acc;
}

vec3 Evaluate_Specular(vec3 normal_cs, vec3 world_pos)
{
	if (ubo.specular_enable < 0.5) {
		return vec3(0.0);
	}
	vec3 spec_acc = vec3(0.0);
	vec3 view_dir_cs = vec3(0.0, 0.0, 1.0);
	float shininess = max(ubo.material_shininess, 1.0);
	vec3 n = normalize(normal_cs);

	for (int i = 0; i < 4; ++i) {
		float light_type = ubo.light_params[i].x;
		if (light_type < 0.5) {
			continue;
		}
		vec3 light_dir_ws = D3d8_Light_To_Surface(i, world_pos);
		if (dot(light_dir_ws, light_dir_ws) < 1e-8) {
			continue;
		}
		float atten = 1.0;
		vec3 light_spec = ubo.light_params[i].yzw;
		if (light_type >= 1.5) {
			float dist = length(ubo.light_dir_or_pos[i].xyz - world_pos);
			atten = D3d8_Light_Attenuation(i, dist);
			light_spec = ubo.light_diffuse[i].rgb;
		}
		vec3 light_dir_cs = normalize(mat3(ubo.view) * light_dir_ws);
		vec3 reflect_dir = reflect(-light_dir_cs, n);
		float rdotv = max(dot(reflect_dir, view_dir_cs), 0.0);
		spec_acc += light_spec * pow(rdotv, shininess) * atten;
	}
	return ubo.material_specular.rgb * spec_acc;
}

vec2 Transform_Tex_UV(vec2 uv, vec4 mat)
{
	return vec2(uv.x * mat.x + mat.z, uv.y * mat.y + mat.w);
}

vec2 Compute_Stage_UV(int stage, vec2 uv0, vec2 uv1, vec3 normal_ws, vec3 normal_cs, vec3 world_pos)
{
	float tci = (stage == 0) ? ubo.tex_tci[0] : ubo.tex_tci[1];
	float uv_index = (stage == 0) ? ubo.tex_uv_index[0] : ubo.tex_uv_index[1];
	vec4 mat = (stage == 0) ? ubo.tex_mat[0] : ubo.tex_mat[1];

	vec2 base_uv = (uv_index < 0.5) ? uv0 : uv1;
	if (tci < 0.5) {
		return Transform_Tex_UV(base_uv, mat);
	}

	if (tci < 1.5) {
		return Transform_Tex_UV(normal_cs.xy, mat);
	}

	vec3 view_dir_ws = normalize(world_pos - (ubo.view * vec4(0.0, 0.0, 0.0, 1.0)).xyz);
	vec3 reflect_ws = reflect(-view_dir_ws, normal_ws);
	vec3 reflect_cs = normalize((vec4(reflect_ws, 0.0) * ubo.view).xyz);
	if (tci < 2.5) {
		return Transform_Tex_UV(reflect_cs.xy, mat);
	}

	/* CameraSpacePosition: cam_pos * inv(view) * tex ≈ world XY * scale + offset. */
	if (tci < 3.5) {
		return Transform_Tex_UV(world_pos.xy, mat);
	}

	float m = 2.0 * length(vec3(reflect_cs.x, reflect_cs.y, reflect_cs.z + 1.0));
	vec2 sphere_uv = vec2(reflect_cs.x / m + 0.5, -reflect_cs.y / m + 0.5);
	return Transform_Tex_UV(sphere_uv, mat);
}

void main()
{
	vec4 pos = vec4(in_position, 1.0);
	gl_Position = pos * ubo.world * ubo.view_proj;

	vec4 vertex_color;
	if (HAS_DIFFUSE != 0u) {
		vertex_color = Unpack_Diffuse(in_diffuse);
	} else {
		vertex_color = vec4(1.0);
	}

	/*
	 * Output alpha: material Diffuse.a is opacity (placement ghosts, stealth).
	 * Skinned meshes often write diffuse=0 when no color array exists — treat
	 * that zero alpha as "unused" so SRC_ALPHA stealth draws are not invisible.
	 * When vertex alpha is present (meshmatdesc bake / soft edges), multiply.
	 */
	float vertex_a = vertex_color.a;
	float out_alpha = ubo.material_diffuse.a;
	if (vertex_a > 0.001) {
		out_alpha *= vertex_a;
	}

	uint flags = uint(ubo.flags);

	/* Shadow decals carry object-space UVs in the VB; ignore leftover tex transforms. */
	if ((flags & FLAG_SHADOW_DECAL) != 0u) {
		v_color = vertex_color;
		v_uv = in_uv;
		v_uv2 = in_uv;
		v_fog_factor = 1.0;
		v_normal_cs = vec3(0.0, 0.0, 1.0);
		v_shroud_uv = vec2(0.0);
		return;
	}

	vec3 world_pos = (pos * ubo.world).xyz;
	v_shroud_uv = (vec4(world_pos, 1.0) * ubo.shroud_transform).xy;

	/* Mesh shroud: optional road-alpha mask (roads) vs full overlay (bridges). */
	if ((flags & FLAG_MESH_SHROUD) != 0u) {
		v_color = vec4(1.0);
		if ((flags & FLAG_MESH_SHROUD_ROAD_MASK) != 0u) {
			v_uv = in_uv;
			v_uv2 = v_shroud_uv;
		} else {
			v_uv = v_shroud_uv;
			v_uv2 = v_shroud_uv;
		}
		v_fog_factor = 1.0;
		v_normal_cs = vec3(0.0, 0.0, 1.0);
		return;
	}

	vec3 normal_ws;
	vec3 normal_cs;
	if (HAS_NORMAL != 0u) {
		normal_ws = normalize((vec4(in_normal, 0.0) * ubo.world).xyz);
		normal_cs = normalize((vec4(in_normal, 0.0) * ubo.world * ubo.view).xyz);
	} else {
		normal_ws = normalize((vec4(0.0, 0.0, 1.0, 0.0) * ubo.world).xyz);
		normal_cs = normalize((vec4(0.0, 0.0, 1.0, 0.0) * ubo.world * ubo.view).xyz);
	}
	v_normal_cs = normal_cs;

	if ((flags & FLAG_LIGHTING) != 0u) {
		vec3 ambient_src = ((flags & FLAG_AMBIENT_FROM_VERTEX) != 0u)
			? vertex_color.rgb
			: ubo.material_ambient.rgb;
		vec3 ambient = ubo.scene_ambient.rgb * ambient_src;
		vec3 diffuse_light = Evaluate_Diffuse_Light(normal_ws, world_pos);
		vec3 lit = ubo.material_emissive.rgb + ambient;
		if ((flags & FLAG_DIFFUSE_FROM_VERTEX) != 0u) {
			lit += vertex_color.rgb * diffuse_light;
		} else {
			lit += ubo.material_diffuse.rgb * diffuse_light;
		}
		if (ubo.specular_enable > 0.5) {
			lit += Evaluate_Specular(normalize(v_normal_cs), world_pos);
		}
		/* D3D8 saturates lighting before MODULATE; without this, infantry
		 * light scale + bright ambients blow textured meshes to white. */
		lit = clamp(lit, vec3(0.0), vec3(1.0));
		v_color = vec4(lit, out_alpha);
	} else if ((flags & FLAG_DIFFUSE_FROM_VERTEX) != 0u) {
		/* D3D8 DIFFUSEMATERIALSOURCE=COLOR1 with lighting off. */
		v_color = vec4(vertex_color.rgb, out_alpha);
	} else {
		/*
		 * D3D8 DIFFUSEMATERIALSOURCE=MATERIAL with lighting off uses the
		 * material diffuse (not the vertex diffuse slot). Skinned meshes often
		 * write diffuse=0 when no color array exists; using that made zero-W3D
		 * materials render black/white instead of textured.
		 */
		v_color = vec4(ubo.material_diffuse.rgb, out_alpha);
	}

	vec2 uv1 = in_uv;
	vec2 uv2 = (TEX_LAYER_COUNT >= 2u) ? in_uv2 : in_uv;
	v_uv = Compute_Stage_UV(0, uv1, uv2, normal_ws, normal_cs, world_pos);
	v_uv2 = Compute_Stage_UV(1, uv1, uv2, normal_ws, normal_cs, world_pos);

	/* Road cloud: same world-XY UV as terrain SPIR-V (ignore broken stage1 TCI). */
	if ((flags & FLAG_ROAD_CLOUD_MOD) != 0u) {
		float stretch = ubo.terrain_cloud_params.z;
		if (stretch <= 0.0) {
			stretch = 1.0 / 315.0;
		}
		v_uv2 = world_pos.xy * stretch + ubo.terrain_cloud_params.xy;
	}

	v_fog_factor = 1.0;
	if ((flags & FLAG_FOG) != 0u && ubo.fog_end > ubo.fog_start) {
		vec4 view_pos = pos * ubo.world * ubo.view;
		float depth = abs(view_pos.z);
		v_fog_factor = clamp((ubo.fog_end - depth) / (ubo.fog_end - ubo.fog_start), 0.0, 1.0);
	}
}
