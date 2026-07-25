#include "linux_ani_cursor.h"
#include "sdl3_host.h"
#include "mmsystem.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <vector>

namespace {

struct LinuxAniCursor {
	std::vector<SDL_Cursor *> frames;
	std::vector<uint32_t> rates;
	std::vector<uint32_t> sequence;
	uint32_t num_steps = 1;
	uint32_t current_step = 0;
	uint32_t last_advance_ms = 0;
	uint16_t hot_x = 0;
	uint16_t hot_y = 0;
};

static LinuxAniCursor *g_active_cursor = nullptr;
static HCURSOR g_last_cursor_handle = nullptr;
static std::vector<LinuxAniCursor *> g_loaded_cursors;

static uint32_t ReadLe32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t ReadLe16(const uint8_t *p)
{
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static bool StrIeq(const char *a, const char *b)
{
	if (a == nullptr || b == nullptr) {
		return false;
	}
	while (*a && *b) {
		if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) {
			return false;
		}
		++a;
		++b;
	}
	return *a == *b;
}

static void NormalizeSlashes(std::string &path)
{
	for (size_t i = 0; i < path.size(); ++i) {
		if (path[i] == '\\') {
			path[i] = '/';
		}
	}
}

static const char *BaseName(const char *path)
{
	const char *slash = strrchr(path, '/');
	const char *bslash = strrchr(path, '\\');
	const char *base = path;
	if (slash != nullptr && slash + 1 > base) {
		base = slash + 1;
	}
	if (bslash != nullptr && bslash + 1 > base) {
		base = bslash + 1;
	}
	return base;
}

static bool FileExists(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f == nullptr) {
		return false;
	}
	fclose(f);
	return true;
}

static bool FindCursorPathCaseInsensitive(const char *requested, char *out, size_t out_size)
{
	const char *base = BaseName(requested);
	if (base == nullptr || *base == '\0' || out_size == 0) {
		return false;
	}

	static const char *const kDirs[] = {
		"Data/Cursors",
		"data/cursors",
		"Data/cursors",
		"DATA/CURSORS",
		nullptr,
	};

	for (int i = 0; kDirs[i] != nullptr; ++i) {
		DIR *dir = opendir(kDirs[i]);
		if (dir == nullptr) {
			continue;
		}
		for (dirent *ent = readdir(dir); ent != nullptr; ent = readdir(dir)) {
			if (ent->d_name[0] == '.') {
				continue;
			}
			if (!StrIeq(ent->d_name, base)) {
				continue;
			}
			snprintf(out, out_size, "%s/%s", kDirs[i], ent->d_name);
			closedir(dir);
			return true;
		}
		closedir(dir);
	}
	return false;
}

static bool ResolveCursorPath(const char *path, std::string &resolved)
{
	if (path == nullptr || *path == '\0') {
		return false;
	}

	resolved = path;
	NormalizeSlashes(resolved);
	if (FileExists(resolved.c_str())) {
		return true;
	}

	char found[512];
	if (FindCursorPathCaseInsensitive(resolved.c_str(), found, sizeof(found))) {
		resolved = found;
		return true;
	}

	return false;
}

static bool ReadFileBytes(const std::string &path, std::vector<uint8_t> &out)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (f == nullptr) {
		return false;
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return false;
	}
	const long size = ftell(f);
	if (size <= 0) {
		fclose(f);
		return false;
	}
	if (fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return false;
	}
	out.resize((size_t)size);
	const size_t read = fread(out.data(), 1, out.size(), f);
	fclose(f);
	return read == out.size();
}

static SDL_Surface *CreateRgbaSurface(int width, int height)
{
	return SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
}

static void SetPixelRgba(uint8_t *pixels, int pitch, int width, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	if (x < 0 || y < 0 || x >= width) {
		return;
	}
	uint8_t *dst = pixels + y * pitch + x * 4;
	dst[0] = r;
	dst[1] = g;
	dst[2] = b;
	dst[3] = a;
}

