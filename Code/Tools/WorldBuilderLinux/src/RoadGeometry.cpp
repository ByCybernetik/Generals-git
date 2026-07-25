#include "PreRTS.h"
#include "RoadGeometry.h"

#include "Common/MapObject.h"
#include "GameClient/TerrainRoads.h"
#include "W3DDevice/GameClient/W3DRoadBuffer.h"

#include "vector2.h"

#include <stdio.h>
#include <string>
#include <vector>

namespace
{

/** Same constants as W3DRoadBuffer.cpp (file-local there). */
const Real kCornerRadius = 1.5f;
const Real kTightCornerRadius = 0.5f;

/**
 * Runs the real W3DRoadBuffer topology pipeline (addMapObject chain → tees/Y/H →
 * curves → alpha joins) without DX preload. Geometry tessellation stays in MapViewport.
 */
class WbRoadTopology : public W3DRoadBuffer
{
public:
	void buildFromSegments(const std::vector<MapRoadSegment> &segs)
	{
		clearAllRoads();
		if (!m_initialized || !m_roads)
			return;

		for (const MapRoadSegment &s : segs)
		{
			if (m_numRoads >= m_maxRoadSegments)
				break;

			RoadSegment curRoad;
			curRoad.m_scale = DEFAULT_ROAD_SCALE;
			curRoad.m_widthInTexture = 1.0f;
			curRoad.m_uniqueID = 1;
			Bool found = false;
			AsciiString name = s.name.isEmpty() ? AsciiString("TwoLane") : s.name;
			if (TheTerrainRoads)
			{
				TerrainRoadType *road = TheTerrainRoads->findRoad(name);
				if (road)
				{
					curRoad.m_widthInTexture = road->getRoadWidthInTexture();
					curRoad.m_scale = road->getRoadWidth();
					curRoad.m_uniqueID = (Int)road->getID();
					found = TRUE;
				}
			}
			(void)found;

			Vector2 loc1(s.x0, s.y0);
			Vector2 loc2(s.x1, s.y1);
			if (loc1.X == loc2.X && loc1.Y == loc2.Y)
				loc2.X += 0.25f;

			curRoad.m_pt1.loc = loc1;
			curRoad.m_pt1.isAngled = (s.flags0 & FLAG_ROAD_CORNER_ANGLED) != 0;
			curRoad.m_pt1.isJoin = (s.flags0 & FLAG_ROAD_JOIN) != 0;
			curRoad.m_pt1.count = 0;
			curRoad.m_pt1.last = true;
			curRoad.m_pt1.multi = false;
			curRoad.m_pt2.loc = loc2;
			curRoad.m_pt2.isAngled = (s.flags1 & FLAG_ROAD_CORNER_ANGLED) != 0;
			curRoad.m_pt2.isJoin = (s.flags1 & FLAG_ROAD_JOIN) != 0;
			curRoad.m_pt2.count = 0;
			curRoad.m_pt2.last = true;
			curRoad.m_pt2.multi = false;
			curRoad.m_type = SEGMENT;
			curRoad.m_curveRadius = (s.flags0 & FLAG_ROAD_CORNER_TIGHT) ? kTightCornerRadius : kCornerRadius;

			addMapObject(&curRoad, true);
		}

		/* Second pass: re-insert without recounting (W3DRoadBuffer::addMapObjects). */
		const Int curCount = m_numRoads;
		std::vector<RoadSegment> copy;
		copy.reserve((size_t)curCount);
		for (Int i = 0; i < curCount; ++i)
			copy.push_back(m_roads[i]);
		m_numRoads = 0;
		for (RoadSegment &r : copy)
			addMapObject(&r, false);

		updateCountsAndFlags();
		insertTeeIntersections();
		insertCurveSegments();
		insertCrossTypeJoins();
	}

	Int count() const { return m_numRoads; }
	const RoadSegment &at(Int i) const { return m_roads[i]; }
};

static AsciiString nameForUniqueId(Int uid)
{
	if (TheTerrainRoads)
	{
		for (TerrainRoadType *r = TheTerrainRoads->firstRoad(); r; r = TheTerrainRoads->nextRoad(r))
		{
			if ((Int)r->getID() == uid)
				return r->getName();
		}
	}
	return AsciiString("TwoLane");
}

static RoadDrawPiece::Kind kindFromCorner(TCorner t)
{
	switch (t)
	{
	case CURVE:
		return RoadDrawPiece::Curve;
	case TEE:
		return RoadDrawPiece::Tee;
	case FOUR_WAY:
		return RoadDrawPiece::FourWay;
	case THREE_WAY_Y:
		return RoadDrawPiece::YJoin;
	case THREE_WAY_H:
		return RoadDrawPiece::HJoin;
	case THREE_WAY_H_FLIP:
		return RoadDrawPiece::HJoinFlip;
	case ALPHA_JOIN:
		return RoadDrawPiece::AlphaJoin;
	default:
		return RoadDrawPiece::Straight;
	}
}

} // namespace

void buildRoadDrawPieces(const std::vector<MapRoadSegment> &segs, std::vector<RoadDrawPiece> &out)
{
	out.clear();
	if (segs.empty())
		return;
	if (!TheGlobalData)
	{
		fprintf(stderr, "RoadGeometry: TheGlobalData missing — cannot run W3DRoadBuffer\n");
		return;
	}

	WbRoadTopology topo;
	topo.buildFromSegments(segs);

	int nCurve = 0, nTee = 0, n4 = 0, nY = 0, nH = 0, nAlpha = 0;
	out.reserve((size_t)topo.count());

	for (Int i = 0; i < topo.count(); ++i)
	{
		const RoadSegment &r = topo.at(i);
		RoadDrawPiece p;
		p.typeName = nameForUniqueId(r.m_uniqueID).str();
		p.scale = r.m_scale;
		p.widthInTex = r.m_widthInTexture;
		p.curveRadius = r.m_curveRadius;
		p.x0 = r.m_pt1.loc.X;
		p.y0 = r.m_pt1.loc.Y;
		p.x1 = r.m_pt2.loc.X;
		p.y1 = r.m_pt2.loc.Y;
		p.t0x = r.m_pt1.top.X;
		p.t0y = r.m_pt1.top.Y;
		p.b0x = r.m_pt1.bottom.X;
		p.b0y = r.m_pt1.bottom.Y;
		p.t1x = r.m_pt2.top.X;
		p.t1y = r.m_pt2.top.Y;
		p.b1x = r.m_pt2.bottom.X;
		p.b1y = r.m_pt2.bottom.Y;
		p.kind = kindFromCorner(r.m_type);
		switch (p.kind)
		{
		case RoadDrawPiece::Curve:
			++nCurve;
			break;
		case RoadDrawPiece::Tee:
			++nTee;
			break;
		case RoadDrawPiece::FourWay:
			++n4;
			break;
		case RoadDrawPiece::YJoin:
			++nY;
			break;
		case RoadDrawPiece::HJoin:
		case RoadDrawPiece::HJoinFlip:
			++nH;
			break;
		case RoadDrawPiece::AlphaJoin:
			++nAlpha;
			break;
		default:
			break;
		}
		out.push_back(p);
	}

	fprintf(stderr,
		"RoadGeometry: %zu pieces via W3DRoadBuffer (curves=%d tees=%d Y=%d H=%d 4way=%d alpha=%d) from %zu map segments\n",
		out.size(), nCurve, nTee, nY, nH, n4, nAlpha, segs.size());
}
