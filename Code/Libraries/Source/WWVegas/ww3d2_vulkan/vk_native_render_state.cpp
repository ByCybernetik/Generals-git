#include "vk_native_render_state.h"

#if defined(RENEGADE_VULKAN)

#include "render_device.h"
#include "vk_dx8_state.h"
#include "vk_dx8_texture.h"
#include "../ww3d2/dx8wrapper.h"
#include "../ww3d2/shader.h"
#include "../ww3d2/texture.h"
#include "../wwmath/matrix4.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <d3d8.h>

namespace ww3d_vulkan {

namespace {

struct StageNative {
	UvMode uv_mode = UvMode::Passthru;
	uint8_t uv_index = 0;
	unsigned ttf = D3DTTFF_DISABLE;
	float tex_mat[4] = {1.0f, 1.0f, 0.0f, 0.0f};
	uint8_t address_u = D3DTADDRESS_WRAP;
	uint8_t address_v = D3DTADDRESS_WRAP;
	uint8_t min_filter = D3DTEXF_POINT;
	uint8_t mag_filter = D3DTEXF_POINT;
	uint8_t mip_filter = D3DTEXF_NONE;
};

static const unsigned kMaxStages = 2;

StageNative g_stage[kMaxStages];
float g_tex_stage0_mode = 1.0f;
float g_tex_stage1_color_mode = 0.0f;
float g_tex_stage1_alpha_mode = 0.0f;
float g_bump_mat[4] = {0.0f, 0.0f, 0.0f, 0.0f};
float g_bump_l_scale = 0.0f;
float g_bump_l_offset = 0.0f;
static float g_shroud_transform[16] = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
};
static float g_terrain_cloud_params[4] = {0.0f, 0.0f, 1.0f / 315.0f, 0.0f};
static uint32_t g_terrain_cloud_flags = 0;
static uint32_t g_extra_draw_flags = 0;

static NativeTextureState g_texture_override;
static bool g_texture_override_active = false;
static bool g_terrain_spirv_draw = false;

static UvMode Tci_To_UvMode(unsigned tci)
{
	const unsigned flags = (tci >> 16) & 0xFFFFu;
	switch (flags) {
	case 0:
		return UvMode::Passthru;
	case 1:
		return UvMode::CameraNormal;
	case 2:
		return UvMode::CameraReflection;
	case 3:
		return UvMode::CameraPosition;
	case 4:
		return UvMode::SphereMap;
	default:
		return UvMode::Passthru;
	}
}

static float Shader_Gradient_To_Stage0(const ShaderClass &shader)
{
	switch (shader.Get_Primary_Gradient()) {
	case ShaderClass::GRADIENT_DISABLE:
		return 0.0f;
	case ShaderClass::GRADIENT_ADD:
		return 2.0f;
	case ShaderClass::GRADIENT_BUMPENVMAP:
		return 3.0f;
	case ShaderClass::GRADIENT_BUMPENVMAPLUMINANCE:
		return 4.0f;
	case ShaderClass::GRADIENT_DOTPRODUCT3:
		return 5.0f;
	default:
		return 1.0f;
	}
}

static void Extract_Tex_Mat(const Matrix4 &tex_mat, float out[4])
{
	out[0] = tex_mat[0][0];
	out[1] = tex_mat[1][1];
	/*
	 * TextureMatrices[] stores Transpose(mapper_matrix).
	 * LinearOffset mappers put UV translation in [0][2]/[1][2] before transpose
	 * -> [2][0]/[2][1] after.
	 * ClassicEnvironmentMapper puts translation in row0/row1 W before transpose
	 * -> [3][0]/[3][1] after.
	 */
	out[2] = tex_mat[2][0];
	out[3] = tex_mat[2][1];
	if (out[2] == 0.0f && out[3] == 0.0f) {
		const float row3_u = tex_mat[3][0];
		const float row3_v = tex_mat[3][1];
		if (row3_u != 0.0f || row3_v != 0.0f) {
			out[2] = row3_u;
			out[3] = row3_v;
		}
	}
}

static bool Is_Valid_Ttf(unsigned ttf)
{
	return (ttf & ~D3DTTFF_PROJECTED) <= D3DTTFF_COUNT4 || ttf == D3DTTFF_DISABLE;
}

static void Apply_Stage_To_Draw_State(
	VulkanDrawState *state,
	unsigned stage,
	const StageNative &native)
{
	state->tex_uv_index[stage] = (float)native.uv_index;
	if (!Is_Valid_Ttf(native.ttf) || native.ttf == D3DTTFF_DISABLE) {
		state->tex_tci[stage] = 0.0f;
		state->tex_mat[stage][0] = 1.0f;
		state->tex_mat[stage][1] = 1.0f;
		state->tex_mat[stage][2] = 0.0f;
		state->tex_mat[stage][3] = 0.0f;
	} else {
		state->tex_tci[stage] = (float)static_cast<uint8_t>(native.uv_mode);
		state->tex_mat[stage][0] = native.tex_mat[0];
		state->tex_mat[stage][1] = native.tex_mat[1];
		state->tex_mat[stage][2] = native.tex_mat[2];
		state->tex_mat[stage][3] = native.tex_mat[3];
	}
}

static void Apply_Override_To_Draw_State(
	VulkanDrawState *state,
	const NativeTextureState &native)
{
	state->tex_stage0_mode = native.tex_stage0_mode;
	state->tex_stage1_color_mode = native.tex_stage1_color_mode;
	state->tex_stage1_alpha_mode = native.tex_stage1_alpha_mode;
	state->bump_mat[0] = 0.0f;
	state->bump_mat[1] = 0.0f;
	state->bump_mat[2] = 0.0f;
	state->bump_mat[3] = 0.0f;
	state->bump_l_scale = 0.0f;
	state->bump_l_offset = 0.0f;
	/* Preserve the shroud projection set by the terrain shader; the override
	 * only replaces legacy texture-stage state, not the SPIR-V terrain UBO. */
	memcpy(state->shroud_transform, g_shroud_transform, sizeof(state->shroud_transform));

	for (unsigned stage = 0; stage < kMaxStages; ++stage) {
		state->tex_tci[stage] = (float)static_cast<uint8_t>(native.uv_mode[stage]);
		state->tex_uv_index[stage] = (float)native.uv_index[stage];
		state->tex_mat[stage][0] = native.tex_mat[stage][0];
		state->tex_mat[stage][1] = native.tex_mat[stage][1];
		state->tex_mat[stage][2] = native.tex_mat[stage][2];
		state->tex_mat[stage][3] = native.tex_mat[stage][3];
	}
}

} /* namespace */

