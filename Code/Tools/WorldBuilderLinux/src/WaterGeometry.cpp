#include "PreRTS.h"
#include "WaterGeometry.h"

#include "Common/GlobalData.h"
#include "Common/MapObject.h"
#include "GameClient/Water.h"
#include "GameLogic/PolygonTrigger.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{

constexpr float kWaterFactor = 150.f; /* drawTrapezoidWater UV scale */
constexpr float kBumpSize = 50.f;     /* river UV along width (HEIGHT_TO_USE path uses u) */

void waterShadeColors(float &outR, float &outG, float &outB, float &outA)
{
	float shadeR = 1.f, shadeG = 1.f, shadeB = 1.f;
	if (TheGlobalData)
	{
		shadeR = TheGlobalData->m_terrainAmbient[0].red;
		shadeG = TheGlobalData->m_terrainAmbient[0].green;
		shadeB = TheGlobalData->m_terrainAmbient[0].blue;
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
	}

	TimeOfDay tod = TIME_OF_DAY_AFTERNOON;
	if (TheGlobalData && TheGlobalData->m_timeOfDay >= 0
		&& TheGlobalData->m_timeOfDay < TIME_OF_DAY_COUNT)
		tod = TheGlobalData->m_timeOfDay;

	UnsignedInt waterDiffuse = 0xffb9b9b9;
	if (WaterSettings[tod].m_waterDiffuseColor.alpha ||
		WaterSettings[tod].m_waterDiffuseColor.red)
	{
		const RGBAColorInt &c = WaterSettings[tod].m_waterDiffuseColor;
		waterDiffuse = (c.alpha << 24) | (c.red << 16) | (c.green << 8) | c.blue;
	}

	const float waterShadeR = (waterDiffuse & 0xff) / 255.f;
	const float waterShadeG = ((waterDiffuse >> 8) & 0xff) / 255.f;
	const float waterShadeB = ((waterDiffuse >> 16) & 0xff) / 255.f;
	outA = ((waterDiffuse >> 24) & 0xff) / 255.f;

	outR = std::min(1.f, shadeR * waterShadeR);
	outG = std::min(1.f, shadeG * waterShadeG);
	outB = std::min(1.f, shadeB * waterShadeB);
}

