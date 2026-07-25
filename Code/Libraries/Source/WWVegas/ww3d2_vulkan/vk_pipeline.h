#ifndef WW3D2_VULKAN_VK_PIPELINE_H
#define WW3D2_VULKAN_VK_PIPELINE_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

namespace ww3d_vulkan {

struct MeshPipelineKey {
	bool terrain_shader = false;
	bool alpha_blend = false;
	uint8_t src_blend = 1;
	uint8_t dst_blend = 0;
	bool depth_write = true;
	bool depth_test = true;
	uint8_t depth_compare = 3;
	bool two_sided = false;
	bool cull_inverted = false;
	bool depth_clamp = false;
	bool depth_bias_enable = true;
	bool alpha_test = false;
	bool wireframe = false;
	uint8_t topology = 0;
	unsigned fvf = 0;
	uint16_t vertex_stride = 36;

	bool stencil_test = false;
	uint8_t stencil_func = 1;       /* D3DCMP_LESSEQUAL placeholder; default matches no-stencil. */
	uint32_t stencil_ref = 0;
	uint32_t stencil_mask = 0xFFFFFFFF;
	uint32_t stencil_write_mask = 0xFFFFFFFF;
	uint8_t stencil_fail = 1;       /* D3DSTENCILOP_KEEP */
	uint8_t stencil_zfail = 1;      /* D3DSTENCILOP_KEEP */
	uint8_t stencil_pass = 1;       /* D3DSTENCILOP_KEEP */
	/* When set, back-face stencil pass op differs (two-sided stencil shadow). */
	uint8_t stencil_pass_back = 1;
	bool two_sided_stencil = false;

	uint8_t color_write_mask = 0xF;

	bool operator==(const MeshPipelineKey &other) const
	{
		return terrain_shader == other.terrain_shader &&
			alpha_blend == other.alpha_blend &&
			src_blend == other.src_blend &&
			dst_blend == other.dst_blend &&
			depth_write == other.depth_write &&
			depth_test == other.depth_test &&
			depth_compare == other.depth_compare &&
			two_sided == other.two_sided &&
			cull_inverted == other.cull_inverted &&
			depth_clamp == other.depth_clamp &&
			depth_bias_enable == other.depth_bias_enable &&
			alpha_test == other.alpha_test &&
			wireframe == other.wireframe &&
			topology == other.topology &&
			fvf == other.fvf &&
			vertex_stride == other.vertex_stride &&
			stencil_test == other.stencil_test &&
			stencil_func == other.stencil_func &&
			stencil_ref == other.stencil_ref &&
			stencil_mask == other.stencil_mask &&
			stencil_write_mask == other.stencil_write_mask &&
			stencil_fail == other.stencil_fail &&
			stencil_zfail == other.stencil_zfail &&
			stencil_pass == other.stencil_pass &&
			stencil_pass_back == other.stencil_pass_back &&
			two_sided_stencil == other.two_sided_stencil &&
			color_write_mask == other.color_write_mask;
	}
};

class VkPipelineCache {
public:
	bool Create(
		VkRenderPass render_pass,
		const std::vector<uint32_t> &vert_spirv,
		const std::vector<uint32_t> &frag_spirv,
		const std::vector<uint32_t> &terrain_vert_spirv,
		const std::vector<uint32_t> &terrain_frag_spirv);
	void Destroy();

	VkPipeline Get(const MeshPipelineKey &key);
	VkPipelineLayout Layout() const { return pipeline_layout_; }
	VkDescriptorSetLayout Descriptor_Set_Layout() const { return descriptor_set_layout_; }

private:
	bool Create_Pipeline(const MeshPipelineKey &key);

	VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
	VkShaderModule vert_shader_ = VK_NULL_HANDLE;
	VkShaderModule frag_shader_ = VK_NULL_HANDLE;
	VkShaderModule terrain_vert_shader_ = VK_NULL_HANDLE;
	VkShaderModule terrain_frag_shader_ = VK_NULL_HANDLE;
	VkRenderPass render_pass_ = VK_NULL_HANDLE;
	::VkPipelineCache vk_driver_cache_ = VK_NULL_HANDLE;

	struct PipelineEntry {
		MeshPipelineKey key;
		VkPipeline pipeline = VK_NULL_HANDLE;
	};
	std::vector<PipelineEntry> pipelines_;
	MeshPipelineKey last_lookup_key_ = {};
	VkPipeline last_lookup_pipeline_ = VK_NULL_HANDLE;
	bool last_lookup_valid_ = false;
};

} /* namespace ww3d_vulkan */

#endif
