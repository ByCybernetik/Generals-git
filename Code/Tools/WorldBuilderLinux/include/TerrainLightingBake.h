#pragma once

class WorldHeightMap;

/**
 * CPU lighting helpers mirroring HeightMapRenderObjClass::doTheLight /
 * getStaticDiffuse and W3DDisplay object lights — used by the Vulkan MapViewport.
 */
namespace TerrainLightingBake
{

/** Copy m_terrainLighting[tod] into the active ambient/diffuse/lightPos arrays. */
void applyActiveTimeOfDay();

/** Log active terrain + object light summary (stderr). */
void logActiveLights(const char *tag);

/**
 * Bake PRELIT diffuse for a heightmap index (global lights only, no point lights).
 * Matches HeightMapRenderObjClass::getStaticDiffuse with a NULL lights iterator.
 */
void bakeTerrainIndex(const WorldHeightMap *hm, int x, int y, float &outR, float &outG, float &outB);

/**
 * Bake terrain lighting for a world-space XY (playable origin at 0,0).
 * Converts through borderSize like MapViewport height sampling.
 */
void bakeTerrainWorld(const WorldHeightMap *hm, float worldX, float worldY, float &outR, float &outG,
	float &outB);

/**
 * Bake object/scene lighting for a world-space unit normal using
 * m_terrainObjectsLighting[tod] (ambient from light 0 + diffuse N·L).
 */
void bakeObjectNormal(float nx, float ny, float nz, float &outR, float &outG, float &outB);

/** Flat water shade factor used by WaterRenderObjClass (ambient[0] + max(-Lz,0)*diffuse). */
void bakeWaterShade(float &outR, float &outG, float &outB);

} // namespace TerrainLightingBake
