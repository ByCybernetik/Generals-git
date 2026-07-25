#include "vk_pipeline.h"
#include "vk_check.h"
#include "vk_context.h"
#include "vk_shader.h"
#include "vk_dx8_texture.h"

#include "../ww3d2/dx8fvf.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#if defined(RENEGADE_LINUX)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ww3d_vulkan {

namespace {

VkVertexInputBindingDescription Mesh_Binding(uint16_t stride)
{
	VkVertexInputBindingDescription binding = {};
	binding.binding = 0;
	binding.stride = stride;
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return binding;
}

void Mesh_Attributes(
	VkVertexInputAttributeDescription attrs[5],
	unsigned fvf,
	uint32_t &attr_count,
	uint32_t &has_normal,
	uint32_t &has_diffuse,
	uint32_t &tex_layers)
{
	FVFInfoClass fi(fvf);
	/*
	 * Emit all five attribute descriptions unconditionally.  The vertex
	 * shader (mesh_textured.vert) always declares inputs at locations 0..4;
	 * specialization constants (HAS_NORMAL, HAS_DIFFUSE, TEX_LAYER_COUNT)
	 * only dead-strip the code that consumes the missing attributes, they do
	 * not remove the input declarations from SPIR-V.  Without a matching
	 * VkVertexInputAttributeDescription for every declared location, the
	 * Khronos validation layer reports VUID-VkGraphicsPipelineCreateInfo-
	 * Input-07904 and strict drivers reject the pipeline — which broke the
	 * XYZ-only shadow-volume FVF.  When an attribute is absent from the FVF,
	 * point it at the position offset (guaranteed in-bounds for any stride
	 * >= 12) so the read never overruns the vertex; the dead code strips the
	 * value anyway.
	 */
	const uint32_t pos_off = fi.Get_Location_Offset();
	attr_count = 0;
	has_normal = (fvf & D3DFVF_NORMAL) ? 1u : 0u;
	has_diffuse = (fvf & D3DFVF_DIFFUSE) ? 1u : 0u;
	tex_layers = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	if (tex_layers > 2u) {
		tex_layers = 2u;
	}

	auto add_attr = [&](uint32_t location, VkFormat format, uint32_t offset) {
		attrs[attr_count].binding = 0;
		attrs[attr_count].location = location;
		attrs[attr_count].format = format;
		attrs[attr_count].offset = offset;
		attr_count++;
	};

	add_attr(0, VK_FORMAT_R32G32B32_SFLOAT, pos_off);
	add_attr(1, VK_FORMAT_R32G32B32_SFLOAT, has_normal ? fi.Get_Normal_Offset() : pos_off);
	add_attr(2, VK_FORMAT_R32_UINT, has_diffuse ? fi.Get_Diffuse_Offset() : pos_off);
	add_attr(3, VK_FORMAT_R32G32_SFLOAT, tex_layers >= 1u ? fi.Get_Tex_Offset(0) : pos_off);
	add_attr(4, VK_FORMAT_R32G32_SFLOAT, tex_layers >= 2u ? fi.Get_Tex_Offset(1) : pos_off);
}

static VkBlendFactor Src_Blend_To_Vk(uint8_t mode)
{
	switch (mode) {
	case 0:
		return VK_BLEND_FACTOR_ZERO;
	case 1:
		return VK_BLEND_FACTOR_ONE;
	case 2:
		return VK_BLEND_FACTOR_SRC_ALPHA;
	case 3:
		/* WW3D srcBlendLUT[3] == D3DBLEND_DESTCOLOR (see shader.cpp). */
		return VK_BLEND_FACTOR_DST_COLOR;
	default:
		return VK_BLEND_FACTOR_ONE;
	}
}

static VkBlendFactor Dst_Blend_To_Vk(uint8_t mode)
{
	switch (mode) {
	case 0:
		return VK_BLEND_FACTOR_ZERO;
	case 1:
		return VK_BLEND_FACTOR_ONE;
	case 2:
		return VK_BLEND_FACTOR_SRC_COLOR;
	case 3:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	case 4:
		return VK_BLEND_FACTOR_SRC_ALPHA;
	case 5:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	default:
		return VK_BLEND_FACTOR_ZERO;
	}
}

static VkCompareOp Depth_Compare_To_Vk(uint8_t mode)
{
	switch (mode) {
	case 1:
		return VK_COMPARE_OP_LESS;
	case 2:
		return VK_COMPARE_OP_EQUAL;
	case 3:
		return VK_COMPARE_OP_LESS_OR_EQUAL;
	case 4:
		return VK_COMPARE_OP_GREATER;
	case 5:
		return VK_COMPARE_OP_NOT_EQUAL;
	case 6:
		return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case 7:
		return VK_COMPARE_OP_ALWAYS;
	default:
		return VK_COMPARE_OP_LESS_OR_EQUAL;
	}
}

static VkCompareOp Stencil_Compare_To_Vk(uint8_t mode)
{
	/* D3DCMP_NEVER=1 .. D3DCMP_ALWAYS=8 */
	switch (mode) {
	case 1:
		return VK_COMPARE_OP_NEVER;
	case 2:
		return VK_COMPARE_OP_LESS;
	case 3:
		return VK_COMPARE_OP_EQUAL;
	case 4:
		return VK_COMPARE_OP_LESS_OR_EQUAL;
	case 5:
		return VK_COMPARE_OP_GREATER;
	case 6:
		return VK_COMPARE_OP_NOT_EQUAL;
	case 7:
		return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case 8:
		return VK_COMPARE_OP_ALWAYS;
	default:
		return VK_COMPARE_OP_ALWAYS;
	}
}

static VkStencilOp Stencil_Op_To_Vk(uint8_t op)
{
	/* D3DSTENCILOP_KEEP=1 .. D3DSTENCILOP_DECRSAT=5, D3DSTENCILOP_INCR=7, D3DSTENCILOP_DECR=8 */
	switch (op) {
	case 1:
		return VK_STENCIL_OP_KEEP;
	case 2:
		return VK_STENCIL_OP_ZERO;
	case 3:
		return VK_STENCIL_OP_REPLACE;
	case 4:
		return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
	case 5:
		return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
	case 6:
		return VK_STENCIL_OP_INVERT;
	case 7:
		return VK_STENCIL_OP_INCREMENT_AND_WRAP;
	case 8:
		return VK_STENCIL_OP_DECREMENT_AND_WRAP;
	default:
		return VK_STENCIL_OP_KEEP;
	}
}

#if defined(RENEGADE_LINUX)

static bool Ensure_Cache_Directory(const char *path)
{
	char dir[512];
	size_t len = strlen(path);
	if (len >= sizeof(dir)) {
		return false;
	}
	memcpy(dir, path, len + 1);
	char *slash = strrchr(dir, '/');
	if (slash == NULL) {
		return true;
	}
	*slash = '\0';
	if (dir[0] == '\0') {
		return true;
	}
	mkdir(dir, 0755);
	return true;
}

static const char *Driver_Pipeline_Cache_Path()
{
	static char path[512];
	const char *xdg = getenv("XDG_CACHE_HOME");
	if (xdg != NULL && xdg[0] != '\0') {
		snprintf(path, sizeof(path), "%s/renegade/vulkan_pipeline.cache", xdg);
	} else {
		const char *home = getenv("HOME");
		if (home == NULL || home[0] == '\0') {
			return NULL;
		}
		snprintf(path, sizeof(path), "%s/.cache/renegade/vulkan_pipeline.cache", home);
	}
	return path;
}

static bool Load_Driver_Pipeline_Cache(std::vector<char> &out)
{
	const char *path = Driver_Pipeline_Cache_Path();
	if (path == NULL) {
		return false;
	}

	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		return false;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return false;
	}
	long size = ftell(file);
	if (size <= 0) {
		fclose(file);
		return false;
	}
	if (fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return false;
	}

