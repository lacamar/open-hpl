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
#include "RustLuxSaveA8.h"

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

void cLuxPropLoader_Button::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{
    cLuxProp_Button  *pButton = static_cast<cLuxProp_Button*>(apProp);

	OhplGa4VarsAdapter vars(apInstanceVars);
	OhplGa4Run run(pButton);
	ohpl_a5_button_load_instance(vars.Vars(), &pButton->mS, run.Sink());
}
//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_Button::cLuxProp_Button(const tString &asName,int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_Button)
{
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
	mS.can_on = abX;
}

//-----------------------------------------------------------------------

void cLuxProp_Button::SetCanBeSwitchedOff(bool abX)
{
	mS.can_off = abX;
}

//-----------------------------------------------------------------------

bool cLuxProp_Button::CanInteract(iPhysicsBody *apBody)
{
	return ohpl_a5_button_can_interact(&mS);
}

//-----------------------------------------------------------------------

bool cLuxProp_Button::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	OhplGa4Run run(this);
	ohpl_a5_button_interact(&mS, run.Sink());
	return true;
}

//-----------------------------------------------------------------------

void cLuxProp_Button::SetSwitchedOn(bool abX, bool abUseEffects)
{
	OhplGa4Run run(this);
	ohpl_a5_button_set_on(&mS, abX, abUseEffects, run.Sink());
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
	OhplGa4Run run(this);
	ohpl_a5_button_conn(&mS, alState, run.Sink());
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
	ohpl_a8_button_save(&mS, pData);
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
	ohpl_a8_button_load(&mS, pData);
}

//-----------------------------------------------------------------------
