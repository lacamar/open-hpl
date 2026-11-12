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

#include "LuxEnemy_ManPig.h"

#include "LuxEnemyMover.h"
#include "LuxEnemyPathfinder.h"

#include "LuxMap.h"
#include "LuxMapHelper.h"
#include "LuxMusicHandler.h"
#include "LuxDebugHandler.h"
#include "LuxGlobalDataHandler.h"

#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxEffectHandler.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEnemyLoader_ManPig::cLuxEnemyLoader_ManPig(const tString& asName) : iLuxEnemyLoader(asName)
{
}

//-----------------------------------------------------------------------

iLuxEnemy *cLuxEnemyLoader_ManPig::CreateEnemy(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxEnemy_ManPig, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxEnemyLoader_ManPig::LoadVariables(iLuxEnemy *apEnemy, cXmlElement *apRootElem)
{
	cLuxEnemy_ManPig *pManPig = static_cast<cLuxEnemy_ManPig*>(apEnemy);

	pManPig->mfDefaultForwardSpeed[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Walk] =	GetVarFloat("Quadruped_Walk_ForwardSpeed", 0);
	pManPig->mfDefaultBackwardSpeed[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Walk] =	GetVarFloat("Quadruped_Walk_BackwardSpeed", 0);
	pManPig->mfDefaultForwardAcc[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Walk] =		GetVarFloat("Quadruped_Walk_ForwardAcc", 0);
	pManPig->mfDefaultForwardDeacc[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Walk] =	GetVarFloat("Quadruped_Walk_ForwardDeacc", 0);

	pManPig->mfDefaultForwardSpeed[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Run] =		GetVarFloat("Quadruped_Run_ForwardSpeed", 0);
	pManPig->mfDefaultBackwardSpeed[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Run] =	GetVarFloat("Quadruped_Run_BackwardSpeed", 0);
	pManPig->mfDefaultForwardAcc[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Run] =		GetVarFloat("Quadruped_Run_ForwardAcc", 0);
	pManPig->mfDefaultForwardDeacc[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Run] =		GetVarFloat("Quadruped_Run_ForwardDeacc", 0);

	pManPig->mfStoppedToWalkSpeed[eLuxEnemyPoseType_Quadruped] =	GetVarFloat("Quadruped_StoppedToWalkSpeed", 0);
	pManPig->mfWalkToStoppedSpeed[eLuxEnemyPoseType_Quadruped] =	GetVarFloat("Quadruped_WalkToStoppedSpeed", 0);
	pManPig->mfWalkToRunSpeed[eLuxEnemyPoseType_Quadruped] =		GetVarFloat("Quadruped_WalkToRunSpeed", 0);
	pManPig->mfRunToWalkSpeed[eLuxEnemyPoseType_Quadruped] =		GetVarFloat("Quadruped_RunToWalkSpeed", 0);

	pManPig->msNoticeSound = GetVarString("NoticeSound");
	pManPig->msGiveUpNoticeSound = GetVarString("GiveUpNoticeSound");
	pManPig->msEnabledSound = GetVarString("EnabledSound");
	pManPig->msChaseSound = GetVarString("ChaseSound");
	gpBase->PreloadSound(pManPig->msNoticeSound);
	gpBase->PreloadSound(pManPig->msEnabledSound);
	gpBase->PreloadSound(pManPig->msChaseSound);

	pManPig->mfGroggyDamageCount = GetVarFloat("GroggyDamageCount", 0);
	pManPig->mfAlertToHuntDistance = GetVarFloat("AlertToHuntDistance", 0);
	pManPig->mfAlertToInstantHuntDistance = GetVarFloat("AlertToInstantHuntDistance", 0);
	pManPig->mfHuntPauseMinTime = GetVarFloat("HuntPauseMinTime", 0);
	pManPig->mfHuntPauseMaxTime = GetVarFloat("HuntPauseMaxTime", 0);
	pManPig->mfIncreaseAlertSpeedDistance = GetVarFloat("IncreaseAlertSpeedDistance", 0);
	pManPig->mfIncreasedAlertSpeedMul = GetVarFloat("IncreasedAlertSpeedMul", 0);
	
	pManPig->mfAlertRunTowardsToHuntLimit = GetVarFloat("AlertRunTowardsToHuntLimit", 0);
	pManPig->mfAlertRunTowardsCheckDistance = GetVarFloat("AlertRunTowardsCheckDistance", 0);

	pManPig->msTeslaMindFuckLoop = GetVarString("TeslaSoundLoop");
	pManPig->mbIsTelsa = GetVarBool("IsTelsa", false);
}

//-----------------------------------------------------------------------

static eLuxIdleBehavior ToIdleBehavior(const tString& asStr)
{
	if(asStr == "None") return eLuxIdleBehavior_None;
	if(asStr == "Stalk") return eLuxIdleBehavior_Stalk;
	if(asStr == "Track") return eLuxIdleBehavior_Track;

	Error("eLuxIdleBehavior behavior '%s' does not exist!\n", asStr.c_str());
	return eLuxIdleBehavior_None;
}

static eLuxEnemyMoveSpeed ToMoveSpeed(const tString& asStr)
{
	if(asStr == "Run") return eLuxEnemyMoveSpeed_Run;
	if(asStr == "Walk") return eLuxEnemyMoveSpeed_Walk;

	Error("eLuxEnemyMoveSpeed '%s' does not exist, falling back to walk!\n", asStr.c_str());
	return eLuxEnemyMoveSpeed_Walk;
}

void cLuxEnemyLoader_ManPig::LoadInstanceVariables(iLuxEnemy *apEnemy, cResourceVarsObject *apInstanceVars)
{
	cLuxEnemy_ManPig *pManPig = static_cast<cLuxEnemy_ManPig*>(apEnemy);

	pManPig->mbThreatenOnAlert = apInstanceVars->GetVarBool("ThreatenOnAlert", false);
	pManPig->mbFleeFromPlayer = apInstanceVars->GetVarBool("FleeFromPlayer", false);
	pManPig->mbAutoDisableAfterFlee = apInstanceVars->GetVarBool("AutoDisableAfterFlee", false);
	pManPig->mIdleBehavior = ToIdleBehavior(apInstanceVars->GetVarString("IdleBehavior", "None"));
	pManPig->mPatrolMoveSpeed = ToMoveSpeed(apInstanceVars->GetVarString("PatrolMoveSpeed", "Walk"));
	pManPig->mbAllowZeroWaitTime = apInstanceVars->GetVarBool("AllowZeroNodeWaitTimes", false);
	pManPig->mfDamageMul = apInstanceVars->GetVarFloat("DamageMul", 1.0f);
	pManPig->mfRunSpeedMul = apInstanceVars->GetVarFloat("RunSpeedMul", 1.0f);
	pManPig->mbPlayActivateSound = apInstanceVars->GetVarBool("PlayActivateSound", true);
	pManPig->mfLanternSensitivity = apInstanceVars->GetVarFloat("LanternSensitivity", 1.0f);
	pManPig->mfHuntPauseTimeMul = apInstanceVars->GetVarFloat("HuntPauseTimeMul", 1.0f);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEnemy_ManPig::cLuxEnemy_ManPig(const tString &asName, int alID, cLuxMap *apMap) : iLuxEnemy(asName,alID,apMap, eLuxEnemyType_ManPig)
{
	mfWaitTime =0;
	mfAlertRunTowardsCount = 0;

	mGa3Ctx.waitTime = &mfWaitTime;
	mGa3Ctx.alertRunTowardsCount = &mfAlertRunTowardsCount;
	mGa3Ctx.groggyDamageCount = &mfGroggyDamageCount;
	mGa3Ctx.alertToHuntDistance = &mfAlertToHuntDistance;
	mGa3Ctx.alertToInstantHuntDistance = &mfAlertToInstantHuntDistance;
	mGa3Ctx.huntPauseMinTime = &mfHuntPauseMinTime;
	mGa3Ctx.huntPauseMaxTime = &mfHuntPauseMaxTime;
	mGa3Ctx.increaseAlertSpeedDistance = &mfIncreaseAlertSpeedDistance;
	mGa3Ctx.increasedAlertSpeedMul = &mfIncreasedAlertSpeedMul;
	mGa3Ctx.alertRunTowardsToHuntLimit = &mfAlertRunTowardsToHuntLimit;
	mGa3Ctx.alertRunTowardsCheckDistance = &mfAlertRunTowardsCheckDistance;
	mGa3Pig.lastShortMiss = &mbLastShortAttackWasMiss;
	mGa3Pig.forceCharge = &mbForceChargeAttack;
	mGa3Pig.fleeInvisibleCount = &mfFleeCheckIfInvisbleCount;
	mGa3Pig.threatenOnAlert = &mbThreatenOnAlert;
	mGa3Pig.fleeFromPlayer = &mbFleeFromPlayer;
	mGa3Pig.autoDisableAfterFlee = &mbAutoDisableAfterFlee;
	mGa3Pig.idleBehavior = reinterpret_cast<int*>(&mIdleBehavior);
	mGa3Pig.patrolMoveSpeed = reinterpret_cast<int*>(&mPatrolMoveSpeed);
	mGa3Pig.allowZeroWait = &mbAllowZeroWaitTime;
	mGa3Pig.damageMul = &mfDamageMul;
	mGa3Pig.runSpeedMul = &mfRunSpeedMul;
	mGa3Pig.playActivateSound = &mbPlayActivateSound;
	mGa3Pig.huntPauseTimeMul = &mfHuntPauseTimeMul;
	mGa3Pig.isTesla = &mbIsTelsa;
	mGa3Pig.teslaNotice = &mfTeslaSpecialNoticeCount;
	mGa3Pig.inLanternCount = &mfInLanternLightCount;
	mGa3Pig.checkFlashLight = &mfCheckFlashLightShining;
	mGa3Pig.lanternSensitivity = &mfLanternSensitivity;
	mGa3Pig.disableTriggers = &mbDisableTriggers;

	mfCheckFlashLightShining =0;

	mfFleeCheckIfInvisbleCount=0;
	
	mbAlignEntityWithGroundRay = true;

	mbIsTelsa=false;

	mbLastShortAttackWasMiss = false;
	mbForceChargeAttack = false;

	mfTeslaFlickerTimer=0;
	mlTeslaFlickerState=0;
	mfTeslaEffectAmount = 1.0f;
	mfTeslaSpecialNoticeCount =0;
	mbTeslaMindFuckActive = false;
	mfTeslaMindFuckPulse =0;
	mfTeslaMindFuckPulseAdd=1;
	mbTeslaTerror = false;
	mfBlackOutCount=0;
	mfBlackOutDurationCount=0;
	mbTeslaForceSighting = false;
	mbTeslaFadeDisabled = false;
	mbTeslaSoundDisabled = false;
	mbTeslaEasyEscapeDisabled = false;

	mbThreatenOnAlert = false;
	mbFleeFromPlayer = false;
	mIdleBehavior = eLuxIdleBehavior_None;

	mCurrentMoveType = eLuxEnemyMoveType_Normal;

	mfDamageMul = 1.0f;
	mfRunSpeedMul = 1.0f;

	mpHuntSound = NULL;
	mlHuntSoundId = -1;

	mpMindFuckSound = NULL;
	mlMindFuckSoundId = -1;

	for(int i=0; i<eLuxEnemyMoveType_LastEnum; ++i)
	{
		eLuxEnemyPoseType pose = eLuxEnemyPoseType_Biped;
		msIdleAnimationName[i][pose] = "IdleBiped";
		msWalkAnimationName[i][pose] = "WalkBiped";
		msRunAnimationName[i][pose] =  i==eLuxEnemyMoveType_Normal ? "RunBiped" : "FleeBiped";;

		pose = eLuxEnemyPoseType_Quadruped;
		msIdleAnimationName[i][pose] = "IdleQuadruped";
		msWalkAnimationName[i][pose] = "WalkQuadruped";
		msRunAnimationName[i][pose] = i==eLuxEnemyMoveType_Normal ? "RunQuadruped" : "FleeQuadruped";
	}
}

//-----------------------------------------------------------------------

cLuxEnemy_ManPig::~cLuxEnemy_ManPig()
{
	if(mbIsTelsa) ResetMindFuckEffects();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxEnemy_ManPig::OnSetupAfterLoad(cWorld *apWorld)
{
	if(mbIsTelsa) mpMeshEntity->SetVisible(false);
}


//-----------------------------------------------------------------------

void cLuxEnemy_ManPig::OnAfterWorldLoad()
{
	mvDefaultLightColors.clear();
	for(size_t i=0; i<mvLights.size(); ++i)
	{
		mvDefaultLightColors.push_back(mvLights[i]->GetDiffuseColor());
	}

	
}

//-----------------------------------------------------------------------

void cLuxEnemy_ManPig::UpdateEnemySpecific(float afTimeStep)
{
	if(mbIsTelsa) UpdateTesla(afTimeStep);
	
	UpdateCheckInLantern(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxEnemy_ManPig::ChangePose(eLuxEnemyPoseType aPose, bool abSendMessage)
{
	if(mCurrentPose == aPose) return;

	//Make sure the enemy is still and stays so for 0.5 seconds, and animation is updated for that.
	mpCharBody->StopMovement();
	mpCharBody->SetMoveDelay(0.5);
	mpMover->UpdateMoveAnimation(0.001f);

	//Set new pose state
	eLuxEnemyPoseType prevPose = mCurrentPose;
	mCurrentPose = aPose;
		
	//Make sure that the correct animation is set.
	mpMover->mMoveState = eLuxEnemyMoveState_LastEnum;
	mpMover->UpdateMoveAnimation(0.001f);

	if(abSendMessage)
		SendMessage(eLuxEnemyMessage_ChangePose, 0, false, 0,0, aPose);
}

//-----------------------------------------------------------------------

void cLuxEnemy_ManPig::ChangeMoveType(eLuxEnemyMoveType aMoveType)
{
	if(mCurrentMoveType == aMoveType) return;

	mCurrentMoveType = aMoveType;

	mpMover->mMoveState = eLuxEnemyMoveState_LastEnum;
	mpMover->UpdateMoveAnimation(0.001f);
}

//-----------------------------------------------------------------------

bool cLuxEnemy_ManPig::StateEventImplement(int alState, eLuxEnemyStateEvent aEvent, cLuxStateMessage *apMessage)
{
	OhplGa3Host host = Ga3PigHost();
	return ohpl_ga3b_pig_event(&mGa3Ctx, &mGa3Pig, &host, alState, aEvent, (const OhplGa3Msg*)apMessage);
}

OhplGa3Host cLuxEnemy_ManPig::Ga3PigHost()
{
	OhplGa3Host host;
	host.ctx = this;
	host.call = &cLuxEnemy_ManPig::Ga3PigCall;
	return host;
}

const tString* cLuxEnemy_ManPig::Ga3SoundName(int alKind)
{
	if(alKind == 0) return &msNoticeSound;
	if(alKind == 1) return &msGiveUpNoticeSound;
	return &msEnabledSound;
}

int cLuxEnemy_ManPig::Ga3PigCall(void *apEnemy, unsigned alOp, OhplGa3Io *apIo)
{
	cLuxEnemy_ManPig *pPig = static_cast<cLuxEnemy_ManPig*>(apEnemy);
	cVector3f vPos(apIo->v[0][0], apIo->v[0][1], apIo->v[0][2]);
	switch(alOp)
	{
	case eOhplGa3bOp_SeenByPlayer:		return pPig->IsSeenByPlayer() ? 1 : 0;
	case eOhplGa3bOp_InFovAtFeet:		return pPig->IsInPlayerFovAtFeetPos(vPos) ? 1 : 0;
	case eOhplGa3bOp_VisibleAtFeet:		return pPig->IsVisibleToPlayerAtFeetPos(vPos) ? 1 : 0;
	case eOhplGa3bOp_TeleportFeet:		pPig->mpCharBody->SetFeetPosition(vPos); return 0;
	case eOhplGa3bOp_BodySetMoveDelay:	pPig->mpCharBody->SetMoveDelay(apIo->f[0]); return 0;
	case eOhplGa3bOp_PlayHuntSound:		pPig->PlayHuntSound(); return 0;
	case eOhplGa3bOp_InsidePlayerView:	return pPig->InsidePlayerView() ? 1 : 0;
	case eOhplGa3bOp_ForceTesla:		pPig->ForceTeslaSighting(); return 0;
	case eOhplGa3bOp_PlayerYaw:			apIo->f[0] = gpBase->mpPlayer->GetCharacterBody()->GetYaw(); return 0;
	case eOhplGa3bOp_PatrolNodeInfo:
	{
		cLuxEnemyPatrolNode *pNode = pPig->Ga3PatrolNode(apIo->i[0]);
		if(pNode == NULL) return 0;
		apIo->f[0] = pNode->mfWaitTime;
		if(pNode->msAnimation == "") return 1;
		return pNode->mbLoopAnimation ? 3 : 2;
	}
	case eOhplGa3bOp_AttackEx:
	{
		if(apIo->i[0] == 2) return pPig->Attack(pPig->mNormalAttackSize, pPig->mBreakDoorAttackDamage, 20.0f) ? 1 : 0;
		return pPig->Attack(pPig->mNormalAttackSize, pPig->mNormalAttackDamage, pPig->mfDamageMul) ? 1 : 0;
	}
	case eOhplGa3Op_PlayAnim:
	{
		int lKind = apIo->i[0];
		if(lKind < eOhplGa3bAnim_IdleExtraPose) break;
		int lN = apIo->i[1];
		tString sSuffix = pPig->GetCurrentPoseSuffix();
		tString sName;
		switch(lKind)
		{
		case eOhplGa3bAnim_IdleExtraPose:	sName = "Idle"+sSuffix+"Extra"+cString::ToString(lN); break;
		case eOhplGa3bAnim_NoticePose:		sName = "Notice"+sSuffix; break;
		case eOhplGa3bAnim_ThreatLoop:		sName = "ThreatLoop"+sSuffix; break;
		case eOhplGa3bAnim_AttackPose:		sName = "Attack"+sSuffix+cString::ToString(lN); break;
		case eOhplGa3bAnim_Bite:			sName = "Bite"+sSuffix; break;
		case eOhplGa3bAnim_Charge:			sName = "Charge"+sSuffix; break;
		case eOhplGa3bAnim_FlinchPose:		sName = "Flinch"+sSuffix; break;
		case eOhplGa3bAnim_DeadPlain:		sName = "Dead"; break;
		case eOhplGa3bAnim_PatrolNode:
		{
			cLuxEnemyPatrolNode *pNode = pPig->Ga3PatrolNode(lN);
			if(pNode) sName = pNode->msAnimation;
			break;
		}
		}
		return pPig->PlayAnimImpl(sName, apIo->i[2] != 0, apIo->f[0], apIo->i[3] != 0, apIo->f[1], apIo->i[4] != 0, apIo->i[6] != 0) ? 1 : 0;
	}
	}
	return iLuxEnemy::Ga3Call(apEnemy, alOp, apIo);
}


//-----------------------------------------------------------------------

void cLuxEnemy_ManPig::OnRenderSolidImplemented(cRendererCallbackFunctions* apFunctions)
{
	iPhysicsWorld *pPhysicsWorld = mpMap->GetPhysicsWorld();

	if(mCurrentState == eLuxEnemyState_AttackMeleeShort)
	{
		pPhysicsWorld->RenderShapeDebugGeometry(GetAttackShape(0), GetDamageShapeMatrix(cVector3f(0,0,1)), apFunctions->GetLowLevelGfx(),
												cColor(1,0,0,1));
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxEnemy_ManPig::PlayerIsDetected()
{
	OhplGa3Host host = Ga3PigHost();
	return ohpl_ga3_grunt_player_is_detected(&mGa3Ctx, &host);
}

//-----------------------------------------------------------------------

void cLuxEnemy_ManPig::OnDisableTriggers()
{
	if(mCurrentState == eLuxEnemyState_Idle) return;

	ChangeState(eLuxEnemyState_Patrol);
}

//-----------------------------------------------------------------------

float cLuxEnemy_ManPig::GetDamageMul(float afAmount, int alStrength)
{
	return ohpl_ga3_grunt_damage_mul(mCurrentState);
}

//-----------------------------------------------------------------------

void cLuxEnemy_ManPig::OnSetActiveEnemySpecific(bool abX)
{
	if(mbIsTelsa)
	{
		mpMeshEntity->SetVisible(false);

		for(size_t i=0; i<mvLights.size(); ++i)
		{
			mvLights[i]->SetVisible(abX);
		}
	}

	if(abX==false && mbIsTelsa)
	{
		ResetMindFuckEffects();
		mbTeslaTerror = false;
		mfTeslaSpecialNoticeCount =0;

		gpBase->mpMusicHandler->RemoveEnemy(eLuxEnemyMusic_Attack,this);
		gpBase->mpMusicHandler->RemoveEnemy(eLuxEnemyMusic_Search,this);
	}
}

//-----------------------------------------------------------------------

void cLuxEnemy_ManPig::PlayHuntSound()
{
	if(mpHuntSound && mpWorld->SoundEntityExists(mpHuntSound, mlHuntSoundId)) return;

	mpHuntSound = PlaySound(msChaseSound);
	if(mpHuntSound) mlHuntSoundId = mpHuntSound->GetCreationID();
}

//-----------------------------------------------------------------------

bool cLuxEnemy_ManPig::InsidePlayerView()
{
	cFrustum *pFrust = gpBase->mpPlayer->GetCamera()->GetFrustum();

	//Log("Check inside ray:\n");

	/////////////////////////
	//If near enough, just check if in frustum
	if(cMath::Vector3Dist(mpCharBody->GetPosition(), gpBase->mpPlayer->GetCharacterBody()->GetPosition())<2.25f)
	{
		//Log(" true: close!\n");
		return pFrust->CollideBoundingVolume(mpCharBody->GetCurrentBody()->GetBoundingVolume())!=eCollision_Outside;
	}
	
	/////////////////////////
	//Check inside frustum
	if(pFrust->CollideBoundingVolume(mpCharBody->GetCurrentBody()->GetBoundingVolume())!=eCollision_Inside)
	{
		//Log(" false: outside frustum!\n");
		return false;
	}

	/////////////////////////
	//Check line of sight
	cVector3f vStart = pFrust->GetOrigin();
	cVector3f vPositions[3];
	vPositions[0] = mpCharBody->GetPosition();

	cVector3f vSideAdd = mpCharBody->GetRight()*mpCharBody->GetSize().x*0.4f;
	vPositions[1] = mpCharBody->GetPosition() + vSideAdd;
	vPositions[2] = mpCharBody->GetPosition() - vSideAdd;
	
	for(int i=0; i<3; ++i)
	{
		if(gpBase->mpMapHelper->CheckLineOfSight(vStart, vPositions[i], true))
		{
			//Log(" true: ray %d!\n", i);
			return true;
		}
	}

	//Log(" false: no rays!\n");
	return false;
}

void cLuxEnemy_ManPig::UpdateCheckInLantern(float afTimeStep)
{
	OhplGa3Host host = Ga3PigHost();
	ohpl_ga3b_pig_update_lantern(&mGa3Ctx, &mGa3Pig, &host, afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxEnemy_ManPig::ForceTeslaSighting()
{
	if(mbIsTelsa==false) return;

	if(mlMindFuckBlinkState==1)
	{
		if(mfMindFuckBlinkAmount>=1)
			mpMeshEntity->SetVisible(true);
		else
			mbTeslaForceSighting = true;
	}
	else
	{
		mfMindFuckBlinkCount =0;
		mbTeslaForceSighting = true;
	}
	
	//mlMindFuckBlinkState=1;
}

void cLuxEnemy_ManPig::SetTeslaEffectsAmount(float afX)
{
	for(size_t i=0; i<mvLights.size(); ++i)
	{
		mvLights[i]->SetDiffuseColor(mvDefaultLightColors[i]*afX);
	}
	mpMeshEntity->SetIlluminationAmount(afX);
}

void cLuxEnemy_ManPig::ResetMindFuckEffects()
{
	gpBase->mpEffectHandler->GetImageTrail()->FadeTo(0, 1);
	gpBase->mpPlayer->FadeAspectMulTo(1.0f, 10.0f);
	gpBase->mpPlayer->FadeFOVMulTo(1.0f,1.0f);
	mbTeslaMindFuckActive = false;
	mfTeslaMindFuckPulse =0;
	mfTeslaMindFuckPulseAdd=1;
	if (mbTeslaFadeDisabled == false)
		gpBase->mpEffectHandler->GetFade()->SetDirectAlpha(0);

	if(mpMindFuckSound)
	{
		cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
		
		if(pSoundHandler->IsValid(mpMindFuckSound, mlMindFuckSoundId))
		{
			mpMindFuckSound->FadeOut(2);
		}
		mpMindFuckSound=NULL;
	}

	mfBlackOutCount=0;
	mfBlackOutDurationCount=0;

	mfMindFuckBlinkCount=0;
	mfMindFuckBlinkAmount=0;
	mlMindFuckBlinkState=0;

	mpMeshEntity->SetVisible(false);
}

void cLuxEnemy_ManPig::SetTeslaFadeDisabled(bool abX)
{
	mbTeslaFadeDisabled = abX;
}

void cLuxEnemy_ManPig::SetTeslaSoundDisabled(bool abX)
{
	mbTeslaSoundDisabled = abX;

	if(abX == true && mpMindFuckSound)
	{
		cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
		
		if(pSoundHandler->IsValid(mpMindFuckSound, mlMindFuckSoundId))
		{
			mpMindFuckSound->FadeOut(2);
		}
		mpMindFuckSound=NULL;
	}
}

void cLuxEnemy_ManPig::SetTeslaEasyEscapeDisabled(bool abX)
{
	mbTeslaEasyEscapeDisabled = abX;
}

//-----------------------------------------------------------------------

void cLuxEnemy_ManPig::UpdateTesla(float afTimeStep)
{
	if(gpBase->mpPlayer->IsDead() || IsActive()==false)
	{
		if(mbTeslaMindFuckActive)
		{
			ResetMindFuckEffects();
		}
		
		return;
	}

	const float fMaxNoticeCount = 1.0f;
	///////////////////////////////////////
	// Tesla Flickering
	{
		mfBlackOutCount -= afTimeStep;
		if(mfBlackOutCount<0)
		{
			mfBlackOutDurationCount = cMath::RandRectf(2.0f, 4.0f);
			mfBlackOutCount = mfBlackOutDurationCount + cMath::RandRectf(2.0f, 8.0f);
		}

		if(mfBlackOutDurationCount>0) mfBlackOutDurationCount -= afTimeStep;
		
		mfTeslaFlickerTimer-= afTimeStep;
		if(mfTeslaFlickerTimer<=0)
		{
			if(mlTeslaFlickerState==0)
			{
				mlTeslaFlickerState = 1;
				mfTeslaFlickerTimer = cMath::RandRectf(0.05f, 0.2f);
				//if (cMath::RandRectl(0,8)==0)
				//{
				//	mfTeslaFlickerTimer = cMath::RandRectf(0.7f, 1.3f);
				//}
				//else
				//{
				//	mfTeslaFlickerTimer = cMath::RandRectf(0.05f, 0.2f);
				//}
			}
			else
			{
				mlTeslaFlickerState = 0;
				mfTeslaFlickerTimer = cMath::RandRectl(0,8)==0 ? cMath::RandRectf(0.1f, 0.2f) : cMath::RandRectf(0.05f, 0.15f);
			}
		}

		if(mfBlackOutDurationCount>0 && mfTeslaSpecialNoticeCount<0.7f)
		{
			mfTeslaEffectAmount -= afTimeStep*10;
			if(mfTeslaEffectAmount<0) mfTeslaEffectAmount =0;
		}
		else if(mlTeslaFlickerState==0)
		{
			mfTeslaEffectAmount -= afTimeStep*25;
			if(mfTeslaEffectAmount<0) mfTeslaEffectAmount =0;
		}
		else
		{
			mfTeslaEffectAmount += afTimeStep*16;
			if(mfTeslaEffectAmount>1) mfTeslaEffectAmount =1;
		}

		SetTeslaEffectsAmount(mlTeslaFlickerState==1 || mpMeshEntity->IsVisible() == false ? 0.0f : 1.0f);
	}

	///////////////////////////////////////
	// Special Notice Checks
	{
		bool bWasNoticed = false;

		//////////////////////////////////
		// Movement
		/*const float fMinSpeed = 1.4;
		float fSpeed = gpBase->mpPlayer->GetAvgSpeed();
		if(fSpeed>fMinSpeed)
		{
			float fMul = 0.15f;
			float fDistance = DistToPlayer2D();
			
			if(fDistance>30) fMul *= 0.5f;
			else if(fDistance>15) fMul *= 0.75f;

			mfTeslaSpecialNoticeCount += (fSpeed/fMinSpeed)*afTimeStep*fMul;
			bWasNoticed = true;
		}*/
		
		//////////////////////////////////
		// Vision
		/*if(mbIsSeenByPlayer)// InsidePlayerView())
		{
			mfTeslaSpecialNoticeCount += afTimeStep*0.15f;
			bWasNoticed = true;
		}*/

		///////////////////////////////
		// Terror
		/*if(mbTeslaTerror)
		{
			mfTeslaSpecialNoticeCount += afTimeStep;
		}*/

		//if(mfTeslaSpecialNoticeCount>fMaxNoticeCount) mfTeslaSpecialNoticeCount =fMaxNoticeCount;

		///////////////////////////////
		// Distance
		float fDistance = DistToPlayer2D();
		if(fDistance < 20)
		{
			mfTeslaSpecialNoticeCount = cMath::Clamp(1.0f-fDistance/19.0f, 0.0f, 1.0f);
		}

		///////////////////////////////
		// No notice
		if(bWasNoticed==false)
		{
			mfTeslaSpecialNoticeCount -= afTimeStep*0.5f;
			if(mfTeslaSpecialNoticeCount<0.0f) mfTeslaSpecialNoticeCount=0;
		}

		///////////////////////////////
		// Check if spotted
		/*if(mfTeslaSpecialNoticeCount>=1.0f)
		{
			ShowPlayerPosition();
		}*/

		///////////////////////////////
		// Make it easier to escape.
		if(mbTeslaEasyEscapeDisabled==false && 
		   mfTeslaSpecialNoticeCount<0.35f && 
		   mbCanSeePlayer == false &&
		   gpBase->mpPlayer->GetHealth()<75)
		{
			if(mCurrentState == eLuxEnemyState_Hunt ||
				mCurrentState == eLuxEnemyState_HuntPause ||
				mCurrentState == eLuxEnemyState_HuntWander)
			{
				mfWaitTime=0.5f;
				gpBase->mpPlayer->RemoveTerrorEnemy(this);
				ChangeState(eLuxEnemyState_Wait);
				mbPlayerDetected=false;
				mbCanSeePlayer=false;
			}
		}

		//gpBase->mpDebugHandler->AddMessage(_W("Notice: ")+cString::ToStringW(mfTeslaSpecialNoticeCount), false);

	}

	///////////////////////////////////////
	// Effects
	{
		/////////////////////////
		// Active
		if(mfTeslaSpecialNoticeCount>0.0f)
		{
			mbTeslaMindFuckActive = true;

			////////////////////////
			// Image trail
			//gpBase->mpEffectHandler->GetImageTrail()->FadeTo(mfTeslaSpecialNoticeCount, 3.0f);

			////////////////////////
			// Pulse update
			mfTeslaMindFuckPulse += afTimeStep*mfTeslaMindFuckPulseAdd*3*mfTeslaSpecialNoticeCount;
			if(mfTeslaMindFuckPulse>1 && mfTeslaMindFuckPulseAdd>0)
			{
				mfTeslaMindFuckPulse = 1;
				mfTeslaMindFuckPulseAdd = -1;
			}
			if(mfTeslaMindFuckPulse<0 && mfTeslaMindFuckPulseAdd<0)
			{
				mfTeslaMindFuckPulse = 0;
				mfTeslaMindFuckPulseAdd = 1;
			}

			////////////////////////
			// Aspect Mul
			//gpBase->mpPlayer->FadeAspectMulTo(1.0f + mfTeslaMindFuckPulse*mfTeslaSpecialNoticeCount*0.3f, 10.0f);
			//gpBase->mpPlayer->FadeFOVMulTo(1.0f - mfTeslaMindFuckPulse*mfTeslaSpecialNoticeCount*0.3f, 10.0f);

			////////////////////////
			// Blink
			{
				float fCountMul = 1.0f;
				if(mlMindFuckBlinkState==0)
				{
					if(mfMindFuckBlinkCount<0.3f) fCountMul= mfTeslaSpecialNoticeCount;
				}
				else
				{
					if(mfTeslaSpecialNoticeCount<0.7f && mfMindFuckBlinkCount>0.2f) fCountMul += (1-mfTeslaSpecialNoticeCount)*5;
				}
				
				mfMindFuckBlinkCount-=afTimeStep*fCountMul;
				if(mfMindFuckBlinkCount<0)
				{
					if(mlMindFuckBlinkState==0)
					{
						mlMindFuckBlinkState=1;
						if (mfTeslaSpecialNoticeCount < 0.5f && cMath::RandRectl(0,8)==0)
							mfMindFuckBlinkCount = cMath::RandRectf(0.75f, 1.2f);
						else
							mfMindFuckBlinkCount = cMath::RandRectf(0.1f, 0.3f);
					}
					else
					{
						mlMindFuckBlinkState=0;
						mfMindFuckBlinkCount = cMath::RandRectl(0,4)==0 ? cMath::RandRectf(0.75f, 2.2f) : cMath::RandRectf(0.1f, 0.5f);
					}
				}

				if(mlMindFuckBlinkState==0 && mfMindFuckBlinkAmount>0)
				{
					mfMindFuckBlinkAmount -= afTimeStep*6;
					if(mfMindFuckBlinkAmount<0){
						mfMindFuckBlinkAmount=0;
					}
				}
				if(mlMindFuckBlinkState==1 && mfMindFuckBlinkAmount<1)
				{
					mfMindFuckBlinkAmount += afTimeStep*15;
					if(mfMindFuckBlinkAmount>1)
					{
						mfMindFuckBlinkAmount=1;
						
						mpMeshEntity->SetVisible(false);
						int lMaxRand =  3;
						if(mfTeslaSpecialNoticeCount<0.7f || mCurrentState==eLuxEnemyState_Patrol)
						{
							lMaxRand =2;
						}
						if(mbTeslaForceSighting || cMath::RandRectl(0, lMaxRand)==0) mpMeshEntity->SetVisible(true);

						mbTeslaForceSighting = false;
					}
				}
						
				//float fDarkness = 0.4f + (mfTeslaSpecialNoticeCount/fMaxNoticeCount)*0.5f;
				if (mbTeslaFadeDisabled == false)
					gpBase->mpEffectHandler->GetFade()->SetDirectAlpha(mfMindFuckBlinkAmount);
			}
			

			////////////////////////
			// Sound
			if (mbTeslaSoundDisabled == false)
			{
				cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
				if(mpMindFuckSound==NULL || pSoundHandler->IsValid(mpMindFuckSound, mlMindFuckSoundId)==false)
				{
					mpMindFuckSound = pSoundHandler->PlayGui(msTeslaMindFuckLoop, true, 1.0f);
					if(mpMindFuckSound)
					{
						mlMindFuckSoundId = mpMindFuckSound->GetId();
					}
				}
				else
				{
					mpMindFuckSound->FadeVolumeMulTo(0.1f + (mfTeslaSpecialNoticeCount/fMaxNoticeCount)*0.9, 10.0f);
				}
			}
		}
		/////////////////////////
		// Not active
		else
		{
			if(mbTeslaMindFuckActive)
			{
				ResetMindFuckEffects();
			}
		}


	}

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxEnemy_ManPig_SaveData, iLuxEnemy_SaveData)

kSerializeVar(mbThreatenOnAlert, eSerializeType_Bool)
kSerializeVar(mbFleeFromPlayer, eSerializeType_Bool)
kSerializeVar(mbAutoDisableAfterFlee, eSerializeType_Bool)
kSerializeVar(mfLanternSensitivity, eSerializeType_Float32)
kSerializeVar(mIdleBehavior, eSerializeType_Int32)
kSerializeVar(mPatrolMoveSpeed, eSerializeType_Int32)
kSerializeVar(mfDamageMul, eSerializeType_Float32)
kSerializeVar(mfRunSpeedMul, eSerializeType_Float32)
kSerializeVar(mfInLanternLightCount, eSerializeType_Float32)
kSerializeVar(mbAllowZeroWaitTime, eSerializeType_Bool)
kSerializeVar(mfHuntPauseTimeMul, eSerializeType_Float32)
kSerializeVar(mbTeslaTerror, eSerializeType_Bool)
kSerializeVar(mbTeslaFadeDisabled, eSerializeType_Bool)
kSerializeVar(mbTeslaSoundDisabled, eSerializeType_Bool)
kSerializeVar(mbTeslaEasyEscapeDisabled, eSerializeType_Bool)
kSerializeVar(mlCurrentPose, eSerializeType_Int32)
kSerializeVar(mlCurrentMoveType, eSerializeType_Int32)
kSerializeVar(mbSkipVisibilityRangeHandicaps, eSerializeType_Bool)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxEnemy_ManPig::CreateSaveData()
{
	return hplNew(cLuxEnemy_ManPig_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxEnemy_ManPig::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxEnemy_ManPig_SaveData *pData = static_cast<cLuxEnemy_ManPig_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	kCopyToVar(pData,mbThreatenOnAlert);
	kCopyToVar(pData,mbFleeFromPlayer);
	kCopyToVar(pData,mbAutoDisableAfterFlee);
	kCopyToVar(pData,mfLanternSensitivity);
	kCopyToVar(pData,mIdleBehavior);
	kCopyToVar(pData,mPatrolMoveSpeed);
	kCopyToVar(pData,mfRunSpeedMul);
	kCopyToVar(pData,mfDamageMul);
	kCopyToVar(pData,mfInLanternLightCount);
	kCopyToVar(pData,mbAllowZeroWaitTime);
	kCopyToVar(pData,mfHuntPauseTimeMul);
	kCopyToVar(pData,mbTeslaTerror);
	kCopyToVar(pData,mbTeslaFadeDisabled);
	kCopyToVar(pData,mbTeslaSoundDisabled);
	kCopyToVar(pData,mbTeslaEasyEscapeDisabled);

	pData->mlCurrentPose = mCurrentPose;
	pData->mlCurrentMoveType = mCurrentMoveType;

	kCopyToVar(pData,mbSkipVisibilityRangeHandicaps);
}

//-----------------------------------------------------------------------

void cLuxEnemy_ManPig::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxEnemy_ManPig_SaveData *pData = static_cast<cLuxEnemy_ManPig_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	kCopyFromVar(pData,mbThreatenOnAlert);
	kCopyFromVar(pData,mbFleeFromPlayer);
	kCopyFromVar(pData,mbAutoDisableAfterFlee);
	kCopyFromVar(pData,mfLanternSensitivity);
	mIdleBehavior  = (eLuxIdleBehavior)pData->mIdleBehavior;
	mPatrolMoveSpeed  = (eLuxEnemyMoveSpeed)pData->mPatrolMoveSpeed;
	kCopyFromVar(pData,mfRunSpeedMul);
	kCopyFromVar(pData,mfDamageMul);
	kCopyFromVar(pData,mfInLanternLightCount);
	kCopyFromVar(pData,mbAllowZeroWaitTime);
	kCopyFromVar(pData,mfHuntPauseTimeMul);
	kCopyFromVar(pData,mbTeslaTerror);
	kCopyFromVar(pData,mbTeslaFadeDisabled);
	kCopyFromVar(pData,mbTeslaSoundDisabled);
	kCopyFromVar(pData,mbTeslaEasyEscapeDisabled);

	mCurrentPose = (eLuxEnemyPoseType)pData->mlCurrentPose;
	mCurrentMoveType = (eLuxEnemyMoveType)pData->mlCurrentMoveType;

	kCopyFromVar(pData,mbSkipVisibilityRangeHandicaps);
}

//-----------------------------------------------------------------------

void cLuxEnemy_ManPig::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------


