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

#include "LuxSavedEngineTypes.h"
#include "RustLuxSave.h"
#include <vector>

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxMap.h"

//////////////////////////////////////////////////////////////////////////
// CHARACTER BODY
//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------

namespace
{

struct RopeFromEnv
{
	cEngineRope_SaveData *mpData;
	cRopeEntity *mpRope;
	iPhysicsRope *mpPhysicsRope;
	std::vector<cVerletParticle*> mvParts;
};

int RopeFromCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	RopeFromEnv &e = *static_cast<RopeFromEnv*>(apCtx);
	cEngineRope_SaveData &d = *e.mpData;
	iPhysicsRope *pPhysicsRope = e.mpPhysicsRope;
	switch(alOp)
	{
	case eOhplGa6Op_RopeFromBase:
		d.msName = e.mpRope->GetName();
		d.mlUniqueGfxID = e.mpRope->GetUniqueID();
		d.msPhysicsName = pPhysicsRope->GetName();
		d.mlUniquePhysicsID = pPhysicsRope->GetUniqueID();
		d.mvStartPos = pPhysicsRope->GetStartParticle()->GetPosition();
		d.mvEndPos = pPhysicsRope->GetEndParticle()->GetPosition();
		d.msMaterial = cString::To8Char(e.mpRope->GetMaterial()->GetFullPath());
		d.mlMaxSegments = e.mpRope->GetMaxSegments();
		d.msSound = pPhysicsRope->GetMotorSound();
		d.mfSoundStartSpeed = pPhysicsRope->GetMotorSoundStartSpeed();
		d.mfSoundStopSpeed = pPhysicsRope->GetMotorSoundStopSpeed();
		d.mfParticleRadius = pPhysicsRope->GetParticleRadius();
		d.mfRadius = e.mpRope->GetRadius();
		d.mfLengthTileAmount = e.mpRope->GetLengthTileAmount();
		d.mfLengthTileSize = e.mpRope->GetLengthTileSize();
		d.mStartBody = LuxGetIdPairFromBody(pPhysicsRope->GetAttachedStartBody());
		d.mEndBody = LuxGetIdPairFromBody(pPhysicsRope->GetAttachedEndBody());
		d.mvStartBodyLocalPos = pPhysicsRope->GetAttachment(0)->mvBodyLocalPos;
		d.mvEndBodyLocalPos = pPhysicsRope->GetAttachment(1)->mvBodyLocalPos;
		d.mlMaxIterations = pPhysicsRope->GetMaxIterations();
		d.mfTotalLength = pPhysicsRope->GetTotalLength();
		d.mfMinTotalLength = pPhysicsRope->GetMinTotalLength();
		d.mfMaxTotalLength = pPhysicsRope->GetMaxTotalLength();
		d.mfSegmentLength = pPhysicsRope->GetSegmentLength();
		d.mfDamping = pPhysicsRope->GetDamping();
		d.mfStrength = pPhysicsRope->GetStrength();
		d.mfStiffness = pPhysicsRope->GetStiffness();
		d.mbMotorActive = pPhysicsRope->GetMotorActive();
		d.mfMotorWantedLength = pPhysicsRope->GetMotorWantedLength();
		d.mfMotorSpeedMul = pPhysicsRope->GetMotorSpeedMul();
		d.mfMotorMinSpeed = pPhysicsRope->GetMotorMinSpeed();
		d.mfMotorMaxSpeed = pPhysicsRope->GetMotorMaxSpeed();
		d.mbAutoMove = pPhysicsRope->GetAutoMoveActive();
		d.mfAutoMoveSpeed = pPhysicsRope->GetAutoMoveSpeed();
		d.mfAutoMoveAcc = pPhysicsRope->GetAutoMoveAcc();
		d.mfAutoMoveMaxSpeed = pPhysicsRope->GetAutoMoveAcc();
		return 0;
	case eOhplGa6Op_RopeLivePartN:
		if(e.mvParts.empty())
		{
			cVerletParticleIterator partIt = pPhysicsRope->GetParticleIterator();
			while(partIt.HasNext()) e.mvParts.push_back(partIt.Next());
		}
		return (int)e.mvParts.size();
	case eOhplGa6Op_RopePartFrom:
		{
			cEngineVerletParticle_SaveData savePart;
			savePart.FromParticle(e.mvParts[alA]);
			d.mvParticles.Add(savePart);
		}
		return 0;
	}
	return 0;
}

struct RopeToEnv
{
	cEngineRope_SaveData *mpData;
	cRopeEntity *mpRope;
	iPhysicsRope *mpPhysicsRope;
	cLuxMap *mpMap;
	std::vector<cVerletParticle*> mvParts;
	iPhysicsBody *mpStartBody;
	iPhysicsBody *mpEndBody;
};

int RopeToCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	RopeToEnv &e = *static_cast<RopeToEnv*>(apCtx);
	cEngineRope_SaveData &d = *e.mpData;
	iPhysicsRope *pPhysicsRope = e.mpPhysicsRope;
	switch(alOp)
	{
	case eOhplGa6Op_RopeToBase:
		pPhysicsRope->SetMaxIterations(d.mlMaxIterations);
		pPhysicsRope->SetTotalLength(d.mfTotalLength);
		pPhysicsRope->SetMinTotalLength(d.mfMinTotalLength);
		pPhysicsRope->SetMaxTotalLength(d.mfMaxTotalLength);
		pPhysicsRope->SetSegmentLength(d.mfSegmentLength);
		pPhysicsRope->SetDamping(d.mfDamping);
		pPhysicsRope->SetStrength(d.mfStrength);
		pPhysicsRope->SetStiffness(d.mfStiffness);
		pPhysicsRope->SetMotorActive(d.mbMotorActive);
		pPhysicsRope->SetMotorWantedLength(d.mfMotorWantedLength);
		pPhysicsRope->SetMotorSpeedMul(d.mfMotorSpeedMul);
		pPhysicsRope->SetMotorMinSpeed(d.mfMotorMinSpeed);
		pPhysicsRope->SetMotorMaxSpeed(d.mfMotorMaxSpeed);
		pPhysicsRope->SetAutoMoveActive(d.mbAutoMove);
		pPhysicsRope->SetAutoMoveSpeed(d.mfAutoMoveSpeed);
		pPhysicsRope->SetAutoMoveAcc(d.mfAutoMoveAcc);
		pPhysicsRope->SetAutoMoveAcc(d.mfAutoMoveMaxSpeed);
		return 0;
	case eOhplGa6Op_RopeLivePartN:
		if(e.mvParts.empty())
		{
			cVerletParticleIterator partIt = pPhysicsRope->GetParticleIterator();
			while(partIt.HasNext()) e.mvParts.push_back(partIt.Next());
		}
		return (int)e.mvParts.size();
	case eOhplGa6Op_RopeSavedPartN:
		return (int)d.mvParticles.Size();
	case eOhplGa6Op_RopePartTo:
		d.mvParticles[alA].ToParticle(e.mvParts[alA]);
		return 0;
	case eOhplGa6Op_RopeFindStartBody:
		e.mpStartBody = e.mpMap->GetBodyFromEntityBodyIdPair(d.mStartBody);
		return e.mpStartBody != NULL;
	case eOhplGa6Op_RopeAttachStart:
		pPhysicsRope->SetAttachedStartBody(e.mpStartBody);
		pPhysicsRope->GetAttachment(0)->mvBodyLocalPos = d.mvStartBodyLocalPos;
		return 0;
	case eOhplGa6Op_RopeFindEndBody:
		e.mpEndBody = e.mpMap->GetBodyFromEntityBodyIdPair(d.mEndBody);
		return e.mpEndBody != NULL;
	case eOhplGa6Op_RopeAttachEnd:
		pPhysicsRope->SetAttachedEndBody(e.mpEndBody);
		pPhysicsRope->GetAttachment(1)->mvBodyLocalPos = d.mvEndBodyLocalPos;
		return 0;
	}
	return 0;
}

}

void cEngineCharacterBody_SaveData::FromBody(iCharacterBody *apBody)
{
	mbActive = apBody->IsActive();
	mfMass = apBody->GetMass();
	mvPosition = apBody->GetPosition();
	mfYaw = apBody->GetYaw();
	mfPitch = apBody->GetPitch();
	mfSpeedForward = apBody->GetMoveSpeed(eCharDir_Forward);
	mfSpeedRight = apBody->GetMoveSpeed(eCharDir_Right);
	mvForceVelocity = apBody->GetForceVelocity();
	mlActiveSize = apBody->GetActiveSize();
}

//------------------------------------------------------------------------

void cEngineCharacterBody_SaveData::ToBody(iCharacterBody *apBody)
{
	apBody->SetActive(mbActive);
	apBody->SetMass(mfMass);
	apBody->SetPosition(mvPosition);
	apBody->SetYaw(mfYaw);
	apBody->SetPitch(mfPitch);
	apBody->SetMoveSpeed(eCharDir_Forward, mfSpeedForward);
	apBody->SetMoveSpeed(eCharDir_Right, mfSpeedRight);
	apBody->SetForceVelocity(mvForceVelocity);
	apBody->SetActiveSize(mlActiveSize);
}

//------------------------------------------------------------------------

kSerializeRustClass(cEngineCharacterBody_SaveData)

//------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// VERLET PARTICLE
//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------

void cEngineVerletParticle_SaveData::FromParticle(cVerletParticle *apPart)
{
	mvPosition = apPart->GetPosition();
	mvPrevPosition = apPart->GetPrevPosition();
	mvSmoothPosition = apPart->GetSmoothPosition();
	mfInvMass = apPart->GetInvMass();
}

//------------------------------------------------------------------------

