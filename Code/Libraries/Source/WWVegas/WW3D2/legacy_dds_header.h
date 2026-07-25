#pragma once

#include <cstdint>
#include <cstring>

/* Retail Generals .dds files store a Win32 DDSURFACEDESC2 blob (dwSize = 124).
 * LegacyDDSURFACEDESC2 uses void* and is larger on 64-bit; never fread into it. */
static const unsigned LEGACY_DDSURFACEDESC2_BYTES = 124u;

static inline uint32_t Legacy_Dds_Read_U32(const uint8_t *bytes, unsigned offset)
{
	uint32_t value = 0;
	memcpy(&value, bytes + offset, sizeof(value));
	return value;
}

struct LegacyDdsHeaderFields
{
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t mip_map_count = 0;
	uint32_t four_cc = 0;
	bool valid = false;
};

static inline LegacyDdsHeaderFields Legacy_Dds_Parse_Header(const uint8_t *bytes, size_t size)
{
	LegacyDdsHeaderFields fields;
	if (bytes == nullptr || size < LEGACY_DDSURFACEDESC2_BYTES) {
		return fields;
	}

	const uint32_t desc_size = Legacy_Dds_Read_U32(bytes, 0u);
	if (desc_size != LEGACY_DDSURFACEDESC2_BYTES) {
		return fields;
	}

	fields.height = Legacy_Dds_Read_U32(bytes, 8u);
	fields.width = Legacy_Dds_Read_U32(bytes, 12u);
	fields.mip_map_count = Legacy_Dds_Read_U32(bytes, 24u);
	if (fields.mip_map_count == 0) {
		fields.mip_map_count = 1;
	}
	/* DDPIXELFORMAT starts at byte 72; dwFourCC is +8 within that struct. */
	fields.four_cc = Legacy_Dds_Read_U32(bytes, 80u);
	fields.valid = true;
	return fields;
}
