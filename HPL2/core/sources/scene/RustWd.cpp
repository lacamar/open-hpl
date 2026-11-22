// Engine objects behind the Rust-owned cWorld (rust/src/scene/wd_host.rs); each thunk goes with the C++ class it wraps.
// The AI node container calls stay C++ behind cWorld.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "scene/Terrain.h"
#include "scene/World.h"
#include "system/Script.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "math/Math.h"
#include "math/MathTypes.h"
#include "engine/Engine.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/MeshCreator.h"
#include "graphics/VertexBuffer.h"
#include "resources/ParticleManager.h"
#include "resources/ScriptManager.h"
#include "resources/MaterialManager.h"
#include "resources/TextureManager.h"
#include "resources/EntFileManager.h"
#include "resources/FileSearcher.h"
#include "resources/Resources.h"
#include "resources/SoundEntityManager.h"
#include "resources/LowLevelResources.h"
#include "resources/XmlDocument.h"
#include "scene/Scene.h"
#include "scene/Node3D.h"
#include "scene/LightPoint.h"
#include "scene/LightSpot.h"
#include "scene/LightBox.h"
#include "scene/LightDirectional.h"
#include "scene/MeshEntity.h"
#include "scene/SoundEntity.h"
#include "scene/ParticleEmitter.h"
#include "scene/ParticleSystem.h"
#include "scene/BillBoard.h"
#include "scene/LensFlare.h"
#include "scene/Beam.h"
#include "scene/GuiSetEntity.h"
#include "scene/RopeEntity.h"
#include "scene/FogArea.h"
#include "scene/ForceField.h"
#include "scene/EnvironmentParticles.h"
#include "scene/RenderableContainer_List.h"
#include "scene/RenderableContainer_BoxTree.h"
#include "scene/RenderableContainer_DynBoxTree.h"
#include "scene/DummyRenderable.h"
#include "scene/RustWorld.h"
#include "scene/RustSceneH13.h"
#include "system/System.h"
#include "system/Platform.h"
#include "sound/SoundEntityData.h"
#include "sound/Sound.h"
#include "sound/SoundHandler.h"
#include "physics/Physics.h"
#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsJoint.h"
#include "ai/AI.h"
#include "ai/AINodeContainer.h"
#include "ai/AINodeGenerator.h"
#include "ai/AStar.h"
#include "scene/RustWd.h"
#include "system/MemoryManager.h"

namespace hpl {

	namespace {
		cGraphics* Gfx(void *p){ return static_cast<cGraphics*>(p); }
		cResources* Res(void *p){ return static_cast<cResources*>(p); }
		tString Str(const char *p, size_t n){ return tString(p, n); }
		iRenderable* Rend(void *p){ return static_cast<iRenderable*>(p); }

		void RegForeign(void *r)
		{
			OhplH13Reg *pReg = static_cast<OhplH13Reg*>(r);
			OhplH13Foreign<iLight>(pReg, kOhplH13Light);
			OhplH13Foreign<cMeshEntity>(pReg, kOhplH13DynMesh);
			OhplH13Foreign<cMeshEntity>(pReg, kOhplH13StaticMesh);
			OhplH13Foreign<cBillboard>(pReg, kOhplH13Billboard);
			OhplH13Foreign<cLensFlare>(pReg, kOhplH13LensFlare);
			OhplH13Foreign<cBeam>(pReg, kOhplH13Beam);
			OhplH13Foreign<cParticleSystem>(pReg, kOhplH13ParticleSystem);
			OhplH13Foreign<cGuiSetEntity>(pReg, kOhplH13GuiSet);
			OhplH13Foreign<cRopeEntity>(pReg, kOhplH13Rope);
			OhplH13Foreign<cSoundEntity>(pReg, kOhplH13Sound);
			OhplH13Foreign<cStartPosEntity>(pReg, kOhplH13StartPos);
			OhplH13Foreign<cFogArea>(pReg, kOhplH13FogArea);
			OhplH13Foreign<cForceField>(pReg, kOhplH13ForceField);
			OhplH13Foreign<cDummyRenderable>(pReg, kOhplH13Dummy);
			OhplH13Foreign<cAINodeContainer>(pReg, kOhplH13AiContainer);
			OhplH13Foreign<cAStarHandler>(pReg, kOhplH13AStar);
			OhplH13Foreign<cEnvironmentParticles>(pReg, kOhplH13EnvParticles);
			OhplH13Foreign<cAreaEntity>(pReg, kOhplH13Area);
		}

