#include "PreRTS.h"
#include "TerrainLightingBake.h"

#include "Common/GlobalData.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace TerrainLightingBake
{
namespace
{

void clamp01(float &v)
{
	if (v < 0.f)
		v = 0.f;
	else if (v > 1.f)
		v = 1.f;
}

void doTheLight(float nx, float ny, float nz, float &outR, float &outG, float &outB)
{
	outR = outG = outB = 1.f;
	if (!TheGlobalData)
		return;

	float shadeR = TheGlobalData->m_terrainAmbient[0].red;
	float shadeG = TheGlobalData->m_terrainAmbient[0].green;
	float shadeB = TheGlobalData->m_terrainAmbient[0].blue;

	for (Int lightIndex = 0; lightIndex < TheGlobalData->m_numGlobalLights; lightIndex++)
	{
		const float lx = -TheGlobalData->m_terrainLightPos[lightIndex].x;
		const float ly = -TheGlobalData->m_terrainLightPos[lightIndex].y;
		const float lz = -TheGlobalData->m_terrainLightPos[lightIndex].z;
		float shade = lx * nx + ly * ny + lz * nz;
		if (shade > 1.f)
			shade = 1.f;
		if (shade < 0.f)
			shade = 0.f;
		shadeR += shade * TheGlobalData->m_terrainDiffuse[lightIndex].red;
		shadeG += shade * TheGlobalData->m_terrainDiffuse[lightIndex].green;
		shadeB += shade * TheGlobalData->m_terrainDiffuse[lightIndex].blue;
	}

	clamp01(shadeR);
	clamp01(shadeG);
	clamp01(shadeB);
	outR = shadeR;
	outG = shadeG;
	outB = shadeB;
}

void terrainNormalAt(const WorldHeightMap *hm, int x, int y, float &nx, float &ny, float &nz)
{
	const Int extentX = hm->getXExtent();
	const Int extentY = hm->getYExtent();
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x >= extentX)
		x = extentX - 1;
	if (y >= extentY)
		y = extentY - 1;

	const Int cellOffset = 1;
	Int vn0 = y - cellOffset;
	Int vp1 = y + cellOffset;
	Int un0 = x - cellOffset;
	Int up1 = x + cellOffset;
	if (vn0 < 0)
		vn0 = 0;
	if (un0 < 0)
		un0 = 0;
	if (vp1 >= extentY)
		vp1 = extentY - 1;
	if (up1 >= extentX)
		up1 = extentX - 1;

	/* Same finite-difference cross product as HeightMapRenderObjClass::getStaticDiffuse. */
	const float l2rX = 2.f * MAP_XY_FACTOR;
	const float l2rY = 0.f;
	const float l2rZ = MAP_HEIGHT_SCALE * (float)(hm->getHeight(up1, y) - hm->getHeight(un0, y));
	const float n2fX = 0.f;
	const float n2fY = 2.f * MAP_XY_FACTOR;
	const float n2fZ = MAP_HEIGHT_SCALE * (float)(hm->getHeight(x, vp1) - hm->getHeight(x, vn0));

	nx = l2rY * n2fZ - l2rZ * n2fY;
	ny = l2rZ * n2fX - l2rX * n2fZ;
	nz = l2rX * n2fY - l2rY * n2fX;
	const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
	if (len > 1e-8f)
	{
		nx /= len;
		ny /= len;
		nz /= len;
	}
	else
	{
		nx = 0.f;
		ny = 0.f;
		nz = 1.f;
	}
}

} // namespace

void applyActiveTimeOfDay()
{
	if (!TheWritableGlobalData)
		return;
	TimeOfDay tod = TheWritableGlobalData->m_timeOfDay;
	if (tod < TIME_OF_DAY_FIRST || tod >= TIME_OF_DAY_COUNT)
		tod = TIME_OF_DAY_AFTERNOON;
	TheWritableGlobalData->setTimeOfDay(tod);
}

void logActiveLights(const char *tag)
{
	if (!TheGlobalData)
	{
		fprintf(stderr, "%s: no GlobalData\n", tag ? tag : "Lighting");
		return;
	}
	const TimeOfDay tod = TheGlobalData->m_timeOfDay;
	fprintf(stderr,
		"%s: tod=%d numLights=%d amb0=(%.3f,%.3f,%.3f) dif0=(%.3f,%.3f,%.3f) pos0=(%.3f,%.3f,%.3f)\n",
		tag ? tag : "Lighting", (int)tod, (int)TheGlobalData->m_numGlobalLights,
		TheGlobalData->m_terrainAmbient[0].red, TheGlobalData->m_terrainAmbient[0].green,
		TheGlobalData->m_terrainAmbient[0].blue, TheGlobalData->m_terrainDiffuse[0].red,
		TheGlobalData->m_terrainDiffuse[0].green, TheGlobalData->m_terrainDiffuse[0].blue,
		TheGlobalData->m_terrainLightPos[0].x, TheGlobalData->m_terrainLightPos[0].y,
		TheGlobalData->m_terrainLightPos[0].z);
	if (tod >= TIME_OF_DAY_FIRST && tod < TIME_OF_DAY_COUNT)
	{
		const GlobalData::TerrainLighting &ol = TheGlobalData->m_terrainObjectsLighting[tod][0];
		fprintf(stderr, "%s: objectLight0 amb=(%.3f,%.3f,%.3f) dif=(%.3f,%.3f,%.3f) pos=(%.3f,%.3f,%.3f)\n",
			tag ? tag : "Lighting", ol.ambient.red, ol.ambient.green, ol.ambient.blue, ol.diffuse.red,
			ol.diffuse.green, ol.diffuse.blue, ol.lightPos.x, ol.lightPos.y, ol.lightPos.z);
	}
}

