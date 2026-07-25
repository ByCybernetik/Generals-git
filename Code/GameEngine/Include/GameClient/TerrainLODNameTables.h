/*
**	INI name table for TerrainLOD. Include after DEFINE_TERRAIN_LOD_NAMES (see GlobalData.cpp).
*/

#ifndef __TERRAIN_LOD_NAME_TABLES_H_
#define __TERRAIN_LOD_NAME_TABLES_H_

#ifdef DEFINE_TERRAIN_LOD_NAMES
static char *TerrainLODNames[] =
{
	"NONE",
	"MIN",
	"STRETCH_NO_CLOUDS",
	"HALF_CLOUDS",
	"NO_CLOUDS",
	"STRETCH_CLOUDS",
	"NO_WATER",
	"MAX",
	"AUTOMATIC",
	"DISABLE",

	NULL
};
#endif

#endif
