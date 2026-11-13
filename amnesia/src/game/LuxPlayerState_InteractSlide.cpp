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
#include "impl/RustBackends.h"
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
	if(OhplBackendRustRequested())
	{
		OhplA3Host h = A3HostTable();
		ohpl_a3_init(&mR.base, &h, eLuxPlayerState_InteractSlide);
		return;
	}

	mfMoveToMouseAddFactor = gpBase->mpGameCfg->GetFloat("Player_Interaction","MoveToMouseAddFactor", 0.01f);
	mfMaxForce = gpBase->mpGameCfg->GetFloat("Player_Interaction","SlideMaxForce", 1000.0f);

	ohpl_ga2b_pid_init(&mForcePid, 10, 6, 0, 0.1f);
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
	if(OhplBackendRustRequested())
	{
		OhplA3Host h = A3HostTable();
		ohpl_a3_enter(mpA3, &h, aPrevState);
		return;
	}
	/////////////////////////////////
	//Get the static variables
	SetupInteractVars();

	mpSlideData = mpCurrentProp->GetSlideData();

	mpCurrentJoint = mpCurrentBody->GetJoint(0);

	//////////////////////////////
	//Reset variables
	ohpl_ga2b_pid_reset(&mForcePid);

	mvMouseAdd =0;
	mvLastMouseAdd =0;

	mfSlideSpeed =0;

	/////////////////////////////////
	//Setup body
	mbHasGravity = mpCurrentBody->GetGravity();

	mpCurrentBody->SetGravity(false);


	//////////////////////////////
	//Setup variables

	//Get the local position
	cMatrixf mtxTransformInv = cMath::MatrixInverse(mpCurrentBody->GetLocalMatrix());
	mvLocalInteractPos = cMath::MatrixMul(mtxTransformInv ,mvCurrentFocusPos);
	
			
	///////////////////////
	//Calculate the max distance
	mfMaxDistance = cMath::Vector3Dist(mpPlayer->GetCamera()->GetPosition(), mvCurrentFocusPos)*1.4f;
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractSlide::OnLeaveState(eLuxPlayerState aNewState)
{
	if(OhplBackendRustRequested())
	{
		OhplA3Host h = A3HostTable();
		ohpl_a3_leave(mpA3, &h, aNewState);
		return;
	}
	mpCurrentBody->SetGravity(mbHasGravity);

	ResetInteractVars();
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractSlide::Update(float afTimeStep)
{
	if(OhplBackendRustRequested())
	{
		OhplA3Host h = A3HostTable();
		ohpl_a3_update(mpA3, &h, afTimeStep);
		return;
	}
	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
	cCamera *pCam = mpPlayer->GetCamera();

	OhplGa2bSlideIn si;
	si.camPos = pCam->GetPosition();
	si.up = pCam->GetUp();
	si.right = pCam->GetRight();
	si.bodyMatrix = mpCurrentBody->GetLocalMatrix();
	si.localInteract = mvLocalInteractPos;
	si.mouse = mvMouseAdd;
	si.pin = mpCurrentJoint->GetPinDir();
	si.vel = mpCurrentBody->GetLinearVelocity();
	si.mass = mpCurrentBody->GetMass();
	si.maxDistance = mfMaxDistance;
	si.slowDownFactor = mpSlideData->mfSlideSlowDownFactor;
	si.speedFactor = mpSlideData->mfSlideSpeedFactor;
	si.maxSpeed = mpSlideData->mfSlideMaxSpeed;
	si.maxForce = mfMaxForce;
	OhplGa2bSlideOut so;
	ohpl_ga2b_slide_update(&si, &mfSlideSpeed, &mForcePid, afTimeStep, &so);

	if(so.outOfRange)
	{
		mpPlayer->ChangeState(mPreviousState);
	}

	mpCurrentBody->AddForce(so.force);
	mvLastForce = so.force;

	mvLastMouseAdd = mvMouseAdd;
	mvMouseAdd = 0;
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractSlide::PostUpdate(float afTimeStep)
{
	
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractSlide::OnDoAction(eLuxPlayerAction aAction,bool abPressed)
{
	if(OhplBackendRustRequested())
	{
		OhplA3Host h = A3HostTable();
		return ohpl_a3_do_action(mpA3, &h, aAction, abPressed);
	}
	int lKind = ohpl_ga2b_interact_action(OhplGa2bActOf(aAction), abPressed);
	if(lKind == 1)
	{
		mpPlayer->ChangeState(mPreviousState);
		return false;
	}
	else if(lKind == 2)
	{
		cVector3f vPin = mpCurrentJoint->GetPinDir();
		cVector3f vForward = mpPlayer->GetCamera()->GetForward();
		cVector3f vImpulse;
		ohpl_ga2b_slide_throw(&vPin, &vForward, mpSlideData->mfSlideThrowImpulse, &vImpulse);
		mpCurrentBody->AddImpulse(vImpulse);
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractSlide::OnAddYaw(float afAmount)
{
	if(OhplBackendRustRequested())
	{
		OhplA3Host h = A3HostTable();
		return ohpl_a3_add_yaw(mpA3, &h, afAmount);
	}
	mvMouseAdd.x += afAmount;

	return false;
}

bool cLuxPlayerState_InteractSlide::OnAddPitch(float afAmount)
{
	if(OhplBackendRustRequested())
	{
		OhplA3Host h = A3HostTable();
		return ohpl_a3_add_pitch(mpA3, &h, afAmount);
	}
#ifdef USE_GAMEPAD
	////////////////
	// We have both gamepad and mouse connected?
	if(gpBase->mpInputHandler->IsGamepadPresent())
	{
		/////////////
		// Check which one of them was used for the input this frame
#if USE_SDL2
		if(cMath::Abs(gpBase->mpInputHandler->GetGamepad()->GetAxisValue(eGamepadAxis_RightY)) > 0.0f)
#else
		if(cMath::Abs(gpBase->mpInputHandler->GetGamepad()->GetAxisValue(eGamepadAxis_3)) > 0.0f)
#endif
		{
			//Gamepad was used
			if(gpBase->mpInputHandler->GetInvertGamepadLook()) afAmount = -afAmount;
		}
		else if(gpBase->mpInputHandler->GetInvertMouse()) afAmount = -afAmount;
	}
	else if(gpBase->mpInputHandler->GetInvertMouse()) afAmount = -afAmount;
#else
	if(gpBase->mpInputHandler->GetInvertMouse()) afAmount = -afAmount;
#endif
	mvMouseAdd.y += afAmount;

	return false;
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractSlide::OnMove(eCharDir aDir, float afMul)
{
	if(OhplBackendRustRequested())
	{
		OhplA3Host h = A3HostTable();
		return ohpl_a3_on_move(mpA3, &h, aDir, afMul);
	}
	ohpl_ga2b_move_to_mouse(aDir == eCharDir_Forward, afMul, mfMoveToMouseAddFactor, &mvMouseAdd);
	return true;
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
	if(OhplBackendRustRequested())
	{
		OhplA3Host h = A3HostTable();
		return ohpl_a3_draw_debug(mpA3, &h, apSet, apFont, afStartY);
	}
	
	apSet->DrawFont(apFont, cVector3f(5,afStartY,0),12,cColor(1,1),_W("SideSpeed: %f Force: %ls"), mfSlideSpeed, cString::To16Char(mvLastForce.ToString()).c_str());
	afStartY += 13;

	

	return afStartY;
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

	cVector3f vWorldInteractPos = cMath::MatrixMul(mpCurrentBody->GetLocalMatrix(), mvLocalInteractPos);

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