		void* RootNew(){ return hplNew( cNode3D, () ); }
		void RootDelete(void *n){ hplDelete(static_cast<cNode3D*>(n)); }
		void* ContNew(bool abDynamic)
		{
			if(abDynamic) return static_cast<iRenderableContainer*>(hplNew( cRenderableContainer_DynBoxTree, () ));
			return static_cast<iRenderableContainer*>(hplNew( cRenderableContainer_BoxTree, () ));
		}
		iRenderableContainer* C(void *c){ return static_cast<iRenderableContainer*>(c); }
		void ContDelete(void *c){ hplDelete(C(c)); }
		void ContAdd(void *c, void *r){ C(c)->Add(Rend(r)); }
		void ContRemove(void *c, void *r){ C(c)->Remove(Rend(r)); }
		void ContRemoveSet(void *c, const size_t *apSet, size_t alNum)
		{
			std::set<iRenderable*> setObjs;
			for(size_t i=0; i<alNum; ++i) setObjs.insert(Rend((void*)apSet[i]));
			C(c)->RemoveSet(setObjs);
		}
		void ContCompile(void *c){ C(c)->Compile(); }
		void ContBounds(void *c, cVector3f *apOut)
		{
			iRenderableContainerNode *pRoot = C(c)->GetRoot();
			apOut[0] = pRoot->GetMin();
			apOut[1] = pRoot->GetMax();
		}

		void* RendOf(uint32_t alKind, void *p)
		{
			switch(alKind)
			{
			case kOhplH13Light: return static_cast<iRenderable*>(static_cast<iLight*>(p));
			case kOhplH13Billboard: return static_cast<iRenderable*>(static_cast<cBillboard*>(p));
			case kOhplH13LensFlare: return static_cast<iRenderable*>(static_cast<cLensFlare*>(p));
			case kOhplH13Beam: return static_cast<iRenderable*>(static_cast<cBeam*>(p));
			case kOhplH13GuiSet: return static_cast<iRenderable*>(static_cast<cGuiSetEntity*>(p));
			case kOhplH13Rope: return static_cast<iRenderable*>(static_cast<cRopeEntity*>(p));
			case kOhplH13FogArea: return static_cast<iRenderable*>(static_cast<cFogArea*>(p));
			case kOhplH13Dummy: return static_cast<iRenderable*>(static_cast<cDummyRenderable*>(p));
			default: return NULL;
			}
		}
		bool IsStatic(uint32_t alKind, void *p)
		{
			if(alKind == kOhplH13DynMesh) return static_cast<cMeshEntity*>(p)->IsStatic();
			return Rend(alKind == UINT32_MAX ? p : RendOf(alKind, p))->IsStatic();
		}
		void SetStatic(uint32_t alKind, void *p, bool abX)
		{
			if(alKind == kOhplH13DynMesh) static_cast<cMeshEntity*>(p)->SetStatic(abX);
			else if(alKind == kOhplH13ForceField) static_cast<cForceField*>(p)->SetStatic(abX);
			else Rend(alKind == UINT32_MAX ? p : RendOf(alKind, p))->SetStatic(abX);
		}
		void SetWorld(uint32_t alKind, void *p, void *apWorld)
		{
			cWorld *pWorld = static_cast<cWorld*>(apWorld);
			if(alKind == kOhplH13Light) static_cast<iLight*>(p)->SetWorld(pWorld);
			else if(alKind == kOhplH13DynMesh) static_cast<cMeshEntity*>(p)->SetWorld(pWorld);
			else static_cast<iParticleEmitter*>(Rend(p))->SetWorld(pWorld);
		}

