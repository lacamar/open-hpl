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

#include "LuxPlayerState_InteractSlide.h"

#include "LuxPlayer.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxMapHelper.h"
#include "LuxProp.h"
#include "LuxInputHandler.h"

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerState_InteractSlide::cLuxPlayerState_InteractSlide(cLuxPlayer *apPlayer) : iLuxPlayerState_Interact(apPlayer, eLuxPlayerState_InteractSlide, &mR.base)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_init(&mR.base, &h, eLuxPlayerState_InteractSlide);
}

//-----------------------------------------------------------------------

cLuxPlayerState_InteractSlide::~cLuxPlayerState_InteractSlide()
{
	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractSlide::OnEnterState(eLuxPlayerState aPrevState)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_enter(mpA3, &h, aPrevState);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractSlide::OnLeaveState(eLuxPlayerState aNewState)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_leave(mpA3, &h, aNewState);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractSlide::Update(float afTimeStep)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_update(mpA3, &h, afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractSlide::PostUpdate(float afTimeStep)
{
	
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractSlide::OnDoAction(eLuxPlayerAction aAction,bool abPressed)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_do_action(mpA3, &h, aAction, abPressed);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractSlide::OnAddYaw(float afAmount)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_add_yaw(mpA3, &h, afAmount);
}

bool cLuxPlayerState_InteractSlide::OnAddPitch(float afAmount)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_add_pitch(mpA3, &h, afAmount);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractSlide::OnMove(eCharDir aDir, float afMul)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_on_move(mpA3, &h, aDir, afMul);
}

//-----------------------------------------------------------------------

cGuiGfxElement* cLuxPlayerState_InteractSlide::GetCrosshair()
{
	return NULL;
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractSlide::OnSaveBody(iPhysicsBody *apBody, float &afMass, bool &abCollideCharacter)
{
	
}

//-----------------------------------------------------------------------

float cLuxPlayerState_InteractSlide::DrawDebug(cGuiSet *apSet,iFontData *apFont, float afStartY)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_draw_debug(mpA3, &h, apSet, apFont, afStartY);
}

void cLuxPlayerState_InteractSlide::RenderSolid(cRendererCallbackFunctions* apFunctions)
{
	return;
	apFunctions->SetMatrix(NULL);

	cCamera *pCam = mpPlayer->GetCamera();
	cVector3f vUp = pCam->GetUp();
	cVector3f vRight = pCam->GetRight();
	cVector3f vForward = pCam->GetForward();

	cVector3f vCamRotation( pCam->GetPitch(), pCam->GetYaw(), pCam->GetRoll());
	cMatrixf mtxCamTransform = cMath::MatrixRotate(vCamRotation, eEulerRotationOrder_XYZ);
	mtxCamTransform.SetTranslation(pCam->GetPosition());

	cVector3f vWorldInteractPos = cMath::MatrixMul(mpCurrentBody->GetLocalMatrix(), mR.localInteractPos);

	apFunctions->GetLowLevelGfx()->DrawSphere(vWorldInteractPos, 0.1f, cColor(0,1,0,1));
	
	cVector3f vMouseCamDir = vUp * mvLastMouseAdd.y + vRight * -mvLastMouseAdd.x;
	float fSpeedAdd = cMath::Vector3Dot(vMouseCamDir, mpCurrentJoint->GetPinDir());

	apFunctions->GetLowLevelGfx()->DrawLine(vWorldInteractPos, vWorldInteractPos+vMouseCamDir*50, cColor(1,0,0,1));

	apFunctions->GetLowLevelGfx()->DrawLine(vWorldInteractPos, vWorldInteractPos+mpCurrentJoint->GetPinDir(),cColor(0,0,0.3f,1));
	apFunctions->GetLowLevelGfx()->DrawLine(vWorldInteractPos, vWorldInteractPos+mpCurrentJoint->GetPinDir()*fSpeedAdd*50,cColor(0,0,1,1));
	 

	//apFunctions->GetLowLevelGfx()->DrawLine(vPos, vPos+vRight, cColor(1,0,1,1));
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxPlayerState_InteractSlide_SaveData, iLuxPlayerState_Interact_SaveData)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxPlayerState_SaveData* cLuxPlayerState_InteractSlide::CreateSaveData()
{
	return hplNew(cLuxPlayerState_InteractSlide_SaveData, ());
}

//-----------------------------------------------------------------------


void cLuxPlayerState_InteractSlide::SaveToSaveData(iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::SaveToSaveData(apSaveData);
	cLuxPlayerState_InteractSlide_SaveData *pData = static_cast<cLuxPlayerState_InteractSlide_SaveData*>(apSaveData);

	
	///////////////////////
	// Save vars
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractSlide::LoadFromSaveDataBeforeEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveDataBeforeEnter(apMap,apSaveData);
	cLuxPlayerState_InteractSlide_SaveData *pData = static_cast<cLuxPlayerState_InteractSlide_SaveData*>(apSaveData);

	///////////////////////
	// Setup before entering
	
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractSlide::LoadFromSaveDataAfterEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveDataAfterEnter(apMap,apSaveData);
	cLuxPlayerState_InteractSlide_SaveData *pData = static_cast<cLuxPlayerState_InteractSlide_SaveData*>(apSaveData);

	///////////////////////
	// Load vars
}

//-----------------------------------------------------------------------



