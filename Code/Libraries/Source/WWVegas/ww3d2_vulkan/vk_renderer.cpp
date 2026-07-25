#include "vk_renderer.h"
#include "vk_check.h"
#include "vk_context.h"
#include "vk_shader.h"
#include "vk_dx8_bridge.h"
#include "vk_dx8_texture.h"
#include "vk_native_render_state.h"
#include "../ww3d2/dx8wrapper.h"
#include "../ww3d2/dx8fvf.h"
#include "../ww3d2/shader.h"

#include <d3d8.h>
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ww3d_vulkan {

namespace {

static void Apply_Depth_Bias_For_ZBias(VkCommandBuffer cmd, int zbias)
{
	// Use a steeper constant factor and no slope bias.  Slope bias was
	// causing shadow-volume sides to be inconsistently offset on steep
	// polygons, producing stripes.
	const float constant = zbias > 0 ? (float)zbias * 8.0f : 0.0f;
	const float slope = 0.0f;
	vkCmdSetDepthBias(cmd, constant, 0.0f, slope);
}

/* Constant bias for stencil volumes: pull fragments slightly toward the
 * camera so front/back faces both pass LEQUAL on heightmap skimming.
 * Scale matches Apply_Depth_Bias_For_ZBias (zbias*8) — a factor of -1.5 is
 * far too weak on D24S8 and does not stop stripe residuals.
 * Slope stays 0 (asymmetric slope bias = stripes). */
static void Apply_Volume_Depth_Bias(VkCommandBuffer cmd)
{
	const char *e = getenv("GENERALS_SHADOW_VOLUME_BIAS");
	/* Mild pull toward camera; 0 disables. Avoid large values (≈ ALWAYS). */
	float constant = -4.0f;
	if (e != nullptr && e[0] != '\0') {
		constant = (float)atof(e);
	}
	vkCmdSetDepthBias(cmd, constant, 0.0f, 0.0f);
}

static void Apply_Dx8_Depth_Bias(VkCommandBuffer cmd)
{
	Apply_Depth_Bias_For_ZBias(cmd, DX8Wrapper::Get_Effective_ZBias());
}

static bool Is_Shadow_Decal_Pipeline_Key(const MeshPipelineKey &key)
{
	return key.fvf == DX8_FVF_XYZDUV1 &&
		key.src_blend == (uint8_t)ShaderClass::SRCBLEND_ZERO &&
		key.dst_blend == (uint8_t)ShaderClass::DSTBLEND_SRC_COLOR &&
		key.two_sided;
}

#if defined(RENEGADE_VULKAN)
static void Apply_Menu_Glow_Draw_State(MeshPipelineKey &key, FrameUBO &ubo)
{
	if (!DX8Wrapper::Vulkan_Menu_Glow_Draw_Active()) {
		return;
	}

	/*
	 * Original D3D8 (Code/ww3d2/render2dsentence.cpp Make_Additive):
	 * SRCBLEND_ONE + DSTBLEND_ONE + GRADIENT_MODULATE, no special shader flag.
	 * StyleMgrClass::Render_Glow blits font quads ~28x with dark vertex color.
	 */
	uint32_t flags = (uint32_t)ubo.flags;
	flags |= (uint32_t)FrameUBO::FLAG_TEXTURING |
		(uint32_t)FrameUBO::FLAG_DIFFUSE_FROM_VERTEX;
	ubo.flags = (float)flags;
	ubo.tex_stage0_mode = 1.0f;
	ubo.tex_stage1_color_mode = 0.0f;
	ubo.tex_stage1_alpha_mode = 0.0f;

	key.src_blend = (uint8_t)ShaderClass::SRCBLEND_ONE;
	key.dst_blend = (uint8_t)ShaderClass::DSTBLEND_ONE;
	key.alpha_blend = true;
}
#endif

static int Compare_Pipeline_Key(const MeshPipelineKey &a, const MeshPipelineKey &b)
{
	if (a.terrain_shader != b.terrain_shader) {
		/*
		 * SPIR-V base terrain first; noise/cloud mesh pass blends on top
		 * (DESTCOLOR). LAST order was for legacy 2-pass mesh blend, not Vulkan.
		 */
		return a.terrain_shader ? -1 : 1;
	}
	if (a.alpha_blend != b.alpha_blend) {
		return a.alpha_blend ? 1 : -1;
	}
	if (a.src_blend != b.src_blend) {
		return (int)a.src_blend - (int)b.src_blend;
	}
	if (a.dst_blend != b.dst_blend) {
		return (int)a.dst_blend - (int)b.dst_blend;
	}
	if (a.depth_write != b.depth_write) {
		return a.depth_write ? 1 : -1;
	}
	if (a.depth_test != b.depth_test) {
		return a.depth_test ? 1 : -1;
	}
	if (a.depth_compare != b.depth_compare) {
		return (int)a.depth_compare - (int)b.depth_compare;
	}
	if (a.two_sided != b.two_sided) {
		return a.two_sided ? 1 : -1;
	}
	if (a.cull_inverted != b.cull_inverted) {
		return a.cull_inverted ? 1 : -1;
	}
	if (a.stencil_test != b.stencil_test) {
		return a.stencil_test ? 1 : -1;
	}
	if (a.stencil_func != b.stencil_func) {
		return (int)a.stencil_func - (int)b.stencil_func;
	}
	if (a.stencil_ref != b.stencil_ref) {
		return (int)a.stencil_ref - (int)b.stencil_ref;
	}
	if (a.stencil_mask != b.stencil_mask) {
		return (int)a.stencil_mask - (int)b.stencil_mask;
	}
	if (a.stencil_write_mask != b.stencil_write_mask) {
		return (int)a.stencil_write_mask - (int)b.stencil_write_mask;
	}
	if (a.stencil_fail != b.stencil_fail) {
		return (int)a.stencil_fail - (int)b.stencil_fail;
	}
	if (a.stencil_zfail != b.stencil_zfail) {
		return (int)a.stencil_zfail - (int)b.stencil_zfail;
	}
	if (a.stencil_pass != b.stencil_pass) {
		return (int)a.stencil_pass - (int)b.stencil_pass;
	}
	if (a.color_write_mask != b.color_write_mask) {
		return (int)a.color_write_mask - (int)b.color_write_mask;
	}
	if (a.alpha_test != b.alpha_test) {
		return a.alpha_test ? 1 : -1;
	}
	if (a.topology != b.topology) {
		return (int)a.topology - (int)b.topology;
	}
	if (a.fvf != b.fvf) {
		return (int)a.fvf - (int)b.fvf;
	}
	if (a.vertex_stride != b.vertex_stride) {
		return (int)a.vertex_stride - (int)b.vertex_stride;
	}
	return 0;
}

static VkTexture *Resolve_Draw_Texture(VkTexture *texture, VkTexture *fallback)
{
	if (texture == nullptr ||
		texture->View() == VK_NULL_HANDLE ||
		texture->Sampler() == VK_NULL_HANDLE) {
		return fallback;
	}
	return texture;
}
} /* namespace */

static_assert(offsetof(FrameUBO, tex_mat) == 672, "FrameUBO tex_mat std140 alignment");
static_assert(offsetof(FrameUBO, shroud_transform) == 704, "FrameUBO shroud_transform std140 alignment");
static_assert(offsetof(FrameUBO, terrain_cloud_params) == 768, "FrameUBO terrain_cloud_params std140 alignment");
static_assert(sizeof(FrameUBO) == 784, "FrameUBO size must match GLSL std140 layout");

static uint32_t Align_Ubo_Size(uint32_t size, uint32_t alignment)
{
	return (size + alignment - 1) & ~(alignment - 1);
}

bool VkRenderer::Init(SDL_Window *window, uint32_t width, uint32_t height, bool vsync)
{
	width_ = width;
	height_ = height;
	vsync_ = vsync;

	if (!VkContext::Get().Init(window, true)) {
		return false;
	}

	VkContext &ctx = VkContext::Get();
	push_descriptor_set_ = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(
		vkGetDeviceProcAddr(ctx.Device(), "vkCmdPushDescriptorSetKHR"));
	if (push_descriptor_set_ == nullptr) {
		fprintf(stderr, "VkRenderer: VK_KHR_push_descriptor not supported by device.\n");
		Shutdown();
		return false;
	}

	if (!Load_Shaders()) {
		Shutdown();
		return false;
	}
	if (!swapchain_.Create(width_, height_, vsync_)) {
		Shutdown();
		return false;
	}
	if (!render_pass_.Create(swapchain_.Image_Format())) {
		Shutdown();
		return false;
	}
	if (!render_pass_.Create_Offscreen(swapchain_.Image_Format())) {
		Shutdown();
		return false;
	}

	std::vector<VkImageView> views;
	for (uint32_t i = 0; i < swapchain_.Image_Count(); ++i) {
		views.push_back(swapchain_.Image_View(i));
	}
	if (!framebuffers_.Create(
			render_pass_.Handle(),
			views,
			render_pass_.Depth_Format(),
			swapchain_.Extent())) {
		Shutdown();
		return false;
	}
	if (!pipelines_.Create(
			render_pass_.Handle(),
			vert_spirv_,
			frag_spirv_,
			terrain_vert_spirv_,
			terrain_frag_spirv_)) {
		Shutdown();
		return false;
	}
	if (!pipelines_offscreen_.Create(
			render_pass_.Offscreen_Handle(),
			vert_spirv_,
			frag_spirv_,
			terrain_vert_spirv_,
			terrain_frag_spirv_)) {
		Shutdown();
		return false;
	}
	if (!Create_Frame_Resources()) {
		Shutdown();
		return false;
	}
	if (!Create_Sync_Objects()) {
		Shutdown();
		return false;
	}
	if (!Create_Default_Texture()) {
		Shutdown();
		return false;
	}
	if (!twod_renderer_.Init(this)) {
		Shutdown();
		return false;
	}

	bound_textures_[0] = &default_texture_;
	bound_textures_[1] = &default_texture_;
	bound_textures_[2] = &default_texture_;
	bound_textures_[3] = &default_texture_;

	memset(&frame_ubo_, 0, sizeof(frame_ubo_));
	frame_ubo_.tex_mat[0][0] = 1.0f;
	frame_ubo_.tex_mat[0][1] = 1.0f;
	frame_ubo_.tex_mat[1][0] = 1.0f;
	frame_ubo_.tex_mat[1][1] = 1.0f;
	for (int i = 0; i < 4; ++i) {
		frame_ubo_.shroud_transform[i][i] = 1.0f;
	}
	frame_ubo_.terrain_cloud_params[2] = 1.0f / 315.0f;
	bound_textures_[0] = &default_texture_;
	bound_textures_[1] = &default_texture_;
	bound_textures_[2] = &default_texture_;
	bound_textures_[3] = &default_texture_;
	textures_dirty_ = true;

	return true;
}

bool VkRenderer::Create_Default_Texture()
{
	return default_texture_.Create_Solid(255, 255, 255, 255);
}

static VkImageLayout Sample_Descriptor_Layout(const VkTexture *texture)
{
	if (texture == nullptr) {
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	const VkImageLayout layout = texture->Layout();
	if (layout == VK_IMAGE_LAYOUT_GENERAL) {
		return VK_IMAGE_LAYOUT_GENERAL;
	}
	if (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	/* Optimal-tiled uploads transition to SHADER_READ_ONLY on GPU; layout_ may be stale. */
	return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void VkRenderer::Bind_Texture(unsigned stage, VkTexture *texture)
{
	if (stage >= 4) {
		return;
	}
	if (texture == nullptr ||
		texture->View() == VK_NULL_HANDLE ||
		texture->Sampler() == VK_NULL_HANDLE) {
		texture = &default_texture_;
	}
	bound_textures_[stage] = texture;
	textures_dirty_ = true;
}

void VkRenderer::Flush_Push_Descriptors(VkCommandBuffer cmd, VkPipelineLayout layout, VkDeviceSize ubo_offset)
{
	const uint32_t aligned_ubo_size =
		Align_Ubo_Size(sizeof(FrameUBO), ubo_alignment_);
	VkDescriptorBufferInfo ubo_info = {};
	ubo_info.buffer = frame_ubo_ring_[current_frame_].Handle();
	ubo_info.offset = ubo_offset;
	ubo_info.range = aligned_ubo_size;

	VkWriteDescriptorSet writes[5] = {};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstBinding = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[0].descriptorCount = 1;
	writes[0].pBufferInfo = &ubo_info;

	VkDescriptorImageInfo image_infos[4] = {};
	unsigned write_count = 1;
	for (unsigned stage = 0; stage < 4; ++stage) {
		VkTexture *tex = bound_textures_[stage];
		if (tex == nullptr || tex->View() == VK_NULL_HANDLE || tex->Sampler() == VK_NULL_HANDLE) {
			tex = &default_texture_;
		}
		image_infos[stage].imageLayout = Sample_Descriptor_Layout(tex);
		image_infos[stage].imageView = tex->View();
		image_infos[stage].sampler = tex->Sampler();

		writes[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[write_count].dstBinding = stage + 1;
		writes[write_count].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[write_count].descriptorCount = 1;
		writes[write_count].pImageInfo = &image_infos[stage];
		++write_count;
	}

	if (push_descriptor_set_ != nullptr) {
		push_descriptor_set_(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			layout,
			0,   // set 0 — all push descriptors
			write_count,
			writes);
	}

	textures_dirty_ = false;
}

void VkRenderer::Flush_Pending_Draws(bool flush_coalesced_2d)
{
	if (flush_coalesced_2d && frame_active_) {
		twod_renderer_.Flush_Coalesced_Draws(frames_[current_frame_].command_buffer);
	}

	if (pending_draws_.empty()) {
		return;
	}

#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)
	{
		int shadow_count = 0;
		int opaque_count = 0;
		for (const PendingDraw &pd : pending_draws_) {
			if (Is_Shadow_Decal_Pipeline_Key(pd.key)) {
				++shadow_count;
			} else if (!pd.key.alpha_blend) {
				++opaque_count;
			}
		}
		if (shadow_count > 0) {
			static int shadow_flush_order_log = 0;
			if (shadow_flush_order_log < 32) {
				++shadow_flush_order_log;
				fprintf(stderr,
					"Flush_Pending_Draws: shadow_decal_count=%d opaque_count=%d pending=%zu\n",
					shadow_count, opaque_count, pending_draws_.size());
			}
		}
	}
#endif

	std::sort(
		pending_draws_.begin(),
		pending_draws_.end(),
		[](const PendingDraw &a, const PendingDraw &b) {
			const int key_cmp = Compare_Pipeline_Key(a.key, b.key);
			if (key_cmp != 0) {
				return key_cmp < 0;
			}
			if (a.submit_order != b.submit_order) {
				return a.submit_order < b.submit_order;
			}
			if (a.vertex_buffer != b.vertex_buffer) {
				return a.vertex_buffer < b.vertex_buffer;
			}
			if (a.index_buffer != b.index_buffer) {
				return a.index_buffer < b.index_buffer;
			}
			if (a.textures[0] != b.textures[0]) {
				return a.textures[0] < b.textures[0];
			}
			if (a.textures[1] != b.textures[1]) {
				return a.textures[1] < b.textures[1];
			}
			if (a.textures[2] != b.textures[2]) {
				return a.textures[2] < b.textures[2];
			}
			if (a.textures[3] != b.textures[3]) {
				return a.textures[3] < b.textures[3];
			}
			return a.depth_bias < b.depth_bias;
		});

	FrameSync &frame = frames_[current_frame_];
	VkCommandBuffer cmd = frame.command_buffer;
	VkExtent2D extent = offscreen_target_ != nullptr
		? offscreen_target_->Render_Extent()
		: swapchain_.Extent();
	Apply_Viewport(cmd, extent);

	VkPipelineCache &pipe_cache =
		offscreen_target_ != nullptr ? pipelines_offscreen_ : pipelines_;
	const VkPipelineLayout pipeline_layout = pipe_cache.Layout();
	const uint32_t aligned_ubo_size = Align_Ubo_Size(sizeof(FrameUBO), ubo_alignment_);

	VkPipeline bound_pipe = VK_NULL_HANDLE;
	VkBuffer bound_vb = VK_NULL_HANDLE;
	VkBuffer bound_ib = VK_NULL_HANDLE;
	int bound_zbias = 0;
	bool have_zbias = false;

	for (size_t i = 0; i < pending_draws_.size(); ++i) {
		const PendingDraw &d = pending_draws_[i];
		if (frame_ubo_draw_count_ >= kUboDrawsPerFrame) {
			static int ubo_cap_warn = 0;
			if (ubo_cap_warn < 4) {
				++ubo_cap_warn;
				fprintf(stderr, "Vulkan UBO draw cap exceeded (%u draws)\n", kUboDrawsPerFrame);
			}
			break;
		}

		VkPipeline pipeline = pipe_cache.Get(d.key);
		if (pipeline == VK_NULL_HANDLE) {
			continue;
		}

		if (pipeline != bound_pipe) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			bound_pipe = pipeline;
			bound_pipeline_ = pipeline;
		}

		if (!have_zbias || d.depth_bias != bound_zbias) {
			Apply_Depth_Bias_For_ZBias(cmd, d.depth_bias);
			bound_zbias = d.depth_bias;
			have_zbias = true;
		}

		VkTexture *tex0 = d.textures[0];
		VkTexture *tex1 = d.textures[1];
		VkTexture *tex2 = d.textures[2];
		VkTexture *tex3 = d.textures[3];

		const VkDeviceSize ubo_offset =
			(VkDeviceSize)frame_ubo_draw_count_ * aligned_ubo_size;
		frame_ubo_ring_[current_frame_].Upload(&d.ubo, sizeof(d.ubo), ubo_offset);
		++frame_ubo_draw_count_;

		frame_ubo_ = d.ubo;
		bound_textures_[0] = tex0;
		bound_textures_[1] = tex1;
		bound_textures_[2] = tex2;
		bound_textures_[3] = tex3;
		textures_dirty_ = true;
		Flush_Push_Descriptors(cmd, pipeline_layout, ubo_offset);

		if (d.vertex_buffer != bound_vb) {
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &d.vertex_buffer, &offset);
			bound_vb = d.vertex_buffer;
		}
		if (d.index_buffer != bound_ib) {
			vkCmdBindIndexBuffer(cmd, d.index_buffer, 0, VK_INDEX_TYPE_UINT16);
			bound_ib = d.index_buffer;
		}

		vkCmdDrawIndexed(cmd, d.index_count, 1, d.first_index, d.vertex_offset, 0);
	}

	pending_draws_.clear();
}

void VkRenderer::Recreate_Swapchain_Resources()
{
	swapchain_.Recreate(width_, height_, vsync_);

	std::vector<VkImageView> views;
	for (uint32_t i = 0; i < swapchain_.Image_Count(); ++i) {
		views.push_back(swapchain_.Image_View(i));
	}
	framebuffers_.Destroy();
	framebuffers_.Create(
		render_pass_.Handle(),
		views,
		render_pass_.Depth_Format(),
		swapchain_.Extent());
	explicit_viewport_ = false;
}

void VkRenderer::Set_Render_Target(VkTexture *target)
{
	Flush_Pending_Draws();

	if (offscreen_target_ != nullptr && target == nullptr) {
		offscreen_target_ = nullptr;
		VkExtent2D extent = swapchain_.Extent();
		if (extent.width != width_ || extent.height != height_) {
			Recreate_Swapchain_Resources();
		}
		return;
	}
	offscreen_target_ = target;
}

void VkRenderer::Set_Viewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	Flush_Pending_Draws();
	explicit_viewport_ = true;
	viewport_state_.x = (float)x;
	viewport_state_.y = (float)y;
	viewport_state_.width = (float)w;
	viewport_state_.height = (float)h;
	viewport_state_.minDepth = 0.0f;
	viewport_state_.maxDepth = 1.0f;
	scissor_state_.offset.x = (int32_t)x;
	scissor_state_.offset.y = (int32_t)y;
	scissor_state_.extent.width = w;
	scissor_state_.extent.height = h;
}

void VkRenderer::Shutdown()
{
	VkContext &ctx = VkContext::Get();
	if (ctx.Device() != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(ctx.Device());
	}

	pending_draws_.clear();
	twod_renderer_.Shutdown();

	for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
		for (size_t t = 0; t < retired_textures_[i].size(); ++t) {
			VkTexture *tex = retired_textures_[i][t];
			if (tex != nullptr) {
				tex->Destroy();
				delete tex;
			}
		}
		retired_textures_[i].clear();
		for (size_t s = 0; s < retired_samplers_[i].size(); ++s) {
			if (retired_samplers_[i][s] != VK_NULL_HANDLE &&
				ctx.Device() != VK_NULL_HANDLE) {
				vkDestroySampler(ctx.Device(), retired_samplers_[i][s], nullptr);
			}
		}
		retired_samplers_[i].clear();
	}

	for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
		frame_ubo_ring_[i].Destroy();
		if (frames_[i].in_flight != VK_NULL_HANDLE) {
			vkDestroyFence(ctx.Device(), frames_[i].in_flight, nullptr);
		}
		if (frames_[i].image_available != VK_NULL_HANDLE) {
			vkDestroySemaphore(ctx.Device(), frames_[i].image_available, nullptr);
		}
		if (frames_[i].render_finished != VK_NULL_HANDLE) {
			vkDestroySemaphore(ctx.Device(), frames_[i].render_finished, nullptr);
		}
		frames_[i] = FrameSync{};
	}

	default_texture_.Destroy();
	pipelines_offscreen_.Destroy();
	pipelines_.Destroy();
	framebuffers_.Destroy();
	render_pass_.Destroy();
	swapchain_.Destroy();
	vert_spirv_.clear();
	frag_spirv_.clear();
	terrain_vert_spirv_.clear();
	terrain_frag_spirv_.clear();
	VkContext::Get().Shutdown();
	frame_active_ = false;
}

