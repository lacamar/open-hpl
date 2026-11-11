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

#include "LuxEnemy.h"

#include <cstring>

#include "LuxEnemyMover.h"
#include "LuxEnemyPathfinder.h"

#include "LuxMap.h"
#include "LuxMapHelper.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxInteractConnections.h"
#include "LuxMusicHandler.h"
#include "LuxGlobalDataHandler.h"
#include "LuxEffectRenderer.h"
#include "LuxMoveState_Normal.h"
#include "LuxDebugHandler.h"
#include "LuxProgressLogHandler.h"
#include "LuxHelpFuncs.h"

#include "LuxProp.h"
#include "LuxProp_Object.h"

//////////////////////////////////////////////////////////////////////////
// Global Vars
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

tWString gsLuxEnemyStates[] =
{
	_W("Idle"),
	_W("GoHome"),

	_W("Wait"),
	_W("Patrol"),
	_W("Investigate"),
	_W("Alert"),
	_W("Search"),
	_W("Eat"),

	_W("Hurt"),
	
	_W("Hunt"),
	_W("HuntPause"),
	_W("HuntWander"),
	
	_W("AttackMeleeShort"),
	_W("AttackMeleeLong"),
	_W("AttackRange"),
	_W("BreakDoor"),
	
	_W("Dead"),
	
	_W("NULL"), // End of pre-Pig states
	
	_W("Flee"),
	_W("Stalk"),
	_W("Track"),
	
	_W("NULL")
};

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxEnemyLoader::iLuxEnemyLoader(const tString& asName) : cEntityLoader_Object(asName)
{
	mfDefaultMaxFocusDistance = gpBase->mpGameCfg->GetFloat("Player_Interaction", "Default_DefaultMaxFocusDist",0);
}

//-----------------------------------------------------------------------

void iLuxEnemyLoader::BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{

}

//-----------------------------------------------------------------------

static eLuxEnemyPoseType ToPoseType(const tString& asPose)
{
	tString sLowStr = cString::ToLowerCase(asPose);

	if(sLowStr == "biped") return eLuxEnemyPoseType_Biped;
	if(sLowStr == "quadruped") return eLuxEnemyPoseType_Quadruped;

	Error("Pose type '%s' does not exist! Using biped\n", asPose.c_str());
	return eLuxEnemyPoseType_Biped;
}

//-----------------------------------------------------------------------

void iLuxEnemyLoader::AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	cLuxMap *pMap = gpBase->mpCurrentMapLoading;
	if(pMap==NULL) return;
	
	iLuxEnemy *pEnemy = CreateEnemy(mpEntity->GetName(), mlID,pMap);

	//////////////////////////////
	// Setup mesh entity
	mpEntity->Stop();
	//mpEntity->SetNormalizeAnimationWeights(false);

	//////////////////////////////
	// Set data
	pEnemy->mpWorld = apWorld;
	pEnemy->msFileName = msFileName;
	pEnemy->m_mtxOnLoadTransform = a_mtxTransform;
	pEnemy->mvOnLoadScale = mvScale;

	pEnemy->mpMeshEntity = mpEntity;

	pEnemy->mvLights = mvLights;
	pEnemy->mvParticleSystems = mvParticleSystems;
	pEnemy->mvBillboards = mvBillboards;
	pEnemy->mvBeams = mvBeams;
	
	//////////////////////////////
	// Load base properties
	pEnemy->mfHealth = GetVarFloat("Health", 100);
	pEnemy->mlToughness = GetVarInt("Toughness", 0);

	pEnemy->mfSightRange = GetVarFloat("SightRange", 0);
	pEnemy->mfDarknessSightRange = GetVarFloat("DarknessSightRange", 0);
	pEnemy->mfHearVolume = GetVarFloat("HearVolume", 0);

	pEnemy->mfFOV = cMath::ToRad(GetVarFloat("FOV", 0));
	pEnemy->mfFOVXMul = GetVarFloat("FOVXMul", 1);

	pEnemy->mfMaxRegenHealth = GetVarFloat("MaxRegenHealth", 100);
	pEnemy->mfRegenHealthSpeed = GetVarFloat("RegenHealthSpeed", 0);

	pEnemy->mfActivationDistance = GetVarFloat("ActivationDistance", 0);

	pEnemy->msDangerMusic = GetVarString("DangerMusic", "");
	pEnemy->mlDangerMusicPrio = GetVarInt("DangerMusicPrio", 0);
	
	pEnemy->msMusic[eLuxEnemyMusic_Search] = GetVarString("SearchMusic", "");
	pEnemy->mlMusicPrio[eLuxEnemyMusic_Search] = GetVarInt("SearchMusicPrio", 0);

	pEnemy->msMusic[eLuxEnemyMusic_Attack] = GetVarString("AttackMusic", "");
	pEnemy->mlMusicPrio[eLuxEnemyMusic_Attack] = GetVarInt("AttackMusicPrio", 0);

	pEnemy->mbAutoRemoveAtPathEnd = GetVarBool("AutoRemoveAtPathEnd", true);

	//////////////////////////////
	// AI base properties
	pEnemy->mfPlayerSearchMaxAngle = cMath::ToRad(GetVarFloat("PlayerSearchMaxAngle", 0));
	pEnemy->mfPlayerSearchMinDistMul = GetVarFloat("PlayerSearchMinDistMul", 0);
	pEnemy->mfPlayerSearchMaxDistMul = GetVarFloat("PlayerSearchMaxDistMul", 0);
	pEnemy->mfPlayerSearchTime = GetVarFloat("PlayerSearchTime", 0);

	pEnemy->mfPlayerPatrolMaxAngle = cMath::ToRad(GetVarFloat("PlayerPatrolMaxAngle", 0));
	pEnemy->mfPlayerPatrolMinDist = GetVarFloat("PlayerPatrolMinDist", 0);
	pEnemy->mfPlayerPatrolMaxDist = GetVarFloat("PlayerPatrolMaxDist", 0);


    //////////////////////////////
	// Load character body
	iCharacterBody *pCharBody = apWorld->GetPhysicsWorld()->CreateCharacterBody(msName, GetVarVector3f("Body_Size", 1));
	
	pCharBody->SetMass(					GetVarFloat("Body_Mass", 1));
	pCharBody->SetAccurateClimbing(		GetVarBool("Body_AccurateClimbing",false) );
	pCharBody->SetMaxNoSlideSlopeAngle(cMath::ToRad(GetVarFloat("Body_MaxNoSlideSlopeAngle",0) ) );
	pCharBody->SetMaxPushMass(			GetVarFloat("Body_MaxPushMass",0) );
	pCharBody->SetPushForce(			GetVarFloat("Body_PushForce",0) );
	pCharBody->SetCharacterMaxPushMass(	GetVarFloat("Body_CharacterMaxPushMass",0) );
	pCharBody->SetCharacterPushForce(	GetVarFloat("Body_CharacterPushForce",0) );
	pCharBody->SetMaxStepSize(			GetVarFloat("Body_MaxStepSize",0) );
	pCharBody->SetMaxStepSizeInAir(		GetVarFloat("Body_MaxStepSize",0) );
	pCharBody->SetStepClimbSpeed(		GetVarFloat("Body_StepClimbSpeed",0) );

	pEnemy->m_mtxCharMeshOffset = cMath::MatrixRotate(  cMath::Vector3ToRad(GetVarVector3f("Body_OffsetRot", 0)), eEulerRotationOrder_XYZ);
	pEnemy->m_mtxCharMeshOffset.SetTranslation( GetVarVector3f("Body_OffsetTrans", 0)- cVector3f(0,pCharBody->GetSize().y/2,0) );

	pCharBody->SetEntity(pEnemy->mpMeshEntity);
	pCharBody->SetEntityOffset(pEnemy->m_mtxCharMeshOffset);
	pCharBody->SetUseEntitySmoothYPos(true);
	pCharBody->SetEntitySmoothYPosNum(20);

	pCharBody->SetFeetPosition(mpEntity->GetWorldPosition());

    pCharBody->SetUserData(pEnemy);
	pEnemy->mpCharBody = pCharBody;

	//////////////////////////////
	// Load default movement speeds
	pEnemy->mfDefaultForwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk] =	GetVarFloat("Walk_ForwardSpeed", 0);
	pEnemy->mfDefaultBackwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk] =	GetVarFloat("Walk_BackwardSpeed", 0);
	pEnemy->mfDefaultForwardAcc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk] =		GetVarFloat("Walk_ForwardAcc", 0);
	pEnemy->mfDefaultForwardDeacc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk] =	GetVarFloat("Walk_ForwardDeacc", 0);

	pEnemy->mfDefaultForwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run] =	GetVarFloat("Run_ForwardSpeed", 0);
	pEnemy->mfDefaultBackwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run] =	GetVarFloat("Run_BackwardSpeed", 0);
	pEnemy->mfDefaultForwardAcc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run] =		GetVarFloat("Run_ForwardAcc", 0);
	pEnemy->mfDefaultForwardDeacc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run] =	GetVarFloat("Run_ForwardDeacc", 0);

	pEnemy->mfDefaultForwardSpeed[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Walk] =	0;
	pEnemy->mfDefaultBackwardSpeed[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Walk] =	0;
	pEnemy->mfDefaultForwardAcc[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Walk] =		0;
	pEnemy->mfDefaultForwardDeacc[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Walk] =	0;

	pEnemy->mfDefaultForwardSpeed[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Run] =	0;
	pEnemy->mfDefaultBackwardSpeed[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Run] =	0;
	pEnemy->mfDefaultForwardAcc[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Run] =		0;
	pEnemy->mfDefaultForwardDeacc[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Run] =	0;


	//////////////////////////////
	// Load movement variables
	pEnemy->mfTurnSpeedMul = GetVarFloat("TurnSpeedMul", 1);
	pEnemy->mfTurnMaxSpeed = GetVarFloat("TurnMaxSpeed", 1);
	pEnemy->mfTurnMinBreakAngle = cMath::ToRad(GetVarFloat("TurnMinBreakAngle", 1));
	pEnemy->mfTurnBreakMul = GetVarFloat("TurnBreakMul", 1);

	pEnemy->mfMoveSpeedAnimMul =	GetVarFloat("MoveSpeedAnimMul", 1);

	pEnemy->mfStoppedToWalkSpeed[eLuxEnemyPoseType_Biped] =	GetVarFloat("StoppedToWalkSpeed", 0);
	pEnemy->mfWalkToStoppedSpeed[eLuxEnemyPoseType_Biped] =	GetVarFloat("WalkToStoppedSpeed", 0);
	pEnemy->mfWalkToRunSpeed[eLuxEnemyPoseType_Biped] =		GetVarFloat("WalkToRunSpeed", 0);
	pEnemy->mfRunToWalkSpeed[eLuxEnemyPoseType_Biped] =		GetVarFloat("RunToWalkSpeed", 0);

	pEnemy->mfStoppedToWalkSpeed[eLuxEnemyPoseType_Quadruped] =	0;
	pEnemy->mfWalkToStoppedSpeed[eLuxEnemyPoseType_Quadruped] = 0;
	pEnemy->mfWalkToRunSpeed[eLuxEnemyPoseType_Quadruped] =		0;
	pEnemy->mfRunToWalkSpeed[eLuxEnemyPoseType_Quadruped] =		0;

	pEnemy->mfWaterStepSpeedWalk = GetVarFloat("WaterStepSpeedWalk", 0);
	pEnemy->mfWaterStepSpeedRun = GetVarFloat("WaterStepSpeedRun", 0);
	pEnemy->mfWaterStepSpeedMisc = GetVarFloat("WaterStepSpeedMisc", 0);
	
	//////////////////////////////
	// Load hit variables
	pEnemy->msHitPS[eLuxWeaponHitType_Sword] = GetVarString("HitPS_Sword");
	pEnemy->msHitSound[eLuxWeaponHitType_Sword] = GetVarString("HitSound_Sword");

	pEnemy->msHitPS[eLuxWeaponHitType_Club] = GetVarString("HitPS_Club");
	pEnemy->msHitSound[eLuxWeaponHitType_Club] = GetVarString("HitSound_Club");

	pEnemy->msHitPS[eLuxWeaponHitType_Bullet] = GetVarString("HitPS_Club");
	pEnemy->msHitSound[eLuxWeaponHitType_Bullet] = GetVarString("HitSound_Club");
	
	pEnemy->msHitPS[eLuxWeaponHitType_Dud] = GetVarString("HitPS_Dud");
	pEnemy->msHitSound[eLuxWeaponHitType_Dud] = GetVarString("HitSound_Dud");

	//////////////////////////////
	// Load attack variables
	pEnemy->mfNormalAttackDistance = GetVarFloat("NormalAttackDistance", 0);

	//////////////////////////
	// HARDMODE 
	if (gpBase->mbHardMode)
	{
		pEnemy->mfSightRange *= 1.15f;
		pEnemy->mfDarknessSightRange *= 1.15f;
		pEnemy->mfHearVolume *= 0.6f;

		pEnemy->mfFOV *= 1.1f;
		pEnemy->mfFOVXMul *= 1.1f;

		pEnemy->mfPlayerSearchMaxAngle *= 1.1f;
		pEnemy->mfPlayerSearchMinDistMul *= 1.25f;
		pEnemy->mfPlayerSearchMaxDistMul *= 1.25f;
		
		pEnemy->mfPlayerSearchTime *= 2.5f;

		pEnemy->mfPlayerPatrolMaxAngle	*= 1.25f;
		pEnemy->mfPlayerPatrolMinDist	*= 1.5f;
		pEnemy->mfPlayerPatrolMaxDist	*= 1.5f;

		pEnemy->mfDefaultForwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk]		*= 1.1f;
		pEnemy->mfDefaultBackwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk]	*= 1.1f;
		pEnemy->mfDefaultForwardAcc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk]		*= 1.1f;
		pEnemy->mfDefaultForwardDeacc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk]		*= 1.1f;

		pEnemy->mfDefaultForwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run]		*= 1.2f;
		pEnemy->mfDefaultBackwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run]		*= 1.2f;
		pEnemy->mfDefaultForwardAcc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run]		*= 1.2f;
		pEnemy->mfDefaultForwardDeacc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run]		*= 1.2f;

		pEnemy->mfTurnSpeedMul		*= 1.35f;
		pEnemy->mfTurnMaxSpeed		*= 1.0f;
		pEnemy->mfTurnMinBreakAngle *= 1.0f;
		pEnemy->mfTurnBreakMul		*= 1.0f;

		pEnemy->mfStoppedToWalkSpeed[eLuxEnemyPoseType_Biped]	*= 1.0f;
		pEnemy->mfWalkToStoppedSpeed[eLuxEnemyPoseType_Biped]	*= 1.0f;
		pEnemy->mfWalkToRunSpeed[eLuxEnemyPoseType_Biped]		*= 1.0f;
		pEnemy->mfRunToWalkSpeed[eLuxEnemyPoseType_Biped]		*= 1.0f;


		pEnemy->mfWaterStepSpeedWalk	*= 1.5f;
		pEnemy->mfWaterStepSpeedRun		*= 1.5f;
		pEnemy->mfWaterStepSpeedMisc	*= 1.0f;
	}


	LoadAttackDamageData("Normal", &pEnemy->mNormalAttackDamage);
	LoadAttackDamageData("BreakDoor", &pEnemy->mBreakDoorAttackDamage);
	LoadAttackSizeData("Normal", &pEnemy->mNormalAttackSize, pEnemy, apWorld); 
	
	
	//////////////////////////////
	// Load ambient sounds
	pEnemy->msAmbientSound[eLuxEnemySoundState_Idle] = GetVarString("AmbientSound_Idle");
	pEnemy->mfAmbientSoundMinTime[eLuxEnemySoundState_Idle] = GetVarFloat("AmbientMinTime_Idle");
	pEnemy->mfAmbientSoundMaxTime[eLuxEnemySoundState_Idle] = GetVarFloat("AmbientMaxTime_Idle");

	pEnemy->msAmbientSound[eLuxEnemySoundState_Alert] = GetVarString("AmbientSound_Alert");
	pEnemy->mfAmbientSoundMinTime[eLuxEnemySoundState_Alert] = GetVarFloat("AmbientMinTime_Alert");
	pEnemy->mfAmbientSoundMaxTime[eLuxEnemySoundState_Alert] = GetVarFloat("AmbientMaxTime_Alert");

	pEnemy->msAmbientSound[eLuxEnemySoundState_Hunt] = GetVarString("AmbientSound_Hunt");
	pEnemy->mfAmbientSoundMinTime[eLuxEnemySoundState_Hunt] = GetVarFloat("AmbientMinTime_Hunt");
	pEnemy->mfAmbientSoundMaxTime[eLuxEnemySoundState_Hunt] = GetVarFloat("AmbientMaxTime_Hunt");

	for(size_t i=0; i<eLuxEnemySoundState_LastEnum; ++i)
		gpBase->PreloadSound(pEnemy->msAmbientSound[i]);
	
	//////////////////////////////
	// Load type specific Properties
	LoadVariables(pEnemy, apRootElem);

	//////////////////////////////
	// Extra setup

	pEnemy->SetupAfterLoad(apWorld);

	//Add to the current map
	pMap->AddEntity(pEnemy);

	pEnemy->SetActive(mbActive);

	///////////////////////////////
	// Instance vars
	if(apInstanceVars)
	{
		pEnemy->msCallbackFunc = apInstanceVars->GetVarString("CallbackFunc", "");
		pEnemy->mbDisableTriggers = apInstanceVars->GetVarBool("DisableTriggers", false);
		pEnemy->mbHallucination =  apInstanceVars->GetVarBool("Hallucination", false);
		pEnemy->mfHallucinationEndDist = apInstanceVars->GetVarFloat("HallucinationEndDist", false);
		pEnemy->mbBlind = apInstanceVars->GetVarBool("Blind", false);
		pEnemy->mbDeaf = apInstanceVars->GetVarBool("Deaf", false);

		pEnemy->mCurrentPose = ToPoseType(apInstanceVars->GetVarString("Pose", "biped"));

		LoadInstanceVariables(pEnemy, apInstanceVars);
	}
}

