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

static_assert(sizeof(OhplEntState) == 48, "OhplEntState layout");

extern "C"
{
	OhplEntState *ohpl_ent_state_new(int id, int entity_type, void *map);
	void ohpl_ent_state_free(OhplEntState *s);
}

class iLuxEntity;

//----------------------------------------------

class cLuxEntityConnection;

class cLuxEntityConnection_SaveData : public iSerializable
{
	kSerializableClassInit(cLuxEntityConnection_SaveData)
public:
	void FromConnection(cLuxEntityConnection *apConnection);
	void ToConnection(cLuxEntityConnection *apConnection, cLuxMap *apMap);

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

class cLuxEntityConnection
{
friend class cLuxEntityConnection_SaveData;
public:
	cLuxEntityConnection(){}
	cLuxEntityConnection(const tString& asName, iLuxEntity *apEntity, bool abInvertStateSent, int alStatesUsed, const tString& asCallbackFunc) 
		: msName(asName), mpEntity(apEntity), mbInvertStateSent(abInvertStateSent), mlStatesUsed(alStatesUsed), msCallbackFunc(asCallbackFunc) {}

	const tString& GetName(){ return msName; }

	iLuxEntity* GetEntity(){ return mpEntity; }

	bool GetInvertStateSent(){ return mbInvertStateSent;}
	int GetStateUsed(){ return mlStatesUsed;}

	const tString& GetCallbackFunc(){ return msCallbackFunc; }

private:
	tString msName;
	iLuxEntity *mpEntity;
	bool mbInvertStateSent;
	int mlStatesUsed;
	tString msCallbackFunc;
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
	
	const tString& GetName()const{ return msName;}
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

	void SetCallbackFunc(const tString& asFunc){ msCallbackFunc = asFunc;}
	void SetConnectionStateChangeCallback(const tString& asFunc){ msConnectionStateChangeCallback = asFunc;}

	void SetPlayerInteractCallback(const tString &asCallbackFunc, bool abRemoveWhenInteracted);
	void SetPlayerLookAtCallback(const tString &asCallbackFunc, bool abRemoveWhenLookedAt);
	bool IsLookedAtByPlayer(){ return mpS->cb.is_looked_at; }

	void AddCollideCallbackParent(iLuxCollideCallbackContainer* apCallback);
	void RemoveCollideCallbackParent(iLuxCollideCallbackContainer* apCallback);
	
	//////////////////
	// Connection
	int GetConnectionNum(){ return (int)mvConnections.size();}
	cLuxEntityConnection *GetConnection(int lIdx){ return mvConnections[lIdx];}
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
	const tString& GetCallbackFunc()const{ return msCallbackFunc;}
	const tString& GetConnectionStateChangeCallback()const{ return msConnectionStateChangeCallback;}
	const tString& GetInteractCallback()const{ return msInteractCallback;}
	void ClearInteractCallback(){ msInteractCallback = "";}
	const tString& GetLookAtCallback()const{ return msLookAtCallback;}
	bool GetInteractCallbackRemove()const{ return mpS->cb.interact_remove;}
	bool GetLookAtCallbackRemove()const{ return mpS->cb.lookat_remove;}
	void SetInteractCallbackRemove(bool abX){ mpS->cb.interact_remove = abX;}
	void SetMaxFocusDistance(float afX){ mpS->max_focus_distance = afX;}
	eLuxFocusCrosshair GetCustomFocusCrossHair()const{ return (eLuxFocusCrosshair)mpS->crosshair;}
	void SetName(const tString& asX){ msName = asX;}
	void SetID(int alX){ mpS->id = alX;}
	void SetMap(cLuxMap *apMap){ mpS->map = apMap;}
	void SetDestroyMeFlag(bool abX){ mpS->destroy_me = abX;}
	void SetIsLookedAt(bool abX){ mpS->cb.is_looked_at = abX;}
	tString& CallbackFuncRef(){ return msCallbackFunc;}
	tString& ConnectionStateChangeCallbackRef(){ return msConnectionStateChangeCallback;}
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
	OhplEntState *mpS;
	tString msName;
	tLuxCollideCallbackContainerList mlstCollideCallbackParents;

	tString msCallbackFunc;
	tString msConnectionStateChangeCallback;

	tString msInteractCallback;
	tString msLookAtCallback;

	iLuxEntity_SaveData *mpSaveData;

	std::vector<cMesh*> mvPreloadedMeshes;
	std::vector<cLuxEntityConnection*> mvConnections;

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
