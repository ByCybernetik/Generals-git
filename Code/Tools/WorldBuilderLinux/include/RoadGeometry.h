#pragma once

#include "MapDocument.h"
#include <string>
#include <vector>

/** Expanded road pieces (mirrors W3DRoadBuffer SEGMENT/CURVE/TEE/…). */
struct RoadDrawPiece
{
	enum Kind
	{
		Straight = 0,
		Curve,
		Tee,
		FourWay,
		YJoin,
		HJoin,
		HJoinFlip,
		AlphaJoin
	};
	Kind kind = Straight;
	std::string typeName;
	float scale = 35.f;
	float widthInTex = 0.9f;
	float curveRadius = 1.5f;

	float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
	float t0x = 0.f, t0y = 0.f, b0x = 0.f, b0y = 0.f;
	float t1x = 0.f, t1y = 0.f, b1x = 0.f, b1y = 0.f;
};

/** Build road pieces via real W3DRoadBuffer topology (tees/Y/H/curves/alpha). */
void buildRoadDrawPieces(const std::vector<MapRoadSegment> &segs, std::vector<RoadDrawPiece> &out);
