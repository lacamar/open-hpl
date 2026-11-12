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

#include "LuxCritter_Spider.h"

#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxPlayerState.h"
#include "LuxMap.h"
#include "LuxMapHelper.h"
#include "LuxDebugHandler.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_Critter_Spider::cLuxPropLoader_Critter_Spider(const tString& asName) : iLuxPropLoader_Critter(asName)
{
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_Critter_Spider::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxCritter_Spider, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Critter_Spider::LoadCritterVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxCritter_Spider  *pCritter_Spider = static_cast<cLuxCritter_Spider*>(apProp);

	OhplGa4bVars<cResourceVarsObject> vars(this);
	vars.Slot(kOhplGa4bSlot_SpiderHitSound, pCritter_Spider->msHitSound);
	vars.Slot(kOhplGa4bSlot_SpiderIdleSound, pCritter_Spider->msIdleSound);
	vars.Slot(kOhplGa4bSlot_SpiderAttackSound, pCritter_Spider->msAttackSound);
	ohpl_ga4b_spider_load(vars.Vars(), &pCritter_Spider->mSP);
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Critter_Spider::LoadCritterInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxCritter_Spider::cLuxCritter_Spider(const tString &asName,int alID, cLuxMap *apMap) : iLuxProp_CritterBase(asName,alID,apMap)
{
	ohpl_ga4b_spider_new(&mC, &mSS);
}

//-----------------------------------------------------------------------

cLuxCritter_Spider::~cLuxCritter_Spider()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxCritter_Spider::UpdateCritterSpecific(float afTimeStep)
{
	UpdateVelocity(afTimeStep);
	UpdateGroundCheck(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxCritter_Spider::OnDamageCritter(float afAmount)
{
	OhplGa4bSink<iLuxProp_CritterBase> sink(this);
	ohpl_ga4b_spider_on_damage(&mC, &mSS, afAmount, sink.Sink());
}

void cLuxCritter_Spider::OnKillCritter()
{

}


//-----------------------------------------------------------------------

float cLuxCritter_Spider::DrawDebug(cGuiSet *apSet,iFontData *apFont,float afStartY)
{
	return afStartY;
	apSet->DrawFont(apFont, cVector3f(5,afStartY,1),12,cColor(1,1),_W("OnGround: %d Colliding: %d Health: %f"), mSS.on_ground, mC.colliding, mfHealth);
	afStartY += 13;
	apSet->DrawFont(apFont, cVector3f(5,afStartY,1),12,cColor(1,1),_W("State: %d"), mSS.state);
	afStartY += 13;

	return afStartY;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxCritter_Spider::UpdateVelocity(float afTimeStep)
{
	cLuxCritterQuery query(this);
	OhplGa4bSink<iLuxProp_CritterBase> sink(this);
	ohpl_ga4b_spider_update_velocity(&mC, &mSS, &mSP, query.Query(), sink.Sink(), afTimeStep);
}

void cLuxCritter_Spider::OnGa4bSound(int alKind)
{
	if(alKind == kOhplGa4bSnd_SpiderHit)		PlaySound("Critter_SpiderHitSound",msHitSound,true, true);
	else if(alKind == kOhplGa4bSnd_SpiderIdle)	PlaySound("Critter_SpiderSound",msIdleSound,true, true);
	else										PlaySound("CritterAttack", msAttackSound, true, true);
}

//-----------------------------------------------------------------------

void cLuxCritter_Spider::UpdateGroundCheck(float afTimeStep)
{
	cLuxCritterQuery query(this);
	ohpl_ga4b_spider_ground_check(&mC, &mSS, query.Query(), afTimeStep);
}


//-----------------------------------------------------------------------

void cLuxCritter_Spider::OnShapeCollision(const cVector3f& avPushVec, float afTimeStep)
{
	cLuxCritterQuery query(this);
	ohpl_ga4b_spider_on_shape_collision(&mC, &mSS, query.Query(), avPushVec.v, afTimeStep);
}



//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxCritter_Spider_SaveData, iLuxProp_CritterBase_SaveData)
kSerializeVar(mlState, eSerializeType_Int32)
kSerializeVar(mfFallingCount, eSerializeType_Float32)
kSerializeVar(mfNewGroundNormalCount, eSerializeType_Float32)
kSerializeVar(mbOnGround, eSerializeType_Bool)
kSerializeVar(mfMaxSpeed, eSerializeType_Float32)
kSerializeVar(mfCount, eSerializeType_Float32)
kSerializeVar(mfPlaySoundCount, eSerializeType_Float32)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxCritter_Spider::CreateSaveData()
{
	return hplNew(cLuxCritter_Spider_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxCritter_Spider::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxCritter_Spider_SaveData *pData = static_cast<cLuxCritter_Spider_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	pData->mlState = mSS.state;
	pData->mfFallingCount = mSS.falling_count;
	pData->mfNewGroundNormalCount = mSS.new_ground_normal_count;
	pData->mbOnGround = mSS.on_ground;
	pData->mfMaxSpeed = mSS.max_speed;
	pData->mfCount = mSS.count;
	pData->mfPlaySoundCount = mSS.play_sound_count;
}

//-----------------------------------------------------------------------

void cLuxCritter_Spider::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxCritter_Spider_SaveData *pData = static_cast<cLuxCritter_Spider_SaveData*>(apSaveData);
	
	//////////////////
	//Set variables
	mSS.state = pData->mlState;
	mSS.falling_count = pData->mfFallingCount;
	mSS.new_ground_normal_count = pData->mfNewGroundNormalCount;
	mSS.on_ground = pData->mbOnGround;
	mSS.max_speed = pData->mfMaxSpeed;
	mSS.count = pData->mfCount;
	mSS.play_sound_count = pData->mfPlaySoundCount;
}

//-----------------------------------------------------------------------

void cLuxCritter_Spider::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------