void cEngineVerletParticle_SaveData::ToParticle(cVerletParticle *apPart)
{
	apPart->SetPosition(mvPosition, false);
	apPart->SetPrevPosition(mvPrevPosition);
	apPart->SetSmoothPosition(mvSmoothPosition);
	apPart->SetInvMass(mfInvMass);
}

//------------------------------------------------------------------------

kSerializeRustClass(cEngineVerletParticle_SaveData)

//------------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// ROPE
//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------

cRopeEntity* cEngineRope_SaveData::CreateRope(cLuxMap *apMap)
{
	////////////////////
	// Create physics rope
	iPhysicsRope *pPhysicsRope = apMap->GetPhysicsWorld()->CreateRope(msPhysicsName,mvStartPos, mvEndPos);
    pPhysicsRope->SetUniqueID(mlUniquePhysicsID);

	pPhysicsRope->SetParticleRadius(mfParticleRadius);

	pPhysicsRope->SetMotorSound(msSound);
	pPhysicsRope->SetMotorSoundStartSpeed(mfSoundStartSpeed);
	pPhysicsRope->SetMotorSoundStopSpeed(mfSoundStopSpeed);

	////////////////////
	// Create rope entity
    cRopeEntity *pRope = apMap->GetWorld()->CreateRopeEntity(msName,pPhysicsRope, mlMaxSegments);
	pRope->SetUniqueID(mlUniqueGfxID);
	
	pRope->SetRadius(mfRadius);
	pRope->SetLengthTileAmount(mfLengthTileAmount);
	pRope->SetLengthTileSize(mfLengthTileSize);
	
	cMaterial *pMaterial = gpBase->mpEngine->GetResources()->GetMaterialManager()->CreateMaterial(msMaterial);
	if(pMaterial) pRope->SetMaterial(pMaterial);

	return pRope;
}

//------------------------------------------------------------------------

void cEngineRope_SaveData::FromRope(cRopeEntity *apRope)
{
	RopeFromEnv env = { this, apRope, apRope->GetPhysicsRope() };
	ohpl_ga6_rope_from(&env, RopeFromCall);
}

//------------------------------------------------------------------------

void cEngineRope_SaveData::ToRope(cRopeEntity *apRope, cLuxMap *apMap)
{
	RopeToEnv env = { this, apRope, apRope->GetPhysicsRope(), apMap, std::vector<cVerletParticle*>(), NULL, NULL };
	ohpl_ga6_rope_to(&env, RopeToCall);
}

//------------------------------------------------------------------------

kSerializeRustClass(cEngineRope_SaveData)

//------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// BILLBOARD
//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------

void cEngineBillboard_SaveData::FromBillboard(cBillboard *apBillboard)
{
	mColor = apBillboard->GetColor();
}

//------------------------------------------------------------------------

void cEngineBillboard_SaveData::ToBillboard(cBillboard *apBillboard)
{
	apBillboard->SetColor(mColor);
}

//------------------------------------------------------------------------

kSerializeRustClass(cEngineBillboard_SaveData)

//------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// ANIMATION STATE
//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------

void cEngineAnimationState_SaveData::FromAnim(cAnimationState *apAnim)
{
	mbActive = apAnim->IsActive();
	mbLoop = apAnim->IsLooping();

	mfWeight = apAnim->GetWeight();
	mfFadeStep = apAnim->GetFadeStep();
	mfTimePos = apAnim->GetTimePosition();
	mfSpeed = apAnim->GetSpeed();

	//Log("  %d %d %f\n", mbActive, mbLoop, mfTimePos);
}
//------------------------------------------------------------------------

void cEngineAnimationState_SaveData::ToAnim(cAnimationState *apAnim)
{
	apAnim->SetActive(mbActive);
	apAnim->SetLoop(mbLoop);

	apAnim->SetWeight(mfWeight);
	apAnim->SetFadeStep(mfFadeStep);
	apAnim->SetTimePosition(mfTimePos);
	apAnim->SetSpeed(mfSpeed);

	//Log("  %d %d %f\n", mbActive, mbLoop, mfTimePos);
}

//------------------------------------------------------------------------

kSerializeRustClass(cEngineAnimationState_SaveData)
//------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SUB MESH ENTITY
//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------

void cEngineSubMeshEntity_SaveData::FromSubMeshEntity(cSubMeshEntity *apSubMeshEntity)
{
	mbActive = apSubMeshEntity->IsActive();
	mbVisible = apSubMeshEntity->IsVisible();
	mfIlluminationAmount = apSubMeshEntity->GetIlluminationAmount();
	m_mtxTransform = apSubMeshEntity->GetLocalMatrix();
}

//------------------------------------------------------------------------

void cEngineSubMeshEntity_SaveData::ToSubMeshEntity(cSubMeshEntity *apSubMeshEntity)
{
	apSubMeshEntity->SetActive(mbActive);
	apSubMeshEntity->SetVisible(mbVisible);
	apSubMeshEntity->SetIlluminationAmount(mfIlluminationAmount);
	apSubMeshEntity->SetMatrix(m_mtxTransform);
}

