#include "stb_texture.h"

#if defined(RENEGADE_VULKAN)

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"

#include "../ww3d2/ddsfile.h"
#include "../ww3d2/legacy_dds_header.h"
#include "../ww3d2/formconv.h"
#include "../wwlib/bufffile.h"
#include "../wwlib/ffactory.h"

#include <cstring>
#include <cctype>
#include <vector>

namespace ww3d_vulkan {

namespace {

static uint32_t Next_Power_Of_Two(uint32_t value)
{
	if (value == 0) {
		return 1;
	}
	uint32_t pot = 1;
	while (pot < value && pot < 4096u) {
		pot <<= 1;
	}
	return pot;
}

static void Validate_Texture_Size(uint32_t &width, uint32_t &height)
{
	width = Next_Power_Of_Two(width);
	height = Next_Power_Of_Two(height);
	if (width > 4096u) {
		width = 4096u;
	}
	if (height > 4096u) {
		height = 4096u;
	}
	if (width > height * 8u) {
		height = width / 8u;
	} else if (height > width * 8u) {
		width = height / 8u;
	}
}

static void Copy_Base_Path(char *out, size_t cap, const char *path)
{
	if (out == nullptr || cap == 0 || path == nullptr) {
		return;
	}
	strncpy(out, path, cap - 1);
	out[cap - 1] = '\0';
	char *dot = strrchr(out, '.');
	if (dot != nullptr && dot != out) {
		*dot = '\0';
	}
}

static void Set_Path_Extension(char *path, size_t cap, const char *extension)
{
	if (path == nullptr || cap == 0 || extension == nullptr || extension[0] == '\0') {
		return;
	}
	char base[256];
	Copy_Base_Path(base, sizeof(base), path);
	snprintf(path, cap, "%s%s", base, extension);
}

static bool Paths_Equal_Ignore_Case(const char *a, const char *b)
{
	if (a == nullptr || b == nullptr) {
		return a == b;
	}
	while (*a && *b) {
		const char ca = (char)tolower((unsigned char)*a);
		const char cb = (char)tolower((unsigned char)*b);
		if (ca != cb) {
			return false;
		}
		++a;
		++b;
	}
	return *a == *b;
}

static size_t Dxt_Level_Size(uint32_t width, uint32_t height, WW3DFormat format)
{
	if (width < 4) {
		width = 4;
	}
	if (height < 4) {
		height = 4;
	}
	const size_t block_bytes = (format == WW3D_FORMAT_DXT1) ? 8u : 16u;
	return (size_t)(width / 4u) * (size_t)(height / 4u) * block_bytes;
}

static bool Is_Dxt_Format(WW3DFormat format)
{
	switch (format) {
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		return true;
	default:
		return false;
	}
}

static WW3DFormat Vk_Normalize_Dxt_Format(WW3DFormat format)
{
	switch (format) {
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		return WW3D_FORMAT_DXT5;
	case WW3D_FORMAT_DXT1:
		return WW3D_FORMAT_DXT1;
	default:
		return format;
	}
}

static void Normalize_Texture_Load_Path(const char *path, char *out, size_t cap)
{
	if (path == nullptr || out == nullptr || cap == 0) {
		return;
	}
	strncpy(out, path, cap - 1);
	out[cap - 1] = '\0';
	for (char *p = out; *p; ++p) {
		if (*p == '\\') {
			*p = '/';
		}
	}
	if (strchr(out, '/') == nullptr) {
		for (char *p = out; *p; ++p) {
			*p = (char)tolower((unsigned char)*p);
		}
	}
}

static bool Read_File_Bytes(const char *path, std::vector<uint8_t> &out)
{
	char normalized[256];
	Normalize_Texture_Load_Path(path, normalized, sizeof(normalized));
	path = normalized;

	file_auto_ptr file(_TheFileFactory, path);
	if (!file->Is_Available()) {
		return false;
	}
	file->Open();
	const int size = file->Size();
	if (size <= 0) {
		file->Close();
		return false;
	}
	out.resize((size_t)size);
	const int read_size = file->Read(out.data(), size);
	file->Close();
	return read_size == size;
}

static bool Load_Dds_From_Bytes(
	const std::vector<uint8_t> &file_bytes,
	unsigned reduction,
	StbLoadedTexture *out)
{
	if (file_bytes.size() < 4u + LEGACY_DDSURFACEDESC2_BYTES) {
		return false;
	}
	if (file_bytes[0] != 'D' || file_bytes[1] != 'D' || file_bytes[2] != 'S' ||
		file_bytes[3] != ' ') {
		return false;
	}

	const LegacyDdsHeaderFields parsed =
		Legacy_Dds_Parse_Header(file_bytes.data() + 4, file_bytes.size() - 4);
	if (!parsed.valid) {
		return false;
	}

	const WW3DFormat format =
		D3DFormat_To_WW3DFormat((D3DFORMAT)parsed.four_cc);
	if (!Is_Dxt_Format(format)) {
		return false;
	}

	unsigned mip_levels = parsed.mip_map_count;
	unsigned reduction_factor = reduction;
	if (mip_levels > reduction_factor) {
		mip_levels -= reduction_factor;
	} else {
		mip_levels = 1;
		reduction_factor = reduction_factor - mip_levels;
	}
	if (mip_levels > 2) {
		mip_levels -= 2;
	} else {
		mip_levels = 1;
	}

	const uint32_t full_width = parsed.width;
	const uint32_t full_height = parsed.height;
	const uint32_t width = full_width >> reduction_factor;
	const uint32_t height = full_height >> reduction_factor;

	unsigned level_size =
		(unsigned)Dxt_Level_Size(full_width, full_height, format);
	size_t data_offset = 4u + LEGACY_DDSURFACEDESC2_BYTES;
	for (unsigned i = 0; i < reduction_factor; ++i) {
		if (data_offset + level_size > file_bytes.size()) {
			return false;
		}
		data_offset += level_size;
		if (level_size > 16) {
			level_size /= 4;
		}
	}

	std::vector<uint8_t> compressed;
	compressed.reserve((size_t)mip_levels * 256u);
	uint32_t mip_w = width;
	uint32_t mip_h = height;
	for (unsigned level = 0; level < mip_levels; ++level) {
		uint32_t level_w = mip_w;
		uint32_t level_h = mip_h;
		if (level_w < 4) {
			level_w = 4;
		}
		if (level_h < 4) {
			level_h = 4;
		}
		const size_t level_bytes = Dxt_Level_Size(level_w, level_h, format);
		if (data_offset + level_bytes > file_bytes.size()) {
			return false;
		}
		const size_t old_size = compressed.size();
		compressed.resize(old_size + level_bytes);
		memcpy(compressed.data() + old_size, file_bytes.data() + data_offset, level_bytes);
		data_offset += level_bytes;
		if (mip_w > 4) {
			mip_w >>= 1;
		}
		if (mip_h > 4) {
			mip_h >>= 1;
		}
	}

	out->format = Vk_Normalize_Dxt_Format(format);
	out->width = width;
	out->height = height;
	out->mip_levels = mip_levels;
	out->compressed = true;
	out->pixels.swap(compressed);
	return true;
}

static bool Image_Is_Png_Or_Jpeg(const std::vector<uint8_t> &bytes)
{
	if (bytes.size() >= 8u && bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' &&
		bytes[3] == 'G') {
		return true;
	}
	if (bytes.size() >= 2u && bytes[0] == 0xFF && bytes[1] == 0xD8) {
		return true;
	}
	return false;
}

static bool Image_Load_Flip_Vertical(const std::vector<uint8_t> &bytes, bool fallback_flip)
{
	/*
	 * stb_image returns PNG/JPEG/BMP top-down already — do not flip those.
	 *
	 * Direct GPU TGA loads (Ensure_Texture_Loaded → Create_From_Rgba8) need
	 * fallback_flip=true to match D3D/TGA UV orientation used by the rest of
	 * the game. CPU surfaces for housecolor remapping (Load_From_File) pass
	 * false: that path uploads via Apply_Texture_From_Cpu_Surface and must
	 * stay unflipped or ZHCA infantry skins end up upside-down.
	 */
	if (Image_Is_Png_Or_Jpeg(bytes)) {
		return false;
	}
	if (bytes.size() >= 2u && bytes[0] == 'B' && bytes[1] == 'M') {
		return false;
	}
	return fallback_flip;
}

static bool Load_Image_From_Bytes(
	const std::vector<uint8_t> &file_bytes,
	unsigned reduction,
	bool allow_compression,
	bool flip_vertical,
	StbLoadedTexture *out)
{
	(void)allow_compression;

	const bool flip = Image_Load_Flip_Vertical(file_bytes, flip_vertical);
	stbi_set_flip_vertically_on_load(flip ? 1 : 0);
	int width = 0;
	int height = 0;
	int channels = 0;
	uint8_t *loaded = stbi_load_from_memory(
		file_bytes.data(),
		(int)file_bytes.size(),
		&width,
		&height,
		&channels,
		4);
	if (loaded == nullptr || width <= 0 || height <= 0) {
		stbi_image_free(loaded);
		return false;
	}

	uint32_t tex_w = (uint32_t)width;
	uint32_t tex_h = (uint32_t)height;
	for (unsigned i = 0; i < reduction; ++i) {
		if (tex_w > 4) {
			tex_w >>= 1;
		}
		if (tex_h > 4) {
			tex_h >>= 1;
		}
	}
	Validate_Texture_Size(tex_w, tex_h);

	std::vector<uint8_t> rgba((size_t)tex_w * (size_t)tex_h * 4u);
	if ((uint32_t)width == tex_w && (uint32_t)height == tex_h) {
		memcpy(rgba.data(), loaded, rgba.size());
	} else {
		for (uint32_t y = 0; y < tex_h; ++y) {
			const uint32_t src_y = (uint32_t)((uint64_t)y * (uint64_t)height / (uint64_t)tex_h);
			for (uint32_t x = 0; x < tex_w; ++x) {
				const uint32_t src_x = (uint32_t)((uint64_t)x * (uint64_t)width / (uint64_t)tex_w);
				const uint8_t *src = loaded + (src_y * (uint32_t)width + src_x) * 4u;
				uint8_t *dst = rgba.data() + (y * tex_w + x) * 4u;
				dst[0] = src[0];
				dst[1] = src[1];
				dst[2] = src[2];
				dst[3] = src[3];
			}
		}
	}
	stbi_image_free(loaded);

	/*
	 * Skip runtime DXT compression: stb_compress_dxt_block is far too slow for
	 * large textures and was causing load hitches / FPS drops. Prebuilt .dds in
	 * archives is handled above; decoded PNG/JPG/TGA upload as RGBA8 like retail TGA.
	 */
	out->format = WW3D_FORMAT_A8R8G8B8;
	out->width = tex_w;
	out->height = tex_h;
	out->mip_levels = 1;
	out->compressed = false;
	out->pixels.swap(rgba);
	return true;
}

static bool Try_Load_From_Bytes(
	const std::vector<uint8_t> &file_bytes,
	unsigned reduction,
	bool allow_compression,
	bool flip_vertical,
	StbLoadedTexture *out)
{
	if (Load_Dds_From_Bytes(file_bytes, reduction, out)) {
		return true;
	}
	return Load_Image_From_Bytes(file_bytes, reduction, allow_compression, flip_vertical, out);
}

static bool Try_Load_Candidate_Path(
	const char *candidate_path,
	unsigned reduction,
	bool allow_compression,
	bool flip_vertical,
	StbLoadedTexture *out)
{
	std::vector<uint8_t> file_bytes;
	if (!Read_File_Bytes(candidate_path, file_bytes)) {
		return false;
	}
	return Try_Load_From_Bytes(file_bytes, reduction, allow_compression, flip_vertical, out);
}

} /* namespace */

bool Stb_Load_Texture(
	const char *path,
	unsigned reduction,
	bool allow_compression,
	bool try_dds,
	bool flip_vertical,
	StbLoadedTexture *out)
{
	if (path == nullptr || path[0] == '\0' || out == nullptr) {
		return false;
	}

	/*
	 * Match retail TextureLoader: prefer the requested path, then sibling files
	 * with other extensions (DDS, TGA, PNG, JPG, BMP). stb_image decodes the
	 * latter group when DDS is missing or not block-compressed.
	 */
	static const char *k_fallback_exts[] = {
		".dds",
		".tga",
		".png",
		".jpg",
		".jpeg",
		".bmp",
	};

	if (Try_Load_Candidate_Path(path, reduction, allow_compression, flip_vertical, out)) {
		return true;
	}

	char candidate[256];
	for (size_t i = 0; i < sizeof(k_fallback_exts) / sizeof(k_fallback_exts[0]); ++i) {
		if (!try_dds && strcmp(k_fallback_exts[i], ".dds") == 0) {
			continue;
		}
		strncpy(candidate, path, sizeof(candidate) - 1);
		candidate[sizeof(candidate) - 1] = '\0';
		Set_Path_Extension(candidate, sizeof(candidate), k_fallback_exts[i]);
		if (Paths_Equal_Ignore_Case(candidate, path)) {
			continue;
		}
		if (Try_Load_Candidate_Path(
				candidate,
				reduction,
				allow_compression,
				flip_vertical,
				out)) {
			return true;
		}
	}

	return false;
}

} /* namespace ww3d_vulkan */

#endif /* RENEGADE_VULKAN */
