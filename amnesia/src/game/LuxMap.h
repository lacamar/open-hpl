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

#ifndef LUX_MAP_H
#define LUX_MAP_H

#include "LuxBase.h"
#include "RustLuxMapA7.h"
#include "RustLuxMapA13.h"

class cLuxNode_Pos;
class cLuxNode_PlayerStart;
class cLuxArea_Sticky;
class cLuxLampLightConnection;
class cLuxProp_Lamp;

typedef std::list<cLuxArea_Sticky*> tLuxArea_StickyList;

// Shell: the state and the logic live in rust/src/luxmap/ (mmap.rs); this class keeps the plain
// properties other code reads (names, vars) and forwards the rest.
class cLuxMap
{
friend class cLuxSavedMap;
friend class cLuxSavedGameMap;
public:
	cLuxMap(const tString& asName);
	~cLuxMap();

	const tString& GetName(){ return msName;}
	const tString& GetFileName(){ return msFileName;}

	void SetDisplayNameEntry(const tString& asEntry){ msDisplayNameEntry = asEntry;}
	const tString& GetDisplayNameEntry(){ return msDisplayNameEntry;}

	bool LoadFromFile(const tString & asFile, bool abLoadEntities)
	{
		msFileName = asFile;
		ohpl_a13_map_load(mpA7, OHPL_A7_S(asFile), abLoadEntities);
		return true;
	}
	void AfterWorldLoadEntitySetup(){ ohpl_a13_map_after_world_load(mpA7);}

	void OnEnter(bool abRunScript, bool abFirstTime){ ohpl_a13_map_on_enter(mpA7, abRunScript, abFirstTime);}
	void OnLeave(bool abRunScript){ ohpl_a13_map_on_leave(mpA7, abRunScript);}

	void Update(float afTimeStep){ ohpl_a13_map_update(mpA7, afTimeStep);}

	void RunScript(const tString& asCommand){ ohpl_a13_map_run_script(mpA7, OHPL_A7_S(asCommand));}
	bool RecompileScript(tString *apOutput)
	{
		tString sOut;
		bool bOk = ohpl_a13_map_recompile(mpA7, ohpl_ga5::Collect, &sOut);
		if(apOutput) *apOutput = sOut;
		return bOk;
	}

	void OnRenderSolid(cRendererCallbackFunctions* apFunctions){ ohpl_a13_map_render_solid(mpA7, apFunctions);}

	OhplA7Ctx* GetA7(){ return mpA7; }

	cWorld* GetWorld(){ return static_cast<cWorld*>(ohpl_a13_map_world(mpA7));}
	iPhysicsWorld* GetPhysicsWorld(){ return static_cast<iPhysicsWorld*>(ohpl_a13_map_phys(mpA7));}

	void PlacePlayerAtStartPos(const tString& asPosName){ ohpl_a13_map_place_player(mpA7, OHPL_A7_S(asPosName));}

	void CreateEntity(const tString& asName, const tString& asFile, const cMatrixf& a_mtxTransform, const cVector3f& avScale)
	{
		ohpl_a13_map_create_entity(mpA7, OHPL_A7_S(asName), OHPL_A7_S(asFile), a_mtxTransform.v, &avScale.x);
	}

	/**
	 * This also destroys timer and light connections
	 */
	void DestroyAllEntities(){ ohpl_a7_destroy_all(mpA7);}

	void AddEntity(iLuxEntity *apEntity){ ohpl_a13_map_add_entity(mpA7, apEntity);}