void VkRenderer::Resize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0) {
		return;
	}
	if (width_ == width && height_ == height) {
		return;
	}
	width_ = width;
	height_ = height;
	if (offscreen_target_ != nullptr) {
		return;
	}
	if (frame_active_) {
		frame_active_ = false;
	}
	Recreate_Swapchain_Resources();
}

bool VkRenderer::Load_Shaders()
{
	return Load_Spirv_From_Search_Path("mesh_textured.vert.spv", &vert_spirv_) &&
		Load_Spirv_From_Search_Path("mesh_textured.frag.spv", &frag_spirv_) &&
		Load_Spirv_From_Search_Path("terrain.vert.spv", &terrain_vert_spirv_) &&
		Load_Spirv_From_Search_Path("terrain.frag.spv", &terrain_frag_spirv_);
}

bool VkRenderer::Create_Frame_Resources()
{
	ubo_alignment_ = (uint32_t)VkContext::Get().Device_Properties().limits.minUniformBufferOffsetAlignment;
	if (ubo_alignment_ < 1) {
		ubo_alignment_ = 1;
	}
	const uint32_t aligned_ubo_size = Align_Ubo_Size(sizeof(FrameUBO), ubo_alignment_);
	for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
		if (!frame_ubo_ring_[i].Create(
				(VkDeviceSize)aligned_ubo_size * kUboDrawsPerFrame,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VMA_MEMORY_USAGE_CPU_TO_GPU)) {
			return false;
		}
	}
	return true;
}