//-----------------------------------------------------------------------

eLuxDamageType iLuxEnemyLoader::ToDamageType(const tString& asType)
{
	tString sLowType = cString::ToLowerCase(asType);

	if(sLowType == "bloodsplat") return eLuxDamageType_BloodSplat;
	if(sLowType == "claws") return eLuxDamageType_Claws;
	if(sLowType == "slash") return eLuxDamageType_Slash;

	Error("Damage type '%s' does not exist!\n", asType.c_str());
	return eLuxDamageType_BloodSplat;
}

//-----------------------------------------------------------------------


void iLuxEnemyLoader::LoadAttackDamageData(const tString &asPrefix, cEnemyAttackDamageData *apData)
{
	cVector2f vDamageMinMax = GetVarVector2f(asPrefix+"AttackDamage", 0);
	cVector2f vForceAndMaxImpulse = GetVarVector2f(asPrefix+"AttackForce", 0);
	apData->msHitSound = GetVarString(asPrefix+"AttackHitSound", "");
	apData->mlStrength = GetVarInt(asPrefix+"AttackStrength", 0);
	apData->mfMinDamage = vDamageMinMax.x;
	apData->mfMaxDamage = vDamageMinMax.y;
	apData->mfForce = vForceAndMaxImpulse.x;
	apData->mfMaxImpulse = vForceAndMaxImpulse.y;
	apData->mDamageType =  ToDamageType(GetVarString(asPrefix+"DamageType","bloodsplat"));
	gpBase->PreloadSound(apData->msHitSound);

	///////////////////////
	// HARDMODE
	if (gpBase->mbHardMode)
	{
		// Double min & max damage
		apData->mfMinDamage *= 2.0f;
		apData->mfMaxDamage *= 2.0f;
	}
}

//-----------------------------------------------------------------------

