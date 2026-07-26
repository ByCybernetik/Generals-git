#include "MapViewport.h"
#include "VulkanHost.h"
#include "MapDocument.h"
#include "RoadGeometry.h"
#include "ObjectMeshCache.h"
#include "WaterGeometry.h"
#include "DdsDecoder.h"
#include "Common/MapObject.h"
#include "Common/GlobalData.h"
#include "Common/FileSystem.h"
#include "Common/File.h"
#include "GameClient/TerrainRoads.h"
#include "GameClient/Water.h"
#include "imgui_impl_vulkan.h"

#define STBI_NO_STDIO
#include "stb_image.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <fstream>
#include <map>
#include <algorithm>
#include <cctype>

#ifndef WB_SHADER_DIR
#define WB_SHADER_DIR "."
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
struct Mat4
{
	float m[16];
	static Mat4 identity()
	{
		Mat4 r = {};
		r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f;
		return r;
	}
	static Mat4 perspective(float fovyDeg, float aspect, float znear, float zfar)
	{
		const float f = 1.f / tanf(fovyDeg * (float)M_PI / 180.f * 0.5f);
		Mat4 r = {};
		r.m[0] = f / aspect;
		r.m[5] = -f; /* Vulkan Y flip */
		r.m[10] = zfar / (znear - zfar);
		r.m[11] = -1.f;
		r.m[14] = (zfar * znear) / (znear - zfar);
		return r;
	}
	static Mat4 lookAt(float ex, float ey, float ez, float tx, float ty, float tz, float ux, float uy, float uz)
	{
		float fx = tx - ex, fy = ty - ey, fz = tz - ez;
		float fl = sqrtf(fx * fx + fy * fy + fz * fz);
		fx /= fl;
		fy /= fl;
		fz /= fl;
		float sx = fy * uz - fz * uy;
		float sy = fz * ux - fx * uz;
		float sz = fx * uy - fy * ux;
		float sl = sqrtf(sx * sx + sy * sy + sz * sz);
		sx /= sl;
		sy /= sl;
		sz /= sl;
		float ux2 = sy * fz - sz * fy;
		float uy2 = sz * fx - sx * fz;
		float uz2 = sx * fy - sy * fx;
		Mat4 r = identity();
		r.m[0] = sx;
		r.m[4] = sy;
		r.m[8] = sz;
		r.m[1] = ux2;
		r.m[5] = uy2;
		r.m[9] = uz2;
		r.m[2] = -fx;
		r.m[6] = -fy;
		r.m[10] = -fz;
		r.m[12] = -(sx * ex + sy * ey + sz * ez);
		r.m[13] = -(ux2 * ex + uy2 * ey + uz2 * ez);
		r.m[14] = -(-fx * ex - fy * ey - fz * ez);
		return r;
	}
	Mat4 operator*(const Mat4 &o) const
	{
		Mat4 r = {};
		for (int c = 0; c < 4; ++c)
			for (int row = 0; row < 4; ++row)
				r.m[c * 4 + row] = m[0 * 4 + row] * o.m[c * 4 + 0] + m[1 * 4 + row] * o.m[c * 4 + 1] +
					m[2 * 4 + row] * o.m[c * 4 + 2] + m[3 * 4 + row] * o.m[c * 4 + 3];
		return r;
	}
};

bool loadSpirv(const char *path, std::vector<uint32_t> &out)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f)
		return false;
	const std::streamsize sz = f.tellg();
	if (sz <= 0 || (sz % 4) != 0)
		return false;
	f.seekg(0);
	out.resize((size_t)sz / 4);
	f.read(reinterpret_cast<char *>(out.data()), sz);
	return (bool)f;
}

VkShaderModule makeShader(VkDevice dev, const std::vector<uint32_t> &spirv)
{
	VkShaderModuleCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = spirv.size() * 4;
	ci.pCode = spirv.data();
	VkShaderModule mod = VK_NULL_HANDLE;
	if (vkCreateShaderModule(dev, &ci, nullptr, &mod) != VK_SUCCESS)
		return VK_NULL_HANDLE;
	return mod;
}

/* Simple value noise for dirt albedo. */
float hash21(int x, int y)
{
	uint32_t n = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
	n = (n ^ (n >> 13)) * 1274126177u;
	return (n & 0xffffu) / 65535.f;
}
} // namespace

MapViewport::MapViewport() = default;

MapViewport::~MapViewport() = default;

uint32_t MapViewport::findMemoryType(VulkanHost &host, uint32_t typeFilter, VkMemoryPropertyFlags props)
{
	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(host.physicalDevice(), &memProps);
	for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
	{
		if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
			return i;
	}
	return 0;
}

bool MapViewport::uploadBuffer(VulkanHost &host, VkBuffer &buf, VkDeviceMemory &mem, const void *data,
	VkDeviceSize size, VkBufferUsageFlags usage)
{
	VkDevice dev = host.device();
	if (buf)
	{
		vkDestroyBuffer(dev, buf, nullptr);
		buf = VK_NULL_HANDLE;
	}
	if (mem)
	{
		vkFreeMemory(dev, mem, nullptr);
		mem = VK_NULL_HANDLE;
	}

	VkBufferCreateInfo bi = {};
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = size;
	bi.usage = usage;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(dev, &bi, nullptr, &buf) != VK_SUCCESS)
		return false;
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(dev, buf, &req);
	VkMemoryAllocateInfo ai = {};
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex =
		findMemoryType(host, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (vkAllocateMemory(dev, &ai, nullptr, &mem) != VK_SUCCESS)
		return false;
	vkBindBufferMemory(dev, buf, mem, 0);
	void *mapped = nullptr;
	vkMapMemory(dev, mem, 0, size, 0, &mapped);
	memcpy(mapped, data, (size_t)size);
	vkUnmapMemory(dev, mem);
	return true;
}

bool MapViewport::createDirtTexture(VulkanHost &host)
{
	destroyAlbedoTexture(host);
	const int W = 128, H = 128;
	std::vector<unsigned char> rgba((size_t)W * H * 4);
	for (int y = 0; y < H; ++y)
	{
		for (int x = 0; x < W; ++x)
		{
			float n = 0.55f * hash21(x, y) + 0.25f * hash21(x / 2, y / 2) + 0.20f * hash21(x / 4, y / 4);
			/* Match status-bar dirt approx {0.44, 0.435, 0.39} with mottling. */
			float r = 0.38f + 0.14f * n;
			float g = 0.36f + 0.13f * n;
			float b = 0.32f + 0.12f * n;
			size_t i = ((size_t)y * W + x) * 4;
			rgba[i + 0] = (unsigned char)(r * 255.f);
			rgba[i + 1] = (unsigned char)(g * 255.f);
			rgba[i + 2] = (unsigned char)(b * 255.f);
			rgba[i + 3] = 255;
		}
	}

	VkDevice dev = host.device();
	const VkDeviceSize size = rgba.size();

	VkBuffer staging = VK_NULL_HANDLE;
	VkDeviceMemory stagingMem = VK_NULL_HANDLE;
	{
		VkBufferCreateInfo bi = {};
		bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bi.size = size;
		bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		vkCreateBuffer(dev, &bi, nullptr, &staging);
		VkMemoryRequirements req;
		vkGetBufferMemoryRequirements(dev, staging, &req);
		VkMemoryAllocateInfo ai = {};
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = findMemoryType(host, req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		vkAllocateMemory(dev, &ai, nullptr, &stagingMem);
		vkBindBufferMemory(dev, staging, stagingMem, 0);
		void *mapped = nullptr;
		vkMapMemory(dev, stagingMem, 0, size, 0, &mapped);
		memcpy(mapped, rgba.data(), (size_t)size);
		vkUnmapMemory(dev, stagingMem);
	}

	{
		VkImageCreateInfo ii = {};
		ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		ii.imageType = VK_IMAGE_TYPE_2D;
		ii.format = VK_FORMAT_R8G8B8A8_UNORM;
		ii.extent = {(uint32_t)W, (uint32_t)H, 1};
		ii.mipLevels = 1;
		ii.arrayLayers = 1;
		ii.samples = VK_SAMPLE_COUNT_1_BIT;
		ii.tiling = VK_IMAGE_TILING_OPTIMAL;
		ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		vkCreateImage(dev, &ii, nullptr, &m_dirtImage);
		VkMemoryRequirements req;
		vkGetImageMemoryRequirements(dev, m_dirtImage, &req);
		VkMemoryAllocateInfo ai = {};
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = findMemoryType(host, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkAllocateMemory(dev, &ai, nullptr, &m_dirtMem);
		vkBindImageMemory(dev, m_dirtImage, m_dirtMem, 0);
	}

	VkCommandPool pool = host.uploadCommandPool();
	VkCommandBuffer cmd;
	VkCommandBufferAllocateInfo cai = {};
	cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cai.commandPool = pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	vkAllocateCommandBuffers(dev, &cai, &cmd);
	VkCommandBufferBeginInfo bi = {};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &bi);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = m_dirtImage;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
		nullptr, 1, &barrier);

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = {(uint32_t)W, (uint32_t)H, 1};
	vkCmdCopyBufferToImage(cmd, staging, m_dirtImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
		nullptr, 1, &barrier);
	vkEndCommandBuffer(cmd);
	VkSubmitInfo si = {};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	vkQueueSubmit(host.queue(), 1, &si, VK_NULL_HANDLE);
	vkQueueWaitIdle(host.queue());
	vkFreeCommandBuffers(dev, pool, 1, &cmd);
	vkDestroyBuffer(dev, staging, nullptr);
	vkFreeMemory(dev, stagingMem, nullptr);

	VkImageViewCreateInfo vi = {};
	vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vi.image = m_dirtImage;
	vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vi.format = VK_FORMAT_R8G8B8A8_UNORM;
	vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vi.subresourceRange.levelCount = 1;
	vi.subresourceRange.layerCount = 1;
	vkCreateImageView(dev, &vi, nullptr, &m_dirtView);

	if (m_dirtSampler == VK_NULL_HANDLE)
	{
		VkSamplerCreateInfo sci = {};
		sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sci.magFilter = VK_FILTER_LINEAR;
		sci.minFilter = VK_FILTER_LINEAR;
		sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		vkCreateSampler(dev, &sci, nullptr, &m_dirtSampler);
	}
	else
	{
		/* Dirt needs wrap; atlas path may have left CLAMP. */
		vkDestroySampler(dev, m_dirtSampler, nullptr);
		m_dirtSampler = VK_NULL_HANDLE;
		VkSamplerCreateInfo sci = {};
		sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sci.magFilter = VK_FILTER_LINEAR;
		sci.minFilter = VK_FILTER_LINEAR;
		sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		vkCreateSampler(dev, &sci, nullptr, &m_dirtSampler);
	}
	return true;
}

void MapViewport::destroyAlbedoTexture(VulkanHost &host)
{
	VkDevice dev = host.device();
	if (dev == VK_NULL_HANDLE)
		return;
	vkDeviceWaitIdle(dev);
	if (m_dirtView)
	{
		vkDestroyImageView(dev, m_dirtView, nullptr);
		m_dirtView = VK_NULL_HANDLE;
	}
	if (m_dirtImage)
	{
		vkDestroyImage(dev, m_dirtImage, nullptr);
		m_dirtImage = VK_NULL_HANDLE;
	}
	if (m_dirtMem)
	{
		vkFreeMemory(dev, m_dirtMem, nullptr);
		m_dirtMem = VK_NULL_HANDLE;
	}
}

