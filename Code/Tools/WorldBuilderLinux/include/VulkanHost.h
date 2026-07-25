#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include "imgui.h"

struct ImDrawData;

/* SDL3 + Vulkan host for ImGui (based on Dear ImGui example_sdl3_vulkan). */
class VulkanHost
{
public:
	bool init(const char *title, int width, int height);
	void shutdown();

	bool beginFrame(); /* returns false if should skip draw (minimized) */
	void endFrame(const ImVec4 &clearColor);

	SDL_Window *window() const { return m_window; }
	VkDevice device() const { return m_device; }
	VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
	VkQueue queue() const { return m_queue; }
	uint32_t queueFamily() const { return m_queueFamily; }
	/** Dedicated one-shot pool for CPU→GPU uploads (not the in-flight frame pool). */
	VkCommandPool uploadCommandPool() const { return m_uploadPool; }

	bool shouldQuit() const { return m_quit; }
	void requestQuit() { m_quit = true; }

	void pollEvents();

private:
	SDL_Window *m_window = nullptr;
	bool m_quit = false;
	bool m_swapChainRebuild = false;

	VkInstance m_instance = VK_NULL_HANDLE;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;
	uint32_t m_queueFamily = (uint32_t)-1;
	VkQueue m_queue = VK_NULL_HANDLE;
	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	VkCommandPool m_uploadPool = VK_NULL_HANDLE;
	uint32_t m_minImageCount = 2;
};
