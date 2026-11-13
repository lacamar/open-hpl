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

#include "physics/CharacterBody.h"

#include "impl/RustBackends.h"

#include "physics/CollideShape.h"
#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"
#include "system/LowLevelSystem.h"
#include "scene/Camera.h"
#include "math/Math.h"

#include "engine/Engine.h"
#include "scene/Scene.h"
#include "scene/World.h"

namespace hpl {

	static const OhplH14BodyVt* BodyVt();

	//////////////////////////////////////////////////////////////////////////
	// COLLIDE GRAVITY
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	std::vector<iPhysicsBody*> iCharacterBody::mvTempBodies;

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// COLLIDE GRAVITY
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cCharacterBodyCollideGravity::cCharacterBodyCollideGravity(iCharacterBody *apCharBody)
	{
		mpCharBody = apCharBody;
	}

	//-----------------------------------------------------------------------

	void cCharacterBodyCollideGravity::OnCollision(iPhysicsBody *apBody, cCollideData *apCollideData)
	{
		ohpl_h14_collide_gravity(iCharacterBody::HostOp, reinterpret_cast<size_t>(mpCharBody), &mpCharBody->mE, &mpCharBody->mS,
			reinterpret_cast<size_t>(apBody), reinterpret_cast<size_t>(apCollideData));
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// COLLIDE PUSH
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cCharacterBodyCollidePush::cCharacterBodyCollidePush(iCharacterBody *apCharBody)
	{
		mpCharBody = apCharBody;
	}

	//-----------------------------------------------------------------------

	void cCharacterBodyCollidePush::OnCollision(iPhysicsBody *apBody, cCollideData *apCollideData)
	{
		ohpl_h14_collide_push(iCharacterBody::HostOp, reinterpret_cast<size_t>(mpCharBody), &mpCharBody->mE, &mpCharBody->mS, reinterpret_cast<size_t>(apBody), reinterpret_cast<size_t>(apCollideData));
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// RAY CAST
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cCharacterBodyRay::cCharacterBodyRay() : mlCollideFlags(eFlagBit_All)
	{

	}

	//-----------------------------------------------------------------------

	void cCharacterBodyRay::Clear()
	{
		mfMinDist = 10000.0f;
		mbCollide = false;
	}

	//-----------------------------------------------------------------------

	bool cCharacterBodyRay::OnIntersect(iPhysicsBody *pBody,cPhysicsRayParams *apParams)
	{
		if(	pBody->IsCharacter()==false && pBody->GetCollideCharacter() &&
			iPhysicsBody::CollideFlagsMatch(mlCollideFlags, pBody->GetCollideFlags()) &&
			apParams->mfDist < mfMinDist)
		{
			mfMinDist = apParams->mfDist;
			mvNormal = apParams->mvNormal;
			mbCollide = true;
		}

		return true;
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	size_t iCharacterBody::HostOp(int32_t alOp, OhplH15Wa *a)
	{
		iPhysicsBody *pBody = reinterpret_cast<iPhysicsBody*>(a->p[0]);
		switch(alOp)
		{
		case 501: pBody->AddConnectedCharacter(reinterpret_cast<iCharacterBody*>(a->p[1])); return 0;
		case 502: *reinterpret_cast<cMatrixf*>(a->p[1]) = pBody->GetLocalMatrix(); return 0;
		case 503: { cVector3f v = pBody->GetWorldPosition(); a->rv[0] = v.x; a->rv[1] = v.y; a->rv[2] = v.z; return 0; }
		case 504: return pBody->GetGravity();
		case 505: pBody->SetGravity(a->i[0] != 0); return 0;
		case 506: return pBody->IsActive();
		case 507: pBody->RemoveConnectedCharacter(reinterpret_cast<iCharacterBody*>(a->p[1])); return 0;
		case 508: return reinterpret_cast<size_t>(pBody->GetMaterial());
		case 509: return (pBody->GetGravityCanAttachCharacter() ? 1 : 0) | (pBody->IsCharacter() ? 2 : 0) | (pBody->GetPushedByCharacterGravity() ? 4 : 0);
		case 510: a->rf = pBody->GetMass(); return 0;
		case 511: return reinterpret_cast<cCollideData*>(a->p[0])->mlNumOfPoints;
		case 512:
		{
			const cCollidePoint &pt = reinterpret_cast<cCollideData*>(a->p[0])->mvContactPoints[a->p[1]];
			if(a->p[2] == 1){ float *pOut = reinterpret_cast<float*>(a->p[3]); pOut[0] = pt.mvPoint.x; pOut[1] = pt.mvPoint.y; pOut[2] = pt.mvPoint.z; }
			else { a->rv[0] = pt.mvNormal.x; a->rv[1] = pt.mvNormal.y; a->rv[2] = pt.mvNormal.z; a->rf = pt.mfDepth; }
			return 0;
		}
		case 514:
		{
			iCharacterBody *pChar = reinterpret_cast<iCharacterBody*>(a->p[0]);
			if(pChar->mpCallback) pChar->mpCallback->OnGravityCollide(pChar, reinterpret_cast<iPhysicsBody*>(a->p[1]), reinterpret_cast<cCollideData*>(a->p[2]));
			return 0;
		}
		case 515: a->rf = reinterpret_cast<iCharacterBody*>(a->p[0])->mpWorld->GetGravity().y; return 0;
		case 516:
		{
			const float *pF = reinterpret_cast<const float*>(a->p[1]); const float *pP = reinterpret_cast<const float*>(a->p[2]);
			pBody->AddForceAtPosition(cVector3f(pF[0], pF[1], pF[2]), cVector3f(pP[0], pP[1], pP[2]));
			return 0;
		}
		case 517:
		{
			iCharacterBody *pChar = reinterpret_cast<iCharacterBody*>(a->p[0]);
			iCharacterBody *pHitCharBody = reinterpret_cast<iPhysicsBody*>(a->p[1])->GetCharacterBody();
			if(pHitCharBody->GetMass() > pChar->GetCharacterMaxPushMass()) return 0;

			float fHitTop = pHitCharBody->GetPosition().y + pHitCharBody->GetSize().y/2;
			//Make bottom 10% of size longer up to remove uneeded pushing.
			float fBottom = pChar->GetPosition().y - (pChar->GetSize().y/2 - pChar->GetSize().y*0.1f);

			//If body is on top of hitbody, skip doing a push.
			if(fHitTop < fBottom) return 0;

			cVector3f vDir = pHitCharBody->GetPosition() - pChar->GetPosition();
			if(pChar->GetCharacterPushIn2D()) vDir.y =0;
			vDir.Normalize();

			pHitCharBody->AddForce(vDir * pChar->GetCharacterPushForce());
			return 0;
		}
		case 519: return reinterpret_cast<size_t>(pBody->GetCharacterBody());
		}
		return 0;
	}

	iCharacterBody::iCharacterBody(const tString &asName, iPhysicsWorld *apWorld, const cVector3f avSize)
	{
		mpAux = ohpl_h14_aux_new();

		/////////////////////////////
		//Set up main properties
		msName = asName;

		mS.size = avSize;
		ohpl_h14_char_init(&mE, &mS, reinterpret_cast<size_t>(apWorld), cos(cMath::ToRad(45.0f)));

		/////////////////////////////
		// Create Shape
		float fRadius = cMath::Max(avSize.x, avSize.z)*0.5f;
		cMatrixf mtxOffset = cMath::MatrixRotateZ(kPi2f);
		iCollideShape *pCollider = NULL;

		float fForceAdd = 0.2f;
		if(fabs(fRadius*2.0f - avSize.y)<0.01)
		{
			mpCurrentShape = mpWorld->CreateSphereShape(fRadius,NULL);
		}
		else
		{
			mpCurrentShape = mpWorld->CreateCylinderShape(fRadius, avSize.y,&mtxOffset);
		}
		mpCurrentBody = mpWorld->CreateBody(asName, mpCurrentShape);

		mpCurrentBody->SetMass(0);
		mpCurrentBody->SetGravity(false);
		mpCurrentBody->SetIsCharacter(true);
		mpCurrentBody->SetCharacterBody(this);
		mpCurrentBody->SetCollideFlags(mS.collideFlags);
		mpCurrentBody->SetCollideCharacter(true);

		mvShapes.push_back(mpCurrentShape);
		mvBodies.push_back(mpCurrentBody);


		/////////////////////////////
		//Create callbacks
		mpCollideGravityCallback = hplNew( cCharacterBodyCollideGravity, (this) );
		mpCollidePushCallback = hplNew( cCharacterBodyCollidePush, (this) );
		mpRayCallback = hplNew( cCharacterBodyRay, () );
	}

	//-----------------------------------------------------------------------

	iCharacterBody::~iCharacterBody()
	{
		DisconnectBody();

		for(size_t i=0; i< mvBodies.size(); i++) mpWorld->DestroyBody(mvBodies[i]);

		ohpl_h14_aux_free(mpAux);

		hplDelete(mpCollideGravityCallback);
		hplDelete(mpCollidePushCallback);
		hplDelete(mpRayCallback);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	float iCharacterBody::GetMass()
	{
		return mS.mass;
	}
	void iCharacterBody::SetMass(float afMass)
	{
		mS.mass = afMass;
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::SetActive(bool abX)
	{
		if(mS.active == abX) return;

		mS.active = abX;
		UpdateBodyActive();
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::SetCollideCharacter(bool abX)
	{
		if(abX == mbCollideCharacter) return;

		mbCollideCharacter = abX;
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::SetTestCollision(bool abX)
	{
		if(mS.testCollision == abX) return;

		mS.testCollision = abX;
		UpdateBodyActive();
	}

	void iCharacterBody::SetPhysicsBodyActive(bool abX)
	{
		mbPhysicsBodyActive = abX;
		UpdateBodyActive();
	}

	void iCharacterBody::UpdateBodyActive()
	{
		mpCurrentBody->SetActive(mS.active && mS.testCollision && mbPhysicsBodyActive);
	}

	//-----------------------------------------------------------------------

	cVector3f iCharacterBody::GetSize()
	{
		return mS.size;
	}

	//-----------------------------------------------------------------------

	int iCharacterBody::AddExtraSize(const cVector3f& avSize)
	{
		float fRadius = cMath::Max(avSize.x, avSize.z)*0.5f;
		cMatrixf mtxOffset = cMath::MatrixRotateZ(kPi2f);
		iCollideShape *pCollider = mpWorld->CreateCylinderShape(fRadius, avSize.y,&mtxOffset);
		iPhysicsBody *pBody = mpWorld->CreateBody(msName, pCollider);

		pBody->SetMass(0);
		pBody->SetGravity(false);
		pBody->SetIsCharacter(true);
		pBody->SetCharacterBody(this);
		pBody->SetActive(false);
		pBody->SetCollideFlags(mS.collideFlags);
		pBody->SetCollideCharacter(true);

		mvBodies.push_back(pBody);
		mvShapes.push_back(pCollider);

		return (int)mvShapes.size() -1;
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::SetActiveSize(int alNum)
	{
		if(alNum == mS.currentShapeIdx || alNum < 0 || alNum >= (int)mvShapes.size()) return;

		cVector3f vFeetPosition = GetFeetPosition();
		mpCurrentBody->SetActive(false);

		mS.currentShapeIdx = alNum;

		mpCurrentShape = mvShapes[alNum];
		mpCurrentBody = mvBodies[alNum];

		SetPosition(vFeetPosition + cVector3f(0,mpCurrentShape->GetSize().y/2.0f,0), true);

		UpdateBodyActive();

		//Set size of the new shape
		mS.size.y = mpCurrentShape->GetHeight();
		mS.size.x = mpCurrentShape->GetRadius() * 2;
		mS.size.z = mpCurrentShape->GetRadius() * 2;
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::SetMaxPositiveMoveSpeed(eCharDir aDir, float afX)
	{
		mS.maxPosMoveSpeed[aDir] = afX;
	}
	float iCharacterBody::GetMaxPositiveMoveSpeed(eCharDir aDir)
	{
		return mS.maxPosMoveSpeed[aDir];
	}
	void iCharacterBody::SetMaxNegativeMoveSpeed(eCharDir aDir, float afX)
	{
		mS.maxNegMoveSpeed[aDir] = afX;
	}
	float iCharacterBody::GetMaxNegativeMoveSpeed(eCharDir aDir)
	{
		return mS.maxNegMoveSpeed[aDir];
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::SetMoveSpeed(eCharDir aDir, float afX)
	{
		mS.moveSpeed[aDir] = afX;
	}
	float iCharacterBody::GetMoveSpeed(eCharDir aDir)
	{
		return mS.moveSpeed[aDir];
	}

	void iCharacterBody::SetMoveAcc(eCharDir aDir, float afX)
	{
		mS.moveAcc[aDir] = afX;
	}
	float iCharacterBody::GetMoveAcc(eCharDir aDir)
	{
		return mS.moveAcc[aDir];
	}

	void iCharacterBody::SetMoveDeacc(eCharDir aDir, float afX)
	{
		mS.moveDeacc[aDir] = afX;
	}
	float iCharacterBody::GetMoveDeacc(eCharDir aDir)
	{
		return mS.moveDeacc[aDir];
	}

	//-----------------------------------------------------------------------

	cVector3f iCharacterBody::GetVelocity(float afFrameTime)
	{
		if(afFrameTime <=0) return 0;

        return (mS.position - mS.lastPosition) / afFrameTime;
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::SetupSafePositions(float afSaveInterval, float afBufferTime)
	{
		ohpl_h14_setup_safe_positions(mpAux, afSaveInterval, afBufferTime);
	}

	// iCharacterBody::PostUpdate 0x14b4a20, iCharacterBody::TeleportToSafePosition 0xe787c0 (Rebirth)
	void iCharacterBody::UpdateSafePositions(float afTimeStep)
	{
		ohpl_h14_update_safe_positions(mpAux, afTimeStep, OhplV3{mS.position.x, mS.position.y, mS.position.z});
	}

	void iCharacterBody::TeleportToSafePosition(float afElapsedTime, bool abSmooth)
	{
		cVector3f vSafe;
		if(!ohpl_h14_safe_position(mpAux, afElapsedTime, reinterpret_cast<OhplV3*>(&vSafe))) return;
		SetPosition(vSafe, abSmooth);
		mS.velocity = cVector3f(0, 0.1f, 0);
	}

	void iCharacterBody::SetPosition(const cVector3f& avPos, bool abSmooth)
	{
		//mS.force =0;
		//mS.velocity =0;
		mS.lastPosition = avPos;
		mS.position = avPos;

		mpCurrentBody->SetPosition(avPos);

		if(!abSmooth){
			ohpl_h14_clear_smoothing(mpAux);
		}
	}
	const cVector3f& iCharacterBody::GetPosition()
	{
		return mS.position;
	}
	const cVector3f& iCharacterBody::GetLastPosition()
	{
		return mS.lastPosition;
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::SetFeetPosition(const cVector3f& avPos, bool abSmooth)
	{
		SetPosition(avPos + cVector3f(0,mS.size.y/2,0),abSmooth);
	}

	cVector3f iCharacterBody::GetFeetPosition()
	{
		return mS.position - cVector3f(0,mS.size.y/2,0);
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::SetYaw(float afX)
	{
		//Do not set rotation, if character is being aligned.
		if(mpConnectedBody && mbConnectionAlignCharacterRotation) return;

		mS.yaw = afX;
	}

	float iCharacterBody::GetYaw()
	{
		return mS.yaw;
	}

	void iCharacterBody::AddYaw(float afX)
	{
		mS.yaw += afX;
	}

	void iCharacterBody::SetPitch(float afX)
	{
		//Do not set rotation, if character is being aligned.
		if(mpConnectedBody && mbConnectionAlignCharacterRotation) return;

		mS.pitch = afX;
	}

	void iCharacterBody::AddPitch(float afX)
	{
		mS.pitch += afX;
	}

	float iCharacterBody::GetPitch()
	{
		return mS.pitch;
	}

	//-----------------------------------------------------------------------

	const cVector3f& iCharacterBody::GetForward()
	{
		return mS.forward;
	}
	const cVector3f& iCharacterBody::GetRight()
	{
		return mS.right;
	}
	const cVector3f& iCharacterBody::GetUp()
	{
		return mS.up;
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::SetGravityActive(bool abX)
	{
		mS.gravityActive = abX;
	}

	bool iCharacterBody::GravityIsActive()
	{
		return mS.gravityActive;
	}

	void iCharacterBody::SetMaxGravitySpeed(float afX)
	{
		mS.maxGravitySpeed = afX;
	}
	float iCharacterBody::GetMaxGravitySpeed()
	{
		return mS.maxGravitySpeed;
	}

	//-----------------------------------------------------------------------

	bool iCharacterBody::GetCustomGravityActive()
	{
		return mS.customGravityActive;
	}

	void iCharacterBody::SetCustomGravityActive(bool abX)
	{
		mS.customGravityActive = abX;
	}

	void iCharacterBody::SetCustomGravity(const cVector3f& avCustomGravity)
	{
		mS.customGravity = avCustomGravity;
	}

	cVector3f iCharacterBody::GetCustomGravity()
	{
		return mS.customGravity;
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::SetCollideFlags(tFlag alX)
	{
		if(mS.collideFlags == alX) return;

		mS.collideFlags = alX;

		for(size_t i=0; i<mvBodies.size(); ++i)
		{
			iPhysicsBody *pBody = mvBodies[i];
			pBody->SetCollideFlags(mS.collideFlags);
		}
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::SetMaxNoSlideSlopeAngle(float afAngle)
	{
		mS.maxNoSlideSlopeCos = cos(afAngle);
	}

	float iCharacterBody::GetMaxNoSlideSlopeAngle()
	{
		return acos(mS.maxNoSlideSlopeCos);
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::ResetClimbing()
	{
		mS.climbing = false;
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::SetForce(const cVector3f &avForce)
	{
		mS.force = avForce;
	}

	void iCharacterBody::AddForce(const cVector3f &avForce)
	{
		mS.force += avForce;
	}

	cVector3f iCharacterBody::GetForce()
	{
		return mS.force;
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::Move(eCharDir aDir, float afMul)
	{
		if(mS.moveDelayCount>0) return; //Just skip any movement.

		mS.currentMoveAcc[aDir] += afMul;

		mS.moving[aDir] = true;
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::StopMovement()
	{
		SetForceVelocity(0);

		for(int i=0; i<2; ++i)
		{
			mS.currentMoveAcc[i] = 0;
			mS.moving[i] = false;
			mS.moveAcc[i] =0;
			mS.moveSpeed[i]=0;
		}
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::ConnectToBody(	iPhysicsBody *apBody, const cVector3f& avConnectionPos,
										tPhysicsConnectionFlag alFlags)
	{
		if(apBody == NULL) return;
		ohpl_h14_connect_to_body(HostOp, reinterpret_cast<size_t>(this), &mE, &mS, reinterpret_cast<size_t>(apBody),
			OhplV3{avConnectionPos.x, avConnectionPos.y, avConnectionPos.z}, alFlags);
		ohpl_h14_reset_pids(mpAux);
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::DisconnectBody()
	{
		ohpl_h14_disconnect_body(HostOp, reinterpret_cast<size_t>(this), &mE);
	}

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

	void iCharacterBody::SetCamera(cCamera *apCam)
	{
		mpCamera = apCam;
	}

	cCamera* iCharacterBody::GetCamera()
	{
		return mpCamera;
	}

	void iCharacterBody::SetCameraPosAdd(const cVector3f &avAdd)
	{
		mvCameraPosAdd = avAdd;
	}
	cVector3f iCharacterBody::GetCameraPosAdd()
	{
		return mvCameraPosAdd;
	}

	//-----------------------------------------------------------------------

	iCollideShape * iCharacterBody::GetCurrentShape()
	{
		return mpCurrentShape;
	}

	iPhysicsBody* iCharacterBody::GetCurrentBody()
	{
		return mpCurrentBody;
	}

	iCollideShape *iCharacterBody::GetShape(int alIdx)
	{
		return mvShapes[alIdx];
	}

	bool iCharacterBody::IsOnGround()
	{
		return mS.onGroundCount >0;
	}

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

	bool iCharacterBody::CheckRayIntersection(const cVector3f &avStart, const cVector3f &avEnd, float *apDistance, cVector3f *apNormalVec)
	{
		mpRayCallback->Clear();
		mpRayCallback->mlCollideFlags = mS.collideFlags;
		mpWorld->CastRay(mpRayCallback,avStart,avEnd,apDistance!=NULL,apNormalVec!=NULL,false);
		bool bCollide = mpRayCallback->mbCollide;
		if(bCollide)
		{
			if(apDistance) *apDistance = mpRayCallback->mfMinDist;
			if(apNormalVec) *apNormalVec = mpRayCallback->mvNormal;
		}
		return bCollide;
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::SetEntity(iEntity3D *apEntity)
	{
		mpEntity = apEntity;
	}
	iEntity3D* iCharacterBody::GetEntity()
	{
		return mpEntity;
	}
	void iCharacterBody::SetEntityOffset(const cMatrixf &a_mtxOffset)
	{
		m_mtxEntityOffset = a_mtxOffset;
	}
	const cMatrixf& iCharacterBody::GetEntityOffset()
	{
		return m_mtxEntityOffset;
	}

	void iCharacterBody::SetEntityPostOffset(const cMatrixf &a_mtxOffset)
	{
		m_mtxEntityPostOffset = a_mtxOffset;
	}
	const cMatrixf& iCharacterBody::GetEntityPostOffset()
	{
		return m_mtxEntityPostOffset;
	}

	//-----------------------------------------------------------------------

	iPhysicsBody * iCharacterBody::GetConnectedBody()
	{
		return mpConnectedBody;
	}

	cVector3f iCharacterBody::GetBodyConnectionPos()
	{
		OhplH14Conn conn = MakeConn();
		cVector3f vPos;
		ohpl_h14_body_connection_pos(BodyVt(), &conn, reinterpret_cast<OhplV3*>(&vPos));
		return vPos;
	}

	cVector3f iCharacterBody::GetCharConnectionPos()
	{
		OhplH14Conn conn = MakeConn();
		cVector3f vPos;
		ohpl_h14_char_connection_pos(&mS, &conn, reinterpret_cast<OhplV3*>(&vPos));
		return vPos;
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

	void iCharacterBody::PreUpdateConnection(float afTimeStep)
	{
		if(mpConnectedBody == NULL) return;

		if(mbConnectionCollision==false)
		{
			mbConnectedBodyIsActive = mpConnectedBody->IsActive();
			if(mbConnectedBodyIsActive) mpConnectedBody->SetActive(false);
		}
	}

	void iCharacterBody::PostUpdateConnection(float afTimeStep)
	{
		if(mpConnectedBody == NULL) return;

		if(mbConnectionCollision==false)
		{
			if(mbConnectedBodyIsActive) mpConnectedBody->SetActive(true);
		}
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::UpdateBodyConnection(float afTimeStep)
	{
		//No connected body = nothing to do
		if(mpConnectedBody == NULL) return;

		OhplH14Conn conn = MakeConn();
		ohpl_h14_update_body_connection(mpAux, BodyVt(), &mS, &conn, IsOnGround(), afTimeStep);
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::UpdateCharacterConnection(float afTimeStep)
	{
		//If no connection, do nothing
		if(mpConnectedBody == NULL) return;

		OhplH14Conn conn = MakeConn();
		cVector3f vForce;
		if(ohpl_h14_update_character_connection(mpAux, BodyVt(), &mS, &conn, afTimeStep, reinterpret_cast<OhplV3*>(&vForce)))
			AddForce(vForce);
	}

	//-----------------------------------------------------------------------



	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

	static const OhplH14BodyVt* BodyVt()
	{
		static const OhplH14BodyVt vt = {
			[](size_t b) { return reinterpret_cast<iPhysicsBody*>(b)->GetMass(); },
			[](size_t b, cMatrixf *m) { *m = reinterpret_cast<iPhysicsBody*>(b)->GetWorldMatrix(); },
			[](size_t b, cMatrixf *m) { *m = reinterpret_cast<iPhysicsBody*>(b)->GetLocalMatrix(); },
			[](size_t b) { cVector3f v = reinterpret_cast<iPhysicsBody*>(b)->GetWorldPosition(); return OhplV3{v.x, v.y, v.z}; },
			[](size_t b, cMatrixf *m) { *m = reinterpret_cast<iPhysicsBody*>(b)->GetInertiaMatrix(); },
			[](size_t b, OhplV3 f, OhplV3 p)
			{
				cVector3f v = reinterpret_cast<iPhysicsBody*>(b)->GetTorqueFromForceAtPosition(cVector3f(f.x, f.y, f.z), cVector3f(p.x, p.y, p.z));
				return OhplV3{v.x, v.y, v.z};
			},
			[](size_t b, OhplV3 p)
			{
				cVector3f v = reinterpret_cast<iPhysicsBody*>(b)->GetVelocityAtPosition(cVector3f(p.x, p.y, p.z));
				return OhplV3{v.x, v.y, v.z};
			},
			[](size_t b, OhplV3 f) { reinterpret_cast<iPhysicsBody*>(b)->AddForce(cVector3f(f.x, f.y, f.z)); },
			[](size_t b, OhplV3 t) { reinterpret_cast<iPhysicsBody*>(b)->AddTorque(cVector3f(t.x, t.y, t.z)); },
			[](size_t b) { return reinterpret_cast<iPhysicsBody*>(b)->GetGravityAttachmentRotation(); },
			[](size_t b) { return reinterpret_cast<iPhysicsBody*>(b)->GetGravityAttachmentVelocity(); },
			[](size_t b) { return (int32_t)reinterpret_cast<iPhysicsBody*>(b)->GetGravityAttachmentVelocityAxes(); },
		};
		return &vt;
	}

	OhplH14Conn iCharacterBody::MakeConn()
	{
		OhplH14Conn conn = {
			reinterpret_cast<size_t>(mpConnectedBody), mfMaxConnectionForce, mfMaxConnectionTorque, mfConnectionDistanceToBody,
			mvConnectionPosLocalToBody, mvConnectionPosLocalToChar, mvConnectionNormalLocalToBody, mvConnectionLocalUpBody,
			mvConnectionBodyStartRotation, mvConnectionCharLocalForward, mpWorld->GetGravity(),
			mbConnectionDependOnBodyRotation, mbConnectionAlignCenter, mbConnectionAlignRotation, mbConnectionAffectChar,
			mbConnectionDependOnCharRotation, mbConnectionAlignCharacterRotation
		};
		return conn;
	}

	OhplCharVt iCharacterBody::MakeVt()
	{
		OhplCharVt vt = {
			this,
			[](void *c) { cVector3f g = static_cast<iCharacterBody*>(c)->mpWorld->GetGravity(); return OhplV3{g.x, g.y, g.z}; },
			[](void *c) { return static_cast<iCharacterBody*>(c)->mpCurrentShape->GetRadius(); },
			[](void *c, int32_t i) { return static_cast<iCharacterBody*>(c)->mvShapes[i]->GetSize().y; },
			[](void *c, OhplV3 *apPush, OhplV3 aPos, int32_t alCallback, int32_t alIdx)
			{
				iCharacterBody *p = static_cast<iCharacterBody*>(c);
				iPhysicsWorldCollisionCallback *pCallback = alCallback == 1 ? (iPhysicsWorldCollisionCallback*)p->mpCollidePushCallback :
					alCallback == 2 ? (iPhysicsWorldCollisionCallback*)p->mpCollideGravityCallback : NULL;
				return p->CheckCollision(reinterpret_cast<cVector3f*>(apPush), cVector3f(aPos.x, aPos.y, aPos.z), pCallback, alIdx);
			},
			[](void *c, OhplV3 aStart, OhplV3 aEnd, float *apDist, OhplV3 *apNormal)
			{
				return static_cast<iCharacterBody*>(c)->CheckRayIntersection(cVector3f(aStart.x, aStart.y, aStart.z),
					cVector3f(aEnd.x, aEnd.y, aEnd.z), apDist, reinterpret_cast<cVector3f*>(apNormal));
			},
			[](void *c) { iCharacterBody *p = static_cast<iCharacterBody*>(c); if(p->mpCallback) p->mpCallback->OnHitGround(p, p->mS.velocity); },
			[](void *c, float dt) { static_cast<iCharacterBody*>(c)->PreUpdateConnection(dt); },
			[](void *c) { static_cast<iCharacterBody*>(c)->ClearGravityAttachment(); },
			[](void *c) { static_cast<iCharacterBody*>(c)->UpdateGravityAttachment(); },
			[](void *c) { static_cast<iCharacterBody*>(c)->UpdateBody(); },
			[](void *c, float dt) { static_cast<iCharacterBody*>(c)->UpdateCharacterConnection(dt); },
			[](void *c, float dt) { static_cast<iCharacterBody*>(c)->UpdateBodyConnection(dt); },
			[](void *c) { static_cast<iCharacterBody*>(c)->UpdateCamera(); },
			[](void *c) { static_cast<iCharacterBody*>(c)->UpdateEntity(); },
			[](void *c, float dt) { static_cast<iCharacterBody*>(c)->PostUpdateConnection(dt); },
			[](void *c, float dt) { static_cast<iCharacterBody*>(c)->UpdateSafePositions(dt); },
		};
		return vt;
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::Update(float afTimeStep)
	{
		OhplCharVt vt = MakeVt();
		ohpl_char_update(&vt, &mS, afTimeStep, mbHpl3);
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::UpdateMoveMatrix()
	{
		ohpl_char_update_move_matrix(&mS);
	}

	//-----------------------------------------------------------------------

	bool iCharacterBody::CheckCharacterFits(const cVector3f &avPosition, bool abFeetPosition, int alSizeIdx, cVector3f *apPushBackVec, float afEpsilon)
	{
		if(alSizeIdx <0) alSizeIdx = mS.currentShapeIdx;

		OhplCharVt vt = MakeVt();
		cVector3f vPushBack(0);
		bool bFits = ohpl_char_check_fits(&vt, OhplV3{avPosition.x, avPosition.y, avPosition.z}, abFeetPosition, alSizeIdx, afEpsilon,
										  reinterpret_cast<OhplV3*>(&vPushBack));
		if(apPushBackVec) *apPushBackVec = vPushBack;
		return bFits;
	}

	//-----------------------------------------------------------------------

	bool iCharacterBody::mbHpl3 = false;


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	void iCharacterBody::UpdateCamera()
	{
		if(mpCamera==NULL) return;
		OhplH14CamIn in = {
			mS.position, mS.right, mS.forward, mvCameraPosAdd,
			mvShapes[0]->GetSize().y, mS.size.y, mS.yaw, mlCameraSmoothPosNum, mbCameraUpdateActive, mbCameraUseSmoothing
		};
		OhplH14CamOut out;
		ohpl_h14_update_camera(mpAux, &in, &out);
		if(out.set)
		{
			mpCamera->SetPosition(out.position);
			mpCamera->SetYaw(out.yaw);
		}
	}

	//-----------------------------------------------------------------------


	void iCharacterBody::UpdateEntity()
	{
		if(mpEntity==NULL) return;
		OhplH14EntIn in = {
			mS.position, mS.mtxMove, m_mtxEntityOffset, m_mtxEntityPostOffset,
			mS.yaw, mlEntitySmoothPosNum, mlEntitySmoothYPosNum, mbEntitySmoothYPos
		};
		cMatrixf mtxEntity;
		ohpl_h14_update_entity(mpAux, &in, &mtxEntity);
		mpEntity->SetMatrix(mtxEntity);
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::UpdateBody()
	{
		mpCurrentBody->SetPosition(mS.position);
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::ClearGravityAttachment()
	{
		mpGravityLastAttachedBody = mpGravityAttachedBody;
		mpGravityAttachedBody = NULL;
 		mlGravityAttachmentContacts = 0;
	}

	//-----------------------------------------------------------------------


	void iCharacterBody::UpdateGravityAttachment()
	{
		OhplH14Grav grav = {
			reinterpret_cast<size_t>(mpGravityAttachedBody), reinterpret_cast<size_t>(mpGravityLastAttachedBody),
			m_mtxGravityAttachedPrevMatrix, mvGravityAttachmentBodyVelocity, mbRotateYawWhenGravityAttached
		};
		ohpl_h14_update_gravity_attachment(BodyVt(), &mS, &grav);
		if(mpGravityAttachedBody)
		{
			mvGravityAttachmentBodyVelocity = grav.bodyVelocity;
			m_mtxGravityAttachedPrevMatrix = grav.prevMatrix;
		}
	}

	//-----------------------------------------------------------------------

	bool iCharacterBody::CheckCollision(cVector3f *apPushBackVector, const cVector3f& avPos, iPhysicsWorldCollisionCallback *apCallback,
										int alShapeIdx)
	{
		if(alShapeIdx <0) alShapeIdx = mS.currentShapeIdx;
		iCollideShape *pShape = mvShapes[alShapeIdx];

		return mpWorld->CheckShapeWorldCollision(apPushBackVector, pShape, cMath::MatrixTranslate(avPos),
												mpCurrentBody, false, true,
												apCallback, true,mlMinBodyPushStrength,
												mS.collideFlags, false);
	}

	//-----------------------------------------------------------------------

}
