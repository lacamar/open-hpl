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

#include "LuxMap.h"
#include "RustLuxMap.h"
#include "RustLuxMapA7.h"

#include "LuxConfigHandler.h"

#include "LuxEntity.h"
#include "LuxAreaNodes.h"

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxDebugHandler.h"
#include "LuxCompletionCountHandler.h"
#include "LuxMusicHandler.h"
#include "LuxProgressLogHandler.h"
#include "LuxEffectHandler.h"
#include "LuxMapHandler.h"

#include "LuxEnemy.h"
#include "LuxEnemyPathfinder.h"

#include "LuxProp_SwingDoor.h"
#include "LuxProp_Item.h"
#include "LuxProp_Lamp.h"
#include "LuxArea_Sticky.h"

#include <sstream>
#include <fstream>

//////////////////////////////////////////////////////////////////////////
// DISSOLVE ENTITIES
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxDissolveEntity::cLuxDissolveEntity(cLuxMap *apMap)
{
	mpMap = apMap;
}

static void A7DropEntity(void *p){ hplDelete(static_cast<iLuxEntity*>(p)); }
static void A7DropDissolve(void *p){ hplDelete(static_cast<cLuxDissolveEntity*>(p)); }
static void A7DropLamp(void *p){ hplDelete(static_cast<cLuxLampLightConnection*>(p)); }
static void A7DropStart(void *p){ hplDelete(static_cast<cLuxNode_PlayerStart*>(p)); }
static void A7DropPos(void *p){ hplDelete(static_cast<cLuxNode_Pos*>(p)); }
static void A7DropUse(void *p){ hplDelete(static_cast<cLuxUseItemCallback*>(p)); }
static void A7DissolveCb(void *apCtx, void *apObj, float afAlpha, bool abRemove){ static_cast<cLuxDissolveEntity*>(apObj)->mpEntity->SetCoverageAmount(afAlpha); }
static void A7LampCb(void *apCtx, void *apConn){ static_cast<cLuxLampLightConnection*>(apConn)->Update(*static_cast<float*>(apCtx)); }