	out.resize((size_t)size);
	if (fread(out.data(), 1, out.size(), file) != out.size()) {
		out.clear();
		fclose(file);
		return false;
	}
	fclose(file);
	return true;
}

static void Save_Driver_Pipeline_Cache(::VkPipelineCache cache)
{
	const char *path = Driver_Pipeline_Cache_Path();
	if (path == NULL || cache == VK_NULL_HANDLE) {
		return;
	}

	VkContext &ctx = VkContext::Get();
	if (ctx.Device() == VK_NULL_HANDLE) {
		return;
	}

	size_t size = 0;
	VkResult result = vkGetPipelineCacheData(ctx.Device(), cache, &size, nullptr);
	if (result != VK_SUCCESS || size == 0) {
		return;
	}

	std::vector<char> data(size);
	result = vkGetPipelineCacheData(ctx.Device(), cache, &size, data.data());
	if (result != VK_SUCCESS || size == 0) {
		return;
	}
	data.resize(size);

	if (!Ensure_Cache_Directory(path)) {
		return;
	}

	FILE *file = fopen(path, "wb");
	if (file == NULL) {
		return;
	}
	fwrite(data.data(), 1, data.size(), file);
	fclose(file);
}

#endif /* RENEGADE_LINUX */

} /* namespace */

