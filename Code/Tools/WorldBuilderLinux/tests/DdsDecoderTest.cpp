#include "DdsDecoder.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace
{

static void putU32(std::vector<unsigned char> &data, size_t offset, unsigned value)
{
	data[offset + 0] = (unsigned char)(value & 255);
	data[offset + 1] = (unsigned char)((value >> 8) & 255);
	data[offset + 2] = (unsigned char)((value >> 16) & 255);
	data[offset + 3] = (unsigned char)((value >> 24) & 255);
}

static unsigned fourcc(char a, char b, char c, char d)
{
	return (unsigned)(unsigned char)a
		| ((unsigned)(unsigned char)b << 8)
		| ((unsigned)(unsigned char)c << 16)
		| ((unsigned)(unsigned char)d << 24);
}

static std::vector<unsigned char> compressedDds(const char *code, const unsigned char *block, size_t blockSize)
{
	std::vector<unsigned char> data(128 + blockSize, 0);
	putU32(data, 0, fourcc('D', 'D', 'S', ' '));
	putU32(data, 4, 124);
	putU32(data, 12, 4);
	putU32(data, 16, 4);
	putU32(data, 76, 32);
	putU32(data, 80, 4);
	putU32(data, 84, fourcc(code[0], code[1], code[2], code[3]));
	memcpy(&data[128], block, blockSize);
	return data;
}

static bool pixelIs(const DdsDecoder::Image &image, int x, int y,
	unsigned r, unsigned g, unsigned b, unsigned a)
{
	const size_t p = ((size_t)y * image.width + x) * 4;
	return image.rgba[p] == r && image.rgba[p + 1] == g
		&& image.rgba[p + 2] == b && image.rgba[p + 3] == a;
}

static bool testDxt1()
{
	const unsigned char block[8] = { 0x00, 0xf8, 0xe0, 0x07, 0, 0, 0, 0 };
	const std::vector<unsigned char> dds = compressedDds("DXT1", block, sizeof(block));
	DdsDecoder::Image image;
	return DdsDecoder::decode(dds.data(), dds.size(), image)
		&& image.width == 4 && image.height == 4
		&& image.format == DdsDecoder::FORMAT_DXT1
		&& pixelIs(image, 0, 0, 255, 0, 0, 255);
}

static bool testDxt3()
{
	unsigned char block[16] = {};
	memset(block, 0xff, 8);
	block[8] = 0x00; block[9] = 0xf8;
	block[10] = 0xe0; block[11] = 0x07;
	const std::vector<unsigned char> dds = compressedDds("DXT3", block, sizeof(block));
	DdsDecoder::Image image;
	return DdsDecoder::decode(dds.data(), dds.size(), image)
		&& image.format == DdsDecoder::FORMAT_DXT3
		&& pixelIs(image, 3, 3, 255, 0, 0, 255);
}

static bool testDxt5()
{
	unsigned char block[16] = {};
	block[0] = 255;
	block[1] = 0;
	block[8] = 0x00; block[9] = 0xf8;
	block[10] = 0xe0; block[11] = 0x07;
	const std::vector<unsigned char> dds = compressedDds("DXT5", block, sizeof(block));
	DdsDecoder::Image image;
	return DdsDecoder::decode(dds.data(), dds.size(), image)
		&& image.format == DdsDecoder::FORMAT_DXT5
		&& pixelIs(image, 1, 2, 255, 0, 0, 255);
}

static bool testBgra()
{
	std::vector<unsigned char> data(128 + 8, 0);
	putU32(data, 0, fourcc('D', 'D', 'S', ' '));
	putU32(data, 4, 124);
	putU32(data, 12, 1);
	putU32(data, 16, 2);
	putU32(data, 76, 32);
	putU32(data, 80, 0x41); /* RGB | ALPHAPIXELS */
	putU32(data, 88, 32);
	putU32(data, 92, 0x00ff0000);
	putU32(data, 96, 0x0000ff00);
	putU32(data, 100, 0x000000ff);
	putU32(data, 104, 0xff000000);
	const unsigned char pixels[8] = { 30, 20, 10, 40, 3, 2, 1, 4 };
	memcpy(&data[128], pixels, sizeof(pixels));
	DdsDecoder::Image image;
	return DdsDecoder::decode(data.data(), data.size(), image)
		&& image.format == DdsDecoder::FORMAT_RGBA
		&& pixelIs(image, 0, 0, 10, 20, 30, 40)
		&& pixelIs(image, 1, 0, 1, 2, 3, 4);
}

static bool testPaddedRgb24()
{
	std::vector<unsigned char> data(128 + 8, 0);
	putU32(data, 0, fourcc('D', 'D', 'S', ' '));
	putU32(data, 4, 124);
	putU32(data, 12, 2);
	putU32(data, 16, 1);
	putU32(data, 20, 4); /* DDS row pitch includes one padding byte. */
	putU32(data, 76, 32);
	putU32(data, 80, 0x40);
	putU32(data, 88, 24);
	putU32(data, 92, 0x00ff0000);
	putU32(data, 96, 0x0000ff00);
	putU32(data, 100, 0x000000ff);
	const unsigned char pixels[8] = { 30, 20, 10, 0, 3, 2, 1, 0 };
	memcpy(&data[128], pixels, sizeof(pixels));
	DdsDecoder::Image image;
	return DdsDecoder::decode(data.data(), data.size(), image)
		&& pixelIs(image, 0, 0, 10, 20, 30, 255)
		&& pixelIs(image, 0, 1, 1, 2, 3, 255);
}

static bool testRejectsTruncatedDxt()
{
	const unsigned char block[7] = {};
	const std::vector<unsigned char> dds = compressedDds("DXT1", block, sizeof(block));
	DdsDecoder::Image image;
	return !DdsDecoder::decode(dds.data(), dds.size(), image);
}

} // namespace

int main()
{
	if (!testDxt1() || !testDxt3() || !testDxt5() || !testBgra()
		|| !testPaddedRgb24() || !testRejectsTruncatedDxt())
	{
		fprintf(stderr, "DDS decoder test failed\n");
		return 1;
	}
	printf("DDS decoder tests passed\n");
	return 0;
}
