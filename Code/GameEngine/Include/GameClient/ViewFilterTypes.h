/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#pragma once

#ifndef _VIEW_FILTER_TYPES_H_
#define _VIEW_FILTER_TYPES_H_

enum FilterTypes
{
	FT_NULL_FILTER=0,
	FT_VIEW_BW_FILTER,
	FT_VIEW_MOTION_BLUR_FILTER,
	FT_VIEW_CROSSFADE,
	FT_MAX
};

enum FilterModes
{
	FM_NULL_MODE = 0,

	FM_VIEW_BW_BLACK_AND_WHITE,
	FM_VIEW_BW_RED_AND_WHITE,
	FM_VIEW_BW_GREEN_AND_WHITE,

	FM_VIEW_CROSSFADE_CIRCLE,
	FM_VIEW_CROSSFADE_FB_MASK,

	FM_VIEW_MB_IN_AND_OUT_ALPHA,
	FM_VIEW_MB_IN_AND_OUT_SATURATE,
	FM_VIEW_MB_IN_ALPHA,
	FM_VIEW_MB_OUT_ALPHA,
	FM_VIEW_MB_IN_SATURATE,
	FM_VIEW_MB_OUT_SATURATE,
	FM_VIEW_MB_END_PAN_ALPHA,

	FM_VIEW_MB_PAN_ALPHA,
};

#endif
