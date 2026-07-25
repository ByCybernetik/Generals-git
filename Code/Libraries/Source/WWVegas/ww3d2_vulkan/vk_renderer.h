#ifndef WW3D2_VULKAN_VK_RENDERER_H
#define WW3D2_VULKAN_VK_RENDERER_H

#include "vk_buffer.h"
#include "vk_framebuffer.h"
#include "vk_pipeline.h"
#include "vk_platform.h"
#include "vk_render_pass.h"
#include "vk_swapchain.h"
#include "vk_texture.h"
#include "vk_2d_renderer.h"
#include <cstring>
#include <vector>

namespace ww3d_vulkan {

struct FrameUBO {
	enum {
		FLAG_LIGHTING = 1u << 0,
		FLAG_DIFFUSE_FROM_VERTEX = 1u << 1,
		FLAG_TEXTURING = 1u << 2,
		FLAG_COLOR1_UNLIT_MODULATE = 1u << 3,
		FLAG_FOG = 1u << 4,
		FLAG_SCREEN_BLEND_UNLIT = 1u << 5,
		FLAG_SCREEN_BLEND_LIT = 1u << 6,
		FLAG_SCREEN_BLEND_EVALOGO = 1u << 7,
		FLAG_SCREEN_BLEND_GIZMO_DIM = 1u << 8,
		/* D3DMCS_COLOR1 for ambient: scene_ambient * vertex_color. */
		FLAG_AMBIENT_FROM_VERTEX = 1u << 9,
		FLAG_PARTICLE_SIMPLE = 1u << 14,
		FLAG_SHADOW_DECAL = 1u << 15,
		FLAG_TERRAIN_CLOUD = 1u << 16,
		FLAG_TERRAIN_NOISE = 1u << 17,
		/* Road NOISE12 pass 1: Frame *= mix(1, lightmap, roadAlpha). */
		FLAG_ROAD_LIGHTMAP_MASK = 1u << 18,
		/* Road pass 0: force cloud modulate via terrain_cloud_params UV. */
		FLAG_ROAD_CLOUD_MOD = 1u << 19,
		/* Terrain FOW: alpha-blend shroud (A4) instead of classic RGB multiply. */
		FLAG_TERRAIN_FOW_SHROUD = 1u << 20,
		/* Mesh shroud pass (roads/bridges): UV from shroud_transform like terrain. */
		FLAG_MESH_SHROUD = 1u << 21,
		/* Road shroud: multiply factor masked by road alpha (avoid FOW edge outlines). */
		FLAG_MESH_SHROUD_ROAD_MASK = 1u << 22,
		/* Road base pass: apply shroud in-shader (avoids multiply stacking at joins). */
		FLAG_MESH_INLINE_SHROUD = 1u << 23,
	};

	enum {
		FOG_MODE_BLEND = 0,
		FOG_MODE_SCALE = 1,
		FOG_MODE_WHITE = 2,
	};

	enum {
		LIGHT_OFF = 0,
		LIGHT_DIRECTIONAL = 1,
		LIGHT_POINT = 2,
	};