static bool DecodeCursorDib(
	const uint8_t *dib,
	size_t dib_size,
	int &out_hot_x,
	int &out_hot_y,
	SDL_Surface **out_surface)
{
	if (dib_size < 40) {
		return false;
	}

	const uint32_t bi_size = ReadLe32(dib + 0);
	const int width = (int)ReadLe32(dib + 4);
	const int dib_height = (int)ReadLe32(dib + 8);
	const uint16_t planes = ReadLe16(dib + 12);
	const uint16_t bit_count = ReadLe16(dib + 14);
	if (bi_size < 40 || width <= 0 || dib_height <= 0 || (dib_height % 2) != 0 || planes != 1) {
		return false;
	}

	const int height = dib_height / 2;
	const uint8_t *palette = dib + bi_size;
	size_t offset = bi_size;

	int palette_colors = 0;
	if (bit_count <= 8) {
		palette_colors = 1 << bit_count;
		offset += (size_t)palette_colors * 4;
	}

	const int xor_row_bytes = ((width * bit_count + 31) / 32) * 4;
	const int and_row_bytes = ((width + 31) / 32) * 4;
	const size_t xor_size = (size_t)xor_row_bytes * (size_t)height;
	const size_t and_size = (size_t)and_row_bytes * (size_t)height;
	if (offset + xor_size + and_size > dib_size) {
		return false;
	}

	const uint8_t *xor_bits = dib + offset;
	const uint8_t *and_bits = xor_bits + xor_size;

	SDL_Surface *surface = CreateRgbaSurface(width, height);
	if (surface == nullptr) {
		return false;
	}

	if (!SDL_LockSurface(surface)) {
		SDL_DestroySurface(surface);
		return false;
	}

	uint8_t *pixels = (uint8_t *)surface->pixels;
	const int pitch = surface->pitch;

	for (int y = 0; y < height; ++y) {
		const int src_y = height - 1 - y;
		const uint8_t *xor_row = xor_bits + (size_t)src_y * (size_t)xor_row_bytes;
		const uint8_t *and_row = and_bits + (size_t)src_y * (size_t)and_row_bytes;
		for (int x = 0; x < width; ++x) {
			uint8_t r = 0;
			uint8_t g = 0;
			uint8_t b = 0;
			uint8_t a = 255;

			if (bit_count == 32) {
				const uint8_t *px = xor_row + x * 4;
				b = px[0];
				g = px[1];
				r = px[2];
				a = px[3];
			} else if (bit_count == 24) {
				const uint8_t *px = xor_row + x * 3;
				b = px[0];
				g = px[1];
				r = px[2];
			} else if (bit_count == 8) {
				const uint8_t index = xor_row[x];
				const uint8_t *color = palette + index * 4;
				b = color[0];
				g = color[1];
				r = color[2];
			} else if (bit_count == 4) {
				const uint8_t packed = xor_row[x / 2];
				const uint8_t index = (x & 1) ? (packed & 0x0f) : ((packed >> 4) & 0x0f);
				const uint8_t *color = palette + index * 4;
				b = color[0];
				g = color[1];
				r = color[2];
			} else if (bit_count == 1) {
				const uint8_t packed = xor_row[x / 8];
				const uint8_t bit = (packed >> (7 - (x & 7))) & 1;
				const uint8_t *color = palette + bit * 4;
				b = color[0];
				g = color[1];
				r = color[2];
			}

			if (bit_count != 32) {
				const uint8_t packed = and_row[x / 8];
				const uint8_t and_bit = (packed >> (7 - (x & 7))) & 1;
				if (and_bit) {
					a = 0;
				}
			}

			SetPixelRgba(pixels, pitch, width, x, y, r, g, b, a);
		}
	}

	SDL_UnlockSurface(surface);

	*out_surface = surface;
	(void)out_hot_x;
	(void)out_hot_y;
	return true;
}

