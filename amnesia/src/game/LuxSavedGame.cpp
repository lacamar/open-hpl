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

#include "LuxSavedGame.h"
#include "RustLuxSave.h"
#include <vector>

#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxEnemy.h"
#include "LuxEnemyPathfinder.h"
#include "LuxProp_SwingDoor.h"
#include "LuxProp_LevelDoor.h"
#include "LuxProp_Item.h"
#include "LuxProp_Object.h"
#include "LuxProp_Chest.h"
#include "LuxProp_Lamp.h"
#include "LuxPlayer.h"
#include "LuxInteractConnections.h"

//////////////////////////////////////////////////////////////////////////
// ENTITY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxSavedGameEntity::~cLuxSavedGameEntity()
{
	for(size_t i=0; i<mvInteractConnections.Size(); ++i)
	{
		iLuxInteractConnection_SaveData *pSaveConn = mvInteractConnections[i];
		hplDelete(pSaveConn);
	}
}

//-----------------------------------------------------------------------

namespace
{

struct EntityEnv
{
	cLuxSavedGameEntity *mpData;
	iLuxEntity *mpEntity;
	cLuxMap *mpMap;
	iLuxInteractConnection *mpConn;
};

}

int cLuxSavedGameEntity::Ga6FromCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	EntityEnv &e = *static_cast<EntityEnv*>(apCtx);
	cLuxSavedGameEntity &d = *e.mpData;
	iLuxEntity *apEntity = e.mpEntity;
	switch(alOp)
	{
	case eOhplGa6Op_GeFromId:
		d.mlID = apEntity->GetID();
		return 0;
	case eOhplGa6Op_GeIsProp:
		return apEntity->GetEntityType() == eLuxEntityType_Prop;
	case eOhplGa6Op_GeFromProp:
		{
			iLuxProp *pProp = static_cast<iLuxProp*>(apEntity);
			tLuxProp_AttachedPropListIt attachIt = pProp->mlstAttachedProps.begin();
			for(; attachIt != pProp->mlstAttachedProps.end(); ++attachIt)
			{
				cLuxProp_AttachedProp attachProp = *(*attachIt);
				attachProp.mpProp = NULL;
				Log("Saving attached prop: '%s' '%s' (%s)\n", attachProp.msName.c_str(), attachProp.msFileName.c_str(), attachProp.m_mtxOffset.ToString().c_str());
				d.mvAttachedProps.Add(attachProp);
			}
			for(size_t i=0; i< pProp->mvInteractConnections.size(); ++i)
			{
				iLuxInteractConnection *pConn = pProp->mvInteractConnections[i];
				iLuxInteractConnection_SaveData *pSaveConn = pConn->CreateSaveData();
				pSaveConn->FromConnection(pConn);
				d.mvInteractConnections.Add(pSaveConn);
			}
		}
		return 0;
	case eOhplGa6Op_GeFromConns:
		d.mvConnections.Resize(apEntity->mvConnections.size());
		for(size_t i=0; i<apEntity->mvConnections.size();++i)
		{
			d.mvConnections[i].FromConnection(apEntity->mvConnections[i]);
		}
		return 0;
	case eOhplGa6Op_GeFromCollide:
		for(tLuxCollideCallbackListIt it = apEntity->mlstCollideCallbacks.begin(); it != apEntity->mlstCollideCallbacks.end(); ++it)
		{
			cLuxCollideCallback *pCallback = *it;
			cLuxCollideCallback_SaveData saveCallback{};
			saveCallback.FromCallback(pCallback);
			d.mlstCollideCallbacks.Add(saveCallback);
		}
		return 0;
	}
	return 0;
}

int cLuxSavedGameEntity::Ga6ToCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	EntityEnv &e = *static_cast<EntityEnv*>(apCtx);
	cLuxSavedGameEntity &d = *e.mpData;
	iLuxEntity *apEntity = e.mpEntity;
	cLuxMap *apMap = e.mpMap;
	switch(alOp)
	{
	case eOhplGa6Op_GeIsProp:
		return apEntity->GetEntityType() == eLuxEntityType_Prop;
	case eOhplGa6Op_GeToAttached:
		{
			iLuxProp *pProp = static_cast<iLuxProp*>(apEntity);
			for(size_t i=0; i<d.mvAttachedProps.Size(); ++i)
			{
				cLuxProp_AttachedProp &attachProp = d.mvAttachedProps[i];
				Log("Loading attached prop: '%s' '%s' (%s)\n", attachProp.msName.c_str(), attachProp.msFileName.c_str(), attachProp.m_mtxOffset.ToString().c_str());
				pProp->AddAndAttachProp(attachProp.msName, attachProp.msFileName, attachProp.m_mtxOffset);
			}
		}
		return 0;
	case eOhplGa6Op_GeSavedInteractN:
		return (int)d.mvInteractConnections.Size();
	case eOhplGa6Op_GeCreateInteract:
		e.mpConn = d.mvInteractConnections[alA]->CreateConnection(apMap);
		return e.mpConn != NULL;
	case eOhplGa6Op_GeAddInteract:
		static_cast<iLuxProp*>(apEntity)->mvInteractConnections.push_back(e.mpConn);
		return 0;
	case eOhplGa6Op_GeToConns:
		apEntity->mvConnections.resize(d.mvConnections.Size());
		for(size_t i=0; i<d.mvConnections.Size();++i)
		{
			apEntity->mvConnections[i] = hplNew( cLuxEntityConnection, () );
			d.mvConnections[i].ToConnection(apEntity->mvConnections[i], apMap);
		}
		return 0;
	case eOhplGa6Op_GeToCollide:
		{
			cContainerListIterator<cLuxCollideCallback_SaveData> it = d.mlstCollideCallbacks.GetIterator();
			while(it.HasNext())
			{
				cLuxCollideCallback_SaveData& saveCallback = it.Next();
				cLuxCollideCallback *pCallback = hplNew(cLuxCollideCallback, ());
				saveCallback.ToCallback(apMap, apEntity, pCallback);
				apEntity->mlstCollideCallbacks.push_back(pCallback);
			}
		}
		return 0;
	}
	return 0;
}