static ::TextureClass *g_terrain_cloud_tex = nullptr;
static ::TextureClass *g_terrain_noise_tex = nullptr;

void Set_Shroud_Transform(const float matrix[16])
{
	if (matrix != nullptr) {
		memcpy(g_shroud_transform, matrix, sizeof(g_shroud_transform));
	}
}

void Set_Terrain_Cloud_Params(float scroll_x, float scroll_y, float stretch, uint32_t flags)
{
	g_terrain_cloud_params[0] = scroll_x;
	g_terrain_cloud_params[1] = scroll_y;
	g_terrain_cloud_params[2] = (stretch > 0.0f) ? stretch : (1.0f / 315.0f);
	g_terrain_cloud_params[3] = 0.0f;
	g_terrain_cloud_flags = flags;
}

void Set_Terrain_Cloud_Textures(::TextureClass *cloud, ::TextureClass *noise)
{
	g_terrain_cloud_tex = cloud;
	g_terrain_noise_tex = noise;
}

void Rebind_Terrain_Cloud_Textures()
{
	if (!g_terrain_spirv_draw) {
		return;
	}
	if ((g_terrain_cloud_flags & (1u << 16)) != 0 && g_terrain_cloud_tex != nullptr) {
		Texture_Stage_Bind(g_terrain_cloud_tex, 2);
	} else {
		Texture_Stage_Bind_Null(2);
	}
	if ((g_terrain_cloud_flags & (1u << 17)) != 0 && g_terrain_noise_tex != nullptr) {
		Texture_Stage_Bind(g_terrain_noise_tex, 3);
	} else {
		Texture_Stage_Bind_Null(3);
	}
}