bool MapViewport::uploadAlbedoTexture(VulkanHost &host, const unsigned char *rgba, int w, int h)
{
	if (!rgba || w < 1 || h < 1)
		return false;
	VkDevice dev = host.device();
	destroyAlbedoTexture(host);

	const VkDeviceSize size = (VkDeviceSize)w * (VkDeviceSize)h * 4;
	VkBuffer staging = VK_NULL_HANDLE;
	VkDeviceMemory stagingMem = VK_NULL_HANDLE;
	{
		VkBufferCreateInfo bi = {};
		bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bi.size = size;
		bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		vkCreateBuffer(dev, &bi, nullptr, &staging);
		VkMemoryRequirements req;
		vkGetBufferMemoryRequirements(dev, staging, &req);
		VkMemoryAllocateInfo ai = {};
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = findMemoryType(host, req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		vkAllocateMemory(dev, &ai, nullptr, &stagingMem);
		vkBindBufferMemory(dev, staging, stagingMem, 0);
		void *mapped = nullptr;
		vkMapMemory(dev, stagingMem, 0, size, 0, &mapped);
		memcpy(mapped, rgba, (size_t)size);
		vkUnmapMemory(dev, stagingMem);
	}

	{
		VkImageCreateInfo ii = {};
		ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		ii.imageType = VK_IMAGE_TYPE_2D;
		ii.format = VK_FORMAT_R8G8B8A8_UNORM;
		ii.extent = {(uint32_t)w, (uint32_t)h, 1};
		ii.mipLevels = 1;
		ii.arrayLayers = 1;
		ii.samples = VK_SAMPLE_COUNT_1_BIT;
		ii.tiling = VK_IMAGE_TILING_OPTIMAL;
		ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		vkCreateImage(dev, &ii, nullptr, &m_dirtImage);
		VkMemoryRequirements req;
		vkGetImageMemoryRequirements(dev, m_dirtImage, &req);
		VkMemoryAllocateInfo ai = {};
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = findMemoryType(host, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkAllocateMemory(dev, &ai, nullptr, &m_dirtMem);
		vkBindImageMemory(dev, m_dirtImage, m_dirtMem, 0);
	}

	VkCommandPool pool = host.uploadCommandPool();
	VkCommandBuffer cmd;
	VkCommandBufferAllocateInfo cai = {};
	cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cai.commandPool = pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	vkAllocateCommandBuffers(dev, &cai, &cmd);
	VkCommandBufferBeginInfo bi = {};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &bi);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = m_dirtImage;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
		nullptr, 1, &barrier);

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
	vkCmdCopyBufferToImage(cmd, staging, m_dirtImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
		nullptr, 1, &barrier);
	vkEndCommandBuffer(cmd);
	VkSubmitInfo si = {};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	vkQueueSubmit(host.queue(), 1, &si, VK_NULL_HANDLE);
	vkQueueWaitIdle(host.queue());
	vkFreeCommandBuffers(dev, pool, 1, &cmd);
	vkDestroyBuffer(dev, staging, nullptr);
	vkFreeMemory(dev, stagingMem, nullptr);

	VkImageViewCreateInfo vi = {};
	vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vi.image = m_dirtImage;
	vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vi.format = VK_FORMAT_R8G8B8A8_UNORM;
	vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vi.subresourceRange.levelCount = 1;
	vi.subresourceRange.layerCount = 1;
	vkCreateImageView(dev, &vi, nullptr, &m_dirtView);

	if (m_dirtSampler != VK_NULL_HANDLE)
	{
		vkDestroySampler(dev, m_dirtSampler, nullptr);
		m_dirtSampler = VK_NULL_HANDLE;
	}
	{
		VkSamplerCreateInfo sci = {};
		sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sci.magFilter = VK_FILTER_LINEAR;
		sci.minFilter = VK_FILTER_LINEAR;
		/* Original AlphaTerrainTextureClass uses CLAMP between distinct atlas tiles. */
		sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		vkCreateSampler(dev, &sci, nullptr, &m_dirtSampler);
	}

	if (m_descSet != VK_NULL_HANDLE)
	{
		VkDescriptorImageInfo ii = {};
		ii.sampler = m_dirtSampler;
		ii.imageView = m_dirtView;
		ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = m_descSet;
		write.dstBinding = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &ii;
		vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
	}
	return true;
}

bool MapViewport::createPipelines(VulkanHost &host)
{
	VkDevice dev = host.device();

	{
		VkAttachmentDescription attachments[2] = {};
		attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		attachments[1].format = VK_FORMAT_D32_SFLOAT;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
		VkAttachmentReference depthRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
		VkSubpassDescription sub = {};
		sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		sub.colorAttachmentCount = 1;
		sub.pColorAttachments = &colorRef;
		sub.pDepthStencilAttachment = &depthRef;

		VkSubpassDependency dep = {};
		dep.srcSubpass = VK_SUBPASS_EXTERNAL;
		dep.dstSubpass = 0;
		dep.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo rp = {};
		rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		rp.attachmentCount = 2;
		rp.pAttachments = attachments;
		rp.subpassCount = 1;
		rp.pSubpasses = &sub;
		rp.dependencyCount = 1;
		rp.pDependencies = &dep;
		if (vkCreateRenderPass(dev, &rp, nullptr, &m_renderPass) != VK_SUCCESS)
			return false;
	}

	{
		VkDescriptorSetLayoutBinding binds[2] = {};
		binds[0].binding = 0;
		binds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binds[0].descriptorCount = 1;
		binds[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		binds[1].binding = 1;
		binds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binds[1].descriptorCount = 1;
		binds[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		VkDescriptorSetLayoutCreateInfo dci = {};
		dci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dci.bindingCount = 2;
		dci.pBindings = binds;
		vkCreateDescriptorSetLayout(dev, &dci, nullptr, &m_dsl);

		VkPipelineLayoutCreateInfo plci = {};
		plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		plci.setLayoutCount = 1;
		plci.pSetLayouts = &m_dsl;
		vkCreatePipelineLayout(dev, &plci, nullptr, &m_pipelineLayout);
	}

	std::vector<uint32_t> vertSpv, fragSpv;
	char path[512];
	snprintf(path, sizeof(path), "%s/map_mesh.vert.spv", WB_SHADER_DIR);
	if (!loadSpirv(path, vertSpv))
	{
		fprintf(stderr, "MapViewport: cannot load %s\n", path);
		return false;
	}
	snprintf(path, sizeof(path), "%s/map_mesh.frag.spv", WB_SHADER_DIR);
	if (!loadSpirv(path, fragSpv))
	{
		fprintf(stderr, "MapViewport: cannot load %s\n", path);
		return false;
	}
	VkShaderModule vert = makeShader(dev, vertSpv);
	VkShaderModule frag = makeShader(dev, fragSpv);
	if (!vert || !frag)
		return false;

	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vert;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = frag;
	stages[1].pName = "main";

	VkVertexInputBindingDescription bind = {};
	bind.binding = 0;
	bind.stride = sizeof(Vertex);
	bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	VkVertexInputAttributeDescription attrs[5] = {};
	attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, px)};
	attrs[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, u)};
	attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, u2)};
	attrs[3] = {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, r)};
	attrs[4] = {4, 0, VK_FORMAT_R32_SFLOAT, offsetof(Vertex, blend)};
	VkPipelineVertexInputStateCreateInfo vi = {};
	vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vi.vertexBindingDescriptionCount = 1;
	vi.pVertexBindingDescriptions = &bind;
	vi.vertexAttributeDescriptionCount = 5;
	vi.pVertexAttributeDescriptions = attrs;

	VkPipelineInputAssemblyStateCreateInfo ia = {};
	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo vp = {};
	vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp.viewportCount = 1;
	vp.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rs = {};
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.f;

	VkPipelineMultisampleStateCreateInfo ms = {};
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo ds = {};
	ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_TRUE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS;

	VkPipelineColorBlendAttachmentState blendAtt = {};
	blendAtt.colorWriteMask = 0xf;
	VkPipelineColorBlendStateCreateInfo blend = {};
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &blendAtt;

	VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dyn = {};
	dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyn.dynamicStateCount = 2;
	dyn.pDynamicStates = dynStates;

	VkGraphicsPipelineCreateInfo gp = {};
	gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gp.stageCount = 2;
	gp.pStages = stages;
	gp.pVertexInputState = &vi;
	gp.pInputAssemblyState = &ia;
	gp.pViewportState = &vp;
	gp.pRasterizationState = &rs;
	gp.pMultisampleState = &ms;
	gp.pDepthStencilState = &ds;
	gp.pColorBlendState = &blend;
	gp.pDynamicState = &dyn;
	gp.layout = m_pipelineLayout;
	gp.renderPass = m_renderPass;
	const bool ok = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gp, nullptr, &m_pipeline) == VK_SUCCESS;

	vkDestroyShaderModule(dev, vert, nullptr);
	vkDestroyShaderModule(dev, frag, nullptr);
	if (!ok)
		return false;

	/* Road pipeline: same layout, alpha blend, depth write off (like W3DRoadBuffer). */
	{
		std::vector<uint32_t> rv, rf;
		snprintf(path, sizeof(path), "%s/map_road.vert.spv", WB_SHADER_DIR);
		if (!loadSpirv(path, rv))
		{
			fprintf(stderr, "MapViewport: cannot load %s\n", path);
			return false;
		}
		snprintf(path, sizeof(path), "%s/map_road.frag.spv", WB_SHADER_DIR);
		if (!loadSpirv(path, rf))
		{
			fprintf(stderr, "MapViewport: cannot load %s\n", path);
			return false;
		}
		VkShaderModule rVert = makeShader(dev, rv);
		VkShaderModule rFrag = makeShader(dev, rf);
		if (!rVert || !rFrag)
			return false;

		VkPipelineShaderStageCreateInfo rstages[2] = {};
		rstages[0] = stages[0];
		rstages[0].module = rVert;
		rstages[1] = stages[1];
		rstages[1].module = rFrag;

		VkVertexInputBindingDescription rbind = {};
		rbind.binding = 0;
		rbind.stride = sizeof(RoadVertex);
		rbind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		VkVertexInputAttributeDescription rattrs[2] = {};
		rattrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(RoadVertex, px)};
		rattrs[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(RoadVertex, u)};
		VkPipelineVertexInputStateCreateInfo rvi = {};
		rvi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		rvi.vertexBindingDescriptionCount = 1;
		rvi.pVertexBindingDescriptions = &rbind;
		rvi.vertexAttributeDescriptionCount = 2;
		rvi.pVertexAttributeDescriptions = rattrs;

		VkPipelineDepthStencilStateCreateInfo rds = ds;
		rds.depthWriteEnable = VK_FALSE;
		rds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		VkPipelineColorBlendAttachmentState rblendAtt = {};
		rblendAtt.blendEnable = VK_TRUE;
		rblendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		rblendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		rblendAtt.colorBlendOp = VK_BLEND_OP_ADD;
		rblendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		rblendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		rblendAtt.alphaBlendOp = VK_BLEND_OP_ADD;
		rblendAtt.colorWriteMask = 0xf;
		VkPipelineColorBlendStateCreateInfo rblend = blend;
		rblend.pAttachments = &rblendAtt;

		VkGraphicsPipelineCreateInfo rgp = gp;
		rgp.pStages = rstages;
		rgp.pVertexInputState = &rvi;
		rgp.pDepthStencilState = &rds;
		rgp.pColorBlendState = &rblend;
		const bool rok = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &rgp, nullptr, &m_roadPipeline) == VK_SUCCESS;
		vkDestroyShaderModule(dev, rVert, nullptr);
		vkDestroyShaderModule(dev, rFrag, nullptr);
		if (!rok)
			return false;
	}

	/* Object pipeline: textured meshes with depth write (WbView3d models). */
	{
		std::vector<uint32_t> ov, of;
		snprintf(path, sizeof(path), "%s/map_object.vert.spv", WB_SHADER_DIR);
		if (!loadSpirv(path, ov))
		{
			fprintf(stderr, "MapViewport: cannot load %s\n", path);
			return false;
		}
		snprintf(path, sizeof(path), "%s/map_object.frag.spv", WB_SHADER_DIR);
		if (!loadSpirv(path, of))
		{
			fprintf(stderr, "MapViewport: cannot load %s\n", path);
			return false;
		}
		VkShaderModule oVert = makeShader(dev, ov);
		VkShaderModule oFrag = makeShader(dev, of);
		if (!oVert || !oFrag)
			return false;

		VkPipelineShaderStageCreateInfo ostages[2] = {};
		ostages[0] = stages[0];
		ostages[0].module = oVert;
		ostages[1] = stages[1];
		ostages[1].module = oFrag;

		VkVertexInputBindingDescription obind = {};
		obind.binding = 0;
		obind.stride = sizeof(RoadVertex);
		obind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		VkVertexInputAttributeDescription oattrs[2] = {};
		oattrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(RoadVertex, px)};
		oattrs[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(RoadVertex, u)};
		VkPipelineVertexInputStateCreateInfo ovi = {};
		ovi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		ovi.vertexBindingDescriptionCount = 1;
		ovi.pVertexBindingDescriptions = &obind;
		ovi.vertexAttributeDescriptionCount = 2;
		ovi.pVertexAttributeDescriptions = oattrs;

		VkPipelineDepthStencilStateCreateInfo ods = ds;
		ods.depthTestEnable = VK_TRUE;
		ods.depthWriteEnable = VK_TRUE;
		ods.depthCompareOp = VK_COMPARE_OP_LESS;

		VkPipelineColorBlendAttachmentState oblendAtt = {};
		oblendAtt.blendEnable = VK_TRUE;
		oblendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		oblendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		oblendAtt.colorBlendOp = VK_BLEND_OP_ADD;
		oblendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		oblendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		oblendAtt.alphaBlendOp = VK_BLEND_OP_ADD;
		oblendAtt.colorWriteMask = 0xf;
		VkPipelineColorBlendStateCreateInfo oblend = blend;
		oblend.pAttachments = &oblendAtt;

		VkGraphicsPipelineCreateInfo ogp = gp;
		ogp.pStages = ostages;
		ogp.pVertexInputState = &ovi;
		ogp.pDepthStencilState = &ods;
		ogp.pColorBlendState = &oblend;
		const bool ook = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &ogp, nullptr, &m_objectPipeline) == VK_SUCCESS;
		vkDestroyShaderModule(dev, oVert, nullptr);
		vkDestroyShaderModule(dev, oFrag, nullptr);
		if (!ook)
			return false;
	}

	/* Water pipeline: translucent trapezoids/rivers (TYPE_0 like WB DrawObject). */
	{
		std::vector<uint32_t> wv, wf;
		snprintf(path, sizeof(path), "%s/map_water.vert.spv", WB_SHADER_DIR);
		if (!loadSpirv(path, wv))
		{
			fprintf(stderr, "MapViewport: cannot load %s\n", path);
			return false;
		}
		snprintf(path, sizeof(path), "%s/map_water.frag.spv", WB_SHADER_DIR);
		if (!loadSpirv(path, wf))
		{
			fprintf(stderr, "MapViewport: cannot load %s\n", path);
			return false;
		}
		VkShaderModule wVert = makeShader(dev, wv);
		VkShaderModule wFrag = makeShader(dev, wf);
		if (!wVert || !wFrag)
			return false;

		VkPipelineShaderStageCreateInfo wstages[2] = {};
		wstages[0] = stages[0];
		wstages[0].module = wVert;
		wstages[1] = stages[1];
		wstages[1].module = wFrag;

		VkVertexInputBindingDescription wbind = {};
		wbind.binding = 0;
		wbind.stride = sizeof(WaterVertex);
		wbind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		VkVertexInputAttributeDescription wattrs[3] = {};
		wattrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(WaterVertex, px)};
		wattrs[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(WaterVertex, u)};
		wattrs[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(WaterVertex, r)};
		VkPipelineVertexInputStateCreateInfo wvi = {};
		wvi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		wvi.vertexBindingDescriptionCount = 1;
		wvi.pVertexBindingDescriptions = &wbind;
		wvi.vertexAttributeDescriptionCount = 3;
		wvi.pVertexAttributeDescriptions = wattrs;

		VkPipelineDepthStencilStateCreateInfo wds = ds;
		wds.depthTestEnable = VK_TRUE;
		wds.depthWriteEnable = VK_FALSE;
		wds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		VkPipelineColorBlendAttachmentState wblendAtt = {};
		wblendAtt.blendEnable = VK_TRUE;
		wblendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		wblendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		wblendAtt.colorBlendOp = VK_BLEND_OP_ADD;
		wblendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		wblendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		wblendAtt.alphaBlendOp = VK_BLEND_OP_ADD;
		wblendAtt.colorWriteMask = 0xf;
		VkPipelineColorBlendStateCreateInfo wblend = blend;
		wblend.pAttachments = &wblendAtt;

		VkGraphicsPipelineCreateInfo wgp = gp;
		wgp.pStages = wstages;
		wgp.pVertexInputState = &wvi;
		wgp.pDepthStencilState = &wds;
		wgp.pColorBlendState = &wblend;
		const bool wok = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &wgp, nullptr, &m_waterPipeline) == VK_SUCCESS;
		vkDestroyShaderModule(dev, wVert, nullptr);
		vkDestroyShaderModule(dev, wFrag, nullptr);
		if (!wok)
			return false;
	}

	/* UBO: mvp + uvScroll (water animation); terrain/roads ignore trailing fields. */
	{
		const VkDeviceSize uboSize = sizeof(float) * 20;
		VkBufferCreateInfo bi = {};
		bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bi.size = uboSize;
		bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		vkCreateBuffer(dev, &bi, nullptr, &m_ubo);
		VkMemoryRequirements req;
		vkGetBufferMemoryRequirements(dev, m_ubo, &req);
		VkMemoryAllocateInfo ai = {};
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = findMemoryType(host, req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		vkAllocateMemory(dev, &ai, nullptr, &m_uboMem);
		vkBindBufferMemory(dev, m_ubo, m_uboMem, 0);
		vkMapMemory(dev, m_uboMem, 0, uboSize, 0, &m_uboMapped);
	}

	{
		VkDescriptorPoolSize sizes[2] = {};
		sizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256};
		sizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256};
		VkDescriptorPoolCreateInfo pci = {};
		pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pci.maxSets = 256;
		pci.poolSizeCount = 2;
		pci.pPoolSizes = sizes;
		vkCreateDescriptorPool(dev, &pci, nullptr, &m_descPool);
		VkDescriptorSetAllocateInfo dai = {};
		dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dai.descriptorPool = m_descPool;
		dai.descriptorSetCount = 1;
		dai.pSetLayouts = &m_dsl;
		vkAllocateDescriptorSets(dev, &dai, &m_descSet);

		VkDescriptorBufferInfo ubi = {};
		ubi.buffer = m_ubo;
		ubi.range = sizeof(float) * 20;
		VkDescriptorImageInfo ii = {};
		ii.sampler = m_dirtSampler;
		ii.imageView = m_dirtView;
		ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		VkWriteDescriptorSet writes[2] = {};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = m_descSet;
		writes[0].dstBinding = 0;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].descriptorCount = 1;
		writes[0].pBufferInfo = &ubi;
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = m_descSet;
		writes[1].dstBinding = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].descriptorCount = 1;
		writes[1].pImageInfo = &ii;
		vkUpdateDescriptorSets(dev, 2, writes, 0, nullptr);
	}

	m_pipelineReady = true;
	return true;
}

