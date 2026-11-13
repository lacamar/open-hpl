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

#include "LuxProp_Photocell.h"
#include "RustLuxSaveA8.h"

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxMap.h"
#include "LuxMapHelper.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_Photocell::cLuxPropLoader_Photocell(const tString& asName) : iLuxPropLoader(asName)
{
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_Photocell::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxProp_Photocell, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Photocell::LoadVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxProp_Photocell  *pPhotocell = static_cast<cLuxProp_Photocell*>(apProp);


}

//-----------------------------------------------------------------------

void cLuxPropLoader_Photocell::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{
	cLuxProp_Photocell  *pPhotocell = static_cast<cLuxProp_Photocell*>(apProp);

	if(hpl::OhplBackendRustRequested())
	{
		OhplGa4VarsAdapter vars(apInstanceVars);
		ohpl_a5_photocell_load_instance(vars.Vars(), &pPhotocell->mS);
		return;
	}
	pPhotocell->mfLightLevelOnLimit = apInstanceVars->GetVarFloat("LightLevelOnLimit", 0);
	pPhotocell->mfLightLevelOffLimit = apInstanceVars->GetVarFloat("LightLevelOffLimit", 0);
}
//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_Photocell::cLuxProp_Photocell(const tString &asName,int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_Photocell)
{
	mfLightLevel = 0;
	mfLightLevelCheckCount = 0;
	mbLit = false;
	mS.light_level = 0;
	mS.check_count = 0;
	mS.lit = false;
}

//-----------------------------------------------------------------------

cLuxProp_Photocell::~cLuxProp_Photocell()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxProp_Photocell::CanInteract(iPhysicsBody *apBody)
{
	return false;

}

//-----------------------------------------------------------------------

bool cLuxProp_Photocell::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	iPhysicsBody *pBody = mpMainBody ? mpMainBody : apBody;

	return true;
}

//-----------------------------------------------------------------------

void cLuxProp_Photocell::OnSetupAfterLoad(cWorld *apWorld)
{

}

//-----------------------------------------------------------------------

void cLuxProp_Photocell::OnResetProperties()
{

}
//-----------------------------------------------------------------------


void cLuxProp_Photocell::UpdatePropSpecific(float afTimeStep)
{
	/////////////////////
	// Update light level
	if(hpl::OhplBackendRustRequested())
	{
		OhplA5PhotoHost host = { this, &HostBounds, &HostLightAt, &HostIllumination };
		OhplGa4Run run(this);
		ohpl_a5_photocell_update(&mS, afTimeStep, &host, run.Sink());
		return;
	}

	UpdateLightLevel(afTimeStep);

	/////////////////////
	// Set mesh alpha
	float fGoalAmount = ohpl_ga4b_photo_goal(mfLightLevel, mfLightLevelOnLimit, mfLightLevelOffLimit);

	if(mpMeshEntity && mpMeshEntity->GetIlluminationAmount() != fGoalAmount)
	{
		mpMeshEntity->SetIlluminationAmount(ohpl_ga4b_photo_illum_step(mpMeshEntity->GetIlluminationAmount(), fGoalAmount, afTimeStep));
	}
}

//-----------------------------------------------------------------------

void cLuxProp_Photocell::BeforePropDestruction()
{
}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxProp_Photocell::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return eLuxFocusCrosshair_Default;
}

//-----------------------------------------------------------------------


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxProp_Photocell::HostBounds(void *apCtx, float *apSize, float *apCenter)
{
	cLuxProp_Photocell *pProp = static_cast<cLuxProp_Photocell*>(apCtx);
	cBoundingVolume *pBV = pProp->mpMeshEntity->GetBoundingVolume();
	cVector3f vSize = pBV->GetSize();
	cVector3f vCenter = pBV->GetWorldCenter();
	apSize[0] = vSize.x; apSize[1] = vSize.y; apSize[2] = vSize.z;
	apCenter[0] = vCenter.x; apCenter[1] = vCenter.y; apCenter[2] = vCenter.z;
}

//-----------------------------------------------------------------------

