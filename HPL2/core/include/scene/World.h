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

#ifndef HPL_WORLD_H
#define HPL_WORLD_H

#include "system/SystemTypes.h"
#include "graphics/GraphicsTypes.h"
#include "math/MathTypes.h"
#include "engine/EngineTypes.h"
#include "scene/SceneTypes.h"
#include "scene/RustWd.h"
#include <set>
#include <vector>


namespace hpl {

	class cGraphics;
	class cResources;
	class cSound;
	class cPhysics;
	class cScene;
	class cSystem;
	class cAI;

	class iCamera;
	class cCamera;
	class cNode3D;
	class iEntity3D;
	class cLightSpot;
	class cLightPoint;
	class cLightBox;
	class cLightDirectional;
	class iLight;
	class cImageEntity;
	class cParticleManager;
	class cParticleSystem;
	class iScript;
	class cPortalContainer;
	class iRenderableContainer;
	class cMeshEntity;
	class cMesh;
	class cBillboard;
	class cBeam;
	class cGuiSetEntity;
	class iPhysicsWorld;
	class iPhysicsBody;
	class cSoundEntity;
	class cAINodeContainer;
	struct OhplH13Reg;
	class cAStarHandler;
	class iVertexBuffer;
	class iTexture;
	class cGuiSet;
	class cRopeEntity;
	class iPhysicsRope;
	class cResourceVarsObject;
	class cFogArea;
	class cTerrain;
	class cForceField;
	class cEnvironmentParticles;
	class cXmlElement;
	class cEntFile;
	class cDummyRenderable;
	

	//-------------------------------------------------------------------
	
	typedef std::list<cEntFile*> tEntFileList;
	typedef tEntFileList::iterator tEntFileListIt;

	//-------------------------------------------------------------------
	
	class cTempAiNode
	{
	public:
		cTempAiNode(const cVector3f& avPos, const tString& asName, int alID) : mvPos(avPos),msName(asName), mlID(alID) {}
		cVector3f mvPos;
		tString msName;
		int mlID;
	};
	
	typedef std::list<cTempAiNode> tTempAiNodeList;
	typedef std::list<cTempAiNode>::iterator tTempAiNodeListIt;

	class cTempNodeContainer
	{
	public:
		tString msName;
        tTempAiNodeList mlstNodes;		
	};

	typedef std::map<tString,cTempNodeContainer*> tTempNodeContainerMap;
	typedef std::map<tString,cTempNodeContainer*>::iterator tTempNodeContainerMapIt;

	//-------------------------------------------------------------------

	class cAreaEntity :public iSerializable
	{
		kSerializableClassInit(cAreaEntity)
	public:
		virtual ~cAreaEntity(){}

		tString msName;
		tString msType;
		cMatrixf m_mtxTransform;
		cVector3f mvSize;
	};
	
	typedef std::map<tString, cAreaEntity*> tAreaEntityMap;
	typedef tAreaEntityMap::iterator tAreaEntityMapIt;

	//-------------------------------------------------------------------

	class cStartPosEntity : public iSerializable
	{
		kSerializableClassInit(cStartPosEntity)
	public:
		cStartPosEntity() {}
		cStartPosEntity(const tString& asName) : msName(asName){}
		virtual ~cStartPosEntity(){}

		cMatrixf& GetWorldMatrix(){ return m_mtxTransform;}
		cMatrixf& GetLocalMatrix(){ return m_mtxTransform;}
		void SetMatrix(const cMatrixf& a_mtxTrans){ m_mtxTransform = a_mtxTrans;}
		
		tString& GetName(){ return msName;}

		cMatrixf m_mtxTransform;
		tString msName;
		bool mbCrouching = false;
	};
	
	typedef std::list<cStartPosEntity*> tStartPosEntityList;
	typedef std::list<cStartPosEntity*>::iterator tStartPosEntityListIt;

	struct cWorldExposureArea
	{
		tString msName;
		cMatrixf m_mtxInvTransform;
		cVector3f mvHalfSize;
		float mfExposure;
		float mfWhiteCut;
		float mfTransitionTime;
	};

	//-------------------------------------------------------------------

	class cWorld
	{
	public:

		cWorld(tString asName,cGraphics *apGraphics,cResources *apResources,cSound* apSound,
					cPhysics *apPhysics, cScene *apScene,cSystem *apSystem, cAI *apAI)
			: mpW(ohpl_wd_new(this, apGraphics, apResources, apSound, apPhysics, apScene, apSystem, apAI, asName.data(), asName.size())){}
		~cWorld(){ ohpl_wd_free(mpW); }

