#pragma once

#include <vulkan/vulkan.h>
#include "imgui.h"
#include <string>
#include <vector>

class VulkanHost;
class MapDocument;

/**
 * Offscreen Vulkan 3D map view approximating original WbView3d blank template:
 * textured heightmap plane + blue outer edge + orange playable boundary + roads.
 *
 * Camera / mouse match HandScrollTool + WbView3d:
 *   LMB/RMB drag  — pan (scrollInView)
 *   MMB drag      — rotate yaw (rotateCamera); pitch if toggled
 *   Wheel         — zoom (m_mouseWheelOffset)
 *   MMB click     — setDefaultCamera
 *   Space+LMB     — pan (same as hand tool)
 */
class MapViewport
{
public:
	MapViewport();
	~MapViewport();

	bool init(VulkanHost &host);
	void destroy(VulkanHost &host);

	bool rebuildMesh(VulkanHost &host, MapDocument &doc);
	void render(VulkanHost &host, int fbW, int fbH);

	bool meshReady() const { return m_meshReady; }
	bool hasTexture() const { return m_imguiTex != VK_NULL_HANDLE; }
	ImTextureID textureId() const { return (ImTextureID)(ImU64)(uintptr_t)m_imguiTex; }
	int fbWidth() const { return m_fbW; }
	int fbHeight() const { return m_fbH; }

	/** Process mouse like original WB while the Map View item is hovered/active. */
	void handleMapViewInput(const ImGuiIO &io, bool viewHovered);

	void resetCamera(const MapDocument &doc);
	void resetCamera();

	bool showModels() const { return m_showModels; }
	void setShowModels(bool v) { m_showModels = v; }

	bool showWater() const { return m_showWater; }
	void setShowWater(bool v) { m_showWater = v; }

private:
	struct Vertex
	{
		float px, py, pz;
		float u, v;   /* base tile UV (getUVData) */
		float u2, v2; /* blend tile UV (getAlphaUVData) */
		float r, g, b;
		float blend; /* 0..1 vertex alpha for terrain mix */
	};

	struct RoadVertex
	{
		float px, py, pz;
		float u, v;
	};

	struct TextureMipLevel
	{
		int width = 0;
		int height = 0;
		std::vector<unsigned char> rgba;
	};

	struct TextureData
	{
		std::vector<TextureMipLevel> mips;
	};

	struct RoadBatch
	{
		std::string texKey;
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory mem = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkDescriptorSet descSet = VK_NULL_HANDLE;
		uint32_t indexOffset = 0;
		uint32_t indexCount = 0;
	};

	struct ObjectBatch
	{
		std::string texKey;
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory mem = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkDescriptorSet descSet = VK_NULL_HANDLE;
		uint32_t indexOffset = 0;
		uint32_t indexCount = 0;
	};

	struct WaterVertex
	{
		float px, py, pz;
		float u, v;
		float u2, v2;
		float r, g, b, a;
		float isRiver;
	};

	enum TrackMode
	{
		TrackNone = 0,
		TrackLeft,
		TrackRight,
		TrackMiddle
	};

	bool createPipelines(VulkanHost &host);
	bool ensureFramebuffer(VulkanHost &host, int w, int h);
	void destroyFramebuffer(VulkanHost &host);
	void destroyMesh(VulkanHost &host);
	void destroyRoads(VulkanHost &host);
	void destroyObjects(VulkanHost &host);
	void destroyWater(VulkanHost &host);
	bool rebuildRoads(VulkanHost &host, MapDocument &doc);
	bool rebuildObjects(VulkanHost &host, MapDocument &doc);
	bool rebuildWater(VulkanHost &host, MapDocument &doc);
	bool uploadBuffer(VulkanHost &host, VkBuffer &buf, VkDeviceMemory &mem, const void *data, VkDeviceSize size,
		VkBufferUsageFlags usage);
	bool uploadTextureImage(VulkanHost &host, const TextureData &texture,
		VkImage &image, VkDeviceMemory &memory, VkImageView &view);
	bool createDirtTexture(VulkanHost &host);
	bool uploadAlbedoTexture(VulkanHost &host, const unsigned char *rgba, int w, int h);
	bool uploadMipmappedAlbedoTexture(VulkanHost &host, const unsigned char *rgba, int w, int h);
	void destroyAlbedoTexture(VulkanHost &host);
	void completeMipChain(TextureData &texture);
	bool loadRoadTexture(const char *texName, TextureData &texture);
	bool uploadRoadTexture(VulkanHost &host, RoadBatch &batch, const TextureData &texture);
	bool uploadObjectTexture(VulkanHost &host, ObjectBatch &batch, const TextureData &texture);
	uint32_t findMemoryType(VulkanHost &host, uint32_t typeFilter, VkMemoryPropertyFlags props);

	void scrollInView(float dxCells, float dyCells);
	void rotateCamera(float deltaAngle);
	void pitchCamera(float delta);
	void constrainCenter();
	void buildCameraEyeTarget(float &eyeX, float &eyeY, float &eyeZ, float &tgtX, float &tgtY, float &tgtZ) const;
	float currentLookDistance() const;

