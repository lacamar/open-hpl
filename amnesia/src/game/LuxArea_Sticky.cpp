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

#include "LuxArea_Sticky.h"
#include "RustLuxMap.h"

#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxPlayerState.h"

#include "LuxProp.h"


//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxAreaLoader_Sticky::cLuxAreaLoader_Sticky(const tString& asName) : iLuxAreaLoader(asName)
{

}

cLuxAreaLoader_Sticky::~cLuxAreaLoader_Sticky()
{

}

//-----------------------------------------------------------------------

iLuxArea *cLuxAreaLoader_Sticky::CreateArea(const tString& asName, int alID, cLuxMap *apMap)
{
	cLuxArea_Sticky *pArea = hplNew(cLuxArea_Sticky, (asName, alID, apMap));
	return pArea;
}

//-----------------------------------------------------------------------

void cLuxAreaLoader_Sticky::LoadVariables(iLuxArea *apArea, cWorld *apWorld)
{
	cLuxArea_Sticky *pStickyArea = static_cast<cLuxArea_Sticky*>(apArea);


	pStickyArea->msAttachFunction = GetVarString("AttachFunction","");
	pStickyArea->msDetachFunction = GetVarString("DetachFunction","");

	pStickyArea->msAttachSound = GetVarString("AttachSound","");
	pStickyArea->msDetachSound = GetVarString("DetachSound","");

	pStickyArea->msAttachPS = GetVarString("AttachPS","");
	pStickyArea->msDetachPS = GetVarString("DetachPS","");

	pStickyArea->msAttachableBodyName = GetVarString("AttachableBodyName","");

	pStickyArea->mbMoveBody = GetVarBool("MoveBody",true);
	pStickyArea->mbRotateBody = GetVarBool("RotateBody",true);

	pStickyArea->mbCheckCenterInArea = GetVarBool("CheckCenterInArea",true);

	pStickyArea->mbCanDetach = GetVarBool("CanDetach",true);

	pStickyArea->mfPoseTime = GetVarFloat("PoseTime",1);

	gpBase->PreloadSound(pStickyArea->msAttachSound);
	gpBase->PreloadSound(pStickyArea->msDetachSound);
	gpBase->PreloadParticleSystem(pStickyArea->msAttachPS);
	gpBase->PreloadParticleSystem(pStickyArea->msDetachPS);
}

void cLuxAreaLoader_Sticky::SetupArea(iLuxArea *apArea, cWorld *apWorld)
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxArea_Sticky::cLuxArea_Sticky(const tString &asName, int alID, cLuxMap *apMap)  : iLuxArea(asName,alID,apMap, eLuxAreaType_Sticky),
	mbMoveBody(Pod<OhplA7Sticky>().moveBody),
	mbRotateBody(Pod<OhplA7Sticky>().rotateBody),
	mbCheckCenterInArea(Pod<OhplA7Sticky>().checkCenterInArea),
	mbCanDetach(Pod<OhplA7Sticky>().canDetach),
	mfPoseTime(Pod<OhplA7Sticky>().poseTime),
	mbAttachedBodyGravity(Pod<OhplA7Sticky>().attachedBodyGravity),
	mfAttachedBodyMass(Pod<OhplA7Sticky>().attachedBodyMass),
	mbAttachedEntityFullGameSaved(Pod<OhplA7Sticky>().attachedEntityFullGameSaved),
	mfSetMtxTime(Pod<OhplA7Sticky>().setMtxTime),
	mtxAttachedStart(*reinterpret_cast<cMatrixf*>(&Pod<OhplA7Sticky>().attachedStart)),
	mpAttachedBody(reinterpret_cast<iPhysicsBody*&>(Pod<OhplA7Sticky>().attachedBody)),
	mpLastAttachedBody(reinterpret_cast<iPhysicsBody*&>(Pod<OhplA7Sticky>().lastAttached))
{
	mpAttachedBody = NULL;
	mpLastAttachedBody = NULL;

	msAttachFunction = "";
	msDetachFunction = "";

	msAttachSound = "";
	msDetachFunction = "";

	msAttachSound = "";
	msDetachSound = "";

	mbMoveBody = true;
	mbRotateBody = true;

	mbCheckCenterInArea = true;

	mfPoseTime = 0.2f;

	mbCanDetach = true;

	mfSetMtxTime = 1.0f;

	mfAttachedBodyMass =0;
	mbAttachedEntityFullGameSaved = false;

	mbFullGameSave = true;
}

//-----------------------------------------------------------------------

cLuxArea_Sticky::~cLuxArea_Sticky()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxArea_Sticky::SetupAfterLoad(cWorld *apWorld)
{
	
}

//-----------------------------------------------------------------------

