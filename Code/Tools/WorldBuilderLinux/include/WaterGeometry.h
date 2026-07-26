#pragma once

#include <cstdint>
#include <vector>

/**
 * Bake WB/game water geometry from PolygonTrigger water areas.
 * Lakes: WaterRenderObjClass::drawTrapezoidWater tessellation.
 * Rivers: WaterRenderObjClass::drawRiverWater strip.
 */
struct WaterMeshVertex
{
	float px, py, pz;
	float u, v;
	float u2, v2;
	float r, g, b, a;
	float isRiver;
};

struct WaterBakeStats
{
	int lakeAreas = 0;
	int riverAreas = 0;
	int verts = 0;
	int tris = 0;
};

class WorldHeightMap;

class WaterGeometry
{
public:
	/** Fill verts/indices from global PolygonTrigger list (water areas only). */
	static WaterBakeStats bake(const WorldHeightMap *hm,
		std::vector<WaterMeshVertex> &outVerts,
		std::vector<uint32_t> &outIndices);
};
