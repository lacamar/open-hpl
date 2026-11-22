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

#ifndef LUX_ENTITY_H
#define LUX_ENTITY_H

//----------------------------------------------

#include "LuxBase.h"
#include "RustLuxProp.h"

//----------------------------------------------

struct OhplEntState
{
	void *map;
	float max_focus_distance;
	int id;
	int entity_type;
	int crosshair;
	OhplGa4cEntityCb cb;
	bool active;
	bool full_game_save;
	bool is_saved;
	bool interaction_disabled;
	bool destroy_me;
};

extern "C"
{
	OhplEntState *ohpl_ent_state_new(int id, int entity_type, void *map);
	void ohpl_ent_state_free(OhplEntState *s);
	const unsigned char *ohpl_ent_str(const OhplEntState *s, int kind, size_t *len);
	void ohpl_ent_str_set(OhplEntState *s, int kind, const unsigned char *p, size_t n);
	const unsigned char *ohpl_ent_script(OhplEntState *s, int code, int i, int j, const unsigned char *type, size_t type_len, size_t *len);
	void ohpl_ent_run_callback(OhplEntState *s, const OhplGa4Sink *sink);
	void ohpl_ent_run_interact(OhplEntState *s, const OhplGa4Sink *sink);
	void ohpl_ent_look_at(OhplEntState *s, float dt, const OhplGa4cLookHost *host, const OhplGa4bLos *los, const OhplGa4Sink *sink);
	void ohpl_ent_conn_change(OhplEntState *s, int state, const OhplGa4Sink *sink);
	void ohpl_ent_conn_add(OhplEntState *s, const unsigned char *name, size_t name_len, void *entity, OhplEntState *entity_state, bool invert, int states, const unsigned char *cb, size_t cb_len);
	void ohpl_ent_conn_clear(OhplEntState *s);
	int ohpl_ent_conn_num(const OhplEntState *s);
	const unsigned char *ohpl_ent_conn_str(const OhplEntState *s, int i, int kind, size_t *len);
	void ohpl_ent_conn_info(const OhplEntState *s, int i, int *entity_id, bool *invert, int *states);
	void *ohpl_ent_conn_entity(const OhplEntState *s, int i);
}

class iLuxEntity;

//----------------------------------------------

class cLuxEntityConnection_SaveData : public iSerializable
{
	kSerializableClassInit(cLuxEntityConnection_SaveData)
public:
	tString msName;
	int mlEntityId;
	bool mbInvertStateSent;
	int mlStatesUsed;
	tString msCallbackFunc;
};

//----------------------------------------------

class iLuxEntity_SaveData : public iSerializable
{
	kSerializableClassInit(iLuxEntity_SaveData)
public:
	virtual ~iLuxEntity_SaveData(){}

	tString msName;
	bool mbActive;
	bool mbFullGameSave;
	int mlEntityType;
	int mlID;

	float mfMaxFocusDistance;

	bool mbInteractionDisabled;

	tString msCallbackFunc;
	tString msConnectionStateChangeCallback;

	tString msInteractCallback;
	bool mbInteractCallbackRemove;

	tString msLookAtCallback;
	bool mbLookAtCallbackRemove;
	bool mbIsLookedAt;

	cContainerVec<cLuxEntityConnection_SaveData> mvConnections;
	cContainerList<cLuxCollideCallback_SaveData> mlstCollideCallbacks;

	virtual iLuxEntity* CreateEntity(cLuxMap *apMap)=0;
};

//----------------------------------------------

class cLuxMap;

class iLuxEntity : public iLuxCollideCallbackContainer
{
friend class cLuxMap;
friend class cLuxSavedGameEntity;
friend class cLuxSavedGameMap;
friend class OhplGa4cEntSink;
public:	
	iLuxEntity(const tString &asName, int alID, cLuxMap *apMap,eLuxEntityType aEntityType);
	virtual ~iLuxEntity();