namespace
{

struct EnemyEnv
{
	cLuxSavedGameEnemy *mpData;
	iLuxEnemy *mpEnemy;
	cAINode *mpNode;
};

int LuxEntityCode(iLuxEntity *apEntity)
{
	switch(apEntity->GetEntityType())
	{
	case eLuxEntityType_Prop: return eOhplGa6Ent_Prop;
	case eLuxEntityType_Enemy: return eOhplGa6Ent_Enemy;
	default: break;
	}
	return eOhplGa6Ent_Other;
}

int LuxPropCode(iLuxProp *apProp)
{
	switch(apProp->GetPropType())
	{
	case eLuxPropType_Object: return eOhplGa6Prop_Object;
	case eLuxPropType_SwingDoor: return eOhplGa6Prop_SwingDoor;
	case eLuxPropType_Lamp: return eOhplGa6Prop_Lamp;
	case eLuxPropType_Item: return eOhplGa6Prop_Item;
	case eLuxPropType_Chest: return eOhplGa6Prop_Chest;
	case eLuxPropType_LevelDoor: return eOhplGa6Prop_LevelDoor;
	default: break;
	}
	return eOhplGa6Prop_Other;
}

}

int cLuxSavedGameEnemy::Ga6ToCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	EnemyEnv &e = *static_cast<EnemyEnv*>(apCtx);
	cLuxSavedGameEnemy &d = *e.mpData;
	iLuxEnemy *apEnemy = e.mpEnemy;
	switch(alOp)
	{
	case eOhplGa6Op_GenToHead:
		apEnemy->SetActive(d.mbActive);
		apEnemy->ClearPatrolNodes();
		return 0;
	case eOhplGa6Op_GenHasPathFinder:
		return apEnemy->GetPathFinder() != NULL;
	case eOhplGa6Op_GenSavedNodeN:
		return (int)d.mvPatrolNodes.Size();
	case eOhplGa6Op_GenFindNode:
		e.mpNode = apEnemy->GetPathFinder()->GetNodeContainer()->GetNodeFromID(d.mvPatrolNodes[alA].mlNodeId);
		return e.mpNode != NULL;
	case eOhplGa6Op_GenAddNode:
		apEnemy->AddPatrolNode(e.mpNode, d.mvPatrolNodes[alA].mfWaitTime, d.mvPatrolNodes[alA].msAnimation);
		return 0;
	}
	return 0;
}


void cLuxSavedGameEntity::FromEntity(iLuxEntity *apEntity)
{
	EntityEnv env = { this, apEntity, NULL, NULL };
	ohpl_ga6_entity_from(&env, cLuxSavedGameEntity::Ga6FromCall);
}

//-----------------------------------------------------------------------

void cLuxSavedGameEntity::ToEntity(cLuxMap *apMap,iLuxEntity *apEntity)
{
	EntityEnv env = { this, apEntity, apMap, NULL };
	ohpl_ga6_entity_to(&env, cLuxSavedGameEntity::Ga6ToCall);
}

//////////////////////////////////////////////////////////////////////////
// ENEMY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxSavedGameEnemy::FromEnemy(iLuxEnemy *apEnemy)
{
	mlID = apEnemy->GetID();
	mbActive = false; //apEnemy->IsActive();
	if(apEnemy->GetHealth() <=0) mbActive = false;

	///////////////////////
	//Patrol nodes
	mvPatrolNodes.Resize(apEnemy->GetPatrolNodeNum());
	for(size_t i=0; i<mvPatrolNodes.Size(); ++i)
	{
		mvPatrolNodes[i].mlNodeId = 	apEnemy->GetPatrolNode(i)->mpNode->GetID();
		mvPatrolNodes[i].mfWaitTime = 	apEnemy->GetPatrolNode(i)->mfWaitTime;
		mvPatrolNodes[i].msAnimation = 	apEnemy->GetPatrolNode(i)->msAnimation;
	}
}

//-----------------------------------------------------------------------

