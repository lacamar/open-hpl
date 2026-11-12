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

#include "LuxPlayerState_InteractSwingDoor.h"

#include "LuxPlayer.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxMapHelper.h"
#include "LuxProp.h"
#include "LuxMoveState_Normal.h"
#include "LuxInputHandler.h"

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerState_InteractSwingDoor::cLuxPlayerState_InteractSwingDoor(cLuxPlayer *apPlayer) : iLuxPlayerState_InteractRotateBase(apPlayer, eLuxPlayerState_InteractSwingDoor)
{
	
}

//-----------------------------------------------------------------------

cLuxPlayerState_InteractSwingDoor::~cLuxPlayerState_InteractSwingDoor()
{
	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------


//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

float cLuxPlayerState_InteractSwingDoor::GetSpeedAdd(cCamera *apCam)
{
	OhplGa2bJointGeom g;
	g.bodyMatrix = mpCurrentBody->GetLocalMatrix();
	g.massCentre = mpCurrentBody->GetMassCentre();
	g.pivot = mpCurrentJoint->GetPivotPoint();
	g.pin = mpCurrentJoint->GetPinDir();
	g.up = apCam->GetUp();
	g.right = apCam->GetRight();
	g.forward = apCam->GetForward();
	g.mouse = mvMouseAdd;
	return ohpl_ga2b_swing_door_speed_add(&g);
}

void cLuxPlayerState_InteractSwingDoor::OnThrow()
{
	cCamera *apCam = mpPlayer->GetCamera();
	OhplGa2bJointGeom g;
	g.bodyMatrix = mpCurrentBody->GetLocalMatrix();
	g.massCentre = mpCurrentBody->GetMassCentre();
	g.pivot = mpCurrentJoint->GetPivotPoint();
	g.pin = mpCurrentJoint->GetPinDir();
	g.up = apCam->GetUp();
	g.right = apCam->GetRight();
	g.forward = apCam->GetForward();
	g.mouse = mvMouseAdd;
	cVector3f vImpulse;
	ohpl_ga2b_swing_door_throw(&g, mpMoveBaseData->mfMoveThrowImpulse, &mvJointForward, &vImpulse);
	mpCurrentBody->AddImpulse(vImpulse);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxPlayerState_InteractSwingDoor_SaveData, iLuxPlayerState_InteractRotateBase_SaveData)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxPlayerState_SaveData* cLuxPlayerState_InteractSwingDoor::CreateSaveData()
{
	return hplNew(cLuxPlayerState_InteractSwingDoor_SaveData, ());
}

//-----------------------------------------------------------------------


void cLuxPlayerState_InteractSwingDoor::SaveToSaveData(iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::SaveToSaveData(apSaveData);
	cLuxPlayerState_InteractSwingDoor_SaveData *pData = static_cast<cLuxPlayerState_InteractSwingDoor_SaveData*>(apSaveData);


	///////////////////////
	// Save vars
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractSwingDoor::LoadFromSaveDataBeforeEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveDataBeforeEnter(apMap,apSaveData);
	cLuxPlayerState_InteractSwingDoor_SaveData *pData = static_cast<cLuxPlayerState_InteractSwingDoor_SaveData*>(apSaveData);

	///////////////////////
	// Load vars
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractSwingDoor::LoadFromSaveDataAfterEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveDataAfterEnter(apMap,apSaveData);
	cLuxPlayerState_InteractSwingDoor_SaveData *pData = static_cast<cLuxPlayerState_InteractSwingDoor_SaveData*>(apSaveData);

	///////////////////////
	// Load vars
}

//-----------------------------------------------------------------------





