#include "PreRTS.h"
#include "ObjectMeshCache.h"

#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/ModelState.h"
#include "W3DDevice/GameClient/Module/W3DModelDraw.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"

#include "mesh.h"
#include "meshmdl.h"
#include "mapper.h"
#include "vertmaterial.h"
#include "matrix3d.h"
#include "vector3.h"
#include "vector2.h"
#include "texture.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <map>

namespace
{

AsciiString bestModelName(const ThingTemplate *tt)
{
	if (!tt)
		return AsciiString::TheEmptyString;
	const ModuleInfo &mi = tt->getDrawModuleInfo();
	if (mi.getCount() <= 0)
		return AsciiString::TheEmptyString;
	const ModuleData *mdd = mi.getNthData(0);
	const W3DModelDrawModuleData *md = mdd ? mdd->getAsW3DModelDrawModuleData() : NULL;
	if (!md)
		return AsciiString::TheEmptyString;
	ModelConditionFlags state;
	state.clear();
	return md->getBestModelNameForWB(state);
}

bool isEnvironmentMapper(const TextureMapperClass *mapper)
{
	if (!mapper)
		return false;
	switch (mapper->Mapper_ID())
	{
	case TextureMapperClass::MAPPER_ID_ENVIRONMENT:
	case TextureMapperClass::MAPPER_ID_CLASSIC_ENVIRONMENT:
	case TextureMapperClass::MAPPER_ID_WS_ENVIRONMENT:
	case TextureMapperClass::MAPPER_ID_WS_CLASSIC_ENVIRONMENT:
	case TextureMapperClass::MAPPER_ID_GRID_ENVIRONMENT:
	case TextureMapperClass::MAPPER_ID_GRID_CLASSIC_ENVIRONMENT:
	case TextureMapperClass::MAPPER_ID_BUMPENV:
		return true;
	default:
		return false;
	}
}

bool textureNameLooksLikeEnv(const char *name)
{
	if (!name || !name[0])
		return false;

	const char *base = name;
	for (const char *p = name; *p; ++p)
	{
		if (*p == '/' || *p == '\\')
			base = p + 1;
	}

	char stem[256];
	strncpy(stem, base, sizeof(stem) - 1);
	stem[sizeof(stem) - 1] = '\0';
	if (char *dot = strrchr(stem, '.'))
		*dot = '\0';
	for (char *p = stem; *p; ++p)
		*p = (char)tolower((unsigned char)*p);

	const size_t n = strlen(stem);
	if (n >= 2 && strcmp(stem + n - 2, "_e") == 0)
		return true;
	if (n >= 3 && (strcmp(stem + n - 3, "_es") == 0 || strcmp(stem + n - 3, "_en") == 0))
		return true;
	if (n >= 4 && strcmp(stem + n - 4, "_env") == 0)
		return true;
	if (strstr(stem, "cloud") || strstr(stem, "sky") || strstr(stem, "env"))
		return true;
	return false;
}

VertexMaterialClass *peekMaterial(MeshModelClass *model, int pass)
{
	if (VertexMaterialClass *vmat = model->Peek_Single_Material(pass))
		return vmat;
	if (model->Has_Material_Array(pass))
		return model->Peek_Material(0, pass);
	return NULL;
}

bool stageIsEnvironment(MeshModelClass *model, int pass, int stage, TextureClass *tex)
{
	if (tex && (tex->Is_Lightmap() || textureNameLooksLikeEnv(tex->Get_Texture_Name())))
		return true;
	if (VertexMaterialClass *vmat = peekMaterial(model, pass))
	{
		if (isEnvironmentMapper(vmat->Peek_Mapper(stage)))
			return true;
	}
	return false;
}

TextureClass *peekStageTexture(MeshModelClass *model, int pass, int stage, int polyHint)
{
	if (TextureClass *tex = model->Peek_Single_Texture(pass, stage))
		return tex;
	if (model->Has_Texture_Array(pass, stage))
		return model->Peek_Texture(polyHint, pass, stage);
	return NULL;
}

/**
 * Pick diffuse (mesh-UV) stage — skip environment/lightmap stages that caused
 * chrome/cloud look on buildings when stage 0 was an env map.
 */
bool findDiffuseStage(MeshModelClass *model, int &outPass, int &outStage)
{
	const int passCount = model->Get_Pass_Count() > 0 ? model->Get_Pass_Count() : 1;
	for (int pass = 0; pass < passCount && pass < MeshMatDescClass::MAX_PASSES; ++pass)
	{
		for (int stage = 0; stage < MeshMatDescClass::MAX_TEX_STAGES; ++stage)
		{
			TextureClass *tex = peekStageTexture(model, pass, stage, 0);
			if (!tex)
				continue;
			if (stageIsEnvironment(model, pass, stage, tex))
				continue;
			outPass = pass;
			outStage = stage;
			return true;
		}
	}
	return false;
}

void pushTransformedVert(ObjectBakedPart &part, const Vector3 &local, const Vector3 *normal,
	const Vector2 *uv, const Matrix3D &tm)
{
	Vector3 world;
	Matrix3D::Transform_Vector(tm, local, &world);
	ObjectMeshVertex v;
	v.px = world.X;
	v.py = world.Y;
	v.pz = world.Z;
	if (normal)
	{
		Vector3 nWorld;
		Matrix3D::Rotate_Vector(tm, *normal, &nWorld);
		if (nWorld.Length2() > 1e-12f)
			nWorld.Normalize();
		else
			nWorld.Set(0.f, 0.f, 1.f);
		v.nx = nWorld.X;
		v.ny = nWorld.Y;
		v.nz = nWorld.Z;
	}
	else
	{
		v.nx = 0.f;
		v.ny = 0.f;
		v.nz = 1.f;
	}
	if (uv)
	{
		v.u = uv->X;
		v.v = uv->Y;
	}
	else
	{
		v.u = 0.f;
		v.v = 0.f;
	}
	part.verts.push_back(v);
}

void appendMeshParts(ObjectBakedModel &out, MeshClass *mesh, const Matrix3D &tm)
{
	if (!mesh)
		return;
	MeshModelClass *model = mesh->Peek_Model();
	if (!model)
		return;
	if (model->Get_Flag(MeshGeometryClass::SKIN))
		return;

	const int vCount = model->Get_Vertex_Count();
	const int pCount = model->Get_Polygon_Count();
	if (vCount < 1 || pCount < 1)
		return;

	Vector3 *verts = model->Get_Vertex_Array();
	const Vector3i *polys = model->Get_Polygon_Array();
	if (!verts || !polys)
		return;
	const Vector3 *normals = model->Get_Vertex_Normal_Array();

	int pass = 0;
	int stage = 0;
	const bool haveDiffuse = findDiffuseStage(model, pass, stage);

	const Vector2 *uvs = haveDiffuse ? model->Get_UV_Array(pass, stage) : NULL;
	if (!uvs)
		uvs = model->Get_UV_Array_By_Index(0);

	if (haveDiffuse && model->Has_Texture_Array(pass, stage))
	{
		/* Per-polygon textures: split into parts (duplicate verts per tri). */
		std::map<std::string, ObjectBakedPart> keyed;
		for (int pi = 0; pi < pCount; ++pi)
		{
			TextureClass *tex = model->Peek_Texture(pi, pass, stage);
			if (tex && stageIsEnvironment(model, pass, stage, tex))
				continue;
			std::string key;
			if (tex && tex->Get_Texture_Name())
				key = tex->Get_Texture_Name();
			ObjectBakedPart &part = keyed[key];
			if (part.textureName.empty() && !key.empty())
				part.textureName = key;

			const int i0 = polys[pi].I;
			const int i1 = polys[pi].J;
			const int i2 = polys[pi].K;
			if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= vCount || i1 >= vCount || i2 >= vCount)
				continue;
			const uint32_t base = (uint32_t)part.verts.size();
			pushTransformedVert(part, verts[i0], normals ? &normals[i0] : NULL, uvs ? &uvs[i0] : NULL, tm);
			pushTransformedVert(part, verts[i1], normals ? &normals[i1] : NULL, uvs ? &uvs[i1] : NULL, tm);
			pushTransformedVert(part, verts[i2], normals ? &normals[i2] : NULL, uvs ? &uvs[i2] : NULL, tm);
			part.indices.push_back(base);
			part.indices.push_back(base + 1);
			part.indices.push_back(base + 2);
		}
		for (auto &kv : keyed)
		{
			if (!kv.second.indices.empty())
				out.parts.push_back(std::move(kv.second));
		}
		return;
	}

