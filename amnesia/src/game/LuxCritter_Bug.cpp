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

#include "LuxCritter_Bug.h"

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxMap.h"
#include "LuxMapHelper.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_Critter_Bug::cLuxPropLoader_Critter_Bug(const tString& asName) : iLuxPropLoader_Critter(asName)
{
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_Critter_Bug::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxCritter_Bug, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Critter_Bug::LoadCritterVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxCritter_Bug  *pCritter_Bug = static_cast<cLuxCritter_Bug*>(apProp);

	OhplGa4bVars<cResourceVarsObject> vars(this);
	vars.Slot(kOhplGa4bSlot_BugNormalSound, pCritter_Bug->msNormalSound);
	vars.Slot(kOhplGa4bSlot_BugScaredSound, pCritter_Bug->msScaredSound);
	ohpl_ga4b_bug_load(vars.Vars(), &pCritter_Bug->mBP);
	pCritter_Bug->mB.max_speed = pCritter_Bug->mBP.max_speed;
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Critter_Bug::LoadCritterInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxCritter_Bug::cLuxCritter_Bug(const tString &asName,int alID, cLuxMap *apMap) : iLuxProp_CritterBase(asName,alID,apMap)
{
	ohpl_ga4b_bug_state_new(&mB);
}

//-----------------------------------------------------------------------

cLuxCritter_Bug::~cLuxCritter_Bug()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxCritter_Bug::UpdateCritterSpecific(float afTimeStep)
{
	UpdateVelocity(afTimeStep);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxCritter_Bug::UpdateVelocity(float afTimeStep)
{
	cLuxCritterQuery query(this);
	OhplGa4bSink<iLuxProp_CritterBase> sink(this);
	ohpl_ga4b_bug_update_velocity(&mC, &mB, &mBP, query.Query(), sink.Sink(), afTimeStep);
}

void cLuxCritter_Bug::OnGa4bSound(int alKind)
{
	PlaySound("Critter_BugSound", alKind == kOhplGa4bSnd_BugScared ? msScaredSound : msNormalSound, true, true);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxCritter_Bug_SaveData, iLuxProp_CritterBase_SaveData)
kSerializeVar(mfPlaySoundCount, eSerializeType_Float32)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxCritter_Bug::CreateSaveData()
{
	return hplNew(cLuxCritter_Bug_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxCritter_Bug::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxCritter_Bug_SaveData *pData = static_cast<cLuxCritter_Bug_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	pData->mfPlaySoundCount = mB.play_sound_count;
}

//-----------------------------------------------------------------------

void cLuxCritter_Bug::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxCritter_Bug_SaveData *pData = static_cast<cLuxCritter_Bug_SaveData*>(apSaveData);
	
	//////////////////
	//Set variables
	mB.play_sound_count = pData->mfPlaySoundCount;
}

//-----------------------------------------------------------------------

void cLuxCritter_Bug::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------
