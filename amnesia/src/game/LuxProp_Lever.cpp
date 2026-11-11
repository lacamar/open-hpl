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

#include "LuxProp_Lever.h"
#include <cstring>

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxMap.h"
#include "LuxHintHandler.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_Lever::cLuxPropLoader_Lever(const tString& asName) : iLuxPropLoader(asName)
{
	mfDefaultMaxFocusDistance = gpBase->mpGameCfg->GetFloat("Player_Interaction","Lever_DefaultMaxFocusDist",0);
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_Lever::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxProp_Lever, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Lever::LoadVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxProp_Lever  *pLever = static_cast<cLuxProp_Lever*>(apProp);

	OhplGa4VarsAdapter vars(this);
	vars.Slot(kOhplGa4Slot_LeverMinSound, pLever->msMinLimitSound)
		.Slot(kOhplGa4Slot_LeverMaxSound, pLever->msMaxLimitSound)
		.Slot(kOhplGa4Slot_LeverStuckSound, pLever->msStuckSound);
	ohpl_ga4_lever_load(vars.Vars(), &pLever->mP);

	pLever->mLeverData.mfMoveMaxSpeed = pLever->mP.data.max_speed;
	pLever->mLeverData.mfMoveSlowDownFactor = pLever->mP.data.slow_down_factor;
	pLever->mLeverData.mfMoveSpeedFactor = pLever->mP.data.speed_factor;
	pLever->mLeverData.mfMoveThrowImpulse = pLever->mP.data.throw_impulse;
}
//-----------------------------------------------------------------------

