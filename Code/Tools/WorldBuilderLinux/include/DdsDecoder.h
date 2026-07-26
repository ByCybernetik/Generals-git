#pragma once

#include <cstddef>
#include <vector>

namespace DdsDecoder
{

enum Format
{
	FORMAT_UNKNOWN = 0,
	FORMAT_DXT1,
	FORMAT_DXT2,
	FORMAT_DXT3,
	FORMAT_DXT4,
	FORMAT_DXT5,
	FORMAT_RGB,
	FORMAT_RGBA
};

struct MipLevel
{
	int width;
	int height;
	std::vector<unsigned char> rgba;

	MipLevel() : width(0), height(0) {}
};

struct Image
{
	int width;
	int height;
	Format format;
	std::vector<MipLevel> mips;

	Image() : width(0), height(0), format(FORMAT_UNKNOWN) {}
};

/** Decode every embedded DDS mip level to tightly packed RGBA8. */
bool decode(const unsigned char *data, size_t size, Image &out);

/** Cheap signature test used before falling back to stb_image. */
bool isDds(const unsigned char *data, size_t size);

const char *formatName(Format format);

} // namespace DdsDecoder
