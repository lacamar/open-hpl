#include "impl/RustL6.h"

#include <cstring>
#include <vector>

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Platform.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/Material.h"
#include "graphics/Skeleton.h"
#include "graphics/Bone.h"
#include "graphics/Animation.h"
#include "graphics/AnimationTrack.h"
#include "resources/MaterialManager.h"
#include "resources/Resources.h"
#include "scene/Node3D.h"
#include "impl/MeshLoaderMSH.h"
#include "impl/MeshLoaderCollada.h"
#include "resources/MeshManager.h"

namespace hpl {

	namespace {

	struct Ctx
	{
		OhplL6Env env;
		const tWString* file;
		tString file8;
		cMesh* mesh = NULL;
		cSkeleton* skel = NULL;
		std::vector<cBone*> bones;
		std::vector<cNode3D*> nodes;
		cSubMesh* sub = NULL;
		iVertexBuffer* vb = NULL;
		cAnimation* anim = NULL;
		cAnimation* result = NULL;
		tString scratch;
	};

	Ctx& C(void* p) { return *static_cast<Ctx*>(p); }

	tString S(OhplL6Str s) { return tString((const char*)s.p, s.n); }

	OhplL6Str Out(const tString& s) { OhplL6Str r = { (const uint8_t*)s.data(), s.size() }; return r; }

	tWString Wide(Ctx& c, OhplL6Str s)
	{
		tString s8 = S(s);
		return s8 == c.file8 ? *c.file : cString::To16Char(s8);
	}

	void Mtx(cMatrixf& aDst, const float* apSrc) { memcpy(aDst.v, apSrc, sizeof(float) * 16); }

	void BLog(void*, int aLevel, OhplL6Str aMsg)
	{
		tString s = S(aMsg);
		if(aLevel == eOhplL6Error) Error("%s", s.c_str());
		else if(aLevel == eOhplL6Warning) Warning("%s", s.c_str());
		else Log("%s", s.c_str());
	}

	bool BFileExists(void* p, OhplL6Str aPath) { return cPlatform::FileExists(Wide(C(p), aPath)); }

	int64_t BFileDate(void* p, OhplL6Str aPath)
	{
		cDate d = cPlatform::FileModifiedDate(Wide(C(p), aPath));
		int64_t k = d.year;
		k = k * 13 + d.month;
		k = k * 32 + d.month_day;
		k = k * 24 + d.hours;
		k = k * 60 + d.minutes;
		return k * 60 + d.seconds;
	}

	OhplL6Str BCacheFile(void* p, OhplL6Str aPath, OhplL6Str aExt)
	{
		Ctx& c = C(p);
		c.scratch = cString::To8Char(cResources::GetCacheFile(Wide(c, aPath), cString::To16Char(S(aExt))));
		return Out(c.scratch);
	}

	OhplL6Str BRelativeDir(void* p, OhplL6Str aPath)
	{
		Ctx& c = C(p);
		c.scratch = cString::To8Char(cString::GetRelativePathW(cString::GetFilePathW(Wide(c, aPath)), cPlatform::GetWorkingDir()));
		return Out(c.scratch);
	}

	bool BSaveMesh(void* p, OhplL6Str aCacheFile)
	{
		Ctx& c = C(p);
		return c.env.msh && c.mesh && c.env.msh->SaveMesh(c.mesh, Wide(c, aCacheFile));
	}

	bool BSaveAnim(void* p, OhplL6Str aCacheFile)
	{
		Ctx& c = C(p);
		return c.env.msh && c.result && c.env.msh->SaveAnimation(c.result, Wide(c, aCacheFile));
	}

	void BSubSelect(void* p, int alIndex)
	{
		Ctx& c = C(p);
		c.sub = c.mesh->GetSubMesh(alIndex);
	}

	void BMeshNew(void* p, OhplL6Str aName, OhplL6Str aPath)
	{
		Ctx& c = C(p);
		c.mesh = hplNew( cMesh, (S(aName), Wide(c, aPath), c.env.materials, c.env.animations) );
		c.skel = NULL;
		c.bones.clear();
		c.nodes.clear();
	}

	void BMeshSetPath(void* p, OhplL6Str aPath)
	{
		Ctx& c = C(p);
		c.mesh->SetFullPath(Wide(c, aPath));
	}