void cLuxPropLoader_Lever::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{
	cLuxProp_Lever  *pLever = static_cast<cLuxProp_Lever*>(apProp);

	OhplGa4VarsAdapter vars(apInstanceVars);
	OhplGa4Run run(pLever);
	ohpl_ga4_lever_load_instance(vars.Vars(), &pLever->mS, &pLever->mP, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_Lever::cLuxProp_Lever(const tString &asName, int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_Lever)
{
	memset(&mP, 0, sizeof(mP));
	memset(&mS, 0, sizeof(mS));

	mRotatePid.SetErrorNum(10);
	mRotatePid.p = 10.0f;
	mRotatePid.i = 0.0f;
	mRotatePid.d = 1.0f;
}

//-----------------------------------------------------------------------

cLuxProp_Lever::~cLuxProp_Lever()
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxProp_Lever::CanInteract(iPhysicsBody *apBody)
{
	return ohpl_ga4_lever_can_interact(&mP, apBody->GetMass());
}

//-----------------------------------------------------------------------

bool cLuxProp_Lever::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	if(apBody->GetMass()==0 && mP.can_interact_static)
	{
		for(size_t i=0; i<mvBodies.size(); ++i)
		{
			if(mvBodies[i]->GetMass() > 0)
			{
				apBody = mvBodies[i];
				break;
			}
		}
	}

	OhplGa4Run run(this);
	bool bHint = ohpl_ga4_lever_on_interact(&mS, &mP, mbShowHints, run.Sink());
	run.Finish();

	if(bHint)
		gpBase->mpHintHandler->Add("EntityLever", kTranslate("Hints", "EntityLever"), 0);

	cLuxPlayerStateVars::SetupInteraction(apBody, avPos);
	gpBase->mpPlayer->ChangeState(eLuxPlayerState_InteractLever);

	mRotatePid.Reset();

	return true;
}

//-----------------------------------------------------------------------

void cLuxProp_Lever::OnSetupAfterLoad(cWorld *apWorld)
{
	int lNum=0;
	iPhysicsJoint *pJoint = mvJoints[0];
	if(pJoint->GetType() != ePhysicsJointType_Hinge)
	{
		Error("Lever %s does not have a hinge joint!\n", msName.c_str());
		return;
	}

	mpHingeJoint = static_cast<iPhysicsJointHinge*>(pJoint);

	mpLeverBody = mpHingeJoint->GetChildBody();

	if(mP.auto_move_to_angle)
	{
		mpLeverBody->SetGravity(false);
	}

	///////////////////////
	//Set update properties
	OhplGa4Run run(this);
	ohpl_ga4_lever_setup(&mS, &mP, mpHingeJoint->GetMinAngle(), mpHingeJoint->GetMaxAngle(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_Lever::OnResetProperties()
{

}
//-----------------------------------------------------------------------

void cLuxProp_Lever::UpdatePropSpecific(float afTimeStep)
{
	OhplGa4Run run(this);
	ohpl_ga4_lever_update(&mS, &mP, afTimeStep, IsInteractedWith(), run.Host(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_Lever::BeforePropDestruction()
{
	
}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxProp_Lever::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	if(ohpl_ga4_lever_can_interact(&mP, apBody->GetMass())==false) return eLuxFocusCrosshair_Default;

	return eLuxFocusCrosshair_Grab;
}

//-----------------------------------------------------------------------

void cLuxProp_Lever::SetStuckState(int alState, bool abEffects)
{
	OhplGa4Run run(this);
	ohpl_ga4_lever_set_stuck_state(&mS, &mP, alState, abEffects, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_Lever::OnConnectionStateChange(iLuxEntity *apEntity, int alState)
{
	OhplGa4Run run(this);
	ohpl_ga4_lever_on_connection_state(&mS, &mP, alState, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_Lever::Ga4Hinge(int alIdx, OhplGa4Hinge *apOut)
{
	apOut->angle = mpHingeJoint->GetAngle();
	apOut->min_angle = mpHingeJoint->GetMinAngle();
	apOut->max_angle = mpHingeJoint->GetMaxAngle();
	cVector3f vPin = mpHingeJoint->GetPinDir();
	cVector3f vPivot = mpHingeJoint->GetPivotPoint();
	cVector3f vAngVel = mpHingeJoint->GetChildBody()->GetAngularVelocity();
	memcpy(apOut->pin_dir, vPin.v, sizeof(apOut->pin_dir));
	memcpy(apOut->pivot, vPivot.v, sizeof(apOut->pivot));
	memcpy(apOut->child, mpLeverBody->GetLocalMatrix().v, sizeof(apOut->child));
	memcpy(apOut->child_ang_vel, vAngVel.v, sizeof(apOut->child_ang_vel));
}

//-----------------------------------------------------------------------

void cLuxProp_Lever::Ga4ApplyOp(const OhplGa4Op &aOp)
{
	switch(aOp.code)
	{
	case kOhplGa4Op_HingeMin:
		mpHingeJoint->SetMinAngle(aOp.f[0]);
		break;
	case kOhplGa4Op_HingeMax:
		mpHingeJoint->SetMaxAngle(aOp.f[0]);
		break;
	case kOhplGa4Op_HingeSticky:
		mpHingeJoint->SetStickyMinLimit(aOp.j != 0);
		mpHingeJoint->SetStickyMaxLimit(aOp.j != 0);
		break;
	case kOhplGa4Op_BodyAngVel:
		mpLeverBody->SetAngularVelocity(cVector3f(aOp.v[0], aOp.v[1], aOp.v[2]));
		break;
	case kOhplGa4Op_PidTorque:
		{
			cVector3f vTorque = mRotatePid.Output(cVector3f(aOp.v[0], aOp.v[1], aOp.v[2]), aOp.f[0]);
			vTorque = cMath::MatrixMul(mpLeverBody->GetInertiaMatrix(), vTorque);

			mpLeverBody->AddTorque(vTorque);
		}
		break;
	case kOhplGa4Op_PlaySound:
		PlaySound("LeverStuck", msStuckSound, true, true);
		break;
	case kOhplGa4Op_LimitSound:
		if(aOp.j)
		{
			tString sSound = aOp.i == 1 ? msMaxLimitSound : msMinLimitSound;
			if(sSound != "")
			{
				cSoundEntity *pSound = mpMap->GetWorld()->CreateSoundEntity("LeverSound", sSound, true);
				if(pSound) pSound->SetPosition(mpHingeJoint->GetPivotPoint());
			}
		}
		break;
	default:
		iLuxProp::Ga4ApplyOp(aOp);
	}
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxProp_Lever_SaveData, iLuxProp_SaveData)
kSerializeVar(mlCurrentState, eSerializeType_Int32)
kSerializeVar(mlStuckState, eSerializeType_Int32)
kSerializeVar(mbInteractionDisablesStuck, eSerializeType_Bool)
kSerializeVar(mfMiddleAngleAmount, eSerializeType_Float32)
kSerializeVar(mbAutoMoveToAngle, eSerializeType_Bool)
kSerializeVar(mlAutoMoveGoal, eSerializeType_Int32)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxProp_Lever::CreateSaveData()
{
	return hplNew(cLuxProp_Lever_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxProp_Lever::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxProp_Lever_SaveData *pData = static_cast<cLuxProp_Lever_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	pData->mlCurrentState = mS.current_state;
	pData->mlStuckState = mS.stuck_state;
	pData->mbInteractionDisablesStuck = mS.interaction_disables_stuck;
	pData->mfMiddleAngleAmount = mP.middle_angle_amount;
	pData->mbAutoMoveToAngle = mP.auto_move_to_angle;
	pData->mlAutoMoveGoal = mP.auto_move_goal;
}

//-----------------------------------------------------------------------

void cLuxProp_Lever::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxProp_Lever_SaveData *pData = static_cast<cLuxProp_Lever_SaveData*>(apSaveData);
	
	//////////////////
	//Set variables
	mS.current_state = pData->mlCurrentState;
	SetStuckState(pData->mlStuckState, false);
	mS.interaction_disables_stuck = pData->mbInteractionDisablesStuck;
	mP.middle_angle_amount = pData->mfMiddleAngleAmount;
	mP.auto_move_to_angle = pData->mbAutoMoveToAngle;
	mP.auto_move_goal = pData->mlAutoMoveGoal;
}

//-----------------------------------------------------------------------

void cLuxProp_Lever::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{	
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------