void cLuxSavedGameEnemy::ToEnemy(cLuxMap *apMap, iLuxEnemy *apEnemy)
{
	EnemyEnv env = { this, apEnemy, NULL };
	ohpl_ga6_enemy_to(&env, cLuxSavedGameEnemy::Ga6ToCall);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// SAVED MAP
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxSavedGameMap::cLuxSavedGameMap()
{

}

cLuxSavedGameMap::~cLuxSavedGameMap()
{
    DestroyAll();
}

//-----------------------------------------------------------------------

void cLuxSavedGameMap::DestroyAll()
{
	///////////////////
	//Full Entities
	{
		cContainerListIterator<iLuxEntity_SaveData*> it = mlstFullEntities.GetIterator();
		while(it.HasNext())
		{
			hplDelete(it.Next());
		}
		mlstFullEntities.Clear();
	}


	///////////////////
	// Connect and Collide Entities
	{
		cContainerListIterator<cLuxSavedGameEntity*> it = mlstCollideAndConnectEntities.GetIterator();
		while(it.HasNext())
		{
			hplDelete(it.Next());
		}
		mlstCollideAndConnectEntities.Clear();
	}

	///////////////////
	// Enemies
	{
		cContainerListIterator<cLuxSavedGameEnemy*> it = mlstEnemies.GetIterator();
		while(it.HasNext())
		{
			hplDelete(it.Next());
		}
		mlstEnemies.Clear();
	}

	///////////////////
	//Other
	mlstVars.Clear();
	mlstTimers.Clear();
	mvUseItemCallbacks.Clear();

	mlstPlayerCollideCallbacks.Clear();

	mlstEntityFunc_Look.Clear();
	mlstEntityFunc_Interact.Clear();
	mlstEntityFunc_Callback.Clear();
	mlstEntityFunc_StateChange.Clear();

	mlstSounds.Clear();
	mlstLights.Clear();
	mlstPS.Clear();

	mlstDisabledEntities.Clear();
	mlstLockedDoors.Clear();
	mlstLockedLevelDoors.Clear();
	mlstActiveItems.Clear();
	mlstOpenChests.Clear();
	mlstUnbrokenItemContainers.Clear();
	mlstBrokenContainersWithActiveItem.Clear();
	mlstUnlitLamps.Clear();
}

//-----------------------------------------------------------------------

namespace
{

struct GmFromEnv
{
	cLuxSavedGameMap *mpData;
	cLuxMap *mpMap;
	cWorld *mpWorld;
	std::vector<iLight*> mvLights;
	std::vector<cSoundEntity*> mvSounds;
	std::vector<cParticleSystem*> mvPS;
	std::vector<iLuxEntity*> mvEntities;
};

iLuxProp *GmProp(GmFromEnv &e, int alI)
{
	return static_cast<iLuxProp*>(e.mvEntities[alI]);
}

}

int cLuxSavedGameMap::Ga6FromCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	GmFromEnv &e = *static_cast<GmFromEnv*>(apCtx);
	cLuxSavedGameMap &d = *e.mpData;
	cLuxMap *apMap = e.mpMap;
	cWorld *pWorld = e.mpWorld;
	switch(alOp)
	{
	case eOhplGa6Op_GmFromHead:
		d.msName = apMap->GetName();
		d.msDisplayNameEntry = apMap->GetDisplayNameEntry();
		d.mlNumberOfQuests = apMap->mlNumberOfQuests;
		d.mlTotalCompletionAmount = apMap->mlTotalCompletionAmount;
		d.mlCurrentCompletionAmount = apMap->mlCurrentCompletionAmount;
		d.mbSkyBoxActive = pWorld->GetSkyBoxActive();
		d.msSkyboxTexture = pWorld->GetSkyBoxTexture() ? pWorld->GetSkyBoxTexture()->GetName() : "";
		d.mSkyBoxColor = pWorld->GetSkyBoxColor();
		d.mbFogActive = pWorld->GetFogActive();
		d.mbFogCulling = pWorld->GetFogCulling();
		d.mfFogStart = pWorld->GetFogStart();
		d.mfFogEnd = pWorld->GetFogEnd();
		d.mfFogFalloffExp = pWorld->GetFogFalloffExp();
		d.mFogColor = pWorld->GetFogColor();
		{
			tLuxScriptVarMapIt it = apMap->m_mapVars.begin();
			for(; it != apMap->m_mapVars.end(); ++it)
			{
				d.mlstVars.Add(it->second);
			}
		}
		{
			std::vector<cLuxUseItemCallback> vCallbacks;
			apMap->CopyUseItemCallbacks(vCallbacks);
			for(size_t i=0; i<vCallbacks.size(); ++i) d.mvUseItemCallbacks.Add(vCallbacks[i]);
		}
		{
			std::vector<cLuxEventTimer> vTimers;
			apMap->CopyTimers(vTimers);
			for(size_t i=0; i<vTimers.size(); ++i) d.mlstTimers.Add(vTimers[i]);
		}
		{
			tLuxCollideCallbackList *pCallbackList = gpBase->mpPlayer->GetCollideCallbackList();
			for(tLuxCollideCallbackListIt it = pCallbackList->begin(); it != pCallbackList->end(); ++it)
			{
				cLuxCollideCallback *pCallback = *it;
				cLuxCollideCallback_SaveData saveCallback{};
				saveCallback.FromCallback(pCallback);
				d.mlstPlayerCollideCallbacks.Add(saveCallback);
			}
		}
		return 0;
	case eOhplGa6Op_MapLightN:
		{
			cLightListIterator lightIt = pWorld->GetLightIterator();
			while(lightIt.HasNext()) e.mvLights.push_back(lightIt.Next());
		}
		return (int)e.mvLights.size();
	case eOhplGa6Op_MapLightFlags:
		{
			iLight *pLight = e.mvLights[alA];
			return (pLight->IsSaved() ? eOhplGa6Flag_Saved : 0) |
				(pLight->GetEntityParent() == NULL ? eOhplGa6Flag_NoEntityParent : 0) |
				(pLight->GetParent() == NULL ? eOhplGa6Flag_NoParent : 0);
		}
	case eOhplGa6Op_MapLightFrom:
		{
			cEngineLight_SaveData saveLight{};
			saveLight.FromLight(e.mvLights[alA]);
			d.mlstLights.Add(saveLight);
		}
		return 0;
	case eOhplGa6Op_MapSoundN:
		{
			cSoundEntityIterator soundIt = pWorld->GetSoundEntityIterator();
			while(soundIt.HasNext()) e.mvSounds.push_back(soundIt.Next());
		}
		return (int)e.mvSounds.size();
	case eOhplGa6Op_MapSoundFlags:
		{
			cSoundEntity *pSound = e.mvSounds[alA];
			return (pSound->IsSaved() ? eOhplGa6Flag_Saved : 0) |
				(pSound->GetEntityParent() == NULL ? eOhplGa6Flag_NoEntityParent : 0) |
				(pSound->GetParent() == NULL ? eOhplGa6Flag_NoParent : 0);
		}
	case eOhplGa6Op_MapSoundFrom:
		{
			cEngineSound_SaveData saveSound{};
			saveSound.FromSound(e.mvSounds[alA]);
			d.mlstSounds.Add(saveSound);
		}
		return 0;
	case eOhplGa6Op_MapPsN:
		{
			cParticleSystemIterator psIt = pWorld->GetParticleSystemIterator();
			while(psIt.HasNext()) e.mvPS.push_back(psIt.Next());
		}
		return (int)e.mvPS.size();
	case eOhplGa6Op_MapPsFlags:
		{
			cParticleSystem *pPS = e.mvPS[alA];
			return (pPS->IsSaved() ? eOhplGa6Flag_Saved : 0) |
				(pPS->GetEntityParent() == NULL ? eOhplGa6Flag_NoEntityParent : 0) |
				(pPS->GetParent() == NULL ? eOhplGa6Flag_NoParent : 0);
		}
	case eOhplGa6Op_MapPsDying:
		return e.mvPS[alA]->IsDying();
	case eOhplGa6Op_MapPsFrom:
		{
			cEnginePS_SaveData savePS{};
			savePS.FromPS(e.mvPS[alA]);
			d.mlstPS.Add(savePS);
		}
		return 0;
	case eOhplGa6Op_GmSnapshot:
		e.mvEntities.clear();
		{
			cLuxEntityIterator it = apMap->GetEntityIterator();
			while(it.HasNext()) e.mvEntities.push_back(it.Next());
		}
		return 0;
	case eOhplGa6Op_GmEntN:
		return (int)e.mvEntities.size();
	case eOhplGa6Op_GmEntFullSave:
		return e.mvEntities[alA]->GetFullGameSave();
	case eOhplGa6Op_GmEntFullFrom:
		{
			iLuxEntity *pEntity = e.mvEntities[alA];
			iLuxEntity_SaveData *pSavedEntity = pEntity->CreateSaveData();
			pEntity->SaveToSaveData(pSavedEntity);
			d.mlstFullEntities.Add(pSavedEntity);
		}
		return 0;
	case eOhplGa6Op_GmEntHasCollide:
		return e.mvEntities[alA]->HasCollideCallbacks();
	case eOhplGa6Op_GmEntConnNum:
		return e.mvEntities[alA]->GetConnectionNum();
	case eOhplGa6Op_GmEntSaveEntity:
		{
			cLuxSavedGameEntity *pSavedEnt = hplNew(cLuxSavedGameEntity, () );
			pSavedEnt->FromEntity(e.mvEntities[alA]);
			d.mlstCollideAndConnectEntities.Add(pSavedEnt);
		}
		return 0;
	case eOhplGa6Op_GmEntHasFunc:
		{
			iLuxEntity *pEntity = e.mvEntities[alA];
			switch(alB)
			{
			case eOhplGa6Func_Look: return pEntity->msLookAtCallback != "";
			case eOhplGa6Func_Interact: return pEntity->msInteractCallback != "";
			case eOhplGa6Func_Callback: return pEntity->msCallbackFunc != "";
			case eOhplGa6Func_StateChange: return pEntity->msConnectionStateChangeCallback != "";
			}
		}
		return 0;
	case eOhplGa6Op_GmEntAddFunc:
		{
			iLuxEntity *pEntity = e.mvEntities[alA];
			switch(alB)
			{
			case eOhplGa6Func_Look:
				d.mlstEntityFunc_Look.Add(cLuxSavedGameEntityFunc(pEntity->GetID(), pEntity->msLookAtCallback, pEntity->mbLookAtCallbackRemove));
				break;
			case eOhplGa6Func_Interact:
				d.mlstEntityFunc_Interact.Add(cLuxSavedGameEntityFunc(pEntity->GetID(), pEntity->msInteractCallback, pEntity->mbInteractCallbackRemove));
				break;
			case eOhplGa6Func_Callback:
				d.mlstEntityFunc_Callback.Add(cLuxSavedGameEntityFunc(pEntity->GetID(), pEntity->msCallbackFunc, false));
				break;
			case eOhplGa6Func_StateChange:
				d.mlstEntityFunc_StateChange.Add(cLuxSavedGameEntityFunc(pEntity->GetID(), pEntity->msConnectionStateChangeCallback, false));
				break;
			}
		}
		return 0;
	case eOhplGa6Op_GmEntActive:
		return e.mvEntities[alA]->IsActive();
	case eOhplGa6Op_GmEntAddDisabled:
		d.mlstDisabledEntities.Add(e.mvEntities[alA]->GetID());
		return 0;
	case eOhplGa6Op_GmEntType:
		return LuxEntityCode(e.mvEntities[alA]);
	case eOhplGa6Op_GmEntSaveEnemy:
		{
			iLuxEnemy *pEnemy = static_cast<iLuxEnemy*>(e.mvEntities[alA]);
			cLuxSavedGameEnemy *pSavedEnemy = hplNew(cLuxSavedGameEnemy, () );
			pSavedEnemy->FromEnemy(pEnemy);
			d.mlstEnemies.Add(pSavedEnemy);
		}
		return 0;
	case eOhplGa6Op_GmPropHasAttached:
		return GmProp(e, alA)->HasAttachedProp();
	case eOhplGa6Op_GmPropInteractConnNum:
		return GmProp(e, alA)->GetInteractionConnectionNum();
	case eOhplGa6Op_GmPropType:
		return LuxPropCode(GmProp(e, alA));
	case eOhplGa6Op_GmLocked:
		{
			iLuxProp *pProp = GmProp(e, alA);
			switch(pProp->GetPropType())
			{
			case eLuxPropType_SwingDoor: return static_cast<cLuxProp_SwingDoor*>(pProp)->GetLocked();
			case eLuxPropType_LevelDoor: return static_cast<cLuxProp_LevelDoor*>(pProp)->GetLocked();
			case eLuxPropType_Chest: return static_cast<cLuxProp_Chest*>(pProp)->GetLocked();
			default: break;
			}
		}
		return 0;
	case eOhplGa6Op_GmItemContainerId:
		return static_cast<cLuxProp_Item*>(GmProp(e, alA))->GetSpawnContainerID();
	case eOhplGa6Op_GmObjectHasContained:
		return static_cast<cLuxProp_Object*>(GmProp(e, alA))->GetContainedItem() != "";
	case eOhplGa6Op_GmLampLit:
		return static_cast<cLuxProp_Lamp*>(GmProp(e, alA))->GetLit();
	case eOhplGa6Op_GmAddList:
		{
			iLuxEntity *pEntity = e.mvEntities[alA];
			switch(alB)
			{
			case eOhplGa6List_LockedDoors: d.mlstLockedDoors.Add(pEntity->GetID()); break;
			case eOhplGa6List_LockedLevelDoors: d.mlstLockedLevelDoors.Add(pEntity->GetID()); break;
			case eOhplGa6List_BrokenWithActiveItem:
				d.mlstBrokenContainersWithActiveItem.Add(static_cast<cLuxProp_Item*>(pEntity)->GetSpawnContainerID());
				break;
			case eOhplGa6List_ActiveItems: d.mlstActiveItems.Add(pEntity->GetID()); break;
			case eOhplGa6List_UnbrokenContainers: d.mlstUnbrokenItemContainers.Add(pEntity->GetID()); break;
			case eOhplGa6List_OpenChests: d.mlstOpenChests.Add(pEntity->GetID()); break;
			case eOhplGa6List_UnlitLamps: d.mlstUnlitLamps.Add(pEntity->GetID()); break;
			}
		}
		return 0;
	}
	return 0;
}