bool MapViewport::init(VulkanHost &host)
{
	if (m_inited)
		return true;
	if (!createDirtTexture(host))
		return false;
	if (!createPipelines(host))
		return false;
	m_inited = true;
	return true;
}

void MapViewport::destroyFramebuffer(VulkanHost &host)
{
	VkDevice dev = host.device();
	vkDeviceWaitIdle(dev);
	if (m_imguiTex)
	{
		ImGui_ImplVulkan_RemoveTexture(m_imguiTex);
		m_imguiTex = VK_NULL_HANDLE;
	}
	if (m_framebuffer)
	{
		vkDestroyFramebuffer(dev, m_framebuffer, nullptr);
		m_framebuffer = VK_NULL_HANDLE;
	}
	if (m_colorView)
	{
		vkDestroyImageView(dev, m_colorView, nullptr);
		m_colorView = VK_NULL_HANDLE;
	}
	if (m_colorImage)
	{
		vkDestroyImage(dev, m_colorImage, nullptr);
		m_colorImage = VK_NULL_HANDLE;
	}
	if (m_colorMem)
	{
		vkFreeMemory(dev, m_colorMem, nullptr);
		m_colorMem = VK_NULL_HANDLE;
	}
	if (m_depthView)
	{
		vkDestroyImageView(dev, m_depthView, nullptr);
		m_depthView = VK_NULL_HANDLE;
	}
	if (m_depthImage)
	{
		vkDestroyImage(dev, m_depthImage, nullptr);
		m_depthImage = VK_NULL_HANDLE;
	}
	if (m_depthMem)
	{
		vkFreeMemory(dev, m_depthMem, nullptr);
		m_depthMem = VK_NULL_HANDLE;
	}
	m_fbW = m_fbH = 0;
}

void MapViewport::destroyRoads(VulkanHost &host)
{
	VkDevice dev = host.device();
	if (dev == VK_NULL_HANDLE)
		return;
	vkDeviceWaitIdle(dev);
	for (RoadBatch &b : m_roadBatches)
	{
		if (b.view)
			vkDestroyImageView(dev, b.view, nullptr);
		if (b.image)
			vkDestroyImage(dev, b.image, nullptr);
		if (b.mem)
			vkFreeMemory(dev, b.mem, nullptr);
		b.view = VK_NULL_HANDLE;
		b.image = VK_NULL_HANDLE;
		b.mem = VK_NULL_HANDLE;
		b.descSet = VK_NULL_HANDLE;
	}
	m_roadBatches.clear();
	if (m_roadVbo)
	{
		vkDestroyBuffer(dev, m_roadVbo, nullptr);
		m_roadVbo = VK_NULL_HANDLE;
	}
	if (m_roadVboMem)
	{
		vkFreeMemory(dev, m_roadVboMem, nullptr);
		m_roadVboMem = VK_NULL_HANDLE;
	}
	if (m_roadIbo)
	{
		vkDestroyBuffer(dev, m_roadIbo, nullptr);
		m_roadIbo = VK_NULL_HANDLE;
	}
	if (m_roadIboMem)
	{
		vkFreeMemory(dev, m_roadIboMem, nullptr);
		m_roadIboMem = VK_NULL_HANDLE;
	}
}

void MapViewport::destroyObjects(VulkanHost &host)
{
	VkDevice dev = host.device();
	if (dev == VK_NULL_HANDLE)
		return;
	vkDeviceWaitIdle(dev);
	for (ObjectBatch &b : m_objectBatches)
	{
		if (b.view)
			vkDestroyImageView(dev, b.view, nullptr);
		if (b.image)
			vkDestroyImage(dev, b.image, nullptr);
		if (b.mem)
			vkFreeMemory(dev, b.mem, nullptr);
		b.view = VK_NULL_HANDLE;
		b.image = VK_NULL_HANDLE;
		b.mem = VK_NULL_HANDLE;
		b.descSet = VK_NULL_HANDLE;
	}
	m_objectBatches.clear();
	if (m_objectVbo)
	{
		vkDestroyBuffer(dev, m_objectVbo, nullptr);
		m_objectVbo = VK_NULL_HANDLE;
	}
	if (m_objectVboMem)
	{
		vkFreeMemory(dev, m_objectVboMem, nullptr);
		m_objectVboMem = VK_NULL_HANDLE;
	}
	if (m_objectIbo)
	{
		vkDestroyBuffer(dev, m_objectIbo, nullptr);
		m_objectIbo = VK_NULL_HANDLE;
	}
	if (m_objectIboMem)
	{
		vkFreeMemory(dev, m_objectIboMem, nullptr);
		m_objectIboMem = VK_NULL_HANDLE;
	}
}

void MapViewport::destroyWater(VulkanHost &host)
{
	VkDevice dev = host.device();
	if (dev == VK_NULL_HANDLE)
		return;
	vkDeviceWaitIdle(dev);
	if (m_waterView)
	{
		vkDestroyImageView(dev, m_waterView, nullptr);
		m_waterView = VK_NULL_HANDLE;
	}
	if (m_waterImage)
	{
		vkDestroyImage(dev, m_waterImage, nullptr);
		m_waterImage = VK_NULL_HANDLE;
	}
	if (m_waterMem)
	{
		vkFreeMemory(dev, m_waterMem, nullptr);
		m_waterMem = VK_NULL_HANDLE;
	}
	m_waterDescSet = VK_NULL_HANDLE;
	if (m_waterVbo)
	{
		vkDestroyBuffer(dev, m_waterVbo, nullptr);
		m_waterVbo = VK_NULL_HANDLE;
	}
	if (m_waterVboMem)
	{
		vkFreeMemory(dev, m_waterVboMem, nullptr);
		m_waterVboMem = VK_NULL_HANDLE;
	}
	if (m_waterIbo)
	{
		vkDestroyBuffer(dev, m_waterIbo, nullptr);
		m_waterIbo = VK_NULL_HANDLE;
	}
	if (m_waterIboMem)
	{
		vkFreeMemory(dev, m_waterIboMem, nullptr);
		m_waterIboMem = VK_NULL_HANDLE;
	}
	m_waterIndexCount = 0;
}

