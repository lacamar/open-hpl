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

cLuxPlayerState_Ladder::cLuxPlayerState_Ladder(cLuxPlayer *apPlayer) : iLuxPlayerState(apPlayer, eLuxPlayerState_Ladder, &mR.base)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_init(&mR.base, &h, eLuxPlayerState_Ladder);
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
	OhplA3Host h = A3HostTable();
	ohpl_a3_enter(mpA3, &h, aPrevState);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_Ladder::OnLeaveState(eLuxPlayerState aNewState)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_leave(mpA3, &h, aNewState);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_Ladder::Update(float afTimeStep)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_update(mpA3, &h, afTimeStep);
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
	OhplA3Host h = A3HostTable();
	return ohpl_a3_on_move(mpA3, &h, aDir, afMul);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_Ladder::OnAddYaw(float afAmount)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_add_yaw(mpA3, &h, afAmount);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_Ladder::OnAddPitch(float afAmount)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_add_pitch(mpA3, &h, afAmount);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_Ladder::OnRun(bool abPressed)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_on_run(mpA3, &h, abPressed);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_Ladder::OnJump(bool abPressed)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_on_jump(mpA3, &h, abPressed);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_Ladder::OnCrouch(bool abPressed)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_on_crouch(mpA3, &h, abPressed);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_Ladder::OnDoAction(eLuxPlayerAction aAction,bool abPressed)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_do_action(mpA3, &h, aAction, abPressed);
}

//-----------------------------------------------------------------------


float cLuxPlayerState_Ladder::DrawDebug(cGuiSet *apSet,iFontData *apFont, float afStartY)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_draw_debug(mpA3, &h, apSet, apFont, afStartY);
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

	(pData->mvCharPosition = mR.charPosition);

	(pData->mvGoalPos = mR.goalPos);
	(pData->mvGoalRot = mR.goalRot);

	(pData->mvPosAdd = mR.posAdd);
	(pData->mvRotAdd = mR.rotAdd);

	(pData->mfMoveMul = mR.moveMul);

	(pData->mfUpSpeed = mR.upSpeed);
	(pData->mfDownSpeed = mR.downSpeed);

	(pData->mfStepLength = mR.stepLength);
	(pData->mfStepCount = mR.stepCount);
	(pData->mbPlayedSound = mR.playedSound);

	kCopyToVar(pData, mfPitchMaxLimit);
	kCopyToVar(pData, mfPitchMinLimit);

	(pData->mfLeaveAtTopCount = mR.leaveAtTop);

	(pData->mbLanternDrawn = mR.lanternDrawn);
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

	(mR.charPosition = pData->mvCharPosition);

	(mR.goalPos = pData->mvGoalPos);
	(mR.goalRot = pData->mvGoalRot);

	(mR.posAdd = pData->mvPosAdd);
	(mR.rotAdd = pData->mvRotAdd);

	(mR.moveMul = pData->mfMoveMul);

	(mR.upSpeed = pData->mfUpSpeed);
	(mR.downSpeed = pData->mfDownSpeed);

	(mR.stepLength = pData->mfStepLength);
	(mR.stepCount = pData->mfStepCount);
	(mR.playedSound = pData->mbPlayedSound);

	kCopyFromVar(pData, mfPitchMaxLimit);
	kCopyFromVar(pData, mfPitchMinLimit);

	(mR.leaveAtTop = pData->mfLeaveAtTopCount);

	(mR.lanternDrawn = pData->mbLanternDrawn);

	if(mlState>0)
	{
		SetupHeadTurnLimits();
	}
}

//-----------------------------------------------------------------------