	void BMeshDrop(void* p)
	{
		Ctx& c = C(p);
		hplDelete(c.mesh);
		c.mesh = NULL;
	}

	void BMeshCompile(void* p) { C(p).mesh->CompileBonesAndSubMeshes(); }

	void BSkeletonBegin(void* p)
	{
		Ctx& c = C(p);
		c.skel = hplNew( cSkeleton, () );
		c.bones.clear();
	}

	void BBone(void* p, const OhplL6Bone* apBone)
	{
		Ctx& c = C(p);
		cBone *pParent = apBone->parent < 0 ? c.skel->GetRootBone() : c.bones[apBone->parent];
		cBone *pBone = pParent->CreateChildBone(S(apBone->name), S(apBone->sid));
		cMatrixf mtx;
		Mtx(mtx, apBone->m);
		pBone->SetTransform(mtx);
		if(apBone->hasInvBind)
		{
			cMatrixf mtxInv;
			Mtx(mtxInv, apBone->invBind);
			pBone->SetInvBindTransform(mtxInv);
		}
		if(apBone->hasUnitScale) pBone->SetLocalUnitScale(apBone->unitScale);
		if(apBone->value) pBone->SetValue(apBone->value);
		c.bones.push_back(pBone);
	}

	void BSkeletonEnd(void* p, int aFileUnits)
	{
		Ctx& c = C(p);
		c.skel->mbFileUnits = aFileUnits != 0;
		c.mesh->SetSkeleton(c.skel);
	}

	void BNode(void* p, int aParent, OhplL6Str aName, const float* apMtx, int aFlags)
	{
		Ctx& c = C(p);
		cNode3D *pParent = aParent < 0 ? c.mesh->GetRootNode() : c.nodes[aParent];
		cNode3D *pNode = pParent->CreateChild(S(aName));
		c.mesh->AddNode(pNode);
		cMatrixf mtx;
		Mtx(mtx, apMtx);
		pNode->SetMatrix(mtx);
		pNode->SetCustomFlags(aFlags);
		c.nodes.push_back(pNode);
	}

	void BSubNew(void* p, OhplL6Str aName, const float* apMtx, const float* apScale, int aCollide)
	{
		Ctx& c = C(p);
		c.sub = c.mesh->CreateSubMesh(S(aName));
		if(apMtx)
		{
			cMatrixf mtx;
			Mtx(mtx, apMtx);
			c.sub->SetLocalTransform(mtx);
		}
		if(apScale) c.sub->SetModelScale(cVector3f(apScale[0], apScale[1], apScale[2]));
		if(aCollide >= 0) c.sub->SetIsCollideShape(aCollide != 0);
	}

	uintptr_t BMaterialCreate(void* p, OhplL6Str aName)
	{
		return (uintptr_t)C(p).env.materials->CreateMaterial(S(aName));
	}

	void BSubMaterial(void* p, OhplL6Str aName, uintptr_t aMaterial)
	{
		Ctx& c = C(p);
		c.sub->SetMaterialName(S(aName));
		if(aMaterial) c.sub->SetMaterial((cMaterial*)aMaterial);
	}

	void BSubCollider(void* p, const OhplL6Collider* apColl)
	{
		cMeshCollider *pCollider = C(p).sub->CreateCollider((eCollideShapeType)apColl->kind);
		Mtx(pCollider->m_mtxOffset, apColl->m);
		pCollider->mvSize = cVector3f(apColl->size[0], apColl->size[1], apColl->size[2]);
		pCollider->mbCharCollider = apColl->charCollider != 0;
	}

	void BSubPairs(void* p, const OhplL6Pair* apPairs, size_t alNum)
	{
		cSubMesh *pSub = C(p).sub;
		for(size_t i=0; i<alNum; ++i)
			pSub->AddVertexBonePair(cVertexBonePair(apPairs[i].vtx, apPairs[i].bone, apPairs[i].weight));
	}

	void BSubVbNew(void* p, const OhplL6VbCfg* apCfg)
	{
		Ctx& c = C(p);
		c.vb = c.env.lowLevel->CreateVertexBuffer(eVertexBufferType_Hardware, (eVertexBufferDrawType)apCfg->drawType,
												(eVertexBufferUsageType)apCfg->usageType, apCfg->reserveVtx, apCfg->reserveIdx);
	}