cLuxDissolveEntity::~cLuxDissolveEntity()
{
	mpMap->mpWorld->DestroyMeshEntity(mpEntity);	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxMap::cLuxMap(const tString& asName)
{
	mpEngine = gpBase->mpEngine;

	msName = asName;

	mpScript = NULL;

	msLanternLitCallback = "";

	mlNumberOfQuests = 0;
	mlTotalCompletionAmount = 0;
	mlCurrentCompletionAmount = 0;

	mbUpdatingTimers = false;

	mbCommentaryIconsActive = false;

	mlA7Frames = 0;
	mpA7 = ohpl_a7_new();
	ohpl_a7_set_drop(mpA7, kOhplA7Entity, A7DropEntity);
	ohpl_a7_set_drop(mpA7, kOhplA7Dissolve, A7DropDissolve);
	ohpl_a7_set_drop(mpA7, kOhplA7Lamp, A7DropLamp);
	ohpl_a7_set_drop(mpA7, kOhplA7Start, A7DropStart);
	ohpl_a7_set_drop(mpA7, kOhplA7Pos, A7DropPos);
	ohpl_a7_set_drop(mpA7, kOhplA7UseItem, A7DropUse);
}

//-----------------------------------------------------------------------

cLuxMap::~cLuxMap()
{
	ohpl_a7_teardown(mpA7);

	mpEngine->GetScene()->DestroyWorld(mpWorld);	

	if(mpScript)
		mpEngine->GetResources()->GetScriptManager()->Destroy(mpScript);

	ohpl_a7_free(mpA7);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxMap::LoadFromFile(const tString & asFile, bool abLoadEntities)
{
	msFileName = asFile;

	gpBase->mpCurrentMapLoading = this;	

	//////////////////////////////
	//Extra demo check!
	#ifdef LUX_DEMO_VERSION
		if(cString::GetFirstStringPos(asFile, "demo")<0)
		{
			FatalError("Do not use demo exe to run full game!\n");
		}
		static int lDemoCount;
	#endif

	tWorldLoadFlag lFlags = (tWorldLoadFlag)ohpl_ga5_world_load_flags(	abLoadEntities, gpBase->mpConfigHandler->mbFastPhysicsLoad,
																		gpBase->mpConfigHandler->mbFastStaticLoad, gpBase->mpConfigHandler->mbFastEntityLoad,
																		eWorldLoadFlag_NoDynamicGameEntities, eWorldLoadFlag_FastPhysicsLoad,
																		eWorldLoadFlag_FastStaticLoad, eWorldLoadFlag_FastEntityLoad);

	LoadScript(asFile);

	//Load the world
	mpWorld = mpEngine->GetScene()->LoadWorld(asFile,lFlags);
	if(mpWorld==NULL)
		FatalError("Could not load world file '%s'\n", asFile.c_str());

	mpPhysicsWorld = mpWorld->GetPhysicsWorld();

	if(abLoadEntities) AfterWorldLoadEntitySetup();

	gpBase->mpCurrentMapLoading = NULL;


	if (gpBase->mbHardMode) ApplyHardMode();

	return true;
}

void cLuxMap::LoadScript(const tString & asFile)
{
	//Script file
	tString sScriptFile = cString::SetFileExt(asFile,"hps");
	bool bPlainScript = gpBase->mpEngine->GetResources()->GetFileSearcher()->GetFilePath(sScriptFile)!=_W("");
	bool bCompressedEnabled = cResources::GetCreateAndLoadCompressedMaps();
	bool bCompressedScript = false;
	if(bPlainScript==false && bCompressedEnabled)
	{
		sScriptFile = cString::SetFileExt(asFile,"chps");
		bCompressedScript = gpBase->mpEngine->GetResources()->GetFileSearcher()->GetFilePath(sScriptFile)!=_W("");
	}
	bool bScriptExists = ohpl_ga5_script_choice(bPlainScript, bCompressedEnabled, bCompressedScript) != kOhplGa5ScriptNone;
	
	if(bScriptExists)
	{
		tString sCompileMessages = "";
		mpScript = mpEngine->GetResources()->GetScriptManager()->CreateScript(sScriptFile, &sCompileMessages);
		if(mpScript==NULL)
		{
			//Only get errors!
			tString sErrorMess;
			ohpl_ga5_compile_errors(sCompileMessages.data(), sCompileMessages.size(), ohpl_ga5::Collect, &sErrorMess);

			FatalError("Could not load script file '%s'!\n%s", sScriptFile.c_str(), sErrorMess.c_str());
		}
	}
	else
	{
		Error("Script file %s does not exist!\n", sScriptFile.c_str());
	}
}

//-----------------------------------------------------------------------

void cLuxMap::ApplyHardMode()
{
	std::vector<iLuxEntity*> vEntitiesToDestroy;
		
	int lNumTotalTinderboxes = 0;
	int lNumRemovedTinderboxes = 0;


	float fTinderBoxRemoveEvery = ohpl_ga5_hard_tinderbox_every(gpBase->mpPlayer->GetTinderboxes());
	float fTinderBoxCounter = 0.0f;
	//float fTinderBoxRemoveEvery = 8.5f / 10.0f;

	int lNumTotalOil = 0;
	int lNumRemovedOil = 0;

	float fLampOilCounter = 0.0f;
	float fLampOilRemoveEvery = ohpl_ga5_hard_oil_every();

	/////////////////////
	// Go through all entities
	for (ohpl_a7::Walk<tLuxEntityList> entityIt(mpA7, kOhplA7Entities); entityIt.Has(); entityIt.Advance())
	{
		iLuxEntity *pEntity = entityIt.Get();

		eLuxEntityType entityType = pEntity->GetEntityType();

		/////////////////////
		// Check if entity is right type
		if (entityType == eLuxEntityType_Prop)
		{
			iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
			eLuxPropType propType = pProp->GetPropType();


			if (propType == eLuxPropType_Item)
			{
				cLuxProp_Item *pItem = static_cast<cLuxProp_Item*>(pProp);
				eLuxItemType itemType = pItem->GetItemType();

				/////////////////////
				// Tinderbox
				if (itemType == eLuxItemType_Tinderbox)
				{
					lNumTotalTinderboxes++;

					if (ohpl_ga5_hard_remove(&fTinderBoxCounter, fTinderBoxRemoveEvery))
					{
						vEntitiesToDestroy.push_back(pEntity);
						lNumRemovedTinderboxes++;
						continue;
					}
				}

				/////////////////////
				// Lamp oil
				if (itemType == eLuxItemType_LampOil)
				{
					lNumTotalOil++;

					if (ohpl_ga5_hard_remove(&fLampOilCounter, fLampOilRemoveEvery))
					{
						vEntitiesToDestroy.push_back(pEntity);
						lNumRemovedOil++;
						continue;
					}
				}
			}
		}
	}

	//////////////////////////
	// Print what i've done
	#if not MAC_OS && not LINUX
	sDebugMsg += L" of ";
	sDebugMsg += std::to_wstring(static_cast<long long>(lNumTotalTinderboxes));
	sDebugMsg += L" tinderboxes removed, ";
	sDebugMsg += std::to_wstring(static_cast<long double>(fTinderBoxRemoveEvery));
	gpBase->mpDebugHandler->AddMessage(sDebugMsg, false);

	sDebugMsg = std::to_wstring(static_cast<long long>(lNumRemovedOil));
	sDebugMsg += L" of ";
	sDebugMsg += std::to_wstring(static_cast<long long>(lNumTotalOil));
	sDebugMsg += L" oil removed";

	gpBase->mpDebugHandler->AddMessage(sDebugMsg, false);
        #endif

	//////////////////////////
	// Remove the entities from above
	for (size_t i = 0; i < vEntitiesToDestroy.size(); ++i)
	{
		DestroyEntity(vEntitiesToDestroy[i]);
	}
}

//-----------------------------------------------------------------------

void cLuxMap::AfterWorldLoadEntitySetup()
{
	//Extra setup for entities
	for(ohpl_a7::Walk<tLuxEntityList> entityIt(mpA7, kOhplA7Entities); entityIt.Has(); entityIt.Advance())
	{
		iLuxEntity *pEntity = entityIt.Get();

		pEntity->AfterWorldLoad();
	}
}

//-----------------------------------------------------------------------

void cLuxMap::OnEnter(bool abRunScript, bool abFirstTime)
{
	///////////////
	//On world load for all entities
	for(ohpl_a7::Walk<tLuxEntityList> entityIt(mpA7, kOhplA7Entities); entityIt.Has(); entityIt.Advance())
	{
		iLuxEntity *pEntity = entityIt.Get();
		pEntity->OnMapEnter();
	}

	///////////////
	//Run script
	int lPlan = ohpl_ga5_enter_plan(abRunScript, mpScript!=NULL, abFirstTime);
	if(lPlan & kOhplGa5EnterStart) mpScript->Run("OnStart()");
	if(lPlan & kOhplGa5EnterCompletion) CalculateTotalCompletionAmount();
	if(lPlan & kOhplGa5EnterEnter) mpScript->Run("OnEnter()");
}


//-----------------------------------------------------------------------

void cLuxMap::OnLeave(bool abRunScript)
{
	if(ohpl_ga5_run_script_allowed(mpScript!=NULL, abRunScript))
	{
		mpScript->Run("OnLeave()");
	}
}

//-----------------------------------------------------------------------

void cLuxMap::Update(float afTimeStep)
{
	static const char *sDumpFile = getenv("OHPL_A7_MAPDUMP");
	if(sDumpFile)
	{
		static const char *sDumpFrames = getenv("OHPL_A7_MAPDUMP_FRAMES");
		if(mlA7Frames == 0) DumpA7(sDumpFile, "load");
		++mlA7Frames;
		if(mlA7Frames == (sDumpFrames ? atoi(sDumpFrames) : 120)) DumpA7(sDumpFile, "frames");
	}

	UpdateCheckCommentaryIconActive(afTimeStep);
    UpdateDissolveEntities(afTimeStep);
	UpdateTimers(afTimeStep);
	
	UpdateToBeDesotroyedEntities(true);	

	////////////////////////////////////
	// Iterate entities
	for(ohpl_a7::Walk<tLuxEntityList> entityIt(mpA7, kOhplA7Entities); entityIt.Has(); entityIt.Advance())
	{
		iLuxEntity *pEntity = entityIt.Get();

        if(pEntity->IsActive()) 
			pEntity->UpdateLogic(afTimeStep);
	}

	UpdateToBeDesotroyedEntities(true);

	UpdateLampLightConnections(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxMap::RunScript(const tString& asCommand)
{
	if(ohpl_ga5_run_script_allowed(mpScript!=NULL, this == gpBase->mpMapHandler->GetCurrentMap())==false) return;

	mpScript->Run(asCommand);
}

bool cLuxMap::RecompileScript(tString *apOutput)
{
	if(mpScript)
		mpEngine->GetResources()->GetScriptManager()->Destroy(mpScript);
	mpScript = NULL;

	tString sScriptFile = cString::SetFileExt(msFileName,"hps");
	if(gpBase->mpEngine->GetResources()->GetFileSearcher()->GetFilePath(sScriptFile)!=_W(""))
	{
		mpScript = mpEngine->GetResources()->GetScriptManager()->CreateScript(sScriptFile, apOutput);
		
		return mpScript != NULL;
	}
	else
	{
		Error("Script file %s does not exist!\n", sScriptFile.c_str());
		return true;
	}
}

//-----------------------------------------------------------------------

void cLuxMap::OnRenderSolid(cRendererCallbackFunctions* apFunctions)
{
	if(gpBase->mpDebugHandler->GetShowEntityInfo()==false) return;

	for(ohpl_a7::Walk<tLuxEntityList> entityIt(mpA7, kOhplA7Entities); entityIt.Has(); entityIt.Advance())
	{
		iLuxEntity *pEntity = entityIt.Get();

		if(pEntity->IsActive()) 
			pEntity->OnRenderSolid(apFunctions);
	}
}

//-----------------------------------------------------------------------

void cLuxMap::PlacePlayerAtStartPos(const tString& asPosName)
{
	cLuxNode_PlayerStart *pNode = GetPlayerStart(asPosName);

	if(pNode == NULL)
	{
		Error("StartPos '%s' does not exist!\n", asPosName.c_str());
		pNode = GetFirstPlayerStart();
	}
	if(pNode)
	{
		gpBase->mpPlayer->PlaceAtStartNode(pNode);
	}
}

//-----------------------------------------------------------------------

void cLuxMap::SetCheckPoint(const tString& asName, const tString& asStartPos, const tString& asCallback)
{
	if(msCheckPointName == asName) return;

	msCheckPointName = asName;
	msCheckPointStartPos = asStartPos;
	msCheckPointCallback = asCallback;
	mlCheckPointCount =0;

	////////////////////////////////////
	// Get the highest prio looping music playing
	msCheckPointMusic = "";
	for(int i= gpBase->mpMusicHandler->GetMaxPrio();i>=0; --i)
	{
		cLuxMusic music = gpBase->mpMusicHandler->GetMusic(i);
		cLuxMusic *pMusic = &music;
		if(ohpl_ga5_checkpoint_music_pick(pMusic->mbLoop, pMusic->mbSpecialEffect, pMusic->msFile.data(), pMusic->msFile.size()))
		{
			msCheckPointMusic = pMusic->msFile;
			mlCheckPointMusicPrio = i;
			mbCheckPointMusicResume = pMusic->mbResume;

			mfCheckPointMusicVolume = pMusic->mfVolume;
			
			//When music handler plays music, it multiplies it to get a new volume. Need to change that back when storing.
			float fMusicVolumeMul =gpBase->mpMusicHandler->GetVolumeMul();
			mfCheckPointMusicVolume = ohpl_ga5_checkpoint_music_volume(mfCheckPointMusicVolume, fMusicVolumeMul);

			break;
		}
	}
	
	gpBase->mpDebugHandler->AddMessage(	_W("Setting check point ") + cString::To16Char(msCheckPointName) +
										_W(" Music: '") + cString::To16Char(msCheckPointMusic)+_W("'"), 
										false);

}

//-----------------------------------------------------------------------


void cLuxMap::LoadCheckPoint()
{
	gpBase->mpDebugHandler->AddMessage(_W("Loading check point ") + cString::To16Char(msCheckPointName) +_W(" Count: ") + cString::ToStringW(mlCheckPointCount), false);
	gpBase->mpProgressLogHandler->AddLog(eLuxProgressLogLevel_High, "Loading checkpoint "+msCheckPointName+" count: "+cString::ToString(mlCheckPointCount));

	//////////////////////////////
	// Place player
	PlacePlayerAtStartPos(msCheckPointStartPos);

	//////////////////////////////
	// Enemies
	for(ohpl_a7::Walk<tLuxEnemyList> it(mpA7, kOhplA7Enemies); it.Has(); it.Advance())
	{
		iLuxEnemy *pEnemy = it.Get();
		pEnemy->SetActive(false);
		pEnemy->ResetProperties();
	}

	//////////////////////////////
	// Player
	gpBase->mpPlayer->ClearTerrorEnemies();

	//////////////////////////////
	// Effects
    gpBase->mpEffectHandler->GetFade()->FadeIn(0.2f);
    
	//////////////////////////////
	// Music
	gpBase->mpMusicHandler->Reset();
	if(msCheckPointMusic != "")
		gpBase->mpMusicHandler->Play(msCheckPointMusic, true, mfCheckPointMusicVolume, 1, mlCheckPointMusicPrio,mbCheckPointMusicResume, false);


	//////////////////////////////
	// Run script (last thing done!)
	tString sCheckPointCall;
	ohpl_ga5_checkpoint_call(mlCheckPointCount, msCheckPointCallback.data(), msCheckPointCallback.size(),
							msCheckPointName.data(), msCheckPointName.size(), ohpl_ga5::Collect, &sCheckPointCall);
	RunScript(sCheckPointCall);
	
	mlCheckPointCount++;
}

//-----------------------------------------------------------------------

void cLuxMap::CreateEntity(const tString& asName, const tString& asFile, const cMatrixf& a_mtxTransform,const cVector3f& avScale)
{
	//Only set var if not already set!
	bool bSetCurrentMapLoading = gpBase->mpCurrentMapLoading==NULL;
	
	if(bSetCurrentMapLoading)	gpBase->mpCurrentMapLoading = this;

	mpWorld->CreateEntity(asName, a_mtxTransform, asFile,GetFreeEntityID() , true, avScale);

	if(bSetCurrentMapLoading)	gpBase->mpCurrentMapLoading = NULL;
}
//-----------------------------------------------------------------------

void cLuxMap::DestroyAllEntities()
{
	ohpl_a7_destroy_all(mpA7);
	mbCommentaryIconsActive = false;
}

//-----------------------------------------------------------------------

void cLuxMap::AddEntity(iLuxEntity *apEntity)
{
	bool bIsEnemy = apEntity->GetEntityType() == eLuxEntityType_Enemy;
	bool bArea = apEntity->GetEntityType() == eLuxEntityType_Area;
	bool bSticky = bArea && static_cast<iLuxArea*>(apEntity)->GetAreaType() == eLuxAreaType_Sticky;
	void *pTyped = NULL;
	if(bIsEnemy) pTyped = static_cast<iLuxEnemy*>(apEntity);
	else if(bSticky) pTyped = static_cast<cLuxArea_Sticky*>(static_cast<iLuxArea*>(apEntity));
	ohpl_a7_ent_add(mpA7, apEntity, pTyped, OHPL_A7_S(apEntity->GetName()), apEntity->GetID(), bIsEnemy, bArea, bSticky);
}

void cLuxMap::DestroyEntity(iLuxEntity *apEntity)
{
	if(IsDeletingAllWorldEntities()) Warning("Trying to delete entity '%s' when Map is destroying all entities!\n", apEntity->GetName().c_str());

	if(apEntity->GetDestroyMe()) return;

	apEntity->DestroyMe();
	ohpl_a7_doom_push(mpA7, apEntity);
}

//-----------------------------------------------------------------------

iLuxEntity *cLuxMap::GetEntityByName(const tString& asName, eLuxEntityType aType, int alSubType)
{
	iLuxEntity *pEntity;
	pEntity = static_cast<iLuxEntity*>(ohpl_a7_ent_find_name(mpA7, OHPL_A7_S(asName)));
	if(pEntity == NULL) return NULL;
	if(LuxIsCorrectType(pEntity, aType, alSubType)== false) return NULL;

	return pEntity;
}

iLuxEntity *cLuxMap::GetEntityByID(int alID, eLuxEntityType aType, int alSubType)
{
	iLuxEntity *pEntity;
	pEntity = static_cast<iLuxEntity*>(ohpl_a7_ent_find_id(mpA7, alID));
	if(pEntity == NULL) return NULL;

	if(LuxIsCorrectType(pEntity, aType, alSubType)== false) return NULL;

	return pEntity;
}

//-----------------------------------------------------------------------

bool cLuxMap::EntityExists(iLuxEntity *apEntity)
{
	return ohpl_a7_ent_exists(mpA7, apEntity);
}

//-----------------------------------------------------------------------

cLuxEntityIterator cLuxMap::GetEntityIterator()
{
	return cLuxEntityIterator(mpA7, kOhplA7Entities);
}

//-----------------------------------------------------------------------

cLuxEnemyIterator cLuxMap::GetEnemyIterator()
{
	return cLuxEnemyIterator(mpA7, kOhplA7Enemies);
}

//-----------------------------------------------------------------------

void cLuxMap::BroadcastEnemyMessage(eLuxEnemyMessage aType, bool abHasPosition, const cVector3f& avPos, float afRadius,
									float afTime, bool abLocalScope, const cVector3f& avX,float afX, int alX)
{
	for(ohpl_a7::Walk<tLuxEnemyList> it(mpA7, kOhplA7Enemies); it.Has(); it.Advance())
	{
		iLuxEnemy *pEnemy = it.Get();
		if(pEnemy->IsActive()==false) continue;

		////////////////////////////////
		// Check if the enemy is in range
		if(abHasPosition)
		{
			cBoundingVolume *pBv = pEnemy->GetCharacterBody()->GetCurrentBody()->GetBoundingVolume();
			if(cMath::CheckSphereIntersection(avPos, afRadius, pBv->GetWorldCenter(), pBv->GetRadius())==false)
			{
				continue;
			}
		}

		////////////////////////////////
		// Send message
        pEnemy->SendMessage(aType, afTime,abLocalScope,avX, afX, alX);
	}
}

//-----------------------------------------------------------------------

void cLuxMap::BroadcastEnemySoundMessage(const cVector3f& avPos, float afVolume ,float afMinDist, float afMaxDist)
{
	cLuxEnemyIterator it =GetEnemyIterator();
	while(it.HasNext())
	{
		iLuxEnemy *pEnemy = it.Next();
		if(pEnemy->IsActive()==false) continue;
		if(pEnemy->GetDeaf()) continue;

		/////////////////////////
		//Check intersection
		cBoundingVolume *pBv = pEnemy->GetCharacterBody()->GetCurrentBody()->GetBoundingVolume();
		if(cMath::CheckSphereIntersection(avPos, afMaxDist, pBv->GetWorldCenter(), pBv->GetRadius())==false)
		{
			continue;
		}

		/////////////////////////
		//Calculate volume (skip sounds that are too close)
		float fDistance = cMath::Vector3Dist(pBv->GetWorldCenter(), avPos);
		float fHearVolume;
		if(ohpl_ga5_sound_volume(fDistance, afMinDist, afMaxDist, afVolume, &fHearVolume)==false) continue;

		/////////////////////////
		//Send message
		pEnemy->SendMessage(eLuxEnemyMessage_SoundHeard, 0, false, avPos, fHearVolume,0);
	}
}

//-----------------------------------------------------------------------


//-----------------------------------------------------------------------

bool cLuxMap::AINodeIsUsedAsGoal(cAINode *apNode)
{
	cLuxEnemyIterator it =GetEnemyIterator();
	while(it.HasNext())
	{
		iLuxEnemy *pEnemy = it.Next();

		tAINodeList *pNodeList = pEnemy->GetPathFinder()->GetNodeList();
		if(pNodeList->empty()) continue;

		if(pNodeList->front() == apNode) return true;
	}
	return false;
}

//-----------------------------------------------------------------------

bool cLuxMap::DoorIsBroken(int alID)
{
	iLuxEntity *pEntity = GetEntityByID(alID);
	if(pEntity==NULL) return false;
	if(pEntity->GetEntityType() != eLuxEntityType_Prop) return false;

	iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
	if(pProp->GetPropType() != eLuxPropType_SwingDoor) return false;

    cLuxProp_SwingDoor *pDoor = static_cast<cLuxProp_SwingDoor*>(pProp);
	return pDoor->IsBroken();
}

int cLuxMap::GetDoorState(int alID)
{
	iLuxEntity *pEntity = GetEntityByID(alID);
	if(pEntity==NULL) return false;
	if(pEntity->GetEntityType() != eLuxEntityType_Prop) return false;

	iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
	if(pProp->GetPropType() != eLuxPropType_SwingDoor) return false;

	cLuxProp_SwingDoor *pDoor = static_cast<cLuxProp_SwingDoor*>(pProp);
	return pDoor->GetDoorState();
}

//-----------------------------------------------------------------------

bool cLuxMap::BodyIsInDetachableStickyArea(iPhysicsBody* apBody)
{
	for(ohpl_a7::Walk<tLuxArea_StickyList> it(mpA7, kOhplA7StickyList); it.Has(); it.Advance())
	{
		cLuxArea_Sticky *pStickyArea = it.Get();

		if(pStickyArea->GetAttachedBody() == apBody)
		{
			return pStickyArea->CanDetach();
		}
	}
	return false;
}

//-----------------------------------------------------------------------

bool cLuxMap::DetachBodyFromStickyArea(iPhysicsBody* apBody)
{
	for(ohpl_a7::Walk<tLuxArea_StickyList> it(mpA7, kOhplA7StickyList); it.Has(); it.Advance())
	{
		cLuxArea_Sticky *pStickyArea = it.Get();
		if(pStickyArea->GetAttachedBody() == apBody)
		{
			if(pStickyArea->CanDetach())
			{
				pStickyArea->DetachBody();
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	return false;
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

iPhysicsBody* cLuxMap::GetBodyFromEntityBodyIdPair(const cLuxIdPair &aIdPair)
{
	iLuxEntity *pEntity = GetEntityByID(aIdPair.mlParentId);
	if(pEntity==NULL) return NULL;

	if(pEntity->GetEntityType() == eLuxEntityType_Prop)
	{	
		iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);

        return pProp->GetBodyFromID(aIdPair.mlChildId);	
	}
	else
	{
		return NULL;
	}
}

//-----------------------------------------------------------------------

bool cLuxMap::CheckCollision(iLuxCollideCallbackContainer *apCollider1, iLuxCollideCallbackContainer* apCollider2)
{
	cCollideData collideData;
	collideData.SetMaxSize(1);

	for(int body1=0; body1<apCollider1->GetBodyNum(); ++body1)
	for(int body2=0; body2<apCollider2->GetBodyNum(); ++body2)
	{
		iPhysicsBody *pBody1 = apCollider1->GetBody(body1);
		iPhysicsBody *pBody2 = apCollider2->GetBody(body2);

		if(cMath::CheckBVIntersection(*pBody1->GetBoundingVolume(), *pBody2->GetBoundingVolume())==false)
		{
			continue;
		}

		if(mpPhysicsWorld->CheckShapeCollision(pBody1->GetShape(), pBody1->GetLocalMatrix(), 
											pBody2->GetShape(), pBody2->GetLocalMatrix(),
											collideData,1,false))
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------
void cLuxMap::AddPlayerStart(cLuxNode_PlayerStart *apNode)
{
	ohpl_a7_start_add(mpA7, apNode, OHPL_A7_S(apNode->GetName()));
}

cLuxNode_PlayerStart * cLuxMap::GetPlayerStart(const tString & asName)
{
	cLuxNode_PlayerStart *pNode = static_cast<cLuxNode_PlayerStart*>(ohpl_a7_start_find(mpA7, OHPL_A7_S(asName)));
	if(pNode == NULL) Warning("Could not find player start node '%s'\n", asName.c_str());
	return pNode;
}

//-----------------------------------------------------------------------

cLuxNode_PlayerStart *cLuxMap::GetFirstPlayerStart()
{
	return static_cast<cLuxNode_PlayerStart*>(ohpl_a7_start_first(mpA7));
}

//-----------------------------------------------------------------------

void cLuxMap::AddPosNode(cLuxNode_Pos *apNode)
{
	ohpl_a7_pos_add(mpA7, apNode, OHPL_A7_S(apNode->GetName()));
}

cLuxNode_Pos *cLuxMap::GetPosNode(const tString & asName)
{
	cLuxNode_Pos *pNode = static_cast<cLuxNode_Pos*>(ohpl_a7_pos_find(mpA7, OHPL_A7_S(asName)));
	if(pNode == NULL) Warning("Could not find pos node '%s'\n", asName.c_str());
	return pNode;
}

//-----------------------------------------------------------------------

void cLuxMap::AddUseItemCallback(	const tString& asName, const tString& asItem, const tString& asEntity,
									const tString& asFunction, bool abAutoCallback)
{
	cLuxUseItemCallback *pCallback = hplNew(cLuxUseItemCallback, ());

	pCallback->msName = asName;
	pCallback->msItem = asItem;
	pCallback->msEntity = asEntity;
	pCallback->msFunction = asFunction;
	pCallback->mbAutoDestroy = abAutoCallback;
	
	ohpl_a7_use_add(mpA7, pCallback, OHPL_A7_S(asName), OHPL_A7_S(asItem), OHPL_A7_S(asEntity));
}

void cLuxMap::RemoveUseItemCallback( const tString& asName)
{
	if(ohpl_a7_use_remove_name(mpA7, OHPL_A7_S(asName))==false)
		Warning("Could not find use item callback '%s'!\n", asName.c_str());
}

void cLuxMap::RemoveUseItemCallback(cLuxUseItemCallback * apCallback, const tString& asName)
{
	ohpl_a7_use_remove_ptr(mpA7, apCallback, OHPL_A7_S(asName));
}

cLuxUseItemCallback* cLuxMap::GetUseItemCallback(const tString& asItem, const tString& asEntity)
{
	return static_cast<cLuxUseItemCallback*>(ohpl_a7_use_find(mpA7, OHPL_A7_S(asItem), OHPL_A7_S(asEntity)));
}

//-----------------------------------------------------------------------

void cLuxMap::AddTimer(const tString& asName, float afTime, const tString& asFunction)
{
	ohpl_a7_timer_add(mpA7, OHPL_A7_S(asName), afTime, OHPL_A7_S(asFunction));
}

//-----------------------------------------------------------------------

void cLuxMap::RemoveTimer(const tString& asName)
{
	ohpl_a7_timer_remove(mpA7, OHPL_A7_S(asName));
}

//-----------------------------------------------------------------------

bool cLuxMap::GetTimerCount(const tString& asName, float *apCount)
{
	return ohpl_a7_timer_get(mpA7, OHPL_A7_S(asName), apCount);
}

void cLuxMap::CopyTimers(std::vector<cLuxEventTimer> &avOut)
{
	for(size_t i=0; i<ohpl_a7_timer_len(mpA7); ++i)
	{
		cLuxEventTimer timer;
		bool bDestroy;
		ohpl_a7_timer_at(mpA7, i, ohpl_ga5::Collect, &timer.msName, &timer.msFunction, &timer.mfCount, &bDestroy);
		timer.mbDestroyMe = bDestroy;
		avOut.push_back(timer);
	}
}

void cLuxMap::SetTimers(const std::vector<cLuxEventTimer> &avIn)
{
	ohpl_a7_timers_clear(mpA7);
	for(size_t i=0; i<avIn.size(); ++i)
	{
		const cLuxEventTimer &t = avIn[i];
		ohpl_a7_timer_push(mpA7, OHPL_A7_S(t.msName), OHPL_A7_S(t.msFunction), t.mfCount, t.mbDestroyMe);
	}
}

void cLuxMap::CopyUseItemCallbacks(std::vector<cLuxUseItemCallback> &avOut)
{
	for(size_t i=0; i<ohpl_a7_use_len(mpA7); ++i) avOut.push_back(*static_cast<cLuxUseItemCallback*>(ohpl_a7_use_at(mpA7, i)));
}

void cLuxMap::SetUseItemCallbacks(const std::vector<cLuxUseItemCallback> &avIn)
{
	ohpl_a7_use_clear(mpA7);
	for(size_t i=0; i<avIn.size(); ++i)
	{
		cLuxUseItemCallback *pCallback = hplNew(cLuxUseItemCallback, ());
		*pCallback = avIn[i];
		ohpl_a7_use_add(mpA7, pCallback, OHPL_A7_S(pCallback->msName), OHPL_A7_S(pCallback->msItem), OHPL_A7_S(pCallback->msEntity));
	}
}

//-----------------------------------------------------------------------

void cLuxMap::AddCompletionAmount(int alAmount, float afDelay)
{
	float fPrevious, fNew;
	ohpl_ga5_completion_add(&mlCurrentCompletionAmount, mlTotalCompletionAmount, alAmount, &fPrevious, &fNew);

	gpBase->mpCompletionCountHandler->ShowCompletionIncrease(fPrevious, fNew, afDelay);
}

//-----------------------------------------------------------------------

void cLuxMap::AddDissolveEntity(cMeshEntity *apMeshEntity, float afTime)
{
	cMesh *pMesh = apMeshEntity->GetMesh();
	pMesh->IncUserCount();

	cMeshEntity *pNewEntity = mpWorld->CreateMeshEntity(apMeshEntity->GetName()+"_Dissolve", pMesh);
	pNewEntity->SetIlluminationAmount(apMeshEntity->GetIlluminationAmount());

	pNewEntity->Stop();

    cLuxDissolveEntity *pDissolveEnt = hplNew(cLuxDissolveEntity, (this));
	pDissolveEnt->mpEntity = pNewEntity;
	pDissolveEnt->mfFadeSpeed = ohpl_ga5_dissolve_speed(afTime);
	pDissolveEnt->mfAlpha =1;
	ohpl_a7_dissolve_add(mpA7, pDissolveEnt, afTime);

	//////////////////////
	//Set up transform
	if(pMesh->GetSkeleton())
	{
		pNewEntity->SetMatrix(apMeshEntity->GetWorldMatrix());

		for(int i=0; i<pNewEntity->GetBoneStateNum(); ++i)
		{
			cBoneState *pOldBone = apMeshEntity->GetBoneState(i);
			cBoneState *pNewBone = pNewEntity->GetBoneState(i);

			pNewBone->SetActive(false);
			pNewBone->SetMatrix(pOldBone->GetLocalMatrix());
		}
	}
	else
	{
		for(int i=0; i<pNewEntity->GetSubMeshEntityNum(); ++i)
		{
			cSubMeshEntity *pOldEnt = apMeshEntity->GetSubMeshEntity(i);
			cSubMeshEntity *pSubEnt = pNewEntity->GetSubMeshEntity(i);
			pSubEnt->SetMatrix(pOldEnt->GetWorldMatrix());
		}
	}


}

//-----------------------------------------------------------------------

cLuxLampLightConnection* cLuxMap::AddLampLightConnection(cLuxProp_Lamp *apLamp, iLight *apLight, float afAmount, bool abUseOnColor, bool abUseSpec)
{
	cLuxLampLightConnection *pConnection = GetLampLightConnection(apLight);
	if(pConnection == NULL)
	{
		pConnection = hplNew(cLuxLampLightConnection, (apLight));
		ohpl_a7_lamp_add(mpA7, pConnection, apLight);
	}
    
	pConnection->AddLamp(apLamp, afAmount, abUseOnColor, abUseSpec);
	pConnection->Update(0.001f);
	return pConnection;
}

//-----------------------------------------------------------------------

cLuxLampLightConnection* cLuxMap::GetLampLightConnection(iLight *apLight)
{
	return static_cast<cLuxLampLightConnection*>(ohpl_a7_lamp_find(mpA7, apLight));
}

//-----------------------------------------------------------------------

cLuxScriptVar* cLuxMap::GetVar(const tString &asName)
{
	tLuxScriptVarMapIt it = m_mapVars.find(asName);
	if(it != m_mapVars.end()) return &(it->second);

	m_mapVars.insert(tLuxScriptVarMap::value_type(asName, cLuxScriptVar(asName)));
	it = m_mapVars.find(asName);
	return &(it->second);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMap::CalculateTotalCompletionAmount()
{
	////////////////
	//Quests
	mlTotalCompletionAmount = ohpl_ga5_quest_completion(mlTotalCompletionAmount, mlNumberOfQuests, gpBase->mpCompletionCountHandler->mlQuestCompletionValue);

	gpBase->mpDebugHandler->AddMessage(_W("Total completion value: ")+cString::ToStringW(mlTotalCompletionAmount), false);
}

//-----------------------------------------------------------------------

int cLuxMap::GetFreeEntityID()
{
	return ohpl_a7_ent_free_id(mpA7);
}

//-----------------------------------------------------------------------

void cLuxMap::UpdateToBeDesotroyedEntities(bool abUseCallbacks)
{
	uint64_t lCur = ohpl_a7_cur_begin(mpA7, kOhplA7Doomed);
	while(iLuxEntity *pEntity = static_cast<iLuxEntity*>(ohpl_a7_cur_get(mpA7, kOhplA7Doomed, lCur)))
	{
		if(abUseCallbacks)
		{
			gpBase->mpPlayer->GetCurrentStateData()->OnDestroyEntity(pEntity);
			pEntity->BeforeEntityDestruction();
		}

		bool bIsEnemy = pEntity->GetEntityType() == eLuxEntityType_Enemy;
		bool bIsArea = pEntity->GetEntityType() == eLuxEntityType_Area;
		bool bSticky = bIsArea && static_cast<iLuxArea*>(pEntity)->GetAreaType() == eLuxAreaType_Sticky;
		void *pTyped = NULL;
		if(bIsEnemy) pTyped = static_cast<iLuxEnemy*>(pEntity);
		else if(bSticky) pTyped = static_cast<cLuxArea_Sticky*>(static_cast<iLuxArea*>(pEntity));
		ohpl_a7_ent_unlink(mpA7, pEntity, pTyped, bIsEnemy, bIsArea, bSticky);

		hplDelete(pEntity);
		lCur = ohpl_a7_cur_next(mpA7, kOhplA7Doomed, lCur);
	}
	ohpl_a7_doom_clear(mpA7);
}
//-----------------------------------------------------------------------

void cLuxMap::UpdateTimers(float afTimeStep)
{
	ohpl_a7_timers_begin(mpA7, afTimeStep);
	size_t lCursor = 0;
	tString sName, sFunc;
	while(ohpl_a7_timer_poll(mpA7, &lCursor, ohpl_ga5::Collect, &sName, &sFunc))
	{
		RunScript(ohpl_ga5::Call1(sFunc, sName));
		ohpl_a7_timer_finish(mpA7, lCursor);
	}
	ohpl_a7_timers_end(mpA7);
}

//-----------------------------------------------------------------------

void cLuxMap::UpdateDissolveEntities(float afTimeStep)
{
	ohpl_a7_dissolve_update(mpA7, afTimeStep, A7DissolveCb, NULL);
}

//-----------------------------------------------------------------------

void cLuxMap::UpdateLampLightConnections(float afTimeStep)
{
	ohpl_a7_lamp_each(mpA7, A7LampCb, &afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxMap::UpdateCheckCommentaryIconActive(float afTimeStep)
{
	int lNewState = ohpl_a7_commentary_sync(mpA7, gpBase->mpMapHandler->GetShowCommentary());
	if(lNewState < 0) return;
	mbCommentaryIconsActive = lNewState != 0;

	//////////////////////////////
	//Go through all entities and set commentary icons with proper active value
	for(ohpl_a7::Walk<tLuxEntityList> entityIt(mpA7, kOhplA7Entities); entityIt.Has(); entityIt.Advance())
	{
		iLuxEntity *pEntity = entityIt.Get();
		if(pEntity->GetEntityType() == eLuxEntityType_CommentaryIcon)
		{
			pEntity->SetActive(mbCommentaryIconsActive);
		}
	}
}

//-----------------------------------------------------------------------





//-----------------------------------------------------------------------

// OHPL_A7_MAPDUMP: every entity in list order with its save data, plus the lookups the map answers.
void cLuxMap::DumpA7(const char *asFile, const char *asLabel)
{
	std::ofstream os(asFile, std::ios::app);
	tString sTmp = tString(asFile) + ".tmp";
	os << "== map " << msName << " " << asLabel << "\n";
	Log("A7 map dump '%s' %s\n", msName.c_str(), asLabel);

	int lIdx = 0;
	cLuxEntityIterator it = GetEntityIterator();
	while(it.HasNext())
	{
		iLuxEntity *pEntity = it.Next();
		os << lIdx++ << "|" << (int)pEntity->GetEntityType() << "|" << pEntity->GetName() << "|" << pEntity->GetID()
			<< "|" << pEntity->IsActive() << "|" << pEntity->GetDestroyMe()
			<< "|" << (GetEntityByName(pEntity->GetName()) == pEntity) << "|" << (GetEntityByID(pEntity->GetID()) == pEntity);
		if(pEntity->GetEntityType() == eLuxEntityType_Area) os << "|area" << (int)static_cast<iLuxArea*>(pEntity)->GetAreaType();
		if(pEntity->GetEntityType() == eLuxEntityType_Prop) os << "|prop" << (int)static_cast<iLuxProp*>(pEntity)->GetPropType();
		os << "|";
		iLuxEntity_SaveData *pData = pEntity->GetEntityType() == eLuxEntityType_Area ? pEntity->CreateSaveData() : NULL;
		if(pData)
		{
			pEntity->SaveToSaveData(pData);
			cSerializeClass::SaveToFile(pData, cString::To16Char(sTmp), "D");
			std::ifstream in(sTmp.c_str());
			std::string sLine;
			while(std::getline(in, sLine))
			{
				size_t lPos = sLine.find_first_not_of(" \t\r");
				if(lPos != std::string::npos) os << sLine.substr(lPos);
			}
			hplDelete(pData);
		}
		os << "\n";
	}

	os << "enemies:";
	cLuxEnemyIterator enemyIt = GetEnemyIterator();
	while(enemyIt.HasNext()) os << " " << enemyIt.Next()->GetName();
	os << "\nfreeid " << GetFreeEntityID() << " latest " << (GetLatestEntity() ? GetLatestEntity()->GetName() : tString("-")) << "\n";

	std::vector<cLuxEventTimer> vTimers;
	CopyTimers(vTimers);
	for(size_t i=0; i<vTimers.size(); ++i) os << "timer " << vTimers[i].msName << " " << vTimers[i].msFunction << " " << vTimers[i].mfCount << " " << vTimers[i].mbDestroyMe << "\n";
	std::vector<cLuxUseItemCallback> vCallbacks;
	CopyUseItemCallbacks(vCallbacks);
	for(size_t i=0; i<vCallbacks.size(); ++i) os << "useitem " << vCallbacks[i].msName << " " << vCallbacks[i].msItem << " " << vCallbacks[i].msEntity << " " << vCallbacks[i].msFunction << "\n";
	for(int i=0; i<GetPlayerStartNodeNum(); ++i) os << "start " << GetPlayerStartNode(i)->GetName() << " " << GetPlayerStartNode(i)->GetPosition().x << "\n";
	cLuxNode_PlayerStart *pFirst = GetFirstPlayerStart();
	os << "firststart " << (pFirst ? pFirst->GetName() : tString("-")) << "\n";
	for(tLuxScriptVarMapIt vit = m_mapVars.begin(); vit != m_mapVars.end(); ++vit) os << "var " << vit->first << "=" << vit->second.msVal << "\n";
	os << "doomed 0 deleting " << IsDeletingAllWorldEntities() << "\n";
}
