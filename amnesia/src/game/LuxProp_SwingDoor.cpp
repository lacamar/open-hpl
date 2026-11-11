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

#include "LuxProp_SwingDoor.h"

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxHintHandler.h"

#include "LuxMap.h"

#include <cstring>

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_SwingDoor::cLuxPropLoader_SwingDoor(const tString& asName) : iLuxPropLoader(asName)
{
	mfDefaultMaxFocusDistance = gpBase->mpGameCfg->GetFloat("Player_Interaction","SwingDoor_DefaultMaxFocusDist",0);
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_SwingDoor::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxProp_SwingDoor, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxPropLoader_SwingDoor::LoadVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxProp_SwingDoor  *pSwingDoor = static_cast<cLuxProp_SwingDoor*>(apProp);

	OhplGa4VarsAdapter vars(this);
	vars.Slot(kOhplGa4Slot_CloseOn, pSwingDoor->msCloseOnSound)
		.Slot(kOhplGa4Slot_CloseOff, pSwingDoor->msCloseOffSound)
		.Slot(kOhplGa4Slot_LockOn, pSwingDoor->msLockOnSound)
		.Slot(kOhplGa4Slot_LockOff, pSwingDoor->msLockOffSound)
		.Slot(kOhplGa4Slot_InteractLocked, pSwingDoor->msInteractLockedSound)
		.Slot(kOhplGa4Slot_DamageMesh1, pSwingDoor->msDamageMesh[0])
		.Slot(kOhplGa4Slot_DamageMesh2, pSwingDoor->msDamageMesh[1])
		.Slot(kOhplGa4Slot_BrokenEntity, pSwingDoor->msBrokenEntity)
		.Slot(kOhplGa4Slot_DamageSound, pSwingDoor->msDamageSound)
		.Slot(kOhplGa4Slot_DamagePs, pSwingDoor->msDamagePS)
		.Slot(kOhplGa4Slot_DoorBreakSound, pSwingDoor->msBreakSound)
		.Slot(kOhplGa4Slot_DoorBreakPs, pSwingDoor->msBreakPS);
	ohpl_ga4_door_load(vars.Vars(), &pSwingDoor->mP);

	pSwingDoor->mSwingDoorData.mfMoveMaxSpeed = pSwingDoor->mP.data.max_speed;
	pSwingDoor->mSwingDoorData.mfMoveSlowDownFactor = pSwingDoor->mP.data.slow_down_factor;
	pSwingDoor->mSwingDoorData.mfMoveSpeedFactor = pSwingDoor->mP.data.speed_factor;
	pSwingDoor->mSwingDoorData.mfMoveThrowImpulse = pSwingDoor->mP.data.throw_impulse;
}

//-----------------------------------------------------------------------

void cLuxPropLoader_SwingDoor::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{
	cLuxProp_SwingDoor  *pSwingDoor = static_cast<cLuxProp_SwingDoor*>(apProp);

	OhplGa4VarsAdapter vars(apInstanceVars);
	std::vector<OhplGa4DoorJoint> vJoints = pSwingDoor->Ga4DoorJoints();

	OhplGa4Run run(pSwingDoor);
	ohpl_ga4_door_load_instance(vars.Vars(), &pSwingDoor->mS, vJoints.empty() ? NULL : &vJoints[0], (int)vJoints.size(),
								(int)pSwingDoor->mvJoints.size(), run.Host(), run.Sink());
	run.Finish();
}
//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_SwingDoor::cLuxProp_SwingDoor(const tString &asName, int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_SwingDoor)
{
	memset(&mP, 0, sizeof(mP));
	ohpl_ga4_door_state_new(&mS);

	for(int i=0; i<3; ++i)
		mpDamageMeshEntity[i] =NULL;
}

//-----------------------------------------------------------------------