void Clear_Terrain_Cloud_Params()
{
	g_terrain_cloud_params[0] = 0.0f;
	g_terrain_cloud_params[1] = 0.0f;
	g_terrain_cloud_params[2] = 1.0f / 315.0f;
	g_terrain_cloud_params[3] = 0.0f;
	g_terrain_cloud_flags = 0;
	g_terrain_cloud_tex = nullptr;
	g_terrain_noise_tex = nullptr;
	g_terrain_spirv_draw = false;
}

void Set_Extra_Draw_Flags(uint32_t flags)
{
	g_extra_draw_flags = flags;
}

void Clear_Extra_Draw_Flags()
{
	g_extra_draw_flags = 0;
}

void Set_World_XY_Tex_Projection(
	unsigned stage,
	float stretch,
	float offset_u,
	float offset_v)
{
	const float s = (stretch > 0.0f) ? stretch : (1.0f / 315.0f);
	Set_World_XY_Tex_Projection_UV(
		stage, s, s, offset_u, offset_v, D3DTADDRESS_WRAP);
}

void Set_World_XY_Tex_Projection_UV(
	unsigned stage,
	float scale_u,
	float scale_v,
	float offset_u,
	float offset_v,
	unsigned address_mode)
{
	if (stage >= kMaxStages) {
		return;
	}
	StageNative &native = g_stage[stage];
	native.uv_mode = UvMode::CameraPosition;
	native.uv_index = 0;
	native.ttf = D3DTTFF_COUNT2;
	native.tex_mat[0] = scale_u;
	native.tex_mat[1] = scale_v;
	native.tex_mat[2] = offset_u;
	native.tex_mat[3] = offset_v;
	native.address_u = (uint8_t)address_mode;
	native.address_v = (uint8_t)address_mode;
}

void Native_Render_State_Reset()
{
	for (unsigned i = 0; i < kMaxStages; ++i) {
		g_stage[i] = StageNative();
	}
	g_stage[0].uv_index = 0;
	g_stage[1].uv_index = 1;
	g_tex_stage0_mode = 1.0f;
	g_tex_stage1_color_mode = 0.0f;
	g_tex_stage1_alpha_mode = 0.0f;
	g_bump_mat[0] = g_bump_mat[1] = g_bump_mat[2] = g_bump_mat[3] = 0.0f;
	g_bump_l_scale = 0.0f;
	g_bump_l_offset = 0.0f;
	g_shroud_transform[0] = 1.0f;
	g_shroud_transform[5] = 1.0f;
	g_shroud_transform[10] = 1.0f;
	g_shroud_transform[15] = 1.0f;
	Clear_Terrain_Cloud_Params();
	Clear_Extra_Draw_Flags();
	g_texture_override_active = false;
	g_terrain_spirv_draw = false;

	/*
	 * Begin_Frame resets g_stage filters to POINT/NONE defaults. The DX8
	 * TextureStageStates cache still holds last frame's LINEAR — TextureClass::Apply
	 * then early-outs and leaves nearest+mip0 samplers (camera sparkle on DXT).
	 */
	for (unsigned i = 0; i < kMaxStages; ++i) {
		DX8Wrapper::Invalidate_Texture_Stage_State_Cache(i, D3DTSS_MINFILTER);
		DX8Wrapper::Invalidate_Texture_Stage_State_Cache(i, D3DTSS_MAGFILTER);
		DX8Wrapper::Invalidate_Texture_Stage_State_Cache(i, D3DTSS_MIPFILTER);
		DX8Wrapper::Invalidate_Texture_Stage_State_Cache(i, D3DTSS_ADDRESSU);
		DX8Wrapper::Invalidate_Texture_Stage_State_Cache(i, D3DTSS_ADDRESSV);
		DX8Wrapper::Invalidate_Texture_Stage_State_Cache(i, D3DTSS_TEXCOORDINDEX);
		DX8Wrapper::Invalidate_Texture_Stage_State_Cache(i, D3DTSS_TEXTURETRANSFORMFLAGS);
	}
}