//------------------------------------------------------------------------

kSerializeRustClass(cEngineSubMeshEntity_SaveData)
//------------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// MESH ENTITY
//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------

namespace
{

struct MeshEnv
{
	cEngineMeshEntity_SaveData *mpData;
	cMeshEntity *mpMesh;
};

int MeshFromCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	MeshEnv &e = *static_cast<MeshEnv*>(apCtx);
	cEngineMeshEntity_SaveData &d = *e.mpData;
	switch(alOp)
	{
	case eOhplGa6Op_MeshFromBase:
		d.mbActive = e.mpMesh->IsActive();
		d.mbVisible = e.mpMesh->IsVisible();
		d.mfIlluminationAmount = e.mpMesh->GetIlluminationAmount();
		d.m_mtxTransform = e.mpMesh->GetLocalMatrix();
		return 0;
	case eOhplGa6Op_MeshLiveAnimN:
		return e.mpMesh->GetAnimationStateNum();
	case eOhplGa6Op_MeshResizeAnims:
		d.mvAnimations.Resize(alA);
		return 0;
	case eOhplGa6Op_MeshAnimFrom:
		d.mvAnimations[alA].FromAnim(e.mpMesh->GetAnimationState(alA));
		return 0;
	case eOhplGa6Op_MeshLiveSubN:
		return e.mpMesh->GetSubMeshEntityNum();
	case eOhplGa6Op_MeshResizeSubs:
		d.mvSubMeshEntities.Resize(alA);
		return 0;
	case eOhplGa6Op_MeshSubFrom:
		d.mvSubMeshEntities[alA].FromSubMeshEntity(e.mpMesh->GetSubMeshEntity(alA));
		return 0;
	}
	return 0;
}

int MeshToCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	MeshEnv &e = *static_cast<MeshEnv*>(apCtx);
	cEngineMeshEntity_SaveData &d = *e.mpData;
	switch(alOp)
	{
	case eOhplGa6Op_MeshToBase:
		e.mpMesh->SetActive(d.mbActive);
		e.mpMesh->SetVisible(d.mbVisible);
		e.mpMesh->SetIlluminationAmount(d.mfIlluminationAmount);
		e.mpMesh->SetMatrix(d.m_mtxTransform);
		return 0;
	case eOhplGa6Op_MeshSavedAnimN:
		return (int)d.mvAnimations.Size();
	case eOhplGa6Op_MeshLiveAnimN:
		return e.mpMesh->GetAnimationStateNum();
	case eOhplGa6Op_MeshAnimTo:
		d.mvAnimations[alA].ToAnim(e.mpMesh->GetAnimationState(alA));
		return 0;
	case eOhplGa6Op_MeshSavedSubN:
		return (int)d.mvSubMeshEntities.Size();
	case eOhplGa6Op_MeshLiveSubN:
		return e.mpMesh->GetSubMeshEntityNum();
	case eOhplGa6Op_MeshSubTo:
		d.mvSubMeshEntities[alA].ToSubMeshEntity(e.mpMesh->GetSubMeshEntity(alA));
		return 0;
	}
	return 0;
}

}

void cEngineMeshEntity_SaveData::FromMeshEntity(cMeshEntity *apMeshEntity)
{
	MeshEnv env = { this, apMeshEntity };
	ohpl_ga6_mesh_from(&env, MeshFromCall);
}

//------------------------------------------------------------------------

void cEngineMeshEntity_SaveData::ToMeshEntity(cMeshEntity *apMeshEntity)
{
	MeshEnv env = { this, apMeshEntity };
	ohpl_ga6_mesh_to(&env, MeshToCall);
}

//-----------------------------------------------------------------------

kSerializeRustClass(cEngineMeshEntity_SaveData)

//------------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// BODY
//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------

void cEngineBody_SaveData::FromBody(iPhysicsBody *apBody)
{
	mbActive = apBody->IsActive();
	mfMass = apBody->GetMass();
	mbCollideCharacter = apBody->GetCollideCharacter();
	mvLinearVelocity = apBody->GetLinearVelocity();
	mvAngularVelocity = apBody->GetAngularVelocity();
	m_mtxTransform = apBody->GetWorldMatrix();

	//This updates the variables mass and collide characters (that are saved) according to
	// the current state (that might have changed these in some manner)
	gpBase->mpPlayer->GetCurrentStateData()->OnSaveBody(apBody, mfMass, mbCollideCharacter);
}

//------------------------------------------------------------------------

void cEngineBody_SaveData::ToBody(iPhysicsBody *apBody)
{
	apBody->SetActive(mbActive);
	apBody->SetMass(mfMass);
	apBody->SetCollideCharacter(mbCollideCharacter);
	apBody->SetAngularVelocity(mvAngularVelocity);
	apBody->SetLinearVelocity(mvLinearVelocity);
	apBody->SetMatrix(m_mtxTransform);
}