	void* ArrayData(iVertexBuffer* apVtx, eVertexBufferElement aElement, eVertexBufferElementFormat aFormat)
	{
		switch(aFormat)
		{
		case eVertexBufferElementFormat_Float: return apVtx->GetFloatArray(aElement);
		case eVertexBufferElementFormat_Int: return apVtx->GetIntArray(aElement);
		default: return apVtx->GetByteArray(aElement);
		}
	}

	void BSubVbArray(void* p, const OhplMshArray* apArr)
	{
		iVertexBuffer *pVtx = C(p).vb;
		eVertexBufferElement type = (eVertexBufferElement)apArr->kind;
		eVertexBufferElementFormat format = (eVertexBufferElementFormat)apArr->format;
		pVtx->CreateElementArray(type, format, apArr->elemNum, apArr->progVar);
		for(int rep=0; rep<apArr->repeats; ++rep) Error("Vertex element of type %d already present in buffer %d!\n", type, (int)(size_t)pVtx);
		pVtx->ResizeArray(type, (int)apArr->count);
		if(apArr->bytes > 0) memcpy(ArrayData(pVtx, type, format), apArr->data, apArr->bytes);
	}

	void BSubVbIndices(void* p, const uint32_t* apIdx, size_t alNum)
	{
		iVertexBuffer *pVtx = C(p).vb;
		pVtx->ResizeIndices((int)alNum);
		if(alNum > 0) memcpy(pVtx->GetIndices(), apIdx, alNum * sizeof(unsigned int));
	}

	void BSubVbTransform(void* p, const float* apMtx)
	{
		cMatrixf mtx;
		Mtx(mtx, apMtx);
		C(p).vb->Transform(mtx);
	}

	void BSubVbCompile(void* p) { C(p).vb->Compile(0); }

	void BSubVbAttach(void* p)
	{
		Ctx& c = C(p);
		c.sub->SetVertexBuffer(c.vb);
	}

	void BSubCompile(void* p) { C(p).sub->Compile(); }

	void BAnimNew(void* p, OhplL6Str aName, OhplL6Str aFileName, OhplL6Str aAnimName, float afLength, size_t alTracks)
	{
		Ctx& c = C(p);
		c.anim = hplNew(cAnimation, (S(aName), *c.file, S(aFileName)));
		if(aAnimName.p) c.anim->SetAnimationName(S(aAnimName));
		c.anim->SetLength(afLength);
		c.anim->ReserveTrackNum((int)alTracks);
	}

	void BAnimTrack(void* p, OhplL6Str aName, int aFlags, const OhplMshKey* apKeys, size_t alNum)
	{
		cAnimationTrack *pTrack = C(p).anim->CreateTrack(S(aName), (tAnimTransformFlag)aFlags);
		for(size_t i=0; i<alNum; ++i)
		{
			cKeyFrame *pFrame = pTrack->CreateKeyFrame(apKeys[i].time);
			pFrame->trans = cVector3f(apKeys[i].trans[0], apKeys[i].trans[1], apKeys[i].trans[2]);
			pFrame->rotation.v = cVector3f(apKeys[i].rot[0], apKeys[i].rot[1], apKeys[i].rot[2]);
			pFrame->rotation.w = apKeys[i].rot[3];
		}
	}

	void BAnimUnitScale(void* p, float afScale)
	{
		cAnimation *pAnim = C(p).anim;
		pAnim->GetTrack(pAnim->GetTrackNum() - 1)->SetUnitScale(afScale);
	}

	void BAnimSetPath(void* p, OhplL6Str aPath)
	{
		Ctx& c = C(p);
		c.result->SetFullPath(Wide(c, aPath));
	}

	void BAnimEnd(void* p, int aAddToMesh)
	{
		Ctx& c = C(p);
		if(aAddToMesh) c.mesh->AddAnimation(c.anim);
		else c.result = c.anim;
		c.anim = NULL;
	}

