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

#include "LuxProp_Wheel.h"

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxMap.h"
#include "LuxHintHandler.h"

#include <cstring>

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_Wheel::cLuxPropLoader_Wheel(const tString& asName) : iLuxPropLoader(asName)
{
	mfDefaultMaxFocusDistance = gpBase->mpGameCfg->GetFloat("Player_Interaction","Wheel_DefaultMaxFocusDist",0);
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_Wheel::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxProp_Wheel, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Wheel::LoadVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxProp_Wheel  *pWheel = static_cast<cLuxProp_Wheel*>(apProp);

	OhplGa4VarsAdapter vars(this);
	vars.Slot(kOhplGa4Slot_WheelMinSound, pWheel->msMinLimitSound)
		.Slot(kOhplGa4Slot_WheelMaxSound, pWheel->msMaxLimitSound)
		.Slot(kOhplGa4Slot_WheelStuckSound, pWheel->msStuckSound);
	ohpl_ga4_wheel_load(vars.Vars(), &pWheel->mP);

	pWheel->mWheelData.mfMoveMaxSpeed = pWheel->mP.data.max_speed;
	pWheel->mWheelData.mfMoveSlowDownFactor = pWheel->mP.data.slow_down_factor;
	pWheel->mWheelData.mfMoveSpeedFactor = pWheel->mP.data.speed_factor;
	pWheel->mWheelData.mfMoveThrowImpulse = pWheel->mP.data.throw_impulse;
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Wheel::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{
	cLuxProp_Wheel  *pWheel = static_cast<cLuxProp_Wheel*>(apProp);

	OhplGa4VarsAdapter vars(apInstanceVars);
	OhplGa4Run run(pWheel);
	ohpl_ga4_wheel_load_instance(vars.Vars(), &pWheel->mS, &pWheel->mP, run.Host(), run.Sink());
	run.Finish();
}
//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_Wheel::cLuxProp_Wheel(const tString &asName, int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_Wheel)
{
	memset(&mP, 0, sizeof(mP));
	memset(&mS, 0, sizeof(mS));

	mRotatePid.SetErrorNum(10);
	mRotatePid.p = 10.0f;
	mRotatePid.i = 0.0f;
	mRotatePid.d = 0.5f;

}

//-----------------------------------------------------------------------

cLuxProp_Wheel::~cLuxProp_Wheel()
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxProp_Wheel::CanInteract(iPhysicsBody *apBody)
{
	return ohpl_ga4_wheel_can_interact(&mP, apBody->GetMass());
}

//-----------------------------------------------------------------------

bool cLuxProp_Wheel::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	if(apBody->GetMass()==0 && mP.can_interact_static)
	{
		apBody = mpWheelBody;
	}

	OhplGa4Run run(this);
	bool bHint = ohpl_ga4_wheel_on_interact(&mS, &mP, mbShowHints, run.Host(), run.Sink());
	run.Finish();

	if(bHint)
		gpBase->mpHintHandler->Add("EntityWheel", kTranslate("Hints", "EntityWheel"), 0);

	cLuxPlayerStateVars::SetupInteraction(apBody, avPos);
	gpBase->mpPlayer->ChangeState(eLuxPlayerState_InteractWheel);

	return true;
}

//-----------------------------------------------------------------------

void cLuxProp_Wheel::OnResetProperties()
{
	
}


//-----------------------------------------------------------------------