	/**
	 * Do not call this when IsDeletingAllWorldEntities is true!
	 */
	void DestroyEntity(iLuxEntity *apEntity){ ohpl_a13_map_destroy_entity(mpA7, apEntity);}
	iLuxEntity *GetEntityByName(const tString& asName, eLuxEntityType aType=eLuxEntityType_LastEnum, int alSubType=-1)
	{
		iLuxEntity *pEntity = static_cast<iLuxEntity*>(ohpl_a7_ent_find_name(mpA7, OHPL_A7_S(asName)));
		if(pEntity == NULL || LuxIsCorrectType(pEntity, aType, alSubType)== false) return NULL;
		return pEntity;
	}
	iLuxEntity *GetEntityByID(int alID, eLuxEntityType aType=eLuxEntityType_LastEnum, int alSubType=-1)
	{
		iLuxEntity *pEntity = static_cast<iLuxEntity*>(ohpl_a7_ent_find_id(mpA7, alID));
		if(pEntity == NULL || LuxIsCorrectType(pEntity, aType, alSubType)== false) return NULL;
		return pEntity;
	}
	iLuxEntity *GetLatestEntity(){ return static_cast<iLuxEntity*>(ohpl_a7_ent_latest(mpA7));}
	void ResetLatestEntity(){ ohpl_a7_ent_reset_latest(mpA7); }
	bool EntityExists(iLuxEntity *apEntity){ return ohpl_a7_ent_exists(mpA7, apEntity);}
	cLuxEntityIterator GetEntityIterator(){ return cLuxEntityIterator(mpA7, kOhplA7Entities);}
	cLuxEnemyIterator GetEnemyIterator(){ return cLuxEnemyIterator(mpA7, kOhplA7Enemies);}

	void BroadcastEnemyMessage(eLuxEnemyMessage aType, bool abHasPosition, const cVector3f& avPos, float afRadius,
								float afTime=0, bool abLocalScope=false, const cVector3f& avX=0,float afX=0, int alX=0)
	{
		ohpl_a13_map_broadcast(mpA7, aType, abHasPosition, &avPos.x, afRadius, afTime, abLocalScope, &avX.x, afX, alX);
	}
	void BroadcastEnemySoundMessage(const cVector3f& avPos, float afVolume ,float afMinDist, float afMaxDist)
	{
		ohpl_a13_map_broadcast_sound(mpA7, &avPos.x, afVolume, afMinDist, afMaxDist);
	}

	bool AINodeIsUsedAsGoal(cAINode *apNode){ return ohpl_a13_map_node_is_goal(mpA7, apNode);}
	bool DoorIsBroken(int alID)
	{
		int lState;
		return ohpl_a13_map_door(mpA7, alID, &lState);
	}
	/**
	* -1 = angle is close to 0, 1=angle is 70% or higher of max, 0=inbetween -1 and 1.
	*/
	int GetDoorState(int alID)
	{
		int lState;
		ohpl_a13_map_door(mpA7, alID, &lState);
		return lState;
	}

	bool BodyIsInDetachableStickyArea(iPhysicsBody* apBody){ return ohpl_a13_map_body_in_sticky(mpA7, apBody);}
	bool DetachBodyFromStickyArea(iPhysicsBody* apBody){ return ohpl_a13_map_detach_sticky(mpA7, apBody);}

	iPhysicsBody* GetBodyFromEntityBodyIdPair(const cLuxIdPair &aIdPair)
	{
		return static_cast<iPhysicsBody*>(ohpl_a13_map_body_from_pair(mpA7, aIdPair.mlParentId, aIdPair.mlChildId));
	}

	bool CheckCollision(iLuxCollideCallbackContainer *apCollider1, iLuxCollideCallbackContainer* apCollider2)
	{
		return ohpl_a13_map_check_collision(mpA7, apCollider1, apCollider2);
	}

	void AddPlayerStart(cLuxNode_PlayerStart *apNode);
	cLuxNode_PlayerStart *GetPlayerStart(const tString & asName);
	cLuxNode_PlayerStart *GetFirstPlayerStart(){ return static_cast<cLuxNode_PlayerStart*>(ohpl_a7_start_first(mpA7));}
	int GetPlayerStartNodeNum(){ return (int)ohpl_a7_start_num(mpA7);}
	cLuxNode_PlayerStart *GetPlayerStartNode(int alIdx){ return static_cast<cLuxNode_PlayerStart*>(ohpl_a7_start_at(mpA7, alIdx));}

	void AddPosNode(cLuxNode_Pos *apNode);
	cLuxNode_Pos *GetPosNode(const tString & asName);

	void SetCheckPoint(const tString& asName, const tString& asStartPos, const tString& asCallback)
	{
		ohpl_a13_map_set_checkpoint(mpA7, OHPL_A7_S(asName), OHPL_A7_S(asStartPos), OHPL_A7_S(asCallback));
	}
	void LoadCheckPoint(){ ohpl_a13_map_load_checkpoint(mpA7);}