		OhplWdPod* Rs() const { return mpW;}

		void ClearTempNodes();
		void DestroyAllEntities(tWorldDestroyAllFlag aFlags){ ohpl_wd_destroy_all(mpW, aFlags); }
		void DumpH13(const char *asPath, const char *asMap){ ohpl_wd_dump(mpW, asPath, asMap); }
		tString GetName(){ size_t lLen; const char *psName = ohpl_wd_name(mpW, &lLen); return tString(psName, lLen); }
		bool CreateFromFile(tString asFile){ return false; }
		void Update(float afTimeStep){ ohpl_wd_update(mpW, afTimeStep); }
		void PreUpdate(float afTotalTime, float afTimeStep){ ohpl_wd_pre_update(mpW, afTotalTime, afTimeStep); }
		iRenderableContainer* GetRenderableContainer(eWorldContainerType aType){ return mpW->mpRenderableContainer[aType]; }

		iEntity3D* CreateEntity(const tString& asName, const cMatrixf &a_mtxTransform,
								const tString& asFile, int alID = -1, bool abActive=true,
								const cVector3f &avScale=cVector3f(1),
								cResourceVarsObject *apInstanceVars=NULL,
								bool abSkipNonStaticEntity=false)
		{
			return static_cast<iEntity3D*>(ohpl_wd_create_entity(mpW, asName.data(), asName.size(), &a_mtxTransform, asFile.data(), asFile.size(),
									alID, abActive, &avScale, apInstanceVars, abSkipNonStaticEntity));
		}

		void Compile(bool abCalcPhysicsWorldSize){ ohpl_wd_compile(mpW, abCalcPhysicsWorldSize); }
		void SetPhysicsWorld(iPhysicsWorld *apWorld, bool abAutoDelete=true){ ohpl_wd_set_physics_world(mpW, apWorld, abAutoDelete); }
		iPhysicsWorld* GetPhysicsWorld(){ return mpW->mpPhysicsWorld; }

		void SetSkyBox(iTexture *apTexture, bool abAutoDestroy){ ohpl_wd_set_skybox(mpW, apTexture, abAutoDestroy); }
		void SetSkyBoxActive(bool abX){ mpW->mbSkyBoxActive = abX; }
		void SetSkyBoxColor(const cColor& aColor){ ohpl_wd_set_skybox_color(mpW, &aColor); }
		void SetSkyBoxBrightness(float afX){ ohpl_wd_set_skybox_brightness(mpW, afX); }
		static void SetHpl3SkyBox(bool abX){ ohpl_wd_set_hpl3_skybox(abX); }

		cLightDirectional* GetDirectionalLight(){ return static_cast<cLightDirectional*>(ohpl_wd_dir_light(mpW)); }
		cEnvironmentParticles* CreateEnvironmentParticles(const tString& asName){ return static_cast<cEnvironmentParticles*>(ohpl_wd_create_env_particles(mpW, asName.data(), asName.size())); }
		cEnvironmentParticles* GetEnvironmentParticles(int alIdx){ return static_cast<cEnvironmentParticles*>(ohpl_h13_reg_get(mpW->mpH13Reg, kOhplH13EnvParticles, (size_t)alIdx)); }
		cEnvironmentParticles* GetEnvironmentParticles(const tString& asName){ return OhplH13Name<cEnvironmentParticles*>(mpW->mpH13Reg, kOhplH13EnvParticles, asName); }
		int GetEnvironmentParticleNum(){ return (int)OhplH13Size(mpW->mpH13Reg, kOhplH13EnvParticles); }

		cAreaEntity* CreateAreaEntity(const tString &asName)
		{
			cAreaEntity *pArea = hplNew( cAreaEntity, () );
			pArea->msName = asName;
			ohpl_h13_area_insert(mpW->mpH13Reg, OhplName{asName.data(), asName.size()}, pArea);
			return pArea;
		}
		cAreaEntity* GetAreaEntity(const tString &asName){ return static_cast<cAreaEntity*>(ohpl_h13_area_get(mpW->mpH13Reg, OhplName{asName.data(), asName.size()})); }

