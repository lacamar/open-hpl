// Retained C++ half of the assimp loader: imports with the engine's flags, walks the aiScene into
// the Rust sink, and exposes the aiMatrix4x4 arithmetic the loader needs (compiled here so floats
// match the original loader exactly).
#include <cstdio>
#include <cstring>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "shims.h"

static_assert(sizeof(aiVertexWeight) == sizeof(OhplAiWeight), "aiVertexWeight layout");
static_assert(sizeof(aiMatrix4x4) == 64 && sizeof(aiVector3D) == 12, "assimp float layout");
static_assert(sizeof(OhplAiBone) == 96 && sizeof(OhplAiMesh) == 80, "Rust mirror in meshio/assimp.rs");

static aiMatrix4x4 Mat(const float* m) { aiMatrix4x4 r; memcpy(&r, m, 64); return r; }
static void Put(float* out, const aiMatrix4x4& m) { memcpy(out, &m, 64); }

static aiMatrix4x4 Rigid(const aiMatrix4x4& a_mtx)
{
	aiVector3D vScale, vPos;
	aiQuaternion qRot;
	a_mtx.Decompose(vScale, qRot, vPos);
	return aiMatrix4x4(aiVector3D(1,1,1), qRot, vPos);
}

static float UnitScale(const aiScene* apScene)
{
	float fUnit;
	double fUnitD;
	if(apScene && apScene->mMetaData)
	{
		if(apScene->mMetaData->Get("UnitScaleFactor", fUnit)) return fUnit / 100.0f;
		if(apScene->mMetaData->Get("UnitScaleFactor", fUnitD)) return (float)fUnitD / 100.0f;
	}
	return 1;
}

static void WalkNode(const aiNode* apNode, int32_t alParent, const OhplAiSink* apSink, int32_t* alCount)
{
	int32_t lSelf = (*alCount)++;
	const char* sName = apNode->mName.C_Str();
	float m[16];
	Put(m, apNode->mTransformation);
	apSink->node(apSink->ctx, alParent, sName, strlen(sName), m, apNode->mMeshes, apNode->mNumMeshes);
	for(unsigned int c=0; c<apNode->mNumChildren; ++c) WalkNode(apNode->mChildren[c], lSelf, apSink, alCount);
}

static void WalkMeshes(const aiScene* apScene, const OhplAiSink* apSink)
{
	for(unsigned int i=0; i<apScene->mNumMeshes; ++i)
	{
		const aiMesh* pSrc = apScene->mMeshes[i];
		std::vector<uint32_t> vIdx;
		vIdx.reserve((size_t)pSrc->mNumFaces * 3);
		for(unsigned int f=0; f<pSrc->mNumFaces; ++f)
			for(int k=0; k<3; ++k) vIdx.push_back(pSrc->mFaces[f].mNumIndices > (unsigned)k ? pSrc->mFaces[f].mIndices[k] : 0);

		std::vector<OhplAiBone> vBones(pSrc->mNumBones);
		for(unsigned int b=0; b<pSrc->mNumBones; ++b)
		{
			const aiBone* pBone = pSrc->mBones[b];
			vBones[b].name = pBone->mName.C_Str();
			vBones[b].name_len = strlen(vBones[b].name);
			Put(vBones[b].offset, pBone->mOffsetMatrix);
			vBones[b].weight_num = pBone->mNumWeights;
			vBones[b].weights = (const OhplAiWeight*)pBone->mWeights;
		}

		OhplAiMesh m = {};
		m.primitive_types = pSrc->mPrimitiveTypes;
		m.vertex_num = pSrc->mNumVertices;
		m.vertices = (const float*)pSrc->mVertices;
		m.normals = (const float*)pSrc->mNormals;
		m.uv0 = (const float*)pSrc->mTextureCoords[0];
		m.tangents = (const float*)pSrc->mTangents;
		m.bitangents = (const float*)pSrc->mBitangents;
		m.face_num = pSrc->mNumFaces;
		m.indices = vIdx.data();
		m.material = pSrc->mMaterialIndex;
		m.bone_num = pSrc->mNumBones;
		m.bones = vBones.data();
		apSink->mesh(apSink->ctx, &m);
	}
}

static void WalkMaterials(const aiScene* apScene, const OhplAiSink* apSink)
{
	for(unsigned int i=0; i<apScene->mNumMaterials; ++i)
	{
		const aiMaterial* pMat = apScene->mMaterials[i];
		aiString sTexture, sName;
		const char* pTex = NULL;
		const char* pName = NULL;
		if(pMat->GetTexture(aiTextureType_DIFFUSE, 0, &sTexture) == AI_SUCCESS && sTexture.length > 0) pTex = sTexture.C_Str();
		if(pMat->Get(AI_MATKEY_NAME, sName) == AI_SUCCESS && sName.length > 0) pName = sName.C_Str();
		apSink->material(apSink->ctx, pTex, pTex ? strlen(pTex) : 0, pName, pName ? strlen(pName) : 0);
	}
}

