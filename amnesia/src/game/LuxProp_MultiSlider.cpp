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

#include "LuxProp_MultiSlider.h"
#include <cstring>

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxMap.h"
#include "LuxHintHandler.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_MultiSlider::cLuxPropLoader_MultiSlider(const tString& asName) : iLuxPropLoader(asName)
{
	mfDefaultMaxFocusDistance = gpBase->mpGameCfg->GetFloat("Player_Interaction","MultiSlider_DefaultMaxFocusDist",0);
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_MultiSlider::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxProp_MultiSlider, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxPropLoader_MultiSlider::LoadVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxProp_MultiSlider  *pMultiSlider = static_cast<cLuxProp_MultiSlider*>(apProp);

	OhplGa4VarsAdapter vars(this);
	vars.Slot(kOhplGa4Slot_ChangeStateSound, pMultiSlider->msChangeStateSound)
		.Slot(kOhplGa4Slot_MultiStuckSound, pMultiSlider->msStuckSound);
	ohpl_ga4_multi_load(vars.Vars(), &pMultiSlider->mP);

	pMultiSlider->mSlideData.mfSlideMaxSpeed = pMultiSlider->mP.data.max_speed;
	pMultiSlider->mSlideData.mfSlideSlowDownFactor = pMultiSlider->mP.data.slow_down_factor;
	pMultiSlider->mSlideData.mfSlideSpeedFactor = pMultiSlider->mP.data.speed_factor;
	pMultiSlider->mSlideData.mfSlideThrowImpulse = pMultiSlider->mP.data.throw_impulse;

	if(pMultiSlider->mP.few_states)
	{
		Warning("MultiSlider '%s' has fewer than 2 states! It will not function properly!\n", pMultiSlider->GetName().c_str());
	}
}
//-----------------------------------------------------------------------

void cLuxPropLoader_MultiSlider::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{
	cLuxProp_MultiSlider  *pMultiSlider = static_cast<cLuxProp_MultiSlider*>(apProp);

	//The callback is read after the stuck state is set, so that setting it does not run it.
	tString sChangeStateCallback;
	OhplGa4VarsAdapter vars(apInstanceVars);
	vars.Slot(kOhplGa4Slot_ChangeStateCallback, sChangeStateCallback);

	OhplGa4Run run(pMultiSlider);
	ohpl_ga4_multi_load_instance(vars.Vars(), &pMultiSlider->mS, pMultiSlider->mvStates.data(), (int)pMultiSlider->mvStates.size(), run.Sink());
	run.Finish();

	pMultiSlider->msChangeStateCallback = sChangeStateCallback;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_MultiSlider::cLuxProp_MultiSlider(const tString &asName, int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_MultiSlider)
{
	memset(&mP, 0, sizeof(mP));
	ohpl_ga4_multi_state_new(&mS);

	mAutoMovePid.SetErrorNum(10);
	mAutoMovePid.p = 40.0f;
	mAutoMovePid.i = 0.0f;
	mAutoMovePid.d = 0.0f;//Derative fucks up big time.. so skip!

}

//-----------------------------------------------------------------------

cLuxProp_MultiSlider::~cLuxProp_MultiSlider()
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxProp_MultiSlider::CanInteract(iPhysicsBody *apBody)
{
	return ohpl_ga4_multi_can_interact(&mP, apBody->GetMass());
}

//-----------------------------------------------------------------------

bool cLuxProp_MultiSlider::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
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
	ohpl_ga4_multi_on_interact(&mS, mvStates.data(), (int)mvStates.size(), run.Sink());
	run.Finish();

	gpBase->mpHintHandler->Add("EntitySlide", kTranslate("Hints", "EntitySlide"), 0);

	cLuxPlayerStateVars::SetupInteraction(apBody, avPos);
	gpBase->mpPlayer->ChangeState(eLuxPlayerState_InteractSlide);

	mAutoMovePid.Reset();

	return true;
}

//-----------------------------------------------------------------------

void cLuxProp_MultiSlider::OnSetupAfterLoad(cWorld *apWorld)
{
	iPhysicsJoint *pJoint = mvJoints[0];
	if(pJoint->GetType() != ePhysicsJointType_Slider)
	{
		Error("MultiSlider %s does not have a hinge joint!\n", msName.c_str());
		return;
	}

	mpSliderJoint = static_cast<iPhysicsJointSlider*>(pJoint);

	mpSliderBody = mpSliderJoint->GetChildBody();

	///////////////////////
	//Set up data
	mS.default_min_dist = mpSliderJoint->GetMinDistance();
	mS.default_max_dist = mpSliderJoint->GetMaxDistance();

	mvStates.resize(mP.num_of_states > 0 ? mP.num_of_states : 0);
	ohpl_ga4_multi_compute_states(mS.default_min_dist, mS.default_max_dist, mP.num_of_states, mvStates.data());
}