		cMeshEntity* CreateMeshEntity(const tString &asName,cMesh *apMesh, bool abStatic=false){ return static_cast<cMeshEntity*>(ohpl_wd_create_mesh(mpW, asName.data(), asName.size(), apMesh, abStatic)); }
		void DestroyMeshEntity(cMeshEntity* apMesh){ ohpl_wd_destroy_mesh(mpW, apMesh); }
		void DestroyStaticMeshEntities(const std::set<cMeshEntity*>& aSet)
		{
			std::vector<size_t> vSet;
			for(cMeshEntity *pMesh : aSet) vSet.push_back((size_t)pMesh);
			ohpl_wd_destroy_static_meshes(mpW, vSet.data(), vSet.size());
		}
		void MakeMeshEntityDynamic(cMeshEntity* apMesh){ ohpl_wd_make_mesh_dynamic(mpW, apMesh); }
		void MakeRenderableDynamic(iRenderable* apObject){ ohpl_wd_make_renderable_dynamic(mpW, apObject); }
		cMeshEntity* GetDynamicMeshEntity(const tString& asName){ return OhplH13Name<cMeshEntity*>(mpW->mpH13Reg, kOhplH13DynMesh, asName); }
		cMeshEntityIterator GetDynamicMeshEntityIterator(){ return cMeshEntityIterator(mpW->mpH13Reg, kOhplH13DynMesh); }
		cMeshEntityIterator GetStaticMeshEntityIterator(){ return cMeshEntityIterator(mpW->mpH13Reg, kOhplH13StaticMesh); }
		void DrawMeshBoundingBoxes(const cColor &aColor, bool abStatic){ ohpl_wd_draw_mesh_bounding_boxes(mpW, &aColor, abStatic); }

		cLightPoint* CreateLightPoint(const tString &asName="",const tString &asGobo="", bool abStatic=false);
		cLightSpot* CreateLightSpot(const tString &asName="", const tString &asGobo="", bool abStatic=false);
		cLightBox* CreateLightBox(const tString &asName="", bool abStatic=false);
		void DestroyLight(iLight* apLight){ ohpl_wd_destroy(mpW, kOhplH13Light, apLight); }
		iLight* GetLight(const tString& asName){ return OhplH13Name<iLight*>(mpW->mpH13Reg, kOhplH13Light, asName); }
		iLight* GetLightFromUniqueID(int alID){ return OhplH13Id<iLight*>(mpW->mpH13Reg, kOhplH13Light, alID); }
		cLightListIterator GetLightIterator(){ return cLightListIterator(mpW->mpH13Reg, kOhplH13Light); }

		cBillboard* CreateBillboard(const tString& asName, const cVector2f& avSize,eBillboardType aType,const tString& asMaterial="",bool abStatic=false)
		{
			return static_cast<cBillboard*>(ohpl_wd_create_billboard(mpW, asName.data(), asName.size(), &avSize, (int32_t)aType, asMaterial.data(), asMaterial.size(), abStatic));
		}
		void DestroyBillboard(cBillboard* apObject){ ohpl_wd_destroy(mpW, kOhplH13Billboard, apObject); }
		cBillboard* GetBillboard(const tString& asName){ return OhplH13Name<cBillboard*>(mpW->mpH13Reg, kOhplH13Billboard, asName); }
		cBillboard* GetBillboardFromUniqueID(int alID){ return OhplH13Id<cBillboard*>(mpW->mpH13Reg, kOhplH13Billboard, alID); }
		cBillboardIterator GetBillboardIterator(){ return cBillboardIterator(mpW->mpH13Reg, kOhplH13Billboard); }

		cLensFlare* CreateLensFlare(const tString& asName, const cVector2f& avSize, const tString& asMaterial="", bool abStatic=false)
		{
			return static_cast<cLensFlare*>(ohpl_wd_create_lens_flare(mpW, asName.data(), asName.size(), &avSize, asMaterial.data(), asMaterial.size(), abStatic));
		}
		void DestroyLensFlare(cLensFlare* apObject){ ohpl_wd_destroy(mpW, kOhplH13LensFlare, apObject); }
		cLensFlare* GetLensFlare(const tString& asName){ return OhplH13Name<cLensFlare*>(mpW->mpH13Reg, kOhplH13LensFlare, asName); }
		cLensFlare* GetLensFlareFromUniqueID(int alID){ return OhplH13Id<cLensFlare*>(mpW->mpH13Reg, kOhplH13LensFlare, alID); }
		cLensFlareIterator GetLensFlareIterator(){ return cLensFlareIterator(mpW->mpH13Reg, kOhplH13LensFlare); }

		cBeam* CreateBeam(const tString& asName, bool abStatic=false){ return static_cast<cBeam*>(Simple(kOhplH13Beam, asName, abStatic)); }
		void DestroyBeam(cBeam* apObject){ ohpl_wd_destroy(mpW, kOhplH13Beam, apObject); }
		cBeam* GetBeam(const tString& asName){ return OhplH13Name<cBeam*>(mpW->mpH13Reg, kOhplH13Beam, asName); }
		cBeam* GetBeamFromUniqueID(int alID){ return OhplH13Id<cBeam*>(mpW->mpH13Reg, kOhplH13Beam, alID); }
		cBeamIterator GetBeamIterator(){ return cBeamIterator(mpW->mpH13Reg, kOhplH13Beam); }