bool VkRenderer::Create_Sync_Objects()
{
	VkContext &ctx = VkContext::Get();

	for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
		VkSemaphoreCreateInfo semaphore_info = {};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VK_CHECK(vkCreateSemaphore(
			ctx.Device(), &semaphore_info, nullptr, &frames_[i].image_available));
		VK_CHECK(vkCreateSemaphore(
			ctx.Device(), &semaphore_info, nullptr, &frames_[i].render_finished));

		VkFenceCreateInfo fence_info = {};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		VK_CHECK(vkCreateFence(ctx.Device(), &fence_info, nullptr, &frames_[i].in_flight));

		VkCommandBufferAllocateInfo cmd_alloc = {};
		cmd_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_alloc.commandPool = ctx.Command_Pool();
		cmd_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_alloc.commandBufferCount = 1;
		VK_CHECK(vkAllocateCommandBuffers(
			ctx.Device(), &cmd_alloc, &frames_[i].command_buffer));
	}
	return true;
}

void VkRenderer::Apply_Viewport(VkCommandBuffer cmd, VkExtent2D extent)
{
	VkViewport viewport = {};
	VkRect2D scissor = {};
	if (explicit_viewport_) {
		viewport.x = viewport_state_.x;
		viewport.width = viewport_state_.width;
		viewport.minDepth = viewport_state_.minDepth;
		viewport.maxDepth = viewport_state_.maxDepth;
		viewport.y = viewport_state_.y + viewport_state_.height;
		viewport.height = -viewport_state_.height;
		scissor = scissor_state_;
	} else {
		viewport.x = 0.0f;
		viewport.width = (float)extent.width;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		viewport.y = (float)extent.height;
		viewport.height = -(float)extent.height;
		scissor.offset = {0, 0};
		scissor.extent = extent;
	}
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);
}