	//////////////////
	// General
	void UpdateLogic(float afTimeStep);

	virtual void OnRenderSolid(cRendererCallbackFunctions* apFunctions){}

	virtual bool CanInteract(iPhysicsBody *apBody)=0;
	virtual bool OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)=0;
	
	virtual void AfterWorldLoad(){}
	virtual void OnMapEnter(){}

	virtual void InFocusDraw(cGuiSet *apGuiSet,float afFrameTime){}

	//////////////////
	// Action
	cSoundEntity* PlaySound(const tString& asName, const tString& asFile, bool abRemoveWhenDone, bool abAttach);

	void RunCallbackFunc(const tString& asType);
	void RunInteractCallbackFunc();

	virtual void GiveDamage(float afAmount, int alStrength)=0;

	//////////////////
	// Properties
	eLuxEntityType GetEntityType()const{ return (eLuxEntityType)mpS->entity_type;}
	
	tString GetName()const{ return GetEntStr(kEntStrName);}
	int GetID()const{ return mpS->id;}

	void SetActive(bool abX);
	bool IsActive()const{ return mpS->active;}

	cLuxMap *GetMap()const{ return (cLuxMap*)mpS->map;}

	void SetFullGameSave(bool abX){ mpS->full_game_save=abX;}
	bool GetFullGameSave(){ return mpS->full_game_save;}

	void SetIsSaved(bool abX){ mpS->is_saved = abX;}
	bool IsSaved(){ return mpS->is_saved;}
	bool GetDestroyMe(){ return mpS->destroy_me;}

	void SetInteractionDisabled(bool abX){ mpS->interaction_disabled = abX; }
	bool GetInteractionDisabled(){ return mpS->interaction_disabled; }

	float GetMaxFocusDistance(){ return mpS->max_focus_distance;}
	virtual eLuxFocusCrosshair GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)=0;

	void SetCustomFocusCrossHair(eLuxFocusCrosshair aX){ mpS->crosshair = aX;}

	virtual tWString GetFocusText(){ return _W("");}

	virtual iEntity3D* GetAttachEntity()=0;

	virtual cMeshEntity* GetMeshEntity(){ return NULL; }

	void SetCallbackFunc(const tString& asFunc){ SetEntStr(kEntStrCallback, asFunc);}
	void SetConnectionStateChangeCallback(const tString& asFunc){ SetEntStr(kEntStrConnChange, asFunc);}

	void SetPlayerInteractCallback(const tString &asCallbackFunc, bool abRemoveWhenInteracted);
	void SetPlayerLookAtCallback(const tString &asCallbackFunc, bool abRemoveWhenLookedAt);
	bool IsLookedAtByPlayer(){ return mpS->cb.is_looked_at; }

	void AddCollideCallbackParent(iLuxCollideCallbackContainer* apCallback);
	void RemoveCollideCallbackParent(iLuxCollideCallbackContainer* apCallback);
	
	//////////////////
	// Connection
	int GetConnectionNum(){ return ohpl_ent_conn_num(mpS);}
	void SaveConnections(cContainerVec<cLuxEntityConnection_SaveData> &avOut);
	void LoadConnections(cContainerVec<cLuxEntityConnection_SaveData> &avIn, cLuxMap *apMap);
	void AddConnection(const tString& asName, iLuxEntity *apEntity, bool abInvertStateSent, int alStatesUsed, const tString &asCallbackFunc);

	////////////////
	// Debug
	virtual float DrawDebug(cGuiSet *apSet,iFontData *apFont,float afStartY){ return afStartY;}

		
	//////////////////////
	//Save data stuff
	void SetSaveData(iLuxEntity_SaveData *apData){ mpSaveData = apData;}
	iLuxEntity_SaveData *GetSaveData(){ return mpSaveData;}

	virtual iLuxEntity_SaveData* CreateSaveData()=0;
	virtual void SaveToSaveData(iLuxEntity_SaveData* apSaveData);
	virtual void LoadFromSaveData(iLuxEntity_SaveData* apSaveData);
	virtual void SetupSaveData(iLuxEntity_SaveData *apSaveData);

	//////////////////
	// Member accessors
	tString GetCallbackFunc()const{ return GetEntStr(kEntStrCallback);}
	tString GetConnectionStateChangeCallback()const{ return GetEntStr(kEntStrConnChange);}
	tString GetInteractCallback()const{ return GetEntStr(kEntStrInteract);}
	void ClearInteractCallback(){ SetEntStr(kEntStrInteract, "");}
	tString GetLookAtCallback()const{ return GetEntStr(kEntStrLookAt);}
	bool GetInteractCallbackRemove()const{ return mpS->cb.interact_remove;}
	bool GetLookAtCallbackRemove()const{ return mpS->cb.lookat_remove;}
	void SetInteractCallbackRemove(bool abX){ mpS->cb.interact_remove = abX;}
	void SetMaxFocusDistance(float afX){ mpS->max_focus_distance = afX;}
	eLuxFocusCrosshair GetCustomFocusCrossHair()const{ return (eLuxFocusCrosshair)mpS->crosshair;}
	void SetName(const tString& asX){ SetEntStr(kEntStrName, asX);}
	void SetID(int alX){ mpS->id = alX;}
	void SetMap(cLuxMap *apMap){ mpS->map = apMap;}
	void SetDestroyMeFlag(bool abX){ mpS->destroy_me = abX;}
	void SetIsLookedAt(bool abX){ mpS->cb.is_looked_at = abX;}
	float* MaxFocusDistancePtr(){ return &mpS->max_focus_distance;}