		cParticleSystem* CreateParticleSystem(const tString& asName,const tString& asType, const cVector3f& avSize, bool abRemoveWhenDead=true)
		{
			return static_cast<cParticleSystem*>(ohpl_wd_create_particle_system(mpW, 0, asName.data(), asName.size(), asType.data(), asType.size(), NULL, &avSize, abRemoveWhenDead));
		}
		cParticleSystem* CreateParticleSystem(const tString& asName,const tString& asDataName, cXmlElement* apElement, const cVector3f& avSize)
		{
			return static_cast<cParticleSystem*>(ohpl_wd_create_particle_system(mpW, 1, asName.data(), asName.size(), asDataName.data(), asDataName.size(), apElement, &avSize, false));
		}
		void DestroyParticleSystem(cParticleSystem* apPS){ ohpl_wd_destroy_particle_system(mpW, apPS); }
		cParticleSystem* GetParticleSystem(const tString& asName){ return OhplH13Name<cParticleSystem*>(mpW->mpH13Reg, kOhplH13ParticleSystem, asName); }
		cParticleSystem* GetParticleSystemFromUniqueID(int alID){ return OhplH13Id<cParticleSystem*>(mpW->mpH13Reg, kOhplH13ParticleSystem, alID); }
		bool ParticleSystemExists(cParticleSystem* apPS){ return ohpl_h13_reg_index(mpW->mpH13Reg, kOhplH13ParticleSystem, apPS) >= 0; }
		void DestroyAllParticleSystems(){ ohpl_wd_destroy_all_particle_systems(mpW); }
		cParticleSystemIterator GetParticleSystemIterator(){ return cParticleSystemIterator(mpW->mpH13Reg, kOhplH13ParticleSystem); }

		cGuiSetEntity* CreateGuiSetEntity(const tString& asName, cGuiSet *apSet, bool abStatic=false){ return static_cast<cGuiSetEntity*>(Simple(kOhplH13GuiSet, asName, abStatic, false, apSet)); }
		void DestroyGuiSetEntity(cGuiSetEntity* apObject){ ohpl_wd_destroy(mpW, kOhplH13GuiSet, apObject); }
		cGuiSetEntity* GetGuiSetEntity(const tString& asName){ return OhplH13Name<cGuiSetEntity*>(mpW->mpH13Reg, kOhplH13GuiSet, asName); }
		cGuiSetEntity* GetGuiSetEntityFromUniqueID(int alID){ return OhplH13Id<cGuiSetEntity*>(mpW->mpH13Reg, kOhplH13GuiSet, alID); }
		cGuiSetEntityIterator GetGuiSetEntityIterator(){ return cGuiSetEntityIterator(mpW->mpH13Reg, kOhplH13GuiSet); }

		cRopeEntity* CreateRopeEntity(const tString& asName, iPhysicsRope *apRope, int alMaxSegments){ return static_cast<cRopeEntity*>(Simple(kOhplH13Rope, asName, false, false, apRope, alMaxSegments)); }
		void DestroyRopeEntity(cRopeEntity* apRope){ ohpl_wd_destroy(mpW, kOhplH13Rope, apRope); }
		cRopeEntity* GetRopeEntity(const tString& asName){ return OhplH13Name<cRopeEntity*>(mpW->mpH13Reg, kOhplH13Rope, asName); }
		cRopeEntity* GetRopeEntityFromUniqueID(int alID){ return OhplH13Id<cRopeEntity*>(mpW->mpH13Reg, kOhplH13Rope, alID); }
		cRopeEntityIterator GetRopeEntityIterator(){ return cRopeEntityIterator(mpW->mpH13Reg, kOhplH13Rope); }

		cFogArea* CreateFogArea(const tString& asName, bool abStatic=false){ return static_cast<cFogArea*>(Simple(kOhplH13FogArea, asName, abStatic)); }
		void DestroyFogArea(cFogArea* apRope){ ohpl_wd_destroy(mpW, kOhplH13FogArea, apRope); }
		cFogArea* GetFogArea(const tString& asName){ return OhplH13Name<cFogArea*>(mpW->mpH13Reg, kOhplH13FogArea, asName); }
		cFogArea* GetFogAreaFromUniqueID(int alID){ return OhplH13Id<cFogArea*>(mpW->mpH13Reg, kOhplH13FogArea, alID); }
		cFogAreaIterator GetFogAreaIterator(){ return cFogAreaIterator(mpW->mpH13Reg, kOhplH13FogArea); }

