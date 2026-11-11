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

#include "LuxProp_MoveObject.h"
#include <cstring>

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxMap.h"
#include "LuxArea.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_MoveObject::cLuxPropLoader_MoveObject(const tString& asName) : iLuxPropLoader(asName)
{
	mbForceFullGameSave = true;
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_MoveObject::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxProp_MoveObject, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxPropLoader_MoveObject::LoadVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxProp_MoveObject  *pMoveObject = static_cast<cLuxProp_MoveObject*>(apProp);

	OhplGa4VarsAdapter vars(this);
	ohpl_ga4_move_load(vars.Vars(), &pMoveObject->mP);

	if(pMoveObject->mP.bad_type)
	{
		Error("MoveObjectType '%s' does not exist!\n", GetVarString("MoveObjectType","Linear").c_str());
	}
	if(pMoveObject->mP.bad_axis)
	{
		Error("Axis '%s' does not exist!\n", GetVarString("MoveAxis", "y").c_str());
	}
}

//-----------------------------------------------------------------------

void cLuxPropLoader_MoveObject::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{
	cLuxProp_MoveObject  *pMoveObject = static_cast<cLuxProp_MoveObject*>(apProp);

	OhplGa4VarsAdapter vars(apInstanceVars);
	vars.Slot(kOhplGa4Slot_AngularOffsetArea, pMoveObject->msAngularOffsetArea);
	ohpl_ga4_move_load_instance(vars.Vars());
}
//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_MoveObject::cLuxProp_MoveObject(const tString &asName, int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_MoveObject)
{
	memset(&mP, 0, sizeof(mP));
	memset(&mS, 0, sizeof(mS));
}

//-----------------------------------------------------------------------

cLuxProp_MoveObject::~cLuxProp_MoveObject()
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxProp_MoveObject::CanInteract(iPhysicsBody *apBody)
{
	return msInteractCallback!="";
}

//-----------------------------------------------------------------------

bool cLuxProp_MoveObject::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return msInteractCallback!="";
}

//-----------------------------------------------------------------------

void cLuxProp_MoveObject::OnSetupAfterLoad(cWorld *apWorld)
{
	iPhysicsBody *pBody = GetMainBody();

	cVector3f vLocalMin(0), vLocalMax(0);
	if(mP.move_type == eLuxMoveObjectType_Linear)
	{
		vLocalMin = pBody->GetBoundingVolume()->GetLocalMin();
		vLocalMax = pBody->GetBoundingVolume()->GetLocalMax();
	}

	ohpl_ga4_move_setup(&mS, &mP, pBody->GetLocalMatrix().v, vLocalMin.v, vLocalMax.v);
}

//-----------------------------------------------------------------------

void cLuxProp_MoveObject::OnResetProperties()
{

}
//-----------------------------------------------------------------------

void cLuxProp_MoveObject::UpdatePropSpecific(float afTimeStep)
{
	//Log("Body pos: (%s) Size: (%s)\n", mvBodies[0]->GetWorldPosition().ToString().c_str(), mvBodies[0]->GetShape()->GetSize().ToString().c_str());

	//////////////////////
	//Check the anglular offset area
	if(msAngularOffsetArea != "")
	{
		iLuxArea *pArea = static_cast<iLuxArea*>(mpMap->GetEntityByName(msAngularOffsetArea));
		if(pArea)
		{
			SetAngularOffsetPos(pArea->GetPosition());
		}
		else
		{
			Error("Angular offset area '%s' does not exist!\n", msAngularOffsetArea.c_str());
		}
		msAngularOffsetArea = "";
	}

	
	//////////////////////
	//Update Automove
	UpdateAutoMove(afTimeStep);
	
}

//-----------------------------------------------------------------------

void cLuxProp_MoveObject::BeforePropDestruction()
{

}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxProp_MoveObject::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	if(msInteractCallback != "")
		return eLuxFocusCrosshair_Grab;
	else
		return eLuxFocusCrosshair_Default;
}

//-----------------------------------------------------------------------

void cLuxProp_MoveObject::MoveToState(float afState, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed)
{
	OhplGa4Run run(this);
	ohpl_ga4_move_to_state(&mS, &mP, &mBase, afState, afAcc, afMaxSpeed, afSlowdownDist, abResetSpeed, run.Sink());
	run.Finish();
}

void cLuxProp_MoveObject::MoveToState(float afState)
{
	OhplGa4Run run(this);
	ohpl_ga4_move_to_state_auto(&mS, &mP, &mBase, GetMainBody()->GetLocalMatrix().v, afState, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_MoveObject::SetAngularOffsetPos(const cVector3f& avWorldPos)
{
	ohpl_ga4_move_set_angular_offset(&mS, &mP, avWorldPos.v);
}

//-----------------------------------------------------------------------

float cLuxProp_MoveObject::GetMoveState()
{
	return ohpl_ga4_move_get_state(&mS, &mP, GetMainBody()->GetLocalMatrix().v);
}

//-----------------------------------------------------------------------

void cLuxProp_MoveObject::OnConnectionStateChange(iLuxEntity *apEntity, int alState)
{
	OhplGa4Run run(this);
	ohpl_ga4_move_on_connection_state(&mS, &mP, &mBase, GetMainBody()->GetLocalMatrix().v, alState, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxProp_MoveObject::UpdateAutoMove(float afTimeStep)
{
	if(mP.auto_move==false || mBase.moving || mS.auto_move_reached_goal) return;

	OhplGa4Run run(this);
	ohpl_ga4_move_update_auto_move(&mS, &mP, &mBase, GetMainBody()->GetLocalMatrix().v, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_MoveObject::OnStartMove()
{
	mS.auto_move_reached_goal = false;
}

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxProp_MoveObject_SaveData, iLuxProp_SaveData)

kSerializeVar(mvAngularOffsetPos, eSerializeType_Vector3f)
kSerializeVar(mbUseAngularLocalOffset, eSerializeType_Bool)

kSerializeVar(mbAutoMoveReachedGoal, eSerializeType_Bool)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxProp_MoveObject::CreateSaveData()
{
	return hplNew(cLuxProp_MoveObject_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxProp_MoveObject::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxProp_MoveObject_SaveData *pData = static_cast<cLuxProp_MoveObject_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	pData->mvAngularOffsetPos = cVector3f(mS.angular_offset_pos[0], mS.angular_offset_pos[1], mS.angular_offset_pos[2]);
	pData->mbUseAngularLocalOffset = mS.use_angular_local_offset;
	pData->mbAutoMoveReachedGoal = mS.auto_move_reached_goal;
}

//-----------------------------------------------------------------------

void cLuxProp_MoveObject::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxProp_MoveObject_SaveData *pData = static_cast<cLuxProp_MoveObject_SaveData*>(apSaveData);
	
	//////////////////
	//Set variables
	mS.angular_offset_pos[0] = pData->mvAngularOffsetPos.x; mS.angular_offset_pos[1] = pData->mvAngularOffsetPos.y; mS.angular_offset_pos[2] = pData->mvAngularOffsetPos.z;
	mS.use_angular_local_offset = pData->mbUseAngularLocalOffset;
	mS.auto_move_reached_goal = pData->mbAutoMoveReachedGoal;

	SetAngularOffsetPos(cVector3f(mS.angular_offset_pos[0], mS.angular_offset_pos[1], mS.angular_offset_pos[2]));
}

//-----------------------------------------------------------------------

void cLuxProp_MoveObject::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------
