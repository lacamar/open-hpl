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

static eLuxDamageType ToDamageType(const tString& asType)
{
	tString sLowType = cString::ToLowerCase(asType);

	if(sLowType == "bloodsplat") return eLuxDamageType_BloodSplat;
	if(sLowType == "claws") return eLuxDamageType_Claws;
	if(sLowType == "slash") return eLuxDamageType_Slash;

	Error("Damage type '%s' does not exist!\n", asType.c_str());
	return eLuxDamageType_BloodSplat;
}

//-----------------------------------------------------------------------


static void ApplyAttack(const OhplAeAttack &aSrc, const tString &asType, cEnemyAttackDamageData *apData)
{
	apData->mlStrength = aSrc.strength;
	apData->mfMinDamage = aSrc.minDamage;
	apData->mfMaxDamage = aSrc.maxDamage;
	apData->mfForce = aSrc.force;
	apData->mfMaxImpulse = aSrc.maxImpulse;
	apData->mDamageType = ToDamageType(asType);
	gpBase->PreloadSound(apData->msHitSound);
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
	tString sNormalType, sBreakDoorType;
	tString *vpStr[18] = {&pEnemy->msDangerMusic, &pEnemy->msMusic[eLuxEnemyMusic_Search], &pEnemy->msMusic[eLuxEnemyMusic_Attack],
		&pEnemy->msHitPS[0], &pEnemy->msHitPS[1], &pEnemy->msHitPS[2], &pEnemy->msHitPS[3],
		&pEnemy->msHitSound[0], &pEnemy->msHitSound[1], &pEnemy->msHitSound[2], &pEnemy->msHitSound[3],
		&pEnemy->msAmbientSound[eLuxEnemySoundState_Idle], &pEnemy->msAmbientSound[eLuxEnemySoundState_Alert], &pEnemy->msAmbientSound[eLuxEnemySoundState_Hunt],
		&pEnemy->mNormalAttackDamage.msHitSound, &sNormalType, &pEnemy->mBreakDoorAttackDamage.msHitSound, &sBreakDoorType};
	OhplAeCfg &cfg = pEnemy->mCfg;
	ohpl_ae_load_base(mpVarsRs, gpBase->mbHardMode, &cfg,
		[](void *c, int alSlot, const unsigned char *s, size_t n){ static_cast<tString**>(c)[alSlot]->assign((const char*)s, n); }, vpStr);
	pEnemy->mlToughness = cfg.toughness;
	pEnemy->mA4.moveSpeedAnimMul = cfg.moveSpeedAnimMul;
	for(int i=eLuxEnemySoundState_Idle; i<eLuxEnemySoundState_LastEnum; ++i)
	{
		pEnemy->mfAmbientSoundMinTime[i] = cfg.ambientMin[i];
		pEnemy->mfAmbientSoundMaxTime[i] = cfg.ambientMax[i];
	}

	//////////////////////////////
	// Load character body
	const OhplAeBody &body = cfg.body;
	iCharacterBody *pCharBody = apWorld->GetPhysicsWorld()->CreateCharacterBody(msName, cVector3f(body.size[0], body.size[1], body.size[2]));

	pCharBody->SetMass(body.mass);
	pCharBody->SetAccurateClimbing(body.accurateClimbing);
	pCharBody->SetMaxNoSlideSlopeAngle(body.maxNoSlideSlope);
	pCharBody->SetMaxPushMass(body.maxPushMass);
	pCharBody->SetPushForce(body.pushForce);
	pCharBody->SetCharacterMaxPushMass(body.charMaxPushMass);
	pCharBody->SetCharacterPushForce(body.charPushForce);
	pCharBody->SetMaxStepSize(body.maxStepSize);
	pCharBody->SetMaxStepSizeInAir(body.maxStepSize);
	pCharBody->SetStepClimbSpeed(body.stepClimbSpeed);

	pEnemy->m_mtxCharMeshOffset = cMath::MatrixRotate(cVector3f(body.offsetRot[0], body.offsetRot[1], body.offsetRot[2]), eEulerRotationOrder_XYZ);
	pEnemy->m_mtxCharMeshOffset.SetTranslation(cVector3f(body.offsetTrans[0], body.offsetTrans[1], body.offsetTrans[2]) - cVector3f(0,pCharBody->GetSize().y/2,0));

	pCharBody->SetEntity(pEnemy->mpMeshEntity);
	pCharBody->SetEntityOffset(pEnemy->m_mtxCharMeshOffset);
	pCharBody->SetUseEntitySmoothYPos(true);
	pCharBody->SetEntitySmoothYPosNum(20);

	pCharBody->SetFeetPosition(mpEntity->GetWorldPosition());

	pCharBody->SetUserData(pEnemy);
	pEnemy->mpCharBody = pCharBody;

	//////////////////////////////
	// Attack data
	ApplyAttack(cfg.normal, sNormalType, &pEnemy->mNormalAttackDamage);
	ApplyAttack(cfg.breakDoor, sBreakDoorType, &pEnemy->mBreakDoorAttackDamage);
	pEnemy->mNormalAttackSize.mvOffset = cVector3f(cfg.normal.offset[0], cfg.normal.offset[1], cfg.normal.offset[2]);
	cVector3f vAttackSize(cfg.normal.size[0], cfg.normal.size[1], cfg.normal.size[2]);
	pEnemy->mNormalAttackSize.mlShapeIdx = pEnemy->CreateAttackShape(apWorld, vAttackSize);

	//////////////////////////////
	// Ambient sounds
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
		pEnemy->SetCallbackFunc(apInstanceVars->GetVarString("CallbackFunc", ""));
		pEnemy->mbDisableTriggers = apInstanceVars->GetVarBool("DisableTriggers", false);
		pEnemy->mA4.hallucination =  apInstanceVars->GetVarBool("Hallucination", false);
		pEnemy->mA4.hallucinationEndDist = apInstanceVars->GetVarFloat("HallucinationEndDist", false);
		pEnemy->mbBlind = apInstanceVars->GetVarBool("Blind", false);
		pEnemy->mbDeaf = apInstanceVars->GetVarBool("Deaf", false);

		pEnemy->mCurrentPose = ToPoseType(apInstanceVars->GetVarString("Pose", "biped"));

		LoadInstanceVariables(pEnemy, apInstanceVars);
	}
}