		cForceField* CreateForceField(const tString& asName, bool abAutoRemove, bool abStatic){ return static_cast<cForceField*>(Simple(kOhplH13ForceField, asName, abStatic, abAutoRemove)); }
		void DestroyForceField(cForceField* apField){ ohpl_wd_destroy(mpW, kOhplH13ForceField, apField); }
		cForceField* GetForceField(const tString& asName){ return OhplH13Name<cForceField*>(mpW->mpH13Reg, kOhplH13ForceField, asName); }
		cForceFieldIterator GetForceFieldIterator(){ return cForceFieldIterator(mpW->mpH13Reg, kOhplH13ForceField); }
		int GetForceFields(const cVector3f& avMin, const cVector3f& avMax, cForceField **apOut){ return ohpl_wd_force_fields(mpW, &avMin, &avMax, (void**)apOut); }

		cSoundEntity* CreateSoundEntity(const tString &asName,const tString &asSoundEntity, bool abRemoveWhenOver)
		{
			return static_cast<cSoundEntity*>(ohpl_wd_create_sound(mpW, asName.data(), asName.size(), asSoundEntity.data(), asSoundEntity.size(), abRemoveWhenOver));
		}
		void DestroySoundEntity(cSoundEntity* apEntity){ ohpl_wd_destroy(mpW, kOhplH13Sound, apEntity); }
		cSoundEntity* GetSoundEntity(const tString& asName){ return OhplH13Name<cSoundEntity*>(mpW->mpH13Reg, kOhplH13Sound, asName); }
		cSoundEntity* GetSoundEntityFromUniqueID(int alID){ return OhplH13Id<cSoundEntity*>(mpW->mpH13Reg, kOhplH13Sound, alID); }
		void DestroyAllSoundEntities(){ ohpl_wd_destroy_all_sounds(mpW); }
		bool SoundEntityExists(cSoundEntity* apEntity, int alCreationID){ return ohpl_wd_sound_exists(mpW, apEntity, alCreationID); }
		bool IsValid(cSoundEntity* apEntity){ return OhplH13Index(mpW->mpH13Reg, kOhplH13Sound, apEntity) >= 0; }
		cSoundEntityIterator GetSoundEntityIterator(){ return cSoundEntityIterator(mpW->mpH13Reg, kOhplH13Sound); }

		void SetTerrain(cTerrain* apTerrain){ ohpl_wd_set_terrain(mpW, apTerrain); }

		cStartPosEntity* CreateStartPos(const tString &asName)
		{
			cStartPosEntity *pStartPos = hplNew( cStartPosEntity, (asName) );
			OhplH13Push(mpW->mpH13Reg, kOhplH13StartPos, pStartPos);
			return pStartPos;
		}
		cStartPosEntity* GetStartPosEntity(const tString &asName){ return OhplH13Name<cStartPosEntity*>(mpW->mpH13Reg, kOhplH13StartPos, asName); }
		cStartPosEntity* GetFirstStartPosEntityByName(){ return static_cast<cStartPosEntity*>(ohpl_h13_reg_first_by_name_f(mpW->mpH13Reg, kOhplH13StartPos)); }
		cStartPosEntity* GetFirstStartPosEntity(){ return static_cast<cStartPosEntity*>(ohpl_h13_reg_get(mpW->mpH13Reg, kOhplH13StartPos, 0)); }

		cDummyRenderable* CreateDummyRenderable(const tString& asName, bool abStatic=false){ return static_cast<cDummyRenderable*>(Simple(kOhplH13Dummy, asName, abStatic)); }
		void DestroyDummyRenderable(cDummyRenderable* apDummy){ ohpl_wd_destroy(mpW, kOhplH13Dummy, apDummy); }
		cDummyRenderable* GetDummyRenderable(const tString& asName){ return OhplH13Name<cDummyRenderable*>(mpW->mpH13Reg, kOhplH13Dummy, asName); }
		cDummyRenderable* GetDummyRenderableFromUniqueID(int alID){ return OhplH13Id<cDummyRenderable*>(mpW->mpH13Reg, kOhplH13Dummy, alID); }
		cDummyRenderableIterator GetDummyRenderableIterator(){ return cDummyRenderableIterator(mpW->mpH13Reg, kOhplH13Dummy); }





		void SetFilePath(const tWString& asFile){ msFilePath = asFile;}
		const tWString& GetFilePath(){ return msFilePath;}

		void SetActive(bool abX) {mpW->mbActive = abX;}
		inline bool IsActive()  const { return mpW->mbActive;}