static bool ParseCurOrIcoResource(
	const uint8_t *data,
	size_t size,
	SDL_Cursor **out_cursor,
	uint16_t &hot_x,
	uint16_t &hot_y)
{
	if (size < 6 + 16) {
		return false;
	}

	const uint16_t type = ReadLe16(data + 2);
	const uint16_t count = ReadLe16(data + 4);
	if (count < 1) {
		return false;
	}

	const uint8_t *entry = data + 6;
	hot_x = (type == 2) ? ReadLe16(entry + 4) : 0;
	hot_y = (type == 2) ? ReadLe16(entry + 6) : 0;
	const uint32_t image_offset = ReadLe32(entry + 12);
	if (image_offset >= size) {
		return false;
	}

	SDL_Surface *surface = nullptr;
	if (!DecodeCursorDib(data + image_offset, size - image_offset, (int &)hot_x, (int &)hot_y, &surface)) {
		return false;
	}

	SDL_Cursor *cursor = SDL_CreateColorCursor(surface, hot_x, hot_y);
	SDL_DestroySurface(surface);
	if (cursor == nullptr) {
		return false;
	}

	*out_cursor = cursor;
	return true;
}

static bool ParseAniBytes(const std::vector<uint8_t> &bytes, LinuxAniCursor &cursor)
{
	if (bytes.size() < 12 || memcmp(bytes.data(), "RIFF", 4) != 0 || memcmp(bytes.data() + 8, "ACON", 4) != 0) {
		return false;
	}

	const uint8_t *data = bytes.data();
	const size_t size = bytes.size();
	size_t offset = 12;

	uint32_t num_frames = 0;
	uint32_t num_steps = 0;
	bool has_sequence = false;

	while (offset + 8 <= size) {
		const char *chunk_id = (const char *)(data + offset);
		const uint32_t chunk_size = ReadLe32(data + offset + 4);
		const uint8_t *chunk = data + offset + 8;
		const size_t chunk_end = offset + 8 + chunk_size;

		if (chunk_end > size) {
			break;
		}

		if (memcmp(chunk_id, "anih", 4) == 0 && chunk_size >= 36) {
			num_frames = ReadLe32(chunk + 4);
			num_steps = ReadLe32(chunk + 8);
			const uint32_t flags = ReadLe32(chunk + 24);
			has_sequence = (flags & 0x2u) != 0u;
		} else if (memcmp(chunk_id, "rate", 4) == 0) {
			const uint32_t count = chunk_size / 4;
			cursor.rates.resize(count);
			for (uint32_t i = 0; i < count; ++i) {
				cursor.rates[i] = ReadLe32(chunk + i * 4);
			}
		} else if (memcmp(chunk_id, "seq ", 4) == 0) {
			const uint32_t count = chunk_size / 4;
			cursor.sequence.resize(count);
			for (uint32_t i = 0; i < count; ++i) {
				cursor.sequence[i] = ReadLe32(chunk + i * 4);
			}
		} else if (memcmp(chunk_id, "LIST", 4) == 0 && chunk_size >= 4 && memcmp(chunk, "fram", 4) == 0) {
			size_t frame_offset = offset + 12;
			while (frame_offset + 8 <= chunk_end) {
				if (memcmp(data + frame_offset, "icon", 4) != 0) {
					break;
				}
				const uint32_t icon_size = ReadLe32(data + frame_offset + 4);
				const uint8_t *icon_data = data + frame_offset + 8;
				if (frame_offset + 8 + icon_size > chunk_end) {
					break;
				}

				SDL_Cursor *frame_cursor = nullptr;
				uint16_t hot_x = 0;
				uint16_t hot_y = 0;
				if (ParseCurOrIcoResource(icon_data, icon_size, &frame_cursor, hot_x, hot_y)) {
					if (cursor.frames.empty()) {
						cursor.hot_x = hot_x;
						cursor.hot_y = hot_y;
					}
					cursor.frames.push_back(frame_cursor);
				}

				size_t advance = 8 + icon_size;
				if (advance & 1u) {
					++advance;
				}
				frame_offset += advance;
			}
		}

		size_t advance = 8 + chunk_size;
		if (advance & 1u) {
			++advance;
		}
		offset += advance;
	}

	if (cursor.frames.empty()) {
		return false;
	}

	if (num_steps == 0) {
		num_steps = (uint32_t)cursor.frames.size();
	}
	if (num_frames == 0) {
		num_frames = (uint32_t)cursor.frames.size();
	}
	cursor.num_steps = has_sequence && !cursor.sequence.empty() ? (uint32_t)cursor.sequence.size() : num_steps;
	if (cursor.num_steps == 0) {
		cursor.num_steps = 1;
	}
	if (cursor.rates.empty()) {
		cursor.rates.assign(cursor.num_steps, 4);
	}
	(void)num_frames;
	return true;
}

