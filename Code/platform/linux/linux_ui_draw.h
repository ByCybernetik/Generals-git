#pragma once

#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)

#include "GameClient/Display.h"
#include "GameClient/Image.h"

/** Stretch a narrow atlas tile across a width (avoids Vulkan tile seams). */
static inline void LinuxUIDrawImageRepeatedH(
	Display *display,
	const Image *image,
	Int startX,
	Int startY,
	Int endX,
	Int endY,
	Color color = 0xFFFFFFFF)
{
	if (display == NULL || image == NULL) {
		return;
	}
	display->drawImage(image, startX, startY, endX, endY, color);
}

/** Tile with 1px overlap between atlas sub-regions (hides Vulkan quad seams). */
static inline void LinuxUIDrawImageTiledHOverlap(
	Display *display,
	const Image *image,
	Int startX,
	Int startY,
	Int endX,
	Int endY,
	Color color = 0xFFFFFFFF)
{
	Int tileW;
	Int x;

	if (display == NULL || image == NULL || endX <= startX) {
		return;
	}

	tileW = image->getImageWidth();
	if (tileW <= 0) {
		return;
	}

	for (x = startX; x < endX; x += tileW) {
		Int drawStart = x;
		Int drawEnd = x + tileW;

		if (drawEnd > endX) {
			drawEnd = endX;
		}
		if (drawStart > startX) {
			drawStart -= 1;
		}
		display->drawImage(image, drawStart, startY, drawEnd, endY, color);
	}
}

/** Tile vertically with 1px overlap (hides Vulkan quad seams on vertical borders). */
static inline void LinuxUIDrawImageTiledVOverlap(
	Display *display,
	const Image *image,
	Int startX,
	Int startY,
	Int endX,
	Int endY,
	Color color = 0xFFFFFFFF)
{
	Int tileH;
	Int y;

	if (display == NULL || image == NULL || endY <= startY) {
		return;
	}

	tileH = image->getImageHeight();
	if (tileH <= 0) {
		return;
	}

	for (y = startY; y < endY; y += tileH) {
		Int drawStart = y;
		Int drawEnd = y + tileH;

		if (drawEnd > endY) {
			drawEnd = endY;
		}
		if (drawStart > startY) {
			drawStart -= 1;
		}
		display->drawImage(image, startX, drawStart, endX, drawEnd, color);
	}
}

#endif