void iLuxEnemyLoader::LoadAttackSizeData(const tString &asPrefix, cEnemyAttackSizeData *apData, iLuxEnemy *apEnemy, cWorld *apWorld)
{
	cVector3f vSize = GetVarVector3f(asPrefix+"DamageSize", 0);
	apData->mvOffset = GetVarVector3f(asPrefix+"DamageOffset", 0);
	apData->mlShapeIdx = apEnemy->CreateAttackShape(apWorld,vSize);
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxEnemy::iLuxEnemy(const tString &asName, int alID, cLuxMap *apMap, eLuxEnemyType aEnemyType) : iLuxEntity(asName,alID,apMap, eLuxEntityType_Enemy)
{
	mEnemyType = aEnemyType;

	mbUseAnimations = true;

	mfInLanternLightCount =0;

	mbCausesSanityDecrease = true;
	mbCausesSanityDecreaseAsDefault = true;

	mNextState = eLuxEnemyState_Idle;
	mCurrentState = eLuxEnemyState_LastEnum;
	mPreviousState = eLuxEnemyState_LastEnum;
	
	mSoundState = eLuxEnemySoundState_Silent;

	mpCurrentSound = NULL;
	mfAmbientSoundCount = 0;

	mfLookForPlayerCount =0;
	mbCanSeePlayer = false;
	mlPlayerInLOSCount =0;
	mbPlayerDetected = false;
	mbPlayerInRange = false;

	mfCheckAtDoorCount =0;
	mbStuckAtDoor = false;

	mfForwardSpeed = 1;
	mfBackwardSpeed =1;
	mfForwardAcc =1;
	mfForwardDeacc =1;

	mfDarknessGlowAlpha =0;
	mfDarknessGlowUpdateCount =0;

	mfFOVMul = 1.0f;

	mpCurrentAnimation = NULL;

	mbAnimationIsSpeedDependant = false;
	mfAnimationSpeedMul = 1;
	mbUseMoveAnimWhenCurrentIsOver = true;

	mpLastSearchNode = NULL;

	mfDamageCount =0;

	mvLastKnownPlayerPos =0;
	mfLastPlayerPosCount =0;

	mlCurrentPatrolNode = 0;

	mlAttackHitCounter =0;

	mbIsSeenByPlayer = false;

	mbDisabled = false;

	mbDisableTriggers = false;

	mbBlind = false;
	mbDeaf = false;

	mbHallucination = false;
	mfHallucinationEndDist = 3.0f;
	
	mfEnemyDarknessGlowMaxDistance = gpBase->mpGameCfg->GetFloat("Enemy", "EnemyDarknessGlowMaxDistance",0);

	mfPlayerInDarknessLightLevel = 0.35;
	mfCrouchVisibleRangeMul = 0.5f;

	m_mtxCharMeshOffset = cMatrixf::Identity;
	mpGa3Queue = ohpl_ga3_queue_new();
	memset(&mGa3Ctx, 0, sizeof(mGa3Ctx));
	mGa3Ctx.queue = mpGa3Queue;
	mGa3Ctx.cur = reinterpret_cast<int*>(&mCurrentState);
	mGa3Ctx.next = reinterpret_cast<int*>(&mNextState);
	mGa3Ctx.prev = reinterpret_cast<int*>(&mPreviousState);
	mGa3Ctx.disabled = &mbDisabled;
	mGa3Ctx.lookCount = &mfLookForPlayerCount;
	mGa3Ctx.losCount = &mlPlayerInLOSCount;
	mGa3Ctx.canSee = &mbCanSeePlayer;
	mGa3Ctx.detected = &mbPlayerDetected;
	mGa3Ctx.inRange = &mbPlayerInRange;
	mGa3Ctx.checkDoorCount = &mfCheckAtDoorCount;
	mGa3Ctx.stuckAtDoor = &mbStuckAtDoor;
	mGa3Ctx.stuckDoorId = &mlStuckDoorID;
	mGa3Ctx.lastPosCount = &mfLastPlayerPosCount;
	mGa3Ctx.lastKnown = &mvLastKnownPlayerPos.x;
	mGa3Ctx.fovMul = &mfFOVMul;
	mGa3Ctx.sightRange = &mfSightRange;
	mGa3Ctx.health = &mfHealth;
	mGa3Ctx.blind = &mbBlind;
	mGa3Ctx.darknessSightRange = &mfDarknessSightRange;
	mGa3Ctx.inDarknessLightLevel = &mfPlayerInDarknessLightLevel;
	mGa3Ctx.crouchVisibleRangeMul = &mfCrouchVisibleRangeMul;
	mGa3Ctx.skipVisibilityHandicaps = &mbSkipVisibilityRangeHandicaps;
	mGa3Ctx.fov = &mfFOV;
	mGa3Ctx.fovXMul = &mfFOVXMul;
	mGa3Ctx.activationDistance = &mfActivationDistance;
	mGa3Ctx.maxRegenHealth = &mfMaxRegenHealth;
	mGa3Ctx.regenHealthSpeed = &mfRegenHealthSpeed;
	mGa3Ctx.damageCount = &mfDamageCount;
	mGa3Ctx.tempPos = &mvTempPos.x;
	mGa3Ctx.tempVal = &mfTempVal;
	mGa3Ctx.tempValL = &mlTempVal;
	mGa3Ctx.attackHitCounter = &mlAttackHitCounter;
	mGa3Ctx.forwardSpeed = &mfForwardSpeed;
	mGa3Ctx.backwardSpeed = &mfBackwardSpeed;
	mGa3Ctx.forwardAcc = &mfForwardAcc;
	mGa3Ctx.forwardDeacc = &mfForwardDeacc;
	mGa3Ctx.defForwardSpeed = &mfDefaultForwardSpeed;
	mGa3Ctx.defBackwardSpeed = &mfDefaultBackwardSpeed;
	mGa3Ctx.defForwardAcc = &mfDefaultForwardAcc;
	mGa3Ctx.defForwardDeacc = &mfDefaultForwardDeacc;
	mGa3Ctx.pose = reinterpret_cast<int*>(&mCurrentPose);
	mGa3Ctx.startPosition = &mvStartPosition.x;
	mGa3Ctx.hearVolume = &mfHearVolume;
	mGa3Ctx.playerSearchTime = &mfPlayerSearchTime;
	mGa3Ctx.normalAttackDistance = &mfNormalAttackDistance;
	mGa3Ctx.curPatrol = &mlCurrentPatrolNode;
	mGa3Ctx.isSeenByPlayer = &mbIsSeenByPlayer;
	mGa3Ctx.autoRemoveAtPathEnd = &mbAutoRemoveAtPathEnd;

	mfCheckGroundRayCount =0;
	mlMaxGroundCheckDists = 3;
	mbAlignEntityWithGroundRay = false;

	mbInWater = false;
	mfWaterSurfaceY = 0;
	mpWaterSurfaceData = NULL;

	mbSkipVisibilityRangeHandicaps = false;

	mCurrentPose = eLuxEnemyPoseType_Biped;

	for(int i=0; i<eLuxEnemyPoseType_LastEnum; ++i)
	for(int j=0; j<eLuxEnemyMoveType_LastEnum; ++j)
	{
		msBackwardAnimationName[j][i] = "Backward";
		msIdleAnimationName[j][i] = "Idle";
		msWalkAnimationName[j][i] = "Walk";
		msRunAnimationName[j][i] = "Run";
	}
}

//-----------------------------------------------------------------------

iLuxEnemy::~iLuxEnemy()
{
	cWorld *pWorld = mpMap->GetWorld();
	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();
	
	////////////////////
	// Destroy graphics
	{
		//Mesh entity
		if(mpMeshEntity) pWorld->DestroyMeshEntity(mpMeshEntity);

		//Lights
		for(size_t i=0; i<mvLights.size(); ++i) pWorld->DestroyLight(mvLights[i]);
		
		//Particle systems
		for(size_t i=0; i<mvParticleSystems.size(); ++i) 
		{
			cParticleSystem *pPS = mvParticleSystems[i];
			if(pPS && pWorld->ParticleSystemExists(pPS)) pPS->Kill();
		}

		//Billboards
		for(size_t i=0; i<mvBillboards.size(); ++i)	pWorld->DestroyBillboard(mvBillboards[i]);
		
		//Beams
		for(size_t i=0; i<mvBeams.size(); ++i)		pWorld->DestroyBeam(mvBeams[i]);
	}

	////////////////////
	// Destroy attack shapes
	for(size_t i=0; i<mvAttackShapes.size();++i)
	{
		pPhysicsWorld->DestroyShape(mvAttackShapes[i]);
	}

	////////////////////
	// Physics
	if(mpCharBody)
	{
		pPhysicsWorld->DestroyCharacterBody(mpCharBody);
	}

	////////////////////
	// Destroy Helpers
	ohpl_ga3_queue_free(mpGa3Queue);
	hplDelete(mpPathfinder);
	hplDelete(mpMover);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void iLuxEnemy::SetupAfterLoad(cWorld *apWorld)
{
	///////////////////////
	// Create helpers
	mpMover = hplNew(cLuxEnemyMover, (this,mpCharBody));
	mpPathfinder = hplNew(cLuxEnemyPathfinder, (this,mpMover));
	mGa3Ctx.mover = &mpMover->mS;
	mGa3Ctx.pfMoving = &mpPathfinder->mbMoving;
	mGa3Ctx.pfGoal = &mpPathfinder->mvMoveGoalPos.x;
	mGa3Ctx.pfWin = &mpPathfinder->mWin;
	
	///////////////////////
	// Setup implemented 
	mpPathfinder->SetupAfterLoad(apWorld);
	mpMover->SetupAfterLoad(apWorld);

	///////////////////////
	// Setup implemented 
	OnSetupAfterLoad(apWorld);

	///////////////////////
	// Setup default move speeds
	SetMoveSpeed(eLuxEnemyMoveSpeed_Walk);
}

//-----------------------------------------------------------------------

void iLuxEnemy::AfterWorldLoad()
{
	mpPathfinder->AfterWorldLoad();

	OnAfterWorldLoad();
}

//-----------------------------------------------------------------------

void iLuxEnemy::OnMapEnter()
{
	mvStartPosition = mpCharBody->GetFeetPosition() + cVector3f(0,0.1f, 0);
}

//-----------------------------------------------------------------------

void iLuxEnemy::OnUpdate(float afTimeStep)
{
	if(mbDisabled) return;

	//In case the update is called in save or anyhting else that just want a quick update, we do not want to be updated.
	//Enemies can be disabled when player is not properly placed and the like.
	if(afTimeStep < gpBase->mpEngine->GetStepSize()*0.8f)
	{
		return;
	}

	//////////////////////
	// Helpers
	mpPathfinder->OnUpdate(afTimeStep);
	mpMover->OnUpdate(afTimeStep);

	//////////////////////
	// Senses
	if(mfHealth > 0)
	{
		// Sight
		UpdateCanSeePlayer(afTimeStep);
		
		// Detection
		UpdatePlayerDetected(afTimeStep);

		// In Range
		UpdatePlayerInRange(afTimeStep);

		// Is stuck at door check
		UpdateCheckStuckAtDoor(afTimeStep);

		// Check last player position
		UpdateCheckLastPlayerPos(afTimeStep);
	}
	else
	{
		mbCanSeePlayer = false;
		mbPlayerDetected = false;
		mbPlayerInRange = false;
	}
	
	//////////////////////
	// State update
	UpdateStateMachine(afTimeStep);
	
	//////////////////////
	// Specific
	UpdateEnemySpecific(afTimeStep);

	//////////////////////
	// Character body
	UpdateCharBody(afTimeStep);

	//////////////////////
	// Animation
	UpdateAnimation(afTimeStep);

	//////////////////////
	// Sound
	UpdateSoundState(afTimeStep);

	//////////////////////
	// Glow
	UpdateDarknessGlow(afTimeStep);

	//////////////////////
	// Health Regeneration
	UpdateRegenHealth(afTimeStep);

	//////////////////////
	// Hallucination
	UpdateHallucination(afTimeStep);

	//////////////////////
	// Align with ground ray
	UpdateAlignEntityWithGroundRay(afTimeStep);

	//////////////////////
	// Debug
	/*Log("Animations\n------------------\n");
	for(int i=0; i<mpMeshEntity->GetAnimationStateNum(); ++i)
	{
		cAnimationState *pState = mpMeshEntity->GetAnimationState(i);
		if(pState->GetWeight() == 0 || pState->IsActive()==false) continue;
		Log("  %s \tt: %f w:%f\n", pState->GetName().c_str(), pState->GetRelativeTimePosition(), pState->GetWeight());
	}*/
}

//-----------------------------------------------------------------------

void iLuxEnemy::OnRenderSolid(cRendererCallbackFunctions* apFunctions)
{
	//return;
	iPhysicsWorld *pPhysicsWorld = mpMap->GetPhysicsWorld();

	//pPhysicsWorld->RenderShapeDebugGeometry(mpCharBody->GetCurrentShape(), mpCharBody->GetMoveMatrix(), apFunctions->GetLowLevelGfx(),cColor(1,1));

	mpPathfinder->OnRenderSolid(apFunctions);

	apFunctions->GetLowLevelGfx()->DrawSphere(mvLastKnownPlayerPos, 0.3f, cColor(1,0,0));


	////////////////////////////////////////
	// See max range
	cLuxPlayer *pPlayer = gpBase->mpPlayer;

	// Set up variables
	iCharacterBody *pPlayerBody = gpBase->mpPlayer->GetCharacterBody();
	float fDist = cMath::Vector3Dist(pPlayerBody->GetPosition(), mpCharBody->GetPosition());

	// Check if crouching
	bool bCrouching = false;
	if(pPlayer->GetCurrentMoveState() == eLuxMoveState_Normal)
	{
		cLuxMoveState_Normal *pNormalMove = static_cast<cLuxMoveState_Normal*>(pPlayer->GetCurrentMoveStateData());
		if(pNormalMove->IsCrouching())
		{
			bCrouching = true;
		}
	}

	// Edit Range
	float fMaxRange = mfSightRange;

	if(pPlayer->GetHelperLightLevel()->GetNormalLightLevel() < mfPlayerInDarknessLightLevel && pPlayer->GetAvgSpeed() < 0.05f)
		fMaxRange = mfDarknessSightRange;

	if(bCrouching)
		fMaxRange = fMaxRange * mfCrouchVisibleRangeMul;

	apFunctions->GetLowLevelGfx()->DrawSphere(mpCharBody->GetPosition(), fMaxRange, cColor(0,1,1));

	OnRenderSolidImplemented(apFunctions);
}

//-----------------------------------------------------------------------

bool iLuxEnemy::CanInteract(iPhysicsBody *apBody)
{
	return false;
}

bool iLuxEnemy::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return false;
}

//-----------------------------------------------------------------------

void iLuxEnemy::OnConnectionStateChange(iLuxEntity *apEntity, int alState)
{

}

//-----------------------------------------------------------------------

eLuxFocusCrosshair iLuxEnemy::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return eLuxFocusCrosshair_LastEnum;
}

//-----------------------------------------------------------------------

iEntity3D* iLuxEnemy::GetAttachEntity()
{
	return mpMeshEntity;
}

//-----------------------------------------------------------------------