		void* Create(uint32_t alKind, OhplWdPod *apPod, const OhplWdCreate *a)
		{
			const tString sName = Str(a->name, a->nameLen);
			cResources *pRes = apPod->mpResources;
			cGraphics *pGfx = apPod->mpGraphics;
			switch(alKind)
			{
			case kOhplH13Light:
				if(a->which == 0) return static_cast<iLight*>(hplNew( cLightPoint, (sName, pRes) ));
				if(a->which == 1) return static_cast<iLight*>(hplNew( cLightSpot, (sName, pRes) ));
				return static_cast<iLight*>(hplNew( cLightBox, (sName, pRes) ));
			case kOhplH13Billboard: return hplNew( cBillboard, (sName, a->size2, (eBillboardType)a->ty, pRes, pGfx) );
			case kOhplH13LensFlare: return hplNew( cLensFlare, (sName, pRes, pGfx) );
			case kOhplH13Beam: return hplNew( cBeam, (sName, pRes, pGfx) );
			case kOhplH13GuiSet: return hplNew( cGuiSetEntity, (sName, static_cast<cGuiSet*>(a->a)) );
			case kOhplH13Rope: return hplNew( cRopeEntity, (sName, pRes, pGfx, static_cast<iPhysicsRope*>(a->a), a->id) );
			case kOhplH13FogArea: return hplNew( cFogArea, (sName, pRes) );
			case kOhplH13ForceField: return hplNew( cForceField, (sName, a->flag) );
			case kOhplH13Dummy: return hplNew( cDummyRenderable, (sName) );
			case kOhplH13DynMesh:
				return hplNew( cMeshEntity, (sName, static_cast<cMesh*>(a->a), pRes->GetMaterialManager(), pRes->GetMeshManager(), pRes->GetAnimationManager()) );
			case kOhplH13Sound:
				return hplNew( cSoundEntity, (sName, static_cast<cSoundEntityData*>(a->a), pRes->GetSoundEntityManager(), apPod->mpOwner,
											  apPod->mpSound->GetSoundHandler(), a->flag, a->id) );
			case kOhplH13EnvParticles: return hplNew( cEnvironmentParticles, (sName, apPod->mpOwner, pGfx, pRes) );
			}
			return NULL;
		}

		void LightGobo(void *r, bool abSpot, void *l, const char *s, size_t n)
		{
			cTextureManager *pMgr = Res(r)->GetTextureManager();
			const tString sGobo = Str(s, n);
			iLight *pLight = static_cast<iLight*>(l);
			pMgr->SetSRGB(true);
			iTexture *pTexture = abSpot ? pMgr->Create2D(sGobo, true) : pMgr->CreateCubeMap(sGobo, true);
			pMgr->SetSRGB(false);
			if(pTexture != NULL) pLight->SetGoboTexture(pTexture);
			else Warning("Couldn't load gobo texture '%s' for light '%s'", sGobo.c_str(), pLight->GetName().c_str());
		}
		void BbMaterial(void *b, void *m){ static_cast<cBillboard*>(b)->SetMaterial(static_cast<cMaterial*>(m)); }
		void FlareHalo(void *f, const cVector2f *apSize)
		{
			cLensFlare *pFlare = static_cast<cLensFlare*>(f);
			pFlare->SetFlareSize(eLensFlareType_Halo, *apSize);
			pFlare->SetFlareActive(eLensFlareType_Halo, true);
		}
		void FlareMaterial(void *f, void *m){ static_cast<cLensFlare*>(f)->SetMaterial(eLensFlareType_Halo, static_cast<cMaterial*>(m)); }

		size_t MeshSubs(void *m, void **apOut, size_t alCap, bool abSkipCollide)
		{
			cMeshEntity *pMesh = static_cast<cMeshEntity*>(m);
			size_t lNum = 0;
			for(int i=0; i<pMesh->GetSubMeshEntityNum() && lNum < alCap; ++i)
			{
				cSubMeshEntity *pSub = pMesh->GetSubMeshEntity(i);
				if(abSkipCollide && pSub->GetSubMesh()->IsCollideShape()) continue;
				apOut[lNum++] = static_cast<iRenderable*>(pSub);
			}
			return lNum;
		}
		void MeshDrawBb(void *g, void *m, const cColor *apColor)
		{
			cBoundingVolume *pBV = static_cast<cMeshEntity*>(m)->GetBoundingVolume();
			Gfx(g)->GetLowLevel()->DrawBoxMinMax(pBV->GetMin(), pBV->GetMax(), *apColor);
		}

		void* PsCreate(void *r, int32_t alMode, const char *asName, size_t alNameLen, const char *asType, size_t alTypeLen, void *apElem, const cVector3f *apSize)
		{
			cParticleManager *pMgr = Res(r)->GetParticleManager();
			if(alMode == 0) return pMgr->CreatePS(Str(asName, alNameLen), Str(asType, alTypeLen), *apSize);
			return pMgr->CreatePS(Str(asName, alNameLen), Str(asType, alTypeLen), static_cast<cXmlElement*>(apElem), *apSize);
		}
		size_t PsEmitters(void *p, void **apOut, size_t alCap)
		{
			cParticleSystem *pPS = static_cast<cParticleSystem*>(p);
			size_t lNum = 0;
			for(int i=0; i<pPS->GetEmitterNum() && lNum < alCap; ++i) apOut[lNum++] = static_cast<iRenderable*>(pPS->GetEmitter(i));
			return lNum;
		}
		void PsRemoveWhenDead(void *p, bool abX){ static_cast<cParticleSystem*>(p)->SetRemoveWhenDead(abX); }