bool VkPipelineCache::Create(
	VkRenderPass render_pass,
	const std::vector<uint32_t> &vert_spirv,
	const std::vector<uint32_t> &frag_spirv,
	const std::vector<uint32_t> &terrain_vert_spirv,
	const std::vector<uint32_t> &terrain_frag_spirv)
{
	Destroy();
	render_pass_ = render_pass;
	vert_shader_ = Load_Shader_Module(vert_spirv);
	frag_shader_ = Load_Shader_Module(frag_spirv);
	if (!terrain_vert_spirv.empty() && !terrain_frag_spirv.empty()) {
		terrain_vert_shader_ = Load_Shader_Module(terrain_vert_spirv);
		terrain_frag_shader_ = Load_Shader_Module(terrain_frag_spirv);
	}

	VkContext &ctx = VkContext::Get();

	VkDescriptorSetLayoutBinding bindings[5] = {};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	layout_info.bindingCount = 5;
	layout_info.pBindings = bindings;
	VK_CHECK(vkCreateDescriptorSetLayout(
		ctx.Device(), &layout_info, nullptr, &descriptor_set_layout_));

	VkPushConstantRange push_range = {};
	push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	push_range.offset = 0;
	push_range.size = 24; // vec4 modulate_color + float texture_enabled + float grayscale_enabled

	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &descriptor_set_layout_;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pPushConstantRanges = &push_range;
	VK_CHECK(vkCreatePipelineLayout(
		ctx.Device(), &pipeline_layout_info, nullptr, &pipeline_layout_));

	std::vector<char> initial_cache;
#if defined(RENEGADE_LINUX)
	Load_Driver_Pipeline_Cache(initial_cache);
#endif
	VkPipelineCacheCreateInfo cache_info = {};
	cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	if (!initial_cache.empty()) {
		cache_info.initialDataSize = initial_cache.size();
		cache_info.pInitialData = initial_cache.data();
	}
	VK_CHECK(vkCreatePipelineCache(
		ctx.Device(), &cache_info, nullptr, &vk_driver_cache_));

	MeshPipelineKey default_opaque;
	default_opaque.fvf = DX8_FVF_XYZNDUV1;
	default_opaque.vertex_stride = 36;
	MeshPipelineKey default_alpha;
	default_alpha.alpha_blend = true;
	default_alpha.depth_write = false;
	default_alpha.fvf = DX8_FVF_XYZNDUV1;
	default_alpha.vertex_stride = 36;
	MeshPipelineKey mesh_nuv1;
	mesh_nuv1.fvf = DX8_FVF_XYZNUV1;
	mesh_nuv1.vertex_stride = 32;
	MeshPipelineKey mesh_nduv2;
	mesh_nduv2.fvf = DX8_FVF_XYZNDUV2;
	mesh_nduv2.vertex_stride = 44;
	MeshPipelineKey mesh_nduv2_alpha;
	mesh_nduv2_alpha.alpha_blend = true;
	mesh_nduv2_alpha.depth_write = false;
	mesh_nduv2_alpha.fvf = DX8_FVF_XYZNDUV2;
	mesh_nduv2_alpha.vertex_stride = 44;
	MeshPipelineKey ui_depth;
	ui_depth.alpha_blend = true;
	ui_depth.depth_write = false;
	ui_depth.depth_compare = 7;
	ui_depth.fvf = DX8_FVF_XYZNDUV2;
	ui_depth.vertex_stride = 44;

	MeshPipelineKey terrain_default;
	terrain_default.terrain_shader = true;
	terrain_default.fvf = DX8_FVF_XYZDUV2;
	terrain_default.vertex_stride = 0;

	return Create_Pipeline(default_opaque) &&
		Create_Pipeline(default_alpha) &&
		Create_Pipeline(mesh_nuv1) &&
		Create_Pipeline(mesh_nduv2) &&
		Create_Pipeline(mesh_nduv2_alpha) &&
		Create_Pipeline(ui_depth) &&
		(terrain_vert_shader_ == VK_NULL_HANDLE || Create_Pipeline(terrain_default));
}