void Native_Render_State_On_Tss(unsigned stage, unsigned tss, unsigned value)
{
	if (stage >= kMaxStages) {
		return;
	}

	StageNative &native = g_stage[stage];
	switch (tss) {
	case D3DTSS_TEXCOORDINDEX:
		native.uv_index = (uint8_t)(value & 0xFFFFu);
		native.uv_mode = Tci_To_UvMode(value);
		break;
	case D3DTSS_TEXTURETRANSFORMFLAGS:
		native.ttf = value;
		break;
	case D3DTSS_ADDRESSU:
		native.address_u = (uint8_t)value;
		break;
	case D3DTSS_ADDRESSV:
		native.address_v = (uint8_t)value;
		break;
	case D3DTSS_MINFILTER:
		native.min_filter = (uint8_t)value;
		break;
	case D3DTSS_MAGFILTER:
		native.mag_filter = (uint8_t)value;
		break;
	case D3DTSS_MIPFILTER:
		native.mip_filter = (uint8_t)value;
		break;
	case D3DTSS_COLOROP:
		/*
		 * Map fixed-function COLOROP into mesh SPIR-V stage modes.
		 * DetailColorFunc: 0=off, 1=select, 2=modulate/scale, ...
		 */
		if (stage == 1) {
			switch (value) {
			case D3DTOP_DISABLE:
				g_tex_stage1_color_mode = 0.0f;
				break;
			case D3DTOP_SELECTARG1:
				g_tex_stage1_color_mode = 1.0f;
				break;
			case D3DTOP_MODULATE:
			case D3DTOP_MODULATE2X:
			case D3DTOP_MODULATE4X:
				g_tex_stage1_color_mode = 2.0f;
				break;
			case D3DTOP_ADD:
				g_tex_stage1_color_mode = 4.0f;
				break;
			case D3DTOP_ADDSIGNED:
			case D3DTOP_ADDSIGNED2X:
				g_tex_stage1_color_mode = 4.0f;
				break;
			case D3DTOP_SUBTRACT:
				g_tex_stage1_color_mode = 5.0f;
				break;
			case D3DTOP_BLENDTEXTUREALPHA:
				g_tex_stage1_color_mode = 7.0f;
				break;
			default:
				break;
			}
		}
		break;
	case D3DTSS_ALPHAOP:
		if (stage == 1) {
			switch (value) {
			case D3DTOP_DISABLE:
				g_tex_stage1_alpha_mode = 0.0f;
				break;
			case D3DTOP_SELECTARG1:
				g_tex_stage1_alpha_mode = 1.0f;
				break;
			case D3DTOP_MODULATE:
				g_tex_stage1_alpha_mode = 2.0f;
				break;
			default:
				break;
			}
		}
		break;
	case D3DTSS_BUMPENVMAT00:
		g_bump_mat[0] = *reinterpret_cast<const float *>(&value);
		break;
	case D3DTSS_BUMPENVMAT01:
		g_bump_mat[1] = *reinterpret_cast<const float *>(&value);
		break;
	case D3DTSS_BUMPENVMAT10:
		g_bump_mat[2] = *reinterpret_cast<const float *>(&value);
		break;
	case D3DTSS_BUMPENVMAT11:
		g_bump_mat[3] = *reinterpret_cast<const float *>(&value);
		break;
	case D3DTSS_BUMPENVLSCALE:
		g_bump_l_scale = *reinterpret_cast<const float *>(&value);
		break;
	case D3DTSS_BUMPENVLOFFSET:
		g_bump_l_offset = *reinterpret_cast<const float *>(&value);
		break;
	default:
		break;
	}
}