void bakeTerrainIndex(const WorldHeightMap *hm, int x, int y, float &outR, float &outG, float &outB)
{
	if (!hm)
	{
		outR = outG = outB = 1.f;
		return;
	}
	float nx, ny, nz;
	terrainNormalAt(hm, x, y, nx, ny, nz);
	doTheLight(nx, ny, nz, outR, outG, outB);
}

void bakeTerrainWorld(const WorldHeightMap *hm, float worldX, float worldY, float &outR, float &outG,
	float &outB)
{
	if (!hm)
	{
		outR = outG = outB = 1.f;
		return;
	}
	const Int border = hm->getBorderSize();
	Int ix = (Int)(worldX / MAP_XY_FACTOR + (float)border + 0.5f);
	Int iy = (Int)(worldY / MAP_XY_FACTOR + (float)border + 0.5f);
	bakeTerrainIndex(hm, ix, iy, outR, outG, outB);
}

void bakeObjectNormal(float nx, float ny, float nz, float &outR, float &outG, float &outB)
{
	outR = outG = outB = 1.f;
	if (!TheGlobalData)
		return;

	TimeOfDay tod = TheGlobalData->m_timeOfDay;
	if (tod < TIME_OF_DAY_FIRST || tod >= TIME_OF_DAY_COUNT)
		tod = TIME_OF_DAY_AFTERNOON;

	/* Scene ambient comes from object light 0 only (W3DDisplay::setTimeOfDay). */
	const GlobalData::TerrainLighting &primary = TheGlobalData->m_terrainObjectsLighting[tod][0];
	float shadeR = primary.ambient.red;
	float shadeG = primary.ambient.green;
	float shadeB = primary.ambient.blue;

	const float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
	if (nlen > 1e-8f)
	{
		nx /= nlen;
		ny /= nlen;
		nz /= nlen;
	}
	else
	{
		nx = 0.f;
		ny = 0.f;
		nz = 1.f;
	}

	for (Int i = 0; i < MAX_GLOBAL_LIGHTS; ++i)
	{
		const GlobalData::TerrainLighting &ol = TheGlobalData->m_terrainObjectsLighting[tod][i];
		const float lx = -ol.lightPos.x;
		const float ly = -ol.lightPos.y;
		const float lz = -ol.lightPos.z;
		float shade = lx * nx + ly * ny + lz * nz;
		if (shade > 1.f)
			shade = 1.f;
		if (shade < 0.f)
			shade = 0.f;
		shadeR += shade * ol.diffuse.red;
		shadeG += shade * ol.diffuse.green;
		shadeB += shade * ol.diffuse.blue;
	}

	clamp01(shadeR);
	clamp01(shadeG);
	clamp01(shadeB);
	outR = shadeR;
	outG = shadeG;
	outB = shadeB;
}

void bakeWaterShade(float &outR, float &outG, float &outB)
{
	outR = outG = outB = 1.f;
	if (!TheGlobalData)
		return;

	float shadeR = TheGlobalData->m_terrainAmbient[0].red;
	float shadeG = TheGlobalData->m_terrainAmbient[0].green;
	float shadeB = TheGlobalData->m_terrainAmbient[0].blue;
	for (Int lightIndex = 0; lightIndex < TheGlobalData->m_numGlobalLights; lightIndex++)
	{
		if (-TheGlobalData->m_terrainLightPos[lightIndex].z > 0)
		{
			shadeR += -TheGlobalData->m_terrainLightPos[lightIndex].z *
				TheGlobalData->m_terrainDiffuse[lightIndex].red;
			shadeG += -TheGlobalData->m_terrainLightPos[lightIndex].z *
				TheGlobalData->m_terrainDiffuse[lightIndex].green;
			shadeB += -TheGlobalData->m_terrainLightPos[lightIndex].z *
				TheGlobalData->m_terrainDiffuse[lightIndex].blue;
		}
	}
	clamp01(shadeR);
	clamp01(shadeG);
	clamp01(shadeB);
	outR = shadeR;
	outG = shadeG;
	outB = shadeB;
}

} // namespace TerrainLightingBake
