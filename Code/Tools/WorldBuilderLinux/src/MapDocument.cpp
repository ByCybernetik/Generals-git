#include "PreRTS.h"
#include "MapDocument.h"

#include "Common/DataChunk.h"
#include "Common/GlobalData.h"
#include "Common/MapObject.h"
#include "GameLogic/PolygonTrigger.h"
#include "Common/Dict.h"
#include "Common/FileSystem.h"
#include "Common/GlobalData.h"
#include "Common/MapObject.h"
#include "Common/MapReaderWriterInfo.h"
#include "Common/WellKnownKeys.h"
#include "GameLogic/SidesList.h"
#include "W3DDevice/GameClient/TileData.h"
#include "W3DDevice/GameClient/TerrainTex.h"

#include <string.h>
#include <vector>
#include <stdio.h>

/* Subclass: protected Parse* + atlas helpers for Linux viewport. */
class HeightOnlyMap : public WorldHeightMap
{
public:
	bool m_gotHeight = false;
	bool m_gotBlend = false;

	HeightOnlyMap() {}

	void ensureCellFlags()
	{
		if (!m_data || m_width <= 0 || m_height <= 0)
			return;
		const Int numBytesX = (m_width + 1) / 8;
		m_flipStateWidth = numBytesX;
		if (!m_cellFlipState)
		{
			m_cellFlipState = new UnsignedByte[numBytesX * m_height];
			memset(m_cellFlipState, 0, (size_t)(numBytesX * m_height));
		}
		if (!m_cellCliffState)
		{
			m_cellCliffState = new UnsignedByte[numBytesX * m_height];
			memset(m_cellCliffState, 0, (size_t)(numBytesX * m_height));
		}
		m_drawOriginX = 0;
		m_drawOriginY = 0;
		m_drawWidthX = m_width;
		m_drawHeightY = m_height;
	}

	static Bool ParseHeightChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
	{
		HeightOnlyMap *self = (HeightOnlyMap *)userData;
		if (!self->ParseHeightMapData(file, info, userData))
			return false;
		self->ensureCellFlags();
		self->m_gotHeight = true;
		return true;
	}

	static Bool ParseBlendChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
	{
		HeightOnlyMap *self = (HeightOnlyMap *)userData;
		if (!self->ParseBlendTileData(file, info, userData))
			return false;
		self->m_gotBlend = (self->m_tileNdxes != NULL && self->m_numBitmapTiles > 0);
		return true;
	}

	bool prepareTileLayout()
	{
		if (!m_tileNdxes || m_numBitmapTiles <= 0)
			return false;
		Int edgeHeight = 0;
		Int height = updateTileTexturePositions(&edgeHeight);
		if (height <= 0)
			height = TILE_PIXEL_EXTENT + TILE_OFFSET;
		Int pow2 = 1;
		while (pow2 < height)
			pow2 *= 2;
		m_terrainTexHeight = pow2;
		return true;
	}

	bool buildAtlasRGBA(std::vector<unsigned char> &rgba, int &outW, int &outH)
	{
		if (!prepareTileLayout())
			return false;
		outW = TEXTURE_WIDTH;
		outH = m_terrainTexHeight;
		rgba.assign((size_t)outW * (size_t)outH * 4, 0);

		const Int tilePixelExtent = TILE_PIXEL_EXTENT;
		for (Int tileNdx = 0; tileNdx < m_numBitmapTiles; ++tileNdx)
		{
			TileData *pTile = getSourceTile(tileNdx);
			if (!pTile)
				continue;
			const ICoord2D position = pTile->m_tileLocationInTexture;
			if (position.x <= 0)
				continue;

			for (Int j = 0; j < tilePixelExtent; ++j)
			{
				UnsignedByte *pBGR = pTile->getRGBDataForWidth(tilePixelExtent);
				pBGR += (tilePixelExtent - 1 - j) * TILE_BYTES_PER_PIXEL * tilePixelExtent;
				const Int row = position.y + j;
				if (row < 0 || row >= outH)
					continue;
				for (Int i = 0; i < tilePixelExtent; ++i)
				{
					const Int col = position.x + i;
					if (col < 0 || col >= outW)
						continue;
					const size_t dst = ((size_t)row * (size_t)outW + (size_t)col) * 4;
					/* TileData is BGRA; upload as RGBA. */
					rgba[dst + 0] = pBGR[2];
					rgba[dst + 1] = pBGR[1];
					rgba[dst + 2] = pBGR[0];
					rgba[dst + 3] = 255;
					pBGR += TILE_BYTES_PER_PIXEL;
				}
			}
		}
		return true;
	}

