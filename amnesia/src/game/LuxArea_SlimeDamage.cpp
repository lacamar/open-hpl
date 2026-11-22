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

#include "LuxArea_SlimeDamage.h"
#include "RustLuxMap.h"

#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxHelpFuncs.h"
#include "LuxMessageHandler.h"
#include "LuxEffectHandler.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxAreaLoader_SlimeDamage::cLuxAreaLoader_SlimeDamage(const tString& asName) : iLuxAreaLoader(asName)
{

}

cLuxAreaLoader_SlimeDamage::~cLuxAreaLoader_SlimeDamage()
{

}

//-----------------------------------------------------------------------

iLuxArea *cLuxAreaLoader_SlimeDamage::CreateArea(const tString& asName, int alID, cLuxMap *apMap)
{
	cLuxArea_SlimeDamage *pArea = hplNew(cLuxArea_SlimeDamage, (asName, alID, apMap));
	return pArea;
}

//-----------------------------------------------------------------------

void cLuxAreaLoader_SlimeDamage::LoadVariables(iLuxArea *apArea, cWorld *apWorld)
{
	cLuxArea_SlimeDamage *pFlashArea = static_cast<cLuxArea_SlimeDamage*>(apArea);

	pFlashArea->mlSlimeType = GetVarInt("SlimeType",0);

	pFlashArea->mfMinCheckAttackTime = GetVarFloat("MinCheckAttackTime",0);
	pFlashArea->mfMaxCheckAttackTime = GetVarFloat("MaxCheckAttackTime",0);
	pFlashArea->mbDisableAfterAttack = GetVarBool("DisableAfterAttack",false);
	pFlashArea->msCallback = GetVarString("Callback","");

	pFlashArea->mfCheckCollisionCount = cMath::RandRectf(pFlashArea->mfMinCheckAttackTime, pFlashArea->mfMaxCheckAttackTime);
	
	//Load from config file.
	tString sPrefix = "SlimeType" + cString::ToString(pFlashArea->mlSlimeType);

	pFlashArea->msAttackSound = gpBase->mpGameCfg->GetString("Slime",sPrefix+"_AttackSound", "");
	pFlashArea->msAttackPS = gpBase->mpGameCfg->GetString("Slime",sPrefix+"_AttackPS", "");
	pFlashArea->mfMinAttackDamage = gpBase->mpGameCfg->GetFloat("Slime",sPrefix+"_MinAttackDamage", 0);
	pFlashArea->mfMaxAttackDamage = gpBase->mpGameCfg->GetFloat("Slime",sPrefix+"_MaxAttackDamage", 0);
	pFlashArea->mfScreenShakeAmount = gpBase->mpGameCfg->GetFloat("Slime",sPrefix+"_ScreenShakeAmount", 0);
}

void cLuxAreaLoader_SlimeDamage::SetupArea(iLuxArea *apArea, cWorld *apWorld)
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxArea_SlimeDamage::cLuxArea_SlimeDamage(const tString &asName, int alID, cLuxMap *apMap)  : iLuxArea(asName,alID,apMap, eLuxAreaType_SlimeDamage),
	mlSlimeType(Pod<OhplA7Slime>().slimeType),
	mfMinCheckAttackTime(Pod<OhplA7Slime>().minCheckAttackTime),
	mfMaxCheckAttackTime(Pod<OhplA7Slime>().maxCheckAttackTime),
	mbDisableAfterAttack(Pod<OhplA7Slime>().disableAfterAttack),
	mfMinAttackDamage(Pod<OhplA7Slime>().minAttackDamage),
	mfMaxAttackDamage(Pod<OhplA7Slime>().maxAttackDamage),
	mfScreenShakeAmount(Pod<OhplA7Slime>().screenShakeAmount),
	mfCheckCollisionCount(Pod<OhplA7Slime>().checkCollisionCount)
{
	mfCheckCollisionCount = 0;
}

//-----------------------------------------------------------------------

cLuxArea_SlimeDamage::~cLuxArea_SlimeDamage()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxArea_SlimeDamage::OnUpdate(float afTimeStep)
{
	ohpl_a7_area_update(mpA7Area, this, mpMap->GetA7(), afTimeStep);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kSerializeRustClassP(cLuxArea_SlimeDamage_SaveData, iLuxArea_SaveData)

//-----------------------------------------------------------------------

iLuxArea* cLuxArea_SlimeDamage_SaveData::CreateArea(cLuxMap *apMap)
{
	return hplNew(cLuxArea_SlimeDamage, (msName, mlID, apMap));
}

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxArea_SlimeDamage::CreateSaveData()
{
	return hplNew(cLuxArea_SlimeDamage_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxArea_SlimeDamage::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::SaveToSaveData(apSaveData);
	cLuxArea_SlimeDamage_SaveData *pData = static_cast<cLuxArea_SlimeDamage_SaveData*>(apSaveData);
	
	kCopyToVar(pData, mlSlimeType);

    kCopyToVar(pData, mfMinCheckAttackTime);
	kCopyToVar(pData, mfMaxCheckAttackTime);
	kCopyToVar(pData, mbDisableAfterAttack);
	kCopyFromVar(pData, msCallback);

	kCopyToVar(pData, mfCheckCollisionCount);

	kCopyToVar(pData, msAttackSound);
	kCopyToVar(pData, msAttackPS);
	kCopyToVar(pData, mfMinAttackDamage);
	kCopyToVar(pData, mfMaxAttackDamage);
	kCopyToVar(pData, mfScreenShakeAmount);
}

//-----------------------------------------------------------------------

void cLuxArea_SlimeDamage::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::LoadFromSaveData(apSaveData);
	cLuxArea_SlimeDamage_SaveData *pData = static_cast<cLuxArea_SlimeDamage_SaveData*>(apSaveData);

	kCopyFromVar(pData, mlSlimeType);

	kCopyFromVar(pData, mfMinCheckAttackTime);
	kCopyFromVar(pData, mfMaxCheckAttackTime);
	kCopyFromVar(pData, mbDisableAfterAttack);
	kCopyFromVar(pData, msCallback);

	kCopyFromVar(pData, mfCheckCollisionCount);

	kCopyFromVar(pData, msAttackSound);
	kCopyFromVar(pData, msAttackPS);
	kCopyFromVar(pData, mfMinAttackDamage);
	kCopyFromVar(pData, mfMaxAttackDamage);
	kCopyFromVar(pData, mfScreenShakeAmount);
}

//-----------------------------------------------------------------------

void cLuxArea_SlimeDamage::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);

}

//-----------------------------------------------------------------------

