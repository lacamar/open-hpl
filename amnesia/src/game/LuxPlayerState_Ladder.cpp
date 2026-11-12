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

#include "LuxPlayerState_Ladder.h"

#include "LuxPlayer.h"
#include "LuxArea_Ladder.h"
#include "LuxMoveState_Normal.h"
#include "LuxHelpFuncs.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxPlayerHelpers.h"


//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerState_Ladder::cLuxPlayerState_Ladder(cLuxPlayer *apPlayer) : iLuxPlayerState(apPlayer, eLuxPlayerState_Ladder)
{
	mpLadder = NULL;

	mfUpSpeed = gpBase->mpGameCfg->GetFloat("Player_Ladder","UpSpeed",0);
	mfDownSpeed = gpBase->mpGameCfg->GetFloat("Player_Ladder","DownSpeed",0);

	mfStepLength = gpBase->mpGameCfg->GetFloat("Player_Ladder","StepLength",0);

	mfStepCount =0;

	mlState =0;
}

//-----------------------------------------------------------------------

cLuxPlayerState_Ladder::~cLuxPlayerState_Ladder()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerState_Ladder::OnEnterState(eLuxPlayerState aPrevState)
{
	
	/////////////////////////////////////
	// Load the data
	mpLadder = cLuxPlayerStateVars::mpLadder;
	mvStartPosition = cLuxPlayerStateVars::mvLadderStartPos;

	/////////////////////////////////////
	// Get data
	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
	
	/////////////////////////////////////
	// Set up player
	mpPlayer->ChangeMoveState(eLuxMoveState_Normal);
	pCharBody->SetGravityActive(false);
	pCharBody->SetTestCollision(false);
	pCharBody->StopMovement();
	pCharBody->ResetClimbing();

    cLuxMoveState_Normal *pMoveState = static_cast<cLuxMoveState_Normal*>(mpPlayer->GetMoveStateData(eLuxMoveState_Normal));
	pMoveState->SetCrouch(false);
	pMoveState->ResetJumping();

	/////////////////////////////////////
	// Holster lantern
	mbLanternDrawn = mpPlayer->GetHelperLantern()->IsActive();
	mpPlayer->GetHelperLantern()->SetActive(false,true,true, false);

	///////////////////////////////////////
	// Set up variables
	mlState =0;
	mfStepCount =0;
	mbPlayedSound = false;
	mfMoveMul =0;

	///////////////////////////////////////
	// Save pitch limits
	mfPitchMinLimit = mpPlayer->GetCamera()->GetPitchMinLimit();
	mfPitchMaxLimit = mpPlayer->GetCamera()->GetPitchMaxLimit();

	/////////////////////////////////////		
	//Different time if player is above the ladder.
	OhplGa2bLadderEnterIn li;
	li.pos = mpPlayer->GetCharacterBody()->GetPosition();
	li.start = mvStartPosition;
	li.maxY = mpLadder->GetMaxY();
	li.startYaw = mpLadder->GetStartRotation().y;
	li.pitch = mpPlayer->GetCamera()->GetPitch();
	li.yaw = mpPlayer->GetCamera()->GetYaw();
	OhplGa2bLadderEnterOut lo;
	ohpl_ga2b_ladder_enter(&li, &lo);

	mfTimeCount = lo.time;

	mvGoalPos = mvStartPosition;
	mvPosAdd = lo.posAdd;

	mvCharPosition = mpPlayer->GetCharacterBody()->GetPosition();

	mvGoalRot.x = 0;
	mvGoalRot.y = mpLadder->GetStartRotation().y;

	mvRotAdd.x = lo.rotAdd.x;
	mvRotAdd.y = lo.rotAdd.y;

    /////////////////////////////////////
	// Play attach sound
	PlaySound("attach");
}

//-----------------------------------------------------------------------

