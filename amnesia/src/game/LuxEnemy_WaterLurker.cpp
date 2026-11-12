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

#include "LuxEnemy_WaterLurker.h"

#include "LuxEnemyMover.h"
#include "LuxEnemyPathfinder.h"

#include "LuxMap.h"
#include "LuxMapHelper.h"
#include "LuxMusicHandler.h"
#include "LuxDebugHandler.h"

#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"

#include "LuxProp_Object.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEnemyLoader_WaterLurker::cLuxEnemyLoader_WaterLurker(const tString& asName) : iLuxEnemyLoader(asName)
{
}

//-----------------------------------------------------------------------

iLuxEnemy *cLuxEnemyLoader_WaterLurker::CreateEnemy(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxEnemy_WaterLurker, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxEnemyLoader_WaterLurker::LoadVariables(iLuxEnemy *apEnemy, cXmlElement *apRootElem)
{
	cLuxEnemy_WaterLurker *pWaterLurker = static_cast<cLuxEnemy_WaterLurker*>(apEnemy);

	pWaterLurker->mfPlayerDetectionHeight = GetVarFloat("PlayerDetectionHeight", 0.6f);
	pWaterLurker->mfEatDamage = GetVarFloat("EatDamage", 0);
	pWaterLurker->mfAfterAttackPauseTime = GetVarFloat("AfterAttackPauseTime", 0);

	pWaterLurker->msSplashPS_Walk = GetVarString("SplashPS_Walk", "");
	pWaterLurker->msSplashPS_Run = GetVarString("SplashPS_Run", "");
	pWaterLurker->msSplashPS_Eat = GetVarString("SplashPS_Eat", "");
	pWaterLurker->msSplashPS_Attack = GetVarString("SplashPS_Attack", "");

	pWaterLurker->msSplashSound_Walk = GetVarString("SplashSound_Walk", "");
	pWaterLurker->msSplashSound_Run = GetVarString("SplashSound_Run", "");
	pWaterLurker->msSplashSound_Eat = GetVarString("SplashSound_Eat", "");
	pWaterLurker->msSplashSound_Attack = GetVarString("SplashSound_Attack", "");

	gpBase->PreloadSound(pWaterLurker->msSplashSound_Walk);
	gpBase->PreloadSound(pWaterLurker->msSplashSound_Run);
	gpBase->PreloadSound(pWaterLurker->msSplashSound_Eat);
	gpBase->PreloadSound(pWaterLurker->msSplashSound_Attack);

	gpBase->PreloadParticleSystem(pWaterLurker->msSplashPS_Walk);
	gpBase->PreloadParticleSystem(pWaterLurker->msSplashPS_Run);
	gpBase->PreloadParticleSystem(pWaterLurker->msSplashPS_Eat);
	gpBase->PreloadParticleSystem(pWaterLurker->msSplashPS_Attack);
}

//-----------------------------------------------------------------------

void cLuxEnemyLoader_WaterLurker::LoadInstanceVariables(iLuxEnemy *apEnemy, cResourceVarsObject *apInstanceVars)
{
	cLuxEnemy_WaterLurker *pWaterLurker = static_cast<cLuxEnemy_WaterLurker*>(apEnemy);

	float fHeight = apInstanceVars->GetVarFloat("PlayerDetectionHeight", 0);
	if(fHeight >0) pWaterLurker->mfPlayerDetectionHeight = fHeight;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEnemy_WaterLurker::cLuxEnemy_WaterLurker(const tString &asName, int alID, cLuxMap *apMap) : iLuxEnemy(asName,alID,apMap, eLuxEnemyType_WaterLurker)
{
	mbUseAnimations = false;
	mbCausesSanityDecrease = false;
	mbCausesSanityDecreaseAsDefault = false;

	mGa3Wl.playerDetectionHeight = &mfPlayerDetectionHeight;
	mGa3Wl.afterAttackPause = &mfAfterAttackPauseTime;
	mGa3Wl.causesSanityDecrease = &mbCausesSanityDecrease;
}

//-----------------------------------------------------------------------

cLuxEnemy_WaterLurker::~cLuxEnemy_WaterLurker()
{
	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxEnemy_WaterLurker::OnSetupAfterLoad(cWorld *apWorld)
{
	mpMeshEntity->SetActive(false);
	mpMeshEntity->SetVisible(false);
}


//-----------------------------------------------------------------------

void cLuxEnemy_WaterLurker::OnAfterWorldLoad()
{

}

//-----------------------------------------------------------------------

void cLuxEnemy_WaterLurker::UpdateEnemySpecific(float afTimeStep)
{
	}	

//-----------------------------------------------------------------------

bool cLuxEnemy_WaterLurker::StateEventImplement(int alState, eLuxEnemyStateEvent aEvent, cLuxStateMessage *apMessage)
{
	OhplGa3Host host = Ga3WlHost();
	return ohpl_ga3b_wl_event(&mGa3Ctx, &mGa3Wl, &host, alState, aEvent, (const OhplGa3Msg*)apMessage);
}

OhplGa3Host cLuxEnemy_WaterLurker::Ga3WlHost()
{
	OhplGa3Host host;
	host.ctx = this;
	host.call = &cLuxEnemy_WaterLurker::Ga3WlCall;
	return host;
}

int cLuxEnemy_WaterLurker::Ga3WlCall(void *apEnemy, unsigned alOp, OhplGa3Io *apIo)
{
	cLuxEnemy_WaterLurker *pLurker = static_cast<cLuxEnemy_WaterLurker*>(apEnemy);
	switch(alOp)
	{
	case eOhplGa3bOp_WlSplash:
		pLurker->SplashWater((eWaterLurkerSplash)apIo->i[0]);
		return 0;
	case eOhplGa3bOp_WlDebug:
		switch(apIo->i[0])
		{
		case 0:	gpBase->mpDebugHandler->AddMessage(_W("Found food!"),false); break;
		case 1:	gpBase->mpDebugHandler->AddMessage(_W("Found food but no node.."),false); break;
		case 2:	gpBase->mpDebugHandler->AddMessage(_W("to eat state!\n"),false); break;
		}
		return 0;
	case eOhplGa3bOp_WlFoodBite:
	{
		cLuxProp_Object *pFood = pLurker->GetClosestFood(3.5f, apIo->f[0]);
		if(pFood == NULL)
		{
			gpBase->mpDebugHandler->AddMessage(_W("No more food!!"),false);
			return 0;
		}
		iPhysicsBody *pFoodBody = pFood->GetBody(0);
		cVector3f vToFeet(pLurker->mpCharBody->GetFeetPosition() - pFoodBody->GetLocalPosition());
		vToFeet.Normalize();
		pFoodBody->AddImpulse(vToFeet*2.5f);
		pLurker->SplashWater(eWaterLurkerSplash_Eat);
		pFood->GiveDamage(pLurker->mfEatDamage, 10);
		gpBase->mpDebugHandler->AddMessage(_W("took bite!!"),false);
		gpBase->mpDebugHandler->AddMessage(_W("Health: ")+cString::ToStringW(pFood->GetHealth()),false);
		cVector3f vFoodPos = pFood->GetBody(0)->GetLocalPosition();
		apIo->v[0][0] = vFoodPos.x; apIo->v[0][1] = vFoodPos.y; apIo->v[0][2] = vFoodPos.z;
		return 1;
	}
	}
	return iLuxEnemy::Ga3Call(apEnemy, alOp, apIo);
}

//-----------------------------------------------------------------------

bool cLuxEnemy_WaterLurker::InRangeOfFood(iPhysicsBody *apFoodBody)
{
	//TODO: Check if in water?
	float fYDist = apFoodBody->GetLocalPosition().y - mpCharBody->GetFeetPosition().y;
	
	bool bX = cMath::Abs(fYDist) <= mfPlayerDetectionHeight;
	return bX;
}

//-----------------------------------------------------------------------

void cLuxEnemy_WaterLurker::OnRenderSolidImplemented(cRendererCallbackFunctions* apFunctions)
{
	iPhysicsWorld *pPhysicsWorld = mpMap->GetPhysicsWorld();

	mpCharBody->GetCurrentBody()->RenderDebugGeometry(apFunctions->GetLowLevelGfx(), cColor(1,1));

	if(mCurrentState == eLuxEnemyState_AttackMeleeShort || mCurrentState== eLuxEnemyState_BreakDoor)
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

void cLuxEnemy_WaterLurker::SplashWater(eWaterLurkerSplash aType)
{
	tString sPS = "";
	if(aType == eWaterLurkerSplash_Walk)		sPS = msSplashPS_Walk;
	else if(aType == eWaterLurkerSplash_Run)	sPS = msSplashPS_Run;
	else if(aType == eWaterLurkerSplash_Eat)	sPS = msSplashPS_Eat;
	else if(aType == eWaterLurkerSplash_Attack)	sPS = msSplashPS_Attack;
	
	if(sPS!="")
	{
		cParticleSystem *pPS = mpMap->GetWorld()->CreateParticleSystem("LurkerSplash", sPS,1);
		if(pPS) pPS->SetPosition(mpCharBody->GetFeetPosition() + cVector3f(0, 0.7f, 0));
	}


	tString sSound = "";
	if(aType == eWaterLurkerSplash_Walk)		sSound = msSplashSound_Walk;
	else if(aType == eWaterLurkerSplash_Run)	sSound = msSplashSound_Run;
	else if(aType == eWaterLurkerSplash_Eat)	sSound = msSplashSound_Eat;
	else if(aType == eWaterLurkerSplash_Attack)	sSound = msSplashSound_Attack;

	PlaySound(sSound);
}

//-----------------------------------------------------------------------

bool cLuxEnemy_WaterLurker::PlayerIsDetected()
{
	OhplGa3Host host = Ga3WlHost();
	return ohpl_ga3b_wl_player_is_detected(&mGa3Ctx, &mGa3Wl, &host);
}

//-----------------------------------------------------------------------

void cLuxEnemy_WaterLurker::OnSetActiveEnemySpecific(bool abX)
{
	//Make sure the mesh is invisible!
	mpMeshEntity->SetActive(false);
	mpMeshEntity->SetVisible(false);
}

//-----------------------------------------------------------------------

float cLuxEnemy_WaterLurker::GetDamageMul(float afAmount, int alStrength)
{
	return ohpl_ga3_grunt_damage_mul(mCurrentState);
}

//-----------------------------------------------------------------------

void cLuxEnemy_WaterLurker::PatrolUpdateGoal()
{
	
}

//-----------------------------------------------------------------------

void cLuxEnemy_WaterLurker::PatrolEndOfPath()
{
	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxEnemy_WaterLurker_SaveData, iLuxEnemy_SaveData)

kSerializeVar(mfPlayerDetectionHeight, eSerializeType_Float32)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxEnemy_WaterLurker::CreateSaveData()
{
	return hplNew(cLuxEnemy_WaterLurker_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxEnemy_WaterLurker::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxEnemy_WaterLurker_SaveData *pData = static_cast<cLuxEnemy_WaterLurker_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	kCopyToVar(pData,mfPlayerDetectionHeight);
}

//-----------------------------------------------------------------------

void cLuxEnemy_WaterLurker::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxEnemy_WaterLurker_SaveData *pData = static_cast<cLuxEnemy_WaterLurker_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	kCopyFromVar(pData,mfPlayerDetectionHeight);
	
	////////////////////////
	// Handle changed enums
	if (mCurrentState >= eLuxEnemyState_PigEnumStart) mCurrentState = eLuxEnemyState_LastEnum;
	if (mNextState >= eLuxEnemyState_PigEnumStart) mNextState = eLuxEnemyState_LastEnum;
	if (mPreviousState >= eLuxEnemyState_PigEnumStart) mPreviousState = eLuxEnemyState_LastEnum;
}

//-----------------------------------------------------------------------

void cLuxEnemy_WaterLurker::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}


//-----------------------------------------------------------------------