	ObjectBakedPart part;
	if (haveDiffuse)
	{
		if (TextureClass *tex = model->Peek_Single_Texture(pass, stage))
		{
			if (tex->Get_Texture_Name())
				part.textureName = tex->Get_Texture_Name();
		}
	}

	part.verts.resize((size_t)vCount);
	for (int i = 0; i < vCount; ++i)
	{
		Vector3 world;
		Matrix3D::Transform_Vector(tm, verts[i], &world);
		part.verts[i].px = world.X;
		part.verts[i].py = world.Y;
		part.verts[i].pz = world.Z;
		if (normals)
		{
			Vector3 nWorld;
			Matrix3D::Rotate_Vector(tm, normals[i], &nWorld);
			if (nWorld.Length2() > 1e-12f)
				nWorld.Normalize();
			else
				nWorld.Set(0.f, 0.f, 1.f);
			part.verts[i].nx = nWorld.X;
			part.verts[i].ny = nWorld.Y;
			part.verts[i].nz = nWorld.Z;
		}
		else
		{
			part.verts[i].nx = 0.f;
			part.verts[i].ny = 0.f;
			part.verts[i].nz = 1.f;
		}
		if (uvs)
		{
			part.verts[i].u = uvs[i].X;
			part.verts[i].v = uvs[i].Y;
		}
		else
		{
			part.verts[i].u = 0.f;
			part.verts[i].v = 0.f;
		}
	}
	part.indices.reserve((size_t)pCount * 3);
	for (int i = 0; i < pCount; ++i)
	{
		part.indices.push_back((uint32_t)polys[i].I);
		part.indices.push_back((uint32_t)polys[i].J);
		part.indices.push_back((uint32_t)polys[i].K);
	}
	out.parts.push_back(std::move(part));
}