void cLuxProp_Wheel::OnSetupAfterLoad(cWorld *apWorld)
{
	int lNum=0;
	iPhysicsJoint *pJoint = mvJoints[0];
	if(pJoint->GetType() != ePhysicsJointType_Hinge)
	{
		Error("Wheel %s does not have a hinge joint!\n", msName.c_str());
		return;
	}

	mpHingeJoint = static_cast<iPhysicsJointHinge*>(pJoint);

	mpWheelBody = mpHingeJoint->GetChildBody();

	mpWheelBody->SetGravity(false);

	OhplGa4Run run(this);
	ohpl_ga4_wheel_setup(&mP, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_Wheel::UpdatePropSpecific(float afTimeStep)
{
	OhplGa4Run run(this);
	ohpl_ga4_wheel_update(&mS, &mP, afTimeStep, IsInteractedWith(), run.Host(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_Wheel::BeforePropDestruction()
{
	
}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxProp_Wheel::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	if(ohpl_ga4_wheel_can_interact(&mP, apBody->GetMass())==false) return eLuxFocusCrosshair_Default;

	return eLuxFocusCrosshair_Grab;
}

//-----------------------------------------------------------------------

float cLuxProp_Wheel::OnInteractDebugDraw(cGuiSet *apSet,iFontData *apFont, float afStartY)
{
	apSet->DrawFont(apFont,cVector3f(5,afStartY,5),12,cColor(1,1),_W("WheelAngle: %f, Max: %f, Min: %f"),cMath::ToDeg(mS.angle),
						cMath::ToDeg(mP.max_limit),cMath::ToDeg(mP.min_limit));
	afStartY += 13.0f;
	
	return afStartY;
}

//-----------------------------------------------------------------------

void cLuxProp_Wheel::SetStuckState(int alState, bool abEffects)
{
	OhplGa4Run run(this);
	ohpl_ga4_wheel_set_stuck_state(&mS, &mP, alState, abEffects, run.Host(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_Wheel::SetAngle(float afX, bool abAutoMoveToAngle)
{
	OhplGa4Run run(this);
	ohpl_ga4_wheel_set_angle(&mS, &mP, afX, abAutoMoveToAngle, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_Wheel::OnConnectionStateChange(iLuxEntity *apEntity, int alState)
{
	OhplGa4Run run(this);
	ohpl_ga4_wheel_on_connection_state(&mS, &mP, alState, run.Host(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_Wheel::Ga4Hinge(int alIdx, OhplGa4Hinge *apOut)
{
	apOut->angle = mpHingeJoint->GetAngle();
	apOut->min_angle = mpHingeJoint->GetMinAngle();
	apOut->max_angle = mpHingeJoint->GetMaxAngle();
	cVector3f vPin = mpHingeJoint->GetPinDir();
	cVector3f vPivot = mpHingeJoint->GetPivotPoint();
	cVector3f vAngVel = mpWheelBody->GetAngularVelocity();
	memcpy(apOut->pin_dir, vPin.v, sizeof(apOut->pin_dir));
	memcpy(apOut->pivot, vPivot.v, sizeof(apOut->pivot));
	memcpy(apOut->child, mpWheelBody->GetLocalMatrix().v, sizeof(apOut->child));
	memcpy(apOut->child_ang_vel, vAngVel.v, sizeof(apOut->child_ang_vel));
}

//-----------------------------------------------------------------------

void cLuxProp_Wheel::Ga4ApplyOp(const OhplGa4Op &aOp)
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
	case kOhplGa4Op_PidReset:
		mRotatePid.Reset();
		break;
	case kOhplGa4Op_PidTorque:
		{
			cVector3f vTorque = mRotatePid.Output(cVector3f(aOp.v[0], aOp.v[1], aOp.v[2]), aOp.f[0]);
			vTorque = cMath::MatrixMul(mpWheelBody->GetInertiaMatrix(), vTorque);

			mpWheelBody->AddTorque(vTorque);
		}
		break;
	case kOhplGa4Op_PlaySound:
		PlaySound("WheelStuck", msStuckSound, true, true);
		break;
	case kOhplGa4Op_LimitSound:
		if(aOp.j)
		{
			tString sSound = aOp.i == 1 ? msMaxLimitSound : msMinLimitSound;
			if(sSound != "")
			{
				cSoundEntity *pSound = mpMap->GetWorld()->CreateSoundEntity("WheelSound", sSound, true);
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

kBeginSerialize(cLuxProp_Wheel_SaveData, iLuxProp_SaveData)
kSerializeVar(mfAngle, eSerializeType_Float32)
kSerializeVar(mfPrevAngle, eSerializeType_Float32)

kSerializeVar(mfLastToMax, eSerializeType_Float32)
kSerializeVar(mfLastToMin, eSerializeType_Float32)

kSerializeVar(mlCurrentState, eSerializeType_Int32)
kSerializeVar(mlStuckState, eSerializeType_Int32)

kSerializeVar(mfJointAngle, eSerializeType_Float32)
kSerializeVar(mfPrevJointAngle, eSerializeType_Float32)

kSerializeVar(mbAutoMoving, eSerializeType_Bool)
kSerializeVar(mfAutoMoveGoal, eSerializeType_Float32)

kSerializeVar(mfMinLimit, eSerializeType_Float32)
kSerializeVar(mfMaxLimit, eSerializeType_Float32)
kSerializeVar(mbMinLimitStuck, eSerializeType_Bool)
kSerializeVar(mbMaxLimitStuck, eSerializeType_Bool)

kSerializeVar(mlSpinDir, eSerializeType_Int32)

kSerializeVar(mbInteractionDisablesStuck, eSerializeType_Bool)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxProp_Wheel::CreateSaveData()
{
	return hplNew(cLuxProp_Wheel_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxProp_Wheel::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxProp_Wheel_SaveData *pData = static_cast<cLuxProp_Wheel_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	pData->mfAngle = mS.angle;
	pData->mfPrevAngle = mS.prev_angle;

	pData->mfLastToMin = mS.last_to_min;
	pData->mfLastToMax = mS.last_to_max;
	
	pData->mlCurrentState = mS.current_state;
	pData->mlStuckState = mS.stuck_state;

	pData->mfJointAngle = mS.joint_angle;
	pData->mfPrevJointAngle = mS.prev_joint_angle;

	pData->mbAutoMoving = mS.auto_moving;
	pData->mfAutoMoveGoal = mS.auto_move_goal;

	pData->mfMinLimit = mP.min_limit;
	pData->mfMaxLimit = mP.max_limit;
	pData->mbMinLimitStuck = mP.min_limit_stuck;
	pData->mbMaxLimitStuck = mP.max_limit_stuck;

	pData->mlSpinDir = mP.spin_dir;

	pData->mbInteractionDisablesStuck = mS.interaction_disables_stuck;
}

//-----------------------------------------------------------------------

void cLuxProp_Wheel::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxProp_Wheel_SaveData *pData = static_cast<cLuxProp_Wheel_SaveData*>(apSaveData);
	
	//////////////////
	//Set variables
	mS.angle = pData->mfAngle;
	mS.prev_angle = pData->mfPrevAngle;

	mS.last_to_min = pData->mfLastToMin;
	mS.last_to_max = pData->mfLastToMax;

	mS.current_state = pData->mlCurrentState;

	mS.joint_angle = pData->mfJointAngle;
	mS.prev_joint_angle = pData->mfPrevJointAngle;

	mS.auto_moving = pData->mbAutoMoving;
	mS.auto_move_goal = pData->mfAutoMoveGoal;

	mP.min_limit = pData->mfMinLimit;
	mP.max_limit = pData->mfMaxLimit;
	mP.min_limit_stuck = pData->mbMinLimitStuck;
	mP.max_limit_stuck = pData->mbMaxLimitStuck;

	mP.spin_dir = pData->mlSpinDir;
	mS.interaction_disables_stuck = pData->mbInteractionDisablesStuck;

	SetStuckState(pData->mlStuckState, false);
}

//-----------------------------------------------------------------------

void cLuxProp_Wheel::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------
