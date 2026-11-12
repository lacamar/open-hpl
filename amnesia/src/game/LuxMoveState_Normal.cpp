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

#include "LuxMoveState_Normal.h"

#include "LuxPlayer.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxHelpFuncs.h"
#include "LuxPlayerHelpers.h"
#include "RustLuxPlayer.h"

//////////////////////////////////////////////////////////////////////////
// CHARACTER CALLBACK
//////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------

cLuxMoveState_Normal_Callback::cLuxMoveState_Normal_Callback(cLuxMoveState_Normal *apMoveState)
{
	mpMoveState = apMoveState;
}

//-----------------------------------------------------------------------

void cLuxMoveState_Normal_Callback::OnGravityCollide(iCharacterBody *apCharBody, iPhysicsBody *apBody, cCollideData *apCollideData)
{
	//Check if there is enough speed for a bounce!
	if(ohpl_ga2b_bounce_starts(apCollideData->mvContactPoints[0].mvNormal.y, apCharBody->GetForceVelocity().y,
							mpMoveState->mfMinHitGroundBounceSpeed, mpMoveState->mbHeadGroundBounceActive))
	{
		mpMoveState->mbHeadGroundBounceActive = true;
		mpMoveState->mfBounceSizeMul = 1.0f;
		mpMoveState->mfBounceSpeedMul = 1.0f;
		mpMoveState->FootSound(eLuxFootSound_Landing);

		mpMoveState->FallDamage(apCharBody->GetForceVelocity().y);
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxMoveState_Normal::cLuxMoveState_Normal(cLuxPlayer *apPlayer) : iLuxMoveState(apPlayer)
{
	////////////////////////////
	// Create data
	mpCallback = hplNew( cLuxMoveState_Normal_Callback, (this));
	
	mbRunning = false;
	mbCrouching = false;
	mbJumping = false;

	////////////////////////////
	// Sounds
	msJumpSound = gpBase->mpGameCfg->GetString("Player_Movement_Normal","JumpSound","player_jump");
	msCrouchSound = gpBase->mpGameCfg->GetString("Player_Movement_Normal","CrouchSound","player_crouch");
	msStandSound = gpBase->mpGameCfg->GetString("Player_Movement_Normal","StandSound","player_stand");
	
	////////////////////////////
	// Set up speeds
	mfMaxForwardSpeed = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","MaxForwardSpeed",0);
	mfMaxBackwardSpeed = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","MaxBackwardSpeed",0);
	mfMaxSidwaySpeed = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","MaxSidwaySpeed",0);
	
	////////////////////////////
	// Set movement multipliers
	mfRunForwardMul = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","RunForwardMul",0);
	mfRunBackwardMul = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","RunBackwardMul",0);
	mfRunSidewayMul = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","RunSidewayMul",0);

	mfCrouchForwardMul = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","CrouchForwardMul",0);
	mfCrouchBackwardMul = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","CrouchBackwardMul",0);
	mfCrouchSidewayMul = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","CrouchSidewayMul",0);

	mfInAirForwardMul = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","InAirForwardMul",0);
	mfInAirBackwardMul = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","InAirBackwardMul",0);
	mfInAirSidewayMul = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","InAirSidewayMul",0);

	////////////////////////////
	// Set up jumping
	mfMaxJumpCount =gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","MaxJumpCount",0);
	mfJumpCount = mfMaxJumpCount;
	mfJumpStartForce = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","JumpStartForce",0);
	mfJumpCrouchStartForce = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","JumpCrouchStartForce",0);

	////////////////////////////
	// Set up bobbing
	mvCrouchBobMax = gpBase->mpGameCfg->GetVector2f("Player_Movement_Normal","CrouchBobMax",0);
	mvWalkBobMax = gpBase->mpGameCfg->GetVector2f("Player_Movement_Normal","WalkBobMax",0);
	mvRunBobMax = gpBase->mpGameCfg->GetVector2f("Player_Movement_Normal","RunBobMax",0);

	mfCrouchMinBobSpeed = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","CrouchMinBobSpeed",0);
	mfCrouchMaxBobSpeed = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","CrouchMaxBobSpeed",0);
	
	mfWalkMinBobSpeed = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","WalkMinBobSpeed",0);
	mfWalkMaxBobSpeed = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","WalkMaxBobSpeed",0);
	
	mfRunMinBobSpeed = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","RunMinBobSpeed",0);
	mfRunMaxBobSpeed = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","RunMaxBobSpeed",0);

	////////////////////////////
	// Set up ground bounce
	mfGroundBounceSize = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","GroundBounceSize",0);
	mfGroundBounceSpeed = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","GroundBounceSpeed",0);
	mfMinHitGroundBounceSpeed = gpBase->mpGameCfg->GetFloat("Player_Movement_Normal","MinHitGroundBounceSpeed",0);

	////////////////////////////
	// Set up ledge climbing
	mfMaxClimbLedgeCount =  1.0f / gpBase->mpGameCfg->GetFloat("Player_Movement_ClimbLedge","CheckUpdatesPerSecond",0);

	mfClimbLedgeCheckDistForward = gpBase->mpGameCfg->GetFloat("Player_Movement_ClimbLedge","ClimbLedgeCheckDistForward",0);
	mfClimbLedgeCheckDistFromTop = gpBase->mpGameCfg->GetFloat("Player_Movement_ClimbLedge","ClimbLedgeCheckDistFromTop",0);
	mfClimbLedgeCheckDistTopToEnd = gpBase->mpGameCfg->GetFloat("Player_Movement_ClimbLedge","ClimbLedgeCheckDistTopToEnd",0);
	mfClimbLedgeMaxPushDist = gpBase->mpGameCfg->GetFloat("Player_Movement_ClimbLedge","CheckMaxPushDist",0);


	////////////////////////////
	// Set up fall damage
	mfFallDamageBounceSizeMul = gpBase->mpGameCfg->GetFloat("Player_General","FallDamageBounceSizeMul",0);
	mfFallDamageBounceSpeedMul = gpBase->mpGameCfg->GetFloat("Player_General","FallDamageBounceSpeedMul",0);

	mfFallDamageSpeed_Min = gpBase->mpGameCfg->GetFloat("Player_General","FallDamageSpeed_Min",0);
	mfFallDamage_Min = gpBase->mpGameCfg->GetFloat("Player_General","FallDamage_Min",0);
	msFallDamageSound_Min = gpBase->mpGameCfg->GetString("Player_General","FallDamageSound_Min","");

	mfFallDamageSpeed_Med = gpBase->mpGameCfg->GetFloat("Player_General","FallDamageSpeed_Med",0);
	mfFallDamage_Med = gpBase->mpGameCfg->GetFloat("Player_General","FallDamage_Med",0);
	msFallDamageSound_Med = gpBase->mpGameCfg->GetString("Player_General","FallDamageSound_Med","");

	mfFallDamageSpeed_Max = gpBase->mpGameCfg->GetFloat("Player_General","FallSpeed_Max",0);
	mfFallDamage_Max = gpBase->mpGameCfg->GetFloat("Player_General","FallDamage_Max",0);
	msFallDamageSound_Max = gpBase->mpGameCfg->GetString("Player_General","FallDamageSound_Max","");
}

//-----------------------------------------------------------------------

cLuxMoveState_Normal::~cLuxMoveState_Normal()
{
	hplDelete(mpCallback);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::OnMapEnter()
{
	////////////////////////////
	// Set up variables
	
	//Perhaps have this in player and call the movestes from the callback?
    mpPlayer->GetCharacterBody()->SetCallback(mpCallback);

	if(mpPlayer->GetCurrentMoveState()==eLuxMoveState_Normal)
	{
		if(mbCrouching)        
			mpPlayer->GetCharacterBody()->SetActiveSize(1);
	}
}

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::OnEnterState(eLuxMoveState aPrevState)
{
	////////////////////////////
	// Set up variables
	mbRunning = false;
	
	mbWasMoving = false;
	mbWasOnGround = false;

	mfClimbLedgeCount = 0;

	mbHeadGroundBounceActive = false;
	mfHeadGroundBounce = 0;
	
	mfBounceSizeMul = 1.0f;
	mfBounceSpeedMul = 1.0f;
	
	mvHeadBob =0;
	mfPrevHeadBobCount =0;
	mfHeadBobCount = 0;
	mbBobbing = false;
	mvBobMaxGoal = mvWalkBobMax;
	mvCurrentBobMax = mvBobMaxGoal;

	if(aPrevState!= eLuxMoveState_Normal)
	{
		mbCrouching = false;
	}

	//The mul without any extras, used to determine the bob speed compared to move speed.
	mfDefaultForwardMul = 1.0f;
}

void cLuxMoveState_Normal::OnLeaveState(eLuxMoveState aNewState)
{
	//Reset head bob
	mpPlayer->MoveHeadPosAdd(eLuxHeadPosAdd_Bob, 0.0f, 0.5f, 0.01f);
}

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::OnUpdate(float afTimeStep)
{
	////////////////////////////
	// Check if there is any ledge up ahead.
	if(UpdateLedgeCheck(afTimeStep)) return;

	////////////////////////////
	// Update movement related stuff
	UpdateMovement(afTimeStep);

	
	////////////////////////////
	// Update multipliers
	UpdateSpeedMultipliers(afTimeStep);

	////////////////////////////
	// Update jump and ground checks
	UpdateJumpAndGroundCheck(afTimeStep);

	////////////////////////////
	// Update head bob
	UpdateHeadBob(afTimeStep);
	
}

//-----------------------------------------------------------------------

bool cLuxMoveState_Normal::GetAllowsInteraction()
{
	return true;
}

//-----------------------------------------------------------------------

bool cLuxMoveState_Normal::OnMove(eCharDir aDir, float afMul)
{
	return true;
}
bool cLuxMoveState_Normal::OnAddYaw(float afAmount)
{
	return true;
}
bool cLuxMoveState_Normal::OnAddPitch(float afAmount)
{
	return true;
}

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::OnRun(bool abActive)
{
	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
	if(abActive && ohpl_ga2b_run_pressed_stands(mpPlayer->GetCrouchDisabled(), mpPlayer->GetInsanityCollapse()->IsActive(),
												pCharBody && pCharBody->IsOnGround(), mpPlayer->GetPressedMove()))
	{
		SetCrouch(false);
	}
}

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::OnCrouch(bool abActive)
{
	//Do nothing on button release
	if(abActive==false) return;

	SetCrouch(!mbCrouching);	
}

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::OnJump(bool abActive)
{
	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
	if(ohpl_ga2b_jump_starts(abActive, mbJumping, pCharBody && pCharBody->IsOnGround()))
	{
		Jump();
	}
}

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::Jump()
{
	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();

	SetCrouch(false);

	gpBase->mpHelpFuncs->PlayGuiSoundData(msJumpSound, eSoundEntryType_World);

	float fStartForce = ohpl_ga2b_jump_force(mbCrouching, mfJumpCrouchStartForce, mfJumpStartForce, mpPlayer->GetScriptJumpForceMul(),
											mpPlayer->GetDefaultMass());
	pCharBody->AddForce(cVector3f(0, fStartForce,0));
	mbJumping = true;
	mfJumpCount = 0;
}

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::OnDraw(float afFrameTime)
{
	return;//Skip for now! Have some special thingy for this!
	float fY = 70;
	gpBase->mpGameDebugSet->DrawFont(gpBase->mpDefaultFont, cVector3f(5,70,0),12,cColor(1,1),
								_W("Crouch: %d Running: %d JumpCount: %f\n"),mbCrouching,mbRunning,mfJumpCount);
	fY+=13;
	gpBase->mpGameDebugSet->DrawFont(gpBase->mpDefaultFont, cVector3f(5,fY,0),12,cColor(1,1),
								_W("Bob: %ls (%f). Bounce: %f\n"),cString::To16Char(mvHeadBob.ToString()).c_str(), cMath::ToDeg(cMath::Wrap(mfHeadBobCount,0, k2Pif)),
											mfHeadGroundBounce);
}

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::SetCrouch(bool abActive)
{
	if(mbCrouching == abActive) return;

	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();

	/////////////////////
	//Crouch
	if(abActive)
	{
		gpBase->mpHelpFuncs->PlayGuiSoundData(msCrouchSound, eSoundEntryType_World);

		mbCrouching = true;
		pCharBody->SetActiveSize(1);

		//Move the main size add to the size crouch size.
		float fSizeDiff = mpPlayer->GetBodySize().y - mpPlayer->GetBodyCrouchSize().y;
		mpPlayer->MoveHeadPosAdd(eLuxHeadPosAdd_Main, cVector3f(0,-fSizeDiff, 0), 1.3f, 0.05f);
	}
	//////////////////////
	//Stand up
	else
	{
		cVector3f vPosTestAdd[5] = {cVector3f(0,0.001f,0), 
									cVector3f(0.01f,0.001f,0), cVector3f(-0.01f,0.001f,0),
									cVector3f(0,0.001f,0.01f), cVector3f(0,0.001f,-0.01f)
								};

		////////////////////////////////////////
		// Iterate through some positions to see if player will fit there
		bool bFits = false;
		int lCurrentFitTest =0;
		for(; lCurrentFitTest<5; ++lCurrentFitTest)
		{
			if(pCharBody->CheckCharacterFits(pCharBody->GetFeetPosition()+vPosTestAdd[lCurrentFitTest],true, 0))
			{
				bFits = true;
				break;
			}
		}


		////////////////////////////////////////
		// Player fits, stand up!
		if(bFits)
		{
			gpBase->mpHelpFuncs->PlayGuiSoundData(msStandSound, eSoundEntryType_World);

			mbCrouching = false;
			pCharBody->SetPosition(pCharBody->GetPosition()+vPosTestAdd[lCurrentFitTest]); //Make sure to move the player to the fitting positon!
			pCharBody->SetActiveSize(0);
			mpPlayer->MoveHeadPosAdd(eLuxHeadPosAdd_Main, 0, 1.6f, 0.05f);
		}
		else
		{
			//Some grunt or something?
		}
	}
}

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::ResetJumping()
{
	mbJumping = false;
}

//-----------------------------------------------------------------------

float cLuxMoveState_Normal::GetMoveSpeedMul()
{
	OhplGa2bMoveMulIn in;
	in.interaction = mpPlayer->GetInteractionMoveSpeedMul();
	in.script = mpPlayer->GetScriptMoveSpeedMul();
	in.event = mpPlayer->GetEventMoveSpeedMul();
	in.hurt = mpPlayer->GetHurtMoveSpeedMul();
	in.collapse = mpPlayer->GetInsanityCollapseSpeedMul();
	in.inWater = mpPlayer->IsInWater();
	in.water = in.inWater ? mpPlayer->GetWaterSpeedMul() : 0.0f;
	return ohpl_ga2b_move_speed_mul(&in);
}

//-----------------------------------------------------------------------

float cLuxMoveState_Normal::GetRunSpeedMul()
{
	return mpPlayer->GetScriptRunSpeedMul() * mpPlayer->GetEventRunSpeedMul();
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::UpdateMovement(float afTimeStep)
{
	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();

	mbRunning = mpPlayer->IsPressingRun();

	if(ohpl_ga2b_movement_update(pCharBody->IsOnGround(), mpPlayer->GetPressedMove(), &mbWasMoving, mbRunning,
								mpPlayer->GetCrouchDisabled(), mpPlayer->GetInsanityCollapse()->IsActive()))
	{
		SetCrouch(false);
	}
}

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::UpdateSpeedMultipliers(float afTimeStep)
{
	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();

	OhplGa2bSpeedCfg cfg = {mfCrouchForwardMul, mfCrouchBackwardMul, mfCrouchSidewayMul, mfRunForwardMul, mfRunBackwardMul,
							mfRunSidewayMul, mfInAirForwardMul, mfInAirBackwardMul, mfInAirSidewayMul};
	OhplGa2bSpeedMuls m;
	ohpl_ga2b_speed_multipliers(&cfg, GetMoveSpeedMul(), GetRunSpeedMul(), pCharBody->IsOnGround(), mbCrouching, mbRunning, &m);
	mfMaxForwardSpeedMul = m.fwd;
	mfMaxBackwardSpeedMul = m.back;
	mfMaxSidwaySpeedMul = m.side;
	mfDefaultForwardMul = m.defaultFwd;
}

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::UpdateJumpAndGroundCheck(float afTimeStep)
{
	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();

	ohpl_ga2b_ground_check(pCharBody->IsOnGround(), pCharBody->GetForceVelocity().y, &mbWasOnGround);

	OhplGa2bJumpState js = {mfJumpCount, mfMaxJumpCount, mbJumping};
	if(ohpl_ga2b_jump_tick(&js, afTimeStep))
	{
		mfJumpCount = js.count;
		mbJumping = js.jumping;

		iPhysicsWorld *pPhysicsWorld = gpBase->mpMapHandler->GetCurrentMap()->GetPhysicsWorld();
		cVector3f vGravity = pCharBody->GetCustomGravityActive() ?	pCharBody->GetCustomGravity() :
																	pPhysicsWorld->GetGravity();
		pCharBody->AddForce(cVector3f(0, ohpl_ga2b_jump_gravity_force(mfJumpCount, mfMaxJumpCount, vGravity.y, pCharBody->GetMass()),0));
	}
}

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::UpdateHeadBob(float afTimeStep)
{
	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();

	OhplGa2bBobCfg cfg = {mvWalkBobMax, mvRunBobMax, mvCrouchBobMax, mfWalkMinBobSpeed, mfWalkMaxBobSpeed, mfRunMinBobSpeed,
						mfRunMaxBobSpeed, mfCrouchMinBobSpeed, mfCrouchMaxBobSpeed, mfMaxForwardSpeed, mfGroundBounceSize,
						mfGroundBounceSpeed};
	OhplGa2bBobState s = {mvHeadBob, mfPrevHeadBobCount, mfHeadBobCount, mbBobbing, mbHeadGroundBounceActive, mfHeadGroundBounce,
						mvCurrentBobMax, mvBobMaxGoal, mfBounceSizeMul, mfBounceSpeedMul, mfDefaultForwardMul};
	OhplGa2bBobIn in = {pCharBody->GetVelocity(afTimeStep), mbCrouching, mbRunning, pCharBody->IsOnGround(),
						pCharBody->GetMovedLastUpdate()};

	bool bFootStep = ohpl_ga2b_head_bob(&s, &cfg, &in, afTimeStep);

	mvHeadBob = s.headBob;
	mfPrevHeadBobCount = s.prevCount;
	mfHeadBobCount = s.count;
	mbBobbing = s.bobbing;
	mbHeadGroundBounceActive = s.groundBounceActive;
	mfHeadGroundBounce = s.groundBounce;
	mvCurrentBobMax = s.curMax;
	mvBobMaxGoal = s.maxGoal;

	if(bFootStep) FootSound(eLuxFootSound_Step);

	mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_Bob,mvHeadBob);
}


//-----------------------------------------------------------------------

bool cLuxMoveState_Normal::UpdateLedgeCheck(float afTimeStep)
{
	//////////////////////////////
	// Skipping this for now! It messes up puzzles too much!
	return false;

	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();

	////////////////////////////
	//Check player is in the air and pressing jump
	if(pCharBody->IsOnGround() || mpPlayer->IsPressingJump()==false)
	{
		mfClimbLedgeCount =0;
		return false;
	}

	////////////////////////////
	//Check so counter is right
	mfClimbLedgeCount -= afTimeStep;
	if(mfClimbLedgeCount > 0) return false;
	
	mfClimbLedgeCount = mfMaxClimbLedgeCount;
	
	///////////////////////////////
	//Get the position for the ray
	//TODO: Should start a little bit above center, (var is specified in config!)
	cVector3f vStartPos = 	pCharBody->GetFeetPosition() + 
							cVector3f(0,pCharBody->GetSize().y - mfClimbLedgeCheckDistFromTop,0) + 
							pCharBody->GetForward()* (pCharBody->GetSize().x*0.5f + mfClimbLedgeCheckDistForward);
		
	cVector3f vStartPosAdd = cVector3f(0, -mfClimbLedgeCheckDistTopToEnd, 0);

	//////////////////////////////
	// Cast the ray and check if there is some ground ahead.
	float fMinDistance =1000000.0f;
	cVector3f vRayStart[3];
	vRayStart[0] = vStartPos;
	vRayStart[1] = vStartPos + pCharBody->GetRight() * pCharBody->GetSize().x/4;
	vRayStart[2] = vStartPos - pCharBody->GetRight() * pCharBody->GetSize().x/4;
	
	bool bIntersection = false;
	for(int i=0; i<3; ++i)
	{
		float fDistance;
		if(pCharBody->CheckRayIntersection(vRayStart[i], vRayStart[i]+vStartPosAdd, &fDistance, NULL))
		{
			bIntersection = true;
			if(fDistance < fMinDistance) fMinDistance = fDistance;
		}
	}
	
	if(bIntersection== false) return false;

	//////////////////////////////
	// Check if the new position works
	
	//Set up variables
	cVector3f vFeetPos = vStartPos - cVector3f(0,fMinDistance-0.01f,0);
	cVector3f vPushBack=0;
	
	//Check if fit
	bool bFitting = pCharBody->CheckCharacterFits(vFeetPos, true,-1, &vPushBack);

	//Check if the character will fit after applying push back vector.
	if(	bFitting==false && vPushBack.y > 0 && vPushBack.Length() <= mfClimbLedgeMaxPushDist)
	{
		bFitting = pCharBody->CheckCharacterFits(vFeetPos + vPushBack, true);
		if(bFitting) vFeetPos += vPushBack;
    }

	//Check if crouching works
	if( bFitting==false && mbCrouching==false)
	{
		bFitting = pCharBody->CheckCharacterFits(vFeetPos, true,1);
		if(bFitting) SetCrouch(true);
	}
	
	if(bFitting == false) return false;	
	
	//////////////////////////////////////
	// Player fits! Set up stuff

	//Find out how much the new old position differs from the new one and then translate this into
	// the local space of the character and set that as a head position add.
	//This so that it will be possible to fade to the new positon.
	cVector3f vPosDifference = pCharBody->GetFeetPosition() - vFeetPos;
	cVector3f vDiff =0;
	vDiff.y = vPosDifference.y;
	vDiff.z = cMath::Vector3Dot(pCharBody->GetForward(),vPosDifference);
	vDiff.x = cMath::Vector3Dot(pCharBody->GetRight(),vPosDifference);
	mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_ClimbLedge, vDiff);
	
	//Set the new position
	pCharBody->SetFeetPosition(vFeetPos);
	
	//Change the move state
	mpPlayer->ChangeMoveState(eLuxMoveState_ClimbLedge);

	return false;
}

//-----------------------------------------------------------------------

void cLuxMoveState_Normal::FootSound(eLuxFootSound aType)
{
	///////////////////////
	//Get Material
	iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
    iPhysicsMaterial *pGravityMat = pCharBody->GetGravityCollideMaterial();
	
	tString sMaterialStep;
	
	if(mpPlayer->IsInWater())
	{
		sMaterialStep = mpPlayer->GetWaterStepSound();
	}
	else
	{
		sMaterialStep = pGravityMat ? pGravityMat->GetSurfaceData()->GetStepType() : "rock";
	}
	
	///////////////////////
	//Get Sound data name
	tString sSoundDataName="";
	if(aType == eLuxFootSound_Step)
	{
		tString sMoveType = "walk";
		if(mbRunning) sMoveType = "run";
		if(mbCrouching) sMoveType = "sneak";
		if(mpPlayer->UsesDragFootsteps()) sMoveType = "drag";

		sSoundDataName = "step_"+sMoveType+"_"+sMaterialStep;
	}
	else if(aType == eLuxFootSound_Landing)
	{
		sSoundDataName = "step_run_"+sMaterialStep;
	}

	///////////////////////
	//Play sound
	cLuxSoundExtraData extraData;
	gpBase->mpHelpFuncs->PlayGuiSoundData(sSoundDataName, eSoundEntryType_World,1,eSoundEntityType_Main, true,&extraData);

	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();

	float fVolumeMul = 1.0f;
	float fDistanceMul = 1.0f;

	if (gpBase->mbHardMode)
	{
		if (mbRunning)
		{
			fVolumeMul = 2.0f;
			fDistanceMul = 1.25f;
		}
	}
	
	pMap->BroadcastEnemySoundMessage(	mpPlayer->GetCharacterBody()->GetFeetPosition() + cVector3f(0,0.1f,0),extraData.mfVolume * fVolumeMul,
										extraData.mfMinDistance, extraData.mfMaxDistance * fDistanceMul);
}

//-----------------------------------------------------------------------


void cLuxMoveState_Normal::FallDamage(float afYSpeed)
{
	if(mpPlayer->GetNoFallDamage()) return;

	OhplGa2bFallCfg cfg = {mfFallDamageSpeed_Min, mfFallDamage_Min, mfFallDamageSpeed_Med, mfFallDamage_Med, mfFallDamageSpeed_Max,
						mfFallDamage_Max};
	float fDamage;
	int lTier = ohpl_ga2b_fall_damage(&cfg, afYSpeed, &fDamage);
	if(lTier != 0)
	{
		const tString &sSound = lTier == 3 ? msFallDamageSound_Max : lTier == 2 ? msFallDamageSound_Med : msFallDamageSound_Min;

		mfBounceSizeMul = mfFallDamageBounceSizeMul;
		mfBounceSpeedMul = mfFallDamageBounceSpeedMul;

		mpPlayer->GiveDamage(fDamage,100,eLuxDamageType_BloodSplat, false, false);
		gpBase->mpHelpFuncs->PlayGuiSoundData(sSound, eSoundEntryType_World);
	}
}

//-----------------------------------------------------------------------