void walkRenderObj(RenderObjClass *obj, const Matrix3D &parentTM, ObjectBakedModel &out)
{
	if (!obj)
		return;

	Matrix3D tm;
	Matrix3D::Multiply(parentTM, obj->Get_Transform(), &tm);

	if (obj->Class_ID() == RenderObjClass::CLASSID_MESH)
		appendMeshParts(out, (MeshClass *)obj, tm);

	const int n = obj->Get_Num_Sub_Objects();
	for (int i = 0; i < n; ++i)
	{
		RenderObjClass *sub = obj->Get_Sub_Object(i);
		if (!sub)
			continue;
		walkRenderObj(sub, tm, out);
		sub->Release_Ref();
	}
}

} // namespace

void ObjectMeshCache::clear()
{
	m_models.clear();
}

int ObjectMeshCache::bakeModel(const AsciiString &modelName, float scale)
{
	if (modelName.isEmpty() || strncmp(modelName.str(), "No ", 3) == 0)
		return -1;
	W3DAssetManager *mgr = (W3DAssetManager *)WW3DAssetManager::Get_Instance();
	if (!mgr)
	{
		fprintf(stderr, "ObjectMeshCache: no WW3DAssetManager\n");
		return -1;
	}

	RenderObjClass *robj = mgr->Create_Render_Obj(modelName.str(), scale, 0);
	if (!robj)
	{
		fprintf(stderr, "ObjectMeshCache: Create_Render_Obj failed for '%s'\n", modelName.str());
		return -1;
	}

	ObjectBakedModel baked;
	baked.modelName = modelName.str();
	Matrix3D identity(true);
	walkRenderObj(robj, identity, baked);
	robj->Release_Ref();

	if (baked.parts.empty())
	{
		fprintf(stderr, "ObjectMeshCache: no static mesh parts in '%s'\n", modelName.str());
		return -1;
	}

	m_models.push_back(std::move(baked));
	return (int)m_models.size() - 1;
}