	bool m_inited = false;
	bool m_meshReady = false;
	bool m_pipelineReady = false;

	int m_fbW = 0;
	int m_fbH = 0;
	int m_mapW = 0;
	int m_mapH = 0;
	int m_border = 0;

	/* WbView3d camera state (center in heightmap cell units). */
	float m_centerCellX = 50.f;
	float m_centerCellY = 50.f;
	float m_groundLevel = 10.f;
	float m_cameraAngle = 0.f;
	float m_fxPitch = 1.f;
	float m_mouseWheelOffset = 0.f;
	float m_cameraPitchDeg = 37.5f;
	float m_cameraYawDeg = 0.f;
	float m_maxCameraHeight = 310.f;
	float m_camOffX = 0.f;
	float m_camOffY = 0.f;
	float m_camOffZ = 320.f;
	bool m_doPitch = false;

	TrackMode m_track = TrackNone;
	float m_prevMouseX = 0.f;
	float m_prevMouseY = 0.f;
	float m_downMouseX = 0.f;
	float m_downMouseY = 0.f;
	double m_downTime = 0.0;
	bool m_scrolling = false;

	uint32_t m_indexCount = 0;
	uint32_t m_lineIndexCount = 0;
	uint32_t m_lineIndexOffset = 0;

	VkRenderPass m_renderPass = VK_NULL_HANDLE;
	VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
	VkPipeline m_pipeline = VK_NULL_HANDLE;
	VkPipeline m_roadPipeline = VK_NULL_HANDLE;
	VkPipeline m_objectPipeline = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_dsl = VK_NULL_HANDLE;
	VkDescriptorPool m_descPool = VK_NULL_HANDLE;
	VkDescriptorSet m_descSet = VK_NULL_HANDLE;

	VkImage m_colorImage = VK_NULL_HANDLE;
	VkDeviceMemory m_colorMem = VK_NULL_HANDLE;
	VkImageView m_colorView = VK_NULL_HANDLE;
	VkImage m_depthImage = VK_NULL_HANDLE;
	VkDeviceMemory m_depthMem = VK_NULL_HANDLE;
	VkImageView m_depthView = VK_NULL_HANDLE;
	VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
	VkDescriptorSet m_imguiTex = VK_NULL_HANDLE;

	VkBuffer m_vbo = VK_NULL_HANDLE;
	VkDeviceMemory m_vboMem = VK_NULL_HANDLE;
	VkBuffer m_ibo = VK_NULL_HANDLE;
	VkDeviceMemory m_iboMem = VK_NULL_HANDLE;
	VkBuffer m_ubo = VK_NULL_HANDLE;
	VkDeviceMemory m_uboMem = VK_NULL_HANDLE;
	void *m_uboMapped = nullptr;

	VkImage m_dirtImage = VK_NULL_HANDLE;
	VkDeviceMemory m_dirtMem = VK_NULL_HANDLE;
	VkImageView m_dirtView = VK_NULL_HANDLE;
	VkSampler m_dirtSampler = VK_NULL_HANDLE;

	VkSampler m_roadSampler = VK_NULL_HANDLE;
	VkBuffer m_roadVbo = VK_NULL_HANDLE;
	VkDeviceMemory m_roadVboMem = VK_NULL_HANDLE;
	VkBuffer m_roadIbo = VK_NULL_HANDLE;
	VkDeviceMemory m_roadIboMem = VK_NULL_HANDLE;
	std::vector<RoadBatch> m_roadBatches;

	bool m_showModels = true;
	VkBuffer m_objectVbo = VK_NULL_HANDLE;
	VkDeviceMemory m_objectVboMem = VK_NULL_HANDLE;
	VkBuffer m_objectIbo = VK_NULL_HANDLE;
	VkDeviceMemory m_objectIboMem = VK_NULL_HANDLE;
	std::vector<ObjectBatch> m_objectBatches;

	bool m_showWater = true;
	VkPipeline m_waterPipeline = VK_NULL_HANDLE;
	VkBuffer m_waterVbo = VK_NULL_HANDLE;
	VkDeviceMemory m_waterVboMem = VK_NULL_HANDLE;
	VkBuffer m_waterIbo = VK_NULL_HANDLE;
	VkDeviceMemory m_waterIboMem = VK_NULL_HANDLE;
	uint32_t m_waterIndexCount = 0;
	VkImage m_waterImage = VK_NULL_HANDLE;
	VkDeviceMemory m_waterMem = VK_NULL_HANDLE;
	VkImageView m_waterView = VK_NULL_HANDLE;
	VkImage m_waterAlphaImage = VK_NULL_HANDLE;
	VkDeviceMemory m_waterAlphaMem = VK_NULL_HANDLE;
	VkImageView m_waterAlphaView = VK_NULL_HANDLE;
	VkDescriptorSet m_waterDescSet = VK_NULL_HANDLE;
};