	OhplL6Builder MakeBuilder(Ctx& c)
	{
		OhplL6Builder b = {};
		b.ctx = &c;
		b.log = BLog;
		b.file_exists = BFileExists;
		b.file_date = BFileDate;
		b.cache_file = BCacheFile;
		b.relative_dir = BRelativeDir;
		b.save_mesh = BSaveMesh;
		b.save_anim = BSaveAnim;
		b.sub_select = BSubSelect;
		b.mesh_new = BMeshNew;
		b.mesh_set_path = BMeshSetPath;
		b.mesh_drop = BMeshDrop;
		b.mesh_compile = BMeshCompile;
		b.skeleton_begin = BSkeletonBegin;
		b.bone = BBone;
		b.skeleton_end = BSkeletonEnd;
		b.node = BNode;
		b.sub_new = BSubNew;
		b.material_create = BMaterialCreate;
		b.sub_material = BSubMaterial;
		b.sub_collider = BSubCollider;
		b.sub_pairs = BSubPairs;
		b.sub_vb_new = BSubVbNew;
		b.sub_vb_array = BSubVbArray;
		b.sub_vb_indices = BSubVbIndices;
		b.sub_vb_transform = BSubVbTransform;
		b.sub_vb_compile = BSubVbCompile;
		b.sub_vb_attach = BSubVbAttach;
		b.sub_compile = BSubCompile;
		b.anim_new = BAnimNew;
		b.anim_track = BAnimTrack;
		b.anim_unit_scale = BAnimUnitScale;
		b.anim_set_path = BAnimSetPath;
		b.anim_end = BAnimEnd;
		return b;
	}

	} // namespace

	void OhplL6FillCfg(OhplL6CfgBuf& aBuf, cMeshManager* apMeshManager, bool abLoadAndSaveMsh, uint32_t alMeshFlags)
	{
		OhplL6Cfg& c = aBuf.cfg;
		memset(&c, 0, sizeof(c));
		c.boneUnitScale = cMeshLoaderMSH::mbBoneUnitScale;
		c.keepHpl3Anims = cMeshLoaderMSH::mbKeepHpl3Animations;
		c.convertUnit = cMeshLoaderCollada::GetConvertUnit();
		c.convertUnitAny = cMeshLoaderCollada::GetConvertUnitFromAnyTool();
		c.unscaledSkeleton = cMeshLoaderCollada::GetUnscaledSkeleton();
		c.loadAndSaveMsh = abLoadAndSaveMsh;
		c.forceCacheSkipSaving = cResources::GetForceCacheLoadingAndSkipSaving();
		c.meshFlags = alMeshFlags;
		aBuf.cacheDir = cString::To8Char(cResources::GetMeshCacheDir());
		aBuf.workDir = cString::To8Char(cPlatform::GetWorkingDir());
		if(apMeshManager)
		{
			c.useFastload = apMeshManager->GetUseFastloadMaterial();
			aBuf.fastload = apMeshManager->GetFastloadMaterial();
		}
		c.meshCacheDir = Out(aBuf.cacheDir);
		c.workingDir = Out(aBuf.workDir);
		c.fastloadMaterial = Out(aBuf.fastload);
	}

	cMesh* OhplL6LoadMesh(int aKind, const OhplL6Env& aEnv, const OhplL6Cfg& aCfg, const tWString& asFile)
	{
		Ctx c;
		c.env = aEnv;
		c.file = &asFile;
		c.file8 = cString::To8Char(asFile);
		OhplL6Builder b = MakeBuilder(c);
		OhplL6Str path = { (const uint8_t*)c.file8.data(), c.file8.size() };
		int lRet = aKind == eOhplL6KindMsh ? ohpl_l6_msh_mesh(&aCfg, &b, path)
				: aKind == eOhplL6KindCollada ? ohpl_l6_collada_mesh(&aCfg, &b, path)
				: ohpl_l6_assimp_mesh(&aCfg, &b, path);
		return lRet == 0 ? c.mesh : NULL;
	}

	cAnimation* OhplL6LoadAnim(int aKind, const OhplL6Env& aEnv, const OhplL6Cfg& aCfg, const tWString& asFile)
	{
		Ctx c;
		c.env = aEnv;
		c.file = &asFile;
		c.file8 = cString::To8Char(asFile);
		OhplL6Builder b = MakeBuilder(c);
		OhplL6Str path = { (const uint8_t*)c.file8.data(), c.file8.size() };
		int lRet = aKind == eOhplL6KindMsh ? ohpl_l6_msh_anim(&aCfg, &b, path)
				: aKind == eOhplL6KindCollada ? ohpl_l6_collada_anim(&aCfg, &b, path)
				: ohpl_l6_assimp_anim(&aCfg, &b, path);
		return lRet == 0 ? c.result : NULL;
	}

}