		cVector3f GetWorldSize(){ return mpW->mvWorldSize;}

		void SetIsSoundEmitter(bool abX){ mpW->mbIsSoundEmitter = abX;}
		bool IsSoundEmitter(){ return mpW->mbIsSoundEmitter;}

		
		cPhysics* GetPhysics(){ return mpW->mpPhysics;}
		cResources* GetResources(){ return mpW->mpResources;}
		cSound* GetSound(){ return mpW->mpSound;}
		cSystem* GetSystem(){ return mpW->mpSystem;}

								
		/**
		 * Call this when all things have been added to set up things like physics world size.
		 **/


		///// PHYSICS ////////////////////////////////


		///// SKYBOX ////////////////////////////////

		float GetSkyBoxBrightness(){ return mpW->mfSkyBoxBrightness;}
		void SetSkyBoxRotation(const cVector3f& avRot){ mpW->mvSkyBoxRotation = avRot;}
		const cVector3f& GetSkyBoxRotation(){ return mpW->mvSkyBoxRotation;}

		iTexture* GetSkyBoxTexture(){return mpW->mpSkyBoxTexture;}
		iVertexBuffer *GetSkyBoxVertexBuffer(){ return mpW->mpSkyBoxVtxBuffer;}
		bool GetSkyBoxActive(){ return mpW->mbSkyBoxActive;}
		cColor GetSkyBoxColor(){ return mpW->mSkyBoxColor;}
        
		void SetDistanceCullActive(bool abX){ mpW->mbDistanceCullActive = abX;}
		void SetDistanceCullMinRange(float afX){ mpW->mfDistanceCullMinRange = afX;}
		void SetDistanceCullScreenSize(float afX){ mpW->mfDistanceCullScreenSize = afX;}
		void SetDistanceCullRandomSize(float afX){ mpW->mfDistanceCullRandomSize = afX;}
		void SetDistanceCullMaxRange(float afX){ mpW->mfDistanceCullMaxRange = afX;}
		void SetDistanceCullFadeSpeed(float afX){ mpW->mfDistanceCullFadeSpeed = afX;}
		bool GetDistanceCullActive(){ return mpW->mbDistanceCullActive;}
		float GetDistanceCullMinRange(){ return mpW->mfDistanceCullMinRange;}
		float GetDistanceCullScreenSize(){ return mpW->mfDistanceCullScreenSize;}
		float GetDistanceCullRandomSize(){ return mpW->mfDistanceCullRandomSize;}
		float GetDistanceCullMaxRange(){ return mpW->mfDistanceCullMaxRange;}
		float GetDistanceCullFadeSpeed(){ return mpW->mfDistanceCullFadeSpeed;}

		///// FOG ////////////////////////////////

		void SetFogActive(bool abX){ mpW->mbFogActive = abX;}
		void SetFogStart(float afX){ mpW->mfFogStart = afX;}
		void SetFogEnd(float afX){ mpW->mfFogEnd = afX;}
		void SetFogFalloffExp(float afX){ mpW->mfFogFalloffExp = afX;}
		void SetFogColor(const cColor& aCol){ mpW->mFogColor = aCol; }
		void SetFogCulling(bool abX) {mpW->mbFogCulling=abX;}
		void SetFogBrightness(float afX){ mpW->mfFogBrightness = afX > 0.0001f ? afX : 0.0001f; }
		void SetSecondaryFogBrightness(float afX){ mpW->mfSecondaryFogBrightness = afX > 0.0001f ? afX : 0.0001f; }
		void SetFogUnderwater(bool abX){ mpW->mbFogUnderwater = abX; }
		void SetFogUseSkybox(bool abX){ mpW->mbFogUseSkybox = abX; }
		void SetFogSkyboxTexture(iTexture *apTexture){ mpW->mpFogSkyboxTexture = apTexture; }
		void SetFogApplyAfterFogAreas(bool abX){ mpW->mbFogApplyAfterFogAreas = abX; }
		void SetFogNoise(float afStrength, float afSize, const cVector3f& avTurbulence){ mpW->mfFogNoiseStrength = afStrength; mpW->mfFogNoiseSize = afSize; mpW->mvFogNoiseTurbulence = avTurbulence; }
		void SetSecondaryFogActive(bool abX){ mpW->mbSecondaryFogActive = abX; }
		void SetSecondaryFogStart(float afX){ mpW->mfSecondaryFogStart = afX; }
		void SetSecondaryFogEnd(float afX){ mpW->mfSecondaryFogEnd = afX; }
		void SetSecondaryFogFalloffExp(float afX){ mpW->mfSecondaryFogFalloffExp = afX; }
		void SetSecondaryFogColor(const cColor& aCol){ mpW->mSecondaryFogColor = aCol; }

