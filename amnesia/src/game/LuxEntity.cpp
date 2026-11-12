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

iLuxEntity::iLuxEntity(const tString &asName, int alID, cLuxMap *apMap, eLuxEntityType aEntityType) : iLuxCollideCallbackContainer()
{
	msName = asName;
	mpMap = apMap;
	mEntityType = aEntityType;
	mlID = alID;
	mbActive = true;
	mbFullGameSave = false;

	msCallbackFunc = "";
	msConnectionStateChangeCallback = "";

	mfMaxFocusDistance = 2.0f;

	mbDestroyMe = false;
	mbIsSaved = true;

	mCustomFocusCrossHair = eLuxFocusCrosshair_Default;

	msInteractCallback = "";
	mbInteractCallbackRemove = true;

	msLookAtCallback = "";
	mbLookAtCallbackRemove = true;
	mfLookAtCount =0;
	mbIsLookedAt = false;

	mbInteractionDisabled = false;
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

	STLDeleteAll(mvConnections);

	DestroyCollideCallbacks();
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

	cSoundEntity *pSound = mpMap->GetWorld()->CreateSoundEntity(msName + "_" + asName,asFile,abRemoveWhenDone);
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
	ohpl_ga4c_ent_run_callback(msCallbackFunc != "", sink.Sink());
}

//-----------------------------------------------------------------------

void iLuxEntity::RunInteractCallbackFunc()
{
	OhplGa4cEntSink sink(this);
	ohpl_ga4c_ent_run_interact(&mGa4cCb, msInteractCallback != "", sink.Sink());
}

//-----------------------------------------------------------------------

void iLuxEntity::SetActive(bool abX)
{
	if(ohpl_ga4c_ent_set_active(&mbActive, abX) == false) return;

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
	msInteractCallback = asCallbackFunc;
	mbInteractCallbackRemove = abRemoveWhenInteracted;
}

//-----------------------------------------------------------------------

void iLuxEntity::SetPlayerLookAtCallback(const tString &asCallbackFunc, bool abRemoveWhenLookedAt)
{
	msLookAtCallback = asCallbackFunc;
	mbLookAtCallbackRemove = abRemoveWhenLookedAt;
}

//-----------------------------------------------------------------------

