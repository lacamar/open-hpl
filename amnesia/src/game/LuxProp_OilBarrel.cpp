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

#include "LuxProp_OilBarrel.h"
#include "RustLuxSaveA8.h"

#include "LuxPlayer.h"
#include "LuxMessageHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxInventory.h"

#include "LuxMap.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_OilBarrel::cLuxPropLoader_OilBarrel(const tString& asName) : iLuxPropLoader(asName)
{
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_OilBarrel::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxProp_OilBarrel, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxPropLoader_OilBarrel::LoadVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxProp_OilBarrel  *pOilBarrel = static_cast<cLuxProp_OilBarrel*>(apProp);

	///////////////////////////
	// General
	//pOilBarrel->mSwingDoorData.mfMoveMaxSpeed = GetVarFloat("MoveMaxSpeed", 13.5f);
	OhplGa4VarsAdapter vars(this);
	vars.Slot(kOhplA5Slot_OilFill, pOilBarrel->msFillSound)
		.Slot(kOhplA5Slot_OilEmpty, pOilBarrel->msEmptySound);
	ohpl_a5_oil_load(vars.Vars());
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_OilBarrel::cLuxProp_OilBarrel(const tString &asName, int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_OilBarrel)
{
	mS.calculated = false;
}

//-----------------------------------------------------------------------

cLuxProp_OilBarrel::~cLuxProp_OilBarrel()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxProp_OilBarrel::CanInteract(iPhysicsBody *apBody)
{
	return true;
}

//-----------------------------------------------------------------------

bool cLuxProp_OilBarrel::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	OhplA5OilHost host = { this, &HostHardMode, &HostLampOil, &HostTotalOil };
	OhplGa4Run run(this);
	return ohpl_a5_oil_interact(&mS, &host, run.Sink());
}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxProp_OilBarrel::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return eLuxFocusCrosshair_Grab;
}

//-----------------------------------------------------------------------

tWString cLuxProp_OilBarrel::GetFocusText()
{
	tWString sText;
	int lTier;
	OhplA5OilHost host = { this, &HostHardMode, &HostLampOil, &HostTotalOil };
	lTier = ohpl_a5_oil_focus_tier(&mS, &host);
	if(lTier == 0)			sText = kTranslate("Game", "OilBarrel_FocusText_Empty");
	else if(lTier == 1)		sText = kTranslate("Game", "OilBarrel_FocusText_025");
	else if(lTier == 2)		sText = kTranslate("Game", "OilBarrel_FocusText_050");
	else if(lTier == 3)		sText = kTranslate("Game", "OilBarrel_FocusText_075");
	else					sText = kTranslate("Game", "OilBarrel_FocusText_100");

	return sText;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxProp_OilBarrel::HostHardMode(void *apCtx)
{
	return gpBase->mbHardMode;
}

//-----------------------------------------------------------------------

float cLuxProp_OilBarrel::HostLampOil(void *apCtx)
{
	return gpBase->mpPlayer->GetLampOil();
}

//-----------------------------------------------------------------------

float cLuxProp_OilBarrel::HostTotalOil(void *apCtx)
{
	float fTotalOil = gpBase->mpPlayer->GetLampOil();
	for(int i=0; i<gpBase->mpInventory->GetItemNum(); ++i)
	{
		cLuxInventory_Item *pItem = gpBase->mpInventory->GetItem(i);
		if(OhplInvType(pItem) != eLuxItemType_LampOil) continue;

		fTotalOil += OhplInvAmount(pItem) * (float)OhplInvCount(pItem);
	}
	return fTotalOil;
}

//-----------------------------------------------------------------------

void cLuxProp_OilBarrel::Ga4ApplyOp(const OhplGa4Op &aOp)
{
	switch(aOp.code)
	{
	case kOhplA5Op_OilSound:
		if(aOp.i == kOhplA5Snd_Empty) PlaySound("EmptyBarell",msEmptySound, true, false);
		else PlaySound("FillLantern",msFillSound, true, false);
		break;
	case kOhplA5Op_OilSetLamp:
		gpBase->mpPlayer->SetLampOil(aOp.f[0]);
		break;
	case kOhplA5Op_OilMessage:
		{
			const char *sEntry = "OilBarrel_Empty";
			switch(aOp.i)
			{
			case kOhplA5Msg_LanternFull:	sEntry = "OilBarrel_LanternFull"; break;
			case kOhplA5Msg_RefillFull:		sEntry = "OilBarrel_Refill_Full"; break;
			case kOhplA5Msg_Refill025:		sEntry = "OilBarrel_Refill_Amount_025"; break;
			case kOhplA5Msg_Refill050:		sEntry = "OilBarrel_Refill_Amount_050"; break;
			case kOhplA5Msg_Refill075:		sEntry = "OilBarrel_Refill_Amount_075"; break;
			case kOhplA5Msg_Refill100:		sEntry = "OilBarrel_Refill_Amount_100"; break;
			}
			gpBase->mpMessageHandler->SetMessage(kTranslate("Game", sEntry), 0);
		}
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

kSerializeRustClassP(cLuxProp_OilBarrel_SaveData, iLuxProp_SaveData)

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxProp_OilBarrel::CreateSaveData()
{
	return hplNew(cLuxProp_OilBarrel_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxProp_OilBarrel::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxProp_OilBarrel_SaveData *pData = static_cast<cLuxProp_OilBarrel_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	ohpl_a8_oil_save(&mS, pData);
}

//-----------------------------------------------------------------------

void cLuxProp_OilBarrel::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxProp_OilBarrel_SaveData *pData = static_cast<cLuxProp_OilBarrel_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	ohpl_a8_oil_load(&mS, pData);
}

//-----------------------------------------------------------------------