void VkPipelineCache::Destroy()
{
	VkContext &ctx = VkContext::Get();
	if (ctx.Device() != VK_NULL_HANDLE) {
#if defined(RENEGADE_LINUX)
		if (vk_driver_cache_ != VK_NULL_HANDLE) {
			Save_Driver_Pipeline_Cache(vk_driver_cache_);
		}
#endif
		for (size_t i = 0; i < pipelines_.size(); ++i) {
			if (pipelines_[i].pipeline != VK_NULL_HANDLE) {
				vkDestroyPipeline(ctx.Device(), pipelines_[i].pipeline, nullptr);
			}
		}
		if (pipeline_layout_ != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(ctx.Device(), pipeline_layout_, nullptr);
		}
		if (descriptor_set_layout_ != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(ctx.Device(), descriptor_set_layout_, nullptr);
		}
		if (vk_driver_cache_ != VK_NULL_HANDLE) {
			vkDestroyPipelineCache(ctx.Device(), vk_driver_cache_, nullptr);
		}
	}
	pipelines_.clear();
	Destroy_Shader_Module(vert_shader_);
	Destroy_Shader_Module(frag_shader_);
	Destroy_Shader_Module(terrain_vert_shader_);
	Destroy_Shader_Module(terrain_frag_shader_);
	pipeline_layout_ = VK_NULL_HANDLE;
	descriptor_set_layout_ = VK_NULL_HANDLE;
	vert_shader_ = VK_NULL_HANDLE;
	frag_shader_ = VK_NULL_HANDLE;
	terrain_vert_shader_ = VK_NULL_HANDLE;
	terrain_frag_shader_ = VK_NULL_HANDLE;
	vk_driver_cache_ = VK_NULL_HANDLE;
}

VkPipeline VkPipelineCache::Get(const MeshPipelineKey &key)
{
	if (last_lookup_valid_ && last_lookup_key_ == key) {
		return last_lookup_pipeline_;
	}

	for (size_t i = 0; i < pipelines_.size(); ++i) {
		if (pipelines_[i].key == key) {
			last_lookup_key_ = key;
			last_lookup_pipeline_ = pipelines_[i].pipeline;
			last_lookup_valid_ = true;
			return pipelines_[i].pipeline;
		}
	}

	if (!Create_Pipeline(key)) {
		return VK_NULL_HANDLE;
	}
	last_lookup_key_ = key;
	last_lookup_pipeline_ = pipelines_.back().pipeline;
	last_lookup_valid_ = true;
	return pipelines_.back().pipeline;
}