//-----------------------------------------------------------------------

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
	
	mA4.soundState = eLuxEnemySoundState_Silent;

	mA4.curSound = NULL;
	mA4.ambientCount = 0;

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

	mA4.glowAlpha =0;
	mA4.glowUpdateCount =0;

	mfFOVMul = 1.0f;

	mA4.curAnim = NULL;

	mA4.animSpeedDependant = false;
	mA4.animSpeedMul = 1;
	mA4.useMoveAnimWhenOver = true;

	mNav = 0;
	mpNavDraw = NULL;

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

	mA4.hallucination = false;
	mA4.hallucinationEndDist = 3.0f;
	
	mA4.glowMaxDist = gpBase->mpGameCfg->GetFloat("Enemy", "EnemyDarknessGlowMaxDistance",0);

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
	ohpl_a4_init(&mA4, &mGa3Ctx, (int)aEnemyType);

	mA4.groundRayCount =0;
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
	cWorld *pWorld = GetMap()->GetWorld();
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
	if(uintptr_t lAStar = ohpl_a4b_nav_free(mNav))
	{
		pWorld->DestroyAStarHandler((cAStarHandler*)lAStar);
	}
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
	mNav = ohpl_a4b_nav_new();
	ohpl_a4b_nav_fill_ctx(mNav, &mGa3Ctx);

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
	cWorld *pWorld = GetMap()->GetWorld();
	tString sType = cString::SetFileExt(cString::GetFileName(msFileName),"");
	cAINodeContainer *pNodeContainer = pWorld->CreateAINodeContainer( sType, "Default", mpCharBody->GetSize(), false, 2 , 6 , 5.0f,0.41f);
	if(pNodeContainer==NULL)
		Error("No node container found for enemy '%s'\n", GetName().c_str());
	ohpl_a4b_nav_set_nodes(mNav, (uintptr_t)pNodeContainer, pNodeContainer ? (uintptr_t)pWorld->CreateAStarHandler(pNodeContainer) : 0);

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
	OhplA4Host h = A4Host();
	ohpl_a4_on_update(&mA4,&h,afTimeStep);
	A4bDump();
}

//-----------------------------------------------------------------------