extern "C" {

// mode 0: the engine's mesh import; mode 1: bare import (scene unit scale and node names only).
// Returns 0, or 1 with the importer's error text in err.
int ohpl_ai_read(const char* path, int mode, const OhplAiSink* sink, char* err, size_t err_len)
{
	Assimp::Importer importer;
	importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
	const aiScene* pScene = importer.ReadFile(path, mode == 0 ?
			(aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals |
			aiProcess_CalcTangentSpace | aiProcess_FlipUVs | aiProcess_SortByPType | aiProcess_GlobalScale) : 0);
	if(pScene == NULL || pScene->mRootNode == NULL)
	{
		snprintf(err, err_len, "%s", importer.GetErrorString());
		return 1;
	}
	sink->scene(sink->ctx, UnitScale(pScene));
	int32_t lCount = 0;
	WalkNode(pScene->mRootNode, -1, sink, &lCount);
	if(mode == 0)
	{
		WalkMaterials(pScene, sink);
		WalkMeshes(pScene, sink);
	}
	return 0;
}

void ohpl_ai_mul(const float* a, const float* b, float* out) { Put(out, Mat(a) * Mat(b)); }

void ohpl_ai_inverse(const float* a, float* out) { aiMatrix4x4 m = Mat(a); m.Inverse(); Put(out, m); }

void ohpl_ai_rigid(const float* a, float* out) { Put(out, Rigid(Mat(a))); }

float ohpl_ai_scale_x(const float* a)
{
	aiVector3D vScale, vPos;
	aiQuaternion qRot;
	Mat(a).Decompose(vScale, qRot, vPos);
	return vScale.x;
}

// One mesh's vertex arrays for the node with world matrix `world`: position (4), normal (3),
// texture0 (3), color0 (4), tangent (4) floats per vertex.
void ohpl_ai_vertices(const float* world, uint32_t vertex_num, const float* vertices, const float* normals, const float* uv0,
					const float* tangents, const float* bitangents,
					float* out_pos, float* out_nrm, float* out_uv, float* out_col, float* out_tan)
{
	aiMatrix4x4 mtxWorld = Mat(world);
	aiMatrix3x3 mtxNormal(mtxWorld);
	mtxNormal.Inverse().Transpose();
	const aiVector3D* pVtx = (const aiVector3D*)vertices;
	const aiVector3D* pNrm = (const aiVector3D*)normals;
	const aiVector3D* pUv = (const aiVector3D*)uv0;
	const aiVector3D* pTan = (const aiVector3D*)tangents;
	const aiVector3D* pBit = (const aiVector3D*)bitangents;

	for(uint32_t v=0; v<vertex_num; ++v)
	{
		aiVector3D vPos = mtxWorld * pVtx[v];
		aiVector3D vNrm = (mtxNormal * pNrm[v]).NormalizeSafe();

		out_pos[v*4+0] = vPos.x; out_pos[v*4+1] = vPos.y; out_pos[v*4+2] = vPos.z; out_pos[v*4+3] = 1.0f;
		out_nrm[v*3+0] = vNrm.x; out_nrm[v*3+1] = vNrm.y; out_nrm[v*3+2] = vNrm.z;

		float fU = 0, fV = 0;
		if(pUv) { fU = pUv[v].x; fV = pUv[v].y; }
		out_uv[v*3+0] = fU; out_uv[v*3+1] = fV; out_uv[v*3+2] = 0;

		out_col[v*4+0] = 1; out_col[v*4+1] = 1; out_col[v*4+2] = 1; out_col[v*4+3] = 1;

		float vTanX = 1, vTanY = 0, vTanZ = 0;
		float fHandedness = 1;
		if(pTan && pBit)
		{
			aiVector3D vT = (mtxNormal * pTan[v]).NormalizeSafe();
			aiVector3D vB = mtxNormal * pBit[v];
			vTanX = vT.x; vTanY = vT.y; vTanZ = vT.z;
			fHandedness = ((vNrm ^ vT) * vB) < 0 ? -1.0f : 1.0f;
		}
		out_tan[v*4+0] = vTanX; out_tan[v*4+1] = vTanY; out_tan[v*4+2] = vTanZ; out_tan[v*4+3] = fHandedness;
	}
}

}
