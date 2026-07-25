#include "HeightmapPreview.h"
#include "VulkanHost.h"
#include "MapDocument.h"
#include "imgui_impl_vulkan.h"

#include <string.h>
#include <stdio.h>
#include <algorithm>

HeightmapPreview::HeightmapPreview() {}

HeightmapPreview::~HeightmapPreview()
{
	/* Caller must destroy() with a live VulkanHost before delete. */
}

void HeightmapPreview::destroy(VulkanHost &host)
{
	destroyImage(host);
}

void HeightmapPreview::destroyImage(VulkanHost &host)
{
	VkDevice dev = host.device();
	if (dev == VK_NULL_HANDLE)
		return;
	vkDeviceWaitIdle(dev);
	if (m_descriptor != VK_NULL_HANDLE)
	{
		ImGui_ImplVulkan_RemoveTexture(m_descriptor);
		m_descriptor = VK_NULL_HANDLE;
	}
	if (m_sampler != VK_NULL_HANDLE)
	{
		vkDestroySampler(dev, m_sampler, nullptr);
		m_sampler = VK_NULL_HANDLE;
	}
	if (m_view != VK_NULL_HANDLE)
	{
		vkDestroyImageView(dev, m_view, nullptr);
		m_view = VK_NULL_HANDLE;
	}
	if (m_image != VK_NULL_HANDLE)
	{
		vkDestroyImage(dev, m_image, nullptr);
		m_image = VK_NULL_HANDLE;
	}
	if (m_memory != VK_NULL_HANDLE)
	{
		vkFreeMemory(dev, m_memory, nullptr);
		m_memory = VK_NULL_HANDLE;
	}
	m_width = m_height = 0;
}

static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props)
{
	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
	for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
	{
		if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
			return i;
	}
	return 0;
}

bool HeightmapPreview::createImage(VulkanHost &host, const unsigned char *rgba, int w, int h)
{
	VkDevice dev = host.device();
	VkPhysicalDevice phys = host.physicalDevice();
	const VkDeviceSize size = (VkDeviceSize)w * (VkDeviceSize)h * 4;

	VkBuffer staging = VK_NULL_HANDLE;
	VkDeviceMemory stagingMem = VK_NULL_HANDLE;
	{
		VkBufferCreateInfo bi = {};
		bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bi.size = size;
		bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateBuffer(dev, &bi, nullptr, &staging) != VK_SUCCESS)
			return false;
		VkMemoryRequirements req;
		vkGetBufferMemoryRequirements(dev, staging, &req);
		VkMemoryAllocateInfo ai = {};
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex =
			findMemoryType(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		if (vkAllocateMemory(dev, &ai, nullptr, &stagingMem) != VK_SUCCESS)
			return false;
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
		ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (vkCreateImage(dev, &ii, nullptr, &m_image) != VK_SUCCESS)
			return false;
		VkMemoryRequirements req;
		vkGetImageMemoryRequirements(dev, m_image, &req);
		VkMemoryAllocateInfo ai = {};
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = findMemoryType(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (vkAllocateMemory(dev, &ai, nullptr, &m_memory) != VK_SUCCESS)
			return false;
		vkBindImageMemory(dev, m_image, m_memory, 0);
	}

	VkCommandPool pool = host.uploadCommandPool();
	if (pool == VK_NULL_HANDLE)
		return false;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	{
		VkCommandBufferAllocateInfo ai = {};
		ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		ai.commandPool = pool;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(dev, &ai, &cmd) != VK_SUCCESS)
			return false;
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
		barrier.image = m_image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
			nullptr, 0, nullptr, 1, &barrier);

		VkBufferImageCopy region = {};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
		vkCmdCopyBufferToImage(cmd, staging, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
			nullptr, 0, nullptr, 1, &barrier);

		vkEndCommandBuffer(cmd);
		VkSubmitInfo si = {};
		si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		si.commandBufferCount = 1;
		si.pCommandBuffers = &cmd;
		vkQueueSubmit(host.queue(), 1, &si, VK_NULL_HANDLE);
		vkQueueWaitIdle(host.queue());
		vkFreeCommandBuffers(dev, pool, 1, &cmd);
	}

	vkDestroyBuffer(dev, staging, nullptr);
	vkFreeMemory(dev, stagingMem, nullptr);

	{
		VkImageViewCreateInfo vi = {};
		vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vi.image = m_image;
		vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vi.format = VK_FORMAT_R8G8B8A8_UNORM;
		vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vi.subresourceRange.levelCount = 1;
		vi.subresourceRange.layerCount = 1;
		if (vkCreateImageView(dev, &vi, nullptr, &m_view) != VK_SUCCESS)
			return false;
	}
	{
		VkSamplerCreateInfo si = {};
		si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		si.magFilter = VK_FILTER_NEAREST;
		si.minFilter = VK_FILTER_NEAREST;
		si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		if (vkCreateSampler(dev, &si, nullptr, &m_sampler) != VK_SUCCESS)
			return false;
	}

	/* ImGui 1.92 Vulkan: sampled image view is the texture ID (default sampler). */
	m_descriptor = ImGui_ImplVulkan_AddTexture(m_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_width = w;
	m_height = h;
	return m_descriptor != VK_NULL_HANDLE;
}

