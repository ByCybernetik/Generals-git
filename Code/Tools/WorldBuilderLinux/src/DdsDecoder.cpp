#include "DdsDecoder.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

namespace DdsDecoder
{
namespace
{

static const uint32_t DDS_MAGIC = 0x20534444u; /* "DDS " */
static const uint32_t DDPF_ALPHAPIXELS = 0x00000001u;
static const uint32_t DDPF_FOURCC = 0x00000004u;
static const uint32_t DDPF_RGB = 0x00000040u;

static uint32_t readU32(const unsigned char *p)
{
	uint32_t value;
	memcpy(&value, p, sizeof(value));
	return value;
}

static uint32_t fourcc(char a, char b, char c, char d)
{
	return (uint32_t)(unsigned char)a
		| ((uint32_t)(unsigned char)b << 8)
		| ((uint32_t)(unsigned char)c << 16)
		| ((uint32_t)(unsigned char)d << 24);
}

static unsigned char expand5(unsigned value)
{
	return (unsigned char)((value << 3) | (value >> 2));
}

static unsigned char expand6(unsigned value)
{
	return (unsigned char)((value << 2) | (value >> 4));
}

static void decode565(uint16_t color, unsigned char out[4])
{
	out[0] = expand5((color >> 11) & 31);
	out[1] = expand6((color >> 5) & 63);
	out[2] = expand5(color & 31);
	out[3] = 255;
}

static unsigned char lerpByte(unsigned a, unsigned b, unsigned wa, unsigned wb, unsigned divisor)
{
	return (unsigned char)((a * wa + b * wb + divisor / 2) / divisor);
}

static void buildColorTable(const unsigned char *block, bool allowTransparency,
	unsigned char colors[4][4])
{
	const uint16_t c0 = (uint16_t)(block[0] | (block[1] << 8));
	const uint16_t c1 = (uint16_t)(block[2] | (block[3] << 8));
	decode565(c0, colors[0]);
	decode565(c1, colors[1]);

	if (c0 > c1 || !allowTransparency)
	{
		for (int c = 0; c < 3; ++c)
		{
			colors[2][c] = lerpByte(colors[0][c], colors[1][c], 2, 1, 3);
			colors[3][c] = lerpByte(colors[0][c], colors[1][c], 1, 2, 3);
		}
		colors[2][3] = colors[3][3] = 255;
	}
	else
	{
		for (int c = 0; c < 3; ++c)
			colors[2][c] = lerpByte(colors[0][c], colors[1][c], 1, 1, 2);
		colors[2][3] = 255;
		memset(colors[3], 0, 4);
	}
}

static void writePixel(Image &out, unsigned x, unsigned y, const unsigned char rgba[4])
{
	if (x >= (unsigned)out.width || y >= (unsigned)out.height)
		return;
	const size_t offset = ((size_t)y * (size_t)out.width + x) * 4;
	memcpy(&out.rgba[offset], rgba, 4);
}

static void decodeColorBlock(const unsigned char *block, unsigned bx, unsigned by,
	bool allowTransparency, Image &out, const unsigned char *alpha)
{
	unsigned char colors[4][4];
	buildColorTable(block, allowTransparency, colors);
	const uint32_t indices = readU32(block + 4);
	for (unsigned py = 0; py < 4; ++py)
	{
		for (unsigned px = 0; px < 4; ++px)
		{
			const unsigned pixel = py * 4 + px;
			unsigned char value[4];
			memcpy(value, colors[(indices >> (pixel * 2)) & 3], 4);
			if (alpha)
				value[3] = alpha[pixel];
			writePixel(out, bx * 4 + px, by * 4 + py, value);
		}
	}
}

static void decodeDxt3Alpha(const unsigned char *block, unsigned char alpha[16])
{
	for (unsigned i = 0; i < 16; ++i)
	{
		const unsigned nibble = (block[i / 2] >> ((i & 1) * 4)) & 15;
		alpha[i] = (unsigned char)(nibble * 17);
	}
}

static void decodeDxt5Alpha(const unsigned char *block, unsigned char alpha[16])
{
	unsigned char table[8];
	table[0] = block[0];
	table[1] = block[1];
	if (table[0] > table[1])
	{
		for (unsigned i = 1; i <= 6; ++i)
			table[i + 1] = (unsigned char)(((7 - i) * table[0] + i * table[1] + 3) / 7);
	}
	else
	{
		for (unsigned i = 1; i <= 4; ++i)
			table[i + 1] = (unsigned char)(((5 - i) * table[0] + i * table[1] + 2) / 5);
		table[6] = 0;
		table[7] = 255;
	}

	uint64_t bits = 0;
	for (unsigned i = 0; i < 6; ++i)
		bits |= (uint64_t)block[2 + i] << (i * 8);
	for (unsigned i = 0; i < 16; ++i)
		alpha[i] = table[(bits >> (i * 3)) & 7];
}

static void unpremultiply(Image &out)
{
	for (size_t i = 0; i < out.rgba.size(); i += 4)
	{
		const unsigned a = out.rgba[i + 3];
		if (a == 0)
		{
			out.rgba[i + 0] = out.rgba[i + 1] = out.rgba[i + 2] = 0;
			continue;
		}
		for (int c = 0; c < 3; ++c)
			out.rgba[i + c] =
				(unsigned char)std::min(255u, ((unsigned)out.rgba[i + c] * 255u + a / 2) / a);
	}
}

static bool decodeBlocks(const unsigned char *src, size_t srcSize, Image &out)
{
	const unsigned blocksX = ((unsigned)out.width + 3) / 4;
	const unsigned blocksY = ((unsigned)out.height + 3) / 4;
	const size_t blockBytes = out.format == FORMAT_DXT1 ? 8 : 16;
	if ((size_t)blocksX > std::numeric_limits<size_t>::max() / blocksY
		|| (size_t)blocksX * blocksY > srcSize / blockBytes)
		return false;

	for (unsigned by = 0; by < blocksY; ++by)
	{
		for (unsigned bx = 0; bx < blocksX; ++bx)
		{
			const unsigned char *block = src + ((size_t)by * blocksX + bx) * blockBytes;
			if (out.format == FORMAT_DXT1)
			{
				decodeColorBlock(block, bx, by, true, out, NULL);
				continue;
			}

			unsigned char alpha[16];
			if (out.format == FORMAT_DXT2 || out.format == FORMAT_DXT3)
				decodeDxt3Alpha(block, alpha);
			else
				decodeDxt5Alpha(block, alpha);
			decodeColorBlock(block + 8, bx, by, false, out, alpha);
		}
	}

	if (out.format == FORMAT_DXT2 || out.format == FORMAT_DXT4)
		unpremultiply(out);
	return true;
}

static unsigned char extractMasked(uint32_t pixel, uint32_t mask, unsigned char defaultValue)
{
	if (!mask)
		return defaultValue;
	unsigned shift = 0;
	while (shift < 32 && ((mask >> shift) & 1u) == 0u)
		++shift;
	const uint32_t shiftedMask = mask >> shift;
	const uint32_t value = (pixel & mask) >> shift;
	return shiftedMask ? (unsigned char)((value * 255u + shiftedMask / 2u) / shiftedMask) : defaultValue;
}

static bool decodeRgb(const unsigned char *src, size_t srcSize, unsigned bits,
	uint32_t rMask, uint32_t gMask, uint32_t bMask, uint32_t aMask, Image &out)
{
	if (bits != 16 && bits != 24 && bits != 32)
		return false;
	const size_t bytesPerPixel = bits / 8;
	const size_t rowBytes = (size_t)out.width * bytesPerPixel;
	if ((size_t)out.height > srcSize / rowBytes)
		return false;

	for (int y = 0; y < out.height; ++y)
	{
		const unsigned char *row = src + (size_t)y * rowBytes;
		for (int x = 0; x < out.width; ++x)
		{
			uint32_t pixel = 0;
			memcpy(&pixel, row + (size_t)x * bytesPerPixel, bytesPerPixel);
			unsigned char value[4] = {
				extractMasked(pixel, rMask, 0),
				extractMasked(pixel, gMask, 0),
				extractMasked(pixel, bMask, 0),
				extractMasked(pixel, aMask, 255)
			};
			writePixel(out, (unsigned)x, (unsigned)y, value);
		}
	}
	return true;
}

} // namespace

bool isDds(const unsigned char *data, size_t size)
{
	return data && size >= 4 && readU32(data) == DDS_MAGIC;
}

bool decode(const unsigned char *data, size_t size, Image &out)
{
	out = Image();
	if (!isDds(data, size) || size < 128 || readU32(data + 4) != 124)
		return false;

	const uint32_t height = readU32(data + 12);
	const uint32_t width = readU32(data + 16);
	if (!width || !height || width > 32768 || height > 32768
		|| (size_t)width > std::numeric_limits<size_t>::max() / height / 4)
		return false;

	const uint32_t pfSize = readU32(data + 76);
	const uint32_t pfFlags = readU32(data + 80);
	const uint32_t code = readU32(data + 84);
	uint32_t bits = readU32(data + 88);
	uint32_t rMask = readU32(data + 92);
	uint32_t gMask = readU32(data + 96);
	uint32_t bMask = readU32(data + 100);
	uint32_t aMask = readU32(data + 104);
	if (pfSize != 32)
		return false;

	size_t dataOffset = 128;
	Format format = FORMAT_UNKNOWN;
	if (pfFlags & DDPF_FOURCC)
	{
		if (code == fourcc('D', 'X', 'T', '1')) format = FORMAT_DXT1;
		else if (code == fourcc('D', 'X', 'T', '2')) format = FORMAT_DXT2;
		else if (code == fourcc('D', 'X', 'T', '3')) format = FORMAT_DXT3;
		else if (code == fourcc('D', 'X', 'T', '4')) format = FORMAT_DXT4;
		else if (code == fourcc('D', 'X', 'T', '5')) format = FORMAT_DXT5;
		else if (code == fourcc('D', 'X', '1', '0'))
		{
			if (size < 148)
				return false;
			const uint32_t dxgi = readU32(data + 128);
			dataOffset += 20;
			if (dxgi == 71 || dxgi == 72) format = FORMAT_DXT1;
			else if (dxgi == 74 || dxgi == 75) format = FORMAT_DXT3;
			else if (dxgi == 77 || dxgi == 78) format = FORMAT_DXT5;
			else if (dxgi == 28 || dxgi == 29)
			{
				format = FORMAT_RGBA;
				bits = 32;
				rMask = 0x000000ffu;
				gMask = 0x0000ff00u;
				bMask = 0x00ff0000u;
				aMask = 0xff000000u;
			}
			else if (dxgi == 87 || dxgi == 91)
			{
				format = FORMAT_RGBA;
				bits = 32;
				rMask = 0x00ff0000u;
				gMask = 0x0000ff00u;
				bMask = 0x000000ffu;
				aMask = 0xff000000u;
			}
			else return false;
		}
		else return false;
	}
	else if (pfFlags & DDPF_RGB)
	{
		format = (pfFlags & DDPF_ALPHAPIXELS) || aMask ? FORMAT_RGBA : FORMAT_RGB;
	}
	else
	{
		return false;
	}

	if (dataOffset > size)
		return false;
	out.width = (int)width;
	out.height = (int)height;
	out.format = format;
	out.rgba.assign((size_t)width * height * 4, 0);

	const unsigned char *pixels = data + dataOffset;
	const size_t pixelsSize = size - dataOffset;
	if (format >= FORMAT_DXT1 && format <= FORMAT_DXT5)
		return decodeBlocks(pixels, pixelsSize, out);
	if (format == FORMAT_RGB || format == FORMAT_RGBA)
		return decodeRgb(pixels, pixelsSize, bits, rMask, gMask, bMask, aMask, out);
	return false;
}

const char *formatName(Format format)
{
	switch (format)
	{
	case FORMAT_DXT1: return "DXT1";
	case FORMAT_DXT2: return "DXT2";
	case FORMAT_DXT3: return "DXT3";
	case FORMAT_DXT4: return "DXT4";
	case FORMAT_DXT5: return "DXT5";
	case FORMAT_RGB: return "RGB";
	case FORMAT_RGBA: return "RGBA";
	default: return "unknown";
	}
}

} // namespace DdsDecoder