	bool initBlank(Int playableX, Int playableY, UnsignedByte initialHeight, Int border)
	{
		if (playableX < 1 || playableY < 1 || border < 0)
			return false;

		const Int width = playableX + 2 * border;
		const Int height = playableY + 2 * border;
		const Int dataSize = width * height;
		if (dataSize <= 0)
			return false;

		m_alphaEdgeTex = NULL;
		m_numEdgeTiles = 0;
		m_numEdgeTextureClasses = 0;
		m_alphaEdgeHeight = 1;

		m_width = width;
		m_height = height;
		m_borderSize = border;
		m_dataSize = dataSize;
		m_data = new UnsignedByte[m_dataSize];
		memset(m_data, initialHeight, (size_t)m_dataSize);

		m_boundaries.clear();
		ICoord2D initialBorder;
		initialBorder.x = playableX;
		initialBorder.y = playableY;
		m_boundaries.push_back(initialBorder);

		ensureCellFlags();
		m_gotHeight = true;
		m_gotBlend = false;
		return true;
	}
};

/* Collect roads + placeable objects from ObjectsList (no MapObject list). */
struct ObjectsParseState
{
	std::vector<MapRoadSegment> *roads = nullptr;
	std::vector<MapPlacedObject> *objects = nullptr;
	bool haveP1 = false;
	float p1x = 0.f, p1y = 0.f;
	AsciiString p1Name;
	Int p1Flags = 0;
};

static Bool ParseMapObjectChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	ObjectsParseState *st = (ObjectsParseState *)userData;
	Coord3D loc;
	loc.x = file.readReal();
	loc.y = file.readReal();
	loc.z = file.readReal();
	if (info->version <= K_OBJECTS_VERSION_2)
		loc.z = 0;
	Real angle = file.readReal();
	Int flags = file.readInt();
	AsciiString name = file.readAsciiString();
	Dict d;
	if (info->version >= K_OBJECTS_VERSION_2)
		d = file.readDict();

	if (flags & FLAG_ROAD_POINT1)
	{
		st->haveP1 = true;
		st->p1x = loc.x;
		st->p1y = loc.y;
		st->p1Name = name;
		st->p1Flags = flags;
		return true;
	}
	if ((flags & FLAG_ROAD_POINT2) && st->haveP1)
	{
		MapRoadSegment seg;
		seg.name = st->p1Name;
		seg.x0 = st->p1x;
		seg.y0 = st->p1y;
		seg.x1 = loc.x;
		seg.y1 = loc.y;
		seg.flags0 = st->p1Flags;
		seg.flags1 = flags;
		if (seg.x0 == seg.x1 && seg.y0 == seg.y1)
			seg.x1 += 0.25f;
		st->roads->push_back(seg);
		st->haveP1 = false;
		return true;
	}
	if (flags & FLAG_ROAD_FLAGS)
		return true;

	st->haveP1 = false;

	/* Skip bridges, lights, scorches, waypoints, DONT_RENDER — same as WB model path. */
	if (flags & (FLAG_BRIDGE_FLAGS | FLAG_DONT_RENDER))
		return true;
	if (d.getType(TheKey_lightHeightAboveTerrain) == Dict::DICT_REAL)
		return true;
	if (d.getType(TheKey_scorchType) == Dict::DICT_INT)
		return true;
	if (d.getType(TheKey_waypointID) == Dict::DICT_INT)
		return true;

	MapPlacedObject obj;
	obj.name = name;
	obj.x = loc.x;
	obj.y = loc.y;
	obj.z = loc.z;
	obj.angle = angle;
	obj.flags = flags;
	st->objects->push_back(obj);
	return true;
}

