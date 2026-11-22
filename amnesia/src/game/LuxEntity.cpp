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

#include "LuxEntity.h"
#include "RustLuxProp.h"
#include <cstring>

#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxMapHelper.h"

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxEntity::iLuxEntity(const tString &asName, int alID, cLuxMap *apMap, eLuxEntityType aEntityType) : iLuxCollideCallbackContainer(), mpS(ohpl_ent_state_new(alID, aEntityType, apMap)), mpSaveData(NULL)
{
	SetEntStr(kEntStrName, asName);
}

//-----------------------------------------------------------------------

iLuxEntity::~iLuxEntity()
{
	for(size_t i=0; i<mvPreloadedMeshes.size();++i)
	{
		gpBase->mpEngine->GetResources()->GetMeshManager()->Destroy(mvPreloadedMeshes[i]);
	}

	tLuxCollideCallbackContainerListIt it = mlstCollideCallbackParents.begin();
	for(; it != mlstCollideCallbackParents.end(); ++it)
	{
		iLuxCollideCallbackContainer *pParent = *it;
		pParent->RemoveCollideCallbackInstantly(this);
	}

	DestroyCollideCallbacks();
	ohpl_ent_state_free(mpS);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void iLuxEntity::UpdateLogic(float afTimeStep)
{	
	//////////////////////
	// Normal update
	UpdateCheckCollideCallback(afTimeStep);
	UpdatePlayerLookAt(afTimeStep);


	///////////////////////
	// Update implemented 
	OnUpdate(afTimeStep);
}

//-----------------------------------------------------------------------

cSoundEntity* iLuxEntity::PlaySound(const tString& asName, const tString& asFile, bool abRemoveWhenDone, bool abAttach)
{
	if(asFile == "") return NULL;

	cSoundEntity *pSound = GetMap()->GetWorld()->CreateSoundEntity(GetName() + "_" + asName,asFile,abRemoveWhenDone);
	iEntity3D *pAttachEnt = GetAttachEntity();

	switch(ohpl_ga4c_ent_attach(pSound != NULL, pAttachEnt != NULL, abAttach))
	{
	case kOhplGa4cAttach_Child:		pAttachEnt->AddChild(pSound); break;
	case kOhplGa4cAttach_Position:	pSound->SetPosition(pAttachEnt->GetWorldPosition()); break;
	}

	return pSound;
}

//-----------------------------------------------------------------------

void iLuxEntity::RunCallbackFunc(const tString& asType)
{
	OhplGa4cEntSink sink(this, &asType);
	ohpl_ent_run_callback(mpS, sink.Sink());
}

//-----------------------------------------------------------------------

void iLuxEntity::RunInteractCallbackFunc()
{
	OhplGa4cEntSink sink(this);
	ohpl_ent_run_interact(mpS, sink.Sink());
}

//-----------------------------------------------------------------------

void iLuxEntity::SetActive(bool abX)
{
	if(ohpl_ga4c_ent_set_active(&mpS->active, abX) == false) return;

    OnSetActive(abX);
}

//-----------------------------------------------------------------------

void iLuxEntity::AddCollideCallbackParent(iLuxCollideCallbackContainer* apCallback)
{
	mlstCollideCallbackParents.push_back(apCallback);
}

void iLuxEntity::RemoveCollideCallbackParent(iLuxCollideCallbackContainer* apCallback)
{
	STLFindAndRemove(mlstCollideCallbackParents, apCallback);
}

//-----------------------------------------------------------------------

void iLuxEntity::SetPlayerInteractCallback(const tString &asCallbackFunc, bool abRemoveWhenInteracted)
{
	SetEntStr(kEntStrInteract, asCallbackFunc);
	mpS->cb.interact_remove = abRemoveWhenInteracted;
}

//-----------------------------------------------------------------------

void iLuxEntity::SetPlayerLookAtCallback(const tString &asCallbackFunc, bool abRemoveWhenLookedAt)
{
	SetEntStr(kEntStrLookAt, asCallbackFunc);
	mpS->cb.lookat_remove = abRemoveWhenLookedAt;
}

//-----------------------------------------------------------------------

void iLuxEntity::AddConnection(const tString& asName, iLuxEntity *apEntity, bool abInvertStateSent, int alStatesUsed, const tString &asCallbackFunc)
{
	ohpl_ent_conn_add(mpS, (const unsigned char*)asName.data(), asName.size(), apEntity, apEntity ? apEntity->mpS : NULL,
		abInvertStateSent, alStatesUsed, (const unsigned char*)asCallbackFunc.data(), asCallbackFunc.size());
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void iLuxEntity::UpdateCheckCollideCallback(float afTimeStep)
{
	CheckCollisionCallback(GetName(), GetMap());
}


//-----------------------------------------------------------------------

static bool LuxEntityLineOfSight(void *apCtx, const float *apStart, const float *apEnd)
{
	return gpBase->mpMapHelper->CheckLineOfSight(cVector3f(apStart[0], apStart[1], apStart[2]), cVector3f(apEnd[0], apEnd[1], apEnd[2]), false);
}

//-----------------------------------------------------------------------

class cLuxEntityLookHost
{
public:
	cLuxEntityLookHost(iLuxEntity *apEntity) : mpEntity(apEntity)
	{
		mHost.ctx = this; mHost.body_num = &BodyNum; mHost.body = &Body;
	}
	const OhplGa4cLookHost *Host() const { return &mHost; }

private:
	static int BodyNum(void *c) { return ((cLuxEntityLookHost*)c)->mpEntity->GetBodyNum(); }
	static bool Body(void *c, int i, OhplGa4bLookIn *apOut)
	{
		iPhysicsBody *pBody = ((cLuxEntityLookHost*)c)->mpEntity->GetBody(i);
		cCamera *pCamera = gpBase->mpPlayer->GetCamera();
		cFrustum *pFrustum = pCamera->GetFrustum();
		cBoundingVolume *pBV = pBody->GetBoundingVolume();

		if(pFrustum->CollideBoundingVolume(pBV) == eCollision_Outside) return false;

		cVector3f vCamPos = pCamera->GetPosition();
		cVector3f vCamFwd = pCamera->GetForward();
		cVector3f vCamUp = pCamera->GetUp();
		cVector3f vCamRight = pCamera->GetRight();
		cVector3f vMin = pBV->GetMin();
		cVector3f vMax = pBV->GetMax();
		cVector3f vCenter = pBV->GetWorldCenter();
		memcpy(apOut->cam_pos, vCamPos.v, sizeof(apOut->cam_pos));
		memcpy(apOut->cam_fwd, vCamFwd.v, sizeof(apOut->cam_fwd));
		memcpy(apOut->cam_up, vCamUp.v, sizeof(apOut->cam_up));
		memcpy(apOut->cam_right, vCamRight.v, sizeof(apOut->cam_right));
		memcpy(apOut->bv_min, vMin.v, sizeof(apOut->bv_min));
		memcpy(apOut->bv_max, vMax.v, sizeof(apOut->bv_max));
		memcpy(apOut->bv_center, vCenter.v, sizeof(apOut->bv_center));
		apOut->bv_radius = pBV->GetRadius();
		return true;
	}

	iLuxEntity *mpEntity;
	OhplGa4cLookHost mHost;
};

//-----------------------------------------------------------------------

void iLuxEntity::UpdatePlayerLookAt(float afTimeStep)
{
	cLuxEntityLookHost host(this);
	OhplGa4bLos los;
	los.ctx = NULL;
	los.los = &LuxEntityLineOfSight;

	OhplGa4cEntSink sink(this);
	ohpl_ent_look_at(mpS, afTimeStep, host.Host(), &los, sink.Sink());
}

//-----------------------------------------------------------------------

void iLuxEntity::ConnectionStateChange(int alState)
{
	OhplGa4cEntSink sink(this);
	ohpl_ent_conn_change(mpS, alState, sink.Sink());
}

void iLuxEntity::Ga4cApplyOp(const OhplGa4Op &aOp, const tString *apType)
{
	if(aOp.code == kOhplGa4cOp_EntConnSend)
	{
		iLuxEntity *pChild = (iLuxEntity*)ohpl_ent_conn_entity(mpS, aOp.i);
		if(pChild) pChild->OnConnectionStateChange(this, aOp.j);
		return;
	}
	size_t lLen;
	const unsigned char *pCmd = ohpl_ent_script(mpS, aOp.code, aOp.i, aOp.j, apType ? (const unsigned char*)apType->data() : NULL, apType ? apType->size() : 0, &lLen);
	if(pCmd) GetMap()->RunScript(tString((const char*)pCmd, lLen));
}

//-----------------------------------------------------------------------

void iLuxEntity::PreloadEntityModel(const tString &asFile)
{
	cResources *pResources = gpBase->mpEngine->GetResources();

	tString sFileName = cString::SetFileExt(asFile,"ent");
	
	//////////////////////
	// Load XML document
	iXmlDocument *pEntityDoc = pResources->LoadXmlDocument(sFileName);
	if(ohpl_ga4c_ent_preload_has_doc(pEntityDoc != NULL) == false){
		Error("Could not load xml file '%s'\n", sFileName.c_str());
		return;
	}
	
	//////////////////////
	// Get Model File name
	cXmlElement *pModelDataElem = pEntityDoc->GetFirstElement("ModelData");
	cXmlElement *pMeshElem = pModelDataElem->GetFirstElement("Mesh");

	tString sModelFile = pMeshElem->GetAttributeString("Filename","");

	//////////////////////
	// Load Mesh
	cMesh *pMesh = pResources->GetMeshManager()->CreateMesh(sModelFile);
	if(ohpl_ga4c_ent_preload_has_mesh(pMesh != NULL))
		mvPreloadedMeshes.push_back(pMesh);
	else
		Error("Could not load mesh '%s' when preloading!\n",sModelFile.c_str());

	pResources->DestroyXmlDocument(pEntityDoc);
}

//-----------------------------------------------------------------------

class cLuxEntityCollideHost
{
public:
	cLuxEntityCollideHost(iLuxEntity *apEntity, iPhysicsWorld *apPhysicsWorld, iPhysicsBody *apPlayerBody) : mpEntity(apEntity), mpPhysicsWorld(apPhysicsWorld), mpPlayerBody(apPlayerBody)
	{
		mHost.ctx = this; mHost.body_num = &BodyNum; mHost.bv_intersects = &BvIntersects; mHost.shape_collides = &ShapeCollides;
		mCollideData.SetMaxSize(1);
	}
	const OhplGa4cCollideHost *Host() const { return &mHost; }

private:
	static cLuxEntityCollideHost *Me(void *c) { return (cLuxEntityCollideHost*)c; }
	static int BodyNum(void *c) { return Me(c)->mpEntity->GetBodyNum(); }
	static bool BvIntersects(void *c, int i)
	{
		iPhysicsBody *pBody = Me(c)->mpEntity->GetBody(i);
		return cMath::CheckBVIntersection(*Me(c)->mpPlayerBody->GetBoundingVolume(), *pBody->GetBoundingVolume());
	}
	static bool ShapeCollides(void *c, int i)
	{
		cLuxEntityCollideHost *me = Me(c);
		iPhysicsBody *pBody = me->mpEntity->GetBody(i);
		return me->mpPhysicsWorld->CheckShapeCollision(pBody->GetShape(), pBody->GetLocalMatrix(), me->mpPlayerBody->GetShape(), me->mpPlayerBody->GetLocalMatrix(), me->mCollideData,1, false);
	}

	iLuxEntity *mpEntity;
	iPhysicsWorld *mpPhysicsWorld;
	iPhysicsBody *mpPlayerBody;
	cCollideData mCollideData;
	OhplGa4cCollideHost mHost;
};

//-----------------------------------------------------------------------

bool iLuxEntity::CollidesWithPlayer()
{
	iPhysicsWorld *pPhysicsWorld = GetMap()->GetPhysicsWorld();
	iPhysicsBody *pPlayerBody = gpBase->mpPlayer->GetCharacterBody()->GetCurrentBody();

	cLuxEntityCollideHost host(this, pPhysicsWorld, pPlayerBody);
	return ohpl_ga4c_ent_collides(host.Host());
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kSerializeRustClass(cLuxEntityConnection_SaveData)

//-----------------------------------------------------------------------

kSerializeRustClassV(iLuxEntity_SaveData, )

//-----------------------------------------------------------------------

void iLuxEntity::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	apSaveData->mlEntityType = mpS->entity_type;

	apSaveData->msName = GetName();
	apSaveData->mbFullGameSave = mpS->full_game_save;
	apSaveData->mbActive = mpS->active;
	apSaveData->mlID = mpS->id;
	apSaveData->mbInteractionDisabled = mpS->interaction_disabled;

	apSaveData->mfMaxFocusDistance = mpS->max_focus_distance;

	apSaveData->msCallbackFunc = GetCallbackFunc();
	apSaveData->msConnectionStateChangeCallback = GetConnectionStateChangeCallback();

	apSaveData->msInteractCallback = GetInteractCallback();
	apSaveData->mbInteractCallbackRemove = mpS->cb.interact_remove;

	apSaveData->msLookAtCallback = GetLookAtCallback();
	apSaveData->mbLookAtCallbackRemove = mpS->cb.lookat_remove;
	apSaveData->mbIsLookedAt = mpS->cb.is_looked_at;

	SaveConnections(apSaveData->mvConnections);

	for(tLuxCollideCallbackListIt it = mlstCollideCallbacks.begin(); it != mlstCollideCallbacks.end(); ++it)
	{
		cLuxCollideCallback *pCallback = *it;
		cLuxCollideCallback_SaveData saveCallback;
		saveCallback.FromCallback(pCallback);

		apSaveData->mlstCollideCallbacks.Add(saveCallback);
	}
}

//-----------------------------------------------------------------------

void iLuxEntity::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	SetEntStr(kEntStrName, apSaveData->msName);
	mpS->full_game_save = apSaveData->mbFullGameSave;
	SetActive(apSaveData->mbActive);
	mpS->id = apSaveData->mlID;
	mpS->interaction_disabled = apSaveData->mbInteractionDisabled;

	mpS->max_focus_distance = apSaveData->mfMaxFocusDistance;

	SetEntStr(kEntStrCallback, apSaveData->msCallbackFunc);
	SetEntStr(kEntStrConnChange, apSaveData->msConnectionStateChangeCallback);

	SetEntStr(kEntStrInteract, apSaveData->msInteractCallback);
	mpS->cb.interact_remove = apSaveData->mbInteractCallbackRemove;

	SetEntStr(kEntStrLookAt, apSaveData->msLookAtCallback);
	mpS->cb.lookat_remove = apSaveData->mbLookAtCallbackRemove;
	mpS->cb.is_looked_at = apSaveData->mbIsLookedAt;
}

//-----------------------------------------------------------------------

void iLuxEntity::SaveConnections(cContainerVec<cLuxEntityConnection_SaveData> &avOut)
{
	int lNum = ohpl_ent_conn_num(mpS);
	avOut.Resize(lNum);
	for(int i=0; i<lNum; ++i)
	{
		cLuxEntityConnection_SaveData &c = avOut[i];
		size_t lLen;
		const unsigned char *p = ohpl_ent_conn_str(mpS, i, 0, &lLen);
		c.msName.assign((const char*)p, lLen);
		p = ohpl_ent_conn_str(mpS, i, 1, &lLen);
		c.msCallbackFunc.assign((const char*)p, lLen);
		ohpl_ent_conn_info(mpS, i, &c.mlEntityId, &c.mbInvertStateSent, &c.mlStatesUsed);
	}
}

//-----------------------------------------------------------------------

void iLuxEntity::LoadConnections(cContainerVec<cLuxEntityConnection_SaveData> &avIn, cLuxMap *apMap)
{
	ohpl_ent_conn_clear(mpS);
	for(size_t i=0; i<avIn.Size(); ++i)
	{
		cLuxEntityConnection_SaveData &c = avIn[i];
		iLuxEntity *pEntity = apMap->GetEntityByID(c.mlEntityId);
		if(ohpl_ga4c_ent_connection_missing(pEntity != NULL))
		{
			Error("Could not find entity  with id %d for connection '%s'\n", c.mlEntityId, c.msName.c_str());
		}
		AddConnection(c.msName, pEntity, c.mbInvertStateSent, c.mlStatesUsed, c.msCallbackFunc);
	}
}

//-----------------------------------------------------------------------

void iLuxEntity::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	LoadConnections(apSaveData->mvConnections, GetMap());

	cContainerListIterator<cLuxCollideCallback_SaveData> it = apSaveData->mlstCollideCallbacks.GetIterator();
	while(it.HasNext())
	{
		cLuxCollideCallback_SaveData& saveCallback = it.Next();
		cLuxCollideCallback *pCallback = hplNew(cLuxCollideCallback, ());
		
		saveCallback.ToCallback(GetMap(), this, pCallback);
		mlstCollideCallbacks.push_back(pCallback);
	}
}

//-----------------------------------------------------------------------
