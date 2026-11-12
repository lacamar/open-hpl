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

#include "LuxProp_Chest.h"

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxMessageHandler.h"
#include "LuxCompletionCountHandler.h"
#include "LuxHelpFuncs.h"

#include "LuxMap.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_Chest::cLuxPropLoader_Chest(const tString& asName) : iLuxPropLoader(asName)
{
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_Chest::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxProp_Chest, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Chest::LoadVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxProp_Chest  *pChest = static_cast<cLuxProp_Chest*>(apProp);

	///////////////////////////
	// General
	OhplGa4VarsAdapter vars(this);
	OhplGa4MoveData data;
	ohpl_ga4c_chest_load(vars.Vars(), &data);

	pChest->mSwingDoorData.mfMoveMaxSpeed = data.max_speed;
	pChest->mSwingDoorData.mfMoveSlowDownFactor = data.slow_down_factor;
	pChest->mSwingDoorData.mfMoveSpeedFactor = data.speed_factor;
	pChest->mSwingDoorData.mfMoveThrowImpulse = data.throw_impulse;
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Chest::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{
	cLuxProp_Chest  *pChest = static_cast<cLuxProp_Chest*>(apProp);

	OhplGa4VarsAdapter vars(apInstanceVars);
	pChest->mlCoinsNeeded = ohpl_ga4c_chest_load_instance(vars.Vars());
}
//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// MESSAGE CALLBACK
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxChestMessageCallback::cLuxChestMessageCallback(cLuxProp_Chest *apChest)
{
	mpChest = apChest;
}

//-----------------------------------------------------------------------

void cLuxChestMessageCallback::OnPress(bool abYes)
{
	OhplGa4Run run(mpChest);
	ohpl_ga4c_chest_press(abYes, mpChest->mlCoinsNeeded, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_Chest::cLuxProp_Chest(const tString &asName, int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_Chest)
{
	mbLocked = false;	

	mpMessageCallback = hplNew(cLuxChestMessageCallback, (this));
}

//-----------------------------------------------------------------------

cLuxProp_Chest::~cLuxProp_Chest()
{
	hplDelete(mpMessageCallback);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxProp_Chest::CanInteract(iPhysicsBody *apBody)
{
	return mbLocked;
}

//-----------------------------------------------------------------------

bool cLuxProp_Chest::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	int lAction = ohpl_ga4c_chest_interact(mbLocked, mlCoinsNeeded, gpBase->mpPlayer->GetCoins());
	if(lAction == kOhplGa4cChest_NoInteract) return false;

	tWString sText = kTranslate("Game", "InteractChest01")+_W(" ")+cString::ToStringW(mlCoinsNeeded)+_W(" ")+kTranslate("Game", "InteractChest02")+_W("\n");
	sText += kTranslate("Game", "InteractChest03")+_W(" ")+cString::ToStringW(gpBase->mpPlayer->GetCoins())+_W("\n");

	if(lAction == kOhplGa4cChest_NoCoins)
	{
		sText += kTranslate("Game", "InteractChest03_NoCoins")+_W("\n");
		gpBase->mpMessageHandler->StartPauseMessage(sText,false, NULL);
	}
	else
	{
		sText += kTranslate("Game", "InteractChest03_Question")+_W("\n");
		gpBase->mpMessageHandler->StartPauseMessage(sText,true, mpMessageCallback);
	}
	
	return true;
}

//-----------------------------------------------------------------------

void cLuxProp_Chest::OnSetupAfterLoad(cWorld *apWorld)
{
	////////////////////////////////////
	// Set up joints
	int lNum=0;
	for(size_t i=0; i< mvJoints.size(); ++i)
	{
		iPhysicsJoint *pJoint = mvJoints[i];
		if(pJoint->GetType() != ePhysicsJointType_Hinge) continue;

		iPhysicsJointHinge *pHingeJoint = static_cast<iPhysicsJointHinge*>(pJoint);
		mvJointData.push_back(cLuxChestJointData());

		iPhysicsBody *pChildBody = pJoint->GetChildBody();

		///////////////////////
		//Set update properties
		pHingeJoint->SetStickyMinLimit(true);
		pHingeJoint->SetStickyMaxLimit(true);

		///////////////////////
		//Set up data
		mvJointData[lNum].mpHingeJoint = pHingeJoint;
		mvJointData[lNum].mpChildBody = pChildBody;

		mvJointData[lNum].mfMaxAngle = pHingeJoint->GetMaxAngle();

		++lNum;
	}


	SetLocked(true,false);
}

//-----------------------------------------------------------------------

void cLuxProp_Chest::OnResetProperties()
{

}

//-----------------------------------------------------------------------

void cLuxProp_Chest::UpdatePropSpecific(float afTimeStep)
{
	
}

//-----------------------------------------------------------------------

void cLuxProp_Chest::BeforePropDestruction()
{

}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxProp_Chest::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	if(ohpl_ga4c_chest_crosshair(mbLocked) == kOhplGa4cCrosshair_Grab)
		return eLuxFocusCrosshair_Grab;
	else
		return eLuxFocusCrosshair_Default;
}

//-----------------------------------------------------------------------

tWString cLuxProp_Chest::GetFocusText()
{
	if(mbLocked)
		return	_W("Locked. Costs ")+cString::ToStringW(mlCoinsNeeded)+_W(" Thalers to open.");
	else
		return _W("");
}

//-----------------------------------------------------------------------

void cLuxProp_Chest::ImplementedOnSetActive(bool abX)
{
	
}

//-----------------------------------------------------------------------

void cLuxProp_Chest::OnDamage(float afAmount, int alStrength)
{
	
}

//-----------------------------------------------------------------------

void cLuxProp_Chest::InFocusDraw(cGuiSet *apGuiSet, float afFrameTime)
{
	
}

//-----------------------------------------------------------------------

void cLuxProp_Chest::SetLocked(bool abLocked, bool abEffects)
{
	std::vector<float> vMaxAngles;
	for(size_t i=0; i<mvJointData.size(); ++i) vMaxAngles.push_back(mvJointData[i].mfMaxAngle);

	OhplGa4Run run(this);
	ohpl_ga4c_chest_set_locked(&mbLocked, abLocked, vMaxAngles.empty() ? NULL : &vMaxAngles[0], (int)vMaxAngles.size(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_Chest::Ga4ApplyOp(const OhplGa4Op &aOp)
{
	switch(aOp.code)
	{
	case kOhplGa4cOp_ChestHingeMin:
		mvJointData[aOp.i].mpHingeJoint->SetMinAngle(aOp.f[0]);
		break;
	case kOhplGa4cOp_ChestHingeMax:
		mvJointData[aOp.i].mpHingeJoint->SetMaxAngle(aOp.f[0]);
		break;
	case kOhplGa4cOp_ChestHingeSticky:
		mvJointData[aOp.i].mpHingeJoint->SetStickyMinLimit(false);
		mvJointData[aOp.i].mpHingeJoint->SetStickyMaxLimit(false);
		break;
	case kOhplGa4cOp_ChestAddForce:
		mvJointData[aOp.i].mpChildBody->AddForce(cVector3f(0,1,0));
		break;
	case kOhplGa4cOp_ChestPay:
		gpBase->mpPlayer->AddCoins(aOp.i);
		break;
	case kOhplGa4cOp_ChestUnlock:
		SetLocked(false, true);
		break;
	case kOhplGa4cOp_ChestLog:
		ProgLog(eLuxProgressLogLevel_Medium, "Opened chest "+ msName);
		break;
	case kOhplGa4cOp_ChestCompletion:
		mpMap->AddCompletionAmount(gpBase->mpCompletionCountHandler->mlChestCompletionValue);
		break;
	case kOhplGa4cOp_ChestSound:
		gpBase->mpHelpFuncs->PlayGuiSoundData("gameplay_open_chest", eSoundEntryType_Gui);
		break;
	default:
		iLuxProp::Ga4ApplyOp(aOp);
	}
}

//-----------------------------------------------------------------------

void cLuxProp_Chest::OnConnectionStateChange(iLuxEntity *apEntity, int alState)
{
	SetLocked(ohpl_ga4c_chest_conn_locked(alState), true);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxChestJointData* cLuxProp_Chest::GetJointDataFromBody(iPhysicsBody *apBody)
{
	for(size_t i=0; i<mvJointData.size(); ++i)
	{
		if(mvJointData[i].mpChildBody == apBody) return &mvJointData[i];
	}
	return NULL;
}

//-----------------------------------------------------------------------

cLuxChestJointData* cLuxProp_Chest::GetJointDataFromJoint(iPhysicsJoint *apJoint)
{
	for(size_t i=0; i<mvJointData.size(); ++i)
	{
		if(mvJointData[i].mpHingeJoint == apJoint) return &mvJointData[i];
	}
	return NULL;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxProp_Chest_SaveData, iLuxProp_SaveData)

kSerializeVar(mbLocked, eSerializeType_Bool)

kSerializeVar(mlCoinsNeeded, eSerializeType_Float32)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxProp_Chest::CreateSaveData()
{
	return hplNew(cLuxProp_Chest_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxProp_Chest::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxProp_Chest_SaveData *pData = static_cast<cLuxProp_Chest_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	kCopyToVar(pData, mbLocked);
	kCopyToVar(pData, mlCoinsNeeded);
	
}

//-----------------------------------------------------------------------

void cLuxProp_Chest::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxProp_Chest_SaveData *pData = static_cast<cLuxProp_Chest_SaveData*>(apSaveData);
	
	//////////////////
	//Set variables
	SetLocked(pData->mbLocked, false);
	kCopyFromVar(pData, mlCoinsNeeded);
}

//-----------------------------------------------------------------------

void cLuxProp_Chest::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------