	void AddUseItemCallback(	const tString& asName, const tString& asItem, const tString& asEntity,
								const tString& asFunction, bool abAutoCallback);
	void RemoveUseItemCallback( const tString& asName);

	// Save glue: the callbacks, timers and vars as the save data holds them.
	void CopyUseItemCallbacks(std::vector<cLuxUseItemCallback> &avOut);
	void SetUseItemCallbacks(const std::vector<cLuxUseItemCallback> &avIn);
	void CopyTimers(std::vector<cLuxEventTimer> &avOut);
	void SetTimers(const std::vector<cLuxEventTimer> &avIn);
	bool GetTimerCount(const tString& asName, float *apCount){ return ohpl_a7_timer_get(mpA7, OHPL_A7_S(asName), apCount);}

	/**
	 * if asName is not "" it can be used as a safty check so it really is the correct callback that is destroyed!
	 */
	void RemoveUseItemCallback( cLuxUseItemCallback * apCallback, const tString& asName=""){ ohpl_a7_use_remove_ptr(mpA7, apCallback, OHPL_A7_S(asName));}
	cLuxUseItemCallback* GetUseItemCallback(const tString& asItem, const tString& asEntity)
	{
		return static_cast<cLuxUseItemCallback*>(ohpl_a7_use_find(mpA7, OHPL_A7_S(asItem), OHPL_A7_S(asEntity)));
	}

	void AddTimer(const tString& asName, float afTime, const tString& asFunction){ ohpl_a7_timer_add(mpA7, OHPL_A7_S(asName), afTime, OHPL_A7_S(asFunction));}
	void RemoveTimer(const tString& asName){ ohpl_a7_timer_remove(mpA7, OHPL_A7_S(asName));}

	void AddDissolveEntity(cMeshEntity *apMeshEntity, float afTime){ ohpl_a13_map_add_dissolve(mpA7, apMeshEntity, afTime);}

	cLuxLampLightConnection* AddLampLightConnection(cLuxProp_Lamp *apLamp, iLight *apLight, float afAmount, bool abUseOnColor, bool abUseSpec)
	{
		return static_cast<cLuxLampLightConnection*>(ohpl_a13_map_add_lamp(mpA7, apLamp, apLight, afAmount, abUseOnColor, abUseSpec));
	}

	cLuxScriptVar* GetVar(const tString &asName);

	void UpdateToBeDesotroyedEntities(bool abUseCallbacks){ ohpl_a13_map_update_doomed(mpA7, abUseCallbacks);}

	bool IsDeletingAllWorldEntities(){ return ohpl_a7_deleting_all(mpA7);}

	//////////////////////////
	// Properties
	void SetNumberOfQuests(int alX){ ohpl_a13_map_set_quests(mpA7, alX);}
	void AddCompletionAmount(int alAmount, float afDelay=0.0f){ ohpl_a13_map_add_completion(mpA7, alAmount, afDelay);}

	void SetLanternLitCallback(const tString& asCallback){ msLanternLitCallback = asCallback;}
	const tString& GetLanternLitCallback(){ return msLanternLitCallback;}

	// Save glue: the counters and the checkpoint as the save data holds them.
	void GetState(OhplA13MapState &aState){ ohpl_a13_map_get(mpA7, &aState);}
	void SetState(const OhplA13MapState &aState){ ohpl_a13_map_set(mpA7, &aState);}

	// iLuxEntity members only this friend may call: 0 BeforeEntityDestruction, 1 DestroyMe
	static void HostEntityCall(iLuxEntity *apEntity, int alWhich);

	// OHPL_A7_MAPDUMP hook (LuxA13aDump.cpp)
	void DumpA7(const char *asFile, const char *asLabel);

private:
	tString msName;
	tString msFileName;
	tString msDisplayNameEntry;
	tString msLanternLitCallback;

	OhplA7Ctx *mpA7;

	tLuxScriptVarMap m_mapVars;
};

#endif // LUX_MAP_H