void appendTrapezoid(const WorldHeightMap *hm, const float points[4][3], float cr, float cg, float cb,
	float ca, std::vector<WaterMeshVertex> &verts, std::vector<uint32_t> &indices)
{
	(void)hm;
	/* Mirror WaterRenderObjClass::drawTrapezoidWater bilinear patch. */
	float origin[3] = {points[0][0], points[0][1], points[0][2]};
	float uVec1[3] = {points[1][0] - origin[0], points[1][1] - origin[1], points[1][2] - origin[2]};
	float vVec1[3] = {points[3][0] - origin[0], points[3][1] - origin[1], points[3][2] - origin[2]};
	float uVec2[3] = {points[2][0] - points[3][0], points[2][1] - points[3][1], points[2][2] - points[3][2]};
	float vVec2[3] = {points[2][0] - points[1][0], points[2][1] - points[1][1], points[2][2] - points[1][2]};

	auto len3 = [](const float *v) {
		return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	};
	Int uCount = (Int)((len3(uVec1) + len3(uVec2)) / (8.f * MAP_XY_FACTOR));
	Int vCount = (Int)((len3(vVec1) + len3(vVec2)) / (8.f * MAP_XY_FACTOR));
	if (uCount < 1)
		uCount = 1;
	if (vCount < 1)
		vCount = 1;
	if (uCount > 50)
		uCount = 50;
	if (vCount > 50)
		vCount = 50;

	const Int rectangleCount = uCount * vCount;
	uCount++;
	vCount++;

	const uint32_t base = (uint32_t)verts.size();
	verts.reserve(verts.size() + (size_t)uCount * (size_t)vCount);

	const float ooWaterFactor = 1.f / kWaterFactor;
	const float constE = 1.f / (float)(vCount - 1);
	const float constF = 1.f / (float)(uCount - 1);

	for (Int j = 0; j < vCount; j++)
	{
		const float dv = (float)j * constE;
		for (Int i = 0; i < uCount; i++)
		{
			const float du = (float)i * constF;
			float x = origin[0] + uVec1[0] * du + vVec1[0] * dv + (dv) * (du) * (vVec2[0] - vVec1[0]);
			float y = origin[1] + uVec1[1] * du + vVec1[1] * dv + (dv) * (du) * (vVec2[1] - vVec1[1]);
			float z = origin[2] + uVec1[2] * du + vVec1[2] * dv + (dv) * (du) * (vVec2[2] - vVec1[2]);

			WaterMeshVertex v;
			v.px = x;
			v.py = y;
			v.pz = z;
			v.u = x * ooWaterFactor;
			v.v = y * ooWaterFactor;
			v.u2 = x / kBumpSize;
			v.v2 = (y + 0.3f * x) / kBumpSize;
			v.r = cr;
			v.g = cg;
			v.b = cb;
			/* Original lake opacity is uniform; shoreline softness comes from
			 * a separate terrain destination-alpha pass, not vertex depth. */
			v.a = ca;
			v.isRiver = 0.f;
			verts.push_back(v);
		}
	}

	indices.reserve(indices.size() + (size_t)rectangleCount * 6);
	for (Int j = 0; j < vCount - 1; j++)
	{
		for (Int i = 0; i < uCount - 1; i++)
		{
			const uint32_t i0 = base + (uint32_t)(j * uCount + i);
			const uint32_t i1 = base + (uint32_t)((j + 1) * uCount + i + 1);
			const uint32_t i2 = base + (uint32_t)((j + 1) * uCount + i);
			const uint32_t i3 = base + (uint32_t)(j * uCount + i + 1);
			/* Same winding as drawTrapezoidWater */
			indices.push_back(i0);
			indices.push_back(i1);
			indices.push_back(i2);
			indices.push_back(i0);
			indices.push_back(i3);
			indices.push_back(i1);
		}
	}
	(void)rectangleCount;
}

void appendLake(const WorldHeightMap *hm, PolygonTrigger *pTrig, float cr, float cg, float cb, float ca,
	std::vector<WaterMeshVertex> &verts, std::vector<uint32_t> &indices)
{
	if (!pTrig || pTrig->getNumPoints() <= 2)
		return;
	/* Same fan as WaterRenderObjClass::renderWater */
	for (Int k = 1; k < pTrig->getNumPoints() - 1; k = k + 2)
	{
		const ICoord3D pt3 = *pTrig->getPoint(0);
		const ICoord3D pt2 = *pTrig->getPoint(k);
		const ICoord3D pt1 = *pTrig->getPoint(k + 1);
		ICoord3D pt0 = *pTrig->getPoint(k + 1);
		if (k + 2 < pTrig->getNumPoints())
			pt0 = *pTrig->getPoint(k + 2);

		float points[4][3] = {
			{(float)pt0.x, (float)pt0.y, (float)pt0.z},
			{(float)pt1.x, (float)pt1.y, (float)pt1.z},
			{(float)pt2.x, (float)pt2.y, (float)pt2.z},
			{(float)pt3.x, (float)pt3.y, (float)pt3.z},
		};
		appendTrapezoid(hm, points, cr, cg, cb, ca, verts, indices);
	}
}

