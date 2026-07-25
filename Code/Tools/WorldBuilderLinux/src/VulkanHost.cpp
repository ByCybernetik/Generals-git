// VulkanHost — SDL3 + Vulkan presenter for Dear ImGui.
// Adapted from third_party/imgui/examples/example_sdl3_vulkan/main.cpp

#include "VulkanHost.h"

#include "imgui.h"
#include <string.h>
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL_vulkan.h>

static VkAllocationCallbacks *g_Allocator = nullptr;
static VkInstance g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice g_Device = VK_NULL_HANDLE;
static uint32_t g_QueueFamily = (uint32_t)-1;
static VkQueue g_Queue = VK_NULL_HANDLE;
static VkPipelineCache g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool g_DescriptorPool = VK_NULL_HANDLE;
static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t g_MinImageCount = 2;

static void check_vk_result(VkResult err)
{
	if (err == VK_SUCCESS)
		return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0)
		abort();
}

static bool IsExtensionAvailable(const ImVector<VkExtensionProperties> &properties, const char *extension)
{
	for (const VkExtensionProperties &p : properties)
		if (strcmp(p.extensionName, extension) == 0)
			return true;
	return false;
}

static void SetupVulkan(ImVector<const char *> instance_extensions)
{
	VkResult err;

	{
		VkInstanceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

		uint32_t properties_count;
		ImVector<VkExtensionProperties> properties;
		vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
		properties.resize(properties_count);
		err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
		check_vk_result(err);

		if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
		{
			instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
			create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		}

		create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
		create_info.ppEnabledExtensionNames = instance_extensions.Data;
		err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
		check_vk_result(err);
	}

	{
		uint32_t gpu_count;
		err = vkEnumeratePhysicalDevices(g_Instance, &gpu_count, nullptr);
		check_vk_result(err);
		IM_ASSERT(gpu_count > 0);
		ImVector<VkPhysicalDevice> gpus;
		gpus.resize(gpu_count);
		err = vkEnumeratePhysicalDevices(g_Instance, &gpu_count, gpus.Data);
		check_vk_result(err);

		int use_gpu = -1;
		for (int i = 0; i < (int)gpu_count; i++)
		{
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(gpus[i], &properties);
			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				use_gpu = i;
				break;
			}
		}
		if (use_gpu == -1)
			use_gpu = 0;
		g_PhysicalDevice = gpus[use_gpu];
	}

	{
		uint32_t count;
		vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, nullptr);
		ImVector<VkQueueFamilyProperties> queues;
		queues.resize(count);
		vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, queues.Data);
		for (uint32_t i = 0; i < count; i++)
			if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				g_QueueFamily = i;
				break;
			}
		IM_ASSERT(g_QueueFamily != (uint32_t)-1);
	}

	{
		ImVector<const char *> device_extensions;
		device_extensions.push_back("VK_KHR_swapchain");

		uint32_t properties_count;
		ImVector<VkExtensionProperties> properties;
		vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &properties_count, nullptr);
		properties.resize(properties_count);
		vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &properties_count, properties.Data);
		if (IsExtensionAvailable(properties, "VK_KHR_portability_subset"))
			device_extensions.push_back("VK_KHR_portability_subset");

		const float queue_priority[] = {1.0f};
		VkDeviceQueueCreateInfo queue_info[1] = {};
		queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info[0].queueFamilyIndex = g_QueueFamily;
		queue_info[0].queueCount = 1;
		queue_info[0].pQueuePriorities = queue_priority;
		VkDeviceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
		create_info.pQueueCreateInfos = queue_info;
		create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
		create_info.ppEnabledExtensionNames = device_extensions.Data;
		err = vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
		check_vk_result(err);
		vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
	}

	{
		VkDescriptorPoolSize pool_sizes[] = {
			{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE + 64},
			{VK_DESCRIPTOR_TYPE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE + 8},
		};
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 0;
		for (VkDescriptorPoolSize &pool_size : pool_sizes)
			pool_info.maxSets += pool_size.descriptorCount;
		pool_info.poolSizeCount = (uint32_t)(sizeof(pool_sizes) / sizeof(pool_sizes[0]));
		pool_info.pPoolSizes = pool_sizes;
		err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool);
		check_vk_result(err);
	}
}

static void SetupVulkanWindow(ImGui_ImplVulkanH_Window *wd, VkSurfaceKHR surface, int width, int height)
{
	wd->Surface = surface;
	VkBool32 res;
	vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, wd->Surface, &res);
	if (res != VK_TRUE)
	{
		fprintf(stderr, "Error no WSI support on physical device 0\n");
		exit(-1);
	}
	const VkFormat requestSurfaceImageFormat[] = {
		VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM};
	const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
		g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat,
		(size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);
	VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};
	wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
		g_PhysicalDevice, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
	IM_ASSERT(g_MinImageCount >= 2);
	ImGui_ImplVulkanH_CreateOrResizeWindow(
		g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount, 0);
}

static void CleanupVulkan()
{
	vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);
	vkDestroyDevice(g_Device, g_Allocator);
	vkDestroyInstance(g_Instance, g_Allocator);
}