//------------------------------------------------------------------------

kSerializeRustClass(cEngineBody_SaveData)

//------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// JOINT
//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------
namespace
{

struct JointEnv
{
	cEngineJoint_SaveData *mpData;
	iPhysicsJoint *mpJoint;
	iPhysicsWorld *mpWorld;
	iPhysicsController *mpCtrl;
	std::vector<iPhysicsController*> mvCtrls;
};

int JointType(iPhysicsJoint *apJoint)
{
	switch(apJoint->GetType())
	{
	case ePhysicsJointType_Ball: return eOhplGa6Joint_Ball;
	case ePhysicsJointType_Hinge: return eOhplGa6Joint_Hinge;
	case ePhysicsJointType_Screw: return eOhplGa6Joint_Screw;
	case ePhysicsJointType_Slider: return eOhplGa6Joint_Slider;
	}
	return eOhplGa6Joint_Other;
}

int JointFromCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	JointEnv &e = *static_cast<JointEnv*>(apCtx);
	cEngineJoint_SaveData &d = *e.mpData;
	iPhysicsJoint *apJoint = e.mpJoint;
	switch(alOp)
	{
	case eOhplGa6Op_JointExists:
		return apJoint != NULL && e.mpWorld->JointExists(apJoint);
	case eOhplGa6Op_JointSetBroken:
		d.mbBroken = alA != 0;
		return 0;
	case eOhplGa6Op_JointFromBase:
		d.msName = apJoint->GetName();
		d.mlID = apJoint->GetUniqueID();
		d.mbBroken = false;
		return 0;
	case eOhplGa6Op_JointLiveCtrlN:
		{
			cPhysicsControllerIterator ctrlIt = apJoint->GetControllerIterator();
			while(ctrlIt.HasNext()) e.mvCtrls.push_back(ctrlIt.Next());
			return (int)e.mvCtrls.size();
		}
	case eOhplGa6Op_JointCtrlFrom:
		{
			iPhysicsController *pCtrl = e.mvCtrls[alA];
			cEngineJointController_SaveData saveCtrl;
			saveCtrl.msName = pCtrl->GetName();
			saveCtrl.mfDestValue = pCtrl->GetDestValue();
			saveCtrl.mbActive = pCtrl->IsActive();
			d.mvControllers.Add(saveCtrl);
		}
		return 0;
	case eOhplGa6Op_JointType:
		return JointType(apJoint);
	case eOhplGa6Op_JointFromLimits:
		switch(alA)
		{
		case eOhplGa6Joint_Ball:
			{
				iPhysicsJointBall *pBallJoint = static_cast<iPhysicsJointBall*>(apJoint);
				d.mfMaxLimit = pBallJoint->GetMaxConeAngle();
				d.mfMinLimit = pBallJoint->GetMaxTwistAngle();
				break;
			}
		case eOhplGa6Joint_Hinge:
			{
				iPhysicsJointHinge *pHingeJoint = static_cast<iPhysicsJointHinge*>(apJoint);
				d.mfMaxLimit = pHingeJoint->GetMaxAngle();
				d.mfMinLimit = pHingeJoint->GetMinAngle();
				break;
			}
		case eOhplGa6Joint_Screw:
			{
				iPhysicsJointScrew *pScrewJoint = static_cast<iPhysicsJointScrew*>(apJoint);
				d.mfMinLimit = pScrewJoint->GetMinDistance();
				d.mfMaxLimit = pScrewJoint->GetMaxDistance();
				break;
			}
		case eOhplGa6Joint_Slider:
			{
				iPhysicsJointSlider *pSliderJoint = static_cast<iPhysicsJointSlider*>(apJoint);
				d.mfMinLimit = pSliderJoint->GetMinDistance();
				d.mfMaxLimit = pSliderJoint->GetMaxDistance();
				break;
			}
		}
		return 0;
	}
	return 0;
}

int JointToCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	JointEnv &e = *static_cast<JointEnv*>(apCtx);
	cEngineJoint_SaveData &d = *e.mpData;
	iPhysicsJoint *apJoint = e.mpJoint;
	switch(alOp)
	{
	case eOhplGa6Op_JointSavedCtrlN:
		return (int)d.mvControllers.Size();
	case eOhplGa6Op_JointFindCtrl:
		e.mpCtrl = apJoint->GetController(d.mvControllers[alA].msName);
		return e.mpCtrl != NULL;
	case eOhplGa6Op_JointCtrlTo:
		e.mpCtrl->SetDestValue(d.mvControllers[alA].mfDestValue);
		e.mpCtrl->SetActive(d.mvControllers[alA].mbActive);
		return 0;
	case eOhplGa6Op_JointType:
		return JointType(apJoint);
	case eOhplGa6Op_JointToLimits:
		switch(alA)
		{
		case eOhplGa6Joint_Ball:
			{
				iPhysicsJointBall *pBallJoint = static_cast<iPhysicsJointBall*>(apJoint);
				pBallJoint->SetConeLimits(d.mfMaxLimit,d.mfMinLimit);
				break;
			}
		case eOhplGa6Joint_Hinge:
			{
				iPhysicsJointHinge *pHingeJoint = static_cast<iPhysicsJointHinge*>(apJoint);
				pHingeJoint->SetMaxAngle(d.mfMaxLimit);
				pHingeJoint->SetMinAngle(d.mfMinLimit);
				break;
			}
		case eOhplGa6Joint_Screw:
			{
				iPhysicsJointScrew *pScrewJoint = static_cast<iPhysicsJointScrew*>(apJoint);
				pScrewJoint->SetMinDistance(d.mfMinLimit);
				pScrewJoint->SetMaxDistance(d.mfMaxLimit);
				break;
			}
		case eOhplGa6Joint_Slider:
			{
				iPhysicsJointSlider *pSliderJoint = static_cast<iPhysicsJointSlider*>(apJoint);
				pSliderJoint->SetMinDistance(d.mfMinLimit);
				pSliderJoint->SetMaxDistance(d.mfMaxLimit);
				break;
			}
		}
		return 0;
	}
	return 0;
}

}

void cEngineJoint_SaveData::FromJoint(iPhysicsJoint *apJoint, iPhysicsWorld *apPhysicsWorld)
{
	JointEnv env = { this, apJoint, apPhysicsWorld, NULL, std::vector<iPhysicsController*>() };
	ohpl_ga6_joint_from(&env, JointFromCall);
}

//------------------------------------------------------------------------

void cEngineJoint_SaveData::ToJoint(iPhysicsJoint *apJoint)
{
	JointEnv env = { this, apJoint, NULL, NULL, std::vector<iPhysicsController*>() };
	ohpl_ga6_joint_to(&env, JointToCall);
}

//------------------------------------------------------------------------

kSerializeRustClass(cEngineJointController_SaveData)

kSerializeRustClass(cEngineJoint_SaveData)
//------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PS
//////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------
namespace
{

struct PSEnv
{
	cEnginePS_SaveData *mpData;
	cParticleSystem *mpPS;
};

int PSFromCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	PSEnv &e = *static_cast<PSEnv*>(apCtx);
	cEnginePS_SaveData &d = *e.mpData;
	cParticleSystem *apPS = e.mpPS;
	switch(alOp)
	{
	case eOhplGa6Op_PsPresent:
		return apPS != NULL;
	case eOhplGa6Op_PsFromNull:
		d.msName = "NULL";
		d.msType = "";
		d.mvEmitterActive.Clear();
		return 0;
	case eOhplGa6Op_PsFromHead:
		d.msName = apPS->GetName();
		d.mlID = apPS->GetUniqueID();
		d.msType = apPS->GetDataName();
		d.mvSize = apPS->GetDataSize();
		d.m_mtxTransform = apPS->GetLocalMatrix();
		d.mColor = apPS->GetColor();
		d.mbFadeAtDistance = apPS->GetFadeAtDistance();
		d.mfMinFadeDistanceStart = apPS->GetMinFadeDistanceStart();
		d.mfMinFadeDistanceEnd =apPS->GetMinFadeDistanceEnd();
		d.mfMaxFadeDistanceStart =apPS->GetMaxFadeDistanceStart();
		d.mfMaxFadeDistanceEnd =apPS->GetMaxFadeDistanceEnd();
		d.mbVisible = apPS->IsVisible();
		d.mbActive = apPS->IsActive();
		return 0;
	case eOhplGa6Op_PsLiveEmitterN:
		return apPS->GetEmitterNum();
	case eOhplGa6Op_PsResizeEmitters:
		d.mvEmitterActive.Resize(alA);
		return 0;
	case eOhplGa6Op_PsEmitterDying:
		return apPS->GetEmitter(alA)->IsDying();
	case eOhplGa6Op_PsEmitterDead:
		return apPS->GetEmitter(alA)->IsDead();
	case eOhplGa6Op_PsSetEmitter:
		d.mvEmitterActive[alA].mbActive = alB != 0;
		return 0;
	}
	return 0;
}

int PSToCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	PSEnv &e = *static_cast<PSEnv*>(apCtx);
	cEnginePS_SaveData &d = *e.mpData;
	cParticleSystem *apPS = e.mpPS;
	switch(alOp)
	{
	case eOhplGa6Op_PsPresent:
		return apPS != NULL;
	case eOhplGa6Op_PsToHead:
		apPS->SetActive(d.mbActive);
		apPS->SetVisible(d.mbVisible);
		apPS->SetUniqueID(d.mlID);
		apPS->SetMatrix(d.m_mtxTransform);
		apPS->SetColor(d.mColor);
		apPS->SetFadeAtDistance(d.mbFadeAtDistance);
		apPS->SetMinFadeDistanceStart(d.mfMinFadeDistanceStart);
		apPS->SetMinFadeDistanceEnd(d.mfMinFadeDistanceEnd);
		apPS->SetMaxFadeDistanceStart(d.mfMaxFadeDistanceStart);
		apPS->SetMaxFadeDistanceEnd(d.mfMaxFadeDistanceEnd);
		return 0;
	case eOhplGa6Op_PsSavedEmitterN:
		return (int)d.mvEmitterActive.Size();
	case eOhplGa6Op_PsLiveEmitterN:
		return apPS->GetEmitterNum();
	case eOhplGa6Op_PsSavedEmitterActive:
		return d.mvEmitterActive[alA].mbActive;
	case eOhplGa6Op_PsKillEmitter:
		apPS->GetEmitter(alA)->KillInstantly();
		return 0;
	}
	return 0;
}

}

