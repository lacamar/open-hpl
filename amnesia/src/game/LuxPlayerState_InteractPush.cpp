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

#include "LuxPlayerState_InteractPush.h"

#include "LuxPlayer.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxMapHelper.h"
#include "LuxProp.h"
#include "LuxMoveState_Normal.h"

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerState_InteractPush::cLuxPlayerState_InteractPush(cLuxPlayer *apPlayer) : iLuxPlayerState_Interact(apPlayer, eLuxPlayerState_InteractPush)
{
	mfWalkMaxSpeed = gpBase->mpGameCfg->GetFloat("Player_Interaction","PushMaxSpeedWalk",0);
	mfWalkForce = gpBase->mpGameCfg->GetFloat("Player_Interaction","PushForceWalk",0);
	
	mfRunMaxSpeed = gpBase->mpGameCfg->GetFloat("Player_Interaction","PushMaxSpeedRun",0);
	mfRunForce = gpBase->mpGameCfg->GetFloat("Player_Interaction","PushForceRun",0);
	
	mfCrouchMaxSpeed = gpBase->mpGameCfg->GetFloat("Player_Interaction","PushMaxSpeedCrouch",0);
	mfCrouchForce = gpBase->mpGameCfg->GetFloat("Player_Interaction","PushForceCrouch",0);
	
	mfYawRange = cMath::ToRad(gpBase->mpGameCfg->GetFloat("Player_Interaction","PushYawRange",0));
	mfPitchMinRange = cMath::ToRad(gpBase->mpGameCfg->GetFloat("Player_Interaction","PushPitchMinRange",0));
	mfPitchMaxRange = cMath::ToRad(gpBase->mpGameCfg->GetFloat("Player_Interaction","PushPitchMaxRange",0));
	
	mfMaxForce = gpBase->mpGameCfg->GetFloat("Player_Interaction","PushMaxForce",0);

	ohpl_ga2b_pid_init(&mVelocityPid, 20, 15, 0, 0.1f);
	ohpl_ga2b_pid_init(&mStopVelocityPid, 20, 10, 0, 0.1f);
}

//-----------------------------------------------------------------------