void cLuxPlayerState_Ladder::OnLeaveState(eLuxPlayerState aNewState)
{
	//////////////////////////////
	//Reset variables
	// Need to so pointer exists! (as this might be on leave!)
	if(mpPlayer->GetCharacterBody())
	{
		mpPlayer->GetCharacterBody()->SetGravityActive(true);
		mpPlayer->GetCharacterBody()->SetTestCollision(true);
	}

	mpPlayer->GetCamera()->SetPitchLimits(mfPitchMinLimit, mfPitchMaxLimit);
	mpPlayer->GetCamera()->SetYawLimits(0, 0);

	/////////////////////////////////////
	// Draw lantern if it was before
	if(mbLanternDrawn)
        mpPlayer->GetHelperLantern()->SetActive(true, true, true, false);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_Ladder::Update(float afTimeStep)
{
	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
	cCamera *pCam = mpPlayer->GetCamera();

	auto toRust = [this]()
	{
		OhplGa2bLadderState s;
		s.state = mlState;
		s.timeCount = mfTimeCount;
		s.stepCount = mfStepCount;
		s.moveMul = mfMoveMul;
		s.leaveAtTop = mfLeaveAtTopCount;
		s.playedSound = mbPlayedSound;
		s.charPos = mvCharPosition;
		s.posAdd = mvPosAdd;
		s.rotAdd = cVector2f(mvRotAdd.x, mvRotAdd.y);
		return s;
	};
	auto fromRust = [this](const OhplGa2bLadderState &s)
	{
		mfTimeCount = s.timeCount;
		mfStepCount = s.stepCount;
		mfMoveMul = s.moveMul;
		mfLeaveAtTopCount = s.leaveAtTop;
		mbPlayedSound = s.playedSound;
		mvCharPosition = s.charPos;
	};

	if(mlState ==0)
	{
		OhplGa2bLadderState ls = toRust();
		OhplGa2bLadderAttachOut ao;
		ohpl_ga2b_ladder_attach(&ls, afTimeStep, &ao);
		fromRust(ls);

		pCharBody->SetPosition(mvCharPosition);
		mvCharPosition = pCharBody->GetPosition();

		pCam->AddPitch(ao.pitchAdd);
		pCam->AddYaw(ao.yawAdd);
		pCharBody->SetYaw(pCam->GetYaw());

		if(ao.done)
		{
			mpPlayer->GetCharacterBody()->SetTestCollision(true);

			SetupHeadTurnLimits();

			mlState++;
		}
	}
	else if(mlState == 1)
	{
		cVector3f vPos = pCharBody->GetPosition();
		OhplGa2bLadderState ls = toRust();
		int lSound = ohpl_ga2b_ladder_move_pre(&ls, &vPos, mfUpSpeed, mfDownSpeed, afTimeStep);
		fromRust(ls);
		if(lSound == kOhplGa2bSoundUp) PlaySound("up");
		else if(lSound == kOhplGa2bSoundDown) PlaySound("down");

		if(pCharBody->CheckCharacterFits(mvCharPosition,false,0,NULL,kEpsilonf)==false)
		{
			mfMoveMul =0;
			return;
		}

		ls = toRust();
		lSound = ohpl_ga2b_ladder_move_post(&ls, mfUpSpeed, mfDownSpeed, mfStepLength, afTimeStep);
		fromRust(ls);
		if(lSound == kOhplGa2bSoundUp) PlaySound("up");
		else if(lSound == kOhplGa2bSoundDown) PlaySound("down");

		pCharBody->SetPosition(mvCharPosition);

		cVector3f vFeet = pCharBody->GetFeetPosition();
		cVector3f vLadderForward = mpLadder->GetForward();
		cVector3f vRays[4];
		ohpl_ga2b_ladder_ground_rays(&vFeet, &vLadderForward, pCharBody->GetSize().x, vRays);

		if(ohpl_ga2b_ladder_at_top(mfMoveMul, mvCharPosition.y, pCharBody->GetSize().y, mpLadder->GetMaxY()))
		{
			mlState = 2;
			mfLeaveAtTopCount = 2;
			PlaySound("dettach");
		}
		else
		{
			int lBottom = ohpl_ga2b_ladder_at_bottom(mfMoveMul, mvCharPosition.y, pCharBody->GetSize().y, mpLadder->GetMinY());
			if(lBottom == kOhplGa2bBottomYes ||
				(lBottom == kOhplGa2bBottomCheckRays &&
					(pCharBody->CheckRayIntersection(vRays[0], vRays[1],NULL,NULL) ||
					pCharBody->CheckRayIntersection(vRays[2], vRays[3],NULL,NULL))))
			{
				mpPlayer->ChangeState(eLuxPlayerState_Normal);
			}
		}

		mfMoveMul =0;
	}
	else if(mlState == 2)
	{
		pCharBody->Move(eCharDir_Forward,1);

		cVector3f vFeet = pCharBody->GetFeetPosition();
		cVector3f vRay[2];
		ohpl_ga2b_ladder_top_rays(&vFeet, vRay);

		bool bTimeout = ohpl_ga2b_ladder_leave_tick(&mfLeaveAtTopCount, afTimeStep);

		if( pCharBody->CheckRayIntersection(vRay[0], vRay[1],NULL,NULL))
		{
			mpPlayer->ChangeState(eLuxPlayerState_Normal);
		}
		else if(bTimeout)
		{
			mpPlayer->ChangeState(eLuxPlayerState_Normal);
		}
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerState_Ladder::PostUpdate(float afTimeStep)
{

}

//-----------------------------------------------------------------------


void cLuxPlayerState_Ladder::OnDraw(cGuiSet *apGuiSet ,float afFrameTime)
{

}

//-----------------------------------------------------------------------


cGuiGfxElement* cLuxPlayerState_Ladder::GetCrosshair()
{
	return NULL;
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_Ladder::OnMove(eCharDir aDir, float afMul)
{
	ohpl_ga2b_ladder_on_move(mlState, aDir == eCharDir_Right, afMul, &mfMoveMul);
	return false;
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_Ladder::OnAddYaw(float afAmount)
{
	return ohpl_ga2b_ladder_camera_free(mlState);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_Ladder::OnAddPitch(float afAmount)
{
	return ohpl_ga2b_ladder_camera_free(mlState);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_Ladder::OnRun(bool abPressed)
{
	return false;
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_Ladder::OnJump(bool abPressed)
{
    if(abPressed)
	{
		mpPlayer->ChangeState(eLuxPlayerState_Normal);
		cLuxMoveState_Normal *pMoveState = static_cast<cLuxMoveState_Normal*>(mpPlayer->GetMoveStateData(eLuxMoveState_Normal));
		pMoveState->Jump();
	}

	return false;
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_Ladder::OnCrouch(bool abPressed)
{
	return false;
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_Ladder::OnDoAction(eLuxPlayerAction aAction,bool abPressed)
{
	bool bRelease;
	bool bPass = ohpl_ga2b_ladder_action(OhplGa2bActOf(aAction), abPressed, &bRelease);
	if(bRelease)
	{
		mpPlayer->ChangeState(eLuxPlayerState_Normal);
	}
	return bPass;
}

//-----------------------------------------------------------------------


float cLuxPlayerState_Ladder::DrawDebug(cGuiSet *apSet,iFontData *apFont, float afStartY)
{
	return afStartY;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerState_Ladder::PlaySound(const tString &asType)
{
	tString sSound = "player_ladder_"+mpLadder->GetMaterial()+"_"+asType;
	gpBase->mpHelpFuncs->PlayGuiSoundData(sSound,eSoundEntryType_World);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_Ladder::SetupHeadTurnLimits()
{
	cCamera *pCam = mpPlayer->GetCamera();

	float vLimits[4];
	ohpl_ga2b_ladder_head_limits(pCam->GetYaw(), pCam->GetPitch(), vLimits);

	pCam->SetPitchLimits(vLimits[0], vLimits[1]);
	pCam->SetYawLimits(vLimits[2], vLimits[3]);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxPlayerState_Ladder_SaveData, iLuxPlayerState_DefaultBase_SaveData)
kSerializeVar(mlState, eSerializeType_Int32)
kSerializeVar(mvStartPosition, eSerializeType_Vector3f)

kSerializeVar(mlLadderID, eSerializeType_Int32)

kSerializeVar(mfTimeCount, eSerializeType_Float32)

kSerializeVar(mvCharPosition, eSerializeType_Vector3f)

kSerializeVar(mvGoalPos, eSerializeType_Vector3f)
kSerializeVar(mvGoalRot, eSerializeType_Vector3f)

kSerializeVar(mvPosAdd, eSerializeType_Vector3f)
kSerializeVar(mvRotAdd, eSerializeType_Vector3f)

kSerializeVar(mfMoveMul, eSerializeType_Float32)

kSerializeVar(mfUpSpeed, eSerializeType_Float32)
kSerializeVar(mfDownSpeed, eSerializeType_Float32)

kSerializeVar(mfStepLength, eSerializeType_Float32)
kSerializeVar(mfStepCount, eSerializeType_Float32)
kSerializeVar(mbPlayedSound, eSerializeType_Bool)

kSerializeVar(mfPitchMaxLimit, eSerializeType_Float32)
kSerializeVar(mfPitchMinLimit, eSerializeType_Float32)

kSerializeVar(mbLanternDrawn, eSerializeType_Bool)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxPlayerState_SaveData* cLuxPlayerState_Ladder::CreateSaveData()
{
	return hplNew(cLuxPlayerState_Ladder_SaveData, ());
}

//-----------------------------------------------------------------------


void cLuxPlayerState_Ladder::SaveToSaveData(iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::SaveToSaveData(apSaveData);
	cLuxPlayerState_Ladder_SaveData *pData = static_cast<cLuxPlayerState_Ladder_SaveData*>(apSaveData);


	///////////////////////
	// Save vars
	if(mpLadder)
		pData->mlLadderID = mpLadder->GetID();
	else
		pData->mlLadderID = -1;
	
	kCopyToVar(pData, mlState);
	kCopyToVar(pData, mvStartPosition);

	kCopyToVar(pData, mfTimeCount);

	kCopyToVar(pData, mvCharPosition);

	kCopyToVar(pData, mvGoalPos);
	kCopyToVar(pData, mvGoalRot);

	kCopyToVar(pData, mvPosAdd);
	kCopyToVar(pData, mvRotAdd);

	kCopyToVar(pData, mfMoveMul);

	kCopyToVar(pData, mfUpSpeed);
	kCopyToVar(pData, mfDownSpeed);

	kCopyToVar(pData, mfStepLength);
	kCopyToVar(pData, mfStepCount);
	kCopyToVar(pData, mbPlayedSound);

	kCopyToVar(pData, mfPitchMaxLimit);
	kCopyToVar(pData, mfPitchMinLimit);

	kCopyToVar(pData, mfLeaveAtTopCount);

	kCopyToVar(pData, mbLanternDrawn);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_Ladder::LoadFromSaveDataBeforeEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveDataBeforeEnter(apMap,apSaveData);
	cLuxPlayerState_Ladder_SaveData *pData = static_cast<cLuxPlayerState_Ladder_SaveData*>(apSaveData);

	///////////////////////
	// Load vars
	iLuxEntity *pEntity = gpBase->mpMapHandler->GetCurrentMap()->GetEntityByID(pData->mlLadderID,eLuxEntityType_Area, eLuxAreaType_Ladder);
	if(pEntity)
	{
		mpLadder = static_cast<cLuxArea_Ladder*>(pEntity);
	}
	
	kCopyFromVar(pData, mvStartPosition);

	cLuxPlayerStateVars::SetupLadder(mpLadder, mvStartPosition);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_Ladder::LoadFromSaveDataAfterEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveDataAfterEnter(apMap,apSaveData);
	cLuxPlayerState_Ladder_SaveData *pData = static_cast<cLuxPlayerState_Ladder_SaveData*>(apSaveData);

	///////////////////////
	// Load vars
	kCopyFromVar(pData, mlState);
	
	kCopyFromVar(pData, mfTimeCount);

	kCopyFromVar(pData, mvCharPosition);

	kCopyFromVar(pData, mvGoalPos);
	kCopyFromVar(pData, mvGoalRot);

	kCopyFromVar(pData, mvPosAdd);
	kCopyFromVar(pData, mvRotAdd);

	kCopyFromVar(pData, mfMoveMul);

	kCopyFromVar(pData, mfUpSpeed);
	kCopyFromVar(pData, mfDownSpeed);

	kCopyFromVar(pData, mfStepLength);
	kCopyFromVar(pData, mfStepCount);
	kCopyFromVar(pData, mbPlayedSound);

	kCopyFromVar(pData, mfPitchMaxLimit);
	kCopyFromVar(pData, mfPitchMinLimit);

	kCopyFromVar(pData, mfLeaveAtTopCount);

	kCopyFromVar(pData, mbLanternDrawn);

	if(mlState>0)
	{
		SetupHeadTurnLimits();
	}
}

//-----------------------------------------------------------------------