static void ApplyCursorFrame(LinuxAniCursor *cursor, uint32_t step)
{
	if (cursor == nullptr || cursor->frames.empty()) {
		return;
	}

	uint32_t frame_index = step;
	if (!cursor->sequence.empty() && step < cursor->sequence.size()) {
		frame_index = cursor->sequence[step];
	}
	if (frame_index >= cursor->frames.size()) {
		frame_index = frame_index % (uint32_t)cursor->frames.size();
	}

	SDL_Cursor *sdl_cursor = cursor->frames[frame_index];
	if (sdl_cursor != nullptr) {
		SDL_SetCursor(sdl_cursor);
	}
}

static void DestroyAniCursor(LinuxAniCursor *cursor)
{
	if (cursor == nullptr) {
		return;
	}
	for (size_t i = 0; i < cursor->frames.size(); ++i) {
		if (cursor->frames[i] != nullptr) {
			SDL_DestroyCursor(cursor->frames[i]);
		}
	}
	delete cursor;
}

} /* namespace */

extern "C" HCURSOR Linux_Load_Cursor_From_FileA(LPCSTR fileName)
{
	std::string resolved;
	if (!ResolveCursorPath(fileName, resolved)) {
		return nullptr;
	}

	std::vector<uint8_t> bytes;
	if (!ReadFileBytes(resolved, bytes)) {
		return nullptr;
	}

	LinuxAniCursor *cursor = new LinuxAniCursor();
	if (!ParseAniBytes(bytes, *cursor)) {
		delete cursor;
		return nullptr;
	}

	g_loaded_cursors.push_back(cursor);


	return (HCURSOR)cursor;
}

extern "C" HCURSOR Linux_Set_Cursor(HCURSOR hCursor)
{
	HCURSOR previous = g_last_cursor_handle;
	g_last_cursor_handle = hCursor;

	if (hCursor == nullptr) {
		g_active_cursor = nullptr;
		return previous;
	}

	g_active_cursor = (LinuxAniCursor *)hCursor;
	if (g_active_cursor == nullptr || g_active_cursor->frames.empty()) {
		return previous;
	}

	g_active_cursor->current_step = 0;
	g_active_cursor->last_advance_ms = timeGetTime();
	ApplyCursorFrame(g_active_cursor, 0);
	return previous;
}

extern "C" void Linux_Ani_Cursor_Tick_Active(void)
{
	if (g_active_cursor == nullptr || g_active_cursor->num_steps <= 1) {
		return;
	}

	const uint32_t now = timeGetTime();
	const uint32_t step = g_active_cursor->current_step;
	uint32_t jiffies = 4;
	if (step < g_active_cursor->rates.size()) {
		jiffies = g_active_cursor->rates[step];
	}
	if (jiffies == 0) {
		jiffies = 1;
	}

	const uint32_t frame_ms = (jiffies * 1000u) / 60u;
	if (frame_ms == 0 || now - g_active_cursor->last_advance_ms < frame_ms) {
		return;
	}

	g_active_cursor->current_step = (step + 1) % g_active_cursor->num_steps;
	g_active_cursor->last_advance_ms = now;
	ApplyCursorFrame(g_active_cursor, g_active_cursor->current_step);
}

extern "C" void Linux_Ani_Cursor_Shutdown(void)
{
	for (size_t i = 0; i < g_loaded_cursors.size(); ++i) {
		if (g_loaded_cursors[i] == g_active_cursor) {
			g_active_cursor = nullptr;
		}
		DestroyAniCursor(g_loaded_cursors[i]);
	}
	g_loaded_cursors.clear();
	g_last_cursor_handle = nullptr;
}
