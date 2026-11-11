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

#include "LuxEnemy_Grunt.h"

#include "LuxEnemyMover.h"
#include "LuxEnemyPathfinder.h"

#include "LuxMap.h"
#include "LuxMapHelper.h"
#include "LuxMusicHandler.h"
#include "LuxDebugHandler.h"
#include "LuxGlobalDataHandler.h"

#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEnemyLoader_Grunt::cLuxEnemyLoader_Grunt(const tString& asName) : iLuxEnemyLoader(asName)
{
}

//-----------------------------------------------------------------------

iLuxEnemy *cLuxEnemyLoader_Grunt::CreateEnemy(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxEnemy_Grunt, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxEnemyLoader_Grunt::LoadVariables(iLuxEnemy *apEnemy, cXmlElement *apRootElem)
{
	cLuxEnemy_Grunt *pGrunt = static_cast<cLuxEnemy_Grunt*>(apEnemy);

	pGrunt->msNoticeSound = GetVarString("NoticeSound");
	pGrunt->msGiveUpNoticeSound = GetVarString("GiveUpNoticeSound");
	pGrunt->msEnabledSound = GetVarString("EnabledSound");
	gpBase->PreloadSound(pGrunt->msNoticeSound);
	gpBase->PreloadSound(pGrunt->msEnabledSound);

	pGrunt->mfGroggyDamageCount = GetVarFloat("GroggyDamageCount", 0);
	pGrunt->mfAlertToHuntDistance = GetVarFloat("AlertToHuntDistance", 0);
	pGrunt->mfAlertToInstantHuntDistance = GetVarFloat("AlertToInstantHuntDistance", 0);
	pGrunt->mfHuntPauseMinTime = GetVarFloat("HuntPauseMinTime", 0);
	pGrunt->mfHuntPauseMaxTime = GetVarFloat("HuntPauseMaxTime", 0);
	pGrunt->mfIncreaseAlertSpeedDistance = GetVarFloat("IncreaseAlertSpeedDistance", 0);
	pGrunt->mfIncreasedAlertSpeedMul = GetVarFloat("IncreasedAlertSpeedMul", 0);
	
	pGrunt->mfAlertRunTowardsToHuntLimit = GetVarFloat("AlertRunTowardsToHuntLimit", 0);
	pGrunt->mfAlertRunTowardsCheckDistance = GetVarFloat("AlertRunTowardsCheckDistance", 0);

	pGrunt->mfIdleExtraTimeMin = GetVarFloat("IdleExtraTimeMin", 2);
	pGrunt->mfIdleExtraTimeMax = GetVarFloat("IdleExtraTimeMax", 6);
	pGrunt->mlIdleExtraNum = GetVarInt("IdleExtraNum", 3);
}

//-----------------------------------------------------------------------

void cLuxEnemyLoader_Grunt::LoadInstanceVariables(iLuxEnemy *apEnemy, cResourceVarsObject *apInstanceVars)
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEnemy_Grunt::cLuxEnemy_Grunt(const tString &asName, int alID, cLuxMap *apMap) : iLuxEnemy(asName,alID,apMap, eLuxEnemyType_Grunt)
{
	mfWaitTime =0;
	mfAlertRunTowardsCount = 0;

	mbAlignEntityWithGroundRay = true;

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
	mGa3Ctx.idleExtraTimeMin = &mfIdleExtraTimeMin;
	mGa3Ctx.idleExtraTimeMax = &mfIdleExtraTimeMax;
	mGa3Ctx.idleExtraNum = &mlIdleExtraNum;
}

//-----------------------------------------------------------------------

cLuxEnemy_Grunt::~cLuxEnemy_Grunt()
{
	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxEnemy_Grunt::OnSetupAfterLoad(cWorld *apWorld)
{
}


//-----------------------------------------------------------------------

void cLuxEnemy_Grunt::OnAfterWorldLoad()
{

}

//-----------------------------------------------------------------------

void cLuxEnemy_Grunt::UpdateEnemySpecific(float afTimeStep)
{

}

//-----------------------------------------------------------------------

bool cLuxEnemy_Grunt::StateEventImplement(int alState, eLuxEnemyStateEvent aEvent, cLuxStateMessage *apMessage)
{
	OhplGa3Host host = Ga3Host();
	return ohpl_ga3_grunt_event(&mGa3Ctx, &host, alState, aEvent, (const OhplGa3Msg*)apMessage);
}

const tString* cLuxEnemy_Grunt::Ga3SoundName(int alKind)
{
	if(alKind == 0) return &msNoticeSound;
	if(alKind == 1) return &msGiveUpNoticeSound;
	return &msEnabledSound;
}


//-----------------------------------------------------------------------

void cLuxEnemy_Grunt::OnRenderSolidImplemented(cRendererCallbackFunctions* apFunctions)
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

bool cLuxEnemy_Grunt::PlayerIsDetected()
{
	OhplGa3Host host = Ga3Host();
	return ohpl_ga3_grunt_player_is_detected(&mGa3Ctx, &host);
}

//-----------------------------------------------------------------------

void cLuxEnemy_Grunt::OnDisableTriggers()
{
	if(mCurrentState == eLuxEnemyState_Idle) return;

	ChangeState(eLuxEnemyState_Patrol);
}

//-----------------------------------------------------------------------

float cLuxEnemy_Grunt::GetDamageMul(float afAmount, int alStrength)
{
	return ohpl_ga3_grunt_damage_mul(mCurrentState);
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxEnemy_Grunt_SaveData, iLuxEnemy_SaveData)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxEnemy_Grunt::CreateSaveData()
{
	return hplNew(cLuxEnemy_Grunt_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxEnemy_Grunt::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxEnemy_Grunt_SaveData *pData = static_cast<cLuxEnemy_Grunt_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	//kCopyToVar(pData,mbLit);
}

//-----------------------------------------------------------------------

void cLuxEnemy_Grunt::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxEnemy_Grunt_SaveData *pData = static_cast<cLuxEnemy_Grunt_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	
	////////////////////////
	// Handle changed enums
	if (mCurrentState >= eLuxEnemyState_PigEnumStart) mCurrentState = eLuxEnemyState_LastEnum;
	if (mNextState >= eLuxEnemyState_PigEnumStart) mNextState = eLuxEnemyState_LastEnum;
	if (mPreviousState >= eLuxEnemyState_PigEnumStart) mPreviousState = eLuxEnemyState_LastEnum;
}

//-----------------------------------------------------------------------

void cLuxEnemy_Grunt::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}


//-----------------------------------------------------------------------