static Bool ParseObjectsListChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	ObjectsParseState *st = (ObjectsParseState *)userData;
	file.registerParser(AsciiString("Object"), info->label, ParseMapObjectChunk);
	return file.parse(st);
}

struct MapLoadUserData
{
	HeightOnlyMap *hm;
	ObjectsParseState *objects;
};

static Bool ParseHeightForLoad(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	return HeightOnlyMap::ParseHeightChunk(file, info, ((MapLoadUserData *)userData)->hm);
}

static Bool ParseBlendForLoad(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	return HeightOnlyMap::ParseBlendChunk(file, info, ((MapLoadUserData *)userData)->hm);
}

static Bool ParseObjectsForLoad(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	return ParseObjectsListChunk(file, info, ((MapLoadUserData *)userData)->objects);
}

MapDocument::MapDocument()
	: m_heightMap(NULL), m_hasTiles(false)
{
}

MapDocument::~MapDocument()
{
	clear();
}

void MapDocument::clear()
{
	if (m_heightMap)
	{
		REF_PTR_RELEASE(m_heightMap);
		m_heightMap = NULL;
	}
	m_path.clear();
	m_lastError.clear();
	m_hasTiles = false;
	m_roads.clear();
	m_objects.clear();
	PolygonTrigger::deleteTriggers();
}

static void addDefaultWaterArea(Int playableX, Int playableY, Int border)
{
	PolygonTrigger::deleteTriggers();
	PolygonTrigger *pTrig = newInstance(PolygonTrigger)(4);
	ICoord3D loc;
	pTrig->setWaterArea(true);
	const Real waterZ = TheGlobalData ? TheGlobalData->m_waterPositionZ : 0.f;
	loc.x = (Int)(-border * MAP_XY_FACTOR);
	loc.y = (Int)(-border * MAP_XY_FACTOR);
	loc.z = (Int)waterZ;
	pTrig->addPoint(loc);
	loc.x = (Int)((playableX + border) * MAP_XY_FACTOR);
	pTrig->addPoint(loc);
	loc.y = (Int)((playableY + border) * MAP_XY_FACTOR);
	pTrig->addPoint(loc);
	loc.x = (Int)(-border * MAP_XY_FACTOR);
	pTrig->addPoint(loc);
	PolygonTrigger::addPolygonTrigger(pTrig);
}

static Int countWaterAreas()
{
	Int n = 0;
	for (PolygonTrigger *p = PolygonTrigger::getFirstPolygonTrigger(); p; p = p->getNext())
	{
		if (p->isWaterArea())
			++n;
	}
	return n;
}

Int MapDocument::width() const
{
	return m_heightMap ? m_heightMap->getXExtent() : 0;
}

Int MapDocument::height() const
{
	return m_heightMap ? m_heightMap->getYExtent() : 0;
}

Int MapDocument::borderSize() const
{
	return m_heightMap ? m_heightMap->getBorderSize() : 0;
}

bool MapDocument::hasTerrainTextures() const
{
	return m_hasTiles;
}

bool MapDocument::buildTerrainAtlas(std::vector<unsigned char> &rgba, int &outW, int &outH)
{
	if (!m_heightMap || !m_hasTiles)
		return false;
	return ((HeightOnlyMap *)m_heightMap)->buildAtlasRGBA(rgba, outW, outH);
}

bool MapDocument::cellUV(Int x, Int y, float U[4], float V[4])
{
	if (!m_heightMap || !m_hasTiles)
		return false;
	/* getUVData always fills UVs when tiles exist; return value may be false when cliff adjust is off. */
	m_heightMap->getUVData(x, y, U, V, false);
	return (U[0] != 0.f || U[1] != 0.f || V[0] != 0.f || V[1] != 0.f);
}