	float view_proj[16];
	float world[16];
	float view[16];
	float material_ambient[4];
	float material_diffuse[4];
	float material_emissive[4];
	float material_specular[4];
	float scene_ambient[4];
	float fog_color[4];
	float light_dir_or_pos[4][4];
	float light_diffuse[4][4];
	/* [0]=type; directional yzw=specular rgb; point yzw=Att0/Att1/Att2 */
	float light_params[4][4];
	float fog_start;
	float fog_end;
	float fog_mode;
	float flags;
	float tex_stage0_mode;
	float tex_stage1_color_mode;
	float tex_stage1_alpha_mode;
	float material_shininess;
	float specular_enable;
	float _pad_after_specular[3];
	/* std140: each float in an array occupies a 16-byte slot */
	struct { float v; float _pad[3]; } bump_mat[4];
	float bump_l_scale;
	float bump_l_offset;
	float _pad_before_tex_tci[2];
	struct { float v; float _pad[3]; } tex_tci[2];
	struct { float v; float _pad[3]; } tex_uv_index[2];
	float tex_mat[2][4];
	float shroud_transform[4][4];
	/* Terrain cloud/noise: xy = scroll offset, z = world-XY stretch, w unused. */
	float terrain_cloud_params[4];
};

inline void FrameUBO_Pack_Tex_Arrays(
	FrameUBO *ubo,
	const float bump_mat[4],
	float bump_l_scale,
	float bump_l_offset,
	const float tex_tci[2],
	const float tex_uv_index[2],
	const float tex_mat[2][4])
{
	if (ubo == nullptr) {
		return;
	}
	for (int i = 0; i < 4; ++i) {
		ubo->bump_mat[i].v = bump_mat[i];
		ubo->bump_mat[i]._pad[0] = 0.0f;
		ubo->bump_mat[i]._pad[1] = 0.0f;
		ubo->bump_mat[i]._pad[2] = 0.0f;
	}
	ubo->bump_l_scale = bump_l_scale;
	ubo->bump_l_offset = bump_l_offset;
	for (int i = 0; i < 2; ++i) {
		ubo->tex_tci[i].v = tex_tci[i];
		ubo->tex_tci[i]._pad[0] = 0.0f;
		ubo->tex_tci[i]._pad[1] = 0.0f;
		ubo->tex_tci[i]._pad[2] = 0.0f;
		ubo->tex_uv_index[i].v = tex_uv_index[i];
		ubo->tex_uv_index[i]._pad[0] = 0.0f;
		ubo->tex_uv_index[i]._pad[1] = 0.0f;
		ubo->tex_uv_index[i]._pad[2] = 0.0f;
	}
	memcpy(ubo->tex_mat, tex_mat, sizeof(ubo->tex_mat));
}

class VkRenderer {
public:
	bool Init(SDL_Window *window, uint32_t width, uint32_t height, bool vsync);
	void Shutdown();

	void Resize(uint32_t width, uint32_t height);
	bool Begin_Frame(float clear_r, float clear_g, float clear_b, float clear_a);
	bool End_Frame(bool present);
	void Clear_During_Frame(bool clear_color, bool clear_depth, float r, float g, float b, float a);

	void Set_View_Projection(const float matrix[16]);
	void Set_World_Matrix(const float matrix[16]);
	void Set_View_Matrix(const float matrix[16]);
	void Set_Lighting_State(const FrameUBO &state);
	void Bind_Texture(unsigned stage, VkTexture *texture);
	void Set_Render_Target(VkTexture *target);
	void Set_Viewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

	VkRenderPass Offscreen_Render_Pass() const { return render_pass_.Offscreen_Handle(); }
	VkFormat Depth_Format() const { return render_pass_.Depth_Format(); }

	void Draw_Indexed(
		VkBuffer vertex_buffer,
		VkBuffer index_buffer,
		uint32_t index_count,
		uint32_t first_index,
		uint32_t vertex_offset,
		const MeshPipelineKey &key);

	void Flush_Pending_Draws(bool flush_coalesced_2d = true);
	size_t Pending_Draw_Count() const { return pending_draws_.size(); }

	VkExtent2D Extent() const { return swapchain_.Extent(); }
	VkPipelineLayout Pipeline_Layout() const { return pipelines_.Layout(); }
	VkDescriptorSetLayout Descriptor_Set_Layout() const { return pipelines_.Descriptor_Set_Layout(); }
	uint32_t Current_Frame() const { return current_frame_; }

	VkCommandBuffer Current_Command_Buffer() const { return frames_[current_frame_].command_buffer; }
	VkRenderPass Main_Render_Pass() const { return render_pass_.Handle(); }
	PFN_vkCmdPushDescriptorSetKHR Push_Descriptor_Func() const { return push_descriptor_set_; }
	VkTexture *Default_Texture() { return &default_texture_; }
	VkTexture *Offscreen_Target() const { return offscreen_target_; }
	bool Frame_Active() const { return frame_active_; }
	void Invalidate_Bound_Pipeline() { bound_pipeline_ = VK_NULL_HANDLE; }

	/*
	 * Defer destruction of a VkSampler that is currently (or may still be)
	 * referenced by a command buffer in flight via a pushed descriptor.  The
	 * sampler is placed in a per-frame retire list and destroyed in
	 * Begin_Frame after vkWaitForFences guarantees the frame's command buffer
	 * is idle.  Destroying a sampler that is still bound to a recording or
	 * in-flight command buffer invalidates the buffer (validation reports
	 * VUID-*-commandBuffer-recording), which in turn breaks every subsequent
	 * draw recorded this frame — including the shadow-volume passes.
	 */
	void Retire_Sampler(VkSampler sampler);
	/*
	 * Defer destruction of a VkTexture still referenced by in-flight
	 * command buffers. Destroyed in Begin_Frame after the frame fence.
	 * Immediate Destroy during Free_Assets / menu reset hangs the GPU.
	 */
	void Retire_Texture(VkTexture *texture);