int ObjectMeshCache::build(const std::vector<MapPlacedObject> &objects, std::vector<ObjectDrawInstance> &outInstances)
{
	clear();
	outInstances.clear();
	if (!TheThingFactory)
	{
		fprintf(stderr, "ObjectMeshCache: TheThingFactory missing\n");
		return 0;
	}

	std::map<std::string, int> modelKeyToIdx;
	int missingTemplate = 0, missingModel = 0;

	for (const MapPlacedObject &obj : objects)
	{
		if (obj.name.isEmpty())
			continue;
		const ThingTemplate *tt = TheThingFactory->findTemplate(obj.name);
		if (!tt)
		{
			++missingTemplate;
			continue;
		}
		AsciiString modelName = bestModelName(tt);
		const float scale = tt->getAssetScale();
		if (modelName.isEmpty() || strncmp(modelName.str(), "No ", 3) == 0)
		{
			++missingModel;
			continue;
		}

		char keyBuf[512];
		snprintf(keyBuf, sizeof(keyBuf), "%s|%.4f", modelName.str(), scale);
		std::string key(keyBuf);

		int modelIdx = -1;
		auto it = modelKeyToIdx.find(key);
		if (it == modelKeyToIdx.end())
		{
			modelIdx = bakeModel(modelName, scale);
			if (modelIdx < 0)
			{
				++missingModel;
				continue;
			}
			modelKeyToIdx[key] = modelIdx;
		}
		else
			modelIdx = it->second;

		ObjectDrawInstance inst;
		inst.modelIndex = modelIdx;
		inst.x = obj.x;
		inst.y = obj.y;
		inst.z = obj.z;
		inst.angle = obj.angle;
		outInstances.push_back(inst);
	}

	/* Resolved diffuse texture names for smoke debugging. */
	std::map<std::string, int> texUse;
	for (const ObjectBakedModel &m : m_models)
	{
		for (const ObjectBakedPart &p : m.parts)
		{
			const std::string &t = p.textureName.empty() ? std::string("(gray)") : p.textureName;
			texUse[t]++;
		}
	}
	int envHits = 0;
	for (const auto &kv : texUse)
	{
		if (textureNameLooksLikeEnv(kv.first.c_str()))
		{
			fprintf(stderr, "ObjectMeshCache: WARN still using env-like tex '%s' (x%d)\n",
				kv.first.c_str(), kv.second);
			++envHits;
		}
	}
	fprintf(stderr, "ObjectMeshCache: %zu unique textures (%d env-like remaining)\n",
		texUse.size(), envHits);
	int logged = 0;
	for (const ObjectBakedModel &m : m_models)
	{
		if (logged >= 12)
			break;
		fprintf(stderr, "ObjectMeshCache: model '%s' parts=%zu", m.modelName.c_str(), m.parts.size());
		for (size_t i = 0; i < m.parts.size() && i < 4; ++i)
		{
			const std::string &t = m.parts[i].textureName;
			fprintf(stderr, " tex%zu='%s'", i, t.empty() ? "(gray)" : t.c_str());
		}
		fprintf(stderr, "\n");
		++logged;
	}

	fprintf(stderr,
		"ObjectMeshCache: %zu instances, %zu unique models (skip template=%d model=%d) from %zu map objects\n",
		outInstances.size(), m_models.size(), missingTemplate, missingModel, objects.size());
	return (int)m_models.size();
}
