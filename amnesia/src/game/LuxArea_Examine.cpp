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

#include "LuxArea_Examine.h"
#include "RustLuxMap.h"

#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxEffectHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxCompletionCountHandler.h"
#include "LuxMessageHandler.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"


//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxAreaLoader_Examine::cLuxAreaLoader_Examine(const tString& asName) : iLuxAreaLoader(asName)
{

}

cLuxAreaLoader_Examine::~cLuxAreaLoader_Examine()
{

}

//-----------------------------------------------------------------------

iLuxArea *cLuxAreaLoader_Examine::CreateArea(const tString& asName, int alID, cLuxMap *apMap)
{
	cLuxArea_Examine *pArea = hplNew(cLuxArea_Examine, (asName, alID, apMap));
	return pArea;
}

//-----------------------------------------------------------------------

void cLuxAreaLoader_Examine::LoadVariables(iLuxArea *apArea, cWorld *apWorld)
{
	cLuxArea_Examine *pExamineArea = static_cast<cLuxArea_Examine*>(apArea);

	pExamineArea->msDescCat = GetVarString("DescCat","");
	pExamineArea->msDescEntry = GetVarString("DescEntry","");

	pExamineArea->msDescInsaneCat = GetVarString("DescInsaneCat","");
	pExamineArea->msDescInsaneEntry = GetVarString("DescInsaneEntry","");

	pExamineArea->msSound = GetVarString("Sound","");
	pExamineArea->msInsaneSound = GetVarString("InsaneSound","");
}

void cLuxAreaLoader_Examine::SetupArea(iLuxArea *apArea, cWorld *apWorld)
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxArea_Examine::cLuxArea_Examine(const tString &asName, int alID, cLuxMap *apMap)  : iLuxArea(asName,alID,apMap, eLuxAreaType_Examine),
	mfInsaneLimit(Pod<OhplA7Examine>().insaneLimit),
	mfPlaySoundCount(Pod<OhplA7Examine>().playSoundCount)
{
	mfMaxFocusDistance = gpBase->mpGameCfg->GetFloat("Player_Interaction","Examine_MaxFocusDist",0);
	mfInsaneLimit = gpBase->mpGameCfg->GetFloat("Player_Interaction","MaxExamineSanity",0);

	mfPlaySoundCount =0;

}

//-----------------------------------------------------------------------

cLuxArea_Examine::~cLuxArea_Examine()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxArea_Examine::SetupAfterLoad(cWorld *apWorld)
{
	
}

//-----------------------------------------------------------------------

void cLuxArea_Examine::OnUpdate(float afTimeStep)
{
	if(mpA7Area) ohpl_a7_area_tick(mpA7Area, afTimeStep, 0);
	else ohpl_ga5_examine_tick(&mfPlaySoundCount, afTimeStep);
}

//-----------------------------------------------------------------------

bool cLuxArea_Examine::CanInteract(iPhysicsBody *apBody)
{
	if(mpA7Area) return ohpl_a7_area_can_interact(mpA7Area, false);
	return true;
}

//-----------------------------------------------------------------------

bool cLuxArea_Examine::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	float fSanity = gpBase->mpPlayer->GetSanity();

	OhplGa5ExamineIn examineIn = {	msDescCat != "", msDescInsaneCat != "", msDescInsaneEntry != "",
									msSound != "", msInsaneSound != "" };
	OhplGa5ExamineOut examineOut;
	if(mpA7Area) ohpl_a7_area_examine(mpA7Area, &examineIn, fSanity, &examineOut);
	else ohpl_ga5_examine_interact(&examineIn, fSanity, mfInsaneLimit, mfPlaySoundCount, &examineOut);

	////////////////////////////////
	// Show text
	if(examineOut.showText)
	{
		tString sCat = msDescCat;
		tString sEntry = msDescEntry;
		if(examineOut.insaneText)
		{
			sCat = msDescInsaneCat;
			sEntry = msDescInsaneEntry;
		}

		gpBase->mpMessageHandler->SetMessage(kTranslate(sCat, sEntry), 0);
	}

	////////////////////////////////
	// Play sound
	if(examineOut.playSound)
	{
		tString sSound = msSound;
		if(examineOut.insaneSound)
			sSound = msInsaneSound;

		gpBase->mpHelpFuncs->PlayGuiSoundData(sSound, eSoundEntryType_Gui);
	}
	if(mpA7Area == NULL) mfPlaySoundCount = 0.5f;

	return true;
}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxArea_Examine::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	if(mpA7Area)
	{
		const int vTable[4] = { eLuxFocusCrosshair_Default, eLuxFocusCrosshair_Grab, eLuxFocusCrosshair_Ladder, eLuxFocusCrosshair_LastEnum };
		return (eLuxFocusCrosshair)ohpl_a7_area_crosshair(mpA7Area, false, 0, &vTable);
	}
	return eLuxFocusCrosshair_Grab;	
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

kBeginSerialize(cLuxArea_Examine_SaveData, iLuxArea_SaveData)

kSerializeVar(msDescCat, eSerializeType_String)
kSerializeVar(msDescEntry, eSerializeType_String)

kSerializeVar(msDescInsaneCat, eSerializeType_String)
kSerializeVar(msDescInsaneEntry, eSerializeType_String)

kSerializeVar(msSound, eSerializeType_String)
kSerializeVar(msInsaneSound, eSerializeType_String)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxArea* cLuxArea_Examine_SaveData::CreateArea(cLuxMap *apMap)
{
	return hplNew(cLuxArea_Examine, (msName, mlID, apMap));
}

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxArea_Examine::CreateSaveData()
{
	return hplNew(cLuxArea_Examine_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxArea_Examine::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::SaveToSaveData(apSaveData);
	cLuxArea_Examine_SaveData *pData = static_cast<cLuxArea_Examine_SaveData*>(apSaveData);

    kCopyToVar(pData, msDescCat);
	kCopyToVar(pData, msDescEntry);

	kCopyToVar(pData, msDescInsaneCat);
	kCopyToVar(pData, msDescInsaneEntry);

	kCopyToVar(pData, msSound);
	kCopyToVar(pData, msInsaneSound);
}

//-----------------------------------------------------------------------

void cLuxArea_Examine::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::LoadFromSaveData(apSaveData);
	cLuxArea_Examine_SaveData *pData = static_cast<cLuxArea_Examine_SaveData*>(apSaveData);

	kCopyFromVar(pData, msDescCat);
	kCopyFromVar(pData, msDescEntry);

	kCopyFromVar(pData, msDescInsaneCat);
	kCopyFromVar(pData, msDescInsaneEntry);

	kCopyFromVar(pData, msSound);
	kCopyFromVar(pData, msInsaneSound);
}

//-----------------------------------------------------------------------

void cLuxArea_Examine::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);

}

//-----------------------------------------------------------------------

