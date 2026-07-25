#pragma once

#include <vulkan/vulkan.h>
#include "imgui.h"
#include <vector>

class VulkanHost;
class MapDocument;

/* CPU heightmap → RGBA VkImage registered with ImGui_ImplVulkan_AddTexture. */
class HeightmapPreview
{
public:
	HeightmapPreview();
	~HeightmapPreview();

	void destroy(VulkanHost &host);
	bool rebuild(VulkanHost &host, const MapDocument &doc);

	bool valid() const { return m_descriptor != VK_NULL_HANDLE; }
	ImTextureID textureId() const { return (ImTextureID)(ImU64)(uintptr_t)m_descriptor; }
	int width() const { return m_width; }
	int height() const { return m_height; }

private:
	bool createImage(VulkanHost &host, const unsigned char *rgba, int w, int h);
	void destroyImage(VulkanHost &host);

	int m_width = 0;
	int m_height = 0;
	VkImage m_image = VK_NULL_HANDLE;
	VkDeviceMemory m_memory = VK_NULL_HANDLE;
	VkImageView m_view = VK_NULL_HANDLE;
	VkSampler m_sampler = VK_NULL_HANDLE;
	VkDescriptorSet m_descriptor = VK_NULL_HANDLE;
};