	void Draw_Batch(
		VkCommandBuffer cmd,
		const Simple2DVertex *vertices,
		uint32_t vertex_count,
		const uint16_t *indices,
		uint32_t index_count,
		VkTexture *texture,
		bool texturing,
		uint8_t src_blend,
		uint8_t dst_blend,
		const float modulate_color[4],
		bool grayscale,
		uint32_t viewport_x,
		uint32_t viewport_y,
		uint32_t viewport_w,
		uint32_t viewport_h,
		bool stencil_test = false,
		bool depth_test = false,
		uint8_t stencil_func = 1,
		uint8_t stencil_ref = 0,
		uint32_t stencil_mask = 0xFFFFFFFFu,
		uint32_t stencil_write_mask = 0xFFFFFFFFu,
		uint8_t stencil_fail = 1,
		uint8_t stencil_zfail = 1,
		uint8_t stencil_pass = 1)
	{
		twod_renderer_.Draw_Batch(
			cmd,
			vertices,
			vertex_count,
			indices,
			index_count,
			texture,
			texturing,
			src_blend,
			dst_blend,
			modulate_color,
			grayscale,
			viewport_x,
			viewport_y,
			viewport_w,
			viewport_h,
			stencil_test,
			depth_test,
			stencil_func,
			stencil_ref,
			stencil_mask,
			stencil_write_mask,
			stencil_fail,
			stencil_zfail,
			stencil_pass);
	}

private:
	struct PendingDraw {
		MeshPipelineKey key;
		VkBuffer vertex_buffer;
		VkBuffer index_buffer;
		uint32_t index_count;
		uint32_t first_index;
		uint32_t vertex_offset;
		int depth_bias;
		uint32_t submit_order = 0;
		FrameUBO ubo;
		VkTexture *textures[4];
	};

	bool Create_Frame_Resources();
	bool Create_Sync_Objects();
	bool Load_Shaders();
	bool Create_Default_Texture();

	void Flush_Push_Descriptors(VkCommandBuffer cmd, VkPipelineLayout layout, VkDeviceSize ubo_offset);
	void Apply_Viewport(VkCommandBuffer cmd, VkExtent2D extent);
	void Recreate_Swapchain_Resources();

	VkSwapchain swapchain_;
	VkRenderPassMgr render_pass_;
	VkFramebufferMgr framebuffers_;
	VkPipelineCache pipelines_;
	VkPipelineCache pipelines_offscreen_;
	FrameSync frames_[kMaxFramesInFlight];
	VkBufferAlloc frame_ubo_ring_[kMaxFramesInFlight];
	uint32_t frame_ubo_draw_count_ = 0;
	uint32_t ubo_alignment_ = 256;

	std::vector<uint32_t> vert_spirv_;
	std::vector<uint32_t> frag_spirv_;
	std::vector<uint32_t> terrain_vert_spirv_;
	std::vector<uint32_t> terrain_frag_spirv_;

	uint32_t current_frame_ = 0;
	uint32_t current_image_ = 0;
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	bool vsync_ = true;
	bool frame_active_ = false;

	FrameUBO frame_ubo_ = {};
	VkTexture default_texture_;
	VkTexture *bound_textures_[4] = {};
	bool textures_dirty_ = false;
	VkTexture *offscreen_target_ = nullptr;
	bool explicit_viewport_ = false;
	VkViewport viewport_state_ = {};
	VkRect2D scissor_state_ = {};
	VkPipeline bound_pipeline_ = VK_NULL_HANDLE;
	PFN_vkCmdPushDescriptorSetKHR push_descriptor_set_ = nullptr;

	/* 2D/UI direct draw path. */
	Vk2DRenderer twod_renderer_;

	/* Draw batching: queued per frame, sorted and flushed at frame end. */
	std::vector<PendingDraw> pending_draws_;
	uint32_t next_submit_order_ = 0;

	/*
	 * Per-frame retired samplers.  Each VkTexture::Update_Sampler call that
	 * replaces a sampler pushes the old handle here instead of destroying it
	 * inline; the list for the current frame is drained in Begin_Frame after
	 * the fence wait.
	 */
	std::vector<VkSampler> retired_samplers_[kMaxFramesInFlight];
	std::vector<VkTexture *> retired_textures_[kMaxFramesInFlight];
};

} /* namespace ww3d_vulkan */

#endif
