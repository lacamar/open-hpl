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

#include "LuxProp_Button.h"

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxMap.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_Button::cLuxPropLoader_Button(const tString& asName) : iLuxPropLoader(asName)
{
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_Button::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxProp_Button, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Button::LoadVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxProp_Button  *pButton = static_cast<cLuxProp_Button*>(apProp);

	///////////////////////////
	// General
	//pButton->mbCanBeLitByPlayer = GetVarBool("CanBeLitByPlayer", true);
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Button::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{
    cLuxProp_Button  *pButton = static_cast<cLuxProp_Button*>(apProp);

	if(hpl::OhplBackendRustRequested())
	{
		OhplGa4VarsAdapter vars(apInstanceVars);
		OhplGa4Run run(pButton);
		ohpl_a5_button_load_instance(vars.Vars(), &pButton->mS, run.Sink());
		return;
	}
	pButton->SetSwitchedOn(apInstanceVars->GetVarBool("SwitchedOn",false), false);
	pButton->mbCanBeSwitchedOn = apInstanceVars->GetVarBool("CanBeSwitchedOn",true);
	pButton->mbCanBeSwitchedOff = apInstanceVars->GetVarBool("CanBeSwitchedOff",true);
}
//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_Button::cLuxProp_Button(const tString &asName,int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_Button)
{
	mbSwitchedOn = true;
	ohpl_a5_button_init(&mS);
}

//-----------------------------------------------------------------------

cLuxProp_Button::~cLuxProp_Button()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxProp_Button::SetCanBeSwitchedOn(bool abX)
{
	mbCanBeSwitchedOn = abX;
	mS.can_on = abX;
}

//-----------------------------------------------------------------------

void cLuxProp_Button::SetCanBeSwitchedOff(bool abX)
{
	mbCanBeSwitchedOff = abX;
	mS.can_off = abX;
}

//-----------------------------------------------------------------------

bool cLuxProp_Button::CanInteract(iPhysicsBody *apBody)
{
	if(hpl::OhplBackendRustRequested()) return ohpl_a5_button_can_interact(&mS);
	return ohpl_ga4b_button_can_interact(mbSwitchedOn, mbCanBeSwitchedOn, mbCanBeSwitchedOff);
}

//-----------------------------------------------------------------------

bool cLuxProp_Button::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	if(hpl::OhplBackendRustRequested())
	{
		OhplGa4Run run(this);
		ohpl_a5_button_interact(&mS, run.Sink());
		return true;
	}

	SetSwitchedOn(!mbSwitchedOn, true);

	return true;
}

//-----------------------------------------------------------------------

void cLuxProp_Button::OnSetupAfterLoad(cWorld *apWorld)
{

}

//-----------------------------------------------------------------------

void cLuxProp_Button::OnResetProperties()
{

}

//-----------------------------------------------------------------------

void cLuxProp_Button::UpdatePropSpecific(float afTimeStep)
{
}

//-----------------------------------------------------------------------

void cLuxProp_Button::BeforePropDestruction()
{
}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxProp_Button::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	if(CanInteract(apBody) ) return eLuxFocusCrosshair_Grab;

	return eLuxFocusCrosshair_Default;
}

//-----------------------------------------------------------------------

void cLuxProp_Button::SetSwitchedOn(bool abX, bool abUseEffects)
{
	if(hpl::OhplBackendRustRequested())
	{
		OhplGa4Run run(this);
		ohpl_a5_button_set_on(&mS, abX, abUseEffects, run.Sink());
		return;
	}

	int lConnState;
	if(ohpl_ga4b_button_set(&mbSwitchedOn, abX, &lConnState)==false) return;

	ConnectionStateChange(lConnState);

	SetEffectsActive(mbSwitchedOn, abUseEffects);
}

//-----------------------------------------------------------------------

void cLuxProp_Button::Ga4ApplyOp(const OhplGa4Op &aOp)
{
	switch(aOp.code)
	{
	case kOhplA5Op_ConnState:
		ConnectionStateChange(aOp.i);
		break;
	case kOhplA5Op_SetEffects:
		SetEffectsActive(aOp.i != 0, aOp.j != 0);
		break;
	default:
		iLuxProp::Ga4ApplyOp(aOp);
	}
}

//-----------------------------------------------------------------------

void cLuxProp_Button::OnConnectionStateChange(iLuxEntity *apEntity, int alState)
{
	if(hpl::OhplBackendRustRequested())
	{
		OhplGa4Run run(this);
		ohpl_a5_button_conn(&mS, alState, run.Sink());
		return;
	}

	int lSwitch = ohpl_ga4b_button_conn_state(alState);
	if(lSwitch == 1)		SetSwitchedOn(true, true);
	else if(lSwitch == 0)	SetSwitchedOn(false, true);
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

kBeginSerialize(cLuxProp_Button_SaveData, iLuxProp_SaveData)
kSerializeVar(mbSwitchedOn,	eSerializeType_Bool)
kSerializeVar(mbCanBeSwitchedOn,	eSerializeType_Bool)
kSerializeVar(mbCanBeSwitchedOff,	eSerializeType_Bool)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxProp_Button::CreateSaveData()
{
	return hplNew(cLuxProp_Button_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxProp_Button::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxProp_Button_SaveData *pData = static_cast<cLuxProp_Button_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	if(hpl::OhplBackendRustRequested())
	{
		pData->mbSwitchedOn = mS.on;
		pData->mbCanBeSwitchedOn = mS.can_on;
		pData->mbCanBeSwitchedOff = mS.can_off;
		return;
	}
	kCopyToVar(pData,mbSwitchedOn);
	kCopyToVar(pData,mbCanBeSwitchedOn);
	kCopyToVar(pData,mbCanBeSwitchedOff);
}

//-----------------------------------------------------------------------

void cLuxProp_Button::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxProp_Button_SaveData *pData = static_cast<cLuxProp_Button_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	if(hpl::OhplBackendRustRequested())
	{
		mS.on = pData->mbSwitchedOn;
		mS.can_on = pData->mbCanBeSwitchedOn;
		mS.can_off = pData->mbCanBeSwitchedOff;
		return;
	}
	kCopyFromVar(pData,mbSwitchedOn);
	kCopyFromVar(pData,mbCanBeSwitchedOn);
	kCopyFromVar(pData,mbCanBeSwitchedOff);
}

//-----------------------------------------------------------------------

void cLuxProp_Button::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------
