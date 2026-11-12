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

//////////////////////////////////////////////////////////////////////////
// DISSOLVE ENTITIES
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxDissolveEntity::cLuxDissolveEntity(cLuxMap *apMap)
{
	mpMap = apMap;
}

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

	mpLatestAddedEntity = NULL;

	mpScript = NULL;

	msLanternLitCallback = "";

	mlNumberOfQuests = 0;
	mlTotalCompletionAmount = 0;
	mlCurrentCompletionAmount = 0;

	mbUpdatingTimers = false;

	mbDeletingAllWorldEntities = false;

	mbCommentaryIconsActive = false;
}

//-----------------------------------------------------------------------

cLuxMap::~cLuxMap()
{
	STLDeleteAll(mlstTimers);
	
	STLDeleteAll(mlstLampLightConnections);


	mbDeletingAllWorldEntities = true;
	STLDeleteAll(mlstEntities);
	mbDeletingAllWorldEntities = false;
	

	STLMapDeleteAll(m_mapPlayerStartNodes);
	STLMapDeleteAll(m_mapPosNodes);
	STLDeleteAll(mlstUseItemCallbacks);
	STLDeleteAll(mlstDissolveEntities);

	mpEngine->GetScene()->DestroyWorld(mpWorld);	

	if(mpScript)
		mpEngine->GetResources()->GetScriptManager()->Destroy(mpScript);
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
	for (tLuxEntityListIt entityIt = mlstEntities.begin(); entityIt != mlstEntities.end(); ++entityIt)
	{
		iLuxEntity *pEntity = *entityIt;

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
	tLuxEntityListIt entityIt = mlstEntities.begin();
	for(; entityIt != mlstEntities.end(); ++entityIt)
	{
		iLuxEntity *pEntity = *entityIt;

		pEntity->AfterWorldLoad();
	}
}

//-----------------------------------------------------------------------

void cLuxMap::OnEnter(bool abRunScript, bool abFirstTime)
{
	///////////////
	//On world load for all entities
	tLuxEntityListIt entityIt = mlstEntities.begin();
	for(; entityIt != mlstEntities.end(); ++entityIt)
	{
		iLuxEntity *pEntity = *entityIt;
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
	UpdateCheckCommentaryIconActive(afTimeStep);
    UpdateDissolveEntities(afTimeStep);
	UpdateTimers(afTimeStep);
	
	UpdateToBeDesotroyedEntities(true);	

	////////////////////////////////////
	// Iterate entities
	tLuxEntityListIt entityIt = mlstEntities.begin();
	for(; entityIt != mlstEntities.end(); ++entityIt)
	{
		iLuxEntity *pEntity = *entityIt;

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

	tLuxEntityListIt entityIt = mlstEntities.begin();
	for(; entityIt != mlstEntities.end(); ++entityIt)
	{
		iLuxEntity *pEntity = *entityIt;

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
		cLuxMusic *pMusic = gpBase->mpMusicHandler->GetMusic(i);
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
	tLuxEnemyListIt it = mlstEnemies.begin(); 
	for(; it != mlstEnemies.end(); ++it)
	{
		iLuxEnemy *pEnemy = *it;
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
	STLDeleteAll(mlstTimers);

	STLDeleteAll(mlstLampLightConnections);//Since these depend on entities, destroy...

	mbDeletingAllWorldEntities = true;
	STLDeleteAll(mlstEntities);
	mbDeletingAllWorldEntities = false;
	
	m_mapEntitiesByID.clear();
	m_mapEntitiesByName.clear();
	mlstToBeDestroyedEntities.clear();
	mpLatestAddedEntity = NULL;
	mlstEnemies.clear();
	mlstStickyAreas.clear();

	mbCommentaryIconsActive = false;//Can reset this since all commentary icons are destroyed
}

//-----------------------------------------------------------------------

void cLuxMap::AddEntity(iLuxEntity *apEntity)
{
	m_mapEntitiesByName.insert(tLuxEntityNameMap::value_type(cString::ToLowerCase(apEntity->GetName()), apEntity));
	m_mapEntitiesByID.insert(tLuxEntityIDMap::value_type(apEntity->GetID(), apEntity));

	mlstEntities.push_back(apEntity);
	mpLatestAddedEntity = apEntity;

	bool bIsArea = apEntity->GetEntityType() == eLuxEntityType_Area;
	int lLists = ohpl_ga5_entity_lists(	apEntity->GetEntityType() == eLuxEntityType_Enemy, bIsArea,
										bIsArea && static_cast<iLuxArea*>(apEntity)->GetAreaType() == eLuxAreaType_Sticky);
	if(lLists & kOhplGa5ListEnemy)
	{
		iLuxEnemy *pEnemy = static_cast<iLuxEnemy*>(apEntity);
		mlstEnemies.push_back(pEnemy);
	}
	else if(lLists & kOhplGa5ListSticky)
	{
		mlstStickyAreas.push_back(static_cast<cLuxArea_Sticky*>(static_cast<iLuxArea*>(apEntity)));
	}
}

void cLuxMap::DestroyEntity(iLuxEntity *apEntity)
{
	if(mbDeletingAllWorldEntities) Warning("Trying to delete entity '%s' when Map is destroying all entities!\n", apEntity->GetName().c_str());

	if(apEntity->GetDestroyMe()) return;

	apEntity->DestroyMe();
    mlstToBeDestroyedEntities.push_back(apEntity);
}

//-----------------------------------------------------------------------

iLuxEntity *cLuxMap::GetEntityByName(const tString& asName, eLuxEntityType aType, int alSubType)
{
	tLuxEntityNameMapIt it = m_mapEntitiesByName.find(cString::ToLowerCase(asName));
	if(it == m_mapEntitiesByName.end()) return NULL;

	iLuxEntity *pEntity = it->second;
	if(LuxIsCorrectType(pEntity, aType, alSubType)== false) return NULL;

	return pEntity;
}

iLuxEntity *cLuxMap::GetEntityByID(int alID, eLuxEntityType aType, int alSubType)
{
	tLuxEntityIDMapIt it = m_mapEntitiesByID.find(alID);
	if(it == m_mapEntitiesByID.end()) return NULL;

	iLuxEntity *pEntity = it->second;

	if(LuxIsCorrectType(pEntity, aType, alSubType)== false) return NULL;

	return pEntity;
}

//-----------------------------------------------------------------------

bool cLuxMap::EntityExists(iLuxEntity *apEntity)
{
	tLuxEntityListIt entityIt = mlstEntities.begin();
	for(; entityIt != mlstEntities.end(); ++entityIt)
	{
		iLuxEntity *pEntity = *entityIt;
		if(pEntity == apEntity) return true; 
	}

	return false;
}

//-----------------------------------------------------------------------

cLuxEntityIterator cLuxMap::GetEntityIterator()
{
	return cLuxEntityIterator(&mlstEntities);
}

//-----------------------------------------------------------------------

cLuxEnemyIterator cLuxMap::GetEnemyIterator()
{
	return cLuxEnemyIterator(&mlstEnemies);
}

//-----------------------------------------------------------------------

void cLuxMap::BroadcastEnemyMessage(eLuxEnemyMessage aType, bool abHasPosition, const cVector3f& avPos, float afRadius,
									float afTime, bool abLocalScope, const cVector3f& avX,float afX, int alX)
{
	tLuxEnemyListIt it = mlstEnemies.begin();
	for(; it != mlstEnemies.end(); ++it)
	{
		iLuxEnemy *pEnemy = *it;
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


int cLuxMap::GetInRangeEnemyNum()
{
	int lNum =0;
	cLuxEnemyIterator it =GetEnemyIterator();
	while(it.HasNext())
	{
		iLuxEnemy *pEnemy = it.Next();
		if(pEnemy->IsActive()==false) continue;
		if(pEnemy->GetPlayerInRange()==false) continue;
		
		++lNum;
	}

	return lNum;
}

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

bool cLuxMap::DoorIsClosed(int alID)
{
	iLuxEntity *pEntity = GetEntityByID(alID);
	if(pEntity==NULL) return false;
	if(pEntity->GetEntityType() != eLuxEntityType_Prop) return false;

	iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
	if(pProp->GetPropType() != eLuxPropType_SwingDoor) return false;

	cLuxProp_SwingDoor *pDoor = static_cast<cLuxProp_SwingDoor*>(pProp);
	return pDoor->GetClosed();	
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
	for(tLuxArea_StickyListIt it = mlstStickyAreas.begin(); it != mlstStickyAreas.end(); ++it)
	{
		cLuxArea_Sticky *pStickyArea = *it;

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
	for(tLuxArea_StickyListIt it = mlstStickyAreas.begin(); it != mlstStickyAreas.end(); ++it)
	{
		cLuxArea_Sticky *pStickyArea = *it;
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

void cLuxMap::DestroyAllRopes()
{
	/////////////////////////////
	//Put all ropes in temporary list
	tRopeEntityList lstRopes; 
	cRopeEntityIterator ropeIt = mpWorld->GetRopeEntityIterator();
	while(ropeIt.HasNext())
	{
		lstRopes.push_back(ropeIt.Next());
	}

	/////////////////////////////
	//Destroy ropes in list
	for(tRopeEntityListIt it = lstRopes.begin(); it != lstRopes.end(); ++it)
	{
		cRopeEntity *pRope = *it;

        mpWorld->DestroyRopeEntity(pRope);
		mpPhysicsWorld->DestroyRope(pRope->GetPhysicsRope());
	}
}

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
	m_mapPlayerStartNodes.insert(tLuxPlayerStartMap::value_type(apNode->GetName(), apNode));
	mvPlayerStartNodes.push_back(apNode);
}

cLuxNode_PlayerStart * cLuxMap::GetPlayerStart(const tString & asName)
{
	tLuxPlayerStartMapIt it = m_mapPlayerStartNodes.find(asName);
	if(it == m_mapPlayerStartNodes.end()){
		Warning("Could not find player start node '%s'\n", asName.c_str());
		return NULL;
	}

	return it->second;
}

//-----------------------------------------------------------------------

cLuxNode_PlayerStart *cLuxMap::GetFirstPlayerStart()
{
	tLuxPlayerStartMapIt it = m_mapPlayerStartNodes.begin();
	if(it == m_mapPlayerStartNodes.end()) return NULL;

	return it->second;
}

//-----------------------------------------------------------------------

void cLuxMap::AddPosNode(cLuxNode_Pos *apNode)
{
	m_mapPosNodes.insert(tLuxPosNodeMap::value_type(apNode->GetName(), apNode));
}

cLuxNode_Pos *cLuxMap::GetPosNode(const tString & asName)
{
	tLuxPosNodeMapIt it = m_mapPosNodes.find(asName);
	if(it == m_mapPosNodes.end()){
		Warning("Could not find pos node '%s'\n", asName.c_str());
		return NULL;
	}

	return it->second;
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
	
	mlstUseItemCallbacks.push_back(pCallback);
}

void cLuxMap::RemoveUseItemCallback( const tString& asName)
{
	tLuxUseItemCallbackListIt it = mlstUseItemCallbacks.begin();
	for(; it != mlstUseItemCallbacks.end(); ++it)
	{
		cLuxUseItemCallback *pCallback = *it;

		if(pCallback->msName == asName)
		{
			hplDelete(pCallback);
			mlstUseItemCallbacks.erase(it);
			return;
		}
	}

	Warning("Could not find use item callback '%s'!\n", asName.c_str());
}

void cLuxMap::RemoveUseItemCallback(cLuxUseItemCallback * apCallback, const tString& asName)
{
	tLuxUseItemCallbackListIt it = mlstUseItemCallbacks.begin();
	for(; it != mlstUseItemCallbacks.end(); ++it)
	{
		cLuxUseItemCallback *pCallback = *it;
		
		if(pCallback == apCallback)
		{
			//Safety check that the callback really is the same!
			if(ohpl_ga5_callback_remove_ok(asName.data(), asName.size(), pCallback->msName.data(), pCallback->msName.size())==false)
			{
				break;
			}

			hplDelete(apCallback);
			mlstUseItemCallbacks.erase(it);
			break;
		}
	}
}

cLuxUseItemCallback* cLuxMap::GetUseItemCallback(const tString& asItem, const tString& asEntity)
{
	tLuxUseItemCallbackListIt it = mlstUseItemCallbacks.begin();
	for(; it != mlstUseItemCallbacks.end(); ++it)
	{
		cLuxUseItemCallback *pCallback = *it;
		//Log("Checking '%s' n '%s' vs callback: '%s' '%s' '%s' '%s'\n", 
		//	asItem.c_str(), asEntity.c_str(), 
		//	pCallback->msName.c_str(), pCallback->msEntity.c_str(), pCallback->msItem.c_str(), pCallback->msFunction.c_str());


        if(ohpl_ga5_use_item_matches(	pCallback->msItem.data(), pCallback->msItem.size(), pCallback->msEntity.data(), pCallback->msEntity.size(),
										asItem.data(), asItem.size(), asEntity.data(), asEntity.size()))
		{
			return pCallback;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------

void cLuxMap::AddTimer(const tString& asName, float afTime, const tString& asFunction)
{
	cLuxEventTimer *pTimer = hplNew( cLuxEventTimer, ());
	pTimer->msName = asName;
	pTimer->mfCount = ohpl_ga5_timer_count(afTime); //Not allow 0 or lower for time!
	pTimer->msFunction = asFunction;
	pTimer->mbDestroyMe = false;

	mlstTimers.push_back(pTimer);
}

//-----------------------------------------------------------------------

void cLuxMap::RemoveTimer(const tString& asName)
{
	for(tLuxEventTimerListIt it= mlstTimers.begin(); it != mlstTimers.end(); )
	{
		cLuxEventTimer *pTimer = *it;
		if(pTimer->msName == asName)
		{
			if(ohpl_ga5_timer_remove_marks(mbUpdatingTimers))
			{
				pTimer->mbDestroyMe = true;
				++it;
			}
			else
			{
				it = mlstTimers.erase(it);
				hplDelete(pTimer);
				
			}
			
		}
		else
		{
			++it;
		}
	}
}

//-----------------------------------------------------------------------

cLuxEventTimer* cLuxMap::GetTimer(const tString& asName)
{
	for(tLuxEventTimerListIt it= mlstTimers.begin(); it != mlstTimers.end();++it)
	{
		cLuxEventTimer *pTimer = *it;
		if(pTimer->msName == asName)
		{
			return pTimer;
		}
	}

	return NULL;
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

	
	mlstDissolveEntities.push_back(pDissolveEnt);
}

//-----------------------------------------------------------------------

cLuxLampLightConnection* cLuxMap::AddLampLightConnection(cLuxProp_Lamp *apLamp, iLight *apLight, float afAmount, bool abUseOnColor, bool abUseSpec)
{
	cLuxLampLightConnection *pConnection = GetLampLightConnection(apLight);
	if(pConnection == NULL)
	{
		pConnection = hplNew(cLuxLampLightConnection, (apLight));
		mlstLampLightConnections.push_back(pConnection);
	}
    
	pConnection->AddLamp(apLamp, afAmount, abUseOnColor, abUseSpec);
	pConnection->Update(0.001f);
	return pConnection;
}

//-----------------------------------------------------------------------

cLuxLampLightConnection* cLuxMap::GetLampLightConnection(iLight *apLight)
{
	tLuxLampLightConnectionListIt it = mlstLampLightConnections.begin();
	for(; it != mlstLampLightConnections.end(); ++it)
	{
		cLuxLampLightConnection* pConnection = *it;
        if(pConnection->GetLight() ==apLight) return pConnection;
	}
	return NULL;
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
	int vValues[5] = {	gpBase->mpCompletionCountHandler->mlItemCompletionValue, gpBase->mpCompletionCountHandler->mlNoteCompletionValue,
						gpBase->mpCompletionCountHandler->mlDiaryCompletionValue, gpBase->mpCompletionCountHandler->mlChestCompletionValue,
						gpBase->mpCompletionCountHandler->mlFlashbackCompletionValue };

	////////////////
	//Quests
	mlTotalCompletionAmount = ohpl_ga5_quest_completion(mlTotalCompletionAmount, mlNumberOfQuests, gpBase->mpCompletionCountHandler->mlQuestCompletionValue);

	////////////////
	//Entities
	for(tLuxEntityListIt entityIt = mlstEntities.begin(); entityIt != mlstEntities.end(); ++entityIt)
	{
		iLuxEntity *pEntity = *entityIt;
		eLuxEntityType entityType = pEntity->GetEntityType();
		int lKind = kOhplGa5CompleteNone;

		/////////////////////////
		// Prop
        if(entityType == eLuxEntityType_Prop)
		{
			iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
            eLuxPropType propType = pProp->GetPropType();

			/////////////////////////
			// Item
			if(propType == eLuxPropType_Item)
			{
				cLuxProp_Item *pItem = static_cast<cLuxProp_Item*>(pProp);
				eLuxItemType itemType = pItem->GetItemType();

                if(itemType == eLuxItemType_Puzzle) lKind = kOhplGa5CompletePuzzle;
				if(itemType == eLuxItemType_Note) lKind = kOhplGa5CompleteNote;
				if(itemType == eLuxItemType_Diary) lKind = kOhplGa5CompleteDiary;
			}
			/////////////////////////
			// Chest
			else if(propType == eLuxPropType_Chest)
			{
				lKind = kOhplGa5CompleteChest;
			}
		}
		/////////////////////////
		// Area
		else if(entityType == eLuxEntityType_Area)
		{
			iLuxArea *pArea = static_cast<iLuxArea*>(pEntity);
			eLuxAreaType areaType = pArea->GetAreaType();

            if(areaType == eLuxAreaType_Flashback) lKind = kOhplGa5CompleteFlashback;
		}

		mlTotalCompletionAmount += ohpl_ga5_completion_value(lKind, vValues);
	}

	gpBase->mpDebugHandler->AddMessage(_W("Total completion value: ")+cString::ToStringW(mlTotalCompletionAmount), false);
}

//-----------------------------------------------------------------------

int cLuxMap::GetFreeEntityID()
{
	//Find the lowest id not used in the id map (ascending)
	std::vector<int> vIds;
	for(tLuxEntityIDMapIt it = m_mapEntitiesByID.begin(); it != m_mapEntitiesByID.end(); ++it)
	{
		vIds.push_back(it->first);
	}

	return ohpl_ga5_free_entity_id(vIds.data(), vIds.size());
}

//-----------------------------------------------------------------------

void cLuxMap::UpdateToBeDesotroyedEntities(bool abUseCallbacks)
{
	tLuxEntityListIt entityIt = mlstToBeDestroyedEntities.begin();
	for(; entityIt != mlstToBeDestroyedEntities.end(); ++entityIt)
	{
		iLuxEntity *pEntity = *entityIt;

		if(abUseCallbacks)
		{
			gpBase->mpPlayer->GetCurrentStateData()->OnDestroyEntity(pEntity);
			pEntity->BeforeEntityDestruction();
		}

		STLFindAndRemove(mlstEntities, pEntity);
		STLMapFindAndRemove(m_mapEntitiesByName, pEntity);
		STLMapFindAndRemove(m_mapEntitiesByID, pEntity);

		bool bIsArea = pEntity->GetEntityType() == eLuxEntityType_Area;
		int lLists = ohpl_ga5_entity_lists(	pEntity->GetEntityType() == eLuxEntityType_Enemy, bIsArea,
											bIsArea && static_cast<iLuxArea*>(pEntity)->GetAreaType() == eLuxAreaType_Sticky);

		//Extra remove for enemies
		if(lLists & kOhplGa5ListEnemy)
		{
			iLuxEnemy *pEnemy = static_cast<iLuxEnemy*>(pEntity);
			STLFindAndRemove(mlstEnemies, pEnemy);
		}
		//Extra remove for sticky areas
		else if(lLists & kOhplGa5ListSticky)
		{
			STLFindAndRemove(mlstStickyAreas, static_cast<cLuxArea_Sticky*>(static_cast<iLuxArea*>(pEntity)));
		}

		hplDelete(pEntity);
	}

	mlstToBeDestroyedEntities.clear();
}
//-----------------------------------------------------------------------

void cLuxMap::UpdateTimers(float afTimeStep)
{
	mbUpdatingTimers = true;

	//////////////////////
	// Update time (do this here so a timer callback is not called in the same loop as it is created)
	for(tLuxEventTimerListIt it= mlstTimers.begin(); it != mlstTimers.end(); ++it)
	{
		cLuxEventTimer *pTimer = *it;
		pTimer->mfCount = ohpl_ga5_timer_step(pTimer->mfCount, afTimeStep);
	}

	//////////////////////
	// See if any timers should be deleted / called
	for(tLuxEventTimerListIt it= mlstTimers.begin(); it != mlstTimers.end(); )
	{
		cLuxEventTimer *pTimer = *it;

		if(ohpl_ga5_timer_due(pTimer->mfCount, pTimer->mbDestroyMe))
		{
			RunScript(ohpl_ga5::Call1(pTimer->msFunction, pTimer->msName));
			it = mlstTimers.erase(it);
			hplDelete(pTimer);
			
		}
		else
		{
			++it;
		}
	}
	
	for(tLuxEventTimerListIt it= mlstTimers.begin(); it != mlstTimers.end(); )
	{
		cLuxEventTimer *pTimer = *it;
		if(pTimer->mbDestroyMe)
		{
			it = mlstTimers.erase(it);
			hplDelete(pTimer);
			
		}
		else
		{
			++it;
		}
	}

	mbUpdatingTimers = false;
}

//-----------------------------------------------------------------------

void cLuxMap::UpdateDissolveEntities(float afTimeStep)
{
	tLuxDissolveEntityListIt it = mlstDissolveEntities.begin();
	for(; it != mlstDissolveEntities.end(); )
	{
		cLuxDissolveEntity *pEntity = *it;

        bool bRemove = ohpl_ga5_dissolve_step(&pEntity->mfAlpha, pEntity->mfFadeSpeed, afTimeStep);
		pEntity->mpEntity->SetCoverageAmount(pEntity->mfAlpha);

		if(bRemove)
		{
			it = mlstDissolveEntities.erase(it);
			hplDelete(pEntity);
		}
		else
		{
			++it;
		}
	}
}

//-----------------------------------------------------------------------

void cLuxMap::UpdateLampLightConnections(float afTimeStep)
{
	tLuxLampLightConnectionListIt it = mlstLampLightConnections.begin();
	for(; it != mlstLampLightConnections.end(); ++it)
	{
		cLuxLampLightConnection* pConnection = *it;
		
		pConnection->Update(afTimeStep);
	}
}

//-----------------------------------------------------------------------

void cLuxMap::UpdateCheckCommentaryIconActive(float afTimeStep)
{
	int lNewState = ohpl_ga5_commentary_sync(mbCommentaryIconsActive, gpBase->mpMapHandler->GetShowCommentary());
	if(lNewState < 0) return;
	mbCommentaryIconsActive = lNewState != 0;

	//////////////////////////////
	//Go through all entities and set commentary icons with proper active value
	tLuxEntityListIt entityIt = mlstEntities.begin();
	for(; entityIt != mlstEntities.end(); ++entityIt)
	{
		iLuxEntity *pEntity = *entityIt;
		if(pEntity->GetEntityType() == eLuxEntityType_CommentaryIcon)
		{
			pEntity->SetActive(mbCommentaryIconsActive);
		}
	}
}

//-----------------------------------------------------------------------



