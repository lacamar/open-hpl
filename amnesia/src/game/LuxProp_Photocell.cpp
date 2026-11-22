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

void cLuxPropLoader_Photocell::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{
	cLuxProp_Photocell  *pPhotocell = static_cast<cLuxProp_Photocell*>(apProp);

	OhplGa4VarsAdapter vars(apInstanceVars);
	ohpl_a5_photocell_load_instance(vars.Vars(), &pPhotocell->mS);
}
//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_Photocell::cLuxProp_Photocell(const tString &asName,int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_Photocell)
{
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


void cLuxProp_Photocell::UpdatePropSpecific(float afTimeStep)
{
	/////////////////////
	// Update light level
	OhplA5PhotoHost host = { this, &HostBounds, &HostLightAt, &HostIllumination };
	OhplGa4Run run(this);
	ohpl_a5_photocell_update(&mS, afTimeStep, &host, run.Sink());
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


//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kSerializeRustClassP(cLuxProp_Photocell_SaveData, iLuxProp_SaveData)

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
	ohpl_a8_photocell_save(&mS, pData);
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
	ohpl_a8_photocell_load(&mS, pData);
}

//-----------------------------------------------------------------------