namespace
{

struct GmToEnv
{
	cLuxSavedGameMap *mpData;
	cLuxMap *mpMap;
	cWorld *mpWorld;
	std::vector<iLuxEntity*> mvEntities;
	std::vector<cLuxSavedGameEntity*> mvSavedEntities;
	std::vector<cLuxSavedGameEnemy*> mvSavedEnemies;
	std::vector<cLuxSavedGameEntityFunc*> mvFuncs[4];
	std::vector<cEngineLight_SaveData*> mvSavedLights;
	std::vector<cEngineSound_SaveData*> mvSavedSounds;
	std::vector<cEnginePS_SaveData*> mvSavedPS;
	std::vector<cParticleSystem*> mvLivePS;
	std::vector<iLuxEntity_SaveData*> mvFull;
	std::vector<int> mvLists[8];
	std::vector<iLuxEntity*> mvSetup;
	iLuxEntity *mpEntity;
	iLight *mpLight;
	cSoundEntity *mpSound;
	cParticleSystem *mpPS;
};

}

int cLuxSavedGameMap::Ga6ToCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	GmToEnv &e = *static_cast<GmToEnv*>(apCtx);
	cLuxSavedGameMap &d = *e.mpData;
	cLuxMap *apMap = e.mpMap;
	cWorld *pWorld = e.mpWorld;
	switch(alOp)
	{
	case eOhplGa6Op_GmToHead:
		apMap->SetDisplayNameEntry(d.msDisplayNameEntry);
		apMap->mlNumberOfQuests = d.mlNumberOfQuests;
		apMap->mlTotalCompletionAmount = d.mlTotalCompletionAmount;
		apMap->mlCurrentCompletionAmount = d.mlCurrentCompletionAmount;
		pWorld->SetSkyBoxActive(d.mbSkyBoxActive);
		return 0;
	case eOhplGa6Op_MapSkyIsNone:
		return d.msSkyboxTexture == "";
	case eOhplGa6Op_MapSkyNull:
		pWorld->SetSkyBox(NULL, true);
		return 0;
	case eOhplGa6Op_MapSkyLoad:
		{
			iTexture *pTexture = gpBase->mpEngine->GetResources()->GetTextureManager()->CreateCubeMap(d.msSkyboxTexture, true);
			pWorld->SetSkyBox(pTexture, true);
		}
		return 0;
	case eOhplGa6Op_MapToFog:
		pWorld->SetSkyBoxColor(d.mSkyBoxColor);
		pWorld->SetFogActive(d.mbFogActive);
		pWorld->SetFogCulling(d.mbFogCulling);
		pWorld->SetFogStart(d.mfFogStart);
		pWorld->SetFogEnd(d.mfFogEnd);
		pWorld->SetFogFalloffExp(d.mfFogFalloffExp);
		pWorld->SetFogColor(d.mFogColor);
		return 0;
	case eOhplGa6Op_GmToVarsCallbacksTimers:
		{
			apMap->m_mapVars.clear();
			cContainerListIterator<cLuxScriptVar> it = d.mlstVars.GetIterator();
			while(it.HasNext())
			{
				cLuxScriptVar& scriptVar = it.Next();
				apMap->m_mapVars.insert(tLuxScriptVarMap::value_type(scriptVar.msName, scriptVar));
			}
		}
		{
			std::vector<cLuxUseItemCallback> vCallbacks;
			for(size_t i=0; i<d.mvUseItemCallbacks.Size(); ++i) vCallbacks.push_back(d.mvUseItemCallbacks[i]);
			apMap->SetUseItemCallbacks(vCallbacks);
		}
		{
			std::vector<cLuxEventTimer> vTimers;
			cContainerListIterator<cLuxEventTimer> it = d.mlstTimers.GetIterator();
			while(it.HasNext()) vTimers.push_back(it.Next());
			apMap->SetTimers(vTimers);
		}
		return 0;
	case eOhplGa6Op_GmSavedEntityN:
		{
			cContainerListIterator<cLuxSavedGameEntity*> it = d.mlstCollideAndConnectEntities.GetIterator();
			while(it.HasNext()) e.mvSavedEntities.push_back(it.Next());
		}
		return (int)e.mvSavedEntities.size();
	case eOhplGa6Op_GmSavedEntityFound:
		e.mpEntity = apMap->GetEntityByID(e.mvSavedEntities[alA]->mlID);
		return e.mpEntity != NULL;
	case eOhplGa6Op_GmSavedEntityError:
		Error("Saved entity with ID %d does not exist!\n", e.mvSavedEntities[alA]->mlID);
		return 0;
	case eOhplGa6Op_GmToEntity:
		e.mvSavedEntities[alA]->ToEntity(apMap, e.mpEntity);
		return 0;
	case eOhplGa6Op_GmSavedEnemyN:
		{
			cContainerListIterator<cLuxSavedGameEnemy*> it = d.mlstEnemies.GetIterator();
			while(it.HasNext()) e.mvSavedEnemies.push_back(it.Next());
		}
		return (int)e.mvSavedEnemies.size();
	case eOhplGa6Op_GmSavedEnemyFound:
		e.mpEntity = apMap->GetEntityByID(e.mvSavedEnemies[alA]->mlID);
		return e.mpEntity != NULL && e.mpEntity->GetEntityType() == eLuxEntityType_Enemy;
	case eOhplGa6Op_GmSavedEnemyError:
		Error("Saved enemy with ID %d does not exist!\n", e.mvSavedEnemies[alA]->mlID);
		return 0;
	case eOhplGa6Op_GmToEnemy:
		e.mvSavedEnemies[alA]->ToEnemy(apMap, static_cast<iLuxEnemy*>(e.mpEntity));
		return 0;
	case eOhplGa6Op_GmPlayerCallbacksTo:
		{
			tLuxCollideCallbackList *pPlayerCallbackList = gpBase->mpPlayer->GetCollideCallbackList();
			cContainerListIterator<cLuxCollideCallback_SaveData> it = d.mlstPlayerCollideCallbacks.GetIterator();
			while(it.HasNext())
			{
				cLuxCollideCallback_SaveData& saveCallback = it.Next();
				cLuxCollideCallback *pCallback = hplNew(cLuxCollideCallback, ());
				saveCallback.ToCallback(apMap,gpBase->mpPlayer , pCallback);
				pPlayerCallbackList->push_back(pCallback);
			}
		}
		return 0;
	case eOhplGa6Op_GmSnapshot:
		e.mvEntities.clear();
		{
			cLuxEntityIterator it = apMap->GetEntityIterator();
			while(it.HasNext()) e.mvEntities.push_back(it.Next());
		}
		return 0;
	case eOhplGa6Op_GmEntN:
		return (int)e.mvEntities.size();
	case eOhplGa6Op_GmEntFullSave:
		return e.mvEntities[alA]->GetFullGameSave();
	case eOhplGa6Op_GmEntClearFuncs:
		{
			iLuxEntity *pEntity = e.mvEntities[alA];
			pEntity->SetPlayerLookAtCallback("", false);
			pEntity->SetPlayerInteractCallback("", false);
			pEntity->SetCallbackFunc("");
			pEntity->SetConnectionStateChangeCallback("");
		}
		return 0;
	case eOhplGa6Op_GmFuncN:
		{
			std::vector<cLuxSavedGameEntityFunc*> &vFuncs = e.mvFuncs[alA];
			vFuncs.clear();
			cContainerList<cLuxSavedGameEntityFunc> *pList = NULL;
			switch(alA)
			{
			case eOhplGa6Func_Look: pList = &d.mlstEntityFunc_Look; break;
			case eOhplGa6Func_Interact: pList = &d.mlstEntityFunc_Interact; break;
			case eOhplGa6Func_Callback: pList = &d.mlstEntityFunc_Callback; break;
			case eOhplGa6Func_StateChange: pList = &d.mlstEntityFunc_StateChange; break;
			}
			cContainerListIterator<cLuxSavedGameEntityFunc> it = pList->GetIterator();
			while(it.HasNext()) vFuncs.push_back(&it.Next());
			return (int)vFuncs.size();
		}
	case eOhplGa6Op_GmFuncFound:
		e.mpEntity = apMap->GetEntityByID(e.mvFuncs[alA][alB]->mlID);
		return e.mpEntity != NULL;
	case eOhplGa6Op_GmFuncApply:
		{
			cLuxSavedGameEntityFunc &saveFunc = *e.mvFuncs[alA][alB];
			switch(alA)
			{
			case eOhplGa6Func_Look: e.mpEntity->SetPlayerLookAtCallback(saveFunc.msFunc, saveFunc.mbAutoRemove); break;
			case eOhplGa6Func_Interact: e.mpEntity->SetPlayerInteractCallback(saveFunc.msFunc, saveFunc.mbAutoRemove); break;
			case eOhplGa6Func_Callback: e.mpEntity->SetCallbackFunc(saveFunc.msFunc); break;
			case eOhplGa6Func_StateChange: e.mpEntity->SetConnectionStateChangeCallback(saveFunc.msFunc); break;
			}
		}
		return 0;
	case eOhplGa6Op_GmSavedLightN:
		{
			cContainerListIterator<cEngineLight_SaveData> it = d.mlstLights.GetIterator();
			while(it.HasNext()) e.mvSavedLights.push_back(&it.Next());
		}
		return (int)e.mvSavedLights.size();
	case eOhplGa6Op_MapFindLight:
		e.mpLight = pWorld->GetLightFromUniqueID(e.mvSavedLights[alA]->mlID);
		return e.mpLight != NULL;
	case eOhplGa6Op_MapLightTo:
		e.mvSavedLights[alA]->ToLight(e.mpLight);
		return 0;
	case eOhplGa6Op_MapLightWarn:
		Warning("Could not find saved light '%s'\n", e.mvSavedLights[alA]->msName.c_str());
		return 0;
	case eOhplGa6Op_GmSavedSoundN:
		{
			cContainerListIterator<cEngineSound_SaveData> it = d.mlstSounds.GetIterator();
			while(it.HasNext()) e.mvSavedSounds.push_back(&it.Next());
		}
		return (int)e.mvSavedSounds.size();
	case eOhplGa6Op_GmSoundId:
		return e.mvSavedSounds[alA]->mlID;
	case eOhplGa6Op_GmFindSound:
		e.mpSound = pWorld->GetSoundEntityFromUniqueID(e.mvSavedSounds[alA]->mlID);
		return e.mpSound != NULL;
	case eOhplGa6Op_MapCreateSound:
		{
			cEngineSound_SaveData &saveSound = *e.mvSavedSounds[alA];
			e.mpSound = pWorld->CreateSoundEntity(saveSound.msName, saveSound.msSoundDataName, saveSound.mbRemoveWhenOver);
		}
		return e.mpSound != NULL;
	case eOhplGa6Op_MapSoundTo:
		e.mvSavedSounds[alA]->ToSound(e.mpSound);
		return 0;
	case eOhplGa6Op_GmSoundWarn:
		Warning("Could not create or find sound '%s' of file '%s'\n", e.mvSavedSounds[alA]->msName.c_str(), e.mvSavedSounds[alA]->msSoundDataName.c_str());
		return 0;
	case eOhplGa6Op_GmSavedPsN:
		{
			cContainerListIterator<cEnginePS_SaveData> it = d.mlstPS.GetIterator();
			while(it.HasNext()) e.mvSavedPS.push_back(&it.Next());
		}
		return (int)e.mvSavedPS.size();
	case eOhplGa6Op_GmPsId:
		return e.mvSavedPS[alA]->mlID;
	case eOhplGa6Op_GmFindPs:
		e.mpPS = pWorld->GetParticleSystemFromUniqueID(e.mvSavedPS[alA]->mlID);
		return e.mpPS != NULL;
	case eOhplGa6Op_MapPsTo:
		e.mvSavedPS[alA]->ToPS(e.mpPS);
		return 0;
	case eOhplGa6Op_MapCreatePs:
		{
			cEnginePS_SaveData &savePS = *e.mvSavedPS[alA];
			e.mpPS = pWorld->CreateParticleSystem(savePS.msName, savePS.msType, 1);
		}
		return e.mpPS != NULL;
	case eOhplGa6Op_MapPsWarn:
		Warning("Could not create particle system '%s'\n", e.mvSavedPS[alA]->msType.c_str());
		return 0;
	case eOhplGa6Op_GmLivePsN:
		{
			cParticleSystemIterator psIt = pWorld->GetParticleSystemIterator();
			while(psIt.HasNext()) e.mvLivePS.push_back(psIt.Next());
		}
		return (int)e.mvLivePS.size();
	case eOhplGa6Op_GmLivePsFlags:
		{
			cParticleSystem *pPS = e.mvLivePS[alA];
			return (pPS->IsSaved() ? eOhplGa6Flag_Saved : 0) |
				(pPS->GetEntityParent() == NULL ? eOhplGa6Flag_NoEntityParent : 0) |
				(pPS->GetParent() == NULL ? eOhplGa6Flag_NoParent : 0);
		}
	case eOhplGa6Op_GmLivePsId:
		return e.mvLivePS[alA]->GetUniqueID();
	case eOhplGa6Op_GmLivePsKill:
		e.mvLivePS[alA]->KillInstantly();
		return 0;
	case eOhplGa6Op_MapLoadBegin:
		gpBase->mpCurrentMapLoading = apMap;
		return 0;
	case eOhplGa6Op_MapLoadEnd:
		gpBase->mpCurrentMapLoading = NULL;
		return 0;
	case eOhplGa6Op_GmFullN:
		{
			cContainerListIterator<iLuxEntity_SaveData*> it = d.mlstFullEntities.GetIterator();
			while(it.HasNext()) e.mvFull.push_back(it.Next());
		}
		return (int)e.mvFull.size();
	case eOhplGa6Op_GmFullFind:
		e.mpEntity = apMap->GetEntityByID(e.mvFull[alA]->mlID);
		return e.mpEntity != NULL;
	case eOhplGa6Op_GmFullCreate:
		e.mpEntity = e.mvFull[alA]->CreateEntity(apMap);
		return e.mpEntity != NULL;
	case eOhplGa6Op_GmFullLoad:
		e.mpEntity->LoadFromSaveData(e.mvFull[alA]);
		e.mpEntity->SetSaveData(e.mvFull[alA]);
		e.mvSetup.push_back(e.mpEntity);
		return 0;
	case eOhplGa6Op_GmFullId:
		return e.mvFull[alA]->mlID;
	case eOhplGa6Op_GmEntId:
		return e.mvEntities[alA]->GetID();
	case eOhplGa6Op_GmEntDestroy:
		apMap->DestroyEntity(e.mvEntities[alA]);
		return 0;
	case eOhplGa6Op_GmUpdateDestroyed:
		apMap->UpdateToBeDesotroyedEntities(false);
		return 0;
	case eOhplGa6Op_GmListN:
		{
			std::vector<int> &vList = e.mvLists[alA];
			vList.clear();
			cContainerList<int> *pList = NULL;
			switch(alA)
			{
			case eOhplGa6List_Disabled: pList = &d.mlstDisabledEntities; break;
			case eOhplGa6List_LockedDoors: pList = &d.mlstLockedDoors; break;
			case eOhplGa6List_LockedLevelDoors: pList = &d.mlstLockedLevelDoors; break;
			case eOhplGa6List_ActiveItems: pList = &d.mlstActiveItems; break;
			case eOhplGa6List_OpenChests: pList = &d.mlstOpenChests; break;
			case eOhplGa6List_UnbrokenContainers: pList = &d.mlstUnbrokenItemContainers; break;
			case eOhplGa6List_BrokenWithActiveItem: pList = &d.mlstBrokenContainersWithActiveItem; break;
			case eOhplGa6List_UnlitLamps: pList = &d.mlstUnlitLamps; break;
			}
			cContainerListIterator<int> it = pList->GetIterator();
			while(it.HasNext()) vList.push_back(it.Next());
			return (int)vList.size();
		}
	case eOhplGa6Op_GmListGet:
		return e.mvLists[alA][alB];
	case eOhplGa6Op_GmEntType:
		return LuxEntityCode(e.mvEntities[alA]);
	case eOhplGa6Op_GmEntSetActive:
		e.mvEntities[alA]->SetActive(alB != 0);
		return 0;
	case eOhplGa6Op_GmPropType:
		return LuxPropCode(static_cast<iLuxProp*>(e.mvEntities[alA]));
	case eOhplGa6Op_GmSetLocked:
		static_cast<cLuxProp_SwingDoor*>(e.mvEntities[alA])->SetLocked(alB != 0, false);
		return 0;
	case eOhplGa6Op_GmSetLevelDoorLocked:
		static_cast<cLuxProp_LevelDoor*>(e.mvEntities[alA])->SetLocked(alB != 0);
		return 0;
	case eOhplGa6Op_GmEntSaved:
		return e.mvEntities[alA]->IsSaved();
	case eOhplGa6Op_GmObjectHasContained:
		return static_cast<cLuxProp_Object*>(e.mvEntities[alA])->GetContainedItem() != "";
	case eOhplGa6Op_GmObjectClear:
		static_cast<cLuxProp_Object*>(e.mvEntities[alA])->SetContainedItem("");
		return 0;
	case eOhplGa6Op_GmSetChestLocked:
		static_cast<cLuxProp_Chest*>(e.mvEntities[alA])->SetLocked(alB != 0, false);
		return 0;
	case eOhplGa6Op_GmSetLampLit:
		static_cast<cLuxProp_Lamp*>(e.mvEntities[alA])->SetLit(alB != 0, false);
		return 0;
	case eOhplGa6Op_MapLoadedN:
		return (int)e.mvSetup.size();
	case eOhplGa6Op_MapSetupEntity:
		e.mvSetup[alA]->SetupSaveData(e.mvSetup[alA]->GetSaveData());
		return 0;
	case eOhplGa6Op_MapLoadedActive:
		return e.mvSetup[alA]->IsActive();
	case eOhplGa6Op_MapLoadedUpdate:
		e.mvSetup[alA]->UpdateLogic(0.001f);
		return 0;
	}
	return 0;
}

