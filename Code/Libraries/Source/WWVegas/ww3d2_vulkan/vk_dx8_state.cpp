#include "vk_dx8_state.h"

#if defined(RENEGADE_VULKAN)

#include "vk_native_render_state.h"
#include "ww3d_vulkan.h"

namespace ww3d_vulkan {

void Fill_Vulkan_Draw_State(VulkanDrawState *state);

void Sync_Draw_State()
{
	if (!WW3DVulkan::Get().Is_Active()) {
		return;
	}

	VulkanDrawState draw_state = {};
	Fill_Vulkan_Draw_State(&draw_state);

	/*
	 * Do not Clear_Native_Texture_State() here — terrain setShader() pushes
	 * once per pass but draws many tiles; clearing after the first tile
	 * reverts to detailOpaqueShader DETAILCOLOR_SCALE and breaks blending.
	 * Cleared explicitly in TerrainShader2Stage::reset() and UI Prepare_Draw.
	 */

	FrameUBO ubo = {};
	memcpy(ubo.view, draw_state.view, sizeof(ubo.view));
	memcpy(ubo.material_ambient, draw_state.material_ambient, sizeof(ubo.material_ambient));
	memcpy(ubo.material_diffuse, draw_state.material_diffuse, sizeof(ubo.material_diffuse));
	memcpy(ubo.material_emissive, draw_state.material_emissive, sizeof(ubo.material_emissive));
	memcpy(ubo.material_specular, draw_state.material_specular, sizeof(ubo.material_specular));
	memcpy(ubo.scene_ambient, draw_state.scene_ambient, sizeof(ubo.scene_ambient));
	memcpy(ubo.fog_color, draw_state.fog_color, sizeof(ubo.fog_color));
	memcpy(ubo.light_dir_or_pos, draw_state.light_dir_or_pos, sizeof(ubo.light_dir_or_pos));
	memcpy(ubo.light_diffuse, draw_state.light_diffuse, sizeof(ubo.light_diffuse));
	memcpy(ubo.light_params, draw_state.light_params, sizeof(ubo.light_params));
	ubo.fog_start = draw_state.fog_start;
	ubo.fog_end = draw_state.fog_end;
	ubo.fog_mode = draw_state.fog_mode;
	ubo.flags = draw_state.flags;
	ubo.tex_stage0_mode = draw_state.tex_stage0_mode;
	ubo.tex_stage1_color_mode = draw_state.tex_stage1_color_mode;
	ubo.tex_stage1_alpha_mode = draw_state.tex_stage1_alpha_mode;
	ubo.material_shininess = draw_state.material_shininess;
	ubo.specular_enable = draw_state.specular_enable;
	FrameUBO_Pack_Tex_Arrays(
		&ubo,
		draw_state.bump_mat,
		draw_state.bump_l_scale,
		draw_state.bump_l_offset,
		draw_state.tex_tci,
		draw_state.tex_uv_index,
		draw_state.tex_mat);
	memcpy(ubo.shroud_transform, draw_state.shroud_transform, sizeof(ubo.shroud_transform));
	memcpy(ubo.terrain_cloud_params, draw_state.terrain_cloud_params, sizeof(ubo.terrain_cloud_params));

	WW3DVulkan::Get().Set_View_Matrix(draw_state.view);
	WW3DVulkan::Get().Set_Lighting_State(ubo);

#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)
	{
		const uint32_t flags = (uint32_t)ubo.flags;
		if ((flags & (uint32_t)FrameUBO::FLAG_LIGHTING) != 0 &&
				(flags & (uint32_t)FrameUBO::FLAG_SHADOW_DECAL) == 0 &&
				!Terrain_Spirv_Draw_Active())
		{
			static int mesh_light_log = 0;
			if (mesh_light_log < 32) {
				++mesh_light_log;
				int enabled = 0;
				int dir_light = 0;
				int diffuse_r = 0;
				int ambient_r = 0;
				for (int i = 0; i < 4; ++i) {
					if (ubo.light_params[i][0] >= 0.5f) {
						++enabled;
						if (ubo.light_params[i][0] < 1.5f) {
							dir_light = 1;
						}
					}
				}
				diffuse_r = (int)(ubo.light_diffuse[0][0] * 255.0f);
				ambient_r = (int)(ubo.scene_ambient[0] * 255.0f);
			}
		}
	}
#endif
}

} /* namespace ww3d_vulkan */

#endif