bool MapDocument::cellBlendUV(Int x, Int y, float U[4], float V[4], UnsignedByte alpha[4], Bool *flip)
{
	if (!m_heightMap || !m_hasTiles || !U || !V || !alpha)
		return false;
	Bool localFlip = false;
	m_heightMap->getAlphaUVData(x, y, U, V, alpha, &localFlip, false);
	if (flip)
		*flip = localFlip;
	return true;
}

bool MapDocument::createBlank(Int playableX, Int playableY, UnsignedByte initialHeight, Int border)
{
	clear();
	if (TheSidesList)
	{
		TheSidesList->clear();
		TheSidesList->validateSides();
	}

	HeightOnlyMap *hm = new HeightOnlyMap();
	if (!hm->initBlank(playableX, playableY, initialHeight, border))
	{
		REF_PTR_RELEASE(hm);
		m_lastError = "Invalid blank map size";
		return false;
	}
	m_heightMap = hm;
	m_path = "(new map)";
	m_lastError.clear();
	m_hasTiles = false;
	m_roads.clear();
	m_objects.clear();
	addDefaultWaterArea(playableX, playableY, border);
	fprintf(stderr, "MapDocument: default water area created\n");
	return true;
}

bool MapDocument::load(const AsciiString &path)
{
	clear();

	if (path.isEmpty())
	{
		m_lastError = "Empty path";
		return false;
	}

	CachedFileInputStream stream;
	if (!stream.open(path))
	{
		m_lastError.format("Cannot open: %s", path.str());
		return false;
	}

	try
	{
		HeightOnlyMap *hm = new HeightOnlyMap();
		ChunkInputStream *pStrm = &stream;
		DataChunkInput file(pStrm);
		if (!file.isValidFileType())
		{
			REF_PTR_RELEASE(hm);
			m_lastError.format("Not a Generals map (bad header): %s", path.str());
			return false;
		}
		ObjectsParseState objState;
		objState.roads = &m_roads;
		objState.objects = &m_objects;
		MapLoadUserData loadUd = {hm, &objState};
		PolygonTrigger::deleteTriggers();
		file.registerParser(AsciiString("HeightMapData"), AsciiString::TheEmptyString, ParseHeightForLoad);
		file.registerParser(AsciiString("BlendTileData"), AsciiString::TheEmptyString, ParseBlendForLoad);
		file.registerParser(AsciiString("ObjectsList"), AsciiString::TheEmptyString, ParseObjectsForLoad);
		file.registerParser(AsciiString("PolygonTriggers"), AsciiString::TheEmptyString,
			PolygonTrigger::ParsePolygonTriggersDataChunk);
		if (!file.parse(&loadUd) || !hm->m_gotHeight || !hm->getDataPtr() || hm->getXExtent() <= 0)
		{
			REF_PTR_RELEASE(hm);
			m_roads.clear();
			m_objects.clear();
			PolygonTrigger::deleteTriggers();
			m_lastError.format("No HeightMapData in: %s", path.str());
			return false;
		}
		if (TheSidesList)
		{
			TheSidesList->clear();
			TheSidesList->validateSides();
		}
		m_hasTiles = hm->m_gotBlend && hm->prepareTileLayout();
		if (hm->m_gotBlend && !m_hasTiles)
			fprintf(stderr, "MapDocument: BlendTileData present but tile layout/atlas failed\n");
		else if (m_hasTiles)
			fprintf(stderr, "MapDocument: terrain tiles OK (classes loaded from Art/Terrain)\n");
		if (!m_roads.empty())
			fprintf(stderr, "MapDocument: road segments: %zu\n", m_roads.size());
		if (!m_objects.empty())
			fprintf(stderr, "MapDocument: placed objects: %zu\n", m_objects.size());
		{
			const Int waterN = countWaterAreas();
			fprintf(stderr, "MapDocument: water areas: %d\n", waterN);
		}
		m_heightMap = hm;
		m_path = path;
		m_lastError.clear();
		return true;
	}
	catch (...)
	{
		m_heightMap = NULL;
		m_hasTiles = false;
		m_roads.clear();
		m_objects.clear();
		PolygonTrigger::deleteTriggers();
		m_lastError.format("Corrupt or unsupported map: %s", path.str());
		return false;
	}
}