float cLuxProp_Photocell::HostLightAt(void *apCtx, const float *apPos)
{
	return gpBase->mpMapHelper->GetLightLevelAtPos(cVector3f(apPos[0], apPos[1], apPos[2]));
}

//-----------------------------------------------------------------------

bool cLuxProp_Photocell::HostIllumination(void *apCtx, float *apOut)
{
	cLuxProp_Photocell *pProp = static_cast<cLuxProp_Photocell*>(apCtx);
	if(pProp->mpMeshEntity == NULL) return false;
	*apOut = pProp->mpMeshEntity->GetIlluminationAmount();
	return true;
}

//-----------------------------------------------------------------------

void cLuxProp_Photocell::Ga4ApplyOp(const OhplGa4Op &aOp)
{
	switch(aOp.code)
	{
	case kOhplA5Op_ConnState:
		ConnectionStateChange(aOp.i);
		break;
	case kOhplA5Op_PhotoIllum:
		mpMeshEntity->SetIlluminationAmount(aOp.f[0]);
		break;
	default:
		iLuxProp::Ga4ApplyOp(aOp);
	}
}

//-----------------------------------------------------------------------

void cLuxProp_Photocell::UpdateLightLevel(float afTimeStep)
{
	if(ohpl_ga4b_photo_tick(&mfLightLevelCheckCount, afTimeStep)==false) return;

	cBoundingVolume *pBV = mpMeshEntity->GetBoundingVolume();
	cVector3f vSize = pBV->GetSize();
	cVector3f vCenter = pBV->GetWorldCenter();

	cVector3f vSamplePos[6];
	ohpl_ga4b_photo_samples(vSize.v, vCenter.v, vSamplePos[0].v);

	float vLight[6];
	for(int i=0; i<6; ++i)
	{
		vLight[i] = gpBase->mpMapHelper->GetLightLevelAtPos(vSamplePos[i]);
	}
	mfLightLevel = ohpl_ga4b_photo_level(vLight, 6);

	int lConnState = ohpl_ga4b_photo_switch(mfLightLevel, &mbLit, mfLightLevelOnLimit, mfLightLevelOffLimit);
	if(lConnState != 0) ConnectionStateChange(lConnState);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxProp_Photocell_SaveData, iLuxProp_SaveData)
kSerializeVar(mbLit,	eSerializeType_Float32)
kSerializeVar(mfLightLevel,	eSerializeType_Float32)
kSerializeVar(mfLightLevelCheckCount,	eSerializeType_Float32)
kSerializeVar(mfLightLevelOnLimit,	eSerializeType_Float32)
kSerializeVar(mfLightLevelOffLimit,	eSerializeType_Float32)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxProp_Photocell::CreateSaveData()
{
	return hplNew(cLuxProp_Photocell_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxProp_Photocell::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxProp_Photocell_SaveData *pData = static_cast<cLuxProp_Photocell_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	if(hpl::OhplBackendRustRequested())
	{
		ohpl_a8_photocell_save(&mS, pData);
		return;
	}
	kCopyToVar(pData,mbLit);
	kCopyToVar(pData,mfLightLevel);
	kCopyToVar(pData,mfLightLevelCheckCount);
	kCopyToVar(pData,mfLightLevelOnLimit);
	kCopyToVar(pData,mfLightLevelOffLimit);
}

//-----------------------------------------------------------------------

void cLuxProp_Photocell::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxProp_Photocell_SaveData *pData = static_cast<cLuxProp_Photocell_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	if(hpl::OhplBackendRustRequested())
	{
		ohpl_a8_photocell_load(&mS, pData);
		return;
	}
	kCopyFromVar(pData,mbLit);
	kCopyFromVar(pData,mfLightLevel);
	kCopyFromVar(pData,mfLightLevelCheckCount);
	kCopyFromVar(pData,mfLightLevelOnLimit);
	kCopyFromVar(pData,mfLightLevelOffLimit);
}

//-----------------------------------------------------------------------

void cLuxProp_Photocell::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------