static void CleanupVulkanWindow()
{
	ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, &g_MainWindowData, g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window *wd, ImDrawData *draw_data)
{
	VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
	VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
	VkResult err = vkAcquireNextImageKHR(
		g_Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
		return;
	check_vk_result(err);

	ImGui_ImplVulkanH_Frame *fd = &wd->Frames[wd->FrameIndex];
	{
		err = vkWaitForFences(g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
		check_vk_result(err);
		err = vkResetFences(g_Device, 1, &fd->Fence);
		check_vk_result(err);
	}
	{
		err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
		check_vk_result(err);
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
		check_vk_result(err);
	}
	{
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = wd->RenderPass;
		info.framebuffer = fd->Framebuffer;
		info.renderArea.extent.width = wd->Width;
		info.renderArea.extent.height = wd->Height;
		info.clearValueCount = 1;
		info.pClearValues = &wd->ClearValue;
		vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

	vkCmdEndRenderPass(fd->CommandBuffer);
	{
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &image_acquired_semaphore;
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &fd->CommandBuffer;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &render_complete_semaphore;

		err = vkEndCommandBuffer(fd->CommandBuffer);
		check_vk_result(err);
		err = vkQueueSubmit(g_Queue, 1, &info, fd->Fence);
		check_vk_result(err);
	}
}

static void FramePresent(ImGui_ImplVulkanH_Window *wd)
{
	VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &render_complete_semaphore;
	info.swapchainCount = 1;
	info.pSwapchains = &wd->Swapchain;
	info.pImageIndices = &wd->FrameIndex;
	VkResult err = vkQueuePresentKHR(g_Queue, &info);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
		return;
	check_vk_result(err);
	wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
}

bool VulkanHost::init(const char *title, int width, int height)
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
	{
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return false;
	}

	float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
	SDL_WindowFlags window_flags =
		SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
	m_window = SDL_CreateWindow(
		title, (int)(width * main_scale), (int)(height * main_scale), window_flags);
	if (!m_window)
	{
		fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
		return false;
	}

	ImVector<const char *> extensions;
	{
		uint32_t sdl_extensions_count = 0;
		const char *const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extensions_count);
		for (uint32_t n = 0; n < sdl_extensions_count; n++)
			extensions.push_back(sdl_extensions[n]);
	}
	SetupVulkan(extensions);

	VkSurfaceKHR surface;
	if (!SDL_Vulkan_CreateSurface(m_window, g_Instance, g_Allocator, &surface))
	{
		printf("Failed to create Vulkan surface.\n");
		return false;
	}

	int w, h;
	SDL_GetWindowSize(m_window, &w, &h);
	ImGui_ImplVulkanH_Window *wd = &g_MainWindowData;
	SetupVulkanWindow(wd, surface, w, h);
	SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_ShowWindow(m_window);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();
	ImGuiStyle &style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);
	style.FontScaleDpi = main_scale;

	ImGui_ImplSDL3_InitForVulkan(m_window);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = g_Instance;
	init_info.PhysicalDevice = g_PhysicalDevice;
	init_info.Device = g_Device;
	init_info.QueueFamily = g_QueueFamily;
	init_info.Queue = g_Queue;
	init_info.PipelineCache = g_PipelineCache;
	init_info.DescriptorPool = g_DescriptorPool;
	init_info.MinImageCount = g_MinImageCount;
	init_info.ImageCount = wd->ImageCount;
	init_info.Allocator = g_Allocator;
	init_info.PipelineInfoMain.RenderPass = wd->RenderPass;
	init_info.PipelineInfoMain.Subpass = 0;
	init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.CheckVkResultFn = check_vk_result;
	ImGui_ImplVulkan_Init(&init_info);

	m_instance = g_Instance;
	m_physicalDevice = g_PhysicalDevice;
	m_device = g_Device;
	m_queueFamily = g_QueueFamily;
	m_queue = g_Queue;
	m_descriptorPool = g_DescriptorPool;
	m_minImageCount = g_MinImageCount;

	{
		VkCommandPoolCreateInfo ci = {};
		ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		ci.queueFamilyIndex = g_QueueFamily;
		VkResult err = vkCreateCommandPool(g_Device, &ci, g_Allocator, &m_uploadPool);
		check_vk_result(err);
	}
	return true;
}

void VulkanHost::shutdown()
{
	VkResult err = vkDeviceWaitIdle(g_Device);
	check_vk_result(err);
	if (m_uploadPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(g_Device, m_uploadPool, g_Allocator);
		m_uploadPool = VK_NULL_HANDLE;
	}
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	CleanupVulkanWindow();
	CleanupVulkan();
	if (m_window)
		SDL_DestroyWindow(m_window);
	m_window = nullptr;
	SDL_Quit();
}

void VulkanHost::pollEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		ImGui_ImplSDL3_ProcessEvent(&event);
		if (event.type == SDL_EVENT_QUIT)
			m_quit = true;
		if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
			event.window.windowID == SDL_GetWindowID(m_window))
			m_quit = true;
	}
}

bool VulkanHost::beginFrame()
{
	if (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED)
	{
		SDL_Delay(10);
		return false;
	}

	ImGui_ImplVulkanH_Window *wd = &g_MainWindowData;
	int fb_width, fb_height;
	SDL_GetWindowSizeInPixels(m_window, &fb_width, &fb_height);
	if (fb_width > 0 && fb_height > 0 &&
		(m_swapChainRebuild || wd->Width != fb_width || wd->Height != fb_height))
	{
		ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
		ImGui_ImplVulkanH_CreateOrResizeWindow(
			g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, fb_width, fb_height,
			g_MinImageCount, 0);
		wd->FrameIndex = 0;
		m_swapChainRebuild = false;
	}

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	return true;
}

void VulkanHost::endFrame(const ImVec4 &clear_color)
{
	ImGui::Render();
	ImDrawData *main_draw_data = ImGui::GetDrawData();
	const bool main_is_minimized =
		(main_draw_data->DisplaySize.x <= 0.0f || main_draw_data->DisplaySize.y <= 0.0f);
	ImGui_ImplVulkanH_Window *wd = &g_MainWindowData;
	wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
	wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
	wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
	wd->ClearValue.color.float32[3] = clear_color.w;
	if (!main_is_minimized)
		FrameRender(wd, main_draw_data);
	if (!main_is_minimized)
		FramePresent(wd);
}
