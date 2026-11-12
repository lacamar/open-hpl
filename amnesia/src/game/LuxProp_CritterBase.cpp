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

#include "LuxProp_CritterBase.h"

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxPlayerHelpers.h"
#include <cstring>
#include "LuxMap.h"
#include "LuxMapHelper.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxPropLoader_Critter::iLuxPropLoader_Critter(const tString asName) : iLuxPropLoader(asName)
{

}

//-----------------------------------------------------------------------

void iLuxPropLoader_Critter::LoadVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	iLuxProp_CritterBase  *pCritter = static_cast<iLuxProp_CritterBase*>(apProp);

	OhplGa4bVars<cResourceVarsObject> vars(this);
	vars.Slot(kOhplGa4bSlot_DeathEntity, pCritter->msDeathEntity);
	vars.Slot(kOhplGa4bSlot_DeathPS, pCritter->msDeathPS);
	vars.Slot(kOhplGa4bSlot_DeathSound, pCritter->msDeathSound);
	vars.Slot(kOhplGa4bSlot_AttackHitSound, pCritter->msAttackHitSound);
	ohpl_ga4b_critter_load(vars.Vars(), &pCritter->mCP);

	LoadCritterVariables(apProp, apRootElem);
}


//-----------------------------------------------------------------------

void iLuxPropLoader_Critter::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{


	LoadCritterInstanceVariables(apProp, apInstanceVars);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// RAY CALLBACK
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxCritterRayCallback::Reset()
{
	mbIntersected = false;
	mfClosestT = 99999.0f;
    mpBody=NULL;
}

bool cLuxCritterRayCallback::BeforeIntersect(iPhysicsBody *pBody)
{
	if(pBody->GetCollide()==false || pBody->IsCharacter() || mpCritterBase->mpBody == pBody) return false;
	return true;
}

bool cLuxCritterRayCallback::OnIntersect(iPhysicsBody *pBody, cPhysicsRayParams *apParams)
{
	if(apParams->mfT < mfClosestT)
	{
		mfClosestT = apParams->mfT;
		mvNormal = apParams->mvNormal;
		mvPos = apParams->mvPoint;
		mpBody = pBody;
	}
	mbIntersected = true;
    
	
	return true;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxProp_CritterBase::iLuxProp_CritterBase(const tString &asName,int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_Critter)
{
	ohpl_ga4b_critter_state_new(&mC);
	mpRayCallback = hplNew(cLuxCritterRayCallback, (this));
}

//-----------------------------------------------------------------------

iLuxProp_CritterBase::~iLuxProp_CritterBase()
{
	hplDelete(mpRayCallback);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool iLuxProp_CritterBase::CanInteract(iPhysicsBody *apBody)
{
	return false;
	
}

//-----------------------------------------------------------------------

bool iLuxProp_CritterBase::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	iPhysicsBody *pBody = mpMainBody ? mpMainBody : apBody;

	return true;
}

//-----------------------------------------------------------------------

void iLuxProp_CritterBase::OnSetupAfterLoad(cWorld *apWorld)
{
	mpBody = mvBodies[0];

	mvBaseMatrices.resize(mpMeshEntity->GetSubMeshEntityNum());
	for(int i=0; i<mpMeshEntity->GetSubMeshEntityNum(); ++i)
	{
		mvBaseMatrices[i] = mpMeshEntity->GetSubMeshEntity(i)->GetLocalMatrix();
	}

	
	{
		cVector3f vSwarm = mpBody->GetWorldPosition();
		mC.swarm_point[0] = vSwarm.x; mC.swarm_point[1] = vSwarm.y; mC.swarm_point[2] = vSwarm.z;
	}

	if(mpMeshEntity->GetAnimationStateNum() >0)
	{
		mpMeshEntity->Play(0, true,true);
	}

	if(mCP.attack_size_radius > 0)
		mpDamageShape = apWorld->GetPhysicsWorld()->CreateSphereShape(mCP.attack_size_radius, NULL);
	else
		mpDamageShape = NULL;


	cVector3f vGroundNormal = cMath::MatrixMul(mpBody->GetLocalMatrix().GetRotation(), cVector3f(mC.ground_normal[0], mC.ground_normal[1], mC.ground_normal[2]));
	cVector3f vForward = cMath::MatrixMul(mpBody->GetLocalMatrix().GetRotation(), cVector3f(0,0,-1));
	ohpl_ga4b_critter_setup_dirs(&mC, vGroundNormal.v, vForward.v);

	cMatrixf mtxNoRotation = cMatrixf::Identity;
	mtxNoRotation.SetTranslation(mpBody->GetLocalPosition());
	mpBody->SetMatrix(mtxNoRotation);
}

//-----------------------------------------------------------------------

void iLuxProp_CritterBase::OnResetProperties()
{

}

//-----------------------------------------------------------------------

void iLuxProp_CritterBase::UpdatePropSpecific(float afTimeStep)
{
	cVector3f vPlayerPos = gpBase->mpPlayer->GetCharacterBody()->GetFeetPosition();
	cVector3f vBodyPos = mpBody->GetWorldPosition();
	if(ohpl_ga4b_critter_in_range(vPlayerPos.v, vBodyPos.v)==false) return;

	UpdateCritterSpecific(afTimeStep);
	UpdateMovement(afTimeStep);
	UpdateMesh(afTimeStep);
}

//-----------------------------------------------------------------------

void iLuxProp_CritterBase::BeforePropDestruction()
{
}

//-----------------------------------------------------------------------

eLuxFocusCrosshair iLuxProp_CritterBase::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return eLuxFocusCrosshair_Default;
}

//-----------------------------------------------------------------------

void iLuxProp_CritterBase::OnRenderSolid(cRendererCallbackFunctions* apFunctions)
{
	return;
	iPhysicsWorld *pPhysicsWorld = mpWorld->GetPhysicsWorld();

	pPhysicsWorld->RenderShapeDebugGeometry(mpBody->GetShape(), mpBody->GetLocalMatrix(), apFunctions->GetLowLevelGfx(), cColor(1,1));
	
	apFunctions->GetLowLevelGfx()->DrawLine(mpBody->GetLocalPosition(), mpBody->GetLocalPosition()+cVector3f(mC.ground_normal[0], mC.ground_normal[1], mC.ground_normal[2])*0.5f,cColor(1,0,0,1));
	cVector3f vVel(mC.vel[0], mC.vel[1], mC.vel[2]);
	cVector3f vLastFwd(mC.fwd[mC.n_fwd-1][0], mC.fwd[mC.n_fwd-1][1], mC.fwd[mC.n_fwd-1][2]);
	cVector3f vFwdDir = cMath::Vector3Normalize(vVel==0 ? vLastFwd : vVel);
	apFunctions->GetLowLevelGfx()->DrawLine(mpBody->GetLocalPosition(), mpBody->GetLocalPosition()+vFwdDir*0.5f,cColor(0,1,0,1));

	if(mpDamageShape)
	{
		//cVector3f vDir = cMath::Vector3Normalize(gpBase->mpPlayer->GetCharacterBody()->GetPosition() - mpBody->GetLocalPosition());
		//pPhysicsWorld->RenderShapeDebugGeometry(mpDamageShape, GetAttackMatrix(vDir),apFunctions->GetLowLevelGfx(), cColor(1,1));
	}


	/*for(int i=0; i<mpMeshEntity->GetSubMeshEntityNum(); ++i)
	{
		cSubMeshEntity *pSubEnt = mpMeshEntity->GetSubMeshEntity(i);
		cBoundingVolume *pBV =  pSubEnt->GetBoundingVolume();
		apFunctions->GetLowLevelGfx()->DrawBoxMinMax(pBV->GetMin(), pBV->GetMax(),cColor(1,1));
	}*/
}

//-----------------------------------------------------------------------

void iLuxProp_CritterBase::OnHealthChange()
{
	///////////////
	// Death
	if(mfHealth <=0) 
	{
		cWorld *pWorld = mpMap->GetWorld();

		if(msDeathSound != "")
			PlaySound("CritterDeath",msDeathSound, true, true);

		if(msDeathPS != "")
		{
			cParticleSystem *pPS = pWorld->CreateParticleSystem("CritterDeath", msDeathPS,1);
			if(pPS)
			{
				pPS->SetPosition(mpBody->GetLocalPosition());
			}
		}

		if(msDeathEntity != "")
		{
			pWorld->CreateEntity(msName+"_Dead", mpMeshEntity->GetWorldMatrix(), msDeathEntity);
		}

		OnKillCritter();

		mpMap->DestroyEntity(this);
	}
}

//-----------------------------------------------------------------------

void iLuxProp_CritterBase::OnDamage(float afAmount, int alStrength)
{
	OnDamageCritter(afAmount);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

cMatrixf iLuxProp_CritterBase::GetAttackMatrix(const cVector3f& avDir)
{
	/*if(mvVel ==0)
	{
		if(mlstFwdDirs.empty())		vForward = cVector3f(0,0,1);
		else						vForward = mlstFwdDirs.back();
	}
	else
	{
		vForward = cMath::Vector3Normalize(mvVel);
	}

	cVector3f vRight;
	CreateOrthoVectors(vRight, mvGroundNormal, vForward);

	cMatrixf mtxTemp = cMatrixf::Identity;
	mtxTemp.SetForward(vForward);
	mtxTemp.SetUp(mvGroundNormal);
	mtxTemp.SetRight(vRight);

	cMatrixf mtxOffset = mtxTemp.GetTranspose();*/

	cVector3f vPos =	mpBody->GetLocalPosition() + avDir * mCP.attack_offset[2];

	cMatrixf mtxOffset = cMatrixf::Identity;
	mtxOffset.SetTranslation(vPos);

	return mtxOffset;
}

//-----------------------------------------------------------------------

bool iLuxProp_CritterBase::Attack(const cVector3f& avDir)
{
	if(mpDamageShape==NULL) return false;

	bool bHit = gpBase->mpMapHelper->ShapeDamage(mpDamageShape, GetAttackMatrix(avDir), mpBody->GetLocalPosition(),
												mCP.attack_damage[0], mCP.attack_damage[1],
												mCP.attack_force[0], mCP.attack_force[1], mCP.attack_strength, 3,
												eLuxDamageType_BloodSplat, eLuxWeaponHitType_Sword,
												false, true, false, false);	
	if(bHit)
	{
		PlaySound("CritterAttack",msAttackHitSound,true, true);
	}
	return bHit;
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

void iLuxProp_CritterBase::UpdateMovement(float afTimeStep)
{
	cLuxCritterQuery query(this);
	OhplGa4bSink<iLuxProp_CritterBase> sink(this);
	ohpl_ga4b_critter_update_movement(&mC, query.Query(), sink.Sink(), afTimeStep);
}

//-----------------------------------------------------------------------

void iLuxProp_CritterBase::UpdateMesh(float afTimeStep)
{
	cMatrixf mtxRotT;
	ohpl_ga4b_critter_mesh_basis(&mC, &mtxRotT.m[0][0]);

	cMatrixf mtxOffset;
	memcpy(&mtxOffset.m[0][0], mCP.mesh_offset, sizeof(mCP.mesh_offset));

	if(mpMeshEntity->GetMesh()->GetSkeleton())
	{
		cMatrixf mtxRot;
		ohpl_ga4b_critter_mesh_matrix(&mtxRotT.m[0][0], &mtxOffset.m[0][0], true, &mtxRot.m[0][0]);
		mpMeshEntity->SetMatrix(mtxRot);
	}
	else
	{
		cMatrixf mtxRotOffset;
		ohpl_ga4b_critter_mesh_matrix(&mtxRotT.m[0][0], &mtxOffset.m[0][0], false, &mtxRotOffset.m[0][0]);
		for(int i=0; i<mpMeshEntity->GetSubMeshEntityNum(); ++i)
		{
			cMatrixf mtxLocal = cMath::MatrixMul(mtxRotOffset, mvBaseMatrices[i]);
			mpMeshEntity->GetSubMeshEntity(i)->SetMatrix(mtxLocal);
		}
	}

	if(mpMeshEntity->GetAnimationStateNum() >0 && mC.update_animation)
	{
		OhplGa4bSink<iLuxProp_CritterBase> sink(this);
		ohpl_ga4b_critter_anim_step(&mC, sink.Sink());
	}
}

void iLuxProp_CritterBase::Ga4bApplyOp(const OhplGa4Op &aOp)
{
	switch(aOp.code)
	{
	case kOhplGa4bOp_Sound:
		OnGa4bSound(aOp.i);
		break;
	case kOhplGa4bOp_Attack:
		Attack(cVector3f(aOp.v[0], aOp.v[1], aOp.v[2]));
		break;
	case kOhplGa4bOp_AnimFade:
		if(aOp.i == kOhplGa4bAnim_Attack)		mpMeshEntity->PlayFadeToName("Attack", false, 0.3f);
		else if(aOp.i == kOhplGa4bAnim_Walk)	mpMeshEntity->PlayFadeToName("Walk", true, 0.3f);
		else									mpMeshEntity->PlayFadeToName("Idle", true, 0.3f);
		break;
	case kOhplGa4bOp_AnimSpeed:
		{
			cAnimationState *pAnim = mpMeshEntity->GetAnimationStateFromName("Walk");
			if(pAnim) pAnim->SetSpeed(aOp.f[0]);
		}
		break;
	case kOhplGa4bOp_SetBodyPos:
		mpBody->SetPosition(cVector3f(aOp.v[0], aOp.v[1], aOp.v[2]));
		break;
	case kOhplGa4bOp_ShapeCollision:
		OnShapeCollision(cVector3f(aOp.v[0], aOp.v[1], aOp.v[2]), aOp.f[0]);
		break;
	}
}

void cLuxCritterQuery::Ask(void *apCtx, int alCode, const float *apA, const float *apB, int alFlags, OhplGa4bReply *apOut)
{
	iLuxProp_CritterBase *pCritter = ((cLuxCritterQuery*)apCtx)->mpCritter;
	iPhysicsBody *pBody = pCritter->mpBody;
	cLuxCritterRayCallback *pRay = pCritter->mpRayCallback;

	apOut->ok = 0;
	apOut->v[0] = apOut->v[1] = apOut->v[2] = 0;
	apOut->f = 0;
	cVector3f vOut(0);
	bool bVec = true;

	switch(alCode)
	{
	case kOhplGa4bQ_PlayerFeet:		vOut = gpBase->mpPlayer->GetCharacterBody()->GetFeetPosition(); break;
	case kOhplGa4bQ_PlayerPos:		vOut = gpBase->mpPlayer->GetCharacterBody()->GetPosition(); break;
	case kOhplGa4bQ_PlayerDead:		apOut->ok = gpBase->mpPlayer->IsDead(); bVec = false; break;
	case kOhplGa4bQ_BodyWorld:		vOut = pBody->GetWorldPosition(); break;
	case kOhplGa4bQ_BodyLocal:		vOut = pBody->GetLocalPosition(); break;
	case kOhplGa4bQ_BodySize:		vOut = pBody->GetBoundingVolume()->GetSize(); break;
	case kOhplGa4bQ_ShapeRadius:	apOut->f = pBody->GetShape()->GetRadius(); bVec = false; break;
	case kOhplGa4bQ_Lantern:
		bVec = false;
		if(gpBase->mpPlayer->GetHelperLantern()->IsActive())
		{
			apOut->ok = 1;
			apOut->f = gpBase->mpPlayer->GetHelperLantern()->GetLight()->GetRadius();
		}
		break;
	case kOhplGa4bQ_RayReset:		pRay->Reset(); bVec = false; break;
	case kOhplGa4bQ_RayCast:
		{
			iPhysicsWorld *pPhysicsWorld = pCritter->mpWorld->GetPhysicsWorld();
			cVector3f vFrom(apA[0], apA[1], apA[2]);
			cVector3f vTo(apB[0], apB[1], apB[2]);
			if(alFlags == 0)	pPhysicsWorld->CastRay(pRay, vFrom, vTo, false,true,true, true);
			else				pPhysicsWorld->CastRay(pRay, vFrom, vTo, false,false,false, true);
			bVec = false;
		}
		break;
	case kOhplGa4bQ_RayHit:			apOut->ok = pRay->GetIntersected(); bVec = false; break;
	case kOhplGa4bQ_RayNormal:		vOut = pRay->GetNormal(); break;
	case kOhplGa4bQ_RayMass:		apOut->f = pRay->GetBody()->GetMass(); bVec = false; break;
	case kOhplGa4bQ_ShapeCollide:
		{
			cVector3f vPush(0);
			apOut->ok = pCritter->mpWorld->GetPhysicsWorld()->CheckShapeWorldCollision(&vPush, pBody->GetShape(), cMath::MatrixTranslate(cVector3f(apA[0], apA[1], apA[2])), pBody);
			vOut = vPush;
		}
		break;
	}
	if(bVec)
	{
		apOut->v[0] = vOut.x; apOut->v[1] = vOut.y; apOut->v[2] = vOut.z;
	}
}


//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(iLuxProp_CritterBase_SaveData, iLuxProp_SaveData)
kSerializeVar(mvVel, eSerializeType_Vector3f)
kSerializeVar(mvGravityVel, eSerializeType_Vector3f)
kSerializeVar(mvSwarmPoint, eSerializeType_Vector3f)
kSerializeVar(mlAnimState, eSerializeType_Int32)
kSerializeVar(mvGroundNormal, eSerializeType_Vector3f)
kSerializeVar(mbColliding, eSerializeType_Bool)
kSerializeVar(mbUpdateAnimation, eSerializeType_Bool)
kEndSerialize()

//-----------------------------------------------------------------------

void iLuxProp_CritterBase::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	iLuxProp_CritterBase_SaveData *pData = static_cast<iLuxProp_CritterBase_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	pData->mvVel = cVector3f(mC.vel[0], mC.vel[1], mC.vel[2]);
	pData->mvGravityVel = cVector3f(mC.gravity_vel[0], mC.gravity_vel[1], mC.gravity_vel[2]);
	pData->mvSwarmPoint = cVector3f(mC.swarm_point[0], mC.swarm_point[1], mC.swarm_point[2]);
	pData->mlAnimState = mC.anim_state;
	pData->mvGroundNormal = cVector3f(mC.ground_normal[0], mC.ground_normal[1], mC.ground_normal[2]);
	pData->mbColliding = mC.colliding;
	pData->mbUpdateAnimation = mC.update_animation;
	
}

//-----------------------------------------------------------------------

void iLuxProp_CritterBase::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	iLuxProp_CritterBase_SaveData *pData = static_cast<iLuxProp_CritterBase_SaveData*>(apSaveData);
	
	//////////////////
	//Set variables
	mC.vel[0] = pData->mvVel.x; mC.vel[1] = pData->mvVel.y; mC.vel[2] = pData->mvVel.z;
	mC.gravity_vel[0] = pData->mvGravityVel.x; mC.gravity_vel[1] = pData->mvGravityVel.y; mC.gravity_vel[2] = pData->mvGravityVel.z;
	mC.swarm_point[0] = pData->mvSwarmPoint.x; mC.swarm_point[1] = pData->mvSwarmPoint.y; mC.swarm_point[2] = pData->mvSwarmPoint.z;
	mC.anim_state = pData->mlAnimState;
	mC.ground_normal[0] = pData->mvGroundNormal.x; mC.ground_normal[1] = pData->mvGroundNormal.y; mC.ground_normal[2] = pData->mvGroundNormal.z;
	mC.colliding = pData->mbColliding;
	mC.update_animation = pData->mbUpdateAnimation;
}

//-----------------------------------------------------------------------

void iLuxProp_CritterBase::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------