void cEnginePS_SaveData::FromPS(cParticleSystem *apPS)
{
	PSEnv env = { this, apPS };
	ohpl_ga6_ps_from(&env, PSFromCall);
}

//------------------------------------------------------------------------

void cEnginePS_SaveData::ToPS(cParticleSystem *apPS)
{
	PSEnv env = { this, apPS };
	ohpl_ga6_ps_to(&env, PSToCall);
}

//------------------------------------------------------------------------

kSerializeRustClass(cEnginePSEmitter_SaveData)

kSerializeRustClass(cEnginePS_SaveData)

//------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// BEAM
//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------

void cEngineBeam_SaveData::FromBeam(cBeam *apBeam)
{
	msName = apBeam->GetName();
	mlID = apBeam->GetUniqueID();
	msFile = apBeam->GetFileName();

	mvStartPos = apBeam->GetLocalPosition();
	mvEndPos = apBeam->GetEnd()->GetLocalPosition();
}

//------------------------------------------------------------------------

void cEngineBeam_SaveData::ToBeam(cBeam *apBeam)
{
	apBeam->LoadXMLProperties(msFile);	

	apBeam->SetPosition(mvStartPos);
	apBeam->GetEnd()->SetPosition(mvEndPos);
}

//------------------------------------------------------------------------

kSerializeRustClass(cEngineBeam_SaveData)

//------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SOUND
//////////////////////////////////////////////////////////////////////////

namespace
{

struct SoundEnv
{
	cEngineSound_SaveData *mpData;
	cSoundEntity *mpSound;
};

int SoundFromCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	SoundEnv &e = *static_cast<SoundEnv*>(apCtx);
	cEngineSound_SaveData &d = *e.mpData;
	cSoundEntity *apSound = e.mpSound;
	switch(alOp)
	{
	case eOhplGa6Op_SoundFromHead:
		d.msName = apSound->GetName();
		d.mlID = apSound->GetUniqueID();
		d.mbActive = apSound->IsActive();
		d.mbRemoveWhenOver = apSound->GetRemoveWhenOver();
		return 0;
	case eOhplGa6Op_SoundIsStopped:
		return apSound->IsStopped();
	case eOhplGa6Op_SoundIsFadingOut:
		return apSound->IsFadingOut();
	case eOhplGa6Op_SoundSetStopped:
		d.mbStopped = alA != 0;
		return 0;
	case eOhplGa6Op_SoundFromTail:
		d.msSoundDataName = apSound->GetData()->GetName();
		d.mfMinDistance = apSound->GetMinDistance();
		d.mfMaxDistance = apSound->GetMaxDistance();
		d.mfVolume = apSound->GetVolume();
		d.mvLocalPosition = apSound->GetLocalPosition();
		return 0;
	}
	return 0;
}

int SoundToCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	SoundEnv &e = *static_cast<SoundEnv*>(apCtx);
	cEngineSound_SaveData &d = *e.mpData;
	cSoundEntity *apSound = e.mpSound;
	switch(alOp)
	{
	case eOhplGa6Op_SoundToHead:
		apSound->SetUniqueID(d.mlID);
		apSound->SetActive(d.mbActive);
		return 0;
	case eOhplGa6Op_SoundSavedStopped:
		return d.mbStopped;
	case eOhplGa6Op_SoundStop:
		apSound->Stop(false);
		return 0;
	case eOhplGa6Op_SoundToTail:
		apSound->SetMinDistance(d.mfMinDistance);
		apSound->SetMaxDistance(d.mfMaxDistance);
		apSound->SetVolume(d.mfVolume);
		apSound->SetPosition(d.mvLocalPosition);
		return 0;
	}
	return 0;
}

struct LightEnv
{
	cEngineLight_SaveData *mpData;
	iLight *mpLight;
};

int LightFromCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	LightEnv &e = *static_cast<LightEnv*>(apCtx);
	cEngineLight_SaveData &d = *e.mpData;
	iLight *apLight = e.mpLight;
	switch(alOp)
	{
	case eOhplGa6Op_LightHasParent:
		return apLight->GetParent() != NULL || apLight->GetEntityParent() != NULL;
	case eOhplGa6Op_LightFromBase:
		d.msName = apLight->GetName();
		d.mlID = apLight->GetUniqueID();
		d.mbActive = apLight->IsActive();
		d.mbVisible = apLight->GetVisibleVar();
		return 0;
	case eOhplGa6Op_LightIsFading:
		return apLight->IsFading();
	case eOhplGa6Op_LightFlickerActive:
		return apLight->GetFlickerActive();
	case eOhplGa6Op_LightWriteDest:
		apLight->SetDiffuseColor(apLight->GetDestColor());
		apLight->SetRadius(apLight->GetDestRadius());
		return 0;
	case eOhplGa6Op_LightColorFlickerOn:
		d.mDiffuseColor = apLight->GetFlickerOnColor();
		d.mfFarAttenuation = apLight->GetFlickerOnRadius();
		return 0;
	case eOhplGa6Op_LightColorCurrent:
		d.mDiffuseColor = apLight->GetDiffuseColor();
		d.mfFarAttenuation = apLight->GetRadius();
		return 0;
	case eOhplGa6Op_LightFlickerProps:
		d.mbFlickering = apLight->GetFlickerActive();
		d.msFlickerOffSound = apLight->GetFlickerOffSound();
		d.msFlickerOnSound = apLight->GetFlickerOnSound();
		d.msFlickerOffPS = apLight->GetFlickerOffPS();
		d.msFlickerOnPS = apLight->GetFlickerOnPS();
		d.mfFlickerOnMinLength = apLight->GetFlickerOnMinLength();
		d.mfFlickerOffMinLength = apLight->GetFlickerOffMinLength();
		d.mfFlickerOnMaxLength = apLight->GetFlickerOnMaxLength();
		d.mfFlickerOffMaxLength = apLight->GetFlickerOffMaxLength();
		d.mFlickerOffColor = apLight->GetFlickerOffColor();
		d.mfFlickerOffRadius = apLight->GetFlickerOffRadius();
		d.mbFlickerFade = apLight->GetFlickerFade();
		d.mfFlickerOnFadeMinLength = apLight->GetFlickerOnFadeMinLength();
		d.mfFlickerOnFadeMaxLength = apLight->GetFlickerOnFadeMaxLength();
		d.mfFlickerOffFadeMinLength = apLight->GetFlickerOffFadeMinLength();
		d.mfFlickerOffFadeMaxLength = apLight->GetFlickerOffFadeMaxLength();
		return 0;
	}
	return 0;
}

int LightToCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	LightEnv &e = *static_cast<LightEnv*>(apCtx);
	cEngineLight_SaveData &d = *e.mpData;
	iLight *apLight = e.mpLight;
	switch(alOp)
	{
	case eOhplGa6Op_LightHasParent:
		return apLight->GetParent() != NULL || apLight->GetEntityParent() != NULL;
	case eOhplGa6Op_LightSetActiveVisible:
		apLight->SetActive(d.mbActive);
		apLight->SetVisible(d.mbVisible);
		return 0;
	case eOhplGa6Op_LightSetColorFlicker:
		apLight->SetDiffuseColor(d.mDiffuseColor);
		apLight->SetRadius(d.mfFarAttenuation);
		apLight->SetFlickerActive(d.mbFlickering);
		apLight->SetFlicker(d.mFlickerOffColor,d.mfFlickerOffRadius,
			d.mfFlickerOnMinLength,d.mfFlickerOnMaxLength,
			d.msFlickerOnSound,d.msFlickerOnPS,
			d.mfFlickerOffMinLength,d.mfFlickerOffMaxLength,
			d.msFlickerOffSound,d.msFlickerOffPS,
			d.mbFlickerFade,d.mfFlickerOnFadeMinLength,d.mfFlickerOnFadeMaxLength,
			d.mfFlickerOffFadeMinLength, d.mfFlickerOffFadeMaxLength);
		return 0;
	}
	return 0;
}

}

void cEngineSound_SaveData::FromSound(cSoundEntity* apSound)
{
	SoundEnv env = { this, apSound };
	ohpl_ga6_sound_from(&env, SoundFromCall);
}

//------------------------------------------------------------------------

void cEngineSound_SaveData::ToSound(cSoundEntity* apSound)
{
	SoundEnv env = { this, apSound };
	ohpl_ga6_sound_to(&env, SoundToCall);
}
//------------------------------------------------------------------------

kSerializeRustClass(cEngineSound_SaveData)

//------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// LIGHT
//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------

void cEngineLight_SaveData::FromLight(iLight *apLight)
{
	LightEnv env = { this, apLight };
	ohpl_ga6_light_from(&env, LightFromCall);
}

//------------------------------------------------------------------------

void cEngineLight_SaveData::ToLight(iLight *apLight)
{
	LightEnv env = { this, apLight };
	ohpl_ga6_light_to(&env, LightToCall);
}

//------------------------------------------------------------------------

kSerializeRustClass(cEngineLight_SaveData)

//------------------------------------------------------------------------