/* Visible even for flat height=16: mid-green playable, darker red-tinted border. */
static void heightToRgb(UnsignedByte z, bool inBorder, unsigned char out[3])
{
	const float t = (float)z / 255.0f;
	float r, g, b;
	if (t < 0.15f)
	{
		const float u = t / 0.15f;
		r = 25.0f + 35.0f * u;
		g = 70.0f + 90.0f * u;
		b = 55.0f + 40.0f * u;
	}
	else if (t < 0.45f)
	{
		const float u = (t - 0.15f) / 0.30f;
		r = 60.0f + 90.0f * u;
		g = 160.0f - 40.0f * u;
		b = 95.0f - 55.0f * u;
	}
	else if (t < 0.75f)
	{
		const float u = (t - 0.45f) / 0.30f;
		r = 150.0f + 60.0f * u;
		g = 120.0f + 40.0f * u;
		b = 40.0f + 30.0f * u;
	}
	else
	{
		const float u = (t - 0.75f) / 0.25f;
		r = 210.0f + 45.0f * u;
		g = 160.0f + 95.0f * u;
		b = 70.0f + 185.0f * u;
	}
	if (inBorder)
	{
		r = r * 0.35f + 90.0f;
		g = g * 0.35f + 20.0f;
		b = b * 0.35f + 20.0f;
	}
	out[0] = (unsigned char)std::min(255.0f, r);
	out[1] = (unsigned char)std::min(255.0f, g);
	out[2] = (unsigned char)std::min(255.0f, b);
}

bool HeightmapPreview::rebuild(VulkanHost &host, const MapDocument &doc)
{
	destroyImage(host);
	if (!doc.isLoaded() || !doc.heightMap())
		return false;

	WorldHeightMap *hm = const_cast<WorldHeightMap *>(doc.heightMap());
	const Int w = hm->getXExtent();
	const Int h = hm->getYExtent();
	const Int border = hm->getBorderSize();
	if (w <= 0 || h <= 0 || !hm->getDataPtr())
		return false;

	std::vector<unsigned char> rgba((size_t)w * (size_t)h * 4);
	for (Int y = 0; y < h; ++y)
	{
		for (Int x = 0; x < w; ++x)
		{
			const UnsignedByte z = hm->getHeight(x, y);
			const bool inBorder = (x < border || y < border || x >= w - border || y >= h - border);
			unsigned char rgb[3];
			heightToRgb(z, inBorder, rgb);
			const size_t i = ((size_t)y * (size_t)w + (size_t)x) * 4;
			rgba[i + 0] = rgb[0];
			rgba[i + 1] = rgb[1];
			rgba[i + 2] = rgb[2];
			rgba[i + 3] = 255;
		}
	}
	return createImage(host, rgba.data(), w, h);
}