void iLuxEntity::AddConnection(const tString& asName, iLuxEntity *apEntity, bool abInvertStateSent, int alStatesUsed, const tString &asCallbackFunc)
{
	cLuxEntityConnection *pConnection = hplNew(cLuxEntityConnection, (asName, apEntity,abInvertStateSent, alStatesUsed, asCallbackFunc));

	mvConnections.push_back(pConnection);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void iLuxEntity::UpdateCheckCollideCallback(float afTimeStep)
{
	CheckCollisionCallback(msName, mpMap);
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
	ohpl_ga4c_ent_look_at(&mGa4cCb, msLookAtCallback != "", afTimeStep, host.Host(), &los, sink.Sink());
}

//-----------------------------------------------------------------------

class cLuxEntityConnHost
{
public:
	cLuxEntityConnHost(iLuxEntity *apEntity) : mpEntity(apEntity)
	{
		mHost.ctx = this; mHost.conn_num = &ConnNum; mHost.conn = &Conn; mHost.has_callback = &HasCallback;
	}
	const OhplGa4cConnHost *Host() const { return &mHost; }

private:
	static int ConnNum(void *c) { return ((cLuxEntityConnHost*)c)->mpEntity->GetConnectionNum(); }
	static void Conn(void *c, int i, int *apUsed, bool *apInvert)
	{
		cLuxEntityConnection *pConn = ((cLuxEntityConnHost*)c)->mpEntity->GetConnection(i);
		*apUsed = pConn->GetStateUsed();
		*apInvert = pConn->GetInvertStateSent();
	}
	static bool HasCallback(void *c, int i) { return ((cLuxEntityConnHost*)c)->mpEntity->GetConnection(i)->GetCallbackFunc() != ""; }

	iLuxEntity *mpEntity;
	OhplGa4cConnHost mHost;
};

//-----------------------------------------------------------------------

void iLuxEntity::ConnectionStateChange(int alState)
{
	cLuxEntityConnHost host(this);
	OhplGa4cEntSink sink(this);
	ohpl_ga4c_ent_conn_change(alState, msConnectionStateChangeCallback != "", host.Host(), sink.Sink());
}

//-----------------------------------------------------------------------

void iLuxEntity::Ga4cApplyOp(const OhplGa4Op &aOp, const tString *apType)
{
	switch(aOp.code)
	{
	case kOhplGa4cOp_EntCallback:
		mpMap->RunScript(msCallbackFunc + "(\""+msName+"\", \""+*apType+"\")" );
		break;
	case kOhplGa4cOp_EntInteract:
		mpMap->RunScript(msInteractCallback + "(\""+msName+"\")");
		break;
	case kOhplGa4cOp_EntClearInteract:
		msInteractCallback = "";
		break;
	case kOhplGa4cOp_EntLookAtEnter:
		{
			tString sTempCallback = msLookAtCallback;
			if(aOp.i) msLookAtCallback = "";
			mpMap->RunScript(sTempCallback + "(\""+msName+"\", 1)" );
		}
		break;
	case kOhplGa4cOp_EntLookAtLeave:
		mpMap->RunScript(msLookAtCallback + "(\""+msName+"\", -1)" );
		break;
	case kOhplGa4cOp_EntConnChangeCb:
		mpMap->RunScript(	msConnectionStateChangeCallback +
							"(\""+msName+"\", "+ cString::ToString(aOp.i) + ")");
		break;
	case kOhplGa4cOp_EntConnSend:
		mvConnections[aOp.i]->GetEntity()->OnConnectionStateChange(this, aOp.j);
		break;
	case kOhplGa4cOp_EntConnScript:
		{
			cLuxEntityConnection *pConn = mvConnections[aOp.i];
			//Syntax: ConnectionName,ParentEnt, ChildEnt, state
			tString sCommand = pConn->GetCallbackFunc() + "(\"" + pConn->GetName() + "\"," +
															"\"" + msName +  + "\"," +
															"\"" + pConn->GetEntity()->GetName() + "\"," +
															cString::ToString(aOp.j) + ")";
			mpMap->RunScript(sCommand);
		}
		break;
	}
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
	iPhysicsWorld *pPhysicsWorld = mpMap->GetPhysicsWorld();
	iPhysicsBody *pPlayerBody = gpBase->mpPlayer->GetCharacterBody()->GetCurrentBody();

	cLuxEntityCollideHost host(this, pPhysicsWorld, pPlayerBody);
	return ohpl_ga4c_ent_collides(host.Host());
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxEntityConnection_SaveData)
kSerializeVar(msName,eSerializeType_String)
kSerializeVar(mlEntityId,eSerializeType_Int32)
kSerializeVar(mbInvertStateSent,eSerializeType_Bool)
kSerializeVar(mlStatesUsed,eSerializeType_Int32)
kSerializeVar(msCallbackFunc,eSerializeType_String)
kEndSerialize()

void cLuxEntityConnection_SaveData::FromConnection(cLuxEntityConnection *apConnection)
{
	mlEntityId = apConnection->mpEntity->GetID();
	
	kCopyFromVar(apConnection, msName);
	kCopyFromVar(apConnection, mbInvertStateSent);
	kCopyFromVar(apConnection, mlStatesUsed);
	kCopyFromVar(apConnection, msCallbackFunc);
}

void cLuxEntityConnection_SaveData::ToConnection(cLuxEntityConnection *apConnection, cLuxMap *apMap)
{
	apConnection->mpEntity = apMap->GetEntityByID(mlEntityId);
	if(ohpl_ga4c_ent_connection_missing(apConnection->mpEntity != NULL))
	{
		Error("Could not find entity  with id %d for connection '%s'\n", mlEntityId, msName.c_str());
	}

	kCopyToVar(apConnection, msName);
	kCopyToVar(apConnection, mbInvertStateSent);
	kCopyToVar(apConnection, mlStatesUsed);
	kCopyToVar(apConnection, msCallbackFunc);
}

//-----------------------------------------------------------------------

kBeginSerializeBaseVirtual(iLuxEntity_SaveData)
kSerializeVar(msName,eSerializeType_String)
kSerializeVar(mbFullGameSave,eSerializeType_Bool)
kSerializeVar(mbActive,eSerializeType_Bool)
kSerializeVar(mlEntityType,eSerializeType_Int32)
kSerializeVar(mlID,eSerializeType_Int32)

kSerializeVar(mfMaxFocusDistance, eSerializeType_Float32)

kSerializeVar(mbInteractionDisabled, eSerializeType_Bool)

kSerializeVar(msCallbackFunc, eSerializeType_String)
kSerializeVar(msConnectionStateChangeCallback, eSerializeType_String)

kSerializeVar(msInteractCallback,eSerializeType_String)
kSerializeVar(mbInteractCallbackRemove,eSerializeType_Bool)

kSerializeVar(msLookAtCallback,eSerializeType_String)
kSerializeVar(mbLookAtCallbackRemove,eSerializeType_Bool)
kSerializeVar(mbIsLookedAt,eSerializeType_Bool)

kSerializeClassContainer(mvConnections, cLuxEntityConnection_SaveData, eSerializeType_Class)
kSerializeClassContainer(mlstCollideCallbacks, cLuxCollideCallback_SaveData, eSerializeType_Class)

kEndSerialize()

//-----------------------------------------------------------------------

void iLuxEntity::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	apSaveData->mlEntityType = mEntityType;

	kCopyToVar(apSaveData, msName);
	kCopyToVar(apSaveData, mbFullGameSave);
	kCopyToVar(apSaveData, mbActive);
	kCopyToVar(apSaveData, mlID);
	kCopyToVar(apSaveData, mbInteractionDisabled);

	kCopyToVar(apSaveData, mfMaxFocusDistance);

	kCopyToVar(apSaveData, msCallbackFunc);
	kCopyToVar(apSaveData, msConnectionStateChangeCallback);

	kCopyToVar(apSaveData, msInteractCallback);
	kCopyToVar(apSaveData, mbInteractCallbackRemove);

	kCopyToVar(apSaveData, msLookAtCallback);
	kCopyToVar(apSaveData, mbLookAtCallbackRemove);
	kCopyToVar(apSaveData, mbIsLookedAt);

	apSaveData->mvConnections.Resize(mvConnections.size());
    for(size_t i=0; i<mvConnections.size();++i)
	{
		apSaveData->mvConnections[i].FromConnection(mvConnections[i]);
	}

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
	kCopyFromVar(apSaveData, msName);
	kCopyFromVar(apSaveData, mbFullGameSave);
	SetActive(apSaveData->mbActive);
	kCopyFromVar(apSaveData, mlID);
	kCopyFromVar(apSaveData, mbInteractionDisabled);

	kCopyFromVar(apSaveData, mfMaxFocusDistance);

	kCopyFromVar(apSaveData, msCallbackFunc);
	kCopyFromVar(apSaveData, msConnectionStateChangeCallback);

	kCopyFromVar(apSaveData, msInteractCallback);
	kCopyFromVar(apSaveData, mbInteractCallbackRemove);

	kCopyFromVar(apSaveData, msLookAtCallback);
	kCopyFromVar(apSaveData, mbLookAtCallbackRemove);
	kCopyFromVar(apSaveData, mbIsLookedAt);
}

//-----------------------------------------------------------------------

void iLuxEntity::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	mvConnections.resize(apSaveData->mvConnections.Size());
	for(size_t i=0; i<apSaveData->mvConnections.Size();++i)
	{
		mvConnections[i] = hplNew( cLuxEntityConnection, () );
		apSaveData->mvConnections[i].ToConnection(mvConnections[i], mpMap);
	}

	cContainerListIterator<cLuxCollideCallback_SaveData> it = apSaveData->mlstCollideCallbacks.GetIterator();
	while(it.HasNext())
	{
		cLuxCollideCallback_SaveData& saveCallback = it.Next();
		cLuxCollideCallback *pCallback = hplNew(cLuxCollideCallback, ());
		
		saveCallback.ToCallback(mpMap, this, pCallback);
		mlstCollideCallbacks.push_back(pCallback);
	}
}

//-----------------------------------------------------------------------
