/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <cstdio>
#include <cstring>
#include "scene/Terrain.h"
#include "scene/World.h"

#include "impl/tinyXML/tinyxml.h"

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
#include "impl/RustBackends.h"

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

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cWorld::cWorld(tString asName,cGraphics *apGraphics,cResources *apResources,cSound* apSound,
						cPhysics *apPhysics, cScene *apScene,cSystem *apSystem, cAI *apAI)
	{
		mpGraphics = apGraphics;
		mpResources = apResources;
		mpSound = apSound;
		mpPhysics = apPhysics;
		mpScene = apScene;
		mpSystem =apSystem;
		mpAI = apAI;
		mpH13Reg = OhplBackendRustRequested() ? ohpl_h13_reg_new() : NULL;
		if(mpH13Reg)
		{
			OhplH13Foreign<iLight>(mpH13Reg, kOhplH13Light);
			OhplH13Foreign<cMeshEntity>(mpH13Reg, kOhplH13DynMesh);
			OhplH13Foreign<cMeshEntity>(mpH13Reg, kOhplH13StaticMesh);
			OhplH13Foreign<cBillboard>(mpH13Reg, kOhplH13Billboard);
			OhplH13Foreign<cLensFlare>(mpH13Reg, kOhplH13LensFlare);
			OhplH13Foreign<cBeam>(mpH13Reg, kOhplH13Beam);
			OhplH13Foreign<cParticleSystem>(mpH13Reg, kOhplH13ParticleSystem);
			OhplH13Foreign<cGuiSetEntity>(mpH13Reg, kOhplH13GuiSet);
			OhplH13Foreign<cRopeEntity>(mpH13Reg, kOhplH13Rope);
			OhplH13Foreign<cSoundEntity>(mpH13Reg, kOhplH13Sound);
			OhplH13Foreign<cStartPosEntity>(mpH13Reg, kOhplH13StartPos);
			OhplH13Foreign<cFogArea>(mpH13Reg, kOhplH13FogArea);
			OhplH13Foreign<cForceField>(mpH13Reg, kOhplH13ForceField);
			OhplH13Foreign<cDummyRenderable>(mpH13Reg, kOhplH13Dummy);
			OhplH13Foreign<cAINodeContainer>(mpH13Reg, kOhplH13AiContainer);
			OhplH13Foreign<cAStarHandler>(mpH13Reg, kOhplH13AStar);
			OhplH13Foreign<cEnvironmentParticles>(mpH13Reg, kOhplH13EnvParticles);
			OhplH13Foreign<cAreaEntity>(mpH13Reg, kOhplH13Area);
		}

		mpRootNode = hplNew( cNode3D, () );

		msName=asName;

		mbActive = true;

		mAmbientColor=cColor(0,0);

		mbIsSoundEmitter = false;

		mlSoundCreationIDCount =0;

		//TODO: Have the container type as param and create.
		mpRenderableContainer[eWorldContainerType_Static] = hplNew( cRenderableContainer_BoxTree, () );
		mpRenderableContainer[eWorldContainerType_Dynamic] = hplNew( cRenderableContainer_DynBoxTree, () );

		mpPhysicsWorld = NULL;
		mbAutoDeletePhysicsWorld = false;

		//////////////////////////////
		//Sky box
		mpSkyBoxVtxBuffer = mpGraphics->GetMeshCreator()->CreateSkyBoxVertexBuffer(1);
		mpSkyBoxTexture = NULL;
		mbAutoDestroySkybox = false;
		mbSkyBoxActive = false;
		mSkyBoxColor = cColor(1,1);

		//////////////////////////////
		//Fog
		mbFogActive = false;
		mfFogStart =0;
		mfFogEnd = 10;
		mfFogFalloffExp = 1;
		mFogColor = cColor(1,1);
		mbFogCulling = true;

		mfToneMappingKey = 0.5f;
		mfToneMappingExposure = 0;
		mfToneMappingWhiteCut = 3.5f;


		msFilePath = _W("");
	}

	//-----------------------------------------------------------------------

	cLightDirectional* cWorld::GetDirectionalLight()
	{
		if(mpDirectionalLight==NULL) mpDirectionalLight = hplNew(cLightDirectional, ("WorldDirectional", mpResources));
		return mpDirectionalLight;
	}

	//-----------------------------------------------------------------------

	cEnvironmentParticles* cWorld::CreateEnvironmentParticles(const tString& asName)
	{
		cEnvironmentParticles *pEnv = hplNew(cEnvironmentParticles, (asName, this, mpGraphics, mpResources));
		OhplH13Push(mpH13Reg, kOhplH13EnvParticles, mvEnvParticles, pEnv);
		return pEnv;
	}

	cEnvironmentParticles* cWorld::GetEnvironmentParticles(const tString& asName)
	{
		return OhplH13Name(mpH13Reg, kOhplH13EnvParticles, mvEnvParticles, asName);
	}

	//-----------------------------------------------------------------------

	cWorld::~cWorld()
	{
		if(mpDirectionalLight) hplDelete(mpDirectionalLight);
		if(mpTerrain) hplDelete(mpTerrain);
		OhplH13DeleteAll(mpH13Reg, kOhplH13EnvParticles, mvEnvParticles);
		if(mpSkyBoxVtxBuffer) hplDelete(mpSkyBoxVtxBuffer);
		if(mpSkyBoxTexture && mbAutoDestroySkybox)
		{
			mpResources->GetTextureManager()->Destroy(mpSkyBoxTexture);
		}

		for(tEntFileListIt it = mlstEntFileCache.begin(); it != mlstEntFileCache.end(); ++it)
		{
			mpResources->GetEntFileManager()->Destroy(*it);
		}

		DestroyAllEntities(0);
		
		for(int i=0; i<2; ++i)
		{
			if(mpRenderableContainer[i]) hplDelete(mpRenderableContainer[i]);
		}

		hplDelete(mpRootNode);

		if(mpH13Reg) ohpl_h13_reg_free(mpH13Reg);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWorld::DestroyAllEntities(tWorldDestroyAllFlag aFlags)
	{
		const uint32_t lPlan = ohpl_world_destroy_plan(aFlags);
		if(lPlan & kOhplDestroyStatic)
			OhplH13DeleteAll(mpH13Reg, kOhplH13StaticMesh, mlstStaticMeshEntities);
		
		OhplH13DeleteAll(mpH13Reg, kOhplH13DynMesh, mlstDynamicMeshEntities);
		OhplH13DeleteAll(mpH13Reg, kOhplH13Light, mlstLights);
		OhplH13DeleteAll(mpH13Reg, kOhplH13Billboard, mlstBillboards);
		OhplH13DeleteAll(mpH13Reg, kOhplH13LensFlare, mlstLensFlares);
		OhplH13DeleteAll(mpH13Reg, kOhplH13Beam, mlstBeams);
		OhplH13DeleteAll(mpH13Reg, kOhplH13ParticleSystem, mlstParticleSystems);
		OhplH13DeleteAll(mpH13Reg, kOhplH13GuiSet, mlstGuiSetEntities);
		OhplH13DeleteAll(mpH13Reg, kOhplH13Rope, mlstRopeEntities);
		OhplH13DeleteAll(mpH13Reg, kOhplH13FogArea, mlstFogAreas);
		OhplH13DeleteAll(mpH13Reg, kOhplH13ForceField, mlstForceFields);
		OhplH13DeleteAll(mpH13Reg, kOhplH13StartPos, mlstStartPosEntities);
		if(mpH13Reg) ohpl_h13_area_destroy_all(mpH13Reg);
		else STLMapDeleteAll(m_mapAreaEntities);


		OhplH13DeleteAll(mpH13Reg, kOhplH13AiContainer, mlstAINodeContainers);
		OhplH13DeleteAll(mpH13Reg, kOhplH13AStar, mlstAStarHandlers);
		STLMapDeleteAll(m_mapTempNodes);

		if(lPlan & kOhplDestroyPhysics)
		{
			if(mpPhysicsWorld && mbAutoDeletePhysicsWorld)
				mpPhysics->DestroyWorld(mpPhysicsWorld);
		}

		//So that bodies can stop sound entities on destruction.
		OhplH13DeleteAll(mpH13Reg, kOhplH13Sound, mlstSoundEntities);

	}

	//-----------------------------------------------------------------------

	namespace {

	uint32_t H13Bits(float afVal)
	{
		uint32_t lBits;
		memcpy(&lBits, &afVal, 4);
		return lBits;
	}

	void H13Mtx(FILE *f, const cMatrixf &aMtx)
	{
		for(int i=0; i<16; ++i)
		{
			uint32_t lBits;
			memcpy(&lBits, &aMtx.v[i], 4);
			fprintf(f, " %08x", lBits);
		}
	}

	void H13DumpArea(FILE *f, const tString &asName, cAreaEntity *apArea)
	{
		fprintf(f, "area %s %s", asName.c_str(), apArea->msType.c_str());
		H13Mtx(f, apArea->m_mtxTransform);
		fprintf(f, " %08x %08x %08x\n", H13Bits(apArea->mvSize.x), H13Bits(apArea->mvSize.y), H13Bits(apArea->mvSize.z));
	}

	template<class CONT>
	void H13DumpList(FILE *f, const char *asKind, OhplH13Reg *apReg, uint32_t alKind, CONT &aCont)
	{
		fprintf(f, "list %s %d\n", asKind, (int)OhplH13Size(apReg, alKind, aCont));
		int lIdx = 0;
		for(OhplH13Walk<CONT> it(apReg, alKind, aCont); it.Has(); it.Advance())
		{
			auto *pItem = it.Get();
			fprintf(f, "%s %d %s %d", asKind, lIdx++, pItem->GetName().c_str(), pItem->GetUniqueID());
			H13Mtx(f, pItem->GetWorldMatrix());
			fprintf(f, "\n");
		}
	}

	}

	void cWorld::DumpH13(const char *asPath, const char *asMap)
	{
		FILE *f = fopen(asPath, "a");
		if(f == NULL) return;

		fprintf(f, "map %s\nworld %s\n", asMap, msName.c_str());
		fprintf(f, "fog %d %d %08x\n", mbFogActive, mbFogCulling, H13Bits(mfFogStart));
		fprintf(f, "sky %d\n", mbSkyBoxActive);
		H13DumpList(f, "light", mpH13Reg, kOhplH13Light, mlstLights);
		H13DumpList(f, "meshdyn", mpH13Reg, kOhplH13DynMesh, mlstDynamicMeshEntities);
		H13DumpList(f, "meshstatic", mpH13Reg, kOhplH13StaticMesh, mlstStaticMeshEntities);
		H13DumpList(f, "billboard", mpH13Reg, kOhplH13Billboard, mlstBillboards);
		H13DumpList(f, "lensflare", mpH13Reg, kOhplH13LensFlare, mlstLensFlares);
		H13DumpList(f, "beam", mpH13Reg, kOhplH13Beam, mlstBeams);
		H13DumpList(f, "ps", mpH13Reg, kOhplH13ParticleSystem, mlstParticleSystems);
		H13DumpList(f, "guiset", mpH13Reg, kOhplH13GuiSet, mlstGuiSetEntities);
		H13DumpList(f, "rope", mpH13Reg, kOhplH13Rope, mlstRopeEntities);
		H13DumpList(f, "sound", mpH13Reg, kOhplH13Sound, mlstSoundEntities);
		H13DumpList(f, "fogarea", mpH13Reg, kOhplH13FogArea, mlstFogAreas);
		H13DumpList(f, "forcefield", mpH13Reg, kOhplH13ForceField, mlstForceFields);
		H13DumpList(f, "dummy", mpH13Reg, kOhplH13Dummy, mlstDummyRenderables);

		fprintf(f, "list startpos %d\n", (int)OhplH13Size(mpH13Reg, kOhplH13StartPos, mlstStartPosEntities));
		for(OhplH13Walk<tStartPosEntityList> it(mpH13Reg, kOhplH13StartPos, mlstStartPosEntities); it.Has(); it.Advance())
		{
			cStartPosEntity *pStart = it.Get();
			fprintf(f, "startpos %s %d", pStart->msName.c_str(), pStart->mbCrouching);
			H13Mtx(f, pStart->m_mtxTransform);
			fprintf(f, "\n");
		}

		if(mpH13Reg)
		{
			fprintf(f, "list area %d\n", (int)ohpl_h13_area_len(mpH13Reg));
			for(size_t i=0; i<ohpl_h13_area_len(mpH13Reg); ++i)
			{
				OhplName key;
				cAreaEntity *pArea = static_cast<cAreaEntity*>(ohpl_h13_area_at(mpH13Reg, i, &key));
				H13DumpArea(f, tString(key.ptr, key.len), pArea);
			}
		}
		else
		{
			fprintf(f, "list area %d\n", (int)m_mapAreaEntities.size());
			for(tAreaEntityMapIt it = m_mapAreaEntities.begin(); it != m_mapAreaEntities.end(); ++it)
				H13DumpArea(f, it->first, it->second);
		}

		fprintf(f, "list ainode %d\nlist astar %d\nlist envparticles %d\n",
				(int)OhplH13Size(mpH13Reg, kOhplH13AiContainer, mlstAINodeContainers),
				(int)OhplH13Size(mpH13Reg, kOhplH13AStar, mlstAStarHandlers),
				(int)OhplH13Size(mpH13Reg, kOhplH13EnvParticles, mvEnvParticles));
		fclose(f);
	}

	//-----------------------------------------------------------------------

	void cWorld::Update(float afTimeStep)
	{
		START_TIMING(Physics);
		if(mpPhysicsWorld) mpPhysicsWorld->Update(afTimeStep);
		STOP_TIMING(Physics);


		START_TIMING(Entities);
		UpdateEntities(afTimeStep);
		STOP_TIMING(Entities);
		
		START_TIMING(Particles);
		UpdateParticles(afTimeStep);
		STOP_TIMING(Particles);

		for(OhplH13Walk<tLensFlareList> it(mpH13Reg, kOhplH13LensFlare, mlstLensFlares); it.Has(); it.Advance()) it.Get()->UpdateLogic(afTimeStep);

		for(OhplH13Walk<tForceFieldList> it(mpH13Reg, kOhplH13ForceField, mlstForceFields); it.Has();)
		{
			cForceField *pField = it.Get();
			if(pField->IsActive()) pField->UpdateLogic(afTimeStep);
			if(pField->GetAutoRemove() && pField->IsDead())
			{
				hplDelete(pField);
				it.Erase();
			}
			else it.Advance();
		}

		START_TIMING(Lights);
		UpdateLights(afTimeStep);
		STOP_TIMING(Lights);

		START_TIMING(SoundEntities);
		UpdateSoundEntities(afTimeStep);
		STOP_TIMING(SoundEntities);

		if(mpH13Reg)
		{
			for(OhplH13Walk<std::vector<cEnvironmentParticles*> > it(mpH13Reg, kOhplH13EnvParticles, mvEnvParticles); it.Has(); it.Advance()) it.Get()->Update(afTimeStep);
		}
		else
		{
			for(size_t i=0; i<mvEnvParticles.size(); ++i) mvEnvParticles[i]->Update(afTimeStep);
		}
	}

	//-----------------------------------------------------------------------

	void cWorld::PreUpdate(float afTotalTime, float afTimeStep)
	{
		mpSound->GetSoundHandler()->SetSilent(true);

		for(uint32_t lStep = ohpl_world_pre_update_steps(afTotalTime, afTimeStep); lStep > 0; --lStep)
		{
			if(mpPhysicsWorld) mpPhysicsWorld->Update(afTimeStep);
			UpdateParticles(afTimeStep);
		}

		mpSound->GetSoundHandler()->SetSilent(false);
	}

	//-----------------------------------------------------------------------

	iRenderableContainer* cWorld::GetRenderableContainer(eWorldContainerType aType)
	{
		return mpRenderableContainer[aType];
	}

	//-----------------------------------------------------------------------

	void cWorld::SetPhysicsWorld(iPhysicsWorld *apWorld, bool abAutoDelete)
	{
		mpPhysicsWorld = apWorld;
		mbAutoDeletePhysicsWorld = abAutoDelete;
		if(mpPhysicsWorld)
			mpPhysicsWorld->SetWorld(this);
	}

	iPhysicsWorld* cWorld::GetPhysicsWorld()
	{
		return mpPhysicsWorld;
	}

	//-----------------------------------------------------------------------

	static void CheckMinMaxUpdate(cVector3f &avMin,cVector3f &avMax,
							const cVector3f &avLocalMin,const cVector3f &avLocalMax)
	{
		if(avMin.x > avLocalMin.x) avMin.x = avLocalMin.x;
		if(avMax.x < avLocalMax.x) avMax.x = avLocalMax.x;

		if(avMin.y > avLocalMin.y) avMin.y = avLocalMin.y;
		if(avMax.y < avLocalMax.y) avMax.y = avLocalMax.y;

		if(avMin.z > avLocalMin.z) avMin.z = avLocalMin.z;
		if(avMax.z < avLocalMax.z) avMax.z = avLocalMax.z;
	}

	//-----------------------------------------------------------------------

	void cWorld::Compile(bool abCalcPhysicsWorldSize)
	{
		for(int i=0; i<2; ++i)
			if(mpRenderableContainer[i]) mpRenderableContainer[i]->Compile();

		if(mpPhysicsWorld && abCalcPhysicsWorldSize)
		{
			iRenderableContainerNode *pStaticRoot = mpRenderableContainer[eWorldContainerType_Static]->GetRoot();

			//Create a 10 m border around the world too
			cVector3f vMin, vMax;
			ohpl_world_physics_bounds(&pStaticRoot->GetMin(), &pStaticRoot->GetMax(), &vMin, &vMax);

			mpPhysicsWorld->SetWorldSize(vMin, vMax);
		}
	}

	//-----------------------------------------------------------------------

	void cWorld::SetSkyBox(iTexture *apTexture, bool abAutoDestroy)
	{
		if(mpSkyBoxTexture && mbAutoDestroySkybox)
		{
			mpResources->GetTextureManager()->Destroy(mpSkyBoxTexture);
		}

		mbAutoDestroySkybox = abAutoDestroy;
		mpSkyBoxTexture = apTexture;
		if(mpSkyBoxTexture)
		{
			mpSkyBoxTexture->SetWrapS(eTextureWrap_ClampToEdge);
			mpSkyBoxTexture->SetWrapT(eTextureWrap_ClampToEdge);
		}
	}

	void cWorld::SetSkyBoxActive(bool abX)
	{
		mbSkyBoxActive = abX;
	}

	bool cWorld::mbHpl3SkyBox = false;

	void cWorld::SetSkyBoxColor(const cColor& aColor)
	{
		if(mSkyBoxColor == aColor) return;
		mSkyBoxColor = aColor;
		UpdateSkyBoxVBColor();
	}

	void cWorld::SetSkyBoxBrightness(float afX)
	{
		if(mfSkyBoxBrightness == afX) return;
		mfSkyBoxBrightness = afX;
		UpdateSkyBoxVBColor();
	}

	void cWorld::UpdateSkyBoxVBColor()
	{
		cColor col;
		ohpl_world_skybox_color(&mSkyBoxColor, mbHpl3SkyBox, mfSkyBoxBrightness, &col);

		float *pColors = mpSkyBoxVtxBuffer->GetFloatArray(eVertexBufferElement_Color0);
		int lNum = mpSkyBoxVtxBuffer->GetVertexNum();
		for(int i=0; i<lNum;++i)
		{
			pColors[0] = col.r;
			pColors[1] = col.g;
			pColors[2] = col.b;
			pColors[3] = col.a;
			pColors+=4;
		}

		mpSkyBoxVtxBuffer->UpdateData(eVertexElementFlag_Color0,false);
	}
	//-----------------------------------------------------------------------

	cAreaEntity* cWorld::CreateAreaEntity(const tString &asName)
	{
		cAreaEntity *pArea = hplNew( cAreaEntity, () );
		pArea->msName = asName;
		if(mpH13Reg) ohpl_h13_area_insert(mpH13Reg, OhplName{asName.data(), asName.size()}, pArea);
		else m_mapAreaEntities.insert(tAreaEntityMap::value_type(asName, pArea));
		return pArea;
	}

	cAreaEntity* cWorld::GetAreaEntity(const tString &asName)
	{
		if(mpH13Reg) return static_cast<cAreaEntity*>(ohpl_h13_area_get(mpH13Reg, OhplName{asName.data(), asName.size()}));

		tAreaEntityMapIt it = m_mapAreaEntities.find(asName);
		if(it== m_mapAreaEntities.end()) return NULL;

		return it->second;
	}

	//-----------------------------------------------------------------------

	iEntity3D* cWorld::CreateEntity(const tString& asName, const cMatrixf &a_mtxTransform,const tString& asFile, int alID, bool abActive,
									const cVector3f &avScale,
									cResourceVarsObject *apInstanceVars, bool abSkipNonStaticEntity)
	{
		iEntity3D *pEntity = NULL;
		
		cEntFile *pEntFile = mpResources->GetEntFileManager()->CreateEntFile(asFile);
		if(pEntFile==NULL) return NULL;

		mlstEntFileCache.push_back(pEntFile);
		
		tString sEntityType = "";
		iXmlDocument *pDoc = pEntFile->GetXmlDoc();

		//////////////////////////////////
		// Get Root element
		cXmlElement *pVarRootElem = pDoc->GetFirstElement("UserDefinedVariables");
		if(pVarRootElem==NULL){
			Warning("Can not find a UserDefinedVariables element in '%s'. Using default entity type\n", asFile.c_str());
		}
		else
		{
			sEntityType = pVarRootElem->GetAttributeString("EntityType");
		}

		
		//////////////////////////////////
		// Get Loader and load data
		iEntityLoader *pLoader = mpResources->GetEntityLoader(sEntityType);
		if(pLoader)
		{
			if(abSkipNonStaticEntity==false || pLoader->GetCreatesStaticEntity())
			{
				pEntity = pLoader->Load(asName,alID, abActive, pDoc,a_mtxTransform, avScale, this,pEntFile->GetName(),pEntFile->GetFullPath(), apInstanceVars);
				if(pEntity) pEntity->SetSourceFile(pEntFile->GetName());
			}
		}
		else
		{
			Error("Couldn't find loader for type '%s' in file '%s'\n",sEntityType.c_str(),pEntFile->GetName().c_str());
		}
		
		return pEntity;
	}

	//-----------------------------------------------------------------------

	cMeshEntity* cWorld::CreateMeshEntity(const tString &asName,cMesh *apMesh, bool abStatic)
	{
		if(apMesh==NULL) return NULL;
		cMeshEntity* pMeshEntity = hplNew( cMeshEntity, (asName,apMesh,mpResources->GetMaterialManager(),
														mpResources->GetMeshManager(), 
														mpResources->GetAnimationManager()) );

		//////////////////////////////
		//Put in entity list
		if(abStatic)
		{
			OhplH13Push(mpH13Reg, kOhplH13StaticMesh, mlstStaticMeshEntities, pMeshEntity);
		}
		else
		{
			OhplH13Push(mpH13Reg, kOhplH13DynMesh, mlstDynamicMeshEntities, pMeshEntity);
		}


		//////////////////////////////
		// Add submeshes to renderable container
		pMeshEntity->SetStatic(abStatic);
		for(int i=0; i<pMeshEntity->GetSubMeshEntityNum(); ++i) 
		{
			cSubMeshEntity *pSubEntity = pMeshEntity->GetSubMeshEntity(i);
			if(pSubEntity->GetSubMesh()->IsCollideShape()) continue; //Collide shapes are never rendered!
            
			AddRenderableToContainer(pSubEntity);
		}
		
		pMeshEntity->SetWorld(this);

		return pMeshEntity;
	}

	//-----------------------------------------------------------------------

	void cWorld::DestroyMeshEntity(cMeshEntity* apMesh)
	{
		if(apMesh==NULL) return;

		for(int i=0; i<apMesh->GetSubMeshEntityNum(); ++i) 
		{
			RemoveRenderableFromContainer(apMesh->GetSubMeshEntity(i));
		}

		if(apMesh->IsStatic())
		{
			OhplH13Destroy(mpH13Reg, kOhplH13StaticMesh, mlstStaticMeshEntities, apMesh);
		}
		else
		{
			OhplH13Destroy(mpH13Reg, kOhplH13DynMesh, mlstDynamicMeshEntities, apMesh);
		}
	}

	void cWorld::DestroyStaticMeshEntities(const std::set<cMeshEntity*>& aSet)
	{
		std::set<iRenderable*> setSubs;
		for(cMeshEntity* pMesh : aSet)
			for(int i=0; i<pMesh->GetSubMeshEntityNum(); ++i) setSubs.insert(pMesh->GetSubMeshEntity(i));
		mpRenderableContainer[eWorldContainerType_Static]->RemoveSet(setSubs);
		if(mpH13Reg)
		{
			std::vector<size_t> vSet;
			for(cMeshEntity *pMesh : aSet) vSet.push_back((size_t)pMesh);
			ohpl_h13_reg_remove_set(mpH13Reg, kOhplH13StaticMesh, vSet.data(), vSet.size());
		}
		else mlstStaticMeshEntities.remove_if([&](cMeshEntity* pMesh) { return aSet.count(pMesh) != 0; });
		for(cMeshEntity* pMesh : aSet) hplDelete(pMesh);
	}

	//-----------------------------------------------------------------------

	// The compiled static tree never updates object positions
	void cWorld::MakeMeshEntityDynamic(cMeshEntity* apMesh)
	{
		if(apMesh->IsStatic()==false) return;
		for(int i=0; i<apMesh->GetSubMeshEntityNum(); ++i)
			RemoveRenderableFromContainer(apMesh->GetSubMeshEntity(i));
		OhplH13Erase(mpH13Reg, kOhplH13StaticMesh, mlstStaticMeshEntities, apMesh);
		OhplH13Push(mpH13Reg, kOhplH13DynMesh, mlstDynamicMeshEntities, apMesh);
		apMesh->SetStatic(false);
		for(int i=0; i<apMesh->GetSubMeshEntityNum(); ++i)
		{
			cSubMeshEntity *pSubEntity = apMesh->GetSubMeshEntity(i);
			if(pSubEntity->GetSubMesh()->IsCollideShape()==false) AddRenderableToContainer(pSubEntity);
		}
	}

	//-----------------------------------------------------------------------

	void cWorld::MakeRenderableDynamic(iRenderable* apObject)
	{
		if(apObject->IsStatic()==false) return;
		RemoveRenderableFromContainer(apObject);
		apObject->SetStatic(false);
		AddRenderableToContainer(apObject);
	}

	//-----------------------------------------------------------------------

	cMeshEntity* cWorld::GetDynamicMeshEntity(const tString& asName)
	{
		 return OhplH13Name(mpH13Reg, kOhplH13DynMesh, mlstDynamicMeshEntities, asName);
	}

	//-----------------------------------------------------------------------

	cLightListIterator cWorld::GetLightIterator()
	{
		if(mpH13Reg) return cLightListIterator(mpH13Reg, kOhplH13Light);
		return cLightListIterator(&mlstLights);
	}

	cParticleSystemIterator cWorld::GetParticleSystemIterator()
	{
		if(mpH13Reg) return cParticleSystemIterator(mpH13Reg, kOhplH13ParticleSystem);
		return cParticleSystemIterator(&mlstParticleSystems);
	}

	cForceFieldIterator cWorld::GetForceFieldIterator()
	{
		if(mpH13Reg) return cForceFieldIterator(mpH13Reg, kOhplH13ForceField);
		return cForceFieldIterator(&mlstForceFields);
	}

	cSoundEntityIterator cWorld::GetSoundEntityIterator()
	{
		if(mpH13Reg) return cSoundEntityIterator(mpH13Reg, kOhplH13Sound);
		return cSoundEntityIterator(&mlstSoundEntities);
	}

	cMeshEntityIterator cWorld::GetDynamicMeshEntityIterator()
	{
		if(mpH13Reg) return cMeshEntityIterator(mpH13Reg, kOhplH13DynMesh);
		return cMeshEntityIterator(&mlstDynamicMeshEntities);
	}

	cMeshEntityIterator cWorld::GetStaticMeshEntityIterator()
	{
		if(mpH13Reg) return cMeshEntityIterator(mpH13Reg, kOhplH13StaticMesh);
		return cMeshEntityIterator(&mlstStaticMeshEntities);
	}
	
	//-----------------------------------------------------------------------

	void cWorld::DrawMeshBoundingBoxes(const cColor &aColor, bool abStatic)
	{
		for(OhplH13Walk<tMeshEntityList> It(mpH13Reg, kOhplH13DynMesh, mlstDynamicMeshEntities); It.Has(); It.Advance())
		{
			cMeshEntity *pEntity = It.Get();

			if(abStatic==false && pEntity->IsStatic()) continue;

			cBoundingVolume *pBV = pEntity->GetBoundingVolume();
			mpGraphics->GetLowLevel()->DrawBoxMinMax(pBV->GetMin(), pBV->GetMax(), aColor);
		}
	}

	//-----------------------------------------------------------------------


	cLightPoint* cWorld::CreateLightPoint(const tString &asName,const tString &asGobo,bool abStatic)
	{
		cLightPoint* pLight = hplNew( cLightPoint, (asName,mpResources) );
		OhplH13Push(mpH13Reg, kOhplH13Light, mlstLights, pLight);

		if(asGobo != "")
		{
			mpResources->GetTextureManager()->SetSRGB(true);
			iTexture *pTexture = mpResources->GetTextureManager()->CreateCubeMap(asGobo,true);
			mpResources->GetTextureManager()->SetSRGB(false);
			if(pTexture!=NULL)
				pLight->SetGoboTexture(pTexture);
			else
				Warning("Couldn't load gobo texture '%s' for light '%s'",asGobo.c_str(), asName.c_str());
		}

		pLight->SetStatic(abStatic);
		AddRenderableToContainer(pLight);
		
		pLight->SetWorld(this);

		return pLight;
	}

	//-----------------------------------------------------------------------

	cLightSpot* cWorld::CreateLightSpot(const tString &asName, const tString &asGobo,
										bool abStatic)
	{
		cLightSpot* pLight = hplNew( cLightSpot, (asName,mpResources) );
		OhplH13Push(mpH13Reg, kOhplH13Light, mlstLights, pLight);

		if(asGobo != "")
		{
			mpResources->GetTextureManager()->SetSRGB(true);
			iTexture *pTexture = mpResources->GetTextureManager()->Create2D(asGobo,true);
			mpResources->GetTextureManager()->SetSRGB(false);
			if(pTexture!=NULL)
				pLight->SetGoboTexture(pTexture);
			else
				Warning("Couldn't load gobo texture '%s' for light '%s'",asGobo.c_str(), asName.c_str());
		}

		pLight->SetStatic(abStatic);
		AddRenderableToContainer(pLight);
		
		pLight->SetWorld(this);
		
		return pLight;
	}

	//-----------------------------------------------------------------------

	cLightBox* cWorld::CreateLightBox(const tString &asName,bool abStatic)
	{
		cLightBox* pLight = hplNew( cLightBox, (asName,mpResources) );
		OhplH13Push(mpH13Reg, kOhplH13Light, mlstLights, pLight);

		pLight->SetStatic(abStatic);
		AddRenderableToContainer(pLight);
		
		pLight->SetWorld(this);

		return pLight;
	}

	//-----------------------------------------------------------------------

	void cWorld::DestroyLight(iLight* apLight)
	{
		RemoveRenderableFromContainer(apLight);

		OhplH13Destroy(mpH13Reg, kOhplH13Light, mlstLights, apLight);
	}

	//-----------------------------------------------------------------------

	iLight* cWorld::GetLight(const tString& asName)
	{
		return OhplH13Name(mpH13Reg, kOhplH13Light, mlstLights, asName);
	}

	iLight* cWorld::GetLightFromUniqueID(int alID)
	{
		return OhplH13Id(mpH13Reg, kOhplH13Light, mlstLights, alID);
	}

	//-----------------------------------------------------------------------

	cBillboard* cWorld::CreateBillboard(const tString& asName, const cVector2f& avSize,eBillboardType aType,
										const tString& asMaterial, bool abStatic)
	{
		cBillboard* pBillboard = hplNew( cBillboard, (asName, avSize,aType,mpResources,mpGraphics) );
		OhplH13Push(mpH13Reg, kOhplH13Billboard, mlstBillboards, pBillboard);

		if(asMaterial!="")
		{
			cMaterial *pMat = mpResources->GetMaterialManager()->CreateMaterial(asMaterial);
			pBillboard->SetMaterial(pMat);
		}

		pBillboard->SetStatic(abStatic);
		AddRenderableToContainer(pBillboard);
		
		return pBillboard;
	}
	//-----------------------------------------------------------------------

	void cWorld::DestroyBillboard(cBillboard* apObject)
	{
		RemoveRenderableFromContainer(apObject);

		OhplH13Destroy(mpH13Reg, kOhplH13Billboard, mlstBillboards, apObject);
	}

	//-----------------------------------------------------------------------

	cBillboard* cWorld::GetBillboard(const tString& asName)
	{
		return OhplH13Name(mpH13Reg, kOhplH13Billboard, mlstBillboards, asName);
	}

	cBillboard* cWorld::GetBillboardFromUniqueID(int alID)
	{
		return OhplH13Id(mpH13Reg, kOhplH13Billboard, mlstBillboards, alID);
	}

	//-----------------------------------------------------------------------

	cBillboardIterator cWorld::GetBillboardIterator()
	{
		if(mpH13Reg) return cBillboardIterator(mpH13Reg, kOhplH13Billboard);
		return cBillboardIterator(&mlstBillboards);
	}

	//-----------------------------------------------------------------------

	cLensFlare* cWorld::CreateLensFlare(const tString& asName, const cVector2f& avSize, const tString& asMaterial, bool abStatic)
	{
		cLensFlare* pFlare = hplNew( cLensFlare, (asName, mpResources, mpGraphics) );
		OhplH13Push(mpH13Reg, kOhplH13LensFlare, mlstLensFlares, pFlare);
		pFlare->SetFlareSize(eLensFlareType_Halo, avSize);
		pFlare->SetFlareActive(eLensFlareType_Halo, true);
		if(asMaterial!="") pFlare->SetMaterial(eLensFlareType_Halo, mpResources->GetMaterialManager()->CreateMaterial(asMaterial));
		pFlare->SetStatic(abStatic);
		AddRenderableToContainer(pFlare);
		return pFlare;
	}

	void cWorld::DestroyLensFlare(cLensFlare* apObject)
	{
		RemoveRenderableFromContainer(apObject);
		OhplH13Destroy(mpH13Reg, kOhplH13LensFlare, mlstLensFlares, apObject);
	}

	cLensFlare* cWorld::GetLensFlare(const tString& asName)
	{
		return OhplH13Name(mpH13Reg, kOhplH13LensFlare, mlstLensFlares, asName);
	}

	cLensFlare* cWorld::GetLensFlareFromUniqueID(int alID)
	{
		return OhplH13Id(mpH13Reg, kOhplH13LensFlare, mlstLensFlares, alID);
	}

	cLensFlareIterator cWorld::GetLensFlareIterator()
	{
		if(mpH13Reg) return cLensFlareIterator(mpH13Reg, kOhplH13LensFlare);
		return cLensFlareIterator(&mlstLensFlares);
	}

	//-----------------------------------------------------------------------


	cBeam* cWorld::CreateBeam(const tString& asName, bool abStatic)
	{
		cBeam* pBeam = hplNew( cBeam, (asName,mpResources,mpGraphics) );
		OhplH13Push(mpH13Reg, kOhplH13Beam, mlstBeams, pBeam);

		pBeam->SetStatic(abStatic);
		AddRenderableToContainer(pBeam);
			
		
		return pBeam;
	}
	//-----------------------------------------------------------------------

	void cWorld::DestroyBeam(cBeam* apObject)
	{
		RemoveRenderableFromContainer(apObject);

		OhplH13Destroy(mpH13Reg, kOhplH13Beam, mlstBeams, apObject);
	}

	//-----------------------------------------------------------------------

	cBeam* cWorld::GetBeam(const tString& asName)
	{
		return OhplH13Name(mpH13Reg, kOhplH13Beam, mlstBeams, asName);
	}

	cBeam* cWorld::GetBeamFromUniqueID(int alID)
	{
		return OhplH13Id(mpH13Reg, kOhplH13Beam, mlstBeams, alID);
	}

	//-----------------------------------------------------------------------

	cBeamIterator cWorld::GetBeamIterator()
	{
		if(mpH13Reg) return cBeamIterator(mpH13Reg, kOhplH13Beam);
		return cBeamIterator(&mlstBeams);
	}


	//-----------------------------------------------------------------------

	cParticleSystem* cWorld::CreateParticleSystem(const tString& asName,const tString& asType,const cVector3f& avSize, bool abRemoveWhenDead)
	{
		cParticleSystem* pPS = mpResources->GetParticleManager()->CreatePS(asName,asType, avSize);
		if(pPS == NULL){
			Error("Couldn't create particle system '%s' of type '%s'\n",asName.c_str(), asType.c_str());
			return NULL;
		}

		//Log("Created particle system '%s' of type '%s'\n",asName.c_str(), asType.c_str());
		if(false)//asName== "candlestick02_1_ParticleSystem_1")
		{
			for(int i=0; i< pPS->GetEmitterNum();++i)
			{
				iParticleEmitter *pPE = pPS->GetEmitter(i);
				pPE->SetRenderFlagBit(eRenderableFlag_ContainerDebug, true);
			}
		}

		pPS->SetRemoveWhenDead(abRemoveWhenDead);

		//Add the emitters contained in the system.
		//Do not add the system itself.
		for(int i=0; i< pPS->GetEmitterNum();++i)
		{
			iParticleEmitter *pPE = pPS->GetEmitter(i);

			AddRenderableToContainer(pPE);

			pPE->SetWorld(this);
		}

		OhplH13Push(mpH13Reg, kOhplH13ParticleSystem, mlstParticleSystems, pPS);

		//Log("Created particle system '%s'\n",asType.c_str());

		return pPS;
	}

	//-----------------------------------------------------------------------

	cParticleSystem* cWorld::CreateParticleSystem(const tString& asName, const tString& asDataName, cXmlElement* apElement, const cVector3f& avSize)
	{
		cParticleSystem* pPS = mpResources->GetParticleManager()->CreatePS(asName,asDataName, apElement, avSize);
		if(pPS == NULL){
			Error("Couldn't create particle system '%s' of type '%s'\n",asName.c_str(), asDataName.c_str());
			return NULL;
		}

		//Log("Created particle system '%s' of type '%s'\n",asName.c_str(), asType.c_str());

		//Add the emitters contained in the system.
		//Do not add the system itself.
		for(int i=0; i< pPS->GetEmitterNum();++i)
		{
			iParticleEmitter *pPE = pPS->GetEmitter(i);

			AddRenderableToContainer(pPE);

			pPE->SetWorld(this);
		}

		OhplH13Push(mpH13Reg, kOhplH13ParticleSystem, mlstParticleSystems, pPS);

		//Log("Created particle system '%s'\n",asType.c_str());

		return pPS;
	}

	//-----------------------------------------------------------------------

	void cWorld::DestroyParticleSystem(cParticleSystem* apPS)
	{
		if(apPS==NULL)return;

		for(int i=0; i< apPS->GetEmitterNum();++i)
		{
			iParticleEmitter *pPE = apPS->GetEmitter(i);

			RemoveRenderableFromContainer(pPE);
		}

		OhplH13Destroy(mpH13Reg, kOhplH13ParticleSystem, mlstParticleSystems, apPS);
	}

	//-----------------------------------------------------------------------

	cParticleSystem* cWorld::GetParticleSystem(const tString& asName)
	{
		return OhplH13Name(mpH13Reg, kOhplH13ParticleSystem, mlstParticleSystems, asName);
	}

	cParticleSystem* cWorld::GetParticleSystemFromUniqueID(int alID)
	{
		return OhplH13Id(mpH13Reg, kOhplH13ParticleSystem, mlstParticleSystems, alID);
	}

	//-----------------------------------------------------------------------

	bool cWorld::ParticleSystemExists(cParticleSystem* apPS)
	{
		if(mpH13Reg) return ohpl_h13_reg_index(mpH13Reg, kOhplH13ParticleSystem, apPS) >= 0;

		tParticleSystemListIt it = mlstParticleSystems.begin();
		for(; it != mlstParticleSystems.end(); ++it)
		{
			if(apPS == *it) return true;
		}
		return false;
	}

	void cWorld::DestroyAllParticleSystems()
	{
		for(OhplH13Walk<tParticleSystemList> it(mpH13Reg, kOhplH13ParticleSystem, mlstParticleSystems); it.Has(); it.Advance())
		{
			cParticleSystem *pPS = it.Get();

			for(int i=0; i< pPS->GetEmitterNum();++i)
			{
				iParticleEmitter *pPE = pPS->GetEmitter(i);

				RemoveRenderableFromContainer(pPE);
			}
			hplDelete(pPS);
		}
		OhplH13Clear(mpH13Reg, kOhplH13ParticleSystem, mlstParticleSystems);
	}

	//-----------------------------------------------------------------------

	cGuiSetEntity* cWorld::CreateGuiSetEntity(const tString& asName, cGuiSet *apSet, bool abStatic)
	{
		cGuiSetEntity *pSetEntity = hplNew( cGuiSetEntity, (asName, apSet) );
		OhplH13Push(mpH13Reg, kOhplH13GuiSet, mlstGuiSetEntities, pSetEntity);

		pSetEntity->SetStatic(abStatic);
		AddRenderableToContainer(pSetEntity);
		
		return pSetEntity;
	}
	
	void cWorld::DestroyGuiSetEntity(cGuiSetEntity* apObject)
	{
		//TODO...RemoveRenderableFromContainer(...), etc

		OhplH13Destroy(mpH13Reg, kOhplH13GuiSet, mlstGuiSetEntities, apObject);
	}
	
	cGuiSetEntity* cWorld::GetGuiSetEntity(const tString& asName)
	{
		return OhplH13Name(mpH13Reg, kOhplH13GuiSet, mlstGuiSetEntities, asName);
	}

	cGuiSetEntity* cWorld::GetGuiSetEntityFromUniqueID(int alID)
	{
		return OhplH13Id(mpH13Reg, kOhplH13GuiSet, mlstGuiSetEntities, alID);
	}
	
	cGuiSetEntityIterator cWorld::GetGuiSetEntityIterator()
	{
		if(mpH13Reg) return cGuiSetEntityIterator(mpH13Reg, kOhplH13GuiSet);
		return cGuiSetEntityIterator(&mlstGuiSetEntities);
	}
	
	//-----------------------------------------------------------------------

	cRopeEntity* cWorld::CreateRopeEntity(const tString& asName, iPhysicsRope *apRope, int alMaxSegments)
	{
		cRopeEntity *pRope = hplNew( cRopeEntity, (asName, mpResources, mpGraphics, apRope, alMaxSegments));
		OhplH13Push(mpH13Reg, kOhplH13Rope, mlstRopeEntities, pRope);

		AddRenderableToContainer(pRope);
		
		return pRope;
	}

	void cWorld::DestroyRopeEntity(cRopeEntity* apRope)
	{
		RemoveRenderableFromContainer(apRope);

		OhplH13Destroy(mpH13Reg, kOhplH13Rope, mlstRopeEntities, apRope);
	}

	cRopeEntity* cWorld::GetRopeEntity(const tString& asName)
	{
		return OhplH13Name(mpH13Reg, kOhplH13Rope, mlstRopeEntities, asName);
	}

	cRopeEntity* cWorld::GetRopeEntityFromUniqueID(int alID)
	{
		return OhplH13Id(mpH13Reg, kOhplH13Rope, mlstRopeEntities, alID);
	}

	cRopeEntityIterator cWorld::GetRopeEntityIterator()
	{
		if(mpH13Reg) return cRopeEntityIterator(mpH13Reg, kOhplH13Rope);
		return cRopeEntityIterator(&mlstRopeEntities);
	}

	//-----------------------------------------------------------------------

	cFogArea* cWorld::CreateFogArea(const tString& asName, bool abStatic)
	{
		cFogArea *pFog = hplNew( cFogArea, (asName, mpResources));
		OhplH13Push(mpH13Reg, kOhplH13FogArea, mlstFogAreas, pFog);
		pFog->SetStatic(abStatic);


		AddRenderableToContainer(pFog);

		return pFog;
	}

	void cWorld::DestroyFogArea(cFogArea* apRope)
	{
		RemoveRenderableFromContainer(apRope);

		OhplH13Destroy(mpH13Reg, kOhplH13FogArea, mlstFogAreas, apRope);
	}

	cFogArea* cWorld::GetFogArea(const tString& asName)
	{
		return OhplH13Name(mpH13Reg, kOhplH13FogArea, mlstFogAreas, asName);
	}

	cFogArea* cWorld::GetFogAreaFromUniqueID(int alID)
	{
		return OhplH13Id(mpH13Reg, kOhplH13FogArea, mlstFogAreas, alID);
	}

	cFogAreaIterator cWorld::GetFogAreaIterator()
	{
		if(mpH13Reg) return cFogAreaIterator(mpH13Reg, kOhplH13FogArea);
		return cFogAreaIterator(&mlstFogAreas);
	}

	//-----------------------------------------------------------------------

	cForceField* cWorld::CreateForceField(const tString& asName, bool abAutoRemove, bool abStatic)
	{
		cForceField *pField = hplNew(cForceField, (asName, abAutoRemove));
		pField->SetStatic(abStatic);
		OhplH13Push(mpH13Reg, kOhplH13ForceField, mlstForceFields, pField);
		return pField;
	}

	void cWorld::DestroyForceField(cForceField* apField)
	{
		OhplH13Destroy(mpH13Reg, kOhplH13ForceField, mlstForceFields, apField);
	}

	cForceField* cWorld::GetForceField(const tString& asName)
	{
		return OhplH13Name(mpH13Reg, kOhplH13ForceField, mlstForceFields, asName);
	}

	// Visible fields touching the box, biggest radius first, at most 4
	int cWorld::GetForceFields(const cVector3f& avMin, const cVector3f& avMax, cForceField **apOut)
	{
		std::vector<OhplFieldIn> vFields;
		std::vector<cForceField*> vCandidates;
		for(OhplH13Walk<tForceFieldList> it(mpH13Reg, kOhplH13ForceField, mlstForceFields); it.Has(); it.Advance())
		{
			cForceField *pField = it.Get();
			if(pField->IsActive() == false || pField->IsVisible() == false) continue;
			cBoundingVolume *pBV = pField->GetBoundingVolume();
			OhplFieldIn field;
			field.min = pBV->GetMin();
			field.max = pBV->GetMax();
			field.radius = pField->GetFinalRadius();
			vFields.push_back(field);
			vCandidates.push_back(pField);
		}

		uint32_t vOrder[4];
		const int lNum = ohpl_world_force_fields(vFields.data(), (int32_t)vFields.size(), &avMin, &avMax, vOrder);
		for(int i=0; i<lNum; ++i) apOut[i] = vCandidates[vOrder[i]];
		return lNum;
	}

	//-----------------------------------------------------------------------

	cSoundEntity* cWorld::CreateSoundEntity(const tString &asName,const tString &asSoundEntity,
											bool abRemoveWhenOver)
	{
		cSoundEntityData *pData = mpResources->GetSoundEntityManager()->CreateSoundEntity(asSoundEntity);
		if(pData==NULL){
			Error("Cannot find sound entity '%s'\n",asSoundEntity.c_str());
			return NULL;
		}

		cSoundEntity *pSound = hplNew( cSoundEntity, (asName,pData, mpResources->GetSoundEntityManager(), this, mpSound->GetSoundHandler(),
														abRemoveWhenOver,mlSoundCreationIDCount++));
		/*cSoundEntity *pSound = NULL;
		if(mlstSoundEntityPool.empty())
		{
			pSound = hplNew( cSoundEntity, (asName,pData,
											mpResources->GetSoundEntityManager(),
											this,
											mpSound->GetSoundHandler(),abRemoveWhenOver,mlSoundCreationIDCount++));
		}
		else
		{
			pSound = mlstSoundEntityPool.back();
			pSound->Setup(asName, pData, abRemoveWhenOver);
			mlstSoundEntityPool.pop_back();
		}*/

		OhplH13Push(mpH13Reg, kOhplH13Sound, mlstSoundEntities, pSound);

		return pSound;
	}

	void cWorld::DestroySoundEntity(cSoundEntity* apEntity)
	{
		//STLFindAndDelete(mlstSoundEntities,apEntity);
		
		//Only delete if found! (as it is possible that the sound entity is not longer a valid pointer!
		OhplH13Destroy(mpH13Reg, kOhplH13Sound, mlstSoundEntities, apEntity, true);
	}

	void cWorld::DestroyAllSoundEntities()
	{
		//Make sure no body has any sound entity
		if(mpPhysicsWorld)
		{
			cPhysicsBodyIterator bodyIt = mpPhysicsWorld->GetBodyIterator();
			while(bodyIt.HasNext())
			{
				iPhysicsBody *pBody = bodyIt.Next();
				pBody->SetScrapeSoundEntity(NULL);
				pBody->SetRollSoundEntity(NULL);
			}

			cPhysicsJointIterator jointIt = mpPhysicsWorld->GetJointIterator();
			while(jointIt.HasNext())
			{
				iPhysicsJoint *pJoint = jointIt.Next();
				pJoint->SetSound(NULL);
			}
		}
		
		//Destroy all sound entities
		OhplH13DeleteAll(mpH13Reg, kOhplH13Sound, mlstSoundEntities);
	}


	cSoundEntity* cWorld::GetSoundEntity(const tString& asName)
	{
		return OhplH13Name(mpH13Reg, kOhplH13Sound, mlstSoundEntities, asName);
	}

	cSoundEntity* cWorld::GetSoundEntityFromUniqueID(int alID)
	{
		return OhplH13Id(mpH13Reg, kOhplH13Sound, mlstSoundEntities, alID);
	}

	void cWorld::SetTerrain(cTerrain* apTerrain)
	{
		if(mpTerrain) hplDelete(mpTerrain);
		mpTerrain = apTerrain;
	}

	bool cWorld::IsValid(cSoundEntity* apEntity)
	{
		return OhplH13Index(mpH13Reg, kOhplH13Sound, mlstSoundEntities, apEntity) >= 0;
	}

	bool cWorld::SoundEntityExists(cSoundEntity* apEntity, int alCreationID)
	{
		const int lIdx = OhplH13Index(mpH13Reg, kOhplH13Sound, mlstSoundEntities, apEntity);
		if(lIdx < 0) return false;

		return alCreationID == apEntity->GetCreationID();
	}

	//-----------------------------------------------------------------------

	cStartPosEntity* cWorld::CreateStartPos(const tString &asName)
	{
		cStartPosEntity *pStartPos = hplNew( cStartPosEntity, (asName) );

		OhplH13Push(mpH13Reg, kOhplH13StartPos, mlstStartPosEntities, pStartPos);

		return pStartPos;
	}

	cStartPosEntity* cWorld::GetStartPosEntity(const tString &asName)
	{
		return OhplH13Name(mpH13Reg, kOhplH13StartPos, mlstStartPosEntities, asName);
	}

	cStartPosEntity* cWorld::GetFirstStartPosEntityByName()
	{
		if(mpH13Reg) return static_cast<cStartPosEntity*>(ohpl_h13_reg_first_by_name_f(mpH13Reg, kOhplH13StartPos));

		std::vector<OhplName> vNames;
		for(cStartPosEntity *pStart : mlstStartPosEntities)
		{
			const tString& sName = pStart->GetName();
			vNames.push_back(OhplName{sName.data(), sName.size()});
		}
		const int lIdx = ohpl_world_first_by_name(vNames.data(), (int32_t)vNames.size());
		if(lIdx < 0) return NULL;
		tStartPosEntityListIt it = mlstStartPosEntities.begin();
		std::advance(it, lIdx);
		return *it;
	}

	cStartPosEntity* cWorld::GetFirstStartPosEntity()
	{
		if(mpH13Reg) return static_cast<cStartPosEntity*>(ohpl_h13_reg_get(mpH13Reg, kOhplH13StartPos, 0));
		if(mlstStartPosEntities.empty()) return NULL;

		return mlstStartPosEntities.front();
	}


	//-----------------------------------------------------------------------

	void  cWorld::GenerateAINodes(cAINodeGeneratorParams *apParams)
	{
		mpAI->GetNodeGenerator()->Generate(this, apParams);
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
		pContainer = OhplH13Name(mpH13Reg, kOhplH13AiContainer, mlstAINodeContainers, asName, true);
		

		//////////////////////////////////
		//Get file name
		cFileSearcher *pFileSearcher = mpResources->GetFileSearcher();
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
			OhplH13Push(mpH13Reg, kOhplH13AiContainer, mlstAINodeContainers, pContainer);

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

				if(dateAIFile > dateMapFile || cResources::GetForceCacheLoadingAndSkipSaving())
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

		OhplH13Push(mpH13Reg, kOhplH13AStar, mlstAStarHandlers, pAStar);

		return pAStar;
	}

	void cWorld::DestroyAStarHandler(cAStarHandler* apHandler)
	{
		OhplH13Destroy(mpH13Reg, kOhplH13AStar, mlstAStarHandlers, apHandler);
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

	bool cWorld::CreateFromFile(tString asFile)
	{
		return false;
	}

	//-----------------------------------------------------------------------

	cDummyRenderable* cWorld::CreateDummyRenderable(const tString& asName, bool abStatic)
	{
		cDummyRenderable *pDummy = hplNew( cDummyRenderable, (asName));
		OhplH13Push(mpH13Reg, kOhplH13Dummy, mlstDummyRenderables, pDummy);
		pDummy->SetStatic(abStatic);

		AddRenderableToContainer(pDummy);

		return pDummy;
	}

	void cWorld::DestroyDummyRenderable(cDummyRenderable* apDummy)
	{
		RemoveRenderableFromContainer(apDummy);

		OhplH13Destroy(mpH13Reg, kOhplH13Dummy, mlstDummyRenderables, apDummy);
	}

	cDummyRenderable* cWorld::GetDummyRenderable(const tString& asName)
	{
		return OhplH13Name(mpH13Reg, kOhplH13Dummy, mlstDummyRenderables, asName);
	}

	cDummyRenderable* cWorld::GetDummyRenderableFromUniqueID(int alID)
	{
		return OhplH13Id(mpH13Reg, kOhplH13Dummy, mlstDummyRenderables, alID);
	}

	cDummyRenderableIterator cWorld::GetDummyRenderableIterator()
	{
		if(mpH13Reg) return cDummyRenderableIterator(mpH13Reg, kOhplH13Dummy);
		return cDummyRenderableIterator(&mlstDummyRenderables);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cWorld::AddRenderableToContainer(iRenderable *apObject)
	{
		if(apObject->IsStatic())
			mpRenderableContainer[eWorldContainerType_Static]->Add(apObject);
		else
			mpRenderableContainer[eWorldContainerType_Dynamic]->Add(apObject);
	}
	
	//-----------------------------------------------------------------------

	void cWorld::RemoveRenderableFromContainer(iRenderable *apObject)
	{
		if(apObject->IsStatic())	mpRenderableContainer[eWorldContainerType_Static]->Remove(apObject);
		else						mpRenderableContainer[eWorldContainerType_Dynamic]->Remove(apObject);
	}

	//-----------------------------------------------------------------------

	void cWorld::UpdateParticles(float afTimeStep)
	{
		OhplH13Walk<tParticleSystemList> it(mpH13Reg, kOhplH13ParticleSystem, mlstParticleSystems);

		while(it.Has())
		{
			cParticleSystem *pPS = it.Get();

			pPS->UpdateLogic(afTimeStep);

			//Check if the system is alive, else destroy
			if(pPS->GetRemoveWhenDead() && pPS->IsDead())
			{
				it.Erase();
				for(int i=0; i< pPS->GetEmitterNum();++i)
				{
					RemoveRenderableFromContainer(pPS->GetEmitter(i));
				}
				hplDelete(pPS);
			}
			else
			{
				it.Advance();
			}
		}
	}
	//-----------------------------------------------------------------------


	void cWorld::UpdateEntities(float afTimeStep)
	{
		//static size_t lLastSize = 0;
		//bool bRenderDebug = lLastSize != mlstDynamicMeshEntities.size() && mlstDynamicMeshEntities.size()>=2;
		//if(mlstDynamicMeshEntities.size()>=2) lLastSize = mlstDynamicMeshEntities.size();
        	
		//if(bRenderDebug)Log("----\n");
		for(OhplH13Walk<tMeshEntityList> MeshIt(mpH13Reg, kOhplH13DynMesh, mlstDynamicMeshEntities); MeshIt.Has(); MeshIt.Advance())
		{
			cMeshEntity *pEntity = MeshIt.Get();

			if(pEntity->IsActive()){
				//if(pEntity->IsStatic()==false) START_TIMING_EX(pEntity->GetName().c_str(),entity);
				pEntity->UpdateLogic(afTimeStep);
				//if(bRenderDebug) Log("Enitity '%s'. Pos: (%s), Matrix: (%s)\n", pEntity->GetName().c_str(), pEntity->GetWorldPosition().ToString().c_str(),
																			//pEntity->GetWorldMatrix().ToString().c_str());
				//if(pEntity->IsStatic()==false) STOP_TIMING(entity);
			}
		}
		//if(bRenderDebug)Log("----\n");
	}

	//-----------------------------------------------------------------------

	void cWorld::UpdateLights(float afTimeStep)
	{
		for(OhplH13Walk<tLightList> it(mpH13Reg, kOhplH13Light, mlstLights); it.Has(); it.Advance())
		{
			iLight *pLight = it.Get();

			if(pLight->IsActive()) pLight->UpdateLogic(afTimeStep);
		}
	}

	//-----------------------------------------------------------------------

	void cWorld::UpdateSoundEntities(float afTimeStep)
	{
		OhplH13Walk<tSoundEntityList> it(mpH13Reg, kOhplH13Sound, mlstSoundEntities);

		while(it.Has())
		{
			cSoundEntity *pSound = it.Get();

			if(pSound->IsActive()) pSound->UpdateLogic(afTimeStep);

			//Check if the system is stopped, else destroy
			if(pSound->IsStopped() && pSound->GetRemoveWhenOver())
			{
				it.Erase();
				//mlstSoundEntityPool.push_back(pSound);
				hplDelete(pSound);
			}
			else
			{
				it.Advance();
			}
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// SAVE OBJECT STUFF
	//////////////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------

	kBeginSerializeBase(cAreaEntity)
	kSerializeVar(msName, eSerializeType_String)
	kSerializeVar(msType, eSerializeType_String)
	kSerializeVar(m_mtxTransform, eSerializeType_Matrixf)
	kSerializeVar(mvSize, eSerializeType_Vector3f)
	kEndSerialize()

	//-------------------------------------------------------------------

	kBeginSerializeBase(cStartPosEntity)
	kSerializeVar(msName, eSerializeType_String)
	kSerializeVar(m_mtxTransform, eSerializeType_Matrixf)
	kEndSerialize()

	
	//-----------------------------------------------------------------------


}