		bool GetFogActive(){ return mpW->mbFogActive;}
		float GetFogStart(){ return mpW->mfFogStart;}
		float GetFogEnd(){ return mpW->mfFogEnd;}
		float GetFogFalloffExp(){ return mpW->mfFogFalloffExp;}
		const cColor& GetFogColor(){ return mpW->mFogColor; }
		bool GetFogCulling() { return mpW->mbFogCulling;}
		float GetFogBrightness(){ return mpW->mfFogBrightness;}
		float GetSecondaryFogBrightness(){ return mpW->mfSecondaryFogBrightness;}
		bool GetFogUnderwater(){ return mpW->mbFogUnderwater; }
		bool GetFogUseSkybox(){ return mpW->mbFogUseSkybox; }
		iTexture* GetFogSkyboxTexture(){ return mpW->mpFogSkyboxTexture ? mpW->mpFogSkyboxTexture : mpW->mpSkyBoxTexture; }
		bool GetFogApplyAfterFogAreas(){ return mpW->mbFogApplyAfterFogAreas; }
		float GetFogNoiseStrength(){ return mpW->mfFogNoiseStrength; }
		float GetFogNoiseSize(){ return mpW->mfFogNoiseSize; }
		const cVector3f& GetFogNoiseTurbulence(){ return mpW->mvFogNoiseTurbulence; }
		bool GetSecondaryFogActive(){ return mpW->mbSecondaryFogActive; }
		float GetSecondaryFogStart(){ return mpW->mfSecondaryFogStart; }
		float GetSecondaryFogEnd(){ return mpW->mfSecondaryFogEnd; }
		float GetSecondaryFogFalloffExp(){ return mpW->mfSecondaryFogFalloffExp; }
		const cColor& GetSecondaryFogColor(){ return mpW->mSecondaryFogColor; }
		void SetFogLighten(bool abX){ mpW->mbFogLighten = abX; }
		bool GetFogLighten(){ return mpW->mbFogLighten; }
		void SetFogHeightBased(bool abX){ mpW->mbFogHeightBased = abX; }
		bool GetFogHeightBased(){ return mpW->mbFogHeightBased; }
		void SetFogExponential(bool abX){ mpW->mbFogExponential = abX; }
		bool GetFogExponential(){ return mpW->mbFogExponential; }
		void SetFogDensity(float afX){ mpW->mfFogDensity = afX; }
		float GetFogDensity(){ return mpW->mfFogDensity; }
		void SetFogHeightDensity(float afX){ mpW->mfFogHeightDensity = afX; }
		float GetFogHeightDensity(){ return mpW->mfFogHeightDensity; }
		void SetFogHeightHorizon(float afX){ mpW->mfFogHeightHorizon = afX; }
		float GetFogHeightHorizon(){ return mpW->mfFogHeightHorizon; }
		void SetSecondaryFogDensity(float afX){ mpW->mfSecondaryFogDensity = afX; }
		float GetSecondaryFogDensity(){ return mpW->mfSecondaryFogDensity; }
		void SetSecondaryFogHeightDensity(float afX){ mpW->mfSecondaryFogHeightDensity = afX; }
		float GetSecondaryFogHeightDensity(){ return mpW->mfSecondaryFogHeightDensity; }
		void SetSecondaryFogHeightHorizon(float afX){ mpW->mfSecondaryFogHeightHorizon = afX; }
		float GetSecondaryFogHeightHorizon(){ return mpW->mfSecondaryFogHeightHorizon; }

		void SetDirectionalLightActive(bool abX){ mpW->mbDirectionalLightActive = abX; }
		bool GetDirectionalLightActive(){ return mpW->mbDirectionalLightActive; }

		void SetEnvironmentParticlesActive(bool abX){ mpW->mbEnvParticlesActive = abX; }
		bool GetEnvironmentParticlesActive(){ return mpW->mbEnvParticlesActive; }

		void SetDepthOfFieldActive(bool abX){ mpW->mbDepthOfFieldActive = abX;}
		void SetDepthOfFieldFocusStart(float afX){ mpW->mfDepthOfFieldFocusStart = afX;}
		void SetDepthOfFieldFocusEnd(float afX){ mpW->mfDepthOfFieldFocusEnd = afX;}
		void SetDepthOfFieldFalloff(float afX){ mpW->mfDepthOfFieldFalloff = afX;}
		bool IsDepthOfFieldActive(){ return mpW->mbDepthOfFieldActive;}
		float GetDepthOfFieldFocusStart(){ return mpW->mfDepthOfFieldFocusStart;}
		float GetDepthOfFieldFocusEnd(){ return mpW->mfDepthOfFieldFocusEnd;}
		float GetDepthOfFieldFalloff(){ return mpW->mfDepthOfFieldFalloff;}