bool VkPipelineCache::Create_Pipeline(const MeshPipelineKey &key)
{
	if (key.terrain_shader &&
			(terrain_vert_shader_ == VK_NULL_HANDLE || terrain_frag_shader_ == VK_NULL_HANDLE)) {
		return false;
	}

	unsigned fvf = key.fvf;
	if (fvf == 0) {
		fvf = DX8_FVF_XYZNDUV1;
	}

	FVFInfoClass fvf_info(fvf);
	uint16_t stride = key.vertex_stride;
	if (stride == 0) {
		stride = (uint16_t)fvf_info.Get_FVF_Size();
	}

	VkVertexInputBindingDescription binding = Mesh_Binding(stride);
	VkVertexInputAttributeDescription attributes[5];
	uint32_t attr_count = 0;
	uint32_t has_normal = 0;
	uint32_t has_diffuse = 0;
	uint32_t tex_layers = 0;
	Mesh_Attributes(attributes, fvf, attr_count, has_normal, has_diffuse, tex_layers);

	VkPipelineVertexInputStateCreateInfo vertex_input = {};
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 1;
	vertex_input.pVertexBindingDescriptions = &binding;
	vertex_input.vertexAttributeDescriptionCount = attr_count;
	vertex_input.pVertexAttributeDescriptions = attributes;

	VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = key.topology == 1
		? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
		: VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = key.depth_clamp ? VK_TRUE : VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = key.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.depthBiasEnable = key.depth_bias_enable ? VK_TRUE : VK_FALSE;
	/*
	 * D3D8 default is D3DCULL_CW (cull clockwise tris → front faces are CCW).
	 * Invert_Backface_Culling() switches to D3DCULL_CCW (front faces are CW).
	 */
	if (key.two_sided) {
		rasterizer.cullMode = VK_CULL_MODE_NONE;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	} else if (key.cull_inverted) {
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	} else {
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	}

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = key.depth_test ? VK_TRUE : VK_FALSE;
	depth_stencil.depthWriteEnable = key.depth_write ? VK_TRUE : VK_FALSE;
	depth_stencil.depthCompareOp = Depth_Compare_To_Vk(key.depth_compare);
	depth_stencil.stencilTestEnable = key.stencil_test ? VK_TRUE : VK_FALSE;
	if (key.stencil_test) {
		VkStencilOpState stencil_front = {};
		stencil_front.failOp = Stencil_Op_To_Vk(key.stencil_fail);
		stencil_front.passOp = Stencil_Op_To_Vk(key.stencil_pass);
		stencil_front.depthFailOp = Stencil_Op_To_Vk(key.stencil_zfail);
		stencil_front.compareOp = Stencil_Compare_To_Vk(key.stencil_func);
		stencil_front.compareMask = key.stencil_mask;
		stencil_front.writeMask = key.stencil_write_mask;
		stencil_front.reference = key.stencil_ref;
		depth_stencil.front = stencil_front;
		if (key.two_sided_stencil) {
			VkStencilOpState stencil_back = stencil_front;
			stencil_back.passOp = Stencil_Op_To_Vk(key.stencil_pass_back);
			depth_stencil.back = stencil_back;
		} else {
			depth_stencil.back = stencil_front;
		}
	}

	VkPipelineColorBlendAttachmentState color_attachment = {};
	color_attachment.colorWriteMask = key.color_write_mask;
	if (key.alpha_blend) {
		color_attachment.blendEnable = VK_TRUE;
		color_attachment.srcColorBlendFactor = Src_Blend_To_Vk(key.src_blend);
		color_attachment.dstColorBlendFactor = Dst_Blend_To_Vk(key.dst_blend);
		color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		color_attachment.srcAlphaBlendFactor = Src_Blend_To_Vk(key.src_blend);
		color_attachment.dstAlphaBlendFactor = Dst_Blend_To_Vk(key.dst_blend);
		color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}

	VkPipelineColorBlendStateCreateInfo color_blending = {};
	color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.attachmentCount = 1;
	color_blending.pAttachments = &color_attachment;

	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_DEPTH_BIAS,
	};
	VkPipelineDynamicStateCreateInfo dynamic_state = {};
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = 3;
	dynamic_state.pDynamicStates = dynamic_states;

	uint32_t alpha_test_flag = key.alpha_test ? 1u : 0u;
	VkSpecializationMapEntry frag_spec_entry = {};
	frag_spec_entry.constantID = 0;
	frag_spec_entry.offset = 0;
	frag_spec_entry.size = sizeof(uint32_t);

	VkSpecializationInfo frag_spec_info = {};
	frag_spec_info.mapEntryCount = 1;
	frag_spec_info.pMapEntries = &frag_spec_entry;
	frag_spec_info.dataSize = sizeof(uint32_t);
	frag_spec_info.pData = &alpha_test_flag;

	uint32_t vert_spec_data[3] = {has_normal, has_diffuse, tex_layers};
	VkSpecializationMapEntry vert_spec_entries[3] = {};
	vert_spec_entries[0].constantID = 1;
	vert_spec_entries[0].offset = 0;
	vert_spec_entries[0].size = sizeof(uint32_t);
	vert_spec_entries[1].constantID = 2;
	vert_spec_entries[1].offset = sizeof(uint32_t);
	vert_spec_entries[1].size = sizeof(uint32_t);
	vert_spec_entries[2].constantID = 3;
	vert_spec_entries[2].offset = sizeof(uint32_t) * 2u;
	vert_spec_entries[2].size = sizeof(uint32_t);

	VkSpecializationInfo vert_spec_info = {};
	vert_spec_info.mapEntryCount = 3;
	vert_spec_info.pMapEntries = vert_spec_entries;
	vert_spec_info.dataSize = sizeof(vert_spec_data);
	vert_spec_info.pData = vert_spec_data;

	VkShaderModule use_vert = key.terrain_shader ? terrain_vert_shader_ : vert_shader_;
	VkShaderModule use_frag = key.terrain_shader ? terrain_frag_shader_ : frag_shader_;

	VkPipelineShaderStageCreateInfo shader_stages[2] = {};
	shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader_stages[0].module = use_vert;
	shader_stages[0].pName = "main";
	shader_stages[0].pSpecializationInfo = key.terrain_shader ? nullptr : &vert_spec_info;
	shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_stages[1].module = use_frag;
	shader_stages[1].pName = "main";
	shader_stages[1].pSpecializationInfo = key.terrain_shader ? nullptr : &frag_spec_info;

	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = shader_stages;
	pipeline_info.pVertexInputState = &vertex_input;
	pipeline_info.pInputAssemblyState = &input_assembly;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &rasterizer;
	pipeline_info.pMultisampleState = &multisampling;
	pipeline_info.pDepthStencilState = &depth_stencil;
	pipeline_info.pColorBlendState = &color_blending;
	pipeline_info.pDynamicState = &dynamic_state;
	pipeline_info.layout = pipeline_layout_;
	pipeline_info.renderPass = render_pass_;
	pipeline_info.subpass = 0;

	VkContext &ctx = VkContext::Get();
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkResult result = vkCreateGraphicsPipelines(
		ctx.Device(), vk_driver_cache_, 1, &pipeline_info, nullptr, &pipeline);
	if (result != VK_SUCCESS) {
		return false;
	}

	PipelineEntry entry;
	entry.key = key;
	entry.key.fvf = fvf;
	entry.key.vertex_stride = stride;
	entry.pipeline = pipeline;
	pipelines_.push_back(entry);

	return true;
}

} /* namespace ww3d_vulkan */