bool MapViewport::rebuildWater(VulkanHost &host, MapDocument &doc)
{
	destroyWater(host);
	if (!doc.heightMap())
		return true;

	std::vector<WaterMeshVertex> bakedVerts;
	std::vector<uint32_t> bakedIndices;
	const WaterBakeStats stats = WaterGeometry::bake(doc.heightMap(), bakedVerts, bakedIndices);
	if (bakedVerts.empty() || bakedIndices.empty())
	{
		fprintf(stderr, "MapViewport: no water geometry (lakes=%d rivers=%d)\n", stats.lakeAreas,
			stats.riverAreas);
		return true;
	}

	std::vector<WaterVertex> verts(bakedVerts.size());
	for (size_t i = 0; i < bakedVerts.size(); ++i)
	{
		verts[i].px = bakedVerts[i].px;
		verts[i].py = bakedVerts[i].py;
		verts[i].pz = bakedVerts[i].pz;
		verts[i].u = bakedVerts[i].u;
		verts[i].v = bakedVerts[i].v;
		verts[i].r = bakedVerts[i].r;
		verts[i].g = bakedVerts[i].g;
		verts[i].b = bakedVerts[i].b;
		verts[i].a = bakedVerts[i].a;
	}

	/* Texture: TWWater01 like WaterRenderObjClass::setupFlatWaterShader. */
	std::vector<unsigned char> rgba;
	int tw = 0, th = 0;
	const char *texCandidates[] = {"TWWater01.tga", "TWWater01.dds", "TSWater.tga", "TSWater.dds", nullptr};
	bool haveTex = false;
	for (int i = 0; texCandidates[i]; ++i)
	{
		if (loadRoadRgba(texCandidates[i], rgba, tw, th))
		{
			haveTex = true;
			break;
		}
	}
	if (!haveTex)
	{
		tw = th = 4;
		rgba.assign(64, 0);
		for (int i = 0; i < 16; ++i)
		{
			rgba[(size_t)i * 4 + 0] = 40;
			rgba[(size_t)i * 4 + 1] = 90;
			rgba[(size_t)i * 4 + 2] = 140;
			rgba[(size_t)i * 4 + 3] = 180;
		}
		fprintf(stderr, "MapViewport: water texture missing, using solid fallback\n");
	}

	RoadBatch tmp;
	if (!uploadRoadTexture(host, tmp, rgba.data(), tw, th))
	{
		fprintf(stderr, "MapViewport: water texture upload failed\n");
		return false;
	}
	m_waterImage = tmp.image;
	m_waterMem = tmp.mem;
	m_waterView = tmp.view;
	m_waterDescSet = tmp.descSet;

	if (!uploadBuffer(host, m_waterVbo, m_waterVboMem, verts.data(), verts.size() * sizeof(WaterVertex),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
		return false;
	if (!uploadBuffer(host, m_waterIbo, m_waterIboMem, bakedIndices.data(), bakedIndices.size() * sizeof(uint32_t),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
		return false;

	m_waterIndexCount = (uint32_t)bakedIndices.size();
	fprintf(stderr, "MapViewport: water ready (lakes=%d rivers=%d, %d verts, %d tris)\n", stats.lakeAreas,
		stats.riverAreas, stats.verts, stats.tris);
	return true;
}

void MapViewport::destroyMesh(VulkanHost &host)
{
	destroyRoads(host);
	destroyObjects(host);
	destroyWater(host);
	VkDevice dev = host.device();
	vkDeviceWaitIdle(dev);
	if (m_vbo)
	{
		vkDestroyBuffer(dev, m_vbo, nullptr);
		m_vbo = VK_NULL_HANDLE;
	}
	if (m_vboMem)
	{
		vkFreeMemory(dev, m_vboMem, nullptr);
		m_vboMem = VK_NULL_HANDLE;
	}
	if (m_ibo)
	{
		vkDestroyBuffer(dev, m_ibo, nullptr);
		m_ibo = VK_NULL_HANDLE;
	}
	if (m_iboMem)
	{
		vkFreeMemory(dev, m_iboMem, nullptr);
		m_iboMem = VK_NULL_HANDLE;
	}
	m_meshReady = false;
	m_indexCount = m_lineIndexCount = 0;
}

void MapViewport::destroy(VulkanHost &host)
{
	if (!m_inited)
		return;
	VkDevice dev = host.device();
	vkDeviceWaitIdle(dev);
	destroyFramebuffer(host);
	destroyMesh(host);
	if (m_waterPipeline)
		vkDestroyPipeline(dev, m_waterPipeline, nullptr);
	if (m_objectPipeline)
		vkDestroyPipeline(dev, m_objectPipeline, nullptr);
	if (m_roadPipeline)
		vkDestroyPipeline(dev, m_roadPipeline, nullptr);
	if (m_pipeline)
		vkDestroyPipeline(dev, m_pipeline, nullptr);
	if (m_pipelineLayout)
		vkDestroyPipelineLayout(dev, m_pipelineLayout, nullptr);
	if (m_dsl)
		vkDestroyDescriptorSetLayout(dev, m_dsl, nullptr);
	if (m_descPool)
		vkDestroyDescriptorPool(dev, m_descPool, nullptr);
	if (m_renderPass)
		vkDestroyRenderPass(dev, m_renderPass, nullptr);
	if (m_uboMapped)
		vkUnmapMemory(dev, m_uboMem);
	if (m_ubo)
		vkDestroyBuffer(dev, m_ubo, nullptr);
	if (m_uboMem)
		vkFreeMemory(dev, m_uboMem, nullptr);
	if (m_roadSampler)
		vkDestroySampler(dev, m_roadSampler, nullptr);
	if (m_dirtSampler)
		vkDestroySampler(dev, m_dirtSampler, nullptr);
	if (m_dirtView)
		vkDestroyImageView(dev, m_dirtView, nullptr);
	if (m_dirtImage)
		vkDestroyImage(dev, m_dirtImage, nullptr);
	if (m_dirtMem)
		vkFreeMemory(dev, m_dirtMem, nullptr);
	m_waterPipeline = VK_NULL_HANDLE;
	m_objectPipeline = VK_NULL_HANDLE;
	m_roadPipeline = VK_NULL_HANDLE;
	m_pipeline = VK_NULL_HANDLE;
	m_pipelineLayout = VK_NULL_HANDLE;
	m_dsl = VK_NULL_HANDLE;
	m_descPool = VK_NULL_HANDLE;
	m_renderPass = VK_NULL_HANDLE;
	m_ubo = VK_NULL_HANDLE;
	m_uboMem = VK_NULL_HANDLE;
	m_uboMapped = nullptr;
	m_roadSampler = VK_NULL_HANDLE;
	m_dirtSampler = VK_NULL_HANDLE;
	m_dirtView = VK_NULL_HANDLE;
	m_dirtImage = VK_NULL_HANDLE;
	m_dirtMem = VK_NULL_HANDLE;
	m_inited = m_pipelineReady = false;
}

bool MapViewport::ensureFramebuffer(VulkanHost &host, int w, int h)
{
	if (w < 1 || h < 1)
		return false;
	if (m_fbW == w && m_fbH == h && m_framebuffer)
		return true;

	destroyFramebuffer(host);
	VkDevice dev = host.device();

	auto makeImage = [&](VkFormat fmt, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImage &img,
						  VkDeviceMemory &mem, VkImageView &view) -> bool {
		VkImageCreateInfo ii = {};
		ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		ii.imageType = VK_IMAGE_TYPE_2D;
		ii.format = fmt;
		ii.extent = {(uint32_t)w, (uint32_t)h, 1};
		ii.mipLevels = 1;
		ii.arrayLayers = 1;
		ii.samples = VK_SAMPLE_COUNT_1_BIT;
		ii.tiling = VK_IMAGE_TILING_OPTIMAL;
		ii.usage = usage;
		ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (vkCreateImage(dev, &ii, nullptr, &img) != VK_SUCCESS)
			return false;
		VkMemoryRequirements req;
		vkGetImageMemoryRequirements(dev, img, &req);
		VkMemoryAllocateInfo ai = {};
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = findMemoryType(host, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (vkAllocateMemory(dev, &ai, nullptr, &mem) != VK_SUCCESS)
			return false;
		vkBindImageMemory(dev, img, mem, 0);
		VkImageViewCreateInfo vi = {};
		vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vi.image = img;
		vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vi.format = fmt;
		vi.subresourceRange.aspectMask = aspect;
		vi.subresourceRange.levelCount = 1;
		vi.subresourceRange.layerCount = 1;
		return vkCreateImageView(dev, &vi, nullptr, &view) == VK_SUCCESS;
	};

	if (!makeImage(VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, m_colorImage,
			m_colorMem, m_colorView))
		return false;
	if (!makeImage(VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT,
			m_depthImage, m_depthMem, m_depthView))
		return false;

	VkImageView atts[2] = {m_colorView, m_depthView};
	VkFramebufferCreateInfo fbi = {};
	fbi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbi.renderPass = m_renderPass;
	fbi.attachmentCount = 2;
	fbi.pAttachments = atts;
	fbi.width = (uint32_t)w;
	fbi.height = (uint32_t)h;
	fbi.layers = 1;
	if (vkCreateFramebuffer(dev, &fbi, nullptr, &m_framebuffer) != VK_SUCCESS)
		return false;

	m_imguiTex = ImGui_ImplVulkan_AddTexture(m_colorView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_fbW = w;
	m_fbH = h;
	return m_imguiTex != VK_NULL_HANDLE;
}

void MapViewport::constrainCenter()
{
	if (m_mapW <= 0 || m_mapH <= 0)
		return;
	if (m_centerCellX < 0.f)
		m_centerCellX = 0.f;
	if (m_centerCellY < 0.f)
		m_centerCellY = 0.f;
	if (m_centerCellX > (float)m_mapW)
		m_centerCellX = (float)m_mapW;
	if (m_centerCellY > (float)m_mapH)
		m_centerCellY = (float)m_mapH;
}

void MapViewport::scrollInView(float dxCells, float dyCells)
{
	m_centerCellX += dxCells;
	m_centerCellY += dyCells;
	constrainCenter();
}

void MapViewport::rotateCamera(float deltaAngle)
{
	m_cameraAngle += deltaAngle;
	const float pi = (float)M_PI;
	if (m_cameraAngle > pi)
		m_cameraAngle -= 2.f * pi;
	if (m_cameraAngle < -pi)
		m_cameraAngle += 2.f * pi;
}

void MapViewport::pitchCamera(float delta)
{
	m_fxPitch += delta;
	if (m_fxPitch < 0.2f)
		m_fxPitch = 0.2f;
	if (m_fxPitch > 2.0f)
		m_fxPitch = 2.0f;
}

void MapViewport::resetCamera()
{
	m_mouseWheelOffset = 0.f;
	m_cameraAngle = 0.f;
	m_fxPitch = 1.f;
	if (TheGlobalData)
	{
		m_cameraPitchDeg = TheGlobalData->m_cameraPitch;
		m_cameraYawDeg = TheGlobalData->m_cameraYaw;
		m_maxCameraHeight = TheGlobalData->m_maxCameraHeight;
	}
	else
	{
		m_cameraPitchDeg = 37.5f;
		m_cameraYawDeg = 0.f;
		m_maxCameraHeight = 310.f;
	}
	m_groundLevel = 10.f;
	m_camOffZ = m_groundLevel + m_maxCameraHeight;
	const float pitchRad = m_cameraPitchDeg * (float)M_PI / 180.f;
	const float yawRad = m_cameraYawDeg * (float)M_PI / 180.f;
	m_camOffY = -(m_camOffZ / tanf(pitchRad));
	m_camOffX = -(m_camOffY * tanf(yawRad));
}

void MapViewport::resetCamera(const MapDocument &doc)
{
	if (doc.isLoaded())
	{
		m_centerCellX = (float)doc.width() * 0.5f - (float)doc.borderSize();
		m_centerCellY = (float)doc.height() * 0.5f - (float)doc.borderSize();
		m_groundLevel = 16.f * MAP_HEIGHT_SCALE;
	}
	resetCamera();
	constrainCenter();
}

float MapViewport::currentLookDistance() const
{
	float eyeX, eyeY, eyeZ, tgtX, tgtY, tgtZ;
	buildCameraEyeTarget(eyeX, eyeY, eyeZ, tgtX, tgtY, tgtZ);
	const float dx = eyeX - tgtX, dy = eyeY - tgtY, dz = eyeZ - tgtZ;
	float d = sqrtf(dx * dx + dy * dy + dz * dz);
	if (d < 300.f)
		d = 300.f;
	return d;
}

void MapViewport::buildCameraEyeTarget(float &eyeX, float &eyeY, float &eyeZ, float &tgtX, float &tgtY,
	float &tgtZ) const
{
	/* Mirrors WbView3d::setupCamera (simplified, flat ground). */
	float zOffset = -m_mouseWheelOffset / 1200.f;
	float zoom = 1.f;
	if (zOffset != 0.f)
	{
		const float camLen = sqrtf(m_camOffX * m_camOffX + m_camOffY * m_camOffY + m_camOffZ * m_camOffZ);
		float zPos = (camLen - m_groundLevel) / camLen;
		float zAbs = zOffset + zPos;
		if (zAbs < 0.f)
			zAbs = -zAbs;
		if (zAbs < 0.01f)
			zAbs = 0.01f;
		if (zOffset > 0.f)
			zOffset *= zAbs;
		else if (zOffset < -0.3f)
			zOffset = -0.15f + zOffset * 0.5f;
		if (zOffset < -0.6f)
			zOffset = -0.3f + zOffset * 0.5f;
		zoom = zAbs;
	}

	const float posX = m_centerCellX * MAP_XY_FACTOR;
	const float posY = m_centerCellY * MAP_XY_FACTOR;
	const float posZ = 0.f;
	const float groundLevel = m_groundLevel;

	float sx = m_camOffX * zoom;
	float sy = m_camOffY * zoom;
	float sz = m_camOffZ * zoom;

	float factor = 1.f - (groundLevel / sz);
	if (factor < 0.05f)
		factor = 0.05f;

	const float ca = cosf(m_cameraAngle);
	const float sa = sinf(m_cameraAngle);
	float rx = sx * ca - sy * sa;
	float ry = sx * sa + sy * ca;
	rx *= factor;
	ry *= factor;
	sz *= factor;

	eyeX = posX + rx;
	eyeY = posY + ry;
	eyeZ = posZ + groundLevel + sz;
	tgtX = posX;
	tgtY = posY;
	tgtZ = posZ + groundLevel;

	/* FX pitch: scale look-down like WbView3d. */
	float height = eyeZ - tgtZ;
	height *= m_fxPitch;
	tgtZ = eyeZ - height;
}

void MapViewport::handleMapViewInput(const ImGuiIO &io, bool viewHovered)
{
	const float hyst = 3.f;
	const bool space = ImGui::IsKeyDown(ImGuiKey_Space);

	if (viewHovered && m_track == TrackNone)
	{
		if (io.MouseWheel != 0.f)
		{
			/* Win32 WM_MOUSEWHEEL zDelta is typically ±120 per notch. */
			m_mouseWheelOffset += io.MouseWheel * 120.f;
		}
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || (space && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))
		{
			m_track = TrackLeft;
			m_downMouseX = m_prevMouseX = io.MousePos.x;
			m_downMouseY = m_prevMouseY = io.MousePos.y;
			m_scrolling = false;
			m_downTime = ImGui::GetTime();
		}
		else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		{
			m_track = TrackRight;
			m_downMouseX = m_prevMouseX = io.MousePos.x;
			m_downMouseY = m_prevMouseY = io.MousePos.y;
			m_scrolling = false;
			m_downTime = ImGui::GetTime();
		}
		else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
		{
			m_track = TrackMiddle;
			m_downMouseX = m_prevMouseX = io.MousePos.x;
			m_downMouseY = m_prevMouseY = io.MousePos.y;
			m_scrolling = false;
			m_downTime = ImGui::GetTime();
		}
	}

	if (m_track == TrackMiddle)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			const float dx = io.MousePos.x - m_prevMouseX;
			const float factor = 0.01f;
			if (m_doPitch)
				pitchCamera(factor * (io.MousePos.y - m_prevMouseY));
			else
				rotateCamera(factor * dx);
			m_prevMouseX = io.MousePos.x;
			m_prevMouseY = io.MousePos.y;
		}
		else
		{
			const float mdx = io.MousePos.x - m_downMouseX;
			const float mdy = io.MousePos.y - m_downMouseY;
			const bool moved = (fabsf(mdx) > hyst || fabsf(mdy) > hyst);
			/* Quick click without move → setDefaultCamera (HandScrollTool). */
			if (!moved && (ImGui::GetTime() - m_downTime) < 0.5)
				resetCamera();
			m_track = TrackNone;
		}
		return;
	}

	if (m_track == TrackLeft || m_track == TrackRight)
	{
		const ImGuiMouseButton btn = (m_track == TrackLeft) ? ImGuiMouseButton_Left : ImGuiMouseButton_Right;
		if (ImGui::IsMouseDown(btn) || (m_track == TrackLeft && space && ImGui::IsMouseDown(ImGuiMouseButton_Left)))
		{
			const float dxPix = io.MousePos.x - m_prevMouseX;
			const float dyPix = io.MousePos.y - m_prevMouseY;
			if (!m_scrolling)
			{
				if (fabsf(io.MousePos.x - m_downMouseX) > hyst || fabsf(io.MousePos.y - m_downMouseY) > hyst)
					m_scrolling = true;
			}
			if (m_scrolling && (dxPix != 0.f || dyPix != 0.f))
			{
				/* Approximate viewToDocCoords delta on flat ground (sticky pan). */
				const float look = currentLookDistance();
				const float fovY = 50.f * (float)M_PI / 180.f;
				const float worldPerPixel = (2.f * look * tanf(fovY * 0.5f)) / (float)(m_fbH > 0 ? m_fbH : 720);
				const float ca = cosf(m_cameraAngle);
				const float sa = sinf(m_cameraAngle);
				/* Screen +X → camera right on XY; screen +Y (down) → toward camera on ground. */
				const float rightX = ca, rightY = sa;
				const float fwdX = -sa, fwdY = ca;
				const float wdx = (dxPix * rightX - dyPix * fwdX) * worldPerPixel;
				const float wdy = (dxPix * rightY - dyPix * fwdY) * worldPerPixel;
				float dxCells = wdx / MAP_XY_FACTOR;
				float dyCells = wdy / MAP_XY_FACTOR;
				if (fabsf(dxCells) > 1000.f)
					dxCells = 0.f;
				if (fabsf(dyCells) > 1000.f)
					dyCells = 0.f;
				scrollInView(dxCells, dyCells);
				m_prevMouseX = io.MousePos.x;
				m_prevMouseY = io.MousePos.y;
			}
		}
		else
		{
			m_track = TrackNone;
			m_scrolling = false;
		}
	}
}

bool MapViewport::rebuildMesh(VulkanHost &host, MapDocument &doc)
{
	destroyMesh(host);
	if (!doc.isLoaded() || !doc.heightMap())
		return false;

	WorldHeightMap *hm = doc.heightMap();
	const Int w = hm->getXExtent();
	const Int h = hm->getYExtent();
	const Int border = hm->getBorderSize();
	if (w < 2 || h < 2)
		return false;

	m_mapW = w;
	m_mapH = h;
	m_border = border;

	/* Sample ground height near playable center for camera. */
	{
		const Int cx = w / 2;
		const Int cy = h / 2;
		m_groundLevel = hm->getHeight(cx, cy) * MAP_HEIGHT_SCALE;
	}

	const bool useAtlas = doc.hasTerrainTextures();
	if (useAtlas)
	{
		std::vector<unsigned char> atlas;
		int aw = 0, ah = 0;
		if (doc.buildTerrainAtlas(atlas, aw, ah))
		{
			if (!uploadAlbedoTexture(host, atlas.data(), aw, ah))
				fprintf(stderr, "MapViewport: atlas upload failed\n");
			else
				fprintf(stderr, "MapViewport: terrain atlas %dx%d\n", aw, ah);
		}
		else
		{
			fprintf(stderr, "MapViewport: buildTerrainAtlas failed, using dirt\n");
		}
	}
	else
	{
		/* Restore procedural dirt for blank maps. */
		createDirtTexture(host);
		if (m_descSet != VK_NULL_HANDLE && m_dirtView != VK_NULL_HANDLE)
		{
			VkDescriptorImageInfo ii = {};
			ii.sampler = m_dirtSampler;
			ii.imageView = m_dirtView;
			ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			VkWriteDescriptorSet write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = m_descSet;
			write.dstBinding = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write.descriptorCount = 1;
			write.pImageInfo = &ii;
			vkUpdateDescriptorSets(host.device(), 1, &write, 0, nullptr);
		}
	}

	/*
	 * Per-cell quads (4 verts / cell) so each cell can use unique atlas UVs —
	 * same approach as HeightMapRenderObjClass::updateVB + getUVData.
	 */
	std::vector<Vertex> verts;
	std::vector<uint32_t> indices;
	const Int cellsX = w - 1;
	const Int cellsY = h - 1;
	verts.reserve((size_t)cellsX * (size_t)cellsY * 4 + 16);
	indices.reserve((size_t)cellsX * (size_t)cellsY * 6 + 48);

	for (Int y = 0; y < cellsY; ++y)
	{
		for (Int x = 0; x < cellsX; ++x)
		{
			float U[4] = {0, 1, 1, 0};
			float V[4] = {0, 0, 1, 1};
			float UA[4] = {0, 1, 1, 0};
			float VA[4] = {0, 0, 1, 1};
			UnsignedByte alpha[4] = {0, 0, 0, 0};
			Bool flipForBlend = false;
			if (useAtlas)
			{
				doc.cellUV(x, y, U, V);
				doc.cellBlendUV(x, y, UA, VA, alpha, &flipForBlend);
			}
			else
			{
				U[0] = (float)x * 0.25f;
				U[1] = (float)(x + 1) * 0.25f;
				U[2] = U[1];
				U[3] = U[0];
				V[0] = (float)(y + 1) * 0.25f;
				V[1] = V[0];
				V[2] = (float)y * 0.25f;
				V[3] = V[2];
				for (int i = 0; i < 4; ++i)
				{
					UA[i] = U[i];
					VA[i] = V[i];
				}
			}

			const float z00 = hm->getHeight(x, y) * MAP_HEIGHT_SCALE;
			const float z10 = hm->getHeight(x + 1, y) * MAP_HEIGHT_SCALE;
			const float z01 = hm->getHeight(x, y + 1) * MAP_HEIGHT_SCALE;
			const float z11 = hm->getHeight(x + 1, y + 1) * MAP_HEIGHT_SCALE;
			const float x0 = (x - border) * MAP_XY_FACTOR;
			const float x1 = (x + 1 - border) * MAP_XY_FACTOR;
			const float y0 = (y - border) * MAP_XY_FACTOR;
			const float y1 = (y + 1 - border) * MAP_XY_FACTOR;

			/* Corners TL,TR,BR,BL matching getUVData / getAlphaUVData order. */
			Vertex corners[4];
			corners[0] = {x0, y0, z00, U[0], V[0], UA[0], VA[0], 1.f, 1.f, 1.f, alpha[0] / 255.f};
			corners[1] = {x1, y0, z10, U[1], V[1], UA[1], VA[1], 1.f, 1.f, 1.f, alpha[1] / 255.f};
			corners[2] = {x1, y1, z11, U[2], V[2], UA[2], VA[2], 1.f, 1.f, 1.f, alpha[2] / 255.f};
			corners[3] = {x0, y1, z01, U[3], V[3], UA[3], VA[3], 1.f, 1.f, 1.f, alpha[3] / 255.f};

			/* FLIP_TRIANGLES — same rotation as HeightMapRenderObjClass::updateVB. */
			if (flipForBlend)
			{
				Vertex tmp = corners[0];
				corners[0] = corners[1];
				corners[1] = corners[2];
				corners[2] = corners[3];
				corners[3] = tmp;
			}

			const uint32_t base = (uint32_t)verts.size();
			verts.push_back(corners[0]);
			verts.push_back(corners[1]);
			verts.push_back(corners[2]);
			verts.push_back(corners[3]);
			indices.push_back(base + 0);
			indices.push_back(base + 3);
			indices.push_back(base + 1);
			indices.push_back(base + 1);
			indices.push_back(base + 3);
			indices.push_back(base + 2);
		}
	}
	m_indexCount = (uint32_t)indices.size();

	const float lift = 0.5f;
	const float halfW = 2.0f;
	auto addLine = [&](float x0, float y0, float x1, float y1, float z, float r, float g, float b) {
		float dx = x1 - x0, dy = y1 - y0;
		float len = sqrtf(dx * dx + dy * dy);
		if (len < 1e-3f)
			return;
		float nx = -dy / len * halfW, ny = dx / len * halfW;
		uint32_t base = (uint32_t)verts.size();
		Vertex v[4];
		for (int i = 0; i < 4; ++i)
		{
			v[i].u = v[i].v = 0;
			v[i].u2 = v[i].v2 = 0;
			v[i].r = r;
			v[i].g = g;
			v[i].b = b;
			v[i].blend = 0.f;
			v[i].pz = z + lift;
		}
		v[0].px = x0 - nx;
		v[0].py = y0 - ny;
		v[1].px = x0 + nx;
		v[1].py = y0 + ny;
		v[2].px = x1 - nx;
		v[2].py = y1 - ny;
		v[3].px = x1 + nx;
		v[3].py = y1 + ny;
		verts.push_back(v[0]);
		verts.push_back(v[1]);
		verts.push_back(v[2]);
		verts.push_back(v[3]);
		indices.push_back(base + 0);
		indices.push_back(base + 2);
		indices.push_back(base + 1);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
		indices.push_back(base + 3);
	};

	const float ox0 = -border * MAP_XY_FACTOR;
	const float oy0 = -border * MAP_XY_FACTOR;
	const float ox1 = (w - border) * MAP_XY_FACTOR;
	const float oy1 = (h - border) * MAP_XY_FACTOR;
	const float px0 = 0.f;
	const float py0 = 0.f;
	const float px1 = (w - 2 * border) * MAP_XY_FACTOR;
	const float py1 = (h - 2 * border) * MAP_XY_FACTOR;
	const float z = m_groundLevel;

	addLine(ox0, oy0, ox1, oy0, z, 0.f, 0.f, 1.f);
	addLine(ox1, oy0, ox1, oy1, z, 0.f, 0.f, 1.f);
	addLine(ox1, oy1, ox0, oy1, z, 0.f, 0.f, 1.f);
	addLine(ox0, oy1, ox0, oy0, z, 0.f, 0.f, 1.f);

	addLine(px0, py0, px1, py0, z, 1.f, 0.53f, 0.f);
	addLine(px1, py0, px1, py1, z, 1.f, 0.53f, 0.f);
	addLine(px1, py1, px0, py1, z, 1.f, 0.53f, 0.f);
	addLine(px0, py1, px0, py0, z, 1.f, 0.53f, 0.f);

	m_lineIndexOffset = m_indexCount;
	m_lineIndexCount = (uint32_t)indices.size() - m_indexCount;

	if (!uploadBuffer(host, m_vbo, m_vboMem, verts.data(), verts.size() * sizeof(Vertex),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
		return false;
	if (!uploadBuffer(host, m_ibo, m_iboMem, indices.data(), indices.size() * sizeof(uint32_t),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
		return false;

	rebuildRoads(host, doc);
	rebuildObjects(host, doc);
	rebuildWater(host, doc);

	resetCamera(doc);
	m_meshReady = true;
	return true;
}

bool MapViewport::loadRoadRgba(const char *texName, std::vector<unsigned char> &rgba, int &w, int &h)
{
	if (!texName || !texName[0] || !TheFileSystem)
		return false;

	char base[256];
	strncpy(base, texName, sizeof(base) - 1);
	base[sizeof(base) - 1] = '\0';
	char *dot = strrchr(base, '.');
	if (dot)
		*dot = '\0';
	/* Lowercase stem for Art/png / Art/Textures fallbacks. */
	char lower[256];
	for (size_t i = 0; i < sizeof(lower); ++i)
	{
		lower[i] = (char)tolower((unsigned char)base[i]);
		if (base[i] == '\0')
			break;
	}

	const char *candidates[] = {
		texName,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
	};
	char pathArtTex[320], pathArtPng[320], pathArtDds[320], pathData[320], pathDiskPng[320];
	snprintf(pathArtTex, sizeof(pathArtTex), "Art\\Textures\\%s", texName);
	snprintf(pathArtDds, sizeof(pathArtDds), "Art\\Textures\\%s.dds", lower);
	snprintf(pathArtPng, sizeof(pathArtPng), "Art\\png\\%s.png", lower);
	snprintf(pathData, sizeof(pathData), "Data\\Textures\\%s", texName);
	snprintf(pathDiskPng, sizeof(pathDiskPng), "Art/png/%s.png", lower);
	candidates[1] = pathArtTex;
	candidates[2] = pathArtDds;
	candidates[3] = pathArtPng;
	candidates[4] = pathData;
	candidates[5] = pathDiskPng;

	auto tryDecode = [&](const unsigned char *data, Int sz) -> bool {
		if (DdsDecoder::isDds(data, (size_t)sz))
		{
			DdsDecoder::Image image;
			if (!DdsDecoder::decode(data, (size_t)sz, image))
			{
				fprintf(stderr, "MapViewport: unsupported or invalid DDS texture (%d bytes)\n", sz);
				return false;
			}
			w = image.width;
			h = image.height;
			rgba.swap(image.rgba);
			fprintf(stderr, "MapViewport: decoded DDS %dx%d %s\n",
				w, h, DdsDecoder::formatName(image.format));
			return true;
		}
		int channels = 0;
		unsigned char *pixels = stbi_load_from_memory(data, sz, &w, &h, &channels, 4);
		if (!pixels)
			return false;
		rgba.assign(pixels, pixels + (size_t)w * (size_t)h * 4);
		stbi_image_free(pixels);
		return true;
	};

	for (const char *path : candidates)
	{
		if (!path)
			continue;
		File *f = TheFileSystem->openFile(path, File::READ | File::BINARY);
		if (f)
		{
			const Int sz = f->size();
			if (sz > 0)
			{
				f->seek(0, File::START);
				std::vector<unsigned char> fileBytes((size_t)sz);
				const Int got = f->read(fileBytes.data(), sz);
				f->close();
				if (got == sz && tryDecode(fileBytes.data(), sz))
					return true;
			}
			else
				f->close();
		}
		/* Direct disk fallback (Art/png/... on Linux). */
		FILE *fp = fopen(path, "rb");
		if (!fp)
			continue;
		fseek(fp, 0, SEEK_END);
		long fsz = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		if (fsz <= 0)
		{
			fclose(fp);
			continue;
		}
		std::vector<unsigned char> fileBytes((size_t)fsz);
		const size_t got = fread(fileBytes.data(), 1, (size_t)fsz, fp);
		fclose(fp);
		if (got == (size_t)fsz && tryDecode(fileBytes.data(), (Int)fsz))
			return true;
	}
	return false;
}

bool MapViewport::uploadRoadTexture(VulkanHost &host, RoadBatch &batch, const unsigned char *rgba, int w, int h)
{
	if (!rgba || w < 1 || h < 1)
		return false;
	VkDevice dev = host.device();
	const VkDeviceSize size = (VkDeviceSize)w * (VkDeviceSize)h * 4;

	VkBuffer staging = VK_NULL_HANDLE;
	VkDeviceMemory stagingMem = VK_NULL_HANDLE;
	{
		VkBufferCreateInfo bi = {};
		bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bi.size = size;
		bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		vkCreateBuffer(dev, &bi, nullptr, &staging);
		VkMemoryRequirements req;
		vkGetBufferMemoryRequirements(dev, staging, &req);
		VkMemoryAllocateInfo ai = {};
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = findMemoryType(host, req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		vkAllocateMemory(dev, &ai, nullptr, &stagingMem);
		vkBindBufferMemory(dev, staging, stagingMem, 0);
		void *mapped = nullptr;
		vkMapMemory(dev, stagingMem, 0, size, 0, &mapped);
		memcpy(mapped, rgba, (size_t)size);
		vkUnmapMemory(dev, stagingMem);
	}

	{
		VkImageCreateInfo ii = {};
		ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		ii.imageType = VK_IMAGE_TYPE_2D;
		ii.format = VK_FORMAT_R8G8B8A8_UNORM;
		ii.extent = {(uint32_t)w, (uint32_t)h, 1};
		ii.mipLevels = 1;
		ii.arrayLayers = 1;
		ii.samples = VK_SAMPLE_COUNT_1_BIT;
		ii.tiling = VK_IMAGE_TILING_OPTIMAL;
		ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		vkCreateImage(dev, &ii, nullptr, &batch.image);
		VkMemoryRequirements req;
		vkGetImageMemoryRequirements(dev, batch.image, &req);
		VkMemoryAllocateInfo ai = {};
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = findMemoryType(host, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkAllocateMemory(dev, &ai, nullptr, &batch.mem);
		vkBindImageMemory(dev, batch.image, batch.mem, 0);
	}

	VkCommandPool pool = host.uploadCommandPool();
	VkCommandBuffer cmd;
	VkCommandBufferAllocateInfo cai = {};
	cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cai.commandPool = pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	vkAllocateCommandBuffers(dev, &cai, &cmd);
	VkCommandBufferBeginInfo bi = {};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &bi);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = batch.image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
		nullptr, 1, &barrier);

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
	vkCmdCopyBufferToImage(cmd, staging, batch.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
		nullptr, 1, &barrier);
	vkEndCommandBuffer(cmd);
	VkSubmitInfo si = {};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	vkQueueSubmit(host.queue(), 1, &si, VK_NULL_HANDLE);
	vkQueueWaitIdle(host.queue());
	vkFreeCommandBuffers(dev, pool, 1, &cmd);
	vkDestroyBuffer(dev, staging, nullptr);
	vkFreeMemory(dev, stagingMem, nullptr);

	VkImageViewCreateInfo vi = {};
	vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vi.image = batch.image;
	vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vi.format = VK_FORMAT_R8G8B8A8_UNORM;
	vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vi.subresourceRange.levelCount = 1;
	vi.subresourceRange.layerCount = 1;
	vkCreateImageView(dev, &vi, nullptr, &batch.view);

	if (m_roadSampler == VK_NULL_HANDLE)
	{
		VkSamplerCreateInfo sci = {};
		sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sci.magFilter = VK_FILTER_LINEAR;
		sci.minFilter = VK_FILTER_LINEAR;
		sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		vkCreateSampler(dev, &sci, nullptr, &m_roadSampler);
	}

	VkDescriptorSetAllocateInfo dai = {};
	dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dai.descriptorPool = m_descPool;
	dai.descriptorSetCount = 1;
	dai.pSetLayouts = &m_dsl;
	if (vkAllocateDescriptorSets(dev, &dai, &batch.descSet) != VK_SUCCESS)
		return false;

	VkDescriptorBufferInfo ubi = {};
	ubi.buffer = m_ubo;
	ubi.range = sizeof(float) * 20;
	VkDescriptorImageInfo ii = {};
	ii.sampler = m_roadSampler;
	ii.imageView = batch.view;
	ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	VkWriteDescriptorSet writes[2] = {};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = batch.descSet;
	writes[0].dstBinding = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[0].descriptorCount = 1;
	writes[0].pBufferInfo = &ubi;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = batch.descSet;
	writes[1].dstBinding = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].descriptorCount = 1;
	writes[1].pImageInfo = &ii;
	vkUpdateDescriptorSets(dev, 2, writes, 0, nullptr);
	return true;
}

bool MapViewport::uploadObjectTexture(VulkanHost &host, ObjectBatch &batch, const unsigned char *rgba, int w, int h)
{
	RoadBatch tmp;
	if (!uploadRoadTexture(host, tmp, rgba, w, h))
		return false;
	batch.image = tmp.image;
	batch.mem = tmp.mem;
	batch.view = tmp.view;
	batch.descSet = tmp.descSet;
	return true;
}

bool MapViewport::rebuildObjects(VulkanHost &host, MapDocument &doc)
{
	destroyObjects(host);
	if (!m_showModels || doc.objects().empty() || !doc.heightMap())
		return true;

	WorldHeightMap *hm = doc.heightMap();
	const Int border = hm->getBorderSize();
	const Int extentX = hm->getXExtent();
	const Int extentY = hm->getYExtent();

	auto sampleZ = [&](float worldX, float worldY) -> float {
		Int iX = (Int)(worldX / MAP_XY_FACTOR) + border;
		Int iY = (Int)(worldY / MAP_XY_FACTOR) + border;
		if (iX < 0)
			iX = 0;
		if (iY < 0)
			iY = 0;
		if (iX >= extentX - 1)
			iX = extentX - 2;
		if (iY >= extentY - 1)
			iY = extentY - 2;
		if (iX < 0 || iY < 0)
			return 0.f;
		return hm->getHeight(iX, iY) * MAP_HEIGHT_SCALE;
	};

	ObjectMeshCache cache;
	std::vector<ObjectDrawInstance> instances;
	cache.build(doc.objects(), instances);
	if (instances.empty() || cache.models().empty())
		return true;

	std::vector<RoadVertex> verts;
	std::vector<uint32_t> indices;

	/* Group parts by texture across all instances. */
	struct BatchedPart
	{
		const ObjectBakedPart *part;
		float x, y, z, angle;
	};
	std::map<std::string, std::vector<BatchedPart>> batches;

	for (const ObjectDrawInstance &inst : instances)
	{
		if (inst.modelIndex < 0 || inst.modelIndex >= (int)cache.models().size())
			continue;
		const ObjectBakedModel &model = cache.models()[(size_t)inst.modelIndex];
		const float groundZ = sampleZ(inst.x, inst.y);
		const float z = inst.z + groundZ;
		for (const ObjectBakedPart &part : model.parts)
		{
			std::string key = part.textureName.empty() ? std::string("__gray__") : part.textureName;
			BatchedPart bp;
			bp.part = &part;
			bp.x = inst.x;
			bp.y = inst.y;
			bp.z = z;
			bp.angle = inst.angle;
			batches[key].push_back(bp);
		}
	}

	auto appendTransformed = [&](const ObjectBakedPart &part, float x, float y, float z, float angle) {
		const float c = cosf(angle);
		const float s = sinf(angle);
		const uint32_t base = (uint32_t)verts.size();
		for (const ObjectMeshVertex &v : part.verts)
		{
			const float lx = v.px;
			const float ly = v.py;
			const float lz = v.pz;
			RoadVertex out;
			out.px = x + c * lx - s * ly;
			out.py = y + s * lx + c * ly;
			out.pz = z + lz;
			out.u = v.u;
			out.v = v.v;
			verts.push_back(out);
		}
		for (uint32_t idx : part.indices)
			indices.push_back(base + idx);
	};

	for (auto &kv : batches)
	{
		ObjectBatch batch;
		batch.texKey = kv.first;
		batch.indexOffset = (uint32_t)indices.size();

		std::vector<unsigned char> rgba;
		int tw = 0, th = 0;
		bool haveTex = false;
		if (kv.first != "__gray__")
			haveTex = loadRoadRgba(kv.first.c_str(), rgba, tw, th);
		if (!haveTex)
		{
			if (kv.first != "__gray__")
				fprintf(stderr, "MapViewport: missing object texture '%s' (gray fallback)\n", kv.first.c_str());
			tw = th = 4;
			rgba.assign(64, 180);
			for (int i = 3; i < 64; i += 4)
				rgba[(size_t)i] = 255;
		}
		if (!uploadObjectTexture(host, batch, rgba.data(), tw, th))
		{
			fprintf(stderr, "MapViewport: object texture upload failed for %s\n", kv.first.c_str());
			continue;
		}

		for (const BatchedPart &bp : kv.second)
			appendTransformed(*bp.part, bp.x, bp.y, bp.z, bp.angle);

		batch.indexCount = (uint32_t)indices.size() - batch.indexOffset;
		if (batch.indexCount > 0)
			m_objectBatches.push_back(batch);
		else
		{
			VkDevice dev = host.device();
			if (batch.view)
				vkDestroyImageView(dev, batch.view, nullptr);
			if (batch.image)
				vkDestroyImage(dev, batch.image, nullptr);
			if (batch.mem)
				vkFreeMemory(dev, batch.mem, nullptr);
		}
	}

	if (verts.empty() || indices.empty())
		return true;

	if (!uploadBuffer(host, m_objectVbo, m_objectVboMem, verts.data(), verts.size() * sizeof(RoadVertex),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
		return false;
	if (!uploadBuffer(host, m_objectIbo, m_objectIboMem, indices.data(), indices.size() * sizeof(uint32_t),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
		return false;

	fprintf(stderr, "MapViewport: objects ready (%zu batches, %zu verts)\n", m_objectBatches.size(), verts.size());
	return true;
}

bool MapViewport::rebuildRoads(VulkanHost &host, MapDocument &doc)
{
	destroyRoads(host);
	if (doc.roads().empty() || !doc.heightMap())
		return true;

	WorldHeightMap *hm = doc.heightMap();
	const Int border = hm->getBorderSize();
	const Int extentX = hm->getXExtent();
	const Int extentY = hm->getYExtent();
	const float FLOAT_AMOUNT = MAP_HEIGHT_SCALE / 8.f;

	auto maxCellZ = [&](float worldX, float worldY) -> float {
		Int iX = (Int)(worldX / MAP_XY_FACTOR) + border;
		Int iY = (Int)(worldY / MAP_XY_FACTOR) + border;
		if (iX < 0)
			iX = 0;
		if (iY < 0)
			iY = 0;
		if (iX >= extentX - 1)
			iX = extentX - 2;
		if (iY >= extentY - 1)
			iY = extentY - 2;
		float z = hm->getHeight(iX, iY);
		z = fmaxf(z, hm->getHeight(iX + 1, iY));
		z = fmaxf(z, hm->getHeight(iX, iY + 1));
		z = fmaxf(z, hm->getHeight(iX + 1, iY + 1));
		return z * MAP_HEIGHT_SCALE;
	};

	/* Expand straights with curve/miter joins (W3DRoadBuffer::insertCurveSegments). */
	std::vector<RoadDrawPiece> pieces;
	buildRoadDrawPieces(doc.roads(), pieces);
	if (pieces.empty())
		return true;

	std::map<std::string, std::vector<const RoadDrawPiece *>> byType;
	for (const RoadDrawPiece &p : pieces)
		byType[p.typeName].push_back(&p);

	std::vector<RoadVertex> verts;
	std::vector<uint32_t> indices;
	verts.reserve(pieces.size() * 64);
	indices.reserve(pieces.size() * 96);

	auto appendStrip = [&](float blx, float bly, float brx, float bry, float tlx, float tly, float trx, float try_,
						   float locX, float locY, float roadNx, float roadNy, float roadVx, float roadVy,
						   float uOffset, float vOffset, float uScale, float vScale) {
		/* Length for subdivision: match loadFloat4PtSection (roadVector length). */
		float len = sqrtf(roadVx * roadVx + roadVy * roadVy);
		if (len < 1e-3f)
		{
			const float dx = brx - blx;
			const float dy = bry - bly;
			len = sqrtf(dx * dx + dy * dy);
		}
		if (len < 1e-3f)
		{
			const float dx = trx - tlx;
			const float dy = try_ - tly;
			len = sqrtf(dx * dx + dy * dy);
		}
		/* Original: uCount = roadLen/MAP_XY_FACTOR + 1, columns at i = 0 .. uCount-1 only (t in [0,1]). */
		Int uCount = (Int)(len / MAP_XY_FACTOR) + 1;
		if (uCount < 2)
			uCount = 2;
		const Int vCount = 2;
		float rNx = roadNx, rNy = roadNy;
		float rVx = roadVx, rVy = roadVy;
		float nLen = sqrtf(rNx * rNx + rNy * rNy);
		float vLen = sqrtf(rVx * rVx + rVy * rVy);
		if (nLen > 1e-6f)
		{
			rNx /= nLen;
			rNy /= nLen;
		}
		if (vLen > 1e-6f)
		{
			rVx /= vLen;
			rVy /= vLen;
		}

		std::vector<RoadVertex> colVerts;
		colVerts.reserve((size_t)uCount * 2);
		for (Int i = 0; i < uCount; ++i)
		{
			const float t = (uCount <= 1) ? 0.f : (float)i / (float)(uCount - 1);
			float maxZ = 0.f;
			RoadVertex row[2];
			for (Int j = 0; j < vCount; ++j)
			{
				const float s = (float)j / (float)(vCount - 1);
				/* Same bilinear as W3DRoadBuffer::loadFloat4PtSection after uVector2 fixup. */
				const float x = blx * (1 - t) * (1 - s) + brx * t * (1 - s) + tlx * (1 - t) * s + trx * t * s;
				const float y = bly * (1 - t) * (1 - s) + bry * t * (1 - s) + tly * (1 - t) * s + try_ * t * s;
				const float z = maxCellZ(x, y);
				if (j == 0 || z > maxZ)
					maxZ = z;
				row[j].px = x;
				row[j].py = y;
				row[j].pz = 0.f;
				const float along = (x - locX) * rVx + (y - locY) * rVy;
				const float across = (x - locX) * rNx + (y - locY) * rNy;
				row[j].u = uOffset + along / (uScale * 4.f);
				row[j].v = vOffset - across / (vScale * 4.f);
			}
			row[0].pz = maxZ + FLOAT_AMOUNT;
			row[1].pz = maxZ + FLOAT_AMOUNT;
			colVerts.push_back(row[0]);
			colVerts.push_back(row[1]);
		}
		for (Int i = 0; i < uCount - 1; ++i)
		{
			const uint32_t base = (uint32_t)verts.size() + (uint32_t)i * 2;
			/* Winding matches original collapsed strip (top, bottom, next). */
			indices.push_back(base + 1);
			indices.push_back(base + 0);
			indices.push_back(base + 2);
			indices.push_back(base + 1);
			indices.push_back(base + 2);
			indices.push_back(base + 3);
		}
		verts.insert(verts.end(), colVerts.begin(), colVerts.end());
	};

	for (auto &kv : byType)
	{
		const std::string &typeName = kv.first;
		/*
		 * W3DRoadBuffer::loadRoadsInVertexAndIndexBuffers draws pieces in
		 * TCorner enum order. Roads have alpha blending and no depth writes,
		 * therefore this order is part of the visual result at intersections.
		 * The topology array itself is insertion-ordered (joins precede curves),
		 * so reproduce the original ordering explicitly.
		 */
		auto roadDrawOrder = [](RoadDrawPiece::Kind kind) -> int {
			switch (kind)
			{
			case RoadDrawPiece::Straight: return 0;   /* SEGMENT */
			case RoadDrawPiece::Curve: return 1;      /* CURVE */
			case RoadDrawPiece::Tee: return 2;        /* TEE */
			case RoadDrawPiece::FourWay: return 3;    /* FOUR_WAY */
			case RoadDrawPiece::YJoin: return 4;      /* THREE_WAY_Y */
			case RoadDrawPiece::HJoin: return 5;      /* THREE_WAY_H */
			case RoadDrawPiece::HJoinFlip: return 6;  /* THREE_WAY_H_FLIP */
			case RoadDrawPiece::AlphaJoin: return 7;  /* ALPHA_JOIN */
			default: return 8;
			}
		};
		std::stable_sort(kv.second.begin(), kv.second.end(),
			[&](const RoadDrawPiece *a, const RoadDrawPiece *b) {
				return roadDrawOrder(a->kind) < roadDrawOrder(b->kind);
			});

		AsciiString texFile = "TRTwoLane.tga";
		if (TheTerrainRoads)
		{
			TerrainRoadType *road = TheTerrainRoads->findRoad(AsciiString(typeName.c_str()));
			if (road)
				texFile = road->getTexture();
		}

		RoadBatch batch;
		batch.texKey = typeName;
		batch.indexOffset = (uint32_t)indices.size();

		std::vector<unsigned char> rgba;
		int tw = 0, th = 0;
		if (!loadRoadRgba(texFile.str(), rgba, tw, th))
		{
			fprintf(stderr, "MapViewport: road texture missing for %s (%s)\n", typeName.c_str(), texFile.str());
			continue;
		}
		if (!uploadRoadTexture(host, batch, rgba.data(), tw, th))
		{
			fprintf(stderr, "MapViewport: road texture upload failed for %s\n", typeName.c_str());
			continue;
		}

		for (const RoadDrawPiece *piece : kv.second)
		{
			const float scale = piece->scale;
			const float widthInTex = piece->widthInTex;
			const float halfW = scale * widthInTex * 0.5f;

			if (piece->kind == RoadDrawPiece::Curve)
			{
				/* W3DRoadBuffer::loadCurve — atlas corner strip + tweaked quad. */
				float uOffset = 4.f / 512.f;
				float vOffset = 255.f / 512.f;
				const bool tight = (piece->curveRadius < 1.f);
				if (tight)
					vOffset = 425.f / 512.f;

				float dx = piece->x1 - piece->x0;
				float dy = piece->y1 - piece->y0;
				float len = sqrtf(dx * dx + dy * dy);
				if (len < 1e-4f)
					continue;
				const float nxl = -dy / len, nyl = dx / len;
				const float vxl = dx / len, vyl = dy / len;
				float blx, bly, brx, bry, tlx, tly, trx, try_, rNx, rNy, rVx, rVy;
				if (tight)
				{
					blx = piece->x0 - nxl * fabsf(halfW);
					bly = piece->y0 - nyl * fabsf(halfW);
					brx = blx + vxl * scale * 0.5f;
					bry = bly + vyl * scale * 0.5f;
					trx = brx + 2.f * nxl * fabsf(halfW);
					try_ = bry + 2.f * nyl * fabsf(halfW);
					tlx = blx + 2.f * nxl * fabsf(halfW);
					tly = bly + 2.f * nyl * fabsf(halfW);
					brx += vxl * scale * 0.1f + nxl * fabsf(halfW) * 0.2f;
					bry += vyl * scale * 0.1f + nyl * fabsf(halfW) * 0.2f;
					blx -= nxl * fabsf(halfW) * 0.1f + vxl * scale * 0.02f;
					bly -= nyl * fabsf(halfW) * 0.1f + vyl * scale * 0.02f;
					tlx -= vxl * scale * 0.02f;
					tly -= vyl * scale * 0.02f;
					trx -= vxl * scale * 0.4f;
					try_ -= vyl * scale * 0.4f;
					trx += nxl * fabsf(halfW) * 0.2f;
					try_ += nyl * fabsf(halfW) * 0.2f;
					rNx = nxl * fabsf(halfW);
					rNy = nyl * fabsf(halfW);
					rVx = vxl * scale;
					rVy = vyl * scale;
				}
				else
				{
					blx = piece->x0 - nxl * fabsf(halfW);
					bly = piece->y0 - nyl * fabsf(halfW);
					brx = blx + vxl * scale;
					bry = bly + vyl * scale;
					trx = brx + 2.f * nxl * fabsf(halfW);
					try_ = bry + 2.f * nyl * fabsf(halfW);
					tlx = blx + 2.f * nxl * fabsf(halfW);
					tly = bly + 2.f * nyl * fabsf(halfW);
					brx += vxl * scale * 0.1f + nxl * fabsf(halfW) * 0.4f;
					bry += vyl * scale * 0.1f + nyl * fabsf(halfW) * 0.4f;
					blx -= nxl * fabsf(halfW) * 0.2f + vxl * scale * 0.02f;
					bly -= nyl * fabsf(halfW) * 0.2f + vyl * scale * 0.02f;
					tlx -= vxl * scale * 0.02f;
					tly -= vyl * scale * 0.02f;
					trx -= vxl * scale * 0.4f;
					try_ -= vyl * scale * 0.4f;
					trx += nxl * fabsf(halfW) * 0.4f;
					try_ += nyl * fabsf(halfW) * 0.4f;
					rNx = nxl * fabsf(halfW);
					rNy = nyl * fabsf(halfW);
					rVx = vxl * scale;
					rVy = vyl * scale;
				}

				appendStrip(blx, bly, brx, bry, tlx, tly, trx, try_, piece->x0, piece->y0, rNx, rNy, rVx, rVy, uOffset,
					vOffset, scale, scale);
			}
			else if (piece->kind == RoadDrawPiece::Tee || piece->kind == RoadDrawPiece::FourWay ||
				piece->kind == RoadDrawPiece::YJoin || piece->kind == RoadDrawPiece::HJoin ||
				piece->kind == RoadDrawPiece::HJoinFlip || piece->kind == RoadDrawPiece::AlphaJoin)
			{
				/* Atlas join pieces — loadTee / loadY / loadH / loadAlphaJoin. */
				float dx = piece->x1 - piece->x0;
				float dy = piece->y1 - piece->y0;
				float len = sqrtf(dx * dx + dy * dy);
				float vxl = 1.f, vyl = 0.f, nxl = 0.f, nyl = 1.f;
				/* Match W3DRoadBuffer::loadTee/loadY/loadH/loadAlphaJoin:
				 * only use the fallback when both components are shorter than
				 * MIN_ROAD_SEGMENT, not when the vector's Euclidean length is. */
				if (!(fabsf(dx) < 0.25f && fabsf(dy) < 0.25f)
					&& len > 1e-6f)
				{
					vxl = dx / len;
					vyl = dy / len;
					nxl = -vyl;
					nyl = vxl;
				}
				float uOffset = 425.f / 512.f, vOffset = 255.f / 512.f;
				float blx, bly, brx, bry, tlx, tly, trx, try_, rNx, rNy, rVx, rVy;

				if (piece->kind == RoadDrawPiece::FourWay)
				{
					vOffset = 425.f / 512.f;
					const float teeFactor = scale * 1.03f / 2.f;
					const float left = widthInTex * scale / 2.f;
					rNx = nxl * teeFactor;
					rNy = nyl * teeFactor;
					rVx = vxl * (left + teeFactor);
					rVy = vyl * (left + teeFactor);
					const float leftCenterX = piece->x0 - vxl * left;
					const float leftCenterY = piece->y0 - vyl * left;
					blx = leftCenterX - rNx;
					bly = leftCenterY - rNy;
					brx = blx + rVx;
					bry = bly + rVy;
					trx = brx + 2.f * rNx;
					try_ = bry + 2.f * rNy;
					tlx = blx + 2.f * rNx;
					tly = bly + 2.f * rNy;
					appendStrip(blx, bly, brx, bry, tlx, tly, trx, try_, piece->x0, piece->y0, rNx, rNy, rVx, rVy,
						uOffset, vOffset, scale, scale);
				}
				else if (piece->kind == RoadDrawPiece::Tee)
				{
					const float teeFactor = scale * 1.03f / 2.f;
					const float left = widthInTex * scale / 2.f;
					rNx = nxl * teeFactor;
					rNy = nyl * teeFactor;
					rVx = vxl * (left + teeFactor);
					rVy = vyl * (left + teeFactor);
					const float leftCenterX = piece->x0 - vxl * left;
					const float leftCenterY = piece->y0 - vyl * left;
					blx = leftCenterX - rNx;
					bly = leftCenterY - rNy;
					brx = blx + rVx;
					bry = bly + rVy;
					trx = brx + 2.f * rNx;
					try_ = bry + 2.f * rNy;
					tlx = blx + 2.f * rNx;
					tly = bly + 2.f * rNy;
					appendStrip(blx, bly, brx, bry, tlx, tly, trx, try_, piece->x0, piece->y0, rNx, rNy, rVx, rVy,
						uOffset, vOffset, scale, scale);
				}
				else if (piece->kind == RoadDrawPiece::YJoin)
				{
					uOffset = 255.f / 512.f;
					vOffset = 226.f / 512.f;
					rVx = vxl * scale * 1.59f;
					rVy = vyl * scale * 1.59f;
					rNx = nxl * scale;
					rNy = nyl * scale;
					tlx = piece->x0 + nxl * scale * 0.29f - rVx * 0.5f;
					tly = piece->y0 + nyl * scale * 0.29f - rVy * 0.5f;
					blx = tlx - nxl * scale * 1.08f;
					bly = tly - nyl * scale * 1.08f;
					brx = blx + rVx;
					bry = bly + rVy;
					trx = tlx + rVx;
					try_ = tly + rVy;
					appendStrip(blx, bly, brx, bry, tlx, tly, trx, try_, piece->x0, piece->y0, rNx, rNy, rVx, rVy,
						uOffset, vOffset, scale, scale);
				}
				else if (piece->kind == RoadDrawPiece::HJoin || piece->kind == RoadDrawPiece::HJoinFlip)
				{
					const bool flip = (piece->kind == RoadDrawPiece::HJoinFlip);
					uOffset = 202.f / 512.f;
					vOffset = 364.f / 512.f;
					rVx = vxl * scale;
					rVy = vyl * scale;
					rNx = nxl * scale * 1.35f;
					rNy = nyl * scale * 1.35f;
					if (flip)
					{
						blx = piece->x0 - rNx * 0.20f - rVx * widthInTex / 2.f;
						bly = piece->y0 - rNy * 0.20f - rVy * widthInTex / 2.f;
					}
					else
					{
						blx = piece->x0 - rNx * 0.80f - rVx * widthInTex / 2.f;
						bly = piece->y0 - rNy * 0.80f - rVy * widthInTex / 2.f;
					}
					const float wx = rVx * widthInTex / 2.f + rVx * 1.2f;
					const float wy = rVy * widthInTex / 2.f + rVy * 1.2f;
					brx = blx + wx;
					bry = bly + wy;
					trx = brx + rNx;
					try_ = bry + rNy;
					tlx = blx + rNx;
					tly = bly + rNy;
					if (flip)
					{
						rNx = -rNx;
						rNy = -rNy;
					}
					appendStrip(blx, bly, brx, bry, tlx, tly, trx, try_, piece->x0, piece->y0, rNx, rNy, rVx, rVy,
						uOffset, vOffset, scale, scale);
				}
				else /* AlphaJoin */
				{
					uOffset = 106.f / 512.f;
					vOffset = 425.f / 512.f;
					rVx = vxl * scale * 48.f / 128.f;
					rVy = vyl * scale * 48.f / 128.f;
					rNx = nxl * widthInTex * (1.f + 8.f / 128.f);
					rNy = nyl * widthInTex * (1.f + 8.f / 128.f);
					tlx = piece->x0 + rNx * 0.5f - rVx * 0.65f;
					tly = piece->y0 + rNy * 0.5f - rVy * 0.65f;
					blx = tlx - rNx;
					bly = tly - rNy;
					brx = blx + rVx;
					bry = bly + rVy;
					trx = tlx + rVx;
					try_ = tly + rVy;
					appendStrip(blx, bly, brx, bry, tlx, tly, trx, try_, piece->x0, piece->y0, rNx, rNy, rVx, rVy,
						uOffset, vOffset, scale, widthInTex);
				}
			}
			else
			{
				const float uOffset = 0.f;
				const float vOffset = 85.f / 512.f;
				float dx = piece->x1 - piece->x0;
				float dy = piece->y1 - piece->y0;
				/*
				 * Original preloadRoadSegment projects UVs using a normal
				 * derived from the centerline. The top/bottom corners are
				 * mitered positions only. Using (top-loc) as the UV normal
				 * shears the atlas at every bend/intersection.
				 */
				float rNx = -dy;
				float rNy = dx;
				appendStrip(piece->b0x, piece->b0y, piece->b1x, piece->b1y, piece->t0x, piece->t0y, piece->t1x, piece->t1y,
					piece->x0, piece->y0, rNx, rNy, dx, dy, uOffset, vOffset, scale, scale);
			}
		}

		batch.indexCount = (uint32_t)indices.size() - batch.indexOffset;
		if (batch.indexCount > 0)
			m_roadBatches.push_back(batch);
		else
		{
			VkDevice dev = host.device();
			if (batch.view)
				vkDestroyImageView(dev, batch.view, nullptr);
			if (batch.image)
				vkDestroyImage(dev, batch.image, nullptr);
			if (batch.mem)
				vkFreeMemory(dev, batch.mem, nullptr);
		}
	}

	if (verts.empty() || indices.empty())
		return true;

	if (!uploadBuffer(host, m_roadVbo, m_roadVboMem, verts.data(), verts.size() * sizeof(RoadVertex),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
		return false;
	if (!uploadBuffer(host, m_roadIbo, m_roadIboMem, indices.data(), indices.size() * sizeof(uint32_t),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
		return false;

	fprintf(stderr, "MapViewport: roads ready (%zu batches, %zu verts)\n", m_roadBatches.size(), verts.size());
	return true;
}

void MapViewport::render(VulkanHost &host, int fbW, int fbH)
{
	if (!m_inited || !m_meshReady || !m_pipelineReady)
		return;
	if (!ensureFramebuffer(host, fbW, fbH))
		return;

	/* Camera: WbView3d::setupCamera */
	float eyeX, eyeY, eyeZ, tgtX, tgtY, tgtZ;
	buildCameraEyeTarget(eyeX, eyeY, eyeZ, tgtX, tgtY, tgtZ);
	const float look = currentLookDistance();
	Mat4 proj = Mat4::perspective(50.f, (float)fbW / (float)fbH, look / 200.f, look * 3.f);
	Mat4 view = Mat4::lookAt(eyeX, eyeY, eyeZ, tgtX, tgtY, tgtZ, 0.f, 0.f, 1.f);
	Mat4 mvp = proj * view;
	float *ubo = (float *)m_uboMapped;
	memcpy(ubo, mvp.m, sizeof(mvp.m));
	/* Water UV scroll — Water.ini UScrollPerMS / VScrollPerMS (afternoon). */
	float uRate = 0.002f, vRate = 0.002f;
	if (WaterSettings[TIME_OF_DAY_AFTERNOON].m_uScrollPerMs != 0.f ||
		WaterSettings[TIME_OF_DAY_AFTERNOON].m_vScrollPerMs != 0.f)
	{
		uRate = WaterSettings[TIME_OF_DAY_AFTERNOON].m_uScrollPerMs;
		vRate = WaterSettings[TIME_OF_DAY_AFTERNOON].m_vScrollPerMs;
	}
	const float tMs = (float)(ImGui::GetTime() * 1000.0);
	ubo[16] = tMs * uRate;
	ubo[17] = tMs * vRate;
	ubo[18] = 0.f;
	ubo[19] = 0.f;

	VkDevice dev = host.device();
	VkCommandPool pool = host.uploadCommandPool();
	VkCommandBuffer cmd;
	VkCommandBufferAllocateInfo cai = {};
	cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cai.commandPool = pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	vkAllocateCommandBuffers(dev, &cai, &cmd);

	VkCommandBufferBeginInfo bi = {};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &bi);

	VkClearValue clears[2];
	clears[0].color = {{0.55f, 0.55f, 0.55f, 1.f}}; /* WB gray backdrop */
	clears[1].depthStencil = {1.f, 0};

	VkRenderPassBeginInfo rp = {};
	rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rp.renderPass = m_renderPass;
	rp.framebuffer = m_framebuffer;
	rp.renderArea.extent = {(uint32_t)fbW, (uint32_t)fbH};
	rp.clearValueCount = 2;
	rp.pClearValues = clears;
	vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = {0, 0, (float)fbW, (float)fbH, 0.f, 1.f};
	VkRect2D scissor = {{0, 0}, {(uint32_t)fbW, (uint32_t)fbH}};
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descSet, 0, nullptr);
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &m_vbo, &offset);
	vkCmdBindIndexBuffer(cmd, m_ibo, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, m_indexCount + m_lineIndexCount, 1, 0, 0, 0);

	if (m_roadPipeline != VK_NULL_HANDLE && m_roadVbo != VK_NULL_HANDLE && !m_roadBatches.empty())
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_roadPipeline);
		VkDeviceSize roadOff = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &m_roadVbo, &roadOff);
		vkCmdBindIndexBuffer(cmd, m_roadIbo, 0, VK_INDEX_TYPE_UINT32);
		for (const RoadBatch &b : m_roadBatches)
		{
			if (b.indexCount == 0 || b.descSet == VK_NULL_HANDLE)
				continue;
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &b.descSet, 0,
				nullptr);
			vkCmdDrawIndexed(cmd, b.indexCount, 1, b.indexOffset, 0, 0);
		}
	}

	if (m_showModels && m_objectPipeline != VK_NULL_HANDLE && m_objectVbo != VK_NULL_HANDLE && !m_objectBatches.empty())
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_objectPipeline);
		VkDeviceSize objOff = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &m_objectVbo, &objOff);
		vkCmdBindIndexBuffer(cmd, m_objectIbo, 0, VK_INDEX_TYPE_UINT32);
		for (const ObjectBatch &b : m_objectBatches)
		{
			if (b.indexCount == 0 || b.descSet == VK_NULL_HANDLE)
				continue;
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &b.descSet, 0,
				nullptr);
			vkCmdDrawIndexed(cmd, b.indexCount, 1, b.indexOffset, 0, 0);
		}
	}

	/* Water after opaque geometry (sort level 2), depth-test on / write off. */
	if (m_showWater && m_waterPipeline != VK_NULL_HANDLE && m_waterVbo != VK_NULL_HANDLE && m_waterIndexCount > 0 &&
		m_waterDescSet != VK_NULL_HANDLE)
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_waterPipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_waterDescSet, 0,
			nullptr);
		VkDeviceSize waterOff = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &m_waterVbo, &waterOff);
		vkCmdBindIndexBuffer(cmd, m_waterIbo, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmd, m_waterIndexCount, 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(cmd);
	vkEndCommandBuffer(cmd);

	VkSubmitInfo si = {};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	vkQueueSubmit(host.queue(), 1, &si, VK_NULL_HANDLE);
	vkQueueWaitIdle(host.queue());
	vkFreeCommandBuffers(dev, pool, 1, &cmd);
}