		int32_t Update(uint32_t alKind, void *p, float afTimeStep)
		{
			switch(alKind)
			{
			case kOhplH13DynMesh:
			{
				cMeshEntity *pEntity = static_cast<cMeshEntity*>(p);
				if(pEntity->IsActive()) pEntity->UpdateLogic(afTimeStep);
				return 0;
			}
			case kOhplH13Light:
			{
				iLight *pLight = static_cast<iLight*>(p);
				if(pLight->IsActive()) pLight->UpdateLogic(afTimeStep);
				return 0;
			}
			case kOhplH13LensFlare: static_cast<cLensFlare*>(p)->UpdateLogic(afTimeStep); return 0;
			case kOhplH13EnvParticles: static_cast<cEnvironmentParticles*>(p)->Update(afTimeStep); return 0;
			case kOhplH13ParticleSystem:
			{
				cParticleSystem *pPS = static_cast<cParticleSystem*>(p);
				pPS->UpdateLogic(afTimeStep);
				return pPS->GetRemoveWhenDead() && pPS->IsDead();
			}
			case kOhplH13ForceField:
			{
				cForceField *pField = static_cast<cForceField*>(p);
				if(pField->IsActive()) pField->UpdateLogic(afTimeStep);
				return pField->GetAutoRemove() && pField->IsDead();
			}
			case kOhplH13Sound:
			{
				cSoundEntity *pSound = static_cast<cSoundEntity*>(p);
				if(pSound->IsActive()) pSound->UpdateLogic(afTimeStep);
				return pSound->IsStopped() && pSound->GetRemoveWhenOver();
			}
			}
			return 0;
		}

		iPhysicsWorld* Pw(void *p){ return static_cast<iPhysicsWorld*>(p); }
		void PhysUpdate(void *p, float afTimeStep){ Pw(p)->Update(afTimeStep); }
		void PhysAttach(void *p, void *w){ Pw(p)->SetWorld(static_cast<cWorld*>(w)); }
		void PhysDestroy(void *ph, void *p){ static_cast<cPhysics*>(ph)->DestroyWorld(Pw(p)); }
		void PhysSetSize(void *p, const cVector3f *apBounds){ Pw(p)->SetWorldSize(apBounds[0], apBounds[1]); }
		void PhysClearSounds(void *p)
		{
			cPhysicsBodyIterator bodyIt = Pw(p)->GetBodyIterator();
			while(bodyIt.HasNext())
			{
				iPhysicsBody *pBody = bodyIt.Next();
				pBody->SetScrapeSoundEntity(NULL);
				pBody->SetRollSoundEntity(NULL);
			}
			cPhysicsJointIterator jointIt = Pw(p)->GetJointIterator();
			while(jointIt.HasNext()) jointIt.Next()->SetSound(NULL);
		}
		void SoundSilent(void *s, bool abX){ static_cast<cSound*>(s)->GetSoundHandler()->SetSilent(abX); }
		void* SoundData(void *r, const char *s, size_t n){ return Res(r)->GetSoundEntityManager()->CreateSoundEntity(Str(s, n)); }
		int32_t SoundCreationId(void *p){ return static_cast<cSoundEntity*>(p)->GetCreationID(); }
		void* SkyVbNew(void *g){ return Gfx(g)->GetMeshCreator()->CreateSkyBoxVertexBuffer(1); }
		void* DirLightNew(void *r){ return hplNew( cLightDirectional, ("WorldDirectional", Res(r)) ); }
		void DirLightDelete(void *l){ hplDelete(static_cast<cLightDirectional*>(l)); }
		void TerrainDelete(void *t){ hplDelete(static_cast<cTerrain*>(t)); }