void cLuxArea_Sticky::OnUpdate(float afTimeStep)
{
	ohpl_a7_area_update(mpA7Area, this, mpMap->GetA7(), afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxArea_Sticky::AttachBody(iPhysicsBody *apBody)
{
	ohpl_a7_area_sticky_attach_body(mpA7Area, apBody);
}

//-----------------------------------------------------------------------

void cLuxArea_Sticky::DetachBody()
{
	ohpl_a7_area_sticky_detach(mpA7Area, this, mpMap->GetA7());
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxArea_Sticky_SaveData, iLuxArea_SaveData)

kSerializeVar(msAttachFunction, eSerializeType_String)
kSerializeVar(msDetachFunction, eSerializeType_String)

kSerializeVar(msAttachSound, eSerializeType_String)
kSerializeVar(msDetachSound, eSerializeType_String)

kSerializeVar(msAttachPS, eSerializeType_String)
kSerializeVar(msDetachPS, eSerializeType_String)

kSerializeVar(mbCanDetach, eSerializeType_Bool)

kSerializeVar(mfPoseTime, eSerializeType_Float32)

kSerializeVar(mbCheckCenterInArea, eSerializeType_Bool)

kSerializeVar(msAttachableBodyName, eSerializeType_String)

kSerializeVar(mlAttachedEntityID, eSerializeType_Int32)
kSerializeVar(mlAttachedBodyID, eSerializeType_Int32)

kSerializeVar(mfAttachedBodyMass, eSerializeType_Float32)
kSerializeVar(mbAttachedBodyGravity, eSerializeType_Bool)
kSerializeVar(mbAttachedEntityFullGameSaved, eSerializeType_Bool)

kSerializeVar(mfSetMtxTime, eSerializeType_Float32)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxArea* cLuxArea_Sticky_SaveData::CreateArea(cLuxMap *apMap)
{
	return hplNew(cLuxArea_Sticky, (msName, mlID, apMap));
}

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxArea_Sticky::CreateSaveData()
{
	return hplNew(cLuxArea_Sticky_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxArea_Sticky::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::SaveToSaveData(apSaveData);
	cLuxArea_Sticky_SaveData *pData = static_cast<cLuxArea_Sticky_SaveData*>(apSaveData);

	//////////////////////
	// Attached body
	if(mpAttachedBody)
	{
		iLuxEntity *pEntity = static_cast<iLuxEntity*>(mpAttachedBody->GetUserData());
		
		pData->mlAttachedBodyID = mpAttachedBody->GetUniqueID();
		pData->mlAttachedEntityID = pEntity->GetID();
	}
	else
	{
		pData->mlAttachedBodyID = -1;
		pData->mlAttachedEntityID = -1;
	}

	/////////////////////////
	// Properties
	kCopyToVar(pData,msAttachFunction);
	kCopyToVar(pData,msDetachFunction);

	kCopyToVar(pData,msAttachSound);
	kCopyToVar(pData,msDetachSound);

	kCopyToVar(pData,msAttachPS);
	kCopyToVar(pData,msDetachPS);

	kCopyToVar(pData,mbCanDetach);

	kCopyToVar(pData,mfPoseTime);

	kCopyToVar(pData,mbCheckCenterInArea);

	kCopyToVar(pData,msAttachableBodyName);

	kCopyToVar(pData,mfAttachedBodyMass);
	kCopyToVar(pData,mbAttachedBodyGravity);
	kCopyToVar(pData,mbAttachedEntityFullGameSaved);

	kCopyToVar(pData,mfSetMtxTime);
}

//-----------------------------------------------------------------------

void cLuxArea_Sticky::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::LoadFromSaveData(apSaveData);
	cLuxArea_Sticky_SaveData *pData = static_cast<cLuxArea_Sticky_SaveData*>(apSaveData);

	//////////////////////
	// Attached body
	if(pData->mlAttachedEntityID >=0)
	{
		iLuxEntity *pEntity = mpMap->GetEntityByID(pData->mlAttachedEntityID);
		if(pEntity && pEntity->GetEntityType() == eLuxEntityType_Prop)
		{
			iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
			mpAttachedBody = pProp->GetBodyFromID(pData->mlAttachedBodyID);
			if(mpAttachedBody==NULL)
			{
				Error("Could not load body with id %d in prop '%s'\n", pData->mlAttachedBodyID, pProp->GetName().c_str());
			}
		}
		else
		{
			Error("Could not load prop with id %d\n", pData->mlAttachedEntityID);
		}
		mpLastAttachedBody = mpAttachedBody;
	}
	else
	{
		mpAttachedBody = NULL;
		mpLastAttachedBody = NULL;
	}

	
	/////////////////////////
	// Properties
	kCopyFromVar(pData,msAttachFunction);
	kCopyFromVar(pData,msDetachFunction);

	kCopyFromVar(pData,msAttachSound);
	kCopyFromVar(pData,msDetachSound);

	kCopyFromVar(pData,msAttachPS);
	kCopyFromVar(pData,msDetachPS);

	kCopyFromVar(pData,mbCanDetach);

	kCopyFromVar(pData,mfPoseTime);

	kCopyFromVar(pData,mbCheckCenterInArea);

	kCopyFromVar(pData,msAttachableBodyName);

	kCopyFromVar(pData,mfAttachedBodyMass);
	kCopyFromVar(pData,mbAttachedBodyGravity);
	kCopyFromVar(pData,mbAttachedEntityFullGameSaved);

	kCopyFromVar(pData,mfSetMtxTime);
}

//-----------------------------------------------------------------------

void cLuxArea_Sticky::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);

}

//-----------------------------------------------------------------------