void Native_Render_State_Sync_Stage_Matrix(
	unsigned stage,
	const Matrix4 &tex_mat,
	unsigned texture_transform_flags)
{
	if (stage >= kMaxStages) {
		return;
	}

	g_stage[stage].ttf = texture_transform_flags;
	Extract_Tex_Mat(tex_mat, g_stage[stage].tex_mat);
	/*
	 * Set_Transform(TEXTUREn) calls Record_Texture_Matrix before TTF is configured.
	 * TextureStageStates[] may still hold the debug sentinel 0x12345678 — never
	 * let that poison g_stage.ttf or every draw inherits water UV scroll/scale.
	 */
	if (!Is_Valid_Ttf(texture_transform_flags)) {
		g_stage[stage].ttf = D3DTTFF_DISABLE;
	}
}

void Native_Render_State_On_Shader(const ShaderClass &shader)
{
	if (shader.Get_Texturing() == ShaderClass::TEXTURING_ENABLE) {
		g_tex_stage0_mode = Shader_Gradient_To_Stage0(shader);
	} else {
		switch (shader.Get_Primary_Gradient()) {
		case ShaderClass::GRADIENT_DISABLE:
			g_tex_stage0_mode = 0.0f;
			break;
		default:
			g_tex_stage0_mode = 0.0f;
			break;
		}
	}
	g_tex_stage1_color_mode = (float)shader.Get_Post_Detail_Color_Func();
	g_tex_stage1_alpha_mode = (float)shader.Get_Post_Detail_Alpha_Func();
}

void Native_Render_State_Fill_Texture(VulkanDrawState *state)
{
	if (state == nullptr) {
		return;
	}

	state->tex_stage0_mode = g_tex_stage0_mode;
	state->tex_stage1_color_mode = g_tex_stage1_color_mode;
	state->tex_stage1_alpha_mode = g_tex_stage1_alpha_mode;
	state->bump_mat[0] = g_bump_mat[0];
	state->bump_mat[1] = g_bump_mat[1];
	state->bump_mat[2] = g_bump_mat[2];
	state->bump_mat[3] = g_bump_mat[3];
	state->bump_l_scale = g_bump_l_scale;
	state->bump_l_offset = g_bump_l_offset;

	for (unsigned stage = 0; stage < kMaxStages; ++stage) {
		Apply_Stage_To_Draw_State(state, stage, g_stage[stage]);
	}
	memcpy(state->shroud_transform, g_shroud_transform, sizeof(g_shroud_transform));
	memcpy(state->terrain_cloud_params, g_terrain_cloud_params, sizeof(g_terrain_cloud_params));
	state->flags |= g_terrain_cloud_flags;
	state->flags |= g_extra_draw_flags;

	if (g_texture_override_active) {
		Apply_Override_To_Draw_State(state, g_texture_override);
	}
}

