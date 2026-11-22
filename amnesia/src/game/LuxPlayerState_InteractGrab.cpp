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

#include "LuxPlayerState_InteractGrab.h"

#include "LuxPlayer.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxMapHelper.h"
#include "LuxProp.h"
#include "LuxInputHandler.h"

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerState_InteractGrab::cLuxPlayerState_InteractGrab(cLuxPlayer *apPlayer) : iLuxPlayerState_Interact(apPlayer, eLuxPlayerState_InteractGrab, &mR.base)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_init(&mR.base, &h, eLuxPlayerState_InteractGrab);
}

//-----------------------------------------------------------------------

cLuxPlayerState_InteractGrab::~cLuxPlayerState_InteractGrab()
{
	ohpl_a3_free(&mR.base);
	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractGrab::OnEnterState(eLuxPlayerState aPrevState)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_enter(mpA3, &h, aPrevState);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractGrab::OnLeaveState(eLuxPlayerState aNewState)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_leave(mpA3, &h, aNewState);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractGrab::Update(float afTimeStep)
{
	
}

void cLuxPlayerState_InteractGrab::PostUpdate(float afTimeStep)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_post_update(mpA3, &h, afTimeStep);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractGrab::OnDoAction(eLuxPlayerAction aAction,bool abPressed)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_do_action(mpA3, &h, aAction, abPressed);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractGrab::OnScroll(float afAmount)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_scroll(mpA3, &h, afAmount);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractGrab::OnAddYaw(float afAmount)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_add_yaw(mpA3, &h, afAmount);
}

bool cLuxPlayerState_InteractGrab::OnAddPitch(float afAmount)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_add_pitch(mpA3, &h, afAmount);
}

//-----------------------------------------------------------------------

cGuiGfxElement* cLuxPlayerState_InteractGrab::GetCrosshair()
{
	return NULL;
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractGrab::OnSaveBody(iPhysicsBody *apBody, float &afMass, bool &abCollideCharacter)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_save_body(mpA3, apBody, &afMass, &abCollideCharacter);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractGrab::AllowBuoyancy(iPhysicsBody *apBody)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_allow_buoyancy(mpA3, &h, apBody);
}

//-----------------------------------------------------------------------

float cLuxPlayerState_InteractGrab::DrawDebug(cGuiSet *apSet,iFontData *apFont, float afStartY)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_draw_debug(mpA3, &h, apSet, apFont, afStartY);
}


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractGrab::SaveBodyProperties(iPhysicsBody *apBody)
{
	cGrabbedBodyProperties bodyData;

	bodyData.mpBody = apBody;
	bodyData.mbHasGravity = apBody->GetGravity();
	bodyData.mbCollideCharacter = apBody->GetCollideCharacter();
	bodyData.mfMass = apBody->GetMass();

	mvBodyProperties.push_back(bodyData);

    for(int i=0; i<apBody->GetJointNum();++i)
	{
		iPhysicsJoint *pJoint = apBody->GetJoint(i);
		iPhysicsBody *pChild = pJoint->GetChildBody();
		iPhysicsBody *pParent = pJoint->GetParentBody();
		if(pChild && BodyIsAdded(pChild)==false)  SaveBodyProperties(pChild);
		if(pParent && BodyIsAdded(pParent)==false)  SaveBodyProperties(pParent);
	}
}

//-----------------------------------------------------------------------


bool cLuxPlayerState_InteractGrab::BodyIsAdded(iPhysicsBody *apBody)
{
	for(size_t i=0; i<mvBodyProperties.size(); ++i)
	{
		if(mvBodyProperties[i].mpBody == apBody) return true;
	}
	return false;
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxPlayerState_InteractGrab_SaveData, iLuxPlayerState_Interact_SaveData)
kSerializeVar(m_mtxBodyRotation, eSerializeType_Matrixf)
kSerializeVar(mvLocalBodyOffset, eSerializeType_Vector3f)
kSerializeVar(mfDepth, eSerializeType_Float32)
kSerializeVar(mfMaxDistance, eSerializeType_Float32)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxPlayerState_SaveData* cLuxPlayerState_InteractGrab::CreateSaveData()
{
	return hplNew(cLuxPlayerState_InteractGrab_SaveData, ());
}

//-----------------------------------------------------------------------


void cLuxPlayerState_InteractGrab::SaveToSaveData(iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::SaveToSaveData(apSaveData);
	cLuxPlayerState_InteractGrab_SaveData *pData = static_cast<cLuxPlayerState_InteractGrab_SaveData*>(apSaveData);

	
	///////////////////////
	// Save vars
	(pData->m_mtxBodyRotation = mR.bodyRotation);
	(pData->mvLocalBodyOffset = mR.localBodyOffset);
	(pData->mfDepth = mR.depth);
	kCopyToVar(pData, mfMaxDistance);
}

//-----------------------------------------------------------------------


void cLuxPlayerState_InteractGrab::LoadFromSaveDataAfterEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveDataAfterEnter(apMap,apSaveData);
	cLuxPlayerState_InteractGrab_SaveData *pData = static_cast<cLuxPlayerState_InteractGrab_SaveData*>(apSaveData);

	///////////////////////
	// Load vars
	(mR.bodyRotation = pData->m_mtxBodyRotation);
	(mR.localBodyOffset = pData->mvLocalBodyOffset);
	(mR.depth = pData->mfDepth);
	kCopyFromVar(pData, mfMaxDistance);
}

//-----------------------------------------------------------------------