void iLuxEnemy::GiveDamage(float afAmount, int alStrength)
{
	if(mfHealth > 0)

	if(alStrength < mlToughness-1)			afAmount =0;
	else if(alStrength == mlToughness-1)	afAmount *= 0.5;

	afAmount *= GetDamageMul(afAmount, alStrength);
	
	mfHealth -= afAmount;

	gpBase->mpDebugHandler->AddMessage(_W("Enemy damage ") + cString::ToStringW(afAmount), false);

	if(mfHealth <=0)
	{
		ChangeSoundState(eLuxEnemySoundState_Silent);
		ChangeState(eLuxEnemyState_Dead);

		RunCallbackFunc("OnDeath");
	}
	else
	{
		SendMessage(eLuxEnemyMessage_TakeHit,0,false,0,afAmount,alStrength);
		OnDamage(afAmount, alStrength);
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::ShowPlayerPosition()
{
	mvLastKnownPlayerPos = gpBase->mpPlayer->GetCharacterBody()->GetFeetPosition();
}

void iLuxEnemy::AlertOfPlayerPresence()
{
	if(TriggersDisabled()) return;

	ShowPlayerPosition();
	if(	mCurrentState == eLuxEnemyState_Idle ||	mCurrentState == eLuxEnemyState_Patrol)
	{
		ChangeState(eLuxEnemyState_Search);
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::ChangeState(eLuxEnemyState aState)
{
	if(aState == eLuxEnemyState_LastEnum) return;
	if(mCurrentState == aState) return;

	//Log("State %s' -> '%s' LOS: %d\n", cString::To8Char(gsLuxEnemyStates[mCurrentState]).c_str(), cString::To8Char(gsLuxEnemyStates[aState]).c_str(),
	//									mbCanSeePlayer);
	
	mNextState = aState;
}

//-----------------------------------------------------------------------

void iLuxEnemy::SendMessage(eLuxEnemyMessage aType, float afTime, bool abLocalScope, const cVector3f& avX,float afX, int alX)
{
	OhplGa3Host host = Ga3Host();
	ohpl_ga3_send_message(&mGa3Ctx, &host, aType, afTime, abLocalScope, &avX.x, afX, alX);
}

//-----------------------------------------------------------------------

void iLuxEnemy::PlayScriptedAnimation(const tString &asName, bool abLoop)
{
	PlayAnim(asName, abLoop, 0.5f);
}

//-----------------------------------------------------------------------

void iLuxEnemy::PlayAnim(	const tString &asName, bool abLoop, float afFadeTime,
							bool abDependsOnSpeed, float afSpeedMul,
							bool abSyncWithPrevFrame,
							bool abOverideMoveState,
							bool abUseMoveAnimWhenCurrentIsOver)
{
	if(PlayAnimImpl(asName, abLoop, afFadeTime, abDependsOnSpeed, afSpeedMul, abSyncWithPrevFrame, abUseMoveAnimWhenCurrentIsOver))
		mpMover->SetOverideMoveState(abOverideMoveState);
}

//-----------------------------------------------------------------------

bool iLuxEnemy::PlayAnimImpl(	const tString &asName, bool abLoop, float afFadeTime,
								bool abDependsOnSpeed, float afSpeedMul,
								bool abSyncWithPrevFrame,
								bool abUseMoveAnimWhenCurrentIsOver)
{
	//if not using animations, then return.
	if(mbUseAnimations==false) return false;

	//Check if the animation is already playing.
	if(	mpCurrentAnimation != NULL && 
		mpCurrentAnimation->GetName() == asName && 
		mpCurrentAnimation->IsActive() &&
		mpCurrentAnimation->IsOver()== false)
	{
		return false;
	}

	//////////////////////////
	//Get animation
	cAnimationState *pNewAnim = mpMeshEntity->GetAnimationStateFromName(asName);
	if(pNewAnim==NULL)
	{
		Warning("Animation '%s' does not exist!\n",asName.c_str());
		return false;
	}

	//////////////////////////
	//Start animation and fade previous
	pNewAnim->SetActive(true);
	if(mpCurrentAnimation && mpCurrentAnimation != pNewAnim) 
	{
		mpCurrentAnimation->FadeOut(afFadeTime);

		if(pNewAnim->IsFading()==false) pNewAnim->SetWeight(0);
		pNewAnim->FadeIn(afFadeTime);
	}
	else
	{
		pNewAnim->SetWeight(1.0f);
	}
	pNewAnim->SetLoop(abLoop);

	/////////////////////////////////////////
	//Check if this animation should start at the same place as the previous
	if(abSyncWithPrevFrame && mpCurrentAnimation)
	{
		pNewAnim->SetRelativeTimePosition(mpCurrentAnimation->GetRelativeTimePosition());
	}
	else
	{
		pNewAnim->SetTimePosition(0);
	}


	mpCurrentAnimation  = pNewAnim;

	mbAnimationIsSpeedDependant = abDependsOnSpeed;
	mfAnimationSpeedMul = afSpeedMul;
	mbUseMoveAnimWhenCurrentIsOver = abUseMoveAnimWhenCurrentIsOver;

	return true;
}

//-----------------------------------------------------------------------

bool iLuxEnemy::FadeOutCurrentAnimImpl(float afFadeTime)
{
	if(mpCurrentAnimation==NULL) return false;
	if(mpCurrentAnimation->IsLooping()) return false;

	mpCurrentAnimation->FadeOut(afFadeTime);
	mpCurrentAnimation = NULL;

	return mbUseMoveAnimWhenCurrentIsOver;
}

void iLuxEnemy::FadeOutCurrentAnim(float afFadeTime)
{
	if(FadeOutCurrentAnimImpl(afFadeTime)) mpMover->UseMoveStateAnimations();
}

//-----------------------------------------------------------------------

float iLuxEnemy::ConvertAnimToAbsoluteTime(float afRelativeTimePostion)
{
	if(mpCurrentAnimation==NULL) return 0;

    return mpCurrentAnimation->GetLength() * afRelativeTimePostion;	
}

//-----------------------------------------------------------------------

cSoundEntity* iLuxEnemy::PlaySound(const tString &asName)
{
	if(asName=="") return NULL;

	cSoundEntity *pSound = mpMap->GetWorld()->CreateSoundEntity("EnemySound", asName, true);
	if(pSound)
	{
		//pSound->SetPosition(mpCharBody->GetPosition());
		if(mpMeshEntity) mpMeshEntity->AddChild(pSound);
	}

	return pSound;
}

//-----------------------------------------------------------------------

void iLuxEnemy::SetPositionAtStartPos()
{
	mpCharBody->SetFeetPosition(mvStartPosition);
}

//-----------------------------------------------------------------------

void iLuxEnemy::ResetProperties()
{
	SetPositionAtStartPos();
	ChangeState(eLuxEnemyState_Idle);

	mbCausesSanityDecrease = mbCausesSanityDecreaseAsDefault;

	mbDisabled = false;
	mfLookForPlayerCount =0;
	mbCanSeePlayer = false;
	mlPlayerInLOSCount =0;
	mbPlayerDetected = false;
	mbPlayerInRange = false;

	mfCheckAtDoorCount =0;
	mbStuckAtDoor = false;

	mfDarknessGlowAlpha =0;
	mfDarknessGlowUpdateCount =0;

	mfFOVMul = 1.0f;

	mfDamageCount =0;

	mvLastKnownPlayerPos =0;
	mfLastPlayerPosCount =0;

	mlCurrentPatrolNode = 0;

	mlAttackHitCounter =0;

	mbIsSeenByPlayer = false;

	mbSkipVisibilityRangeHandicaps = false;

	SetMoveSpeed(eLuxEnemyMoveSpeed_Walk);
		
	OnResetProperties();
}

//-----------------------------------------------------------------------

void iLuxEnemy::FadeToSmoke(bool abPlaySound)
{
	///////////////////////////
	//Add particles at skeleton bones
	if(mpMeshEntity->GetMesh()->GetSkeleton())
	{
		cWorld *pWorld = mpMap->GetWorld();
		for(int i=0; i<mpMeshEntity->GetBoneStateNum(); ++i)
		{
			cBoneState *pBone = mpMeshEntity->GetBoneState(i);

			//Check "size" of bone
			tString sPSFile = "ps_enemy_disappear_per_bone.ps";
			if(pBone->GetParent())
			{
				float fDist = cMath::Vector3Dist(pBone->GetLocalPosition(), pBone->GetParent()->GetLocalPosition());
				if(fDist < 0.1f) sPSFile = "ps_enemy_disappear_per_bone_small.ps";
			}

			cParticleSystem *pPS = pWorld->CreateParticleSystem("Disappear",sPSFile,0.3f);
			pPS->SetPosition(pBone->GetWorldPosition());
		}
	}

	///////////////////////////
	//Dissolve mesh
	if(mpMeshEntity->IsVisible())
		gpBase->mpMapHandler->GetCurrentMap()->AddDissolveEntity(mpMeshEntity, 1);
	SetActive(false);

	///////////////////////////
	//Play sound
	if(abPlaySound)
		gpBase->mpHelpFuncs->PlayGuiSoundData("enemy_hallucination_disappear", eSoundEntryType_Gui);
}

//-----------------------------------------------------------------------

void iLuxEnemy::AddPatrolNode(cAINode *apNode, float afWaitTime, const tString & asAnimation, bool abLoopAnimation)
{
	cLuxEnemyPatrolNode patrolNode;
	
	patrolNode.mfWaitTime = afWaitTime;
	patrolNode.mpNode = apNode;
	patrolNode.msAnimation = asAnimation;
	patrolNode.mbLoopAnimation = abLoopAnimation;

    mvPatrolNodes.push_back(patrolNode);
}

void iLuxEnemy::ClearPatrolNodes()
{
	mvPatrolNodes.clear();

	mlCurrentPatrolNode =0;
}

//-----------------------------------------------------------------------

cLuxEnemyPatrolNode* iLuxEnemy::GetCurrentPatrolNode()
{
	if(mlCurrentPatrolNode>= (int)mvPatrolNodes.size() || mlCurrentPatrolNode<0)
		return NULL;

	return &mvPatrolNodes[mlCurrentPatrolNode];
}

bool iLuxEnemy::IsAtLastPatrolNode()
{
	return ohpl_ga3_patrol_is_at_last(mlCurrentPatrolNode, (int)mvPatrolNodes.size());
}

void iLuxEnemy::IncCurrentPatrolNode(bool abLoopIfAtEnd)
{
	ohpl_ga3_patrol_inc(&mlCurrentPatrolNode, (int)mvPatrolNodes.size(), abLoopIfAtEnd);
}

//-----------------------------------------------------------------------

void iLuxEnemy::SetDisableTriggers(bool abX)
{
	mbDisableTriggers = abX;

	if(mbDisableTriggers)
	{
		OnDisableTriggers();
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::SetDisabled(bool abX)
{
	mbDisabled = abX;

	if(mbDisabled)
	{
		mpPathfinder->Stop();
	}
}

//-----------------------------------------------------------------------

bool iLuxEnemy::InRangeOfFood(iPhysicsBody *apFoodBody)
{
	//TODO: Some general check here?
	return false;
}

//-----------------------------------------------------------------------

string& iLuxEnemy::GetCurrentEnemyStateName()
{
	msPolledEnemyStateName = cString::To8Char(gsLuxEnemyStates[mCurrentState]);
	return msPolledEnemyStateName;
}

//-----------------------------------------------------------------------

float iLuxEnemy::DrawDebug(cGuiSet *apSet,iFontData *apFont,float afStartY)
{
	if(mbDisabled) return afStartY;

    apSet->DrawFont(apFont, cVector3f(5,afStartY,10),13,cColor(1,1), _W("Name: '%ls'"),cString::To16Char(msName).c_str());
	afStartY += 14;

	apSet->DrawFont(apFont, cVector3f(5,afStartY,10),13,cColor(1,1), 
		_W("  State: '%ls' PlayerSeen: %d PlayerDetected: %d PlayerInRange: %d StuckAtDoor: %d FOVMul: %f"),
		gsLuxEnemyStates[mCurrentState].c_str(), mbCanSeePlayer ? 1 : 0, mbPlayerDetected ? 1 : 0, mbPlayerInRange ? 1 : 0, mbStuckAtDoor ? 1 : 0,
		mfFOVMul);
	afStartY += 14;


	apSet->DrawFont(apFont, cVector3f(5,afStartY,10),13,cColor(1,1), 
		_W("  Health: %f FOVMul: %f DistFromStart: %f StuckCounter: %f Speed: %f SpeedPercent: %f Vel: %f"),
		mfHealth, mfFOVMul, cMath::Vector3Dist(mpCharBody->GetFeetPosition(), mvStartPosition), mpMover->GetStuckCounter(),mpMover->GetMoveSpeed(), mpMover->GetWantedSpeedAmount());
	afStartY += 14;

	apSet->DrawFont(apFont, cVector3f(5,afStartY,10),13,cColor(1,1), 
		_W("  DistToPlayer: %f PlayerDirAmount: %f CurrentPatrolNode: %d MoverSpeedMul: %f"), DistToPlayer(), GetPlayerMovementTowardEnemyAmount(), 
						mlCurrentPatrolNode, mpMover->CalculateSpeedMul(1.0f/60.0f));
	afStartY += 14;

	//apSet->DrawFont(apFont, cVector3f(5,afStartY,10),13,cColor(1,1), 
	//	_W("  Climbing: %d"), mpCharBody->IsClimbing());
	//afStartY += 14;


	return afStartY;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

tString iLuxEnemy::GetCurrentPoseSuffix()
{
	switch(mCurrentPose)
	{
	case eLuxEnemyPoseType_Biped: return "Biped";
	case eLuxEnemyPoseType_Quadruped: return "Quadruped";

	}

	return "";
}

//-----------------------------------------------------------------------

bool iLuxEnemy::StateEvent(int alState, eLuxEnemyStateEvent aEvent, cLuxStateMessage *apMessage)
{
	bool bRet = StateEventImplement(alState, aEvent, apMessage);
	if(bRet==false)
		bRet = StateEventImplement(-1, aEvent, apMessage);
	
	return bRet;
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateStateMachine(float afTimeStep)
{
	OhplGa3Host host = Ga3Host();
	ohpl_ga3_update_state_machine(&mGa3Ctx, &host, afTimeStep, Ga3RustStateMachine() ? NULL : &iLuxEnemy::Ga3StateCb, this);
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

void iLuxEnemy::ChangeSoundState(eLuxEnemySoundState aState)
{
	if(mSoundState == aState) return;

	mSoundState = aState;

    if(mpCurrentSound && mpMap->GetWorld()->SoundEntityExists(mpCurrentSound, mlCurrentSoundID))
	{
		mpCurrentSound->FadeOut(3.0f);
        mpCurrentSound = NULL;
	}

	mfAmbientSoundCount = cMath::RandRectf(mfAmbientSoundMinTime[mSoundState], mfAmbientSoundMaxTime[mSoundState]);
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateSoundState(float afTimeStep)
{
	if(mSoundState == eLuxEnemySoundState_Silent) return;
	if(msAmbientSound[mSoundState] == "") return;

	//If sound is playing still do nothing.
	if(mpCurrentSound && mpMap->GetWorld()->SoundEntityExists(mpCurrentSound,mlCurrentSoundID)) return;
	
	mpCurrentSound = NULL;
	
	
	if(mfAmbientSoundCount <=0)
	{
		mpCurrentSound = PlaySound(msAmbientSound[mSoundState]);
		if(mpCurrentSound )mlCurrentSoundID = mpCurrentSound->GetCreationID();
		mfAmbientSoundCount = cMath::RandRectf(mfAmbientSoundMinTime[mSoundState], mfAmbientSoundMaxTime[mSoundState]);
	}
	else
	{
		mfAmbientSoundCount -= afTimeStep;
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateAnimation(float afTimeStep)
{
	if(mbUseAnimations==false) return;
	if(mpMover->GetOverideMoveState()==false || mpCurrentAnimation==NULL) return;


	//////////////////
	// Check for special event
	if(	mpCurrentAnimation->GetPreviousTimePosition() <= mpCurrentAnimation->GetSpecialEventTime() &&
		mpCurrentAnimation->GetTimePosition() > mpCurrentAnimation->GetSpecialEventTime())
	{
		SendMessage(eLuxEnemyMessage_AnimationSpecialEvent,0,false);
	}

	//////////////////
	// Check if over
	if(mpCurrentAnimation->IsOver())
	{
		if(mbUseMoveAnimWhenCurrentIsOver) mpMover->UseMoveStateAnimations();
		SendMessage(eLuxEnemyMessage_AnimationOver,0,false);
	}

	//////////////////
	// Update speed if needed.
	if(mbAnimationIsSpeedDependant)
	{
		float fSpeed = mpCharBody->GetVelocity(afTimeStep).Length();
		if(mpCharBody->GetMoveSpeed(eCharDir_Forward) <0) fSpeed = -fSpeed;

		mpCurrentAnimation->SetSpeed(std::fabs(fSpeed) * mfMoveSpeedAnimMul);
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateCharBody(float afTimeStep)
{
	OhplGa3Host host = Ga3Host();
	OhplGa3MoverParams params;
	Ga3MoverParams(params);
	ohpl_ga3_update_char_body(&mGa3Ctx, &host, &params);
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateCanSeePlayer(float afTimeStep)
{
	OhplGa3Host host = Ga3Host();
	ohpl_ga3_update_can_see_player(&mGa3Ctx, &host, afTimeStep);
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdatePlayerDetected(float afTimeStep)
{
	OhplGa3Host host = Ga3Host();
	ohpl_ga3_update_player_detected(&mGa3Ctx, &host, &iLuxEnemy::Ga3DetectedCb, this);
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdatePlayerInRange(float afTimeStep)
{
	OhplGa3Host host = Ga3Host();
	ohpl_ga3_update_player_in_range(&mGa3Ctx, &host);
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateCheckStuckAtDoor(float afTimeStep)
{
	OhplGa3Host host = Ga3Host();
	ohpl_ga3_update_check_stuck_at_door(&mGa3Ctx, &host, afTimeStep);
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateCheckLastPlayerPos(float afTimeStep)
{
	OhplGa3Host host = Ga3Host();
	ohpl_ga3_update_check_last_player_pos(&mGa3Ctx, &host, afTimeStep);
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateDarknessGlow(float afTimeStep)
{
	///////////////////////////////
	//Update darkness alpha according to goal
	if(mfDarknessGlowAlpha != mfDarknessGlowAlphaGoal)
	{
		if(mfDarknessGlowAlpha > mfDarknessGlowAlphaGoal)
		{
			mfDarknessGlowAlpha -= afTimeStep;
			if(mfDarknessGlowAlpha < mfDarknessGlowAlphaGoal) mfDarknessGlowAlpha = mfDarknessGlowAlphaGoal;
		}
		else
		{
			mfDarknessGlowAlpha += afTimeStep;
			if(mfDarknessGlowAlpha > mfDarknessGlowAlphaGoal) mfDarknessGlowAlpha = mfDarknessGlowAlphaGoal;
		}
	}

    if(mfDarknessGlowAlpha > 0)
	{
		cCamera *pCam = gpBase->mpPlayer->GetCamera();
		//Check if in frustum
		if(pCam->GetFrustum()->CollideBoundingVolume(mpMeshEntity->GetBoundingVolume()) != eCollision_Outside)
		{
			//Check if in range
			float fDistSqrt = cMath::Vector3DistSqr(pCam->GetPosition(), mpCharBody->GetPosition());
			if(fDistSqrt < mfEnemyDarknessGlowMaxDistance*mfEnemyDarknessGlowMaxDistance)
			{
				float fDist = sqrtf(fDistSqrt);
				
				float fDistMul=1.0f;
				if(fDist > mfEnemyDarknessGlowMaxDistance * 0.5f)
				{
					fDistMul = 1 - ((fDist-mfEnemyDarknessGlowMaxDistance * 0.5f) / (mfEnemyDarknessGlowMaxDistance * 0.5f));
					if(fDistMul<0) fDistMul =0;
				}

				float fSanityMul =  gpBase->mpPlayer->GetSanity()/100.0f;
				fSanityMul = sqrtf(fSanityMul);
				
				float fAlpha = mfDarknessGlowAlpha*mfDarknessGlowAlpha*fDistMul*fSanityMul;

				for(int i=0; i<mpMeshEntity->GetSubMeshEntityNum(); ++i)
				{
					gpBase->mpEffectRenderer->AddEnemyGlow(mpMeshEntity->GetSubMeshEntity(i),fAlpha);
				}
			}
		}
	}


	////////////////////////////////
	//Check if darkness level should be updated
	if(mpMeshEntity->IsVisible()==false) return;
	if(mfDarknessGlowUpdateCount > 0)
	{
		mfDarknessGlowUpdateCount -=afTimeStep;
	}
	mfDarknessGlowUpdateCount = 0.3f;


	////////////////////////////////
	//Update darkness alpha goal
	mfDarknessGlowAlphaGoal = gpBase->mpMapHelper->GetLightLevelAtPos(mpCharBody->GetPosition());
	mfDarknessGlowAlphaGoal += gpBase->mpMapHelper->GetLightLevelAtPos(mpCharBody->GetFeetPosition()+cVector3f(0,0.1f,0));
	mfDarknessGlowAlphaGoal /= 2.0f;
	
	mfDarknessGlowAlphaGoal *= 2;
	if(mfDarknessGlowAlphaGoal>1.0f) mfDarknessGlowAlphaGoal = 1.0f;

	mfDarknessGlowAlphaGoal = 1.0f - mfDarknessGlowAlphaGoal;
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateRegenHealth(float afTimeStep)
{
	OhplGa3Host host = Ga3Host();
	ohpl_ga3_update_regen_health(&mGa3Ctx, &host, afTimeStep);
}

void iLuxEnemy::UpdateHallucination(float afTimeStep)
{
	if(mbHallucination==false || mfHealth <= 0) return;

	float fDistSqr = cMath::Vector3DistSqr(mpCharBody->GetPosition(), gpBase->mpPlayer->GetCharacterBody()->GetPosition());
	if(fDistSqr < mfHallucinationEndDist * mfHallucinationEndDist)
	{
		FadeToSmoke(true);
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateAlignEntityWithGroundRay(float afTimeStep)
{
	if(mbAlignEntityWithGroundRay==false) return;

	mfCheckGroundRayCount -= afTimeStep;
	if(mfCheckGroundRayCount > 0) return;

	mfCheckGroundRayCount = 0.02f;

	///////////////////////////////////
	// Get the distance to ground
	float fStartAdd = 0.05f;
	float fDist=0;
	cVector3f vNormal;

	//Calculate the start postion depending the way that the character faces.
	cVector3f vMoveDir = cMath::MatrixMul(cMath::MatrixRotateY(mpCharBody->GetYaw()),cVector3f(0,0,-1));
	cVector3f vStartPos = mpCharBody->GetFeetPosition()+cVector3f(0,fStartAdd,0) + vMoveDir * mpCharBody->GetSize().x*0.5f;

    bool bIntersect = gpBase->mpMapHelper->GetClosestCharCollider(vStartPos, cVector3f(0,-1,0),0.5f,&fDist,&vNormal,NULL);
    if(bIntersect==false) return;
	fDist -= fStartAdd;

	///////////////////////////////////
	// Add dist to list, pop oldest element if full
	mlstGroundCheckDists.push_back(fDist);
	if((int)mlstGroundCheckDists.size() > mlMaxGroundCheckDists)
		mlstGroundCheckDists.pop_front();

	///////////////////////////////////
	// Calculate avg dist.
	float fFinalDist =0;
	for(tFloatListIt it = mlstGroundCheckDists.begin(); it != mlstGroundCheckDists.end(); ++it)
	{
		fFinalDist += *it;
	}
	fFinalDist /= (float)mlstGroundCheckDists.size();
	
	///////////////////////////////////
	// Set the offset matrix
	cMatrixf mtxOffset = m_mtxCharMeshOffset;
	mtxOffset.SetTranslation(mtxOffset.GetTranslation() + cVector3f(0,-fFinalDist,0));
    mpCharBody->SetEntityOffset(mtxOffset);
}

//-----------------------------------------------------------------------

bool iLuxEnemy::TriggersDisabled()
{
	return mbDisableTriggers || gpBase->mpPlayer->GetHelperFlashback()->IsActive();
}

//-----------------------------------------------------------------------

bool iLuxEnemy::Attack(const cEnemyAttackSizeData &aSizeData, const cEnemyAttackDamageData &aDamageData, float afDamageMul)
{
	bool bHitPlayer = false;
	bool bHit = gpBase->mpMapHelper->ShapeDamage(GetAttackShape(aSizeData.mlShapeIdx), GetDamageShapeMatrix(aSizeData.mvOffset), mpCharBody->GetPosition(),
												aDamageData.mfMinDamage*afDamageMul, aDamageData.mfMaxDamage*afDamageMul, 
												aDamageData.mfForce, aDamageData.mfMaxImpulse, 
												aDamageData.mlStrength, aDamageData.mfHitSpeed,
												aDamageData.mDamageType, aDamageData.mWeaponHitType,
												false, aDamageData.mbCheckPlayer, aDamageData.mbCheckProps, true,
												&bHitPlayer);	
	if(bHit)
	{
		mlAttackHitCounter++;
		PlaySound(aDamageData.msHitSound);
	}

	return bHitPlayer;
}

//-----------------------------------------------------------------------

void iLuxEnemy::SetMoveSpeed(eLuxEnemyMoveSpeed aType)
{
	mfForwardSpeed = mfDefaultForwardSpeed[mCurrentPose][aType];
	mfBackwardSpeed = mfDefaultBackwardSpeed[mCurrentPose][aType];
	mfForwardAcc = mfDefaultForwardAcc[mCurrentPose][aType];
	mfForwardDeacc = mfDefaultForwardDeacc[mCurrentPose][aType];

}

//-----------------------------------------------------------------------

cAINode *iLuxEnemy::GetSearchForPlayerNode(int alMaxIterations, float afMaxAngleMul, float afMinDistMul, float afMaxDistMul)
{
	OhplGa3Host host = Ga3Host();
	const float vParams[4] = {mfPlayerSearchMaxAngle, mfPlayerSearchMinDistMul, mfPlayerSearchMaxDistMul, 0};
	int lIdx = ohpl_ga3_search_for_player_node(&mGa3Ctx, &host, alMaxIterations, afMaxAngleMul, afMinDistMul, afMaxDistMul, &vParams);
	return lIdx < 0 ? NULL : mpPathfinder->mvGa3Nodes[lIdx];
}

//-----------------------------------------------------------------------

cAINode *iLuxEnemy::GetPatrolAroundPlayerNode(int alMaxIterations, float afMaxAngleMul, float afMinDistMul, float afMaxDistMul)
{
	OhplGa3Host host = Ga3Host();
	const float vParams[3] = {mfPlayerPatrolMaxAngle, mfPlayerPatrolMinDist, mfPlayerPatrolMaxDist};
	int lIdx = ohpl_ga3_patrol_around_player_node(&mGa3Ctx, &host, alMaxIterations, afMaxAngleMul, afMinDistMul, afMaxDistMul, &vParams, &mvStartPosition.x);
	return lIdx < 0 ? NULL : mpPathfinder->mvGa3Nodes[lIdx];
}

//-----------------------------------------------------------------------

static const cVector2f gvPosAdds[] = {cVector2f(0,0),
										cVector2f(0.75,0),
										cVector2f(-0.75,0),
										cVector2f(0,1),
										cVector2f(0,-1),
										cVector2f(0.5,0.5),
										cVector2f(-0.5,0.5),
										cVector2f(0.25,0),
										cVector2f(-0.25,0)
};

bool iLuxEnemy::LineOfSight(const cVector3f &avPos, const cVector3f &avSize, bool abCheckFOV)
{
	OhplGa3Host host = Ga3Host();
	return ohpl_ga3_line_of_sight(&mGa3Ctx, &host, &avPos.x, &avSize.x, abCheckFOV, NULL);
}

bool iLuxEnemy::LineOfSight(const cVector3f &avPos, const cVector3f &avSize, bool abCheckFOV, const cVector3f& avSourcePos)
{
	OhplGa3Host host = Ga3Host();
	return ohpl_ga3_line_of_sight(&mGa3Ctx, &host, &avPos.x, &avSize.x, abCheckFOV, &avSourcePos.x);
}

//-----------------------------------------------------------------------

int iLuxEnemy::CreateAttackShape(cWorld *apWorld, cVector3f &avSize, eCollideShapeType aType)
{
	iPhysicsWorld *pPhysicsWorld = apWorld->GetPhysicsWorld();
	iCollideShape *pShape = NULL;
	
	switch(aType)
	{
	case eCollideShapeType_Box:
		pShape = pPhysicsWorld->CreateBoxShape(avSize, NULL);
		break;
	case eCollideShapeType_Sphere:
		pShape = pPhysicsWorld->CreateSphereShape(avSize, NULL);
		break;
	case eCollideShapeType_Cylinder:
		pShape = pPhysicsWorld->CreateCylinderShape(avSize.x, avSize.y, NULL);
		break;
	case eCollideShapeType_Capsule:
		pShape = pPhysicsWorld->CreateCapsuleShape(avSize.x, avSize.y, NULL);
		break;
	}

	if(pShape)
	{
		mvAttackShapes.push_back(pShape);
		return (int)mvAttackShapes.size() -1;
	}
	return -1;
}

//-----------------------------------------------------------------------

cMatrixf iLuxEnemy::GetDamageShapeMatrix(const cVector3f& avOffset)
{
	cVector3f vPos =	mpCharBody->GetPosition() +
						mpCharBody->GetRight() * avOffset.x +
						mpCharBody->GetUp() * avOffset.y +
						mpCharBody->GetForward() * avOffset.z;
		

	cVector3f vRot = cVector3f(0,mpCharBody->GetYaw(),0);
	cMatrixf mtxOffset = cMath::MatrixRotate(vRot,eEulerRotationOrder_XYZ);
	mtxOffset.SetTranslation(vPos);

	return mtxOffset;
}

//-----------------------------------------------------------------------

cLuxProp_Object* iLuxEnemy::GetClosestFood(float afMaxDist, float afMaxHeightDist)
{
	cLuxProp_Object *pOutputFood=NULL;
	float fShortestDist=-1;

	cLuxEntityIterator it = mpMap->GetEntityIterator();
	while(it.HasNext())
	{
		///////////////////////////////////////////
		// Check so type is correct
		iLuxEntity *pEntity = it.Next();
        if(pEntity->GetEntityType() != eLuxEntityType_Prop) continue;
		
		iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
		if(pProp->GetPropType() != eLuxPropType_Object) continue;

		cLuxProp_Object *pObject = static_cast<cLuxProp_Object*>(pProp);
		if(pObject->IsFood()==false) continue;

		///////////////////////////////////////////
		// Check if height is correct
       	float fFoodLowestY = pObject->GetBody(0)->GetBoundingVolume()->GetMin().y;

		if(cMath::Abs(fFoodLowestY - mpCharBody->GetFeetPosition().y) > afMaxHeightDist) continue;


		///////////////////////////////////////////
		// Check distance
		cVector3f vFoodPos = pObject->GetBody(0)->GetLocalPosition();

		float fDistSqr = cMath::Vector3DistSqr(mpCharBody->GetPosition(), vFoodPos);
		if(fDistSqr > afMaxDist*afMaxDist) continue;

		if(fShortestDist <0 || fDistSqr < fShortestDist)
		{
			fShortestDist = fDistSqr;
			pOutputFood = pObject;
		}
	}

	return pOutputFood;
}

//-----------------------------------------------------------------------

cVector3f iLuxEnemy::GetPlayerFeetPos()
{
	return gpBase->mpPlayer->GetCharacterBody()->GetFeetPosition();
}

//-----------------------------------------------------------------------

float iLuxEnemy::Dist2D(const cVector3f &avPos)
{
	cVector3f vStart = mpCharBody->GetFeetPosition();
	cVector3f vEnd = avPos;
	vStart.y =0;
	vEnd.y =0;

	return cMath::Vector3Dist(vStart,vEnd);
}

float iLuxEnemy::DistToChar(iCharacterBody *apBody)
{
	return cMath::Vector3Dist(mpCharBody->GetFeetPosition(), apBody->GetFeetPosition());
}

float iLuxEnemy::DistToChar2D(iCharacterBody *apBody)
{
	cVector3f vStart = mpCharBody->GetFeetPosition();
	cVector3f vEnd = apBody->GetFeetPosition();
	vStart.y =0;
	vEnd.y =0;

	return cMath::Vector3Dist(vStart,vEnd);
}

float iLuxEnemy::AbsHeightDistToChar(iCharacterBody *apBody)
{
	cVector3f vStart = mpCharBody->GetFeetPosition();
	cVector3f vEnd = apBody->GetFeetPosition();

	return cMath::Abs(vStart.y - vEnd.y);
}

cVector3f iLuxEnemy::GetDirection2D(const cVector3f &avPos)
{
	cVector3f vDiff = avPos - mpCharBody->GetPosition();
	vDiff.y=0; vDiff.Normalize();
	return vDiff;
}

float iLuxEnemy::DistToPlayer()
{
	OhplGa3Host host = Ga3Host();
	return ohpl_ga3_dist_to_player(&mGa3Ctx, &host);
}

float iLuxEnemy::DistToPlayer2D()
{
	OhplGa3Host host = Ga3Host();
	return ohpl_ga3_dist_to_player_2d(&mGa3Ctx, &host);
}

float iLuxEnemy::DistToPlayer2D(const cVector3f& avPos)
{
	return cMath::Vector2DistXZ(gpBase->mpPlayer->GetCharacterBody()->GetPosition(), avPos);
}

float iLuxEnemy::AbsHeightDistToPlayer()
{
	if(gpBase->mpPlayer->IsDead()) return 100000.0f;

	return AbsHeightDistToChar(gpBase->mpPlayer->GetCharacterBody());
}

cVector3f iLuxEnemy::GetDirection2DToPlayer()
{
	return GetDirection2D(gpBase->mpPlayer->GetCharacterBody()->GetPosition());
}

//-----------------------------------------------------------------------

bool iLuxEnemy::IsSeenByPlayer()
{
	///////////////////////////////
	// Check frustum
	cFrustum *pFrustum = gpBase->mpPlayer->GetCamera()->GetFrustum();
	if(pFrustum->CollideBoundingVolume(mpCharBody->GetCurrentBody()->GetBoundingVolume())==eCollision_Outside)
	{
		return false;
	}

	///////////////////////////////
	// Line of sight with rays
	cVector3f vPlayerEyePos = gpBase->mpPlayer->GetCamera()->GetPosition();

	return LineOfSight(mpCharBody->GetPosition(), mpCharBody->GetSize(), false,vPlayerEyePos);
}

//-----------------------------------------------------------------------

bool iLuxEnemy::IsInPlayerFovAtFeetPos(const cVector3f& avFeetPos)
{
	cBoundingVolume bv;
	bv = *mpCharBody->GetCurrentBody()->GetBoundingVolume();
	bv.SetPosition(avFeetPos + cVector3f(0, mpCharBody->GetSize().y/2, 0) );

	cFrustum *pFrustum = gpBase->mpPlayer->GetCamera()->GetFrustum();
	return pFrustum->CollideBoundingVolume(&bv)!=eCollision_Outside;
}

//-----------------------------------------------------------------------

bool iLuxEnemy::IsVisibleToPlayerAtFeetPos(const cVector3f& avFeetPos)
{
	cVector3f vPlayerEyePos = gpBase->mpPlayer->GetCamera()->GetPosition();
	cVector3f vCharPos = avFeetPos;
	vCharPos.y += mpCharBody->GetSize().y/2;

	return LineOfSight(vCharPos, mpCharBody->GetSize(), false, vPlayerEyePos);
}

//-----------------------------------------------------------------------

float iLuxEnemy::GetPlayerMovementTowardEnemyAmount()
{
	OhplGa3Host host = Ga3Host();
	return ohpl_ga3_player_movement_toward_enemy(&mGa3Ctx, &host);
}

//-----------------------------------------------------------------------

bool iLuxEnemy::OutsideStartRadius()
{
	float fDist = cMath::Vector3Dist(mpCharBody->GetFeetPosition(), mvStartPosition);

	return fDist > mfActivationDistance * 1.2f + 2.0f;
}

//-----------------------------------------------------------------------

bool iLuxEnemy::InFOV(const cVector3f &avPos)
{
	OhplGa3Host host = Ga3Host();
	return ohpl_ga3_in_fov(&mGa3Ctx, &host, &avPos.x);
}

bool iLuxEnemy::PlayerInFOV()
{
	OhplGa3Host host = Ga3Host();
	return ohpl_ga3_player_in_fov(&mGa3Ctx, &host);
}

//-----------------------------------------------------------------------

void iLuxEnemy::OnSetActive(bool abX)
{	
	if(mpCharBody)
	{
		mpCharBody->SetActive(abX);
	}

	///////////////////
	//Mesh entity
	if(mpMeshEntity)
	{
		mpMeshEntity->SetActive(abX);
		mpMeshEntity->SetVisible(abX);
	}

	//Remove from attack and search music
	if(abX==false)
	{
		gpBase->mpMusicHandler->RemoveEnemy(eLuxEnemyMusic_Attack,this);
		gpBase->mpMusicHandler->RemoveEnemy(eLuxEnemyMusic_Search,this);
		gpBase->mpPlayer->RemoveTerrorEnemy(this);

		ChangeState(eLuxEnemyState_Idle);
	}
	
	OnSetActiveEnemySpecific(abX);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------
kBeginSerializeBase(cLuxStateMessage_SaveData)

kSerializeVar(mlType, eSerializeType_Int32)
kSerializeVar(mfTime, eSerializeType_Float32)
kSerializeVar(mfCount, eSerializeType_Float32)

kSerializeVar(mvCustomValue, eSerializeType_Vector3f)
kSerializeVar(mfCustomValue, eSerializeType_Float32)
kSerializeVar(mlCustomValue, eSerializeType_Int32)

kSerializeVar(mlState, eSerializeType_Int32)

kEndSerialize()

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxEnemyPatrolNode_SaveData)

kSerializeVar(mlNodeId, eSerializeType_Int32)
kSerializeVar(mfWaitTime, eSerializeType_Float32)
kSerializeVar(msAnimation, eSerializeType_String)
kSerializeVar(mbLoopAnimation, eSerializeType_Bool)

kEndSerialize()

//-----------------------------------------------------------------------

kBeginSerializeVirtual(iLuxEnemy_SaveData, iLuxEntity_SaveData)

kSerializeVar(msFileName, eSerializeType_String)
kSerializeVar(m_mtxOnLoadTransform, eSerializeType_Matrixf)
kSerializeVar(mvOnLoadScale, eSerializeType_Vector3f)

kSerializeVar(mbDisabled, eSerializeType_Bool)
kSerializeVar(mbDisableTriggers, eSerializeType_Bool)
kSerializeVar(mbBlind, eSerializeType_Bool)
kSerializeVar(mbDeaf, eSerializeType_Bool)

kSerializeVar(mfHealth, eSerializeType_Float32)
kSerializeVar(mbCausesSanityDecrease, eSerializeType_Bool)

kSerializeVar(mbHallucination, eSerializeType_Bool)

kSerializeVar(mlCurrentState, eSerializeType_Int32)
kSerializeVar(mlNextState, eSerializeType_Int32)
kSerializeVar(mlPreviousState, eSerializeType_Int32)

kSerializeVar(mlSoundState, eSerializeType_Int32)

kSerializeVar(mfLookForPlayerCount, eSerializeType_Float32)
kSerializeVar(mlPlayerInLOSCount, eSerializeType_Int32)
kSerializeVar(mbCanSeePlayer, eSerializeType_Bool)
kSerializeVar(mbPlayerDetected, eSerializeType_Bool)
kSerializeVar(mbPlayerInRange, eSerializeType_Bool)

kSerializeVar(mfCheckAtDoorCount, eSerializeType_Float32)
kSerializeVar(mbStuckAtDoor, eSerializeType_Bool)
kSerializeVar(mlStuckDoorID, eSerializeType_Int32)

kSerializeVar(mfForwardSpeed, eSerializeType_Float32)
kSerializeVar(mfBackwardSpeed, eSerializeType_Float32)
kSerializeVar(mfForwardAcc, eSerializeType_Float32)
kSerializeVar(mfForwardDeacc, eSerializeType_Float32)

kSerializeVar(mfDarknessGlowAlpha, eSerializeType_Float32)
kSerializeVar(mfDarknessGlowAlphaGoal, eSerializeType_Float32)

kSerializeVar(mlAttackHitCounter, eSerializeType_Int32)

kSerializeVar(mfFOVMul, eSerializeType_Float32)

kSerializeVar(msCurrentAnimName, eSerializeType_String)
kSerializeVar(mbAnimationIsSpeedDependant, eSerializeType_Bool)
kSerializeVar(mfAnimationSpeedMul, eSerializeType_Float32)
kSerializeVar(mbUseMoveAnimWhenCurrentIsOver, eSerializeType_Bool)

kSerializeVar(mvStartPosition, eSerializeType_Vector3f)

kSerializeVar(mvLastKnownPlayerPos, eSerializeType_Vector3f)
kSerializeVar(mfLastPlayerPosCount, eSerializeType_Float32)

kSerializeVar(mvTempPos, eSerializeType_Vector3f)
kSerializeVar(mfTempVal, eSerializeType_Float32)
kSerializeVar(mlTempVal, eSerializeType_Int32)
kSerializeVar(mfDamageCount, eSerializeType_Float32)

kSerializeClassContainer(mvPatrolNodes, cLuxEnemyPatrolNode_SaveData, eSerializeType_Class)
kSerializeVar(mlCurrentPatrolNode, eSerializeType_Int32)

kSerializeVar(mMeshEntity, eSerializeType_Class)
kSerializeClassContainer(mvPS, cEnginePS_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvBeams, cEngineBeam_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvLights, cEngineLight_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvBillboards, cEngineBillboard_SaveData, eSerializeType_Class)

kSerializeClassContainer(mvMessages, cLuxStateMessage_SaveData, eSerializeType_Class)

kSerializeVar(mMover, eSerializeType_Class)
kSerializeVar(mPathfinder, eSerializeType_Class)

kSerializeVar(mCharBody, eSerializeType_Class)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxEnemy_SaveData::~iLuxEnemy_SaveData()
{
}

//-----------------------------------------------------------------------

cEnginePS_SaveData* iLuxEnemy_SaveData::GetParticleSystem(cParticleSystem* apPS)
{
	for(size_t i=0; i<mvPS.Size();++i)
		if(mvPS[i].msName == apPS->GetName()) return &mvPS[i];

	return NULL;
}

//-----------------------------------------------------------------------

iLuxEntity* iLuxEnemy_SaveData::CreateEntity(cLuxMap *apMap)
{
	cWorld *pWorld = apMap->GetWorld();
	apMap->ResetLatestEntity();
	pWorld->CreateEntity(msName,m_mtxOnLoadTransform, msFileName, mlID, true, mvOnLoadScale);

	return apMap->GetLatestEntity();
}

//-----------------------------------------------------------------------

void iLuxEnemy::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::SaveToSaveData(apSaveData);
	iLuxEnemy_SaveData *pData = static_cast<iLuxEnemy_SaveData*>(apSaveData);

    cWorld *pWorld = mpMap->GetWorld();
	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

	///////////////////////
	//Properties
	kCopyToVar(pData, msFileName);
	kCopyToVar(pData, m_mtxOnLoadTransform);
	kCopyToVar(pData, mvOnLoadScale);

	kCopyToVar(pData, mbDisabled);
	kCopyToVar(pData, mbDisableTriggers);
	kCopyToVar(pData, mbBlind);
	kCopyToVar(pData, mbDeaf);

	kCopyToVar(pData, mfHealth);
	kCopyToVar(pData, mbCausesSanityDecrease);

	kCopyToVar(pData, mbHallucination);
	
	pData->mlCurrentState = mCurrentState;
	pData->mlNextState = mNextState;
	pData->mlPreviousState = mPreviousState;

	pData->mlSoundState = mSoundState;

	kCopyToVar(pData, mfLookForPlayerCount);
	kCopyToVar(pData, mlPlayerInLOSCount);
	kCopyToVar(pData, mbCanSeePlayer);
	kCopyToVar(pData, mbPlayerDetected);
	kCopyToVar(pData, mbPlayerInRange);

	kCopyToVar(pData, mfCheckAtDoorCount);
	kCopyToVar(pData, mbStuckAtDoor);
	kCopyToVar(pData, mlStuckDoorID);

	kCopyToVar(pData, mfForwardSpeed);
	kCopyToVar(pData, mfBackwardSpeed);
	kCopyToVar(pData, mfForwardAcc);
	kCopyToVar(pData, mfForwardDeacc);

	kCopyToVar(pData, mfDarknessGlowAlpha);
	kCopyToVar(pData, mfDarknessGlowAlphaGoal);

	kCopyToVar(pData, mlAttackHitCounter);

	kCopyToVar(pData,mfFOVMul);

	kCopyToVar(pData, mbAnimationIsSpeedDependant);
	kCopyToVar(pData, mfAnimationSpeedMul);
	kCopyToVar(pData, mbUseMoveAnimWhenCurrentIsOver);

	kCopyToVar(pData, mvStartPosition);

	kCopyToVar(pData, mvLastKnownPlayerPos);
	kCopyToVar(pData, mfLastPlayerPosCount);

	kCopyToVar(pData, mvTempPos);
	kCopyToVar(pData, mfTempVal);
	kCopyToVar(pData, mlTempVal);
	kCopyToVar(pData, mfDamageCount);

	kCopyToVar(pData, mlCurrentPatrolNode);

	///////////////////////
	//Patrol nodes
	pData->mvPatrolNodes.Resize(mvPatrolNodes.size());
	for(size_t i=0; i<mvPatrolNodes.size(); ++i)
	{
		pData->mvPatrolNodes[i].mlNodeId = 	mvPatrolNodes[i].mpNode->GetID();
		pData->mvPatrolNodes[i].mfWaitTime = 	mvPatrolNodes[i].mfWaitTime;
		pData->mvPatrolNodes[i].msAnimation = 	mvPatrolNodes[i].msAnimation;
		pData->mvPatrolNodes[i].mbLoopAnimation = mvPatrolNodes[i].mbLoopAnimation;
	}
	
	///////////////////////
	//Current animation
	if(mpCurrentAnimation)
		pData->msCurrentAnimName = mpCurrentAnimation->GetName();
	else
		pData->msCurrentAnimName = "";

	///////////////////////
	//Mesh Entity
	if(mpMeshEntity)
	{
		pData->mMeshEntity.FromMeshEntity(mpMeshEntity);
	}

	
	///////////////////////
	//Particle Systems
	pData->mvPS.Resize(mvParticleSystems.size());
	for(size_t i=0; i<mvParticleSystems.size(); ++i)
	{
		if(pWorld->ParticleSystemExists(mvParticleSystems[i])==false)
		{
			mvParticleSystems[i] = NULL;
			Warning("particle system %d in %s does not exist anymore!\n",i,GetName().c_str());
		}

		pData->mvPS[i].FromPS(mvParticleSystems[i]);
	}
	
	///////////////////////
	//Lights
	pData->mvLights.Resize(mvLights.size());
	for(size_t i=0; i<mvLights.size(); ++i)
	{
		pData->mvLights[i].FromLight(mvLights[i]);
	}
	
	///////////////////////
	//Billboards
	pData->mvBillboards.Resize(mvBillboards.size());
	for(size_t i=0; i<mvBillboards.size(); ++i)
	{
		pData->mvBillboards[i].FromBillboard(mvBillboards[i]);
	}

	///////////////////////
	//Messages
	for(size_t i=0; i<ohpl_ga3_queue_len(mpGa3Queue); ++i)
	{
		OhplGa3Msg message;
		ohpl_ga3_queue_get(mpGa3Queue, i, &message);
		cLuxStateMessage_SaveData savedMessage;

		savedMessage.mfCount = message.count;
		savedMessage.mfTime = message.time;
		savedMessage.mvCustomValue = cVector3f(message.customV[0], message.customV[1], message.customV[2]);
		savedMessage.mlState = message.state;
		savedMessage.mlType = message.ty;

		pData->mvMessages.Add(savedMessage);
	}

	///////////////////////
	//Helpers
	pData->mMover.FromMover(mpMover);
	pData->mPathfinder.FromPathfinder(mpPathfinder);

	///////////////////////
	//Character body
	if(mpCharBody)
	{
		pData->mCharBody.FromBody(mpCharBody);
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveData(apSaveData);
	iLuxEnemy_SaveData *pData = static_cast<iLuxEnemy_SaveData*>(apSaveData);

	cWorld *pWorld = mpMap->GetWorld();
	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

	///////////////////////
	//Properties
	kCopyFromVar(pData, m_mtxOnLoadTransform);
	kCopyFromVar(pData, mvOnLoadScale);

	kCopyFromVar(pData, mbDisabled);
	kCopyFromVar(pData, mbDisableTriggers);
	kCopyFromVar(pData, mbBlind);
	kCopyFromVar(pData, mbDeaf);

	kCopyFromVar(pData, mfHealth);
	kCopyFromVar(pData, mbCausesSanityDecrease);

	kCopyFromVar(pData, mbHallucination);

	
	mCurrentState = (eLuxEnemyState)pData->mlCurrentState;
	mNextState = (eLuxEnemyState)pData->mlNextState;
	mPreviousState = (eLuxEnemyState)pData->mlPreviousState;

	ChangeSoundState((eLuxEnemySoundState)pData->mlSoundState);

	kCopyFromVar(pData, mfLookForPlayerCount);
	kCopyFromVar(pData, mlPlayerInLOSCount);
	kCopyFromVar(pData, mbCanSeePlayer);
	kCopyFromVar(pData, mbPlayerDetected);
	kCopyFromVar(pData, mbPlayerInRange);

	kCopyFromVar(pData, mfCheckAtDoorCount);
	kCopyFromVar(pData, mbStuckAtDoor);
	kCopyFromVar(pData, mlStuckDoorID);

	kCopyFromVar(pData, mfForwardSpeed);
	kCopyFromVar(pData, mfBackwardSpeed);
	kCopyFromVar(pData, mfForwardAcc);
	kCopyFromVar(pData, mfForwardDeacc);

	kCopyFromVar(pData, mfDarknessGlowAlpha);
	kCopyFromVar(pData, mfDarknessGlowAlphaGoal);

	kCopyFromVar(pData, mlAttackHitCounter);

	kCopyFromVar(pData,mfFOVMul);

	kCopyFromVar(pData, mbAnimationIsSpeedDependant);
	kCopyFromVar(pData, mfAnimationSpeedMul);
	kCopyFromVar(pData, mbUseMoveAnimWhenCurrentIsOver);

	kCopyFromVar(pData, mvStartPosition);

	kCopyFromVar(pData, mvLastKnownPlayerPos);
	kCopyFromVar(pData, mfLastPlayerPosCount);

	kCopyFromVar(pData, mvTempPos);
	kCopyFromVar(pData, mfTempVal);
	kCopyFromVar(pData, mlTempVal);
	kCopyFromVar(pData, mfDamageCount);

	kCopyFromVar(pData, mlCurrentPatrolNode);

	///////////////////////
	//Current animation
	if(pData->msCurrentAnimName != "")
	{
		mpCurrentAnimation = mpMeshEntity->GetAnimationStateFromName(pData->msCurrentAnimName);
	}
	else
	{
		mpCurrentAnimation = NULL;
	}
	
	///////////////////////
	//Mesh Entity
	if(mpMeshEntity)
	{
		pData->mMeshEntity.ToMeshEntity(mpMeshEntity);
	}

	///////////////////////
	//Lights
	for(size_t i=0; i<mvLights.size(); ++i)
	{
		pData->mvLights[i].ToLight(mvLights[i]);
	}
	
	///////////////////////
	//Particle Systems
	int lCount=0;
	for(std::vector<cParticleSystem*>::iterator it = mvParticleSystems.begin(); it != mvParticleSystems.end();)
	{
		cParticleSystem *pPS = *it;

		cEnginePS_SaveData *pSavePS = pData->GetParticleSystem(pPS);
		if(pSavePS)
		{
			pSavePS->ToPS(pPS);
			++it;
		}
		else
		{
			//check if a null was previously saved
			if(pData->mvPS.Size() == mvParticleSystems.size() && pData->mvPS[lCount].msType == "")
			{
				++it;
			}
			//a particle system has been removed.
			else
			{
				pWorld->DestroyParticleSystem(pPS);
				it = mvParticleSystems.erase(it);
			}
		}

		++lCount;
	}

	///////////////////////
	//Billboards
	for(size_t i=0; i<mvBillboards.size(); ++i)
	{
		pData->mvBillboards[i].ToBillboard(mvBillboards[i]);
	}

	///////////////////////
	//Messages
	for(size_t i=0; i<pData->mvMessages.Size(); ++i)
	{
		OhplGa3Msg message;
		memset(&message, 0, sizeof(message));
		cLuxStateMessage_SaveData& savedMessage = pData->mvMessages[i];

		message.count =			savedMessage.mfCount;
		message.time =			savedMessage.mfTime;
		message.customV[0] =	savedMessage.mvCustomValue.x;
		message.customV[1] =	savedMessage.mvCustomValue.y;
		message.customV[2] =	savedMessage.mvCustomValue.z;
		message.state =			savedMessage.mlState;
		message.ty =			savedMessage.mlType;

		ohpl_ga3_queue_push_raw(mpGa3Queue, &message);
	}

	///////////////////////
	//Helpers
	pData->mMover.ToMover(mpMover);
	pData->mPathfinder.ToPathfinder(mpPathfinder);

	///////////////////////
	//Character body
	if(mpCharBody)
	{
		pData->mCharBody.ToBody(mpCharBody);
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
	iLuxEnemy_SaveData *pData = static_cast<iLuxEnemy_SaveData*>(apSaveData);

	cAINodeContainer *pNodeContainer =  mpPathfinder->GetNodeContainer();

	///////////////////////
	//Patrol nodes
	if(pNodeContainer)
	{
		mvPatrolNodes.reserve(pData->mvPatrolNodes.Size());
		for(size_t i=0; i<pData->mvPatrolNodes.Size(); ++i)
		{
			cLuxEnemyPatrolNode patrolNode;

			patrolNode.mpNode =pNodeContainer->GetNodeFromID(pData->mvPatrolNodes[i].mlNodeId);
			if(patrolNode.mpNode==NULL)
			{
				Error("Could not find patrol node id: %d for '%s'!\n", pData->mvPatrolNodes[i].mlNodeId, msName.c_str());
				continue;
			}

			patrolNode.mfWaitTime = pData->mvPatrolNodes[i].mfWaitTime;
			patrolNode.msAnimation = pData->mvPatrolNodes[i].msAnimation;
			patrolNode.mbLoopAnimation = pData->mvPatrolNodes[i].mbLoopAnimation;

			mvPatrolNodes.push_back(patrolNode);
		}
	}

	///////////////////////
	//Helpers
	pData->mPathfinder.SetupPathfinder(mpPathfinder);
}

//-----------------------------------------------------------------------


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// RUST ENEMY CORE HOST
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

OhplGa3Host iLuxEnemy::Ga3Host()
{
	OhplGa3Host host;
	host.ctx = this;
	host.call = &iLuxEnemy::Ga3Call;
	return host;
}

void iLuxEnemy::Ga3MoverParams(OhplGa3MoverParams &aParams)
{
	aParams.turnMinBreakAngle = mfTurnMinBreakAngle;
	aParams.turnBreakMul = mfTurnBreakMul;
	aParams.turnSpeedMul = mfTurnSpeedMul;
	aParams.turnMaxSpeed = mfTurnMaxSpeed;
	aParams.moveSpeedAnimMul = mfMoveSpeedAnimMul;
	aParams.pose = mCurrentPose;
	for(int i=0; i<2; ++i)
	{
		aParams.stoppedToWalk[i] = mfStoppedToWalkSpeed[i];
		aParams.walkToStopped[i] = mfWalkToStoppedSpeed[i];
		aParams.walkToRun[i] = mfWalkToRunSpeed[i];
		aParams.runToWalk[i] = mfRunToWalkSpeed[i];
	}
}

bool iLuxEnemy::Ga3StateCb(void *apEnemy, int alState, int alEvent, const OhplGa3Msg *apMessage)
{
	return static_cast<iLuxEnemy*>(apEnemy)->StateEventImplement(alState, (eLuxEnemyStateEvent)alEvent, (cLuxStateMessage*)apMessage);
}

bool iLuxEnemy::Ga3DetectedCb(void *apEnemy)
{
	return static_cast<iLuxEnemy*>(apEnemy)->PlayerIsDetected();
}

static void Ga3Put(float *apDest, const cVector3f &avV)
{
	apDest[0] = avV.x; apDest[1] = avV.y; apDest[2] = avV.z;
}

static cVector3f Ga3Get(const float *apSrc)
{
	return cVector3f(apSrc[0], apSrc[1], apSrc[2]);
}

int iLuxEnemy::Ga3Call(void *apEnemy, unsigned alOp, OhplGa3Io *apIo)
{
	iLuxEnemy *pEnemy = static_cast<iLuxEnemy*>(apEnemy);
	iCharacterBody *pBody = pEnemy->mpCharBody;
	cLuxPlayer *pPlayer = gpBase->mpPlayer;
	iCharacterBody *pPlayerBody = pPlayer->GetCharacterBody();

	switch(alOp)
	{
	case eOhplGa3Op_StepSize:		apIo->f[0] = gpBase->mpEngine->GetStepSize(); return 0;
	case eOhplGa3Op_BodyPos:		Ga3Put(apIo->v[0], pBody->GetPosition()); return 0;
	case eOhplGa3Op_BodyLastPos:	Ga3Put(apIo->v[0], pBody->GetLastPosition()); return 0;
	case eOhplGa3Op_BodyFeet:		Ga3Put(apIo->v[0], pBody->GetFeetPosition()); return 0;
	case eOhplGa3Op_BodySize:		Ga3Put(apIo->v[0], pBody->GetSize()); return 0;
	case eOhplGa3Op_BodyForward:	Ga3Put(apIo->v[0], pBody->GetForward()); return 0;
	case eOhplGa3Op_BodyYaw:		apIo->f[0] = pBody->GetYaw(); return 0;
	case eOhplGa3Op_BodyMoveSpeedFwd:	apIo->f[0] = pBody->GetMoveSpeed(eCharDir_Forward); return 0;
	case eOhplGa3Op_BodyMoveDelay:	apIo->f[0] = pBody->GetMoveDelay(); return 0;
	case eOhplGa3Op_BodyVelocity:	Ga3Put(apIo->v[0], pBody->GetVelocity(apIo->f[0])); return 0;
	case eOhplGa3Op_BodyMoveFwd:	pBody->Move(eCharDir_Forward, 1.0f); return 0;
	case eOhplGa3Op_BodyAddYaw:		pBody->AddYaw(apIo->f[0]); return 0;
	case eOhplGa3Op_BodySetSpeeds:
		pBody->SetMaxPositiveMoveSpeed(eCharDir_Forward, apIo->f[0]);
		pBody->SetMaxNegativeMoveSpeed(eCharDir_Forward, apIo->f[1]);
		pBody->SetMoveAcc(eCharDir_Forward, apIo->f[2]);
		pBody->SetMoveDeacc(eCharDir_Forward, apIo->f[3]);
		return 0;
	case eOhplGa3Op_BodyBvRadius:	apIo->f[0] = pBody->GetCurrentBody()->GetBoundingVolume()->GetRadius(); return 0;
	case eOhplGa3Op_BodySetActive:	pBody->SetActive(apIo->i[0] != 0); return 0;
	case eOhplGa3Op_PlayAnim:
	{
		int lN = apIo->i[1];
		tString sName;
		switch(apIo->i[0])
		{
		case eOhplGa3Anim_Backward:		sName = pEnemy->GetBackwardAnimationName(); break;
		case eOhplGa3Anim_Idle:			sName = pEnemy->GetIdleAnimationName(); break;
		case eOhplGa3Anim_Walk:			sName = pEnemy->GetWalkAnimationName(); break;
		case eOhplGa3Anim_Run:			sName = pEnemy->GetRunAnimationName(); break;
		case eOhplGa3Anim_IdleExtra:	sName = "IdleExtra"+cString::ToString(lN); break;
		case eOhplGa3Anim_Notice:		sName = "Notice"+cString::ToString(lN); break;
		case eOhplGa3Anim_BreakDoor:	sName = "BreakDoor"; break;
		case eOhplGa3Anim_SwingClaws:	sName = "SwingClaws0"+cString::ToString(lN); break;
		case eOhplGa3Anim_SwingLaunch:	sName = "SwingLaunch"; break;
		case eOhplGa3Anim_Flinch:		sName = "Flinch"; break;
		case eOhplGa3Anim_Dead:			sName = "Dead"; break;
		}
		return pEnemy->PlayAnimImpl(sName, apIo->i[2] != 0, apIo->f[0], apIo->i[3] != 0, apIo->f[1], apIo->i[4] != 0, apIo->i[6] != 0) ? 1 : 0;
	}
	case eOhplGa3Op_AnimExists:		return pEnemy->mpCurrentAnimation ? 1 : 0;
	case eOhplGa3Op_AnimSetSpeed:	pEnemy->mpCurrentAnimation->SetSpeed(apIo->f[0]); return 0;
	case eOhplGa3Op_FadeOutAnim:	return pEnemy->FadeOutCurrentAnimImpl(apIo->f[0]) ? 1 : 0;

	case eOhplGa3Op_PlayerDead:		return pPlayer->IsDead() ? 1 : 0;
	case eOhplGa3Op_PlayerFeet:		Ga3Put(apIo->v[0], pPlayerBody->GetFeetPosition()); return 0;
	case eOhplGa3Op_PlayerPos:		Ga3Put(apIo->v[0], pPlayerBody->GetPosition()); return 0;
	case eOhplGa3Op_PlayerSize:		Ga3Put(apIo->v[0], pPlayerBody->GetSize()); return 0;
	case eOhplGa3Op_PlayerCamPos:	Ga3Put(apIo->v[0], pPlayer->GetCamera()->GetPosition()); return 0;
	case eOhplGa3Op_PlayerBvRadius:	apIo->f[0] = pPlayerBody->GetCurrentBody()->GetBoundingVolume()->GetRadius(); return 0;
	case eOhplGa3Op_PlayerLantern:	return pPlayer->GetHelperLantern()->IsActive() ? 1 : 0;
	case eOhplGa3Op_PlayerCrouch:
		if(pPlayer->GetCurrentMoveState() == eLuxMoveState_Normal)
		{
			cLuxMoveState_Normal *pNormalMove = static_cast<cLuxMoveState_Normal*>(pPlayer->GetCurrentMoveStateData());
			if(pNormalMove->IsCrouching()) return 1;
		}
		return 0;
	case eOhplGa3Op_PlayerLightLevel:	apIo->f[0] = pPlayer->GetHelperLightLevel()->GetNormalLightLevel(); return 0;
	case eOhplGa3Op_PlayerAvgSpeed:		apIo->f[0] = pPlayer->GetAvgSpeed(); return 0;
	case eOhplGa3Op_PlayerVelocity:		Ga3Put(apIo->v[0], pPlayerBody->GetVelocity(apIo->f[0])); return 0;
	case eOhplGa3Op_PlayerTerror:		apIo->f[0] = pPlayer->GetTerror(); return 0;
	case eOhplGa3Op_PlayerSetTerror:	pPlayer->SetTerror(apIo->f[0]); return 0;
	case eOhplGa3Op_PlayerAddTerrorEnemy:		pPlayer->AddTerrorEnemy(pEnemy); return 0;
	case eOhplGa3Op_PlayerRemoveTerrorEnemy:	pPlayer->RemoveTerrorEnemy(pEnemy); return 0;
	case eOhplGa3Op_TriggersDisabled:	return pEnemy->TriggersDisabled() ? 1 : 0;
	case eOhplGa3Op_Music:
		if(apIo->i[0]) gpBase->mpMusicHandler->AddEnemy((eLuxEnemyMusic)apIo->i[1], pEnemy);
		else			gpBase->mpMusicHandler->RemoveEnemy((eLuxEnemyMusic)apIo->i[1], pEnemy);
		return 0;
	case eOhplGa3Op_LosRay:
		return gpBase->mpMapHelper->CheckLineOfSight(Ga3Get(apIo->v[0]), Ga3Get(apIo->v[1]), false) ? 1 : 0;
	case eOhplGa3Op_EntityAttachPos:
	{
		iLuxEntity *pEnt = pEnemy->mpMap->GetEntityByID(apIo->i[0]);
		if(pEnt==NULL){ Ga3Put(apIo->v[0], cVector3f(0)); return 0;}
		Ga3Put(apIo->v[0], pEnt->GetAttachEntity()->GetWorldPosition());
		return 0;
	}
	case eOhplGa3Op_DoorBroken:		return pEnemy->mpMap->DoorIsBroken(apIo->i[0]) ? 1 : 0;
	case eOhplGa3Op_DebugMsg:
		switch(apIo->i[0])
		{
		case 1:		gpBase->mpDebugHandler->AddMessage(_W("Enemy ") + cString::To16Char(pEnemy->msName)+_W(" enabled!"), false ); break;
		case 2:		gpBase->mpDebugHandler->AddMessage(_W("Sound Heard! Vol: ")+cString::ToStringW(apIo->f[0]),false); break;
		case 3:		gpBase->mpDebugHandler->AddMessage(_W("Could not find node near sound!"),false); break;
		case 100:	gpBase->mpProgressLogHandler->AddLog(eLuxProgressLogLevel_Low, "Enemy '"+pEnemy->msName+"'  is in range"); break;
		}
		return 0;
	case eOhplGa3Op_PlaySound:
	{
		const tString *psName = pEnemy->Ga3SoundName(apIo->i[0]);
		if(psName) pEnemy->PlaySound(*psName);
		return 0;
	}
	case eOhplGa3Op_ChangeSoundState:	pEnemy->ChangeSoundState((eLuxEnemySoundState)apIo->i[0]); return 0;
	case eOhplGa3Op_ActivateSoundAllowed:	return gpBase->mpGlobalDataHandler->GetEnemyActivateSoundAllowed() ? 1 : 0;
	case eOhplGa3Op_SetActivateSoundMade:	gpBase->mpGlobalDataHandler->SetEnemyActivateSoundMade(); return 0;

	case eOhplGa3Op_PfMoveTo:		return pEnemy->mpPathfinder->MoveTo(Ga3Get(apIo->v[0])) ? 1 : 0;
	case eOhplGa3Op_PfStop:			pEnemy->mpPathfinder->Stop(); return 0;
	case eOhplGa3Op_PfPathLen:		return (int)pEnemy->mpPathfinder->mlstPathNodes.size();
	case eOhplGa3Op_PfPathBack:		Ga3Put(apIo->v[0], pEnemy->mpPathfinder->mlstPathNodes.back()->GetPosition()); return 0;
	case eOhplGa3Op_PfPathPop:		pEnemy->mpPathfinder->mlstPathNodes.pop_back(); return 0;
	case eOhplGa3Op_PfPathClear:	pEnemy->mpPathfinder->mlstPathNodes.clear(); return 0;

	case eOhplGa3Op_NcPresent:		return pEnemy->mpPathfinder->mpNodeContainer ? 1 : 0;
	case eOhplGa3Op_NcCount:		return pEnemy->mpPathfinder->mpNodeContainer->GetNodeNum();
	case eOhplGa3Op_NcIterBegin:
	{
		cLuxEnemyPathfinder *pPf = pEnemy->mpPathfinder;
		pPf->mlGa3SkipMode = apIo->i[0];
		pPf->mvGa3Nodes.clear();
		cAINodeIterator it = pPf->mpNodeContainer->GetNodeIterator(Ga3Get(apIo->v[0]), apIo->f[0]);
		while(it.HasNext()) pPf->mvGa3Nodes.push_back(it.Next());
		return (int)pPf->mvGa3Nodes.size();
	}
	case eOhplGa3Op_NcIterNode:
	{
		cLuxEnemyPathfinder *pPf = pEnemy->mpPathfinder;
		cAINode *pNode = pPf->mvGa3Nodes[apIo->i[0]];
		cAINode *pSkip = NULL;
		if(pPf->mlGa3SkipMode == 1)			pSkip = pEnemy->mpLastSearchNode;
		else if(pPf->mlGa3SkipMode == 2)	pSkip = pPf->mpGa3ExplicitSkip;
		if(pNode == pSkip) return 0;
		if(apIo->i[1] && pEnemy->mpMap->AINodeIsUsedAsGoal(pNode)) return 0;
		Ga3Put(apIo->v[0], pNode->GetPosition());
		return 1;
	}
	case eOhplGa3Op_NcFreePath:
		return pEnemy->mpPathfinder->mpNodeContainer->FreePath(Ga3Get(apIo->v[0]), Ga3Get(apIo->v[1]), apIo->i[0], apIo->i[1]) ? 1 : 0;
	case eOhplGa3Op_NcCommitLast:	pEnemy->mpLastSearchNode = pEnemy->mpPathfinder->mvGa3Nodes[apIo->i[0]]; return 0;
	case eOhplGa3Op_BvContains:
	{
		cBoundingVolume tempBV;
		tempBV.SetSize(Ga3Get(apIo->v[1]));
		tempBV.SetPosition(Ga3Get(apIo->v[2]));
		return cMath::CheckPointInBVIntersection(Ga3Get(apIo->v[0]), tempBV) ? 1 : 0;
	}
	case eOhplGa3Op_DoorCount:
	{
		pEnemy->mvGa3Doors.clear();
		cBoundingVolume *pBV = pBody->GetCurrentBody()->GetBoundingVolume();
		cLuxEntityIterator entIt = pEnemy->mpMap->GetEntityIterator();
		while(entIt.HasNext())
		{
			iLuxEntity *pEntity = entIt.Next();
			if(pEntity->GetEntityType() != eLuxEntityType_Prop) continue;
			if(pEntity->GetDestroyMe()) continue;
			iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
			if(pProp->GetPropType() != eLuxPropType_SwingDoor) continue;
			for(int i=0; i<pProp->GetBodyNum(); ++i)
			{
				iPhysicsBody *pDoorBody = pProp->GetBody(i);
				if(pDoorBody->GetMass()==0) continue;
				if(cMath::CheckBVIntersection(*pBV, *pDoorBody->GetBoundingVolume())==false) continue;
				pEnemy->mvGa3Doors.push_back(std::make_pair(pProp, pDoorBody));
			}
		}
		return (int)pEnemy->mvGa3Doors.size();
	}
	case eOhplGa3Op_DoorAt:
	{
		iLuxProp *pProp = pEnemy->mvGa3Doors[apIo->i[0]].first;
		iPhysicsBody *pDoorBody = pEnemy->mvGa3Doors[apIo->i[0]].second;
		Ga3Put(apIo->v[0], pDoorBody->GetLocalPosition());
		Ga3Put(apIo->v[1], pProp->GetMainBody()->GetWorldPosition());
		return pProp->GetID();
	}
	case eOhplGa3Op_EnemyStateFatal:	FatalError("Tried calling undefined state %d!\n", apIo->i[0]); return 0;
	case eOhplGa3Op_PatrolCount:		return (int)pEnemy->mvPatrolNodes.size();
	case eOhplGa3Op_PatrolNodePos:
	{
		cLuxEnemyPatrolNode *pNode = pEnemy->Ga3PatrolNode(apIo->i[0]);
		if(pNode==NULL) return 0;
		Ga3Put(apIo->v[0], pNode->mpNode->GetPosition());
		return 1;
	}
	case eOhplGa3Op_PatrolWait:
	{
		cLuxEnemyPatrolNode *pNode = pEnemy->Ga3PatrolNode(apIo->i[0]);
		if(pNode==NULL) return 0;
		apIo->f[0] = pNode->mfWaitTime;
		return 1;
	}
	case eOhplGa3Op_SetActiveFalse:		pEnemy->SetActive(false); return 0;
	case eOhplGa3Op_RunCallback:		pEnemy->RunCallbackFunc("OnAutoDisabled"); return 0;
	case eOhplGa3Op_BroadcastHelp:
		pEnemy->mpMap->BroadcastEnemyMessage(eLuxEnemyMessage_HelpMe, true, Ga3Get(apIo->v[0]), apIo->f[0], 0, false, Ga3Get(apIo->v[1]));
		return 0;
	case eOhplGa3Op_Attack:
		if(apIo->i[0] == 0)	pEnemy->Attack(pEnemy->mNormalAttackSize, pEnemy->mNormalAttackDamage);
		else				pEnemy->Attack(pEnemy->mNormalAttackSize, pEnemy->mBreakDoorAttackDamage);
		return 0;
	}
	return 0;
}