cLuxProp_SwingDoor::~cLuxProp_SwingDoor()
{
	for(int i=1; i<3; ++i)
	{
		if(mpDamageMeshEntity[i]) 
			mpWorld->DestroyMeshEntity(mpDamageMeshEntity[i]);
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxProp_SwingDoor::CanInteract(iPhysicsBody *apBody)
{
	return ohpl_ga4_door_can_interact(&mP, apBody->GetMass(), mpMap->BodyIsInDetachableStickyArea(apBody));
}

//-----------------------------------------------------------------------

bool cLuxProp_SwingDoor::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
    if ( mS.is_updating_open_amount ) return true;

	///////////////////////////////
	//If body mass is 0, get a dynamic body
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

	///////////////////////////////
	//Get special swing door data
	cLuxSwingDoorJointData *pData = GetJointDataFromBody(apBody);
	if(pData==NULL) Error("Could not find swing door data for body '%s'\n", apBody->GetName().c_str());

	mpMap->DetachBodyFromStickyArea(apBody);

	std::vector<OhplGa4DoorJoint> vJoints = Ga4DoorJoints();
	OhplGa4Run run(this);
	bool bHint = ohpl_ga4_door_on_interact(&mS, vJoints.empty() ? NULL : &vJoints[0], (int)vJoints.size(), (int)mvJoints.size(), mbShowHints, run.Sink());
	run.Finish();

	if(bHint)
		gpBase->mpHintHandler->Add("EntitySwingDoor", kTranslate("Hints", "EntitySwingDoor"), 0);

	cLuxPlayerStateVars::SetupInteraction(apBody, avPos);
	gpBase->mpPlayer->ChangeState(eLuxPlayerState_InteractSwingDoor);

	return true;
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::OnSetupAfterLoad(cWorld *apWorld)
{
	////////////////////////////////////
	// Set up joints
	int lNum=0;
	for(size_t i=0; i< mvJoints.size(); ++i)
	{
		iPhysicsJoint *pJoint = mvJoints[i];
		if(pJoint->GetType() != ePhysicsJointType_Hinge) continue;

		iPhysicsJointHinge *pHingeJoint = static_cast<iPhysicsJointHinge*>(pJoint);
		mvJointData.push_back(cLuxSwingDoorJointData());

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
		mvJointData[lNum].mfMinAngle = pHingeJoint->GetMinAngle();

		++lNum;
	}

	////////////////////////////////////
	// Get Dyn body (this only works for single doors!)
	iPhysicsBody *pDynBody = NULL;
	for(size_t i=0; i<mvBodies.size(); ++i)
	{
		if(mvBodies[i]->GetMass()!=0){
			pDynBody = mvBodies[i];
			break;
		}
	}

	cMatrixf mtxInvDynBody = pDynBody ? cMath::MatrixInverse(pDynBody->GetLocalMatrix()) : cMatrixf::Identity;
	
	////////////////////////////////////
	// Set up damage mesh entities
	cMeshManager *pMeshManager = gpBase->mpEngine->GetResources()->GetMeshManager();
	mpDamageMeshEntity[0] = mpMeshEntity;
	for(int i=0; i<2;++i)
	{
		if(msDamageMesh[i]=="") continue;
		//Log("DamageMesh: %d\n", i);

		//////////////////////////
		//Create Mesh Entity
		cMesh *pMesh = pMeshManager->CreateMesh(msDamageMesh[i]);
		if(pMesh==NULL)
		{
			Error("Could not load damage mesh '%s' for '%s'\n", msDamageMesh[i].c_str(), msFileName.c_str());
			continue;
		}

		cMeshEntity *pDamageEntity = apWorld->CreateMeshEntity(msName+"damage"+cString::ToString(i+1),pMesh);
		pDamageEntity->SetMatrix(mpMeshEntity->GetLocalMatrix());

		for(int sub=0; sub < pDamageEntity->GetSubMeshEntityNum(); ++sub)
		{
			cSubMeshEntity *pDamSubEnt = pDamageEntity->GetSubMeshEntity(sub);
			int lIdx = mpMeshEntity->GetMesh()->GetSubMeshIndex(pDamSubEnt->GetSubMesh()->GetName());
			//Log(" sub '%s': %d\n", pDamSubEnt->GetName().c_str(), sub);
			if(lIdx < 0)
			{
				//This is not ideal but should be good enough.
				if(pDynBody)
				{
					cMatrixf mtxWorld = cMath::MatrixMul(m_mtxOnLoadTransform, pDamSubEnt->GetLocalMatrix());
					pDamSubEnt->SetMatrix(cMath::MatrixMul(mtxInvDynBody, mtxWorld));

					pDynBody->AddChild(pDamSubEnt);
				}
				continue;
			}
			cSubMeshEntity *pBaseSubEnt = mpMeshEntity->GetSubMeshEntity(lIdx);
			pDamSubEnt->SetMatrix(pBaseSubEnt->GetLocalMatrix());
			if(pBaseSubEnt->GetEntityParent())
			{
				pBaseSubEnt->GetEntityParent()->AddChild(pDamSubEnt);
			}
		}
				
		pDamageEntity->SetVisible(false);
		pDamageEntity->SetActive(false);
		mpDamageMeshEntity[i+1] = pDamageEntity;
	}
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::OnResetProperties()
{
	{
		std::vector<OhplGa4DoorJoint> vJoints = Ga4DoorJoints();
		OhplGa4Run run(this);
		ohpl_ga4_door_reset(&mS, vJoints.empty() ? NULL : &vJoints[0], (int)vJoints.size(), (int)mvJoints.size(), run.Host(), run.Sink());
		run.Finish();
	}

	//////////////////////////////
	// Enable all bodies and meshes
	for(size_t i=0; i<mvBodies.size(); ++i)
	{
		iPhysicsBody *pBody = mvBodies[i];
		if(pBody->GetMass()==0) continue;

		pBody->SetActive(true);

		/////////////////////////////////////////
		// Check what children math mesh entity sub entities
		cEntity3DIterator entIt = pBody->GetChildIterator();
		while(entIt.HasNext())
		{
			iEntity3D *pEntity = entIt.Next();
			for(int j=0; j<mpMeshEntity->GetSubMeshEntityNum(); ++j)
			{
				cSubMeshEntity *pMeshEnt = mpMeshEntity->GetSubMeshEntity(j);
				if((iEntity3D*)pMeshEnt == pEntity)
				{
					pMeshEnt->SetActive(true);
					pMeshEnt->SetVisible(true);
				}
			}
		}
	}
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::UpdatePropSpecific(float afTimeStep)
{
	std::vector<OhplGa4DoorJoint> vJoints = Ga4DoorJoints();
	OhplGa4Run run(this);
	ohpl_ga4_door_update(&mS, vJoints.empty() ? NULL : &vJoints[0], (int)vJoints.size(), (int)mvJoints.size(), afTimeStep, IsInteractedWith(), run.Host(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::BeforePropDestruction()
{

}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxProp_SwingDoor::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	if(CanInteract(apBody)) return eLuxFocusCrosshair_Grab;

	return eLuxFocusCrosshair_Default;
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::ImplementedOnSetActive(bool abX)
{
	for(int i=0; i<3; ++i)
	{
		if(mpDamageMeshEntity[i])
		{
			bool bActive = abX ? mS.current_mesh_entity==i : false;

			mpDamageMeshEntity[i]->SetVisible(bActive);
			mpDamageMeshEntity[i]->SetActive(bActive);
		}
	}
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::OnHealthChange()
{
	bool bHasMain = false;
	for(size_t i=0; i<mvBodies.size(); ++i)
	{
		if(mvBodies[i]->GetMass()!=0) bHasMain = true;
	}

	OhplGa4Run run(this);
	ohpl_ga4_door_on_health_change(&mS, &mP, mfHealth, bHasMain, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::OnDamage(float afAmount, int alStrength)
{
	
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::SetClosed(bool abClosed, bool abEffects)
{
	std::vector<OhplGa4DoorJoint> vJoints = Ga4DoorJoints();
	OhplGa4Run run(this);
	ohpl_ga4_door_set_closed(&mS, vJoints.empty() ? NULL : &vJoints[0], (int)vJoints.size(), (int)mvJoints.size(), abClosed, abEffects, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

int cLuxProp_SwingDoor::GetDoorState()
{
	std::vector<OhplGa4DoorJoint> vJoints = Ga4DoorJoints();
	OhplGa4Run run(this);
	int lState = ohpl_ga4_door_state(vJoints.empty() ? NULL : &vJoints[0], (int)vJoints.size(), run.Host());
	run.Finish();
	return lState;
}

//-----------------------------------------------------------------------


void cLuxProp_SwingDoor::SetLocked(bool abLocked, bool abEffects)
{
	std::vector<OhplGa4DoorJoint> vJoints = Ga4DoorJoints();
	OhplGa4Run run(this);
	ohpl_ga4_door_set_locked(&mS, vJoints.empty() ? NULL : &vJoints[0], (int)vJoints.size(), (int)mvJoints.size(), abLocked, abEffects, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::SetCurrentDamageLevel(int alX)
{
	OhplGa4Run run(this);
	ohpl_ga4_door_set_damage_level(&mS, alX, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

cMeshEntity* cLuxProp_SwingDoor::GetEffectMeshEntity()
{
	if(mS.current_mesh_entity <0) return NULL;

    return mpDamageMeshEntity[mS.current_mesh_entity];
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::OnConnectionStateChange(iLuxEntity *apEntity, int alState)
{
	std::vector<OhplGa4DoorJoint> vJoints = Ga4DoorJoints();
	OhplGa4Run run(this);
	ohpl_ga4_door_on_connection_state(&mS, vJoints.empty() ? NULL : &vJoints[0], (int)vJoints.size(), (int)mvJoints.size(), alState, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------


void cLuxProp_SwingDoor::SetOpenAmount(float afOpenAmount, float afDuration, bool abGoTowardsMaxAngle)
{
	std::vector<OhplGa4DoorJoint> vJoints = Ga4DoorJoints();
	OhplGa4Run run(this);
	ohpl_ga4_door_set_open_amount(&mS, vJoints.empty() ? NULL : &vJoints[0], (int)vJoints.size(), (int)mvJoints.size(), afOpenAmount, afDuration, abGoTowardsMaxAngle, run.Host(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

std::vector<OhplGa4DoorJoint> cLuxProp_SwingDoor::Ga4DoorJoints()
{
	std::vector<OhplGa4DoorJoint> vJoints(mvJointData.size());
	for(size_t i=0; i<mvJointData.size(); ++i)
	{
		vJoints[i].max_angle = mvJointData[i].mfMaxAngle;
		vJoints[i].min_angle = mvJointData[i].mfMinAngle;
	}
	return vJoints;
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::Ga4Hinge(int alIdx, OhplGa4Hinge *apOut)
{
	iPhysicsJointHinge *pHinge = mvJointData[alIdx].mpHingeJoint;
	iPhysicsBody *pChild = mvJointData[alIdx].mpChildBody;

	apOut->angle = pHinge->GetAngle();
	apOut->min_angle = pHinge->GetMinAngle();
	apOut->max_angle = pHinge->GetMaxAngle();
	cVector3f vPin = pHinge->GetPinDir();
	cVector3f vPivot = pHinge->GetPivotPoint();
	cVector3f vAngVel = pChild->GetAngularVelocity();
	memcpy(apOut->pin_dir, vPin.v, sizeof(apOut->pin_dir));
	memcpy(apOut->pivot, vPivot.v, sizeof(apOut->pivot));
	memcpy(apOut->child, pChild->GetLocalMatrix().v, sizeof(apOut->child));
	memcpy(apOut->child_ang_vel, vAngVel.v, sizeof(apOut->child_ang_vel));
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::SetupDoorPhysics(float afOpenAmount, bool abOpenTowardsMaxAngle, bool abApplyAlways)
{
	std::vector<OhplGa4DoorJoint> vJoints = Ga4DoorJoints();
	OhplGa4Run run(this);
	ohpl_ga4_door_setup_physics(&mS, vJoints.empty() ? NULL : &vJoints[0], (int)vJoints.size(), (int)mvJoints.size(), afOpenAmount, abOpenTowardsMaxAngle, abApplyAlways, run.Host(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

static void Ga4Matrix(const float *apM, cMatrixf &a_mtx)
{
	memcpy(a_mtx.v, apM, sizeof(a_mtx.v));
}

void cLuxProp_SwingDoor::Ga4ApplyOp(const OhplGa4Op &aOp)
{
	switch(aOp.code)
	{
	case kOhplGa4Op_HingeMax:
		mvJointData[aOp.i].mpHingeJoint->SetMaxAngle(aOp.f[0]);
		break;
	case kOhplGa4Op_HingeSticky:
		mvJointData[aOp.i].mpHingeJoint->SetStickyMinLimit(aOp.j != 0);
		mvJointData[aOp.i].mpHingeJoint->SetStickyMaxLimit(aOp.j != 0);
		break;
	case kOhplGa4Op_BodiesWake:
		//Add force to all bodies so that the door really closes.
		for(size_t i=0; i<mvBodies.size(); ++i)
		{
			iPhysicsBody *pBody = mvBodies[i];

			pBody->AddForce(cVector3f(1,1,1));
		}
		break;
	case kOhplGa4Op_PlaySound:
		switch(aOp.i)
		{
		case kOhplGa4Snd_CloseOn:		PlaySound("CloseOn",msCloseOnSound,true, true); break;
		case kOhplGa4Snd_CloseOff:		PlaySound("CloseOff",msCloseOffSound,true, true); break;
		case kOhplGa4Snd_LockOn:		PlaySound("LockedOn",msLockOnSound,true, true); break;
		case kOhplGa4Snd_LockOff:		PlaySound("LockedOff",msLockOffSound,true, true); break;
		case kOhplGa4Snd_InteractLocked:PlaySound("InteractLocked",msInteractLockedSound,true, true); break;
		}
		break;
	case kOhplGa4Op_DoorBodyMatrix:
		{
			cMatrixf mtxBody;
			Ga4Matrix(aOp.m, mtxBody);
			mvJointData[aOp.i].mpChildBody->SetMatrix(mtxBody);
		}
		break;
	case kOhplGa4Op_BodyZeroVel:
		mvJointData[aOp.i].mpChildBody->SetAngularVelocity( cVector3f( 0.0f, 0.0f, 0.0f ) );
		mvJointData[aOp.i].mpChildBody->SetLinearVelocity( cVector3f( 0.0f, 0.0f, 0.0f ) );
		break;
	case kOhplGa4Op_DoorDamageLevel:
		for(int i=0; i<3; ++i)
		{
			if(mpDamageMeshEntity[i])
			{
				bool bActive = aOp.i==i;

				mpDamageMeshEntity[i]->SetVisible(bActive);
				mpDamageMeshEntity[i]->SetActive(bActive);
			}
		}
		break;
	case kOhplGa4Op_DoorDisableBodies:
		////////////////////////////
		//Disable all bodies + any children
		for(size_t i=0; i<mvBodies.size(); ++i)
		{
			iPhysicsBody *pBody = mvBodies[i];
			if(pBody->GetMass()==0) continue;

			pBody->SetActive(false);

			/////////////////////////////////////////
			// Check what children math mesh entity sub entities
			cEntity3DIterator entIt = pBody->GetChildIterator();
			while(entIt.HasNext())
			{
				iEntity3D *pEntity = entIt.Next();
				for(int j=0; j<mpMeshEntity->GetSubMeshEntityNum(); ++j)
				{
					cSubMeshEntity *pMeshEnt = mpMeshEntity->GetSubMeshEntity(j);
					if((iEntity3D*)pMeshEnt == pEntity)
					{
						pMeshEnt->SetActive(false);
						pMeshEnt->SetVisible(false);
					}
				}
			}
		}
		break;
	case kOhplGa4Op_DoorBrokenEntity:
		{
			iPhysicsBody *pMainBody = NULL;
			int lMainBodyIdx =-1;
			for(size_t i=0; i<mvBodies.size(); ++i)
			{
				if(mvBodies[i]->GetMass()!=0)
				{
					pMainBody = mvBodies[i];
					lMainBodyIdx = (int)i;
				}
			}

			////////////////////////////
			//Create broken entity
			if(pMainBody && msBrokenEntity != "")
			{
				cEntityBodyExtraData* pBodyData = &mvBodyExtraData[lMainBodyIdx];

				cMatrixf mtxInvLocalBody = cMath::MatrixInverse(pBodyData->m_mtxLocalTransform);
				cMatrixf mtxEntity = cMath::MatrixMul(pMainBody->GetLocalMatrix(), mtxInvLocalBody);

				mpMap->ResetLatestEntity();
				mpMap->CreateEntity(msName + "_broken", msBrokenEntity, mtxEntity, mvOnLoadScale);

				////////////////////////
				//Add impulse and velocity
				iLuxEntity *pEntity = mpMap->GetLatestEntity();
				if(pEntity && pEntity->GetEntityType() == eLuxEntityType_Prop)
				{
					mS.broken_entity_id = pEntity->GetID();

					iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);

					for(int i=0; i< pProp->GetBodyNum(); ++i)
					{
						iPhysicsBody *pNewBody = pProp->GetBody(i);
						cVector3f vNewBodyCenter = cMath::MatrixMul(pNewBody->GetLocalMatrix(), pNewBody->GetMassCentre());

						cVector3f vMainVel = pMainBody->GetLinearVelocity();
						cVector3f vOrigin = mtxEntity.GetTranslation();
						cVector3f vImpulse;
						ohpl_ga4_break_impulse(vNewBodyCenter.v, vOrigin.v, mP.break_impulse, vMainVel.v, vImpulse.v);

						pNewBody->AddImpulse(vImpulse);
					}
				}
			}
		}
		break;
	case kOhplGa4Op_RunBreakCallback:
		RunCallbackFunc("Break");
		break;
	case kOhplGa4Op_DoorDestroyBroken:
		{
			iLuxEntity *pEntity = mpMap->GetEntityByID(aOp.i);
			if(pEntity)
			{
				mpMap->DestroyEntity(pEntity);
			}
		}
		break;
	case kOhplGa4Op_DoorEffects:
		{
			iPhysicsBody *pMainBody = NULL;
			for(size_t i=0; i<mvBodies.size(); ++i)
			{
				if(mvBodies[i]->GetMass()!=0) pMainBody = mvBodies[i];
			}

			const tString &sSound = aOp.i==2 ? msBreakSound : msDamageSound;
			const tString &sPS = aOp.i==2 ? msBreakPS : msDamagePS;

			if(pMainBody)
			{
				///////////////////////
				// Create Sound
				if(sSound != "")
				{
					cSoundEntity *pSound = mpWorld->CreateSoundEntity(msName + "_BreakSound", sSound, true);
					if(pSound) pSound->SetPosition(pMainBody->GetLocalPosition());
				}

				///////////////////////
				// Create Particle System
				if(sPS != "")
				{
					cParticleSystem *pPS = mpWorld->CreateParticleSystem(msName + "_BreakPS", sPS,1);
					if(pPS) pPS->SetMatrix(pMainBody->GetLocalMatrix());
				}
			}
		}
		break;
	default:
		iLuxProp::Ga4ApplyOp(aOp);
	}
}

//-----------------------------------------------------------------------

cLuxSwingDoorJointData* cLuxProp_SwingDoor::GetJointDataFromBody(iPhysicsBody *apBody)
{
	for(size_t i=0; i<mvJointData.size(); ++i)
	{
		if(mvJointData[i].mpChildBody == apBody) return &mvJointData[i];
	}
	return NULL;
}

//-----------------------------------------------------------------------

cLuxSwingDoorJointData* cLuxProp_SwingDoor::GetJointDataFromJoint(iPhysicsJoint *apJoint)
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

kBeginSerialize(cLuxProp_SwingDoor_SaveData, iLuxProp_SaveData)

kSerializeVar(mbLocked, eSerializeType_Bool)
kSerializeVar(mbBroken, eSerializeType_Bool)
kSerializeVar(mbClosed, eSerializeType_Bool)
kSerializeVar(mlBrokenEntityID, eSerializeType_Int32)
kSerializeVar(mlCurrentMeshEntity, eSerializeType_Int32)
kSerializeVar(mbDisableBreakable, eSerializeType_Bool)
kSerializeVar(mbDisableAutoClose, eSerializeType_Bool)

kSerializeVar(mDamageMesh1, eSerializeType_Class)
kSerializeVar(mDamageMesh2, eSerializeType_Class)

kSerializeVar(mfGoalOpenAmount, eSerializeType_Float32)
kSerializeVar(mbOpeningTowardsMaxAngle, eSerializeType_Bool)
kSerializeVar(mfCurrentOpenAmount, eSerializeType_Float32)
kSerializeVar(mfOpenSpeed, eSerializeType_Float32)
kSerializeVar(mbIsUpdatingOpenAmount, eSerializeType_Bool)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxProp_SwingDoor::CreateSaveData()
{
	return hplNew(cLuxProp_SwingDoor_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxProp_SwingDoor_SaveData *pData = static_cast<cLuxProp_SwingDoor_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	pData->mbLocked = mS.locked;
	pData->mbBroken = mS.broken;
	pData->mbClosed = mS.closed;
	pData->mlBrokenEntityID = mS.broken_entity_id;
	pData->mlCurrentMeshEntity = mS.current_mesh_entity;
	pData->mbDisableBreakable = mS.disable_breakable;
	pData->mbDisableAutoClose = mS.disable_auto_close;
	pData->mfGoalOpenAmount = mS.goal_open_amount;
	pData->mbOpeningTowardsMaxAngle = mS.opening_towards_max;
	pData->mfCurrentOpenAmount = mS.current_open_amount;
	pData->mfOpenSpeed = mS.open_speed;
	pData->mbIsUpdatingOpenAmount = mS.is_updating_open_amount;

	if(mpDamageMeshEntity[1])
		pData->mDamageMesh1.FromMeshEntity(mpDamageMeshEntity[1]);
	if(mpDamageMeshEntity[2])
		pData->mDamageMesh2.FromMeshEntity(mpDamageMeshEntity[2]);
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxProp_SwingDoor_SaveData *pData = static_cast<cLuxProp_SwingDoor_SaveData*>(apSaveData);
	
	//////////////////
	//Set variables
	SetClosed(pData->mbClosed, false);
	mS.locked = pData->mbLocked;
	mS.broken = pData->mbBroken;
	mS.broken_entity_id = pData->mlBrokenEntityID;
	mS.current_mesh_entity = pData->mlCurrentMeshEntity;
	mS.disable_breakable = pData->mbDisableBreakable;
	mS.disable_auto_close = pData->mbDisableAutoClose;
	mS.goal_open_amount = pData->mfGoalOpenAmount;
	mS.opening_towards_max = pData->mbOpeningTowardsMaxAngle;
	mS.current_open_amount = pData->mfCurrentOpenAmount;
	mS.open_speed = pData->mfOpenSpeed;
	mS.is_updating_open_amount = pData->mbIsUpdatingOpenAmount;

	if(mpDamageMeshEntity[1])
		pData->mDamageMesh1.ToMeshEntity(mpDamageMeshEntity[1]);
	if(mpDamageMeshEntity[2])
		pData->mDamageMesh2.ToMeshEntity(mpDamageMeshEntity[2]);
}

//-----------------------------------------------------------------------

void cLuxProp_SwingDoor::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------