		void SetNightVisionColor(const cColor& aCol){ mpW->mNightVisionColor = aCol;}
		void SetNightVisionBrightness(float afX){ mpW->mfNightVisionBrightness = afX;}
		void SetNightVisionRadiusMin(float afX){ mpW->mfNightVisionRadiusMin = afX;}
		void SetNightVisionRadiusMax(float afX){ mpW->mfNightVisionRadiusMax = afX;}
		void SetNightVisionFalloff(float afX){ mpW->mfNightVisionFalloff = afX;}
		const cColor& GetNightVisionColor(){ return mpW->mNightVisionColor;}
		float GetNightVisionBrightness(){ return mpW->mfNightVisionBrightness;}
		float GetNightVisionRadiusMin(){ return mpW->mfNightVisionRadiusMin;}
		float GetNightVisionRadiusMax(){ return mpW->mfNightVisionRadiusMax;}
		float GetNightVisionFalloff(){ return mpW->mfNightVisionFalloff;}
		bool GetNightVisionVisible(){ return mpW->mfNightVisionRadiusMax > 0 && mpW->mfNightVisionBrightness > 0 && mpW->mNightVisionColor.r + mpW->mNightVisionColor.g + mpW->mNightVisionColor.b > 0;}

		void SetToneMapping(float afKey, float afExposure, float afWhiteCut){ mpW->mfToneMappingKey = afKey; mpW->mfToneMappingExposure = afExposure; mpW->mfToneMappingWhiteCut = afWhiteCut; }
		float GetToneMappingKey(){ return mpW->mfToneMappingKey; }
		float GetToneMappingExposure(){ return mpW->mfToneMappingExposure; }
		float GetToneMappingWhiteCut(){ return mpW->mfToneMappingWhiteCut; }
		void SetColorGradingTexture(const tString& asFile){ msColorGradingTexture = asFile; }
		const tString& GetColorGradingTexture(){ return msColorGradingTexture; }

		void AddExposureArea(const cWorldExposureArea& aArea){ mvExposureAreas.push_back(aArea); }
		std::vector<cWorldExposureArea>& GetExposureAreas(){ return mvExposureAreas; }

		///// AREA ////////////////////////////////
		

		///// MESH ENTITY METHODS ////////////////////
		
		
		
		
		///// LIGHT METHODS ////////////////////




		///// BILLBOARD METHODS ////////////////////



		///// BEAM METHODS ////////////////////


		///// PARTICLE METHODS ////////////////////




		///// GUISET ENTITY METHODS ////////////////////
		

		///// ROPE ENTITY METHODS ////////////////////
		

		///// FOG AREA METHODS ////////////////////


		///// SOUND ENTITY METHODS ////////////////////


		cTerrain* GetTerrain(){ return mpW->mpTerrain; }


		///// START POS ENTITY METHODS ////////////////
		

		///// AI NODE METHODS ////////////////


		cAINodeContainer* CreateAINodeContainer(const tString &asName, 
											const tString &asNodeName, 
											const cVector3f &avSize,
											bool abNodeIsAtCenter, 
											int alMinEdges, int alMaxEdges, float afMaxEdgeDistance,
											float afMaxHeight);

		cAStarHandler* CreateAStarHandler(cAINodeContainer* apContainer);
		void DestroyAStarHandler(cAStarHandler* apHandler);
        
		void AddAINode(const tString &asName, int alID, const tString &asType, const cVector3f &avPosition);


		/// NODE METHODS //////////////////////
		// Remove this for the time being, not need it seems.
		//cNode3D* GetRootNode(){ return mpW->mpRootNode; }

		///// DUMMY RENDERABLE METHODS ////////////////////


	private:
		void* Simple(uint32_t alKind, const tString& asName, bool abStatic, bool abFlag=false, void *apArg=NULL, int alId=0)
		{
			return ohpl_wd_create_simple(mpW, alKind, asName.data(), asName.size(), abStatic, abFlag, apArg, alId);
		}

		OhplWdPod *mpW;
		tWString msFilePath;
		tString msColorGradingTexture;
		std::vector<cWorldExposureArea> mvExposureAreas;
		tTempNodeContainerMap m_mapTempNodes;
	};

};
#endif // HPL_WOLRD_H