void iLuxEnemy::OnRenderSolid(cRendererCallbackFunctions* apFunctions)
{
	//return;
	iPhysicsWorld *pPhysicsWorld = GetMap()->GetPhysicsWorld();

	//pPhysicsWorld->RenderShapeDebugGeometry(mpCharBody->GetCurrentShape(), mpCharBody->GetMoveMatrix(), apFunctions->GetLowLevelGfx(),cColor(1,1));

	OhplGa3Host host = Ga3Host();
	mpNavDraw = apFunctions;
	cVector3f vBodyPos = mpCharBody->GetPosition();
	ohpl_a4b_nav_render(mNav, &host, &vBodyPos.x);

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
	OhplA4Host h = A4Host();
	ohpl_a4_give_damage(&mA4,&h,afAmount,alStrength);
}

//-----------------------------------------------------------------------

void iLuxEnemy::ShowPlayerPosition()
{
	OhplA4Host h = A4Host();
	ohpl_a4_show_player_position(&mA4,&h);
}

void iLuxEnemy::AlertOfPlayerPresence()
{
	OhplA4Host h = A4Host();
	ohpl_a4_alert_of_player_presence(&mA4,&h);
}

//-----------------------------------------------------------------------

void iLuxEnemy::ChangeState(eLuxEnemyState aState)
{
	ohpl_a4_change_state(&mA4,(int)aState);
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
	OhplA4Host h = A4Host();
	ohpl_a4_play_anim(&mA4,&h,asName.c_str(),abLoop,afFadeTime,abDependsOnSpeed,afSpeedMul,abSyncWithPrevFrame,abOverideMoveState,abUseMoveAnimWhenCurrentIsOver);
}

//-----------------------------------------------------------------------

bool iLuxEnemy::PlayAnimImpl(	const tString &asName, bool abLoop, float afFadeTime,
								bool abDependsOnSpeed, float afSpeedMul,
								bool abSyncWithPrevFrame,
								bool abUseMoveAnimWhenCurrentIsOver)
{
	OhplA4Host h = A4Host();
	return ohpl_a4_play_anim_impl(&mA4,&h,asName.c_str(),abLoop,afFadeTime,abDependsOnSpeed,afSpeedMul,abSyncWithPrevFrame,abUseMoveAnimWhenCurrentIsOver);
}

//-----------------------------------------------------------------------

bool iLuxEnemy::FadeOutCurrentAnimImpl(float afFadeTime)
{
	OhplA4Host h = A4Host();
	return ohpl_a4_fade_out_current_anim_impl(&mA4,&h,afFadeTime);
}

