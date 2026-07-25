#ifndef WW3D2_VULKAN_NATIVE_RENDER_STATE_H
#define WW3D2_VULKAN_NATIVE_RENDER_STATE_H

#if defined(RENEGADE_VULKAN)

#include <stdint.h>

struct Matrix4;
class ShaderClass;
class TextureClass;

namespace ww3d_vulkan {

struct NativeTextureState;
struct VulkanDrawState;

/* Per-stage native UV / texture-matrix state (Phase 1 — replaces D3DTSS at draw time). */
void Native_Render_State_Reset();
void Native_Render_State_On_Tss(unsigned stage, unsigned tss, unsigned value);
void Native_Render_State_Sync_Stage_Matrix(
	unsigned stage,
	const Matrix4 &tex_mat,
	unsigned texture_transform_flags);
void Native_Render_State_On_Shader(const ShaderClass &shader);
void Native_Render_State_Fill_Texture(VulkanDrawState *state);

/**
 * Read the last D3DTSS_ADDRESSU/V/MINFILTER/MAGFILTER/MIPFILTER values
 * recorded for a texture stage. Values are the raw D3D8 enums.
 */
void Get_Stage_Sampler_State(
	unsigned stage,
	uint8_t *address_u,
	uint8_t *address_v,
	uint8_t *min_filter,
	uint8_t *mag_filter,
	uint8_t *mip_filter);

/* UI draw override (priority over per-stage registry). */
void Push_Native_Texture_State(const NativeTextureState &state);
void Clear_Native_Texture_State();
bool Native_Texture_Override_Active();
bool Terrain_Spirv_Draw_Active();
bool Apply_Native_Texture_State(VulkanDrawState *state);

/** Terrain custom shader: sync UV index, combiners, and alpha-blend pipeline. */
void Terrain_Push_Vulkan_Draw_State(unsigned uv_index, bool alpha_blend);

/** Single-pass terrain.pso equivalent (SPIR-V terrain shader). */
void Terrain_Push_Spirv_Draw_State();

/** Set the matrix used to project world positions into shroud UV space. */
void Set_Shroud_Transform(const float matrix[16]);

/** Terrain cloud/noise scroll + enable flags for SPIR-V single-pass terrain. */
void Set_Terrain_Cloud_Params(float scroll_x, float scroll_y, float stretch, uint32_t flags);
void Set_Terrain_Cloud_Textures(::TextureClass *cloud, ::TextureClass *noise);
void Rebind_Terrain_Cloud_Textures();
void Clear_Terrain_Cloud_Params();

/** Extra per-draw UBO flags (e.g. road lightmap alpha-mask pass). */
void Set_Extra_Draw_Flags(uint32_t flags);
void Clear_Extra_Draw_Flags();

/**
 * World-XY cloud/noise projection for mesh stages (roads, bridges).
 * Equivalent to D3D CAMERASPACEPOSITION * inv(view) * stretch [+ scroll],
 * without baking ViewInv into the extracted scale (which breaks when the
 * camera is rotated).
 */
void Set_World_XY_Tex_Projection(
	unsigned stage,
	float stretch,
	float offset_u,
	float offset_v);

/**
 * World-XY projection with independent U/V scale (shroud cells may be non-square).
 * address_mode: D3DTADDRESS_WRAP or D3DTADDRESS_CLAMP.
 */
void Set_World_XY_Tex_Projection_UV(
	unsigned stage,
	float scale_u,
	float scale_v,
	float offset_u,
	float offset_v,
	unsigned address_mode);

} /* namespace ww3d_vulkan */

#endif /* RENEGADE_VULKAN */

#endif
