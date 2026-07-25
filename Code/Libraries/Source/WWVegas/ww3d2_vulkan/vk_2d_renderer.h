#ifndef WW3D2_VULKAN_VK_2D_RENDERER_H
#define WW3D2_VULKAN_VK_2D_RENDERER_H

#include "vk_buffer.h"
#include "vk_platform.h"
#include "vk_texture.h"
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <vector>

namespace ww3d_vulkan {

class VkRenderer;

struct Simple2DVertex {
	float x;
	float y;
	float u;
	float v;
	uint32_t color; // A8R8G8B8 packed
};

class Vk2DRenderer {
public:
	bool Init(VkRenderer *renderer);
	void Shutdown();
	void Begin_Frame();
	void Flush_Coalesced_Draws(VkCommandBuffer cmd);
	void Draw_Batch(
		VkCommandBuffer cmd,
		const Simple2DVertex *vertices,
		uint32_t vertex_count,
		const uint16_t *indices,
		uint32_t index_count,
		VkTexture *texture,
		bool texturing,
		uint8_t src_blend, // ShaderClass::SrcBlendFuncType
		uint8_t dst_blend, // ShaderClass::DstBlendFuncType
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
		uint8_t stencil_pass = 1);

private:
	struct PipelineKey2D {
		uint8_t src_blend = 1;
		uint8_t dst_blend = 0;
		bool texturing = true;
		bool offscreen = false;
		bool stencil_test = false;
		bool depth_test = false;
		uint8_t stencil_func = 1;
		uint8_t stencil_ref = 0;
		uint32_t stencil_mask = 0xFFFFFFFFu;
		uint32_t stencil_write_mask = 0xFFFFFFFFu;
		uint8_t stencil_fail = 1;
		uint8_t stencil_zfail = 1;
		uint8_t stencil_pass = 1;

		bool operator==(const PipelineKey2D &other) const
		{
			return src_blend == other.src_blend &&
				dst_blend == other.dst_blend &&
				texturing == other.texturing &&
				offscreen == other.offscreen &&
				stencil_test == other.stencil_test &&
				depth_test == other.depth_test &&
				stencil_func == other.stencil_func &&
				stencil_ref == other.stencil_ref &&
				stencil_mask == other.stencil_mask &&
				stencil_write_mask == other.stencil_write_mask &&
				stencil_fail == other.stencil_fail &&
				stencil_zfail == other.stencil_zfail &&
				stencil_pass == other.stencil_pass;
		}
	};

	struct PipelineEntry {
		PipelineKey2D key;
		VkRenderPass render_pass = VK_NULL_HANDLE;
		VkPipeline pipeline = VK_NULL_HANDLE;
	};

	struct PushConstants2D {
		float modulate_color[4];
		float texture_enabled;
		float grayscale_enabled;
		float _pad[2];
	};

	bool Create_Pipeline(const PipelineKey2D &key, VkRenderPass render_pass);
	VkPipeline Get_Pipeline(const PipelineKey2D &key, VkRenderPass render_pass);
	void Submit_Batch(
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
		bool stencil_test,
		bool depth_test,
		uint8_t stencil_func,
		uint8_t stencil_ref,
		uint32_t stencil_mask,
		uint32_t stencil_write_mask,
		uint8_t stencil_fail,
		uint8_t stencil_zfail,
		uint8_t stencil_pass);

	struct CoalesceState {
		bool active = false;
		VkTexture *texture = nullptr;
		bool texturing = true;
		uint8_t src_blend = 1;
		uint8_t dst_blend = 0;
		float modulate_color[4] = {1.f, 1.f, 1.f, 1.f};
		bool grayscale = false;
		uint32_t viewport_x = 0;
		uint32_t viewport_y = 0;
		uint32_t viewport_w = 0;
		uint32_t viewport_h = 0;
		bool stencil_test = false;
		bool depth_test = false;
		uint8_t stencil_func = 1;
		uint8_t stencil_ref = 0;
		uint32_t stencil_mask = 0xFFFFFFFFu;
		uint32_t stencil_write_mask = 0xFFFFFFFFu;
		uint8_t stencil_fail = 1;
		uint8_t stencil_zfail = 1;
		uint8_t stencil_pass = 1;
		std::vector<Simple2DVertex> vertices;
		std::vector<uint16_t> indices;
	};

	VkRenderer *renderer_ = nullptr;

	VkShaderModule vert_shader_ = VK_NULL_HANDLE;
	VkShaderModule frag_shader_ = VK_NULL_HANDLE;
	std::vector<uint32_t> vert_spirv_;
	std::vector<uint32_t> frag_spirv_;

	VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
	::VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;

	VkBufferAlloc vertex_ring_[kMaxFramesInFlight];
	VkBufferAlloc index_ring_[kMaxFramesInFlight];
	VkDeviceSize vertex_offset_[kMaxFramesInFlight] = {};
	VkDeviceSize index_offset_[kMaxFramesInFlight] = {};

	std::vector<PipelineEntry> pipelines_;
	CoalesceState coalesce_;
};

} /* namespace ww3d_vulkan */

#endif /* WW3D2_VULKAN_VK_2D_RENDERER_H */