void appendRiver(const WorldHeightMap *hm, PolygonTrigger *pTrig, float cr, float cg, float cb, float ca,
	std::vector<WaterMeshVertex> &verts, std::vector<uint32_t> &indices)
{
	(void)hm;
	if (!pTrig || pTrig->getNumPoints() < 4)
		return;

	Int rectangleCount = pTrig->getNumPoints() / 2;
	rectangleCount--;
	if (rectangleCount < 1)
		return;

	Int innerNdx = pTrig->getRiverStart();
	Int outerNdx = innerNdx + 1;
	if (innerNdx < 0 || innerNdx >= pTrig->getNumPoints() - 1)
		return;

	Real endLen = 0;
	Real totalLen = 0;
	for (Int i = 0; i < pTrig->getNumPoints() - 1; i++)
	{
		const ICoord3D innerPt = *pTrig->getPoint(i);
		const ICoord3D outerPt = *pTrig->getPoint(i + 1);
		const Real dx = (Real)(innerPt.x - outerPt.x);
		const Real dy = (Real)(innerPt.y - outerPt.y);
		const Real curLen = std::sqrt(dx * dx + dy * dy);
		totalLen += curLen;
		if (i == innerNdx)
			endLen = curLen;
	}
	if (endLen < 1.f)
		endLen = 1.f;
	const Real lengthOfRiver = (totalLen / 2.f) - endLen;
	const Real repeatCount = lengthOfRiver / endLen;
	const Real vScale = (Real)repeatCount / (Real)rectangleCount;

	const uint32_t base = (uint32_t)verts.size();
	const Int pairCount = pTrig->getNumPoints() / 2;
	verts.reserve(verts.size() + (size_t)pairCount * 2);

	for (Int i = 0; i < pairCount; i++)
	{
		const ICoord3D innerPt = *pTrig->getPoint(outerNdx);
		const ICoord3D outerPt = *pTrig->getPoint(innerNdx);
		outerNdx++;
		innerNdx--;
		if (innerNdx < 0)
			innerNdx = pTrig->getNumPoints() - 1;
		if (outerNdx >= pTrig->getNumPoints())
			outerNdx = 0;

		const float wobbleConst = vScale * (Real)i;

		WaterMeshVertex a;
		a.px = (float)innerPt.x;
		a.py = (float)innerPt.y;
		a.pz = (float)innerPt.z;
		a.u = 0.5f; /* HEIGHT_TO_USE */
		a.v = wobbleConst;
		a.u2 = 1.f;
		a.v2 = wobbleConst;
		a.r = cr;
		a.g = cg;
		a.b = cb;
		/* Original river edges are feathered by TWAlphaEdge, not terrain depth. */
		a.a = ca;
		a.isRiver = 1.f;
		verts.push_back(a);

		WaterMeshVertex b;
		b.px = (float)outerPt.x;
		b.py = (float)outerPt.y;
		b.pz = (float)outerPt.z;
		b.u = 0.f;
		b.v = wobbleConst;
		b.u2 = 0.f;
		b.v2 = wobbleConst;
		b.r = cr;
		b.g = cg;
		b.b = cb;
		b.a = ca;
		b.isRiver = 1.f;
		verts.push_back(b);
	}

	indices.reserve(indices.size() + (size_t)rectangleCount * 6);
	for (Int i = 0; i < rectangleCount; i++)
	{
		indices.push_back(base + (uint32_t)(i * 2));
		indices.push_back(base + (uint32_t)(i * 2 + 1));
		indices.push_back(base + (uint32_t)(i * 2 + 3));
		indices.push_back(base + (uint32_t)(i * 2));
		indices.push_back(base + (uint32_t)(i * 2 + 3));
		indices.push_back(base + (uint32_t)(i * 2 + 2));
	}
	(void)kBumpSize;
}

} // namespace

WaterBakeStats WaterGeometry::bake(const WorldHeightMap *hm, std::vector<WaterMeshVertex> &outVerts,
	std::vector<uint32_t> &outIndices)
{
	outVerts.clear();
	outIndices.clear();
	WaterBakeStats stats;

	float cr, cg, cb, ca;
	waterShadeColors(cr, cg, cb, ca);

	for (PolygonTrigger *pTrig = PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext())
	{
		if (!pTrig->isWaterArea() || pTrig->getNumPoints() <= 2)
			continue;
		if (pTrig->isRiver())
		{
			appendRiver(hm, pTrig, cr, cg, cb, ca, outVerts, outIndices);
			++stats.riverAreas;
		}
		else
		{
			appendLake(hm, pTrig, cr, cg, cb, ca, outVerts, outIndices);
			++stats.lakeAreas;
		}
	}

	stats.verts = (int)outVerts.size();
	stats.tris = (int)(outIndices.size() / 3);
	return stats;
}