		void* EntFileCreate(void *r, const char *s, size_t n){ return Res(r)->GetEntFileManager()->CreateEntFile(Str(s, n)); }
		void EntFileDestroy(void *r, void *e){ Res(r)->GetEntFileManager()->Destroy(static_cast<cEntFile*>(e)); }
		void EntFileInfo(void *e, const char **apType, size_t *apTypeLen, const char **apName, size_t *apNameLen, bool *apHasVars)
		{
			static thread_local tString sType;
			cEntFile *pEnt = static_cast<cEntFile*>(e);
			cXmlElement *pVars = pEnt->GetXmlDoc()->GetFirstElement("UserDefinedVariables");
			*apHasVars = pVars != NULL;
			sType = pVars ? pVars->GetAttributeString("EntityType") : "";
			*apType = sType.data();
			*apTypeLen = sType.size();
			*apName = pEnt->GetName().data();
			*apNameLen = pEnt->GetName().size();
		}
		void* LoaderGet(void *r, const char *s, size_t n){ return Res(r)->GetEntityLoader(Str(s, n)); }
		bool LoaderStatic(void *l){ return static_cast<iEntityLoader*>(l)->GetCreatesStaticEntity(); }
		void* LoaderLoad(void *l, void *e, void *w, const OhplWdLoad *a)
		{
			cEntFile *pEnt = static_cast<cEntFile*>(e);
			return static_cast<iEntityLoader*>(l)->Load(Str(a->name, a->nameLen), a->id, a->active, pEnt->GetXmlDoc(), *a->mtx, *a->scale,
														static_cast<cWorld*>(w), pEnt->GetName(), pEnt->GetFullPath(), static_cast<cResourceVarsObject*>(a->vars));
		}
		void EntitySource(void *ent, void *e){ static_cast<iEntity3D*>(ent)->SetSourceFile(static_cast<cEntFile*>(e)->GetName()); }
		void TempNodesClear(void *w){ static_cast<cWorld*>(w)->ClearTempNodes(); }

		template<class T> void Fill(T *p, OhplWdDumpItem *apOut)
		{
			apOut->name = p->GetName().data();
			apOut->nameLen = p->GetName().size();
			apOut->uid = p->GetUniqueID();
			memcpy(apOut->mtx, p->GetWorldMatrix().v, sizeof(apOut->mtx));
		}
		void DumpItem(uint32_t alKind, void *p, OhplWdDumpItem *apOut)
		{
			switch(alKind)
			{
			case kOhplH13Light: Fill(static_cast<iLight*>(p), apOut); break;
			case kOhplH13DynMesh: case kOhplH13StaticMesh: Fill(static_cast<cMeshEntity*>(p), apOut); break;
			case kOhplH13Billboard: Fill(static_cast<cBillboard*>(p), apOut); break;
			case kOhplH13LensFlare: Fill(static_cast<cLensFlare*>(p), apOut); break;
			case kOhplH13Beam: Fill(static_cast<cBeam*>(p), apOut); break;
			case kOhplH13ParticleSystem: Fill(static_cast<cParticleSystem*>(p), apOut); break;
			case kOhplH13GuiSet: Fill(static_cast<cGuiSetEntity*>(p), apOut); break;
			case kOhplH13Rope: Fill(static_cast<cRopeEntity*>(p), apOut); break;
			case kOhplH13Sound: Fill(static_cast<cSoundEntity*>(p), apOut); break;
			case kOhplH13FogArea: Fill(static_cast<cFogArea*>(p), apOut); break;
			case kOhplH13ForceField: Fill(static_cast<cForceField*>(p), apOut); break;
			case kOhplH13Dummy: Fill(static_cast<cDummyRenderable*>(p), apOut); break;
			case kOhplH13StartPos:
			{
				cStartPosEntity *pStart = static_cast<cStartPosEntity*>(p);
				apOut->name = pStart->msName.data();
				apOut->nameLen = pStart->msName.size();
				apOut->crouch = pStart->mbCrouching;
				memcpy(apOut->mtx, pStart->m_mtxTransform.v, sizeof(apOut->mtx));
				break;
			}
			default:
			{
				cAreaEntity *pArea = static_cast<cAreaEntity*>(p);
				apOut->kindName = pArea->msType.data();
				apOut->kindLen = pArea->msType.size();
				memcpy(apOut->mtx, pArea->m_mtxTransform.v, sizeof(apOut->mtx));
				apOut->size[0] = pArea->mvSize.x;
				apOut->size[1] = pArea->mvSize.y;
				apOut->size[2] = pArea->mvSize.z;
				break;
			}
			}
		}
		void FieldInfo(void *p, OhplWdField *apOut)
		{
			cForceField *pField = static_cast<cForceField*>(p);
			apOut->usable = pField->IsActive() && pField->IsVisible();
			if(apOut->usable == false) return;
			cBoundingVolume *pBV = pField->GetBoundingVolume();
			apOut->min = pBV->GetMin();
			apOut->max = pBV->GetMax();
			apOut->radius = pField->GetFinalRadius();
		}
		void Log(bool abError, const uint8_t *s, size_t l)
		{
			if(abError) Error("%.*s", (int)l, (const char*)s);
			else Warning("%.*s", (int)l, (const char*)s);
		}