void cLuxSavedGameMap::FromMap(cLuxMap *apMap)
{
	GmFromEnv env = { this, apMap, apMap->GetWorld() };
	ohpl_ga6_gamemap_from(&env, cLuxSavedGameMap::Ga6FromCall);
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

void cLuxSavedGameMap::ToMap(cLuxMap *apMap)
{
	GmToEnv env = {};
	env.mpData = this;
	env.mpMap = apMap;
	env.mpWorld = apMap->GetWorld();
	ohpl_ga6_gamemap_to(&env, cLuxSavedGameMap::Ga6ToCall);
}

//-----------------------------------------------------------------------



//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVED MAP COLLECTION
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxSavedGameMapCollection::cLuxSavedGameMapCollection()
{
		
}

cLuxSavedGameMapCollection::~cLuxSavedGameMapCollection()
{
	cContainerListIterator<cLuxSavedGameMap*> it = mlstMaps.GetIterator();
	while(it.HasNext())
	{
		hplDelete( it.Next() );
	}
	mlstMaps.Clear();
}

//-----------------------------------------------------------------------

void cLuxSavedGameMapCollection::Reset()
{
	cContainerListIterator<cLuxSavedGameMap*> it = mlstMaps.GetIterator();
	while(it.HasNext())
	{
		hplDelete( it.Next() );
	}
	mlstMaps.Clear();
}

//-----------------------------------------------------------------------

void cLuxSavedGameMapCollection::SaveMap(cLuxMap *apMap)
{
	cLuxSavedGameMap *pSavedMap = GetSavedMap(apMap->GetName(), true);

	pSavedMap->DestroyAll();
	pSavedMap->FromMap(apMap);
	//cSerializeClass::SaveToFile(pSavedMap, cString::To16Char(apMap->GetName())+_W(".testsave"), "SavedMap");
}

//-----------------------------------------------------------------------

void cLuxSavedGameMapCollection::LoadMap(cLuxMap *apMap)
{
	cLuxSavedGameMap *pSavedMap = GetSavedMap(apMap->GetName(), false);
	if(pSavedMap==NULL) return;

	pSavedMap->ToMap(apMap);
}

//-----------------------------------------------------------------------

namespace
{

cLuxSavedGameMap *FindMap(cContainerList<cLuxSavedGameMap*> &alstMaps, const tString &asName)
{
	std::vector<cLuxSavedGameMap*> vMaps;
	std::vector<OhplGa6Str> vNames;
	cContainerListIterator<cLuxSavedGameMap*> it = alstMaps.GetIterator();
	while(it.HasNext())
	{
		cLuxSavedGameMap *pMap = it.Next();
		vMaps.push_back(pMap);
		OhplGa6Str name = { (const unsigned char*)pMap->msName.data(), pMap->msName.size() };
		vNames.push_back(name);
	}
	int lIdx = ohpl_ga6_find_map(vNames.data(), vNames.size(), (const unsigned char*)asName.data(), asName.size());
	return lIdx < 0 ? NULL : vMaps[lIdx];
}

}

bool cLuxSavedGameMapCollection::MapExists(const tString& asName)
{
	return FindMap(mlstMaps, asName) != NULL;
}

//-----------------------------------------------------------------------

cLuxSavedGameMap* cLuxSavedGameMapCollection::GetSavedMap(const tString& asName, bool abCreateNew)
{
	cLuxSavedGameMap *pFound = FindMap(mlstMaps, asName);
	if(pFound) return pFound;
	if(abCreateNew==false) return NULL;
	cLuxSavedGameMap *pSaveMap = hplNew(cLuxSavedGameMap, () );
	mlstMaps.Add(pSaveMap);
	return pSaveMap;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SERIALIZABLE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxSavedGameEntity)

kSerializeVar(mlID, eSerializeType_Int32)

kSerializeClassContainer(mvAttachedProps, cLuxProp_AttachedProp, eSerializeType_Class)
kSerializeClassContainer(mvInteractConnections, iLuxInteractConnection_SaveData, eSerializeType_ClassPointer)
kSerializeClassContainer(mvConnections, cLuxEntityConnection_SaveData, eSerializeType_Class)
kSerializeClassContainer(mlstCollideCallbacks, cLuxCollideCallback_SaveData, eSerializeType_Class)

kEndSerialize()


//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxSavedGameEnemy)

kSerializeVar(mlID, eSerializeType_Int32)
kSerializeVar(mbActive, eSerializeType_Bool)

kSerializeClassContainer(mvPatrolNodes, cLuxEnemyPatrolNode_SaveData, eSerializeType_Class)

kEndSerialize()


kBeginSerializeBase(cLuxSavedGameEntityFunc)

kSerializeVar(mlID, eSerializeType_Int32)
kSerializeVar(msFunc, eSerializeType_String)
kSerializeVar(mbAutoRemove, eSerializeType_Bool)

kEndSerialize()

//-----------------------------------------------------------------------



kBeginSerializeBase(cLuxSavedGameMap)

kSerializeVar(msName, eSerializeType_String)
kSerializeVar(msDisplayNameEntry, eSerializeType_String)

kSerializeVar(mlNumberOfQuests, eSerializeType_Int32)
kSerializeVar(mlTotalCompletionAmount, eSerializeType_Int32)
kSerializeVar(mlCurrentCompletionAmount, eSerializeType_Int32)

kSerializeVar(mbSkyBoxActive, eSerializeType_Bool)
kSerializeVar(msSkyboxTexture, eSerializeType_String)
kSerializeVar(mSkyBoxColor, eSerializeType_Color)

kSerializeVar(mbFogActive, eSerializeType_Bool)
kSerializeVar(mbFogCulling, eSerializeType_Bool)
kSerializeVar(mfFogStart, eSerializeType_Float32)
kSerializeVar(mfFogEnd, eSerializeType_Float32)
kSerializeVar(mfFogFalloffExp, eSerializeType_Float32)
kSerializeVar(mFogColor, eSerializeType_Color)

kSerializeClassContainer(mlstFullEntities, iLuxEntity_SaveData, eSerializeType_ClassPointer)

kSerializeClassContainer(mlstCollideAndConnectEntities, cLuxSavedGameEntity, eSerializeType_ClassPointer)
kSerializeClassContainer(mlstEnemies, cLuxSavedGameEnemy, eSerializeType_ClassPointer)

kSerializeClassContainer(mlstTimers, cLuxEventTimer, eSerializeType_Class)

kSerializeClassContainer(mlstVars, cLuxScriptVar, eSerializeType_Class)
kSerializeClassContainer(mvUseItemCallbacks, cLuxUseItemCallback, eSerializeType_Class)

kSerializeClassContainer(mlstPlayerCollideCallbacks, cLuxCollideCallback_SaveData, eSerializeType_Class)

kSerializeClassContainer(mlstLights, cEngineLight_SaveData, eSerializeType_Class)
kSerializeClassContainer(mlstSounds, cEngineSound_SaveData, eSerializeType_Class)
kSerializeClassContainer(mlstPS, cEnginePS_SaveData, eSerializeType_Class)

kSerializeClassContainer(mlstEntityFunc_Look, cLuxSavedGameEntityFunc, eSerializeType_Class)
kSerializeClassContainer(mlstEntityFunc_Interact, cLuxSavedGameEntityFunc, eSerializeType_Class)
kSerializeClassContainer(mlstEntityFunc_Callback, cLuxSavedGameEntityFunc, eSerializeType_Class)
kSerializeClassContainer(mlstEntityFunc_StateChange, cLuxSavedGameEntityFunc, eSerializeType_Class)

kSerializeVarContainer(mlstDisabledEntities, eSerializeType_Int32)
kSerializeVarContainer(mlstLockedDoors, eSerializeType_Int32)
kSerializeVarContainer(mlstLockedLevelDoors, eSerializeType_Int32)
kSerializeVarContainer(mlstActiveItems, eSerializeType_Int32)
kSerializeVarContainer(mlstOpenChests, eSerializeType_Int32)
kSerializeVarContainer(mlstUnbrokenItemContainers, eSerializeType_Int32)
kSerializeVarContainer(mlstBrokenContainersWithActiveItem, eSerializeType_Int32)
kSerializeVarContainer(mlstUnlitLamps, eSerializeType_Int32)

kEndSerialize()

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxSavedGameMapCollection)

kSerializeClassContainer(mlstMaps,cLuxSavedGameMap, eSerializeType_ClassPointer)

kEndSerialize()


//-----------------------------------------------------------------------

