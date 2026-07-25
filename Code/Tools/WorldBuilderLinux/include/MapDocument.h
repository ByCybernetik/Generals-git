#pragma once

#include "Common/AsciiString.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include <vector>

/** One straight road segment from paired FLAG_ROAD_POINT1/POINT2 MapObjects. */
struct MapRoadSegment
{
	AsciiString name;
	float x0 = 0.f, y0 = 0.f;
	float x1 = 0.f, y1 = 0.f;
	Int flags0 = 0; /* POINT1 flags (angled/tight/join) */
	Int flags1 = 0; /* POINT2 flags */
};

/** Non-road map object for 3D model display (mirrors MapObject placement). */
struct MapPlacedObject
{
	AsciiString name;
	float x = 0.f, y = 0.f, z = 0.f;
	float angle = 0.f;
	Int flags = 0;
};

class MapDocument
{
public:
	MapDocument();
	~MapDocument();

	bool load(const AsciiString &path);
	bool createBlank(Int playableX, Int playableY, UnsignedByte initialHeight, Int border);
	void clear();

	bool isLoaded() const { return m_heightMap != NULL; }
	const AsciiString &path() const { return m_path; }
	const AsciiString &lastError() const { return m_lastError; }

	WorldHeightMap *heightMap() { return m_heightMap; }
	const WorldHeightMap *heightMap() const { return m_heightMap; }

	Int width() const;
	Int height() const;
	Int borderSize() const;

	/** True if BlendTileData loaded real Art/Terrain tiles. */
	bool hasTerrainTextures() const;
	/** Pack source tiles into RGBA atlas (TEXTURE_WIDTH x pow2). */
	bool buildTerrainAtlas(std::vector<unsigned char> &rgba, int &outW, int &outH);
	/** Per-cell atlas UVs (4 corners: TL,TR,BR,BL) like HeightMapRenderObjClass. */
	bool cellUV(Int x, Int y, float U[4], float V[4]);
	/** Blend overlay UVs + vertex alphas (0..255) + triangle flip — getAlphaUVData. */
	bool cellBlendUV(Int x, Int y, float U[4], float V[4], UnsignedByte alpha[4], Bool *flip);

	const std::vector<MapRoadSegment> &roads() const { return m_roads; }
	const std::vector<MapPlacedObject> &objects() const { return m_objects; }

private:
	WorldHeightMap *m_heightMap;
	AsciiString m_path;
	AsciiString m_lastError;
	bool m_hasTiles;
	std::vector<MapRoadSegment> m_roads;
	std::vector<MapPlacedObject> m_objects;
};
