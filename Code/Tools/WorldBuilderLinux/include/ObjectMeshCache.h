#pragma once

#include "Common/AsciiString.h"
#include "MapDocument.h"

#include <string>
#include <vector>

struct ObjectMeshVertex
{
	float px, py, pz;
	float nx, ny, nz; /* model-space normal after asset transform (unit length) */
	float u, v;
};

/** One textured triangle mesh in model space (asset scale already applied). */
struct ObjectBakedPart
{
	std::string textureName; /* empty = solid gray */
	std::vector<ObjectMeshVertex> verts;
	std::vector<uint32_t> indices;
};

struct ObjectBakedModel
{
	std::string modelName;
	std::vector<ObjectBakedPart> parts;
};

/** Placed instance referencing a baked model. */
struct ObjectDrawInstance
{
	int modelIndex = -1;
	float x = 0.f, y = 0.f, z = 0.f;
	float angle = 0.f;
};

/**
 * Resolve ThingTemplate → W3D model and bake static mesh geometry for MapViewport.
 * Mirrors WbView3d::getModelNameAndScale + Create_Render_Obj (no scene).
 */
class ObjectMeshCache
{
public:
	void clear();
	/**
	 * Bake unique models and build draw instances (z = map z only; caller adds terrain height).
	 * Returns number of unique baked models.
	 */
	int build(const std::vector<MapPlacedObject> &objects, std::vector<ObjectDrawInstance> &outInstances);

	const std::vector<ObjectBakedModel> &models() const { return m_models; }

private:
	int bakeModel(const AsciiString &modelName, float scale);

	std::vector<ObjectBakedModel> m_models;
};