protected:
	void UpdateCheckCollideCallback(float afTimeStep);
	void UpdatePlayerLookAt(float afTimeStep);
	void ConnectionStateChange(int alState);

	/////////////////
	//Virtual methods
	virtual void OnConnectionStateChange(iLuxEntity *apEntity, int alState)=0;
	virtual void OnUpdate(float afTimeStep)=0;
	virtual void BeforeEntityDestruction(){}
	virtual void OnSetActive(bool abX){}

	/////////////////
	//Helper methods
	void PreloadEntityModel(const tString &asFile);
	bool CollidesWithPlayer();

private:
	enum { kEntStrName, kEntStrCallback, kEntStrConnChange, kEntStrInteract, kEntStrLookAt };

	OhplEntState *mpS;
	tLuxCollideCallbackContainerList mlstCollideCallbackParents;

	iLuxEntity_SaveData *mpSaveData;

	std::vector<cMesh*> mvPreloadedMeshes;

	tString GetEntStr(int alKind)const{ size_t n; const unsigned char *p = ohpl_ent_str(mpS, alKind, &n); return tString((const char*)p, n);}
	void SetEntStr(int alKind, const tString &asX){ ohpl_ent_str_set(mpS, alKind, (const unsigned char*)asX.data(), asX.size());}

	void DestroyMe(){ mpS->destroy_me = true;}
	void Ga4cApplyOp(const OhplGa4Op &aOp, const tString *apType);
};

//----------------------------------------------


class OhplGa4cEntSink
{
public:
	OhplGa4cEntSink(iLuxEntity *apEntity, const tString *apType = NULL) : mpEntity(apEntity), mpType(apType) { mSink.ctx = this; mSink.push = &Push; }
	const OhplGa4Sink *Sink() const { return &mSink; }

private:
	static void Push(void *c, const OhplGa4Op *op) { OhplGa4cEntSink *me = (OhplGa4cEntSink*)c; me->mpEntity->Ga4cApplyOp(*op, me->mpType); }
	iLuxEntity *mpEntity;
	const tString *mpType;
	OhplGa4Sink mSink;
};

//----------------------------------------------

#endif // LUX_ENTITY_H
