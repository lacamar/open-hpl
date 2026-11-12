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
	if(pSound && pAttachEnt)
	{
		if(abAttach)	pAttachEnt->AddChild(pSound);
		else			pSound->SetPosition(pAttachEnt->GetWorldPosition());
	}

	return pSound;
}

//-----------------------------------------------------------------------

void iLuxEntity::RunCallbackFunc(const tString& asType)
{
	if(msCallbackFunc=="")return;

	mpMap->RunScript(msCallbackFunc + "(\""+msName+"\", \""+asType+"\")" );
}

//-----------------------------------------------------------------------

void iLuxEntity::RunInteractCallbackFunc()
{
	if(msInteractCallback=="")return;
	
	mpMap->RunScript(msInteractCallback + "(\""+msName+"\")");
	
	if(mbInteractCallbackRemove) msInteractCallback = "";
}

//-----------------------------------------------------------------------

void iLuxEntity::SetActive(bool abX)
{
	if(mbActive == abX) return;

	mbActive = abX;

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

void iLuxEntity::UpdatePlayerLookAt(float afTimeStep)
{
	if(msLookAtCallback == "") return;

	if(ohpl_ga4b_entity_look_timer(&mfLookAtCount, afTimeStep)==false) return;

	bool bLookingAt=false;
	cCamera *pCamera = gpBase->mpPlayer->GetCamera();
	cFrustum *pFrustum = pCamera->GetFrustum();

	OhplGa4bLos los;
	los.ctx = NULL;
	los.los = &LuxEntityLineOfSight;

	for(int i = 0; i < GetBodyNum(); i++)
	{
		iPhysicsBody *pBody = GetBody(i);
		cBoundingVolume *pBV = pBody->GetBoundingVolume();

		if(pFrustum->CollideBoundingVolume(pBV) == eCollision_Outside)
		{
			continue;
		}

		OhplGa4bLookIn in;
		cVector3f vCamPos = pCamera->GetPosition();
		cVector3f vCamFwd = pCamera->GetForward();
		cVector3f vCamUp = pCamera->GetUp();
		cVector3f vCamRight = pCamera->GetRight();
		cVector3f vMin = pBV->GetMin();
		cVector3f vMax = pBV->GetMax();
		cVector3f vCenter = pBV->GetWorldCenter();
		memcpy(in.cam_pos, vCamPos.v, sizeof(in.cam_pos));
		memcpy(in.cam_fwd, vCamFwd.v, sizeof(in.cam_fwd));
		memcpy(in.cam_up, vCamUp.v, sizeof(in.cam_up));
		memcpy(in.cam_right, vCamRight.v, sizeof(in.cam_right));
		memcpy(in.bv_min, vMin.v, sizeof(in.bv_min));
		memcpy(in.bv_max, vMax.v, sizeof(in.bv_max));
		memcpy(in.bv_center, vCenter.v, sizeof(in.bv_center));
		in.bv_radius = pBV->GetRadius();

		if(ohpl_ga4b_entity_look_body(&in, &los))
		{
			bLookingAt = true;
			break;
		}
	}

	if(bLookingAt && mbIsLookedAt == false)
	{
		tString sTempCallback = msLookAtCallback;
		if(mbLookAtCallbackRemove) msLookAtCallback = "";
		mpMap->RunScript(sTempCallback + "(\""+msName+"\", 1)" );
	}
	else if(bLookingAt==false && mbIsLookedAt)
	{
		mpMap->RunScript(msLookAtCallback + "(\""+msName+"\", -1)" );
	}

	mbIsLookedAt = bLookingAt;
}

//-----------------------------------------------------------------------

void iLuxEntity::ConnectionStateChange(int alState)
{
	//////////////////////////////////
	// Callback
	if(msConnectionStateChangeCallback != "")
	{
		mpMap->RunScript(	msConnectionStateChangeCallback + 
							"(\""+msName+"\", "+ cString::ToString(alState) + ")");
	}

    //////////////////////////////////
	// Iterate connections
	for(size_t i=0; i< mvConnections.size(); ++i)
	{
		cLuxEntityConnection *pConn = mvConnections[i];
		
		int lState;
		if(ohpl_ga4b_entity_conn_state(alState, pConn->GetStateUsed(), pConn->GetInvertStateSent(), &lState)==false) continue;

		//Log("Send state '%s'%d to '%s'as %d\n", msName.c_str(), alState, pConn->GetEntity()->GetName().c_str(), lState);

        pConn->GetEntity()->OnConnectionStateChange(this,lState);

		if(pConn->GetCallbackFunc()!="")
		{
			//Syntax: ConnectionName,ParentEnt, ChildEnt, state
			tString sCommand = pConn->GetCallbackFunc() + "(\"" + pConn->GetName() + "\"," +
															"\"" + msName +  + "\"," +
															"\"" + pConn->GetEntity()->GetName() + "\"," +
															cString::ToString(lState) + ")";
			mpMap->RunScript(sCommand);		
		}
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
	if(pEntityDoc==NULL){
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
	if(pMesh)
		mvPreloadedMeshes.push_back(pMesh);
	else
		Error("Could not load mesh '%s' when preloading!\n",sModelFile.c_str());

	pResources->DestroyXmlDocument(pEntityDoc);
}

//-----------------------------------------------------------------------

bool iLuxEntity::CollidesWithPlayer()
{
	iPhysicsWorld *pPhysicsWorld = mpMap->GetPhysicsWorld();
	cCollideData collideData;
	collideData.SetMaxSize(1);

	iPhysicsBody *pPlayerBody = gpBase->mpPlayer->GetCharacterBody()->GetCurrentBody();

	for(int i=0; i<GetBodyNum(); ++i)
	{
		iPhysicsBody *pBody = GetBody(i);

		if(cMath::CheckBVIntersection(*pPlayerBody->GetBoundingVolume(), *pBody->GetBoundingVolume())==false)
		{
			continue;
		}
		
		if(pPhysicsWorld->CheckShapeCollision(pBody->GetShape(), pBody->GetLocalMatrix(), pPlayerBody->GetShape(), pPlayerBody->GetLocalMatrix(), collideData,1, false))
		{
			return true;
		}
	}
	return false;
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
	if(apConnection->mpEntity ==NULL)
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
