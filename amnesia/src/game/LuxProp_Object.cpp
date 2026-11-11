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

#include "LuxProp_Object.h"

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxMap.h"
#include "LuxEnemy.h"
#include "LuxProp_Item.h"
#include "LuxHintHandler.h"

#include <cstring>

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_Object::cLuxPropLoader_Object(const tString& asName) : iLuxPropLoader(asName)
{
	mfGrabDefaultMaxFocusDist = gpBase->mpGameCfg->GetFloat("Player_Interaction","Grab_DefaultMaxFocusDist",0);
	mfPushDefaultMaxFocusDist = gpBase->mpGameCfg->GetFloat("Player_Interaction","Push_DefaultMaxFocusDist",0);
	mfSlideDefaultMaxFocusDist = gpBase->mpGameCfg->GetFloat("Player_Interaction","Slide_DefaultMaxFocusDist",0);
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_Object::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxProp_Object, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Object::LoadVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxProp_Object  *pObject = static_cast<cLuxProp_Object*>(apProp);

	float fFocusDefaults[3] = { mfGrabDefaultMaxFocusDist, mfPushDefaultMaxFocusDist, mfSlideDefaultMaxFocusDist };

	OhplGa4VarsAdapter vars(this);
	vars.Slot(kOhplGa4Slot_BreakEntity, pObject->mBreakData.msEntity)
		.Slot(kOhplGa4Slot_BreakAlignBody, pObject->mBreakData.msEntityAlignBody)
		.Slot(kOhplGa4Slot_BreakSound, pObject->mBreakData.msSound)
		.Slot(kOhplGa4Slot_BreakPs, pObject->mBreakData.msParticleSystem);
	ohpl_ga4_object_load(vars.Vars(), (const unsigned char*)msEntitySubType.data(), msEntitySubType.size(),
							&pObject->mfMaxFocusDistance, fFocusDefaults, &pObject->mP);

	const OhplGa4ObjectParams &p = pObject->mP;
	if(p.bad_sub_type)
	{
		Error("Object type '%s' in '%s' does not exist!\n", msEntitySubType.c_str(), msFileName.c_str());
	}

	pObject->mBreakData.mbActive = p.break_active;
	pObject->mBreakData.mbDestroyJoints = p.break_destroy_joints;
	pObject->mBreakData.mfMinEnergy = p.break_min_energy;
	pObject->mBreakData.mfImpulse = p.break_impulse;

	if(p.object_type == eLuxObjectType_Grab)
	{
		pObject->mGrabData.mbGrabUseDepth = p.grab.use_depth;
		pObject->mGrabData.mfGrabDepth = p.grab.depth;
		pObject->mGrabData.mfGrabDepthInc = p.grab.depth_inc;
		pObject->mGrabData.mfGrabMinDepth = p.grab.min_depth;
		pObject->mGrabData.mfGrabMaxDepth = p.grab.max_depth;
		pObject->mGrabData.mvGrabPositionOffset = cVector3f(p.grab.position_offset[0], p.grab.position_offset[1], p.grab.position_offset[2]);
		pObject->mGrabData.mvGrabRotationOffset = cVector3f(p.grab.rotation_offset[0], p.grab.rotation_offset[1], p.grab.rotation_offset[2]);
		pObject->mGrabData.mfGrabThrowImpulse = p.grab.throw_impulse;
		pObject->mGrabData.mbGrabUseOffset = p.grab.use_offset;
		pObject->mGrabData.mfGrabMassMul = p.grab.mass_mul;
		pObject->mGrabData.mfForceMul = p.grab.force_mul;
		pObject->mGrabData.mfTorqueMul = p.grab.torque_mul;
		pObject->mGrabData.mbUseRotation = p.grab.use_rotation;
	}
	else if(p.object_type == eLuxObjectType_Push)
	{
		pObject->mPushData.mbPushAtPoint = p.push.at_point;
		pObject->mPushData.mfPushForceMul = p.push.force_mul;
		pObject->mPushData.mfPushImpulse = p.push.impulse;
	}
	else if(p.object_type == eLuxObjectType_Slide)
	{
		pObject->mSlideData.mfSlideMaxSpeed = p.slide.max_speed;
		pObject->mSlideData.mfSlideSlowDownFactor = p.slide.slow_down_factor;
		pObject->mSlideData.mfSlideSpeedFactor = p.slide.speed_factor;
		pObject->mSlideData.mfSlideThrowImpulse = p.slide.throw_impulse;
	}
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Object::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{
	cLuxProp_Object  *pObject = static_cast<cLuxProp_Object*>(apProp);

	OhplGa4VarsAdapter vars(apInstanceVars);
	vars.Slot(kOhplGa4Slot_ContainedItem, pObject->msContainedItem);

	OhplGa4Run run(pObject);
	ohpl_ga4_object_load_instance(vars.Vars(), &pObject->mS, &pObject->mI, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// COLLISION CALLBACK
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_Object_BodyCallback::cLuxProp_Object_BodyCallback(cLuxProp_Object *apObject)
{
	mpObject = apObject;
}

cLuxProp_Object_BodyCallback::~cLuxProp_Object_BodyCallback()
{

}

//-----------------------------------------------------------------------

bool cLuxProp_Object_BodyCallback::OnAABBCollide(iPhysicsBody *apBody, iPhysicsBody *apCollideBody)
{
	return true;
}

//-----------------------------------------------------------------------

void cLuxProp_Object_BodyCallback::OnBodyCollide(iPhysicsBody *apBody, iPhysicsBody *apCollideBody, cPhysicsContactData* apContactData)
{
	//////////////////////////////////////
	// Check breakage
	if(mpObject->mBreakData.mbActive)
	{

		/////////////////////////////
		// Check energy and see if it breaks.
		OhplGa4ImpactBody vImpact[2];
		iPhysicsBody* vBodies[2] = {apBody, apCollideBody};
		for(int i=0; i<2; ++i)
		{
			iPhysicsBody *pBody = vBodies[i];
			memset(&vImpact[i], 0, sizeof(OhplGa4ImpactBody));
			vImpact[i].mass = pBody->GetMass();
			if(pBody->GetMass()==0) continue;

			cVector3f vBodyCenter = cMath::MatrixMul(pBody->GetLocalMatrix(),pBody->GetMassCentre());
			cVector3f vVelAtImpact = pBody->GetVelocityAtPosition(apContactData->mvContactPosition);
			memcpy(vImpact[i].center, vBodyCenter.v, sizeof(vImpact[i].center));
			memcpy(vImpact[i].vel_at_impact, vVelAtImpact.v, sizeof(vImpact[i].vel_at_impact));
		}

		float fTotalEnergy = ohpl_ga4_object_impact_energy(vImpact, apContactData->mvContactPosition.v, apContactData->mvContactNormal.v);

		if(fTotalEnergy > mpObject->mBreakData.mfMinEnergy)
		{
			mpObject->Break();
		}
	}


	/////////////////////////////
	// Check character and give damage
	if(apCollideBody->IsCharacter())
	{
		float fSpeed = apBody->GetLinearVelocity().Length();
		if(ohpl_ga4_object_hit_hurts(&mpObject->mP, fSpeed))
		{
			if(apCollideBody->GetCharacterBody()->GetUserData())
			{
				iLuxEnemy* pEnemy = (iLuxEnemy*)apCollideBody->GetCharacterBody()->GetUserData();
				pEnemy->GiveDamage(mpObject->mP.hit_damage_amount, mpObject->mP.hit_damage_strength);
			}
		}
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_Object::cLuxProp_Object(const tString &asName,int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_Object)
{
	memset(&mP, 0, sizeof(mP));
	memset(&mI, 0, sizeof(mI));
	memset(&mS, 0, sizeof(mS));

	mpBodyCallback = hplNew(cLuxProp_Object_BodyCallback, (this) );
}

//-----------------------------------------------------------------------

cLuxProp_Object::~cLuxProp_Object()
{
	hplDelete(mpBodyCallback);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------

bool cLuxProp_Object::CanInteract(iPhysicsBody *apBody)
{
	return ohpl_ga4_object_can_interact(&mP, apBody->GetJointNum(), apBody->GetMass(),
										mpMap->BodyIsInDetachableStickyArea(apBody), msInteractCallback!="");
}

//-----------------------------------------------------------------------

bool cLuxProp_Object::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	iPhysicsBody *pBody = mpMainBody ? mpMainBody : apBody;

	mpMap->DetachBodyFromStickyArea(apBody);

	////////////////////////
	// Grab
    if(mP.object_type == eLuxObjectType_Grab)
	{
		gpBase->mpHintHandler->Add("EntityGrab", kTranslate("Hints", "EntityGrab01"), 0);
		gpBase->mpHintHandler->Add("EntityGrab", kTranslate("Hints", "EntityGrab02"), 0);

		cLuxPlayerStateVars::SetupInteraction(pBody, avPos);
		gpBase->mpPlayer->ChangeState(eLuxPlayerState_InteractGrab);
	}
	////////////////////////
	// Push
	if(mP.object_type == eLuxObjectType_Push)
	{
		gpBase->mpHintHandler->Add("EntityPush", kTranslate("Hints", "EntityPush"), 0);

		cLuxPlayerStateVars::SetupInteraction(pBody, avPos);
		gpBase->mpPlayer->ChangeState(eLuxPlayerState_InteractPush);
	}
	////////////////////////
	// Slide
	if(mP.object_type == eLuxObjectType_Slide)
	{
		gpBase->mpHintHandler->Add("EntitySlide", kTranslate("Hints", "EntitySlide"), 0);

		cLuxPlayerStateVars::SetupInteraction(pBody, avPos);
		gpBase->mpPlayer->ChangeState(eLuxPlayerState_InteractSlide);
	}
	
	return true;
}

//-----------------------------------------------------------------------

void cLuxProp_Object::OnSetupAfterLoad(cWorld *apWorld)
{
	/////////////////////
	// Preloading
	cResources *pResources = gpBase->mpEngine->GetResources();
	if(mBreakData.msEntity != "") 
	{
		PreloadEntityModel(mBreakData.msEntity);
	}
	if(mBreakData.msParticleSystem != "")
	{
		gpBase->PreloadParticleSystem(mBreakData.msParticleSystem);	
	}
	if(mBreakData.msSound != "")
	{
		gpBase->PreloadSound(mBreakData.msSound);
	}

	/////////////////////
	// Setup bodies
	for(size_t i=0; i<mvBodies.size(); ++i)
	{
		iPhysicsBody *pBody = mvBodies[i];
		
        pBody->AddBodyCallback(mpBodyCallback);
	}

	/////////////////////
	// Setup joints,
	mvJointData.resize(mvJoints.size());
	for(size_t i=0; i< mvJoints.size(); ++i)
	{
		iPhysicsJoint *pJoint = mvJoints[i];
		OhplGa4JointData *pJointData = &mvJointData[i];

        if(pJoint->GetType() == ePhysicsJointType_Hinge)
		{
			iPhysicsJointHinge *pHingeJoint = static_cast<iPhysicsJointHinge*>(pJoint);
			ohpl_ga4_object_joint_data(0, pHingeJoint->GetMinAngle(), pHingeJoint->GetMaxAngle(), pJointData);
		}
		else if(pJoint->GetType() == ePhysicsJointType_Slider)
		{
			iPhysicsJointSlider *pSliderJoint = static_cast<iPhysicsJointSlider*>(pJoint);
			ohpl_ga4_object_joint_data(1, pSliderJoint->GetMinDistance(), pSliderJoint->GetMaxDistance(), pJointData);
		}
		else if(pJoint->GetType() == ePhysicsJointType_Screw)
		{
			iPhysicsJointScrew *pScrewJoint = static_cast<iPhysicsJointScrew*>(pJoint);
			ohpl_ga4_object_joint_data(2, pScrewJoint->GetMinDistance(), pScrewJoint->GetMaxDistance(), pJointData);
		}
		else
		{
			ohpl_ga4_object_joint_data(-1, 0, 0, pJointData);
		}
	}
}



//-----------------------------------------------------------------------

void cLuxProp_Object::OnResetProperties()
{

}
//-----------------------------------------------------------------------

void cLuxProp_Object::UpdatePropSpecific(float afTimeStep)
{
	OhplGa4ObjectUpdateIn in;
	memset(&in, 0, sizeof(in));
	in.dt = afTimeStep;
	in.has_mesh = mpMeshEntity != NULL;
	in.sanity = (mI.is_insanity_vision && mpMeshEntity) ? gpBase->mpPlayer->GetSanity() : 0;
	in.interacted = IsInteractedWith();

	OhplGa4Run run(this);
	ohpl_ga4_object_update(&mS, &mP, &mI, &in, run.Host(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_Object::BeforePropDestruction()
{
	//////////////////////////////
	// Check if break should happen and init stuff
	if(mS.broken == false || mBreakData.mbActive==false || mvBodies.empty() || mI.disable_breakable)
	{
		return;
	}

	cWorld *pWorld = mpMap->GetWorld();
	cMatrixf mtxCenterTransform = cMatrixf::Identity;

	////////////////////////////////
	// Get the body to use as base for postion
	int lIdx = 0;
	if(mBreakData.msEntityAlignBody != "")
	{
		lIdx = GetBodyIndexFromName(mBreakData.msEntityAlignBody);
		if(lIdx < 0){
			lIdx = 0;
			Warning("Body '%s' was not found in object '%s'", mBreakData.msEntityAlignBody.c_str(), msName.c_str());
		}
	}
	iPhysicsBody *pBaseBody = mvBodies[lIdx];
	cEntityBodyExtraData* pBodyData = &mvBodyExtraData[lIdx];
	mtxCenterTransform = pBaseBody->GetLocalMatrix();

	///////////////////////
	// Destroy joints
	if(mBreakData.mbDestroyJoints)
	{
		//////////////////////
		//Iterate and destroy joints
		for(size_t i=0; i<mvJoints.size(); ++i)
		{
			mvJoints[i]->Break();
			mvJoints[i] = NULL;
		}

		//////////////////////
		//Get center of all bodies
		std::vector<cVector3f> vBodyPositions;
		for(size_t i=0; i<mvBodies.size(); ++i)
		{
			vBodyPositions.push_back(mvBodies[i]->GetLocalPosition());
		}
		cVector3f vBodyCenter;
		ohpl_ga4_bodies_center(&vBodyPositions[0].x, (int)vBodyPositions.size(), vBodyCenter.v);
		mtxCenterTransform = cMath::MatrixTranslate(vBodyCenter);

		//////////////////////
		//Add impulse
		for(size_t i=0; i<mvBodies.size(); ++i)
		{
			iPhysicsBody *pBody = mvBodies[i];
			
			cVector3f vBodyCenter = cMath::MatrixMul(pBody->GetLocalMatrix(), pBody->GetMassCentre());

			cVector3f vImpulse;
			ohpl_ga4_break_impulse(vBodyCenter.v, vBodyCenter.v, mBreakData.mfImpulse, NULL, vImpulse.v);

			pBody->AddImpulse(vImpulse);
		}
	}
	///////////////////////
	// Create Entity
	else 
	{
		

		////////////////////////////
		// Create the entity
		if(mBreakData.msEntity != "")
		{
			// Multiply by inverse local to get the position to center the entity on.
			cMatrixf mtxInvLocalBody = cMath::MatrixInverse(pBodyData->m_mtxLocalTransform);
			cMatrixf mtxEntity = cMath::MatrixMul(pBaseBody->GetLocalMatrix(), mtxInvLocalBody);

			mpMap->ResetLatestEntity();
			mpMap->CreateEntity(msName + "_broken", mBreakData.msEntity, mtxEntity, mvOnLoadScale);		
			
			////////////////////////
			//Add impulse and velocity
			iLuxEntity *pEntity = mpMap->GetLatestEntity();
			if(pEntity && mpMap->EntityExists(pEntity) && pEntity->GetEntityType() == eLuxEntityType_Prop)
			{
				//If the parent has full game save, so must the broken version!
				if(mbFullGameSave)
				{
					pEntity->SetFullGameSave(true);
				}

				iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
				
				for(int i=0; i< pProp->GetBodyNum(); ++i)
				{
					iPhysicsBody *pNewBody = pProp->GetBody(i);
					cVector3f vNewBodyCenter = cMath::MatrixMul(pNewBody->GetLocalMatrix(), pNewBody->GetMassCentre());

					cVector3f vBaseVel = pBaseBody->GetLinearVelocity();
					cVector3f vOrigin = mtxEntity.GetTranslation();
					cVector3f vImpulse;
					ohpl_ga4_break_impulse(vNewBodyCenter.v, vOrigin.v, mBreakData.mfImpulse, vBaseVel.v, vImpulse.v);

					pNewBody->AddImpulse(vImpulse);
				}
			}
		}
	}

	///////////////////////
	// Enable and move connected props
	for(size_t i=0; i<mvConnectedProps.size(); ++i)
	{
		tString sProp = mvConnectedProps[i];
		
		//////////////////////
		//Get Prop
        iLuxEntity *pEntity = mpMap->GetEntityByName(sProp, eLuxEntityType_Prop);
		if(pEntity==NULL)
		{
			Error("Could not find prop '%s' which is connected to '%s'\n", sProp.c_str(), msName.c_str());
			continue;
		}
		iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);

		///////////////////////
		// Enable prop
		pProp->SetActive(true);

		//////////////////////
		//Move all bodies in pro
		for(int i=0; i< pProp->GetBodyNum(); ++i)
		{
			iPhysicsBody *pBody = pProp->GetBody(i);
			cVector3f vNewBodyCenter = cMath::MatrixMul(pBody->GetLocalMatrix(), pBody->GetMassCentre());

			cVector3f vBaseVel = pBaseBody->GetLinearVelocity();
			cVector3f vOrigin = mtxCenterTransform.GetTranslation();
			cVector3f vImpulse;
			ohpl_ga4_break_impulse(vNewBodyCenter.v, vOrigin.v, mBreakData.mfImpulse, vBaseVel.v, vImpulse.v);

			pBody->AddImpulse(vImpulse);
		}
	}

	///////////////////////
	// Create contained item
	if(msContainedItem != "")
	{
		tString sFile = "items/"+ msContainedItem;
		mpMap->ResetLatestEntity();
		mpMap->CreateEntity(msName + "_item", msContainedItem, mtxCenterTransform,1);

		iLuxEntity *pEntity = mpMap->GetLatestEntity();
		if(pEntity && pEntity->GetEntityType() == eLuxEntityType_Prop)
		{
			iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
			if(pProp->GetPropType() == eLuxPropType_Item)
			{
				cLuxProp_Item *pItem = static_cast<cLuxProp_Item*>(pProp);
				pItem->SetSpawnContainerID(GetID());
			}
		}
	}

	///////////////////////
	// Create Sound
	if(mBreakData.msSound != "")
	{
		cSoundEntity *pSound = pWorld->CreateSoundEntity(msName + "_BreakSound", mBreakData.msSound, true);
		if(pSound) pSound->SetPosition(mtxCenterTransform.GetTranslation());
	}

	///////////////////////
	// Create Particle System
	if(mBreakData.msParticleSystem != "")
	{
		cParticleSystem *pPS = pWorld->CreateParticleSystem(msName + "_BreakPS", mBreakData.msParticleSystem,1);
		if(pPS) pPS->SetMatrix(mtxCenterTransform);
	}


	///////////////////////
	// Callback
	RunCallbackFunc("Break");

}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxProp_Object::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	switch(ohpl_ga4_object_focus_crosshair(&mP, CanInteract(apBody), mCustomFocusCrossHair==eLuxFocusCrosshair_Default))
	{
	case 0: return eLuxFocusCrosshair_Default;
	case 1: return eLuxFocusCrosshair_Grab;
	case 2: return eLuxFocusCrosshair_Push;
	case 3: return mCustomFocusCrossHair;
	}
	return eLuxFocusCrosshair_LastEnum;
}

//-----------------------------------------------------------------------

void  cLuxProp_Object::SetStuckState(int alState)
{
	OhplGa4Run run(this);
	ohpl_ga4_object_set_stuck_state(&mS, alState, mvJointData.empty() ? NULL : &mvJointData[0], (int)mvJointData.size(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_Object::Break()
{
	mS.broken = true;
	mpMap->DestroyEntity(this);
}

//-----------------------------------------------------------------------

void cLuxProp_Object::OnHealthChange()
{
	OhplGa4Run run(this);
	ohpl_ga4_object_on_health_change(&mP, mfHealth, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------


void cLuxProp_Object::OnDamage(float afAmount, int alStrength)
{
	
}

//-----------------------------------------------------------------------

bool cLuxProp_Object::ShowOutlinesOnConnectedBodies()
{
	return ohpl_ga4_object_show_outlines(&mP);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxProp_Object::FoodAttractEnemies()
{
	///////////////////////////////////////
	// Iterate enemies and see if any is close to be attracted
	cLuxEnemyIterator enemyIt = mpMap->GetEnemyIterator();
	while(enemyIt.HasNext())
	{
		iLuxEnemy *pEnemy = enemyIt.Next();
		if(pEnemy->IsActive()==false || pEnemy->GetHealth() <= 0) continue;

        if(pEnemy->InRangeOfFood(mvBodies[0])==false) continue;

		pEnemy->SendMessage(eLuxEnemyMessage_FoodInRange, 0, false, mvBodies[0]->GetLocalPosition());
	}
}

//-----------------------------------------------------------------------

void cLuxProp_Object::SetInsanityVisionVisability(bool abX)
{
	mS.insanity_vision_active = abX;

	///////////////
	//Set Bodies and mesh visble
	for(size_t i=0; i<mvBodies.size(); ++i)
	{
		mvBodies[i]->SetActive(abX);
	}

	///////////////////
	//Mesh entity
	if(mpMeshEntity && mbShowMesh)
	{
		mpMeshEntity->SetActive(abX);
		mpMeshEntity->SetVisible(abX);
	}
}

//-----------------------------------------------------------------------

bool cLuxProp_Object::Ga4InsanityInFov()
{
	cCamera *pCam = gpBase->mpPlayer->GetCamera();
	for(int i=0; i<mpMeshEntity->GetSubMeshEntityNum(); ++i)
	{
		cSubMeshEntity *pSubEnt = mpMeshEntity->GetSubMeshEntity(i);

		if(pCam->GetFrustum()->CollideBoundingVolume(pSubEnt->GetBoundingVolume())!=eCollision_Outside)
		{
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------

void cLuxProp_Object::Ga4ApplyOp(const OhplGa4Op &aOp)
{
	switch(aOp.code)
	{
	case kOhplGa4Op_Break:
		Break();
		break;
	case kOhplGa4Op_InsanityVisible:
		SetInsanityVisionVisability(aOp.i != 0);
		break;
	case kOhplGa4Op_FoodAttract:
		FoodAttractEnemies();
		break;
	case kOhplGa4Op_SetJointMinMax:
		SetJointMinMax(aOp.i, aOp.f[0], aOp.f[1]);
		break;
	case kOhplGa4Op_JointWake:
		//If sleeping, make sure it moves!
		if(mvJoints[aOp.i]->GetChildBody())
			mvJoints[aOp.i]->GetChildBody()->AddForce(cVector3f(1,1,1));
		break;
	default:
		iLuxProp::Ga4ApplyOp(aOp);
	}
}

//-----------------------------------------------------------------------

void cLuxProp_Object::SetJointMinMax(int alIdx, float afMin, float afMax)
{
	iPhysicsJoint *pJoint = mvJoints[alIdx];

	if(pJoint->GetType() == ePhysicsJointType_Hinge)
	{
		iPhysicsJointHinge *pHingeJoint = static_cast<iPhysicsJointHinge*>(pJoint);

		pHingeJoint->SetMinAngle(afMin);
		pHingeJoint->SetMaxAngle(afMax);
	}
	else if(pJoint->GetType() == ePhysicsJointType_Slider)
	{
		iPhysicsJointSlider *pSliderJoint = static_cast<iPhysicsJointSlider*>(pJoint);

		pSliderJoint->SetMinDistance(afMin);
		pSliderJoint->SetMaxDistance(afMax);
	}
	else if(pJoint->GetType() == ePhysicsJointType_Screw)
	{
		iPhysicsJointScrew *pScrewJoint = static_cast<iPhysicsJointScrew*>(pJoint);

		pScrewJoint->SetMinDistance(afMin);
		pScrewJoint->SetMaxDistance(afMax);
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxProp_Object_SaveData, iLuxProp_SaveData)
kSerializeVar(mfLifeLengthCount, eSerializeType_Float32)
kSerializeVar(msContainedItem, eSerializeType_String)
kSerializeVar(mlStuckState, eSerializeType_Int32)
kSerializeVar(mfFoodAttractCount, eSerializeType_Float32)
kSerializeVar(mbDisableBreakable, eSerializeType_Bool)
kSerializeVar(mbIsInsanityVision, eSerializeType_Bool)
kSerializeVar(mfVisionMaxSanity, eSerializeType_Float32)
kSerializeVar(mbInsanityVisionActive, eSerializeType_Bool)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxProp_Object::CreateSaveData()
{
	return hplNew(cLuxProp_Object_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxProp_Object::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxProp_Object_SaveData *pData = static_cast<cLuxProp_Object_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	pData->mfLifeLengthCount = mS.life_length_count;
	kCopyToVar(pData, msContainedItem);
	pData->mlStuckState = mS.stuck_state;
	pData->mfFoodAttractCount = mS.food_attract_count;
	pData->mbDisableBreakable = mI.disable_breakable;
	pData->mbIsInsanityVision = mI.is_insanity_vision;
	pData->mfVisionMaxSanity = mI.vision_max_sanity;
	pData->mbInsanityVisionActive = mS.insanity_vision_active;

}

//-----------------------------------------------------------------------

void cLuxProp_Object::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxProp_Object_SaveData *pData = static_cast<cLuxProp_Object_SaveData*>(apSaveData);
	
	//////////////////
	//Set variables
	mS.life_length_count = pData->mfLifeLengthCount;
	kCopyFromVar(pData, msContainedItem);
	SetStuckState(pData->mlStuckState);
	mS.food_attract_count = pData->mfFoodAttractCount;
	mI.disable_breakable = pData->mbDisableBreakable;
	mI.is_insanity_vision = pData->mbIsInsanityVision;
	mI.vision_max_sanity = pData->mfVisionMaxSanity;
	mS.insanity_vision_active = pData->mbInsanityVisionActive;
}

//-----------------------------------------------------------------------

void cLuxProp_Object::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------