cLuxPlayerState_InteractPush::~cLuxPlayerState_InteractPush()
{
	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractPush::OnEnterState(eLuxPlayerState aPrevState)
{
	/////////////////////////////////
	//Get the static variables
	SetupInteractVars();
	
	cVector3f vInteractPos = cLuxPlayerStateVars::mvInteractPos;

	mpPushData = mpCurrentProp->GetPushData();
	
	/////////////////////////////////
	//Reset variables
	ClearMoveVars();

	ohpl_ga2b_pid_reset(&mVelocityPid);
	ohpl_ga2b_pid_reset(&mStopVelocityPid);
	
	mvPushDir =0;
	
	/////////////////////////////////
	//Set up character body
	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
	pCharBody->SetDisableDiagSpeedBoost(false);

	/////////////////////////////////
	//Set up camera
	cCamera *pCam = mpPlayer->GetCamera();
	mfPrevMinPitchLimit = pCam->GetPitchMinLimit();
	mfPrevMaxPitchLimit = pCam->GetPitchMaxLimit();

	OhplGa2bPushEnterIn pi;
	pi.pitch = pCam->GetPitch();
	pi.yaw = pCam->GetYaw();
	pi.pitchMinRange = mfPitchMinRange;
	pi.pitchMaxRange = mfPitchMaxRange;
	pi.yawRange = mfYawRange;
	pi.atPoint = mpPushData->mbPushAtPoint;
	pi.bodyMatrix = mpCurrentBody->GetLocalMatrix();
	pi.bodyPos = mpCurrentBody->GetLocalPosition();
	pi.feet = pCharBody->GetFeetPosition();
	pi.interactPos = vInteractPos;
	OhplGa2bPushEnterOut po;
	po.localAttach = mvLocalAttachPos;
	ohpl_ga2b_push_enter(&pi, &po);

	pCam->SetPitchLimits(po.pitchMin, po.pitchMax);
	pCam->SetYawLimits(po.yawMin, po.yawMax);

	mvLocalAttachPos = po.localAttach;
	mfMaxAttachPointDist = po.maxAttachDist;
}

//-----}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractPush::OnLeaveState(eLuxPlayerState aNewState)
{
	if(mpPlayer->GetCharacterBody())
	{
		mpPlayer->GetCharacterBody()->SetDisableDiagSpeedBoost(true);
	}

	mpPlayer->GetCamera()->SetPitchLimits(mfPrevMinPitchLimit, mfPrevMaxPitchLimit);
	mpPlayer->GetCamera()->SetYawLimits(0, 0);

	ResetInteractVars();
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractPush::Update(float afTimeStep)
{
	iPhysicsWorld *pPhysicsWorld = gpBase->mpMapHandler->GetCurrentMap()->GetWorld()->GetPhysicsWorld();
	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();

	OhplGa2bPushBegin pb;
	pb.atPoint = mpPushData->mbPushAtPoint;
	pb.bodyMatrix = mpCurrentBody->GetLocalMatrix();
	pb.localAttach = mvLocalAttachPos;
	pb.bodyPos = mpCurrentBody->GetLocalPosition();
	pb.feet = pCharBody->GetFeetPosition();
	pb.maxAttachDist = mfMaxAttachPointDist;
	pb.forward = pCharBody->GetForward();
	pb.right = pCharBody->GetRight();
	for(int i=0; i<2; ++i)
	{
		pb.moving[i] = mbMoving[i];
		pb.totalMoveMul[i] = mfTotalMoveMul[i];
	}
	OhplGa2bPushBeginOut pbo;
	ohpl_ga2b_push_begin(&pb, &pbo);

	if(pbo.tooFar)
	{
		mpPlayer->ChangeState(mPreviousState);
		return;
	}

	cVector3f vAttachPos = pbo.attach;
	mvPushDir = pbo.pushDir;

	cVector3f vBodyVel = mpPushData->mbPushAtPoint ?  mpCurrentBody->GetVelocityAtPosition(vAttachPos) : mpCurrentBody->GetLinearVelocity();

	if(pbo.noMovement)
	{
		cVector3f vForce;
		ohpl_ga2b_push_stop(&mStopVelocityPid, &vBodyVel, mpCurrentBody->GetMass(), mfMaxForce, afTimeStep, &vForce);

		if(mpPushData->mbPushAtPoint)
			mpCurrentBody->AddForceAtPosition(vForce,vAttachPos);
		else
			mpCurrentBody->AddForce(vForce);
		return;
	}

	bool bNormalMove = mpPlayer->GetCurrentMoveState() == eLuxMoveState_Normal;
	bool bRunning = false;
	bool bCrouching = false;
	if(bNormalMove)
	{
		cLuxMoveState_Normal *pNormalMove = static_cast<cLuxMoveState_Normal*>(mpPlayer->GetMoveStateData(eLuxMoveState_Normal));
		bRunning = pNormalMove->IsRunning();
		bCrouching = pNormalMove->IsCrouching();
	}
	float vWalk[2] = {mfWalkMaxSpeed, mfWalkForce};
	float vRun[2] = {mfRunMaxSpeed, mfRunForce};
	float vCrouch[2] = {mfCrouchMaxSpeed, mfCrouchForce};
	float vPick[2];
	ohpl_ga2b_push_pick(bNormalMove, bRunning, bCrouching, vWalk, vRun, vCrouch, vPick);

	OhplGa2bPushDrive pd;
	pd.pushDir = mvPushDir;
	pd.vel = vBodyVel;
	pd.mass = mpCurrentBody->GetMass();
	pd.maxForce = mfMaxForce;
	pd.maxSpeed = vPick[0];
	pd.pushForce = vPick[1];
	pd.forceMul = mpPushData->mfPushForceMul;
	pd.bodyMatrix = mpCurrentBody->GetLocalMatrix();
	pd.forward = pCharBody->GetForward();
	pd.right = pCharBody->GetRight();
	OhplGa2bPushDriveOut pdo;
	ohpl_ga2b_push_drive(&mVelocityPid, &pd, afTimeStep, &pdo);

	if(mpPushData->mbPushAtPoint)
		mpCurrentBody->AddForceAtPosition(pdo.correctForce,vAttachPos);
	else
		mpCurrentBody->AddForce(pdo.correctForce);

	if(pdo.hasPushForce)
	{
		if(mpPushData->mbPushAtPoint)
		{
			mpCurrentBody->AddForceAtPosition(pdo.pushForce,vAttachPos);
		}
		else
		{
			mpCurrentBody->AddForce(pdo.pushForce);
		}
	}

	cCollideData collideData;collideData.SetMaxSize(1);
	bool bWillCollideWithPlayer = pPhysicsWorld->CheckShapeCollision(
										pCharBody->GetCurrentShape(), cMath::MatrixTranslate(pCharBody->GetPosition()),
										mpCurrentBody->GetShape(), pdo.nextTransform,
										collideData,1,false);

	if(bWillCollideWithPlayer==false)
	{
		for(int i=0; i<2; ++i)
		{
			pCharBody->SetMaxPositiveMoveSpeed((eCharDir)i,pdo.maxSpeed[i]);
			pCharBody->SetMaxNegativeMoveSpeed((eCharDir)i,-pdo.maxSpeed[i]);
		}
	}

	for(int i=0; i<2; ++i)
	{
		if(mbMoving[i])
			pCharBody->Move((eCharDir)i, mfTotalMoveMul[i]);
	}
}

void cLuxPlayerState_InteractPush::PostUpdate(float afTimeStep)
{
	ClearMoveVars();
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractPush::OnDoAction(eLuxPlayerAction aAction,bool abPressed)
{
	int lKind = ohpl_ga2b_interact_action(OhplGa2bActOf(aAction), abPressed);
	if(lKind == 1)
	{
		mpPlayer->ChangeState(mPreviousState);
		return false;
	}
	else if(lKind == 2)
	{
		iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();

		if(mpPushData->mbPushAtPoint)
		{
			cVector3f vAttachPos = cMath::MatrixMul(mpCurrentBody->GetLocalMatrix(), mvLocalAttachPos);
			mpCurrentBody->AddImpulseAtPosition(mvPushDir * mpPushData->mfPushImpulse,vAttachPos);
		}
		else
		{
			mpCurrentBody->AddImpulse(mvPushDir * mpPushData->mfPushImpulse);
		}

		mpPlayer->ChangeState(mPreviousState);
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractPush::OnScroll(float afAmount)
{
	
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractPush::OnMove(eCharDir aDir, float afMul)
{
	mfTotalMoveMul[aDir] += afMul;
	mbMoving[aDir] = true;

	return false;
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractPush::OnAddYaw(float afAmount)
{
	return true;
}

bool cLuxPlayerState_InteractPush::OnAddPitch(float afAmount)
{
	return true;
}

//-----------------------------------------------------------------------

cGuiGfxElement* cLuxPlayerState_InteractPush::GetCrosshair()
{
	return NULL;
}

//-----------------------------------------------------------------------

float cLuxPlayerState_InteractPush::DrawDebug(cGuiSet *apSet,iFontData *apFont, float afStartY)
{
	cVector3f vBodyVel = mpCurrentBody->GetLinearVelocity(); //TODO: Get vel at point if pick at point!
	cVector3f vVelInDir = mvPushDir * cMath::Vector3Dot(mvPushDir, vBodyVel);
	
	apSet->DrawFont(apFont,cVector3f(5,afStartY,5),12,cColor(1,1),_W("CurrentVel: %ls"), cString::To16Char(vBodyVel.ToString()).c_str());
	afStartY += 13.0f;
	
	apSet->DrawFont(apFont,cVector3f(5,afStartY,5),12,cColor(1,1),_W("WantedVel: %ls"), cString::To16Char(vVelInDir.ToString()).c_str());
	afStartY += 13.0f;

	return afStartY;	
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractPush::ClearMoveVars()
{
	for(int i=0; i<2; ++i)
	{
		mfTotalMoveMul[i] = 0;
		mbMoving[i] = false;
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxPlayerState_InteractPush_SaveData, iLuxPlayerState_Interact_SaveData)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxPlayerState_SaveData* cLuxPlayerState_InteractPush::CreateSaveData()
{
	return hplNew(cLuxPlayerState_InteractPush_SaveData, ());
}

//-----------------------------------------------------------------------


void cLuxPlayerState_InteractPush::SaveToSaveData(iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::SaveToSaveData(apSaveData);
	cLuxPlayerState_InteractPush_SaveData *pData = static_cast<cLuxPlayerState_InteractPush_SaveData*>(apSaveData);


	///////////////////////
	// Save vars
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractPush::LoadFromSaveDataBeforeEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveDataBeforeEnter(apMap,apSaveData);
	cLuxPlayerState_InteractPush_SaveData *pData = static_cast<cLuxPlayerState_InteractPush_SaveData*>(apSaveData);

	///////////////////////
	// Load vars
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractPush::LoadFromSaveDataAfterEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveDataAfterEnter(apMap,apSaveData);
	cLuxPlayerState_InteractPush_SaveData *pData = static_cast<cLuxPlayerState_InteractPush_SaveData*>(apSaveData);

	///////////////////////
	// Load vars
}

//-----------------------------------------------------------------------