//-----------------------------------------------------------------------

void cLuxProp_MultiSlider::OnResetProperties()
{

}
//-----------------------------------------------------------------------

void cLuxProp_MultiSlider::UpdatePropSpecific(float afTimeStep)
{
	OhplGa4Run run(this);
	ohpl_ga4_multi_update(&mS, &mP, mvStates.data(), (int)mvStates.size(), afTimeStep, IsInteractedWith(), run.Host(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_MultiSlider::BeforePropDestruction()
{
	
}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxProp_MultiSlider::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	if(ohpl_ga4_multi_can_interact(&mP, apBody->GetMass())==false) return eLuxFocusCrosshair_Default;

	return eLuxFocusCrosshair_Grab;
}

//-----------------------------------------------------------------------

void cLuxProp_MultiSlider::SetStuckState(int alState, bool abEffects)
{
	OhplGa4Run run(this);
	ohpl_ga4_multi_set_stuck_state(&mS, mvStates.data(), (int)mvStates.size(), alState, abEffects, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_MultiSlider::OnConnectionStateChange(iLuxEntity *apEntity, int alState)
{
}

//-----------------------------------------------------------------------

void cLuxProp_MultiSlider::Ga4Slider(OhplGa4Slider *apOut)
{
	cVector3f vPin = mpSliderJoint->GetPinDir();
	cVector3f vVel = mpSliderJoint->GetChildBody()->GetLinearVelocity();

	apOut->distance = mpSliderJoint->GetDistance();
	memcpy(apOut->pin_dir, vPin.v, sizeof(apOut->pin_dir));
	memcpy(apOut->child_lin_vel, vVel.v, sizeof(apOut->child_lin_vel));
	apOut->child_mass = mpSliderBody->GetMass();
}

//-----------------------------------------------------------------------

void cLuxProp_MultiSlider::Ga4ApplyOp(const OhplGa4Op &aOp)
{
	switch(aOp.code)
	{
	case kOhplGa4Op_SliderMin:
		mpSliderJoint->SetMinDistance(aOp.f[0]);
		break;
	case kOhplGa4Op_SliderMax:
		mpSliderJoint->SetMaxDistance(aOp.f[0]);
		break;
	case kOhplGa4Op_SliderWake:
		mpSliderBody->Enable();
		break;
	case kOhplGa4Op_SliderForce:
		{
			float fForceSize = mAutoMovePid.Output(aOp.f[0], aOp.f[1]);
			fForceSize *= mpSliderBody->GetMass();

			mpSliderBody->AddForce(cVector3f(aOp.v[0], aOp.v[1], aOp.v[2])*fForceSize);
		}
		break;
	case kOhplGa4Op_PlaySound:
		PlaySound("MultiSliderStuck", msStuckSound, true, true);
		break;
	case kOhplGa4Op_ChangeSound:
		if(aOp.j && msChangeStateSound != "")
		{
			cSoundEntity *pSound = mpMap->GetWorld()->CreateSoundEntity("MultiSliderSound", msChangeStateSound, true);
			if(pSound) pSound->SetPosition(mpSliderBody->GetLocalPosition());
		}
		break;
	case kOhplGa4Op_RunChangeCallback:
		if(msChangeStateCallback!="")
		{
			mpMap->RunScript(msChangeStateCallback + "(\""+msName+"\", "+cString::ToString(aOp.i)+")");
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

kBeginSerialize(cLuxProp_MultiSlider_SaveData, iLuxProp_SaveData)
kSerializeVar(mlCurrentState, eSerializeType_Int32)
kSerializeVar(mlStuckState, eSerializeType_Int32)
kSerializeVar(mbInteractionDisablesStuck, eSerializeType_Bool)
kSerializeVar(msChangeStateCallback, eSerializeType_String)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxProp_MultiSlider::CreateSaveData()
{
	return hplNew(cLuxProp_MultiSlider_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxProp_MultiSlider::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxProp_MultiSlider_SaveData *pData = static_cast<cLuxProp_MultiSlider_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	pData->mlCurrentState = mS.current_state;
	pData->mlStuckState = mS.stuck_state;
	pData->mbInteractionDisablesStuck = mS.interaction_disables_stuck;
	kCopyToVar(pData,msChangeStateCallback);
}

//-----------------------------------------------------------------------

void cLuxProp_MultiSlider::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxProp_MultiSlider_SaveData *pData = static_cast<cLuxProp_MultiSlider_SaveData*>(apSaveData);
	
	//////////////////
	//Set variables
	mS.current_state = pData->mlCurrentState;
	SetStuckState(pData->mlStuckState, false);
	mS.interaction_disables_stuck = pData->mbInteractionDisablesStuck;
	kCopyFromVar(pData,msChangeStateCallback);
}

//-----------------------------------------------------------------------

void cLuxProp_MultiSlider::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{	
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------