		struct cRegister
		{
			cRegister()
			{
				static const OhplWdHost h = { RegForeign, RootNew, RootDelete, ContNew, ContDelete, ContAdd, ContRemove, ContRemoveSet,
					ContCompile, ContBounds, RendOf, IsStatic, SetStatic, SetWorld, Create, LightGobo, BbMaterial, FlareHalo, FlareMaterial,
					MeshSubs, MeshDrawBb, PsCreate, PsEmitters, PsRemoveWhenDead, Update, PhysUpdate, PhysAttach, PhysDestroy,
					PhysSetSize, PhysClearSounds, SoundSilent, SoundData, SoundCreationId, SkyVbNew, DirLightNew, DirLightDelete,
					TerrainDelete, EntFileCreate, EntFileDestroy, EntFileInfo, LoaderGet, LoaderStatic, LoaderLoad, EntitySource,
					TempNodesClear, DumpItem, FieldInfo, Log };
				ohpl_wd_set_host(&h);
			}
		} gRegister;
	}

	cLightPoint* cWorld::CreateLightPoint(const tString &asName, const tString &asGobo, bool abStatic)
	{
		return static_cast<cLightPoint*>(static_cast<iLight*>(ohpl_wd_create_light(mpW, 0, asName.data(), asName.size(), asGobo.data(), asGobo.size(), abStatic)));
	}
	cLightSpot* cWorld::CreateLightSpot(const tString &asName, const tString &asGobo, bool abStatic)
	{
		return static_cast<cLightSpot*>(static_cast<iLight*>(ohpl_wd_create_light(mpW, 1, asName.data(), asName.size(), asGobo.data(), asGobo.size(), abStatic)));
	}
	cLightBox* cWorld::CreateLightBox(const tString &asName, bool abStatic)
	{
		return static_cast<cLightBox*>(static_cast<iLight*>(ohpl_wd_create_light(mpW, 2, asName.data(), asName.size(), "", 0, abStatic)));
	}
	void cWorld::ClearTempNodes(){ STLMapDeleteAll(m_mapTempNodes); }

	void  cWorld::GenerateAINodes(cAINodeGeneratorParams *apParams)
	{
		mpW->mpAI->GetNodeGenerator()->Generate(this, apParams);
	}

	//-----------------------------------------------------------------------