void Get_Stage_Sampler_State(
	unsigned stage,
	uint8_t *address_u,
	uint8_t *address_v,
	uint8_t *min_filter,
	uint8_t *mag_filter,
	uint8_t *mip_filter)
{
	if (stage >= kMaxStages) {
		/* Stage 2 = cloud (linear). Stage 3 = lightmap (point min, like D3D). */
		if (address_u != nullptr) { *address_u = D3DTADDRESS_WRAP; }
		if (address_v != nullptr) { *address_v = D3DTADDRESS_WRAP; }
		if (stage >= 3) {
			if (min_filter != nullptr) { *min_filter = D3DTEXF_POINT; }
			if (mag_filter != nullptr) { *mag_filter = D3DTEXF_LINEAR; }
			if (mip_filter != nullptr) { *mip_filter = D3DTEXF_POINT; }
		} else {
			if (min_filter != nullptr) { *min_filter = D3DTEXF_LINEAR; }
			if (mag_filter != nullptr) { *mag_filter = D3DTEXF_LINEAR; }
			if (mip_filter != nullptr) { *mip_filter = D3DTEXF_LINEAR; }
		}
		return;
	}
	const StageNative &native = g_stage[stage];
	if (address_u != nullptr) { *address_u = native.address_u; }
	if (address_v != nullptr) { *address_v = native.address_v; }
	if (min_filter != nullptr) { *min_filter = native.min_filter; }
	if (mag_filter != nullptr) { *mag_filter = native.mag_filter; }
	if (mip_filter != nullptr) { *mip_filter = native.mip_filter; }
}

void Push_Native_Texture_State(const NativeTextureState &state)
{
	g_texture_override = state;
	g_texture_override_active = true;
}

void Clear_Native_Texture_State()
{
	/* Do not clear g_terrain_spirv_draw here — UI/2-stage reset must not
	 * drop terrain SPIR-V mode mid-pass (cloud/noise flags rely on it). */
	g_texture_override_active = false;
}

bool Native_Texture_Override_Active()
{
	return g_texture_override_active;
}

bool Terrain_Spirv_Draw_Active()
{
	return g_terrain_spirv_draw;
}

bool Apply_Native_Texture_State(VulkanDrawState *state)
{
	if (!g_texture_override_active || state == nullptr) {
		return false;
	}
	Apply_Override_To_Draw_State(state, g_texture_override);
	return true;
}

void Terrain_Push_Vulkan_Draw_State(unsigned uv_index, bool alpha_blend)
{
	if (!DX8Wrapper::Vulkan_Device_Active()) {
		return;
	}
	NativeTextureState native;
	native.uv_index[0] = (uint8_t)uv_index;
	native.uv_mode[0] = UvMode::Passthru;
	native.tex_stage0_mode = 1.0f;
	native.tex_stage1_color_mode = 0.0f;
	native.tex_stage1_alpha_mode = 0.0f;
	Push_Native_Texture_State(native);

	ShaderClass shader;
	DX8Wrapper::Get_Shader(shader);
	if (alpha_blend) {
		shader.Set_Src_Blend_Func(ShaderClass::SRCBLEND_SRC_ALPHA);
		shader.Set_Dst_Blend_Func(ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA);
	} else {
		shader.Set_Src_Blend_Func(ShaderClass::SRCBLEND_ONE);
		shader.Set_Dst_Blend_Func(ShaderClass::DSTBLEND_ZERO);
	}
	DX8Wrapper::Set_Shader(shader);
}

void Terrain_Push_Spirv_Draw_State()
{
	if (!DX8Wrapper::Vulkan_Device_Active()) {
		return;
	}
	NativeTextureState native;
	native.tex_stage0_mode = 1.0f;
	native.tex_stage1_color_mode = 0.0f;
	native.tex_stage1_alpha_mode = 0.0f;
	Push_Native_Texture_State(native);
	g_terrain_spirv_draw = true;

	ShaderClass shader;
	DX8Wrapper::Get_Shader(shader);
	shader.Set_Src_Blend_Func(ShaderClass::SRCBLEND_ONE);
	shader.Set_Dst_Blend_Func(ShaderClass::DSTBLEND_ZERO);
	if (DX8Wrapper::Get_Fog_Enable()) {
		shader.Set_Fog_Func(ShaderClass::FOG_ENABLE);
	}
	DX8Wrapper::Set_Shader(shader);
}

} /* namespace ww3d_vulkan */

#endif /* RENEGADE_VULKAN */
