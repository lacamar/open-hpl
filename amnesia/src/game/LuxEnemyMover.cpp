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

#include "LuxEnemyMover.h"

#include "LuxEnemy.h"
#include "LuxMap.h"

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEnemyMover::cLuxEnemyMover(iLuxEnemy *apEnemy, iCharacterBody *apCharBody) : mMoveState(reinterpret_cast<eLuxEnemyMoveState&>(mS.moveState))
{
	mpEnemy = apEnemy;
	mpCharBody = apCharBody;

	mS.turning = false;
	mS.turnGoalAngle =0;
	mS.turnSpeed =0;
	mS.turnBreakAcc =0;

	mS.stuckLimit = 0.3f;
	mS.maxStuckCounter = 2.0f;

	mS.stuckCounter =0;

	mMoveState = eLuxEnemyMoveState_LastEnum;
	mS.overideMoveState = false;
}

//-----------------------------------------------------------------------

cLuxEnemyMover::~cLuxEnemyMover()
{
	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxEnemyMover::SetupAfterLoad(cWorld *apWorld)
{
}
//-----------------------------------------------------------------------

void cLuxEnemyMover::OnUpdate(float afTimeStep)
{
	OhplGa3Host host = mpEnemy->Ga3Host();
	OhplGa3MoverParams params;
	mpEnemy->Ga3MoverParams(params);
	ohpl_ga3_mover_update(&mS, &params, afTimeStep, &host);
	UpdateStepEffects(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxEnemyMover::MoveToPos(const cVector3f& avFeetPos)
{
	OhplGa3Host host = mpEnemy->Ga3Host();
	ohpl_ga3_mover_move_to_pos(&mS, &avFeetPos.x, &host);
}

//-----------------------------------------------------------------------

void cLuxEnemyMover::TurnToPos(const cVector3f& avFeetPos)
{
	OhplGa3Host host = mpEnemy->Ga3Host();
	ohpl_ga3_mover_turn_to_pos(&mS, &avFeetPos.x, &host);
}

void cLuxEnemyMover::TurnToAngle(float afAngle)
{
	ohpl_ga3_mover_turn_to_angle(&mS, afAngle);
}

//-----------------------------------------------------------------------

void cLuxEnemyMover::UseMoveStateAnimations()
{
	ohpl_ga3_mover_use_move_state_animations(&mS);
}

//-----------------------------------------------------------------------

float cLuxEnemyMover::CalculateSpeedMul(float afTimeStep)
{
	OhplGa3Host host = mpEnemy->Ga3Host();
	OhplGa3MoverParams params;
	mpEnemy->Ga3MoverParams(params);
	return ohpl_ga3_mover_speed_mul(&mS, &params, &host);
}

//-----------------------------------------------------------------------

float cLuxEnemyMover::GetMoveSpeed()
{
	OhplGa3Host host = mpEnemy->Ga3Host();
	return ohpl_ga3_mover_move_speed(&host);
}
//-----------------------------------------------------------------------

float cLuxEnemyMover::GetWantedSpeedAmount()
{
	OhplGa3Host host = mpEnemy->Ga3Host();
	return ohpl_ga3_mover_wanted_speed_amount(&host);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxEnemyMover::UpdateMoveAnimation(float afTimeStep)
{
	OhplGa3Host host = mpEnemy->Ga3Host();
	OhplGa3MoverParams params;
	mpEnemy->Ga3MoverParams(params);
	ohpl_ga3_mover_update_move_animation(&mS, &params, &host);
}

//-----------------------------------------------------------------------

void cLuxEnemyMover::UpdateStepEffects(float afTimeStep)
{
	if(mpEnemy->IsInWater()==false || mpEnemy->GetWaterSurfaceData()==NULL) return;

	cAnimationState *pAnim = mpEnemy->GetCurrentAnimation();
	if(pAnim==NULL) return;

	///////////////////////////////////////
	// Check if a step is occuring!
	bool bStep = false;
	for(int i=0; i<pAnim->GetEventNum(); ++i)
	{
		cAnimationEvent *pEvent = pAnim->GetEvent(i);
        if(pEvent->mType != eAnimationEventType_Step) continue;

		if(	pEvent->mfTime >= pAnim->GetPreviousTimePosition() &&
			pEvent->mfTime < pAnim->GetTimePosition())
		{
			bStep = true;
			break;
		}
	}
	if(bStep==false) return;
	
	///////////////////////////////////////
	// Do the liquid effect
	cSurfaceData *pSurface = mpEnemy->GetWaterSurfaceData();
	
	//Get the speed
	float fSpeed = mpEnemy->mfWaterStepSpeedMisc;
	if(mMoveState == eLuxEnemyMoveState_Walking || mMoveState == eLuxEnemyMoveState_Backward)
	{
		fSpeed = mpEnemy->mfWaterStepSpeedWalk;
	}
	else if(mMoveState == eLuxEnemyMoveState_Running)
	{
		fSpeed = mpEnemy->mfWaterStepSpeedWalk;
	}

	//Get impact effect and coordinate
	cSurfaceImpactData *pImpact = pSurface->GetImpactDataFromSpeed(3.0f);
	cVector3f vEffectPos = mpCharBody->GetPosition();
	vEffectPos.y =mpEnemy->GetWaterSurfaceY()+0.01f;

	//Create sounda and ps for effect	
	cWorld *pWorld = mpEnemy->GetMap()->GetWorld();

	if(pImpact->GetPSName() != "")
	{
		cParticleSystem *pPS = pWorld->CreateParticleSystem("Splash", pImpact->GetPSName(),1);
		if(pPS) pPS->SetPosition(vEffectPos);
	}

	if(pImpact->GetSoundName() != "")
	{
		cSoundEntity *pSound = pWorld->CreateSoundEntity("Splash",pImpact->GetSoundName(),true);
		if(pSound) pSound->SetPosition(vEffectPos);
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxEnemyMover_SaveData)

kSerializeVar(mbTurning, eSerializeType_Bool)
kSerializeVar(mfTurnGoalAngle, eSerializeType_Float32)
kSerializeVar(mfTurnSpeed, eSerializeType_Float32)
kSerializeVar(mfTurnBreakAcc, eSerializeType_Float32)

kSerializeVar(mfStuckCounter, eSerializeType_Float32)

kSerializeVar(mlMoveState, eSerializeType_Int32)
kSerializeVar(mbOverideMoveState, eSerializeType_Bool)

kEndSerialize()

//-----------------------------------------------------------------------

void cLuxEnemyMover_SaveData::FromMover(cLuxEnemyMover *apMover)
{	
	mbTurning = apMover->mS.turning;
	mfTurnGoalAngle = apMover->mS.turnGoalAngle;
	mfTurnSpeed = apMover->mS.turnSpeed;
	mfTurnBreakAcc = apMover->mS.turnBreakAcc;

	mfStuckCounter = apMover->mS.stuckCounter;

	mlMoveState = apMover->mMoveState;
	mbOverideMoveState = apMover->mS.overideMoveState;
}

//-----------------------------------------------------------------------

void cLuxEnemyMover_SaveData::ToMover(cLuxEnemyMover *apMover)
{
	apMover->mS.turning = mbTurning;
	apMover->mS.turnGoalAngle = mfTurnGoalAngle;
	apMover->mS.turnSpeed = mfTurnSpeed;
	apMover->mS.turnBreakAcc = mfTurnBreakAcc;

	apMover->mS.stuckCounter = mfStuckCounter;

	apMover->mMoveState = (eLuxEnemyMoveState)mlMoveState;
	apMover->mS.overideMoveState = mbOverideMoveState;
}

//-----------------------------------------------------------------------
