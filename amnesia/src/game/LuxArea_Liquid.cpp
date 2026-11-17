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

#include "LuxArea_Liquid.h"
#include "RustLuxMap.h"

#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxEnemy.h"


//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxAreaLoader_Liquid::cLuxAreaLoader_Liquid(const tString& asName) : iLuxAreaLoader(asName)
{

}

cLuxAreaLoader_Liquid::~cLuxAreaLoader_Liquid()
{

}

//-----------------------------------------------------------------------

iLuxArea *cLuxAreaLoader_Liquid::CreateArea(const tString& asName, int alID, cLuxMap *apMap)
{
	cLuxArea_Liquid *pArea = hplNew(cLuxArea_Liquid, (asName, alID, apMap));
	return pArea;
}

//-----------------------------------------------------------------------

void cLuxAreaLoader_Liquid::LoadVariables(iLuxArea *apArea, cWorld *apWorld)
{
	cLuxArea_Liquid *pLiquidArea = static_cast<cLuxArea_Liquid*>(apArea);

	pLiquidArea->mfDensity = GetVarFloat("Density",0);
	pLiquidArea->mfLinearViscosity = GetVarFloat("LinearViscosity",0);
	pLiquidArea->mfAngularViscosity = GetVarFloat("AngularViscosity",0);

	tString sPhysicsMaterial = GetVarString("PhysicsMaterial", "");
	pLiquidArea->mpPhysicsMaterial = apWorld->GetPhysicsWorld()->GetMaterialFromName(sPhysicsMaterial);
	if(pLiquidArea->mpPhysicsMaterial ==NULL)
		Error("Could not load physics material '%s' for liquid area '%s'!\n",sPhysicsMaterial.c_str(), pLiquidArea->msName.c_str());

	pLiquidArea->mbHasWaves = GetVarBool("HasWaves",false);
	pLiquidArea->mfWaveAmp = GetVarFloat("WaveAmp",0);
	pLiquidArea->mfWaveFreq = GetVarFloat("WaveFreq",0);

	pLiquidArea->mfPlayerSpeedMul = GetVarFloat("PlayerSpeedMul",0.8f);

	float fMaxWaveDistance = GetVarFloat("MaxWaveDistance",25);
	pLiquidArea->mfMaxWaveDistanceSqr = fMaxWaveDistance*fMaxWaveDistance;
}

void cLuxAreaLoader_Liquid::SetupArea(iLuxArea *apArea, cWorld *apWorld)
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxArea_Liquid::cLuxArea_Liquid(const tString &asName, int alID, cLuxMap *apMap)  : iLuxArea(asName,alID,apMap, eLuxAreaType_Liquid),
	mfDensity(Pod<OhplA7Liquid>().density),
	mfLinearViscosity(Pod<OhplA7Liquid>().linearViscosity),
	mfAngularViscosity(Pod<OhplA7Liquid>().angularViscosity),
	mbHasWaves(Pod<OhplA7Liquid>().hasWaves),
	mfWaveAmp(Pod<OhplA7Liquid>().waveAmp),
	mfWaveFreq(Pod<OhplA7Liquid>().waveFreq),
	mfPlayerSpeedMul(Pod<OhplA7Liquid>().playerSpeedMul),
	mfMaxWaveDistanceSqr(Pod<OhplA7Liquid>().maxWaveDistanceSqr),
	mfTimeCount(Pod<OhplA7Liquid>().timeCount),
	mSurfacePlane(*reinterpret_cast<cPlanef*>(&Pod<OhplA7Liquid>().surfacePlane))
{
	mpPhysicsMaterial = NULL;
	mbFullGameSave = true;

	mfTimeCount = 0;
}

//-----------------------------------------------------------------------

cLuxArea_Liquid::~cLuxArea_Liquid()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxArea_Liquid::SetupAfterLoad(cWorld *apWorld)
{
	float fHeight = mpBody->GetShape()->GetSize().y;
	cVector3f vPos = mpBody->GetWorldPosition();

	ohpl_a7_area_liquid_setup(mpA7Area, vPos.y, fHeight);
}

//-----------------------------------------------------------------------

void cLuxArea_Liquid::OnUpdate(float afTimeStep)
{
	ohpl_a7_area_update(mpA7Area, this, mpMap->GetA7(), afTimeStep);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxArea_Liquid_SaveData, iLuxArea_SaveData)
kSerializeVar(mfDensity, eSerializeType_Float32)
kSerializeVar(mfLinearViscosity, eSerializeType_Float32)
kSerializeVar(mfAngularViscosity, eSerializeType_Float32)

kSerializeVar(msPhysicsMaterial, eSerializeType_String)

kSerializeVar(mbHasWaves, eSerializeType_Bool)
kSerializeVar(mfWaveAmp, eSerializeType_Float32)
kSerializeVar(mfWaveFreq, eSerializeType_Float32)

kSerializeVar(mfPlayerSpeedMul, eSerializeType_Float32)
kSerializeVar(mfMaxWaveDistanceSqr, eSerializeType_Float32)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxArea* cLuxArea_Liquid_SaveData::CreateArea(cLuxMap *apMap)
{
	return hplNew(cLuxArea_Liquid, (msName, mlID, apMap));
}

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxArea_Liquid::CreateSaveData()
{
	return hplNew(cLuxArea_Liquid_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxArea_Liquid::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::SaveToSaveData(apSaveData);
	cLuxArea_Liquid_SaveData *pData = static_cast<cLuxArea_Liquid_SaveData*>(apSaveData);

	if(mpPhysicsMaterial)
		pData->msPhysicsMaterial = mpPhysicsMaterial->GetName();

	kCopyToVar(pData, mfDensity);
	kCopyToVar(pData, mfLinearViscosity);
	kCopyToVar(pData, mfAngularViscosity);

	kCopyToVar(pData, mbHasWaves);
	kCopyToVar(pData, mfWaveAmp);
	kCopyToVar(pData, mfWaveFreq);

	kCopyToVar(pData, mfPlayerSpeedMul);
	kCopyToVar(pData, mfMaxWaveDistanceSqr);

}

//-----------------------------------------------------------------------

void cLuxArea_Liquid::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::LoadFromSaveData(apSaveData);
	cLuxArea_Liquid_SaveData *pData = static_cast<cLuxArea_Liquid_SaveData*>(apSaveData);

	if(pData->msPhysicsMaterial != "")
	{
		mpPhysicsMaterial = mpMap->GetPhysicsWorld()->GetMaterialFromName(pData->msPhysicsMaterial);
		if(mpPhysicsMaterial ==NULL)
			Error("Could not load physics material '%s' for liquid area '%s'!\n",pData->msPhysicsMaterial.c_str(), msName.c_str());
	}

	kCopyFromVar(pData, mfDensity);
	kCopyFromVar(pData, mfLinearViscosity);
	kCopyFromVar(pData, mfAngularViscosity);

	kCopyFromVar(pData, mbHasWaves);
	kCopyFromVar(pData, mfWaveAmp);
	kCopyFromVar(pData, mfWaveFreq);

	kCopyFromVar(pData, mfPlayerSpeedMul);
	kCopyFromVar(pData, mfMaxWaveDistanceSqr);
}

//-----------------------------------------------------------------------

void cLuxArea_Liquid::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);

}

//-----------------------------------------------------------------------

