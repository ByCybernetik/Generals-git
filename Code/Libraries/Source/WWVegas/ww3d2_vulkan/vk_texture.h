#ifndef WW3D2_VULKAN_VK_TEXTURE_H
#define WW3D2_VULKAN_VK_TEXTURE_H

#include <vulkan/vulkan.h>
#include "vk_allocator.h"
#include "../ww3d2/ww3dformat.h"

class DDSFileClass;

namespace ww3d_vulkan {

class VkTexture {
public:
	bool Create_Empty(uint32_t width, uint32_t height, WW3DFormat format, bool clamp_uv = false);
	bool Create_Solid(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
	bool Create_From_DDS(
		const DDSFileClass &dds,
		VkSamplerAddressMode address_u,
		VkSamplerAddressMode address_v);
	bool Create_From_Compressed(
		WW3DFormat format,
		uint32_t width,
		uint32_t height,
		uint32_t mip_levels,
		const uint8_t *compressed_data,
		size_t compressed_size,
		VkSamplerAddressMode address_u,
		VkSamplerAddressMode address_v);
	bool Create_From_Rgba8(
		const uint8_t *rgba,
		uint32_t width,
		uint32_t height,
		VkSamplerAddressMode address_u,
		VkSamplerAddressMode address_v);
	bool Create_As_Render_Target(
		uint32_t width,
		uint32_t height,
		WW3DFormat format,
		VkRenderPass render_pass,
		VkFormat depth_format);
	bool Upload_Rgb565_Region(
		const unsigned char *src,
		int src_pitch,
		uint32_t dst_x,
		uint32_t dst_y,
		uint32_t copy_w,
		uint32_t copy_h);
	bool Upload_Rgba8_Region(
		const unsigned char *src,
		int src_pitch,
		uint32_t dst_x,
		uint32_t dst_y,
		uint32_t copy_w,
		uint32_t copy_h);
	void Destroy();

	VkImage Image() const { return image_; }
	VkImageView View() const { return view_; }
	VkSampler Sampler() const { return sampler_; }
	VkImageLayout Layout() const { return layout_; }
	bool Is_Render_Target() const { return is_render_target_; }
	VkFramebuffer Render_Framebuffer() const { return framebuffer_; }
	VkExtent2D Render_Extent() const { return render_extent_; }

	uint32_t Width() const { return width_; }
	uint32_t Height() const { return height_; }
	uint32_t Mip_Levels() const { return mip_levels_; }

	void Set_Layout_Shader_Read_Only() { layout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; }

	/** Recreate sampler when address mode changes (e.g. atlas UI needs CLAMP). */
	void Update_Sampler_Address(VkSamplerAddressMode address_u, VkSamplerAddressMode address_v);

	/** Recreate sampler when any sampler state changes.
	 *  max_lod < 0 → use all mip levels; 0 → mip0 only (D3DTEXF_NONE).
	 *  max_anisotropy > 1 enables sampler anisotropy. */
	void Update_Sampler(
		VkFilter mag_filter,
		VkFilter min_filter,
		VkSamplerMipmapMode mip_mode,
		VkSamplerAddressMode address_u,
		VkSamplerAddressMode address_v,
		float max_lod = -1.0f,
		float max_anisotropy = 1.0f);

private:
	VkImage image_ = VK_NULL_HANDLE;
	VmaAllocation allocation_ = VK_NULL_HANDLE;
	VkImageView view_ = VK_NULL_HANDLE;
	VkSampler sampler_ = VK_NULL_HANDLE;
	VkSamplerAddressMode sampler_u_ = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkSamplerAddressMode sampler_v_ = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkFilter mag_filter_ = VK_FILTER_LINEAR;
	VkFilter min_filter_ = VK_FILTER_LINEAR;
	VkSamplerMipmapMode mip_mode_ = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	float max_lod_ = 0.0f;
	float max_anisotropy_ = 1.0f;
	VkImageLayout layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	uint32_t mip_levels_ = 1;
	bool is_render_target_ = false;
	VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
	VkImage depth_image_ = VK_NULL_HANDLE;
	VmaAllocation depth_allocation_ = VK_NULL_HANDLE;
	VkImageView depth_view_ = VK_NULL_HANDLE;
	VkExtent2D render_extent_ = {};
};

} /* namespace ww3d_vulkan */

#endif