	cAINodeContainer* cWorld::CreateAINodeContainer(const tString &asName,
								const tString &asNodeName,
								const cVector3f &avSize,
								bool abNodeIsAtCenter,
								int alMinEdges, int alMaxEdges, float afMaxEdgeDistance,float afMaxHeight)
	{
		cAINodeContainer* pContainer=NULL;

		//unsigned long lStartTime = mpSystem->GetLowLevel()->GetTime();

		//////////////////////////////////
		//See if the container is allready loaded.
		pContainer = OhplH13Name<cAINodeContainer*>(mpW->mpH13Reg, kOhplH13AiContainer, asName, true);


		//////////////////////////////////
		//Get file name
		cFileSearcher *pFileSearcher = mpW->mpResources->GetFileSearcher();
		tWString sMapPath = GetFilePath();

		tWString sAiFileName = cString::SetFileExtW(sMapPath,_W(""));
		sAiFileName += _W("_")+cString::To16Char(asName);
		sAiFileName = cString::SetFileExtW(sAiFileName,_W("nodes"));
		tWString sAiCacheFile = cResources::GetCacheFile(sAiFileName, _W("nodes"));
		if(cPlatform::FileExists(sAiCacheFile)) sAiFileName = sAiCacheFile;

		//////////////////////////////////
		//If there is no container created, create it.
		if(pContainer == NULL)
		{
			tTempNodeContainerMapIt ContIt = m_mapTempNodes.find(asNodeName);
			if(ContIt == m_mapTempNodes.end()){
				Warning("AI node type '%s' does not exist!\n",asNodeName.c_str());
				return NULL;
			}
			cTempNodeContainer *pTempCont = ContIt->second;

			pContainer = hplNew( cAINodeContainer, (asName,asNodeName,this,avSize) );
			OhplH13Push(mpW->mpH13Reg, kOhplH13AiContainer, pContainer);

			//Set properties
			pContainer->SetMinEdges(alMinEdges);
			pContainer->SetMaxEdges(alMaxEdges);
			pContainer->SetMaxEdgeDistance(afMaxEdgeDistance);
			pContainer->SetMaxHeight(afMaxHeight);
			pContainer->SetNodeIsAtCenter(abNodeIsAtCenter);

			//Reserve space for the incoming nodes.
			pContainer->ReserveSpace(pTempCont->mlstNodes.size());

            //Add nodes to container
			tTempAiNodeListIt NodeIt = pTempCont->mlstNodes.begin();
			for(; NodeIt != pTempCont->mlstNodes.end(); ++NodeIt)
			{
				cTempAiNode& pNode = *NodeIt;
				pContainer->AddNode(pNode.msName,pNode.mlID,pNode.mvPos,NULL);
			}

			bool bLoadedFromFile=false;
			if(cPlatform::FileExists(sAiFileName))
			{
				cDate dateMapFile = cPlatform::FileModifiedDate(sMapPath);
				cDate dateAIFile = cPlatform::FileModifiedDate(sAiFileName);

				if((dateAIFile > dateMapFile && !getenv("OHPL_L9_XMLDUMP")) || cResources::GetForceCacheLoadingAndSkipSaving())
				{
					bLoadedFromFile = true;
					pContainer->LoadFromFile(sAiFileName);
				}
			}

			if(bLoadedFromFile==false)
			{
				Log("Rebuilding node connections and saving to '%s'\n",cString::To8Char(sAiFileName).c_str());

				//Compile
				pContainer->Compile();

				//Save to disk
				if(cResources::GetForceCacheLoadingAndSkipSaving()==false)
				{
					pContainer->SaveToFile(sAiCacheFile);
				}
			}
		}

		//unsigned long lTime = mpSystem->GetLowLevel()->GetTime() - lStartTime;
		//Log("Creating ai nodes took: %d\n",lTime);


		return pContainer;
	}

	//-----------------------------------------------------------------------

	cAStarHandler* cWorld::CreateAStarHandler(cAINodeContainer* apContainer)
	{
		cAStarHandler *pAStar = hplNew( cAStarHandler, (apContainer) );

		OhplH13Push(mpW->mpH13Reg, kOhplH13AStar, pAStar);

		return pAStar;
	}

	void cWorld::DestroyAStarHandler(cAStarHandler* apHandler)
	{
		OhplH13Destroy(mpW->mpH13Reg, kOhplH13AStar, apHandler);
	}

	//-----------------------------------------------------------------------

	void cWorld::AddAINode(const tString &asName, int alID, const tString &asType, const cVector3f &avPosition)
	{
		cTempNodeContainer *pContainer = NULL;
		tTempNodeContainerMapIt it = m_mapTempNodes.find(asType);
		if(it != m_mapTempNodes.end()){
			pContainer = it->second;
		}

		if(pContainer==NULL){
			pContainer = hplNew( cTempNodeContainer, () );
			m_mapTempNodes.insert(tTempNodeContainerMap::value_type(asType,pContainer));
		}

		pContainer->mlstNodes.push_back(cTempAiNode(avPosition,asName, alID));
	}

	//-----------------------------------------------------------------------

	tTempAiNodeList* cWorld::GetAINodeList(const tString &asType)
	{
		cTempNodeContainer *pContainer = NULL;
		tTempNodeContainerMapIt it = m_mapTempNodes.find(asType);
		if(it != m_mapTempNodes.end()){
			pContainer = it->second;
		}

		if(pContainer==NULL){
			pContainer = hplNew( cTempNodeContainer, () );
			m_mapTempNodes.insert(tTempNodeContainerMap::value_type(asType,pContainer));
		}

		return &pContainer->mlstNodes;
	}

	//-----------------------------------------------------------------------

	// SAVE OBJECT STUFF
	//////////////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------

	kSerializeRustClass(cAreaEntity)

	//-------------------------------------------------------------------

	kSerializeRustClass(cStartPosEntity)


	//-----------------------------------------------------------------------


}
