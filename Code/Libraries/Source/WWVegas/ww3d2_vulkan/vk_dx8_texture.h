#ifndef WW3D2_VULKAN_VK_DX8_TEXTURE_H
#define WW3D2_VULKAN_VK_DX8_TEXTURE_H

#if defined(RENEGADE_VULKAN)

#include "../ww3d2/ww3dformat.h"
#include "../ww3d2/surfaceclass.h"

class TextureClass;

namespace ww3d_vulkan {

void Init_Missing_Vulkan_Texture();
void Shutdown_Missing_Vulkan_Texture();

bool Ensure_Texture_Loaded(TextureClass *texture);
/* Create empty RGBA8 Vulkan image for procedural textures (video, shroud, etc). */
bool Ensure_Procedural_Texture(TextureClass *texture);
bool Get_Texture_Level_Description(
	TextureClass *texture,
	SurfaceClass::SurfaceDescription &surface_desc,
	unsigned level);
unsigned Get_Texture_Mip_Level_Count(TextureClass *texture);
SurfaceClass *Create_Texture_Surface_Level(TextureClass *texture, unsigned level);
void Texture_Stage_Bind(TextureClass *texture, unsigned stage);
void Texture_Stage_Bind_Null(unsigned stage);
bool Last_Stage0_Texture_Is_Pickup(void);
bool Apply_Loaded_Texture(TextureClass *texture, bool initialize);
bool Ensure_Shadow_Decal_Texture_Uncompressed(TextureClass *texture);
void Sync_Texture_Sampler_Address(TextureClass *texture);
void Apply_Missing_Texture(TextureClass *texture);
void Warmup_All_File_Textures();
/* Returns true when all file textures are warmed up. */
bool Warmup_File_Textures_Batch(unsigned batch_size);
void Reset_File_Texture_Warmup();
void Rescan_File_Texture_Warmup();

class VkTexture;
VkTexture *Peek_Missing_Vulkan_Texture();
void Dbg_Set_Cursor_Vulkan_Texture(VkTexture *texture);
/* Deferred free — do not Destroy() textures while frames may still sample them. */
void Retire_Owned_Vulkan_Texture(VkTexture *texture);
VkTexture *Dbg_Peek_Cursor_Vulkan_Texture();

bool Upload_Procedural_Texture_Rgb565(
	TextureClass *texture,
	WW3DFormat format,
	const unsigned char *src,
	int src_pitch,
	unsigned copy_w,
	unsigned copy_h);

bool Upload_Procedural_Texture_Rgb565_Region(
	TextureClass *texture,
	WW3DFormat format,
	const unsigned char *src,
	int src_pitch,
	unsigned copy_w,
	unsigned copy_h,
	unsigned dst_x,
	unsigned dst_y);

class VkCpuSurface;
bool Upload_Procedural_Texture_From_Cpu_Surface(
	TextureClass *texture,
	const VkCpuSurface *surface);

bool Sync_Procedural_Texture_From_Surface(
	TextureClass *texture,
	SurfaceClass *surface);

TextureClass *Try_Clone_Render_Target_Texture(TextureClass *render_target);

} /* namespace ww3d_vulkan */

#endif

#endif