bool VkRenderer::Begin_Frame(float clear_r, float clear_g, float clear_b, float clear_a)
{
	VkContext &ctx = VkContext::Get();
	FrameSync &frame = frames_[current_frame_];

	vkWaitForFences(ctx.Device(), 1, &frame.in_flight, VK_TRUE, UINT64_MAX);

	/*
	 * The fence wait above guarantees every command buffer previously
	 * recorded for this frame slot has completed, so VkSampler handles
	 * retired during the last pass through this slot are no longer
	 * referenced by the GPU and can finally be destroyed.
	 */
	{
		std::vector<VkSampler> &retired = retired_samplers_[current_frame_];
		for (VkSampler s : retired) {
			if (s != VK_NULL_HANDLE) {
				vkDestroySampler(ctx.Device(), s, nullptr);
			}
		}
		retired.clear();
	}
	{
		std::vector<VkTexture *> &retired = retired_textures_[current_frame_];
		for (size_t i = 0; i < retired.size(); ++i) {
			VkTexture *tex = retired[i];
			if (tex != nullptr) {
				tex->Destroy();
				delete tex;
			}
		}
		retired.clear();
	}

	/*
	 * Acquire before ResetFences. If acquire fails (minimized / OUT_OF_DATE)
	 * after a fence reset with no submit, the next WaitForFences on this slot
	 * hangs forever.
	 */
	if (!offscreen_target_) {
		if (!swapchain_.Acquire_Next_Image(
				current_frame_, frame.image_available, &current_image_)) {
			Recreate_Swapchain_Resources();
			if (!swapchain_.Acquire_Next_Image(
					current_frame_, frame.image_available, &current_image_)) {
				return false;
			}
		}
	}

	vkResetFences(ctx.Device(), 1, &frame.in_flight);
	Reset_Dynamic_Vb_Frame_Slot(current_frame_);
	Reset_Dynamic_Ib_Frame_Slot(current_frame_);
	vkResetCommandBuffer(frame.command_buffer, 0);
	explicit_viewport_ = false;

	pending_draws_.clear();
	pending_draws_.reserve(512);
	next_submit_order_ = 0;
	bound_pipeline_ = VK_NULL_HANDLE;
	frame_ubo_draw_count_ = 0;
	twod_renderer_.Begin_Frame();

	bound_textures_[0] = &default_texture_;
	bound_textures_[1] = &default_texture_;
	bound_textures_[2] = &default_texture_;
	bound_textures_[3] = &default_texture_;
	textures_dirty_ = true;

	Native_Render_State_Reset();

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	VK_CHECK(vkBeginCommandBuffer(frame.command_buffer, &begin_info));

	VkClearValue clear_values[2] = {};
	clear_values[0].color.float32[0] = clear_r;
	clear_values[0].color.float32[1] = clear_g;
	clear_values[0].color.float32[2] = clear_b;
	clear_values[0].color.float32[3] = clear_a;
	clear_values[1].depthStencil.depth = 1.0f;
	clear_values[1].depthStencil.stencil = 0;

	VkExtent2D extent = swapchain_.Extent();
	VkRenderPassBeginInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.clearValueCount = 2;
	render_pass_info.pClearValues = clear_values;
	render_pass_info.renderArea.offset = {0, 0};

	if (offscreen_target_ != nullptr) {
		render_pass_info.renderPass = render_pass_.Offscreen_Handle();
		render_pass_info.framebuffer = offscreen_target_->Render_Framebuffer();
		extent = offscreen_target_->Render_Extent();
	} else {
		render_pass_info.renderPass = render_pass_.Handle();
		render_pass_info.framebuffer = framebuffers_.Framebuffer(current_image_);
	}
	render_pass_info.renderArea.extent = extent;

	vkCmdBeginRenderPass(
		frame.command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	Apply_Viewport(frame.command_buffer, extent);

	frame_active_ = true;
	return true;
}

void VkRenderer::Clear_During_Frame(
	bool clear_color,
	bool clear_depth,
	float r,
	float g,
	float b,
	float a)
{
	if (!frame_active_) {
		return;
	}

	Flush_Pending_Draws();

	FrameSync &frame = frames_[current_frame_];
	VkCommandBuffer cmd = frame.command_buffer;

	VkClearAttachment attachments[2] = {};
	uint32_t attachment_count = 0;

	if (clear_color) {
		attachments[attachment_count].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		attachments[attachment_count].colorAttachment = 0;
		attachments[attachment_count].clearValue.color.float32[0] = r;
		attachments[attachment_count].clearValue.color.float32[1] = g;
		attachments[attachment_count].clearValue.color.float32[2] = b;
		attachments[attachment_count].clearValue.color.float32[3] = a;
		attachment_count++;
	}
	if (clear_depth) {
		// Clear both depth and stencil aspects together.  The depth/stencil
		// image uses a packed D24S8 / D32S8 format; clearing only DEPTH leaves
		// the stencil aspect at whatever value the previous frame wrote, which
		// breaks the volumetric shadow pass (increment/decrement + darken quad).
		attachments[attachment_count].aspectMask =
			VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		attachments[attachment_count].clearValue.depthStencil.depth = 1.0f;
		attachments[attachment_count].clearValue.depthStencil.stencil = 0;
		attachment_count++;
	}
	if (attachment_count == 0) {
		return;
	}

	VkExtent2D extent = offscreen_target_ != nullptr
		? offscreen_target_->Render_Extent()
		: swapchain_.Extent();
	VkClearRect clear_rect = {};
	clear_rect.rect.offset = {0, 0};
	clear_rect.rect.extent = extent;
	clear_rect.baseArrayLayer = 0;
	clear_rect.layerCount = 1;

	vkCmdClearAttachments(cmd, attachment_count, attachments, 1, &clear_rect);
}

bool VkRenderer::End_Frame(bool present)
{
	if (!frame_active_) {
		return false;
	}

	Flush_Pending_Draws();

	VkContext &ctx = VkContext::Get();
	FrameSync &frame = frames_[current_frame_];

	vkCmdEndRenderPass(frame.command_buffer);
	VK_CHECK(vkEndCommandBuffer(frame.command_buffer));

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &frame.command_buffer;

	if (offscreen_target_ != nullptr) {
		VK_CHECK(vkQueueSubmit(ctx.Graphics_Queue(), 1, &submit_info, frame.in_flight));
		VK_CHECK(vkWaitForFences(ctx.Device(), 1, &frame.in_flight, VK_TRUE, UINT64_MAX));
	} else if (present) {
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &frame.image_available;
		submit_info.pWaitDstStageMask = &wait_stage;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &frame.render_finished;

		VK_CHECK(vkQueueSubmit(ctx.Graphics_Queue(), 1, &submit_info, frame.in_flight));

		if (!swapchain_.Present(current_frame_, frame.render_finished, current_image_)) {
			Resize(width_, height_);
		}

		current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
	} else {
		VK_CHECK(vkQueueSubmit(ctx.Graphics_Queue(), 1, &submit_info, frame.in_flight));
		VK_CHECK(vkWaitForFences(ctx.Device(), 1, &frame.in_flight, VK_TRUE, UINT64_MAX));
	}

	frame_active_ = false;

	return true;
}

void VkRenderer::Set_View_Projection(const float matrix[16])
{
	memcpy(frame_ubo_.view_proj, matrix, sizeof(frame_ubo_.view_proj));
}

void VkRenderer::Set_World_Matrix(const float matrix[16])
{
	memcpy(frame_ubo_.world, matrix, sizeof(frame_ubo_.world));
}

void VkRenderer::Set_View_Matrix(const float matrix[16])
{
	memcpy(frame_ubo_.view, matrix, sizeof(frame_ubo_.view));
}

void VkRenderer::Set_Lighting_State(const FrameUBO &state)
{
	memcpy(frame_ubo_.view, state.view, sizeof(frame_ubo_.view));
	memcpy(frame_ubo_.material_ambient, state.material_ambient, sizeof(state.material_ambient));
	memcpy(frame_ubo_.material_diffuse, state.material_diffuse, sizeof(state.material_diffuse));
	memcpy(frame_ubo_.material_emissive, state.material_emissive, sizeof(state.material_emissive));
	memcpy(frame_ubo_.scene_ambient, state.scene_ambient, sizeof(state.scene_ambient));
	memcpy(frame_ubo_.fog_color, state.fog_color, sizeof(state.fog_color));
	memcpy(frame_ubo_.light_dir_or_pos, state.light_dir_or_pos, sizeof(state.light_dir_or_pos));
	memcpy(frame_ubo_.light_diffuse, state.light_diffuse, sizeof(state.light_diffuse));
	memcpy(frame_ubo_.light_params, state.light_params, sizeof(state.light_params));
	memcpy(frame_ubo_.material_specular, state.material_specular, sizeof(state.material_specular));
	frame_ubo_.fog_start = state.fog_start;
	frame_ubo_.fog_end = state.fog_end;
	frame_ubo_.fog_mode = state.fog_mode;
	frame_ubo_.flags = state.flags;
	frame_ubo_.tex_stage0_mode = state.tex_stage0_mode;
	frame_ubo_.tex_stage1_color_mode = state.tex_stage1_color_mode;
	frame_ubo_.tex_stage1_alpha_mode = state.tex_stage1_alpha_mode;
	frame_ubo_.material_shininess = state.material_shininess;
	frame_ubo_.specular_enable = state.specular_enable;
	memcpy(
		&frame_ubo_.bump_mat,
		&state.bump_mat,
		sizeof(FrameUBO) - offsetof(FrameUBO, bump_mat));
}

void VkRenderer::Draw_Indexed(
	VkBuffer vertex_buffer,
	VkBuffer index_buffer,
	uint32_t index_count,
	uint32_t first_index,
	uint32_t vertex_offset,
	const MeshPipelineKey &key)
{
	if (!frame_active_) {
		return;
	}

	if (index_count == 0) {
		return;
	}

	VkPipelineCache &pipe_cache =
		offscreen_target_ != nullptr ? pipelines_offscreen_ : pipelines_;

	MeshPipelineKey draw_key = key;
	FrameUBO draw_ubo = frame_ubo_;
#if defined(RENEGADE_VULKAN)
	Apply_Menu_Glow_Draw_State(draw_key, draw_ubo);
#endif

	const bool shadow_decal = Is_Shadow_Decal_Pipeline_Key(draw_key);
	if (shadow_decal) {
		uint32_t flags = (uint32_t)draw_ubo.flags;
		flags |= (uint32_t)FrameUBO::FLAG_SHADOW_DECAL;
		flags |= (uint32_t)FrameUBO::FLAG_TEXTURING;
		flags &= ~((uint32_t)FrameUBO::FLAG_LIGHTING |
			(uint32_t)FrameUBO::FLAG_FOG |
			(uint32_t)FrameUBO::FLAG_DIFFUSE_FROM_VERTEX);
		draw_ubo.flags = (float)flags;
		draw_ubo.tex_stage0_mode = 1.0f;
		draw_ubo.tex_stage1_color_mode = 0.0f;
		draw_ubo.tex_stage1_alpha_mode = 0.0f;
		draw_ubo.tex_tci[0].v = 0.0f;
		draw_ubo.tex_tci[1].v = 0.0f;
		draw_ubo.tex_uv_index[0].v = 0.0f;
		draw_ubo.tex_uv_index[1].v = 0.0f;
		draw_ubo.tex_mat[0][0] = 1.0f;
		draw_ubo.tex_mat[0][1] = 1.0f;
		draw_ubo.tex_mat[0][2] = 0.0f;
		draw_ubo.tex_mat[0][3] = 0.0f;
		draw_ubo.tex_mat[1][0] = 1.0f;
		draw_ubo.tex_mat[1][1] = 1.0f;
		draw_ubo.tex_mat[1][2] = 0.0f;
		draw_ubo.tex_mat[1][3] = 0.0f;
		for (int i = 0; i < 4; ++i) {
			draw_ubo.shroud_transform[i][i] = 1.0f;
		}
	}

	/*
	 * Particle workaround: use a simple texture*color path that bypasses
	 * Apply_Stage0 / fog / screen-blend / stage1, matching the stable
	 * debug-MODULATE output.
	 */
	const bool particle_simple =
		draw_key.alpha_blend &&
		draw_key.fvf == dynamic_fvf_type &&
		draw_key.two_sided &&
		!draw_key.depth_write;
	if (particle_simple) {
		uint32_t flags = (uint32_t)draw_ubo.flags;
		flags |= (uint32_t)FrameUBO::FLAG_PARTICLE_SIMPLE;
		draw_ubo.flags = (float)flags;
	}

	VkPipeline pipeline = pipe_cache.Get(draw_key);
	if (pipeline == VK_NULL_HANDLE) {
#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)
		if (Is_Shadow_Decal_Pipeline_Key(draw_key)) {
			static int shadow_pipe_null_log = 0;
			if (shadow_pipe_null_log < 8) {
				++shadow_pipe_null_log;
				fprintf(stderr,
					"Draw_Indexed shadow_decal pipeline NULL: fvf=%u ablend=%d src=%d dst=%d depth_test=%d cmp=%d twosided=%d\n",
					draw_key.fvf, (int)draw_key.alpha_blend,
					(int)draw_key.src_blend, (int)draw_key.dst_blend,
					(int)draw_key.depth_test, (int)draw_key.depth_compare,
					(int)draw_key.two_sided);
			}
		}
#endif
		return;
	}

#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)
	if (Is_Shadow_Decal_Pipeline_Key(draw_key)) {
		static int shadow_draw_log = 0;
		if (shadow_draw_log < 32) {
			++shadow_draw_log;
			const int uses_default_tex =
				(bound_textures_[0] == nullptr || bound_textures_[0] == &default_texture_) ? 1 : 0;
			const uint32_t ubo_flags = (uint32_t)draw_ubo.flags;
			const int has_shadow_flag =
				(ubo_flags & (uint32_t)FrameUBO::FLAG_SHADOW_DECAL) != 0 ? 1 : 0;
			fprintf(stderr,
				"Draw_Indexed shadow_decal: idx_count=%u default_tex=%d shadow_flag=%d fvf=%u depth_test=%d cmp=%d\n",
				index_count, uses_default_tex, has_shadow_flag,
				draw_key.fvf, (int)draw_key.depth_test, (int)draw_key.depth_compare);
		}
	}
#endif

	if (frame_ubo_draw_count_ + pending_draws_.size() >= kUboDrawsPerFrame) {
		static int ubo_cap_warn = 0;
		if (ubo_cap_warn < 4) {
			++ubo_cap_warn;
			fprintf(stderr, "Vulkan UBO draw cap exceeded (%u draws)\n", kUboDrawsPerFrame);
		}
		return;
	}

	/*
	 * Screen-space 2D (XYRHW) and menu font glow: flush queued work, then draw
	 * immediately (matches D3D8 immediate DrawIndexedPrimitive ordering).
	 */
	const bool immediate_menu_glow = DX8Wrapper::Vulkan_Menu_Glow_Draw_Active();
	const bool immediate_2d =
		((key.fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW) ||
		immediate_menu_glow;
	/*
	 * Stencil state changes per shadow pass and must be executed in order.
	 */
	const bool immediate_stencil = key.stencil_test;
	/*
	 * Shadow decals: D3D8 draws immediately after terrain is on screen so
	 * multiplicative blend hits the final color buffer (not a pending batch).
	 */
	if (immediate_2d || shadow_decal || immediate_stencil) {
		FrameSync &frame = frames_[current_frame_];
		VkCommandBuffer cmd = frame.command_buffer;
		const uint32_t aligned_ubo_size = Align_Ubo_Size(sizeof(FrameUBO), ubo_alignment_);

		Flush_Pending_Draws();

#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)
		if (shadow_decal) {
			static int shadow_immediate_log = 0;
			if (shadow_immediate_log < 32) {
				++shadow_immediate_log;
				fprintf(stderr,
					"Draw_Indexed shadow_decal immediate: idx_count=%u fvf=%u depth_test=%d cmp=%d\n",
					index_count, draw_key.fvf,
					(int)draw_key.depth_test, (int)draw_key.depth_compare);
			}
		}
#endif

		VkExtent2D extent = offscreen_target_ != nullptr
			? offscreen_target_->Render_Extent()
			: swapchain_.Extent();
		Apply_Viewport(cmd, extent);

		if (bound_pipeline_ != pipeline) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			bound_pipeline_ = pipeline;
		}

		const VkDeviceSize ubo_offset = (VkDeviceSize)frame_ubo_draw_count_ * aligned_ubo_size;
		frame_ubo_ring_[current_frame_].Upload(&draw_ubo, sizeof(draw_ubo), ubo_offset);
		++frame_ubo_draw_count_;

		Flush_Push_Descriptors(cmd, pipe_cache.Layout(), ubo_offset);
		/* Shadow volumes: constant-only bias (CSM-style acne mitigation).
		 * Do not inherit terrain/decal ZBias / slope. */
		if (key.stencil_test && key.color_write_mask == 0) {
			Apply_Volume_Depth_Bias(cmd);
		} else {
			Apply_Dx8_Depth_Bias(cmd);
		}

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, &offset);
		vkCmdBindIndexBuffer(cmd, index_buffer, 0, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(cmd, index_count, 1, first_index, vertex_offset, 0);
		return;
	}

	PendingDraw draw;
	draw.key = draw_key;
	draw.vertex_buffer = vertex_buffer;
	draw.index_buffer = index_buffer;
	draw.index_count = index_count;
	draw.first_index = first_index;
	draw.vertex_offset = vertex_offset;
	draw.depth_bias = DX8Wrapper::Get_Effective_ZBias();
	draw.ubo = draw_ubo;
	draw.textures[0] = Resolve_Draw_Texture(bound_textures_[0], &default_texture_);
	draw.textures[1] = Resolve_Draw_Texture(bound_textures_[1], &default_texture_);
	draw.textures[2] = Resolve_Draw_Texture(bound_textures_[2], &default_texture_);
	draw.textures[3] = Resolve_Draw_Texture(bound_textures_[3], &default_texture_);
	draw.submit_order = next_submit_order_++;

	pending_draws_.push_back(draw);
}

void VkRenderer::Retire_Sampler(VkSampler sampler)
{
	if (sampler == VK_NULL_HANDLE) {
		return;
	}
	/*
	 * Push onto the current frame's retire list.  The handle is destroyed in
	 * Begin_Frame once the fence for this slot has been waited on.  Using the
	 * *current* frame slot is safe because that slot was drained at the start
	 * of this frame, so we won't clobber an earlier retire; and the slot is
	 * not recycled until the next Begin_Frame, which is past the fence wait.
	 */
	retired_samplers_[current_frame_].push_back(sampler);
}

void VkRenderer::Retire_Texture(VkTexture *texture)
{
	if (texture == nullptr) {
		return;
	}
	/*
	 * Same lifetime rules as Retire_Sampler: GPU may still sample this image
	 * from an in-flight CB. Destroying immediately during Free_Assets (exit to
	 * main menu) deadlocks some drivers; defer until the frame fence clears.
	 */
	retired_textures_[current_frame_].push_back(texture);
}

} /* namespace ww3d_vulkan */