void iLuxEnemy::FadeOutCurrentAnim(float afFadeTime)
{
	OhplA4Host h = A4Host();
	ohpl_a4_fade_out_current_anim(&mA4,&h,afFadeTime);
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

cSoundEntity* iLuxEnemy::PlaySound(const tString &asName)
{
	if(asName=="") return NULL;

	cSoundEntity *pSound = GetMap()->GetWorld()->CreateSoundEntity("EnemySound", asName, true);
	if(pSound)
	{
		//pSound->SetPosition(mpCharBody->GetPosition());
		if(mpMeshEntity) mpMeshEntity->AddChild(pSound);
	}

	return pSound;
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

void iLuxEnemy::ResetProperties()
{
	OhplA4Host h = A4Host();
	ohpl_a4_reset_properties(&mA4,&h);
}

//-----------------------------------------------------------------------

void iLuxEnemy::FadeToSmoke(bool abPlaySound)
{
	///////////////////////////
	//Add particles at skeleton bones
	if(mpMeshEntity->GetMesh()->GetSkeleton())
	{
		cWorld *pWorld = GetMap()->GetWorld();
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
		ohpl_a4b_nav_stop(mNav);
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

	OhplGa3Host host = Ga3Host();
	OhplGa3MoverParams params;
	Ga3MoverParams(params);

    apSet->DrawFont(apFont, cVector3f(5,afStartY,10),13,cColor(1,1), _W("Name: '%ls'"),cString::To16Char(GetName()).c_str());
	afStartY += 14;

	apSet->DrawFont(apFont, cVector3f(5,afStartY,10),13,cColor(1,1), 
		_W("  State: '%ls' PlayerSeen: %d PlayerDetected: %d PlayerInRange: %d StuckAtDoor: %d FOVMul: %f"),
		gsLuxEnemyStates[mCurrentState].c_str(), mbCanSeePlayer ? 1 : 0, mbPlayerDetected ? 1 : 0, mbPlayerInRange ? 1 : 0, mbStuckAtDoor ? 1 : 0,
		mfFOVMul);
	afStartY += 14;


	apSet->DrawFont(apFont, cVector3f(5,afStartY,10),13,cColor(1,1), 
		_W("  Health: %f FOVMul: %f DistFromStart: %f StuckCounter: %f Speed: %f SpeedPercent: %f Vel: %f"),
		mfHealth, mfFOVMul, cMath::Vector3Dist(mpCharBody->GetFeetPosition(), mvStartPosition), ohpl_a4b_mover_stuck(mNav), ohpl_ga3_mover_move_speed(&host), ohpl_ga3_mover_wanted_speed_amount(&host));
	afStartY += 14;

	apSet->DrawFont(apFont, cVector3f(5,afStartY,10),13,cColor(1,1), 
		_W("  DistToPlayer: %f PlayerDirAmount: %f CurrentPatrolNode: %d MoverSpeedMul: %f"), DistToPlayer(), GetPlayerMovementTowardEnemyAmount(), 
						mlCurrentPatrolNode, ohpl_ga3_mover_speed_mul(mNav, &params, &host));
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
	OhplA4Host h = A4Host();
	ohpl_a4_change_sound_state(&mA4,&h,(int)aState);
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateAnimation(float afTimeStep)
{
	OhplA4Host h = A4Host();
	ohpl_a4_update_animation(&mA4,&h,afTimeStep);
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

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateRegenHealth(float afTimeStep)
{
	OhplGa3Host host = Ga3Host();
	ohpl_ga3_update_regen_health(&mGa3Ctx, &host, afTimeStep);
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

bool iLuxEnemy::TriggersDisabled()
{
	return mbDisableTriggers || gpBase->mpPlayer->GetHelperFlashback()->IsActive();
}

//-----------------------------------------------------------------------

bool iLuxEnemy::Attack(const cEnemyAttackSizeData &aSizeData, const cEnemyAttackDamageData &aDamageData, float afDamageMul)
{
	OhplA4Host h = A4Host();
	return ohpl_a4_attack(&mA4,&h,&aSizeData,&aDamageData,afDamageMul);
}

//-----------------------------------------------------------------------

void iLuxEnemy::SetMoveSpeed(eLuxEnemyMoveSpeed aType)
{
	ohpl_a4_set_move_speed(&mA4,(int)aType);
}

//-----------------------------------------------------------------------

cAINode *iLuxEnemy::GetSearchForPlayerNode(int alMaxIterations, float afMaxAngleMul, float afMinDistMul, float afMaxDistMul)
{
	OhplGa3Host host = Ga3Host();
	const float vParams[4] = {mfPlayerSearchMaxAngle, mfPlayerSearchMinDistMul, mfPlayerSearchMaxDistMul, 0};
	int lIdx = ohpl_ga3_search_for_player_node(&mGa3Ctx, &host, alMaxIterations, afMaxAngleMul, afMinDistMul, afMaxDistMul, &vParams);
	return lIdx < 0 ? NULL : (cAINode*)ohpl_a4b_nav_cand(mNav, lIdx);
}

//-----------------------------------------------------------------------

cAINode *iLuxEnemy::GetPatrolAroundPlayerNode(int alMaxIterations, float afMaxAngleMul, float afMinDistMul, float afMaxDistMul)
{
	OhplGa3Host host = Ga3Host();
	const float vParams[3] = {mfPlayerPatrolMaxAngle, mfPlayerPatrolMinDist, mfPlayerPatrolMaxDist};
	int lIdx = ohpl_ga3_patrol_around_player_node(&mGa3Ctx, &host, alMaxIterations, afMaxAngleMul, afMinDistMul, afMaxDistMul, &vParams, &mvStartPosition.x);
	return lIdx < 0 ? NULL : (cAINode*)ohpl_a4b_nav_cand(mNav, lIdx);
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
	OhplA4Host h = A4Host();
	cMatrixf mtxOut;
	ohpl_a4_damage_shape_matrix(&mA4,&h,&avOffset.x,&mtxOut.m[0][0]);
	return mtxOut;
}

//-----------------------------------------------------------------------

cLuxProp_Object* iLuxEnemy::GetClosestFood(float afMaxDist, float afMaxHeightDist)
{
	OhplA4Host h = A4Host();
	return (cLuxProp_Object*)ohpl_a4_closest_food(&mA4,&h,afMaxDist,afMaxHeightDist);
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

cVector3f iLuxEnemy::GetDirection2D(const cVector3f &avPos)
{
	OhplA4Host h = A4Host();
	cVector3f vDir;
	ohpl_a4_direction_2d(&mA4,&h,&avPos.x,&vDir.x);
	return vDir;
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
	OhplA4Host h = A4Host();
	return ohpl_a4_abs_height_dist_to_player(&mA4,&h);
}

//-----------------------------------------------------------------------

bool iLuxEnemy::IsSeenByPlayer()
{
	OhplA4Host h = A4Host();
	return ohpl_a4_is_seen_by_player(&mA4,&h);
}

//-----------------------------------------------------------------------

bool iLuxEnemy::IsInPlayerFovAtFeetPos(const cVector3f& avFeetPos)
{
	OhplA4Host h = A4Host();
	return ohpl_a4_in_player_fov_at_feet(&mA4,&h,&avFeetPos.x);
}

//-----------------------------------------------------------------------

bool iLuxEnemy::IsVisibleToPlayerAtFeetPos(const cVector3f& avFeetPos)
{
	OhplA4Host h = A4Host();
	return ohpl_a4_visible_to_player_at_feet(&mA4,&h,&avFeetPos.x);
}

//-----------------------------------------------------------------------

float iLuxEnemy::GetPlayerMovementTowardEnemyAmount()
{
	OhplGa3Host host = Ga3Host();
	return ohpl_ga3_player_movement_toward_enemy(&mGa3Ctx, &host);
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

void iLuxEnemy::OnSetActive(bool abX)
{	
	OhplA4Host h = A4Host();
	ohpl_a4_on_set_active(&mA4,&h,abX);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------
kSerializeRustClass(cLuxStateMessage_SaveData)

//-----------------------------------------------------------------------

kSerializeRustClass(cLuxEnemyPatrolNode_SaveData)

//-----------------------------------------------------------------------

kSerializeRustClassV(iLuxEnemy_SaveData, iLuxEntity_SaveData)

//-----------------------------------------------------------------------

kSerializeRustClass(cLuxEnemyMover_SaveData)

void cLuxEnemyMover_SaveData::FromMover(uint64_t alNav)
{
	OhplA4bMoverSave s;
	ohpl_a4b_mover_get(alNav, &s);
	mbTurning = s.turning;
	mfTurnGoalAngle = s.turnGoalAngle;
	mfTurnSpeed = s.turnSpeed;
	mfTurnBreakAcc = s.turnBreakAcc;
	mfStuckCounter = s.stuckCounter;
	mlMoveState = s.moveState;
	mbOverideMoveState = s.overrideMoveState;
}

void cLuxEnemyMover_SaveData::ToMover(uint64_t alNav)
{
	OhplA4bMoverSave s;
	s.turning = mbTurning;
	s.turnGoalAngle = mfTurnGoalAngle;
	s.turnSpeed = mfTurnSpeed;
	s.turnBreakAcc = mfTurnBreakAcc;
	s.stuckCounter = mfStuckCounter;
	s.moveState = mlMoveState;
	s.overrideMoveState = mbOverideMoveState;
	ohpl_a4b_mover_set(alNav, &s);
}

//-----------------------------------------------------------------------

kSerializeRustClass(cLuxEnemyPathfinder_SaveData)

void cLuxEnemyPathfinder_SaveData::FromPathfinder(uint64_t alNav)
{
	mbMoving = ohpl_a4b_nav_moving(alNav);
	ohpl_a4b_nav_goal(alNav, &mvMoveGoalPos.x);

	for(size_t i=0, lN = ohpl_a4b_nav_path_len(alNav); i<lN; ++i)
	{
		mvPathNodeIds.Add(((cAINode*)ohpl_a4b_nav_path_at(alNav, i))->GetID());
	}
}

void cLuxEnemyPathfinder_SaveData::ToPathfinder(uint64_t alNav)
{
	ohpl_a4b_nav_set_goal(alNav, mbMoving, &mvMoveGoalPos.x);
}

void cLuxEnemyPathfinder_SaveData::SetupPathfinder(uint64_t alNav)
{
	cAINodeContainer *pNodeContainer = (cAINodeContainer*)ohpl_a4b_nav_nc(alNav);
	if(pNodeContainer==NULL) return;

	for(size_t i=0; i<mvPathNodeIds.Size(); ++i)
	{
		cAINode *pNode = pNodeContainer->GetNodeFromID(mvPathNodeIds[i]);
		if(pNode) ohpl_a4b_nav_path_push(alNav, (uintptr_t)pNode);
	}
}

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

    cWorld *pWorld = GetMap()->GetWorld();
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

	(pData->mbHallucination = mA4.hallucination);
	
	pData->mlCurrentState = mCurrentState;
	pData->mlNextState = mNextState;
	pData->mlPreviousState = mPreviousState;

	pData->mlSoundState = mA4.soundState;

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

	(pData->mfDarknessGlowAlpha = mA4.glowAlpha);
	(pData->mfDarknessGlowAlphaGoal = mA4.glowGoal);

	kCopyToVar(pData, mlAttackHitCounter);

	kCopyToVar(pData,mfFOVMul);

	(pData->mbAnimationIsSpeedDependant = mA4.animSpeedDependant);
	(pData->mfAnimationSpeedMul = mA4.animSpeedMul);
	(pData->mbUseMoveAnimWhenCurrentIsOver = mA4.useMoveAnimWhenOver);

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
	if(mA4.curAnim)
		pData->msCurrentAnimName = mA4.curAnim->GetName();
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
	pData->mMover.FromMover(mNav);
	pData->mPathfinder.FromPathfinder(mNav);

	///////////////////////
	//Character body
	if(mpCharBody)
	{
		pData->mCharBody.FromBody(mpCharBody);
	}
}

//-----------------------------------------------------------------------

void OhplA4bOnLoad();

void iLuxEnemy::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	OhplA4bOnLoad();
	///////////////////////
	// Init
	super_class::LoadFromSaveData(apSaveData);
	iLuxEnemy_SaveData *pData = static_cast<iLuxEnemy_SaveData*>(apSaveData);

	cWorld *pWorld = GetMap()->GetWorld();
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

	(mA4.hallucination = pData->mbHallucination);

	
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

	(mA4.glowAlpha = pData->mfDarknessGlowAlpha);
	(mA4.glowGoal = pData->mfDarknessGlowAlphaGoal);

	kCopyFromVar(pData, mlAttackHitCounter);

	kCopyFromVar(pData,mfFOVMul);

	(mA4.animSpeedDependant = pData->mbAnimationIsSpeedDependant);
	(mA4.animSpeedMul = pData->mfAnimationSpeedMul);
	(mA4.useMoveAnimWhenOver = pData->mbUseMoveAnimWhenCurrentIsOver);

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
		mA4.curAnim = mpMeshEntity->GetAnimationStateFromName(pData->msCurrentAnimName);
	}
	else
	{
		mA4.curAnim = NULL;
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
	pData->mMover.ToMover(mNav);
	pData->mPathfinder.ToPathfinder(mNav);

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

	cAINodeContainer *pNodeContainer =  GetNodeContainer();

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
				Error("Could not find patrol node id: %d for '%s'!\n", pData->mvPatrolNodes[i].mlNodeId, GetName().c_str());
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
	pData->mPathfinder.SetupPathfinder(mNav);
}

//-----------------------------------------------------------------------


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// RUST ENEMY CORE HOST
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool iLuxEnemy::WaterStepCheck()
{
	if(IsInWater()==false || GetWaterSurfaceData()==NULL) return false;

	cAnimationState *pAnim = GetCurrentAnimation();
	if(pAnim==NULL) return false;

	for(int i=0; i<pAnim->GetEventNum(); ++i)
	{
		cAnimationEvent *pEvent = pAnim->GetEvent(i);
		if(pEvent->mType != eAnimationEventType_Step) continue;

		if(	pEvent->mfTime >= pAnim->GetPreviousTimePosition() &&
			pEvent->mfTime < pAnim->GetTimePosition())
		{
			return true;
		}
	}
	return false;
}

void iLuxEnemy::WaterStepFx()
{
	cSurfaceImpactData *pImpact = GetWaterSurfaceData()->GetImpactDataFromSpeed(3.0f);
	cVector3f vEffectPos = mpCharBody->GetPosition();
	vEffectPos.y = GetWaterSurfaceY()+0.01f;

	cWorld *pWorld = GetMap()->GetWorld();

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

void iLuxEnemy::UpdateMoveAnimation()
{
	OhplGa3Host host = Ga3Host();
	OhplGa3MoverParams params;
	Ga3MoverParams(params);
	ohpl_ga3_mover_update_move_animation(mNav, &params, &host);
}

OhplGa3Host iLuxEnemy::Ga3Host()
{
	OhplGa3Host host;
	host.ctx = this;
	host.call = &iLuxEnemy::Ga3Call;
	host.nav = mNav;
	return host;
}

void iLuxEnemy::Ga3MoverParams(OhplGa3MoverParams &aParams)
{
	aParams.turnMinBreakAngle = mfTurnMinBreakAngle;
	aParams.turnBreakMul = mfTurnBreakMul;
	aParams.turnSpeedMul = mfTurnSpeedMul;
	aParams.turnMaxSpeed = mfTurnMaxSpeed;
	aParams.moveSpeedAnimMul = mA4.moveSpeedAnimMul;
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

static void *Ga3Ptr(const OhplGa3Io *apIo, int alSlot)
{
	return (void*)(((uintptr_t)(uint32_t)apIo->i[alSlot + 1] << 32) | (uint32_t)apIo->i[alSlot]);
}

static void Ga3SetPtr(OhplGa3Io *apIo, int alSlot, const void *apPtr)
{
	apIo->i[alSlot] = (int)(uint32_t)(uintptr_t)apPtr;
	apIo->i[alSlot + 1] = (int)(uint32_t)((uintptr_t)apPtr >> 32);
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
	case eOhplGa3Op_AnimExists:		return pEnemy->mA4.curAnim ? 1 : 0;
	case eOhplGa3Op_AnimSetSpeed:	pEnemy->mA4.curAnim->SetSpeed(apIo->f[0]); return 0;
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
		iLuxEntity *pEnt = pEnemy->GetMap()->GetEntityByID(apIo->i[0]);
		if(pEnt==NULL){ Ga3Put(apIo->v[0], cVector3f(0)); return 0;}
		Ga3Put(apIo->v[0], pEnt->GetAttachEntity()->GetWorldPosition());
		return 0;
	}
	case eOhplGa3Op_DoorBroken:		return pEnemy->GetMap()->DoorIsBroken(apIo->i[0]) ? 1 : 0;
	case eOhplGa3Op_DebugMsg:
		switch(apIo->i[0])
		{
		case 1:		gpBase->mpDebugHandler->AddMessage(_W("Enemy ") + cString::To16Char(pEnemy->GetName())+_W(" enabled!"), false ); break;
		case 2:		gpBase->mpDebugHandler->AddMessage(_W("Sound Heard! Vol: ")+cString::ToStringW(apIo->f[0]),false); break;
		case 3:		gpBase->mpDebugHandler->AddMessage(_W("Could not find node near sound!"),false); break;
		case 100:	gpBase->mpProgressLogHandler->AddLog(eLuxProgressLogLevel_Low, "Enemy '"+pEnemy->GetName()+"'  is in range"); break;
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

	case eOhplGa3Op_AStarPath:
	{
		pEnemy->mvNavScratch.clear();
		tAINodeList lstNodes;
		bool bRet = static_cast<cAStarHandler*>(Ga3Ptr(apIo, 0))->GetPath(Ga3Get(apIo->v[0]), Ga3Get(apIo->v[1]), &lstNodes);
		pEnemy->mvNavScratch.assign(lstNodes.begin(), lstNodes.end());
		apIo->i[2] = (int)pEnemy->mvNavScratch.size();
		return bRet ? 1 : 0;
	}
	case eOhplGa3Op_ScratchAt:		Ga3SetPtr(apIo, 0, pEnemy->mvNavScratch[apIo->i[0]]); return 0;
	case eOhplGa3Op_NodePos:		Ga3Put(apIo->v[0], static_cast<cAINode*>(Ga3Ptr(apIo, 0))->GetPosition()); return 0;
	case eOhplGa3Op_NcAtCenter:		return static_cast<cAINodeContainer*>(Ga3Ptr(apIo, 0))->GetNodeIsAtCenter() ? 1 : 0;
	case eOhplGa3Op_NcNodeCount:	return static_cast<cAINodeContainer*>(Ga3Ptr(apIo, 0))->GetNodeNum();
	case eOhplGa3Op_NcNodeAt:		Ga3SetPtr(apIo, 0, static_cast<cAINodeContainer*>(Ga3Ptr(apIo, 0))->GetNode(apIo->i[2])); return 0;
	case eOhplGa3Op_NcCollideY:		apIo->f[0] = static_cast<cAINodeContainer*>(Ga3Ptr(apIo, 0))->GetCollideSize().y; return 0;
	case eOhplGa3Op_NodeEdgeNum:	return static_cast<cAINode*>(Ga3Ptr(apIo, 0))->GetEdgeNum();
	case eOhplGa3Op_NodeEdgeNode:	Ga3SetPtr(apIo, 0, static_cast<cAINode*>(Ga3Ptr(apIo, 0))->GetEdge(apIo->i[2])->mpNode); return 0;
	case eOhplGa3Op_NcIter:
	{
		pEnemy->mvNavScratch.clear();
		cAINodeIterator it = static_cast<cAINodeContainer*>(Ga3Ptr(apIo, 0))->GetNodeIterator(Ga3Get(apIo->v[0]), apIo->f[0]);
		while(it.HasNext()) pEnemy->mvNavScratch.push_back(it.Next());
		return (int)pEnemy->mvNavScratch.size();
	}
	case eOhplGa3Op_NodeUsedAsGoal:	return pEnemy->GetMap()->AINodeIsUsedAsGoal(static_cast<cAINode*>(Ga3Ptr(apIo, 0))) ? 1 : 0;
	case eOhplGa3Op_NcFreePathAt:
		return static_cast<cAINodeContainer*>(Ga3Ptr(apIo, 2))->FreePath(Ga3Get(apIo->v[0]), Ga3Get(apIo->v[1]), apIo->i[0], apIo->i[1]) ? 1 : 0;
	case eOhplGa3Op_WaterStepCheck:	return pEnemy->WaterStepCheck() ? 1 : 0;
	case eOhplGa3Op_WaterStepFx:	pEnemy->WaterStepFx(); return 0;
	case eOhplGa3Op_Draw:
	{
		static const cColor cGrey(0.4f,1), cGreyLine(0.4f,0.4f,0.4f,1), cMagenta(1,0,1);
		iLowLevelGraphics *pGfx = pEnemy->mpNavDraw->GetLowLevelGfx();
		switch(apIo->i[0])
		{
		case 0:		pGfx->DrawSphere(Ga3Get(apIo->v[0]), apIo->f[0], cGrey); break;
		case 1:		pGfx->DrawLine(Ga3Get(apIo->v[0]), Ga3Get(apIo->v[1]), cGreyLine); break;
		case 2:		pGfx->DrawSphere(Ga3Get(apIo->v[0]), apIo->f[0], cMagenta); break;
		case 3:		pGfx->DrawLine(Ga3Get(apIo->v[0]), Ga3Get(apIo->v[1]), cMagenta); break;
		}
		return 0;
	}
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
		cLuxEntityIterator entIt = pEnemy->GetMap()->GetEntityIterator();
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
		pEnemy->GetMap()->BroadcastEnemyMessage(eLuxEnemyMessage_HelpMe, true, Ga3Get(apIo->v[0]), apIo->f[0], 0, false, Ga3Get(apIo->v[1]));
		return 0;
	case eOhplGa3Op_Attack:
		if(apIo->i[0] == 0)	pEnemy->Attack(pEnemy->mNormalAttackSize, pEnemy->mNormalAttackDamage);
		else				pEnemy->Attack(pEnemy->mNormalAttackSize, pEnemy->mBreakDoorAttackDamage);
		return 0;
	}
	return 0;
}
