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
		mpCharBody->mpGravityCollideMaterial = apBody->GetMaterial();

		/////////////////////////////////////
		//Check if body allow character attachment, if so attach character to it
		if(	apBody->GetGravityCanAttachCharacter() && apBody->IsCharacter()==false &&
			apCollideData->mvContactPoints[0].mvNormal.y > 0.001f)	//Quick check to make sure that character is just not sliding against the body.
		{
			//Choose the body that has the most contacts.
			if(	mpCharBody->mlGravityAttachmentContacts < apCollideData->mlNumOfPoints)
			{
				mpCharBody->mlGravityAttachmentContacts = apCollideData->mlNumOfPoints;
				mpCharBody->mpGravityAttachedBody = apBody;
			}
		}

		if(mpCharBody->mpCallback) mpCharBody->mpCallback->OnGravityCollide(mpCharBody,apBody,apCollideData);

		if(apBody->GetMass()==0 || apBody->GetPushedByCharacterGravity()==false) return;

		//////////////////////////////////////
		//Go through all of the contact points and check if any is a movement up.
		//This means the body is below the character and should be pushed down.
		bool bPushDown=false;
		cVector3f vMedianPoint(0,0,0);
		float fNumPoints=0;
		for(int i=0; i< apCollideData->mlNumOfPoints; i++)
		{
			//TODO: Get the point at which to apply the force.
			cCollidePoint &point = apCollideData->mvContactPoints[i];
			if(point.mvNormal.y > 0.001f)
			{
				bPushDown = true;
				fNumPoints+=1;
				vMedianPoint += point.mvPoint;
			}
		}

		if(bPushDown)
		{
			vMedianPoint = vMedianPoint / fNumPoints;

			//Transfer the velocity of the player to the body.
			// SKIPPING THIS! For now. It makes stuff very unstable.
			/*if(mpCharBody->GetForceVelocity().y < 0.1f && mpCharBody->IsOnGround()==false)
			{
				float fImpulse = mpCharBody->GetForceVelocity().y * (mpCharBody->GetMass() / apBody->GetMass());
				apBody->AddImpulseAtPosition(cVector3f(0,fImpulse,0), vMedianPoint);
			}*/

			float fForceMul = mpCharBody->mS.customGravityActive ? mpCharBody->mS.customGravity.y : mpCharBody->mpWorld->GetGravity().y;
			if(fForceMul < 0)
				apBody->AddForceAtPosition(cVector3f(0,mpCharBody->GetMass() * fForceMul,0), vMedianPoint);
		}
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
		///////////////////////////////////
		//Check what bodies not to push.
		if(apBody->GetMass()==0 && apBody->IsCharacter()==false) return;
		if(apBody->GetMass() > mpCharBody->GetMaxPushMass()) return;

		///////////////////////////////
		//Character is hit
		if(apBody->IsCharacter())
		{
			iCharacterBody *pHitCharBody = apBody->GetCharacterBody();
			if(pHitCharBody->GetMass() > mpCharBody->GetCharacterMaxPushMass()) return;


			float fHitTop = pHitCharBody->GetPosition().y + pHitCharBody->GetSize().y/2;
			//Make bottom 10% of size longer up to remove uneeded pushing.
			float fBottom = mpCharBody->GetPosition().y - (mpCharBody->GetSize().y/2 - mpCharBody->GetSize().y*0.1f);

			//Log("Top: %f Bottom: %f. HitSize: %f Size: %f. HitPos: %f Pos: %f\n",fHitTop, fBottom,pHitCharBody->GetSize().y,mpCharBody->GetSize().y,
			//																		pHitCharBody->GetPosition().y,mpCharBody->GetPosition().y);

			//If body is on top of hitbody, skip doing a push.
			if(fHitTop < fBottom) return;

			cVector3f vDir = pHitCharBody->GetPosition() - mpCharBody->GetPosition();
			if(mpCharBody->GetCharacterPushIn2D()) vDir.y =0;
			vDir.Normalize();

			pHitCharBody->AddForce(vDir * mpCharBody->GetCharacterPushForce());
		}
		//////////////////////////////
		//Normal body is hit
		else
		{
				/////////////////////////////////////////
			//Go through all of the contact points and find median
			cVector3f vMedianPoint(0,0,0);
			float fNumPoints=0;

			for(int i=0; i< apCollideData->mlNumOfPoints; i++)
			{
				cCollidePoint &point = apCollideData->mvContactPoints[i];

				fNumPoints+=1;
				vMedianPoint += point.mvPoint;
			}
			vMedianPoint = vMedianPoint / fNumPoints;

			//If median is below (or almost below the player), skip push
			float fMinY = mpCharBody->GetFeetPosition().y;
			fMinY += 0.01f;
			if(vMedianPoint.y  <  fMinY) return;

			float fForce = mpCharBody->GetPushForce();
			if(mpCharBody->GetPushImpulse() > 0) fForce = cMath::Min(fForce, apBody->GetMass() * mpCharBody->GetPushImpulse());

			if(mpCharBody->GetPushIn2D())
			{
				cVector3f vDir = vMedianPoint - mpCharBody->GetPosition();
				vDir.y =0; vDir.Normalize();

				apBody->AddForceAtPosition(vDir * fForce, vMedianPoint);
			}
			else
			{
				cVector3f vDir = cMath::Vector3Normalize(vMedianPoint - mpCharBody->GetPosition());

				apBody->AddForceAtPosition(vDir * fForce, vMedianPoint);
			}
		}
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
		}
		return 0;
	}

	iCharacterBody::iCharacterBody(const tString &asName, iPhysicsWorld *apWorld, const cVector3f avSize)
	{
		mpAux = OhplBackendRustRequested() ? ohpl_h14_aux_new() : NULL;

		/////////////////////////////
		//Set up main properties
		msName = asName;

		mS.size = avSize;
		if(mpAux)
		{
			ohpl_h14_char_init(&mE, &mS, reinterpret_cast<size_t>(apWorld), cos(cMath::ToRad(45.0f)));
		}
		else
		{
		mpWorld = apWorld;

		mS.collideFlags = eFlagBit_All; // orig read it uninitialised below

		mpCallback = NULL;
		}

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

		if(mpAux) return;

		/////////////////////////////
		//Set up properties

		mS.active = true;
		mbCollideCharacter = true;
		mS.testCollision = true;
		mbPhysicsBodyActive = true;

		mbEntitySmoothYPos = false;
		mlEntitySmoothYPosNum = 20;

		mS.disableDiagSpeedBoost = true;

		mS.currentShapeIdx =0;

		mS.yaw =0;
		mS.pitch =0;

		mS.collideFlags = eFlagBit_All;

		//The maximum angle of a slope (compare to gravity direction) where no sliding occurs
		mS.maxNoSlideSlopeCos = cos(cMath::ToRad(45.0f));

		//Set move properties
		for(int i=0; i<eCharDir_LastEnum; i++)
		{
			mS.maxPosMoveSpeed[i]=10;
			mS.maxNegMoveSpeed[i]=-10;
			mS.moveSpeed[i]=0;
			mS.moveSpeedMul[i]=1;
			mS.moveAcc[i]=20;
			mS.moveDeacc[i]=20;
			mS.moving[i]=false;
			mS.currentMoveAcc[i] =0;
			mS.moveOppositeDirAccMul[i] = 1;
		}

		mS.movedLastUpdate = false;

		mS.deaccelerateMoveSpeedInAir = false;

		mS.force = cVector3f(0,0,0);
		mS.velocity = cVector3f(0,0,0);
		mfSafePosTimer = mfSafePosInterval = mfSafePosBuffer = 0;

		mS.lastGroundNormal = cVector3f(0,0,0);

		mS.gravityActive = true;

		mS.maxGravitySpeed = 30.0f;

		mfConstantContactForceMul = 16.0f;
		mfVelocityContactForceMul = 4.0f;
		mfMaxContactForcePerMassUnit = 250.0f;

		mpCamera = NULL;
		mvCameraPosAdd = cVector3f(0,0,0);

		mpEntity = NULL;
		m_mtxEntityOffset = cMatrixf::Identity;
		m_mtxEntityPostOffset = cMatrixf::Identity;

		mpUserData = NULL;

		mlCameraSmoothPosNum =0;
		mbCameraUseSmoothing = true;
		mbCameraUpdateActive = true;
		mlEntitySmoothPosNum =0;

		mS.onGroundCount = 0;
		mS.maxOnGroundCount = 12;

		mS.maxStepHeight = mS.size.y *0.2f;
		mS.maxStepHeightInAir = mS.maxStepHeight;
		mS.stepClimbSpeed = 1.0f;
		mS.climbForwardMul = 1.0f;
		mS.climbHeightAdd = 0.01f;
		mS.climbing = false;
		mS.accurateClimbing = false;

		mS.checkStepClimbCount = 0;
		mS.checkStepClimbInterval = 1/20.0f;

		mS.groundFriction = 6.0f;
		mS.airFriction = 0.3f;

		mS.mass = 1;

		mbStickToSlope = true;

		mfMaxPushMass = 0;
		mfPushForce = 0;
		mfPushImpulse = 0;
		mbPushIn2D = true;

		mfCharacterMaxPushMass =0;
		mfCharacterPushForce =0;
		mbCharacterPushIn2D = true;

		mlMinBodyPushStrength = 0;

		mfMaxConnectionTorque =0;
		mfMaxConnectionForce =0;
		mpConnectedBody = NULL;
		mBodyConnectionPointPid.SetErrorNum(10);
		mBodyCenterPid.SetErrorNum(10);
		mBodyOneAxisAngularPid.SetErrorNum(10);
		mBodyAngularPid.SetErrorNum(10);
		mCharConnectionPointPid.SetErrorNum(10);
		mCharRotatePid.SetErrorNum(10);

		mbRotateYawWhenGravityAttached = true;
		mpGravityAttachedBody = NULL;
		mlGravityAttachmentContacts =0;
		mpGravityLastAttachedBody = NULL;
		mS.gravityAttachmentVelocity =0;

		mS.customGravityActive = false;
		mS.customGravity = cVector3f(0,9.8f,0);

		mpGravityCollideMaterial = NULL;

		mS.moveDelayCount=0;
	}

	//-----------------------------------------------------------------------

	iCharacterBody::~iCharacterBody()
	{
		DisconnectBody();

		for(size_t i=0; i< mvBodies.size(); i++) mpWorld->DestroyBody(mvBodies[i]);

		if(mpAux) ohpl_h14_aux_free(mpAux);

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
		if(mpAux) ohpl_h14_setup_safe_positions(mpAux, afSaveInterval, afBufferTime);
		if(afBufferTime <= 0) return;
		mfSafePosTimer = 0;
		mfSafePosInterval = afSaveInterval;
		mfSafePosBuffer = afBufferTime;
		for(int i=0; i<4; ++i) mvSafePosHistory[i] = cVector3f(-1000000.0f);
		mvSafePositions.clear();
		mvSafePositions.reserve((size_t)ceilf(afBufferTime / afSaveInterval) + 2);
	}

	// cCharacterBodyNewton::PostUpdate 0x14b4a20, iCharacterBody::TeleportToSafePosition 0xe787c0 (Rebirth)
	void iCharacterBody::UpdateSafePositions(float afTimeStep)
	{
		if(mpAux)
		{
			ohpl_h14_update_safe_positions(mpAux, afTimeStep, OhplV3{mS.position.x, mS.position.y, mS.position.z});
			return;
		}
		if(mfSafePosInterval <= 0) return;
		for(int i=0; i<3; ++i) mvSafePosHistory[i] = mvSafePosHistory[i+1];
		mvSafePosHistory[3] = mS.position;
		mfSafePosTimer -= afTimeStep;
		if(mfSafePosTimer > 0) return;
		mfSafePosTimer = mfSafePosInterval;
		if(mvSafePosHistory[0] == cVector3f(-1000000.0f)) return;
		mvSafePositions.push_back(mvSafePosHistory[0]);
		if((float)mvSafePositions.size() > mfSafePosBuffer / mfSafePosInterval) mvSafePositions.pop_back();
	}

	void iCharacterBody::TeleportToSafePosition(float afElapsedTime, bool abSmooth)
	{
		if(mpAux)
		{
			cVector3f vSafe;
			if(!ohpl_h14_safe_position(mpAux, afElapsedTime, reinterpret_cast<OhplV3*>(&vSafe))) return;
			SetPosition(vSafe, abSmooth);
			mS.velocity = cVector3f(0, 0.1f, 0);
			return;
		}
		if(mvSafePositions.empty()) return;
		int lIdx = (int)floorf(afElapsedTime / mfSafePosInterval + 0.5f);
		lIdx = cMath::Min(lIdx, (int)mvSafePositions.size() - 1);
		SetPosition(mvSafePositions[lIdx], abSmooth);
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
			if(mpAux) ohpl_h14_clear_smoothing(mpAux);
			mlstCameraPos.clear();
			mlstEntityPos.clear();
			mlstEntityYPositions.clear();
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
		if(mpAux)
		{
			ohpl_h14_connect_to_body(HostOp, reinterpret_cast<size_t>(this), &mE, &mS, reinterpret_cast<size_t>(apBody),
				OhplV3{avConnectionPos.x, avConnectionPos.y, avConnectionPos.z}, alFlags);
			ohpl_h14_reset_pids(mpAux);
			return;
		}

		//////////////////////////////////////////
		// Set the connected body and add charater as connected on body
		mpConnectedBody = apBody;
		mpConnectedBody->AddConnectedCharacter(this);

		//////////////////////////////////////////
		// Set if connection depends on body rotation.
		mbConnectionDependOnBodyRotation = (alFlags & ePhysicsConnectionFlag_DependOnBodyRotation) != 0;

		////////////////////////////////
		//Get inverse body matrix
        cMatrixf mtxInvBodyWorld = cMath::MatrixInverse(mpConnectedBody->GetLocalMatrix());

		//////////////////////////////////////////
		// Set the local connection postion for body
        if(mbConnectionDependOnBodyRotation)
		{
			mvConnectionPosLocalToBody = cMath::MatrixMul(mtxInvBodyWorld, avConnectionPos);
		}
		else
		{
			mvConnectionPosLocalToBody = avConnectionPos - mpConnectedBody->GetWorldPosition();
		}

		//////////////////////////////////////////
		// Get the start rotation for body
		mvConnectionBodyStartRotation = cMath::MatrixToEulerAngles(mpConnectedBody->GetLocalMatrix().GetRotation(), eEulerRotationOrder_XYZ);

		//////////////////////////////////////////
		// Set up pids
		if(mpAux) ohpl_h14_reset_pids(mpAux);
		mBodyConnectionPointPid.Reset();
		mBodyCenterPid.Reset();
		mBodyOneAxisAngularPid.Reset();
		mBodyAngularPid.Reset();

		mCharConnectionPointPid.Reset();
		mCharRotatePid.Reset();

		//////////////////////////////////////////
		// More body properties
		mbConnectionAlignRotation = (alFlags & ePhysicsConnectionFlag_AlignBodyRotation) != 0;
		mbConnectionAlignCenter = (alFlags & ePhysicsConnectionFlag_AlignBodyCenter) != 0;

		//Connection point to body normal.
        mvConnectionNormalLocalToBody = cMath::Vector3Normalize(mpConnectedBody->GetWorldPosition() - avConnectionPos);
		mfConnectionDistanceToBody = mvConnectionPosLocalToBody.Length();

		//The local wanted up vector (when this vector is up, body is correct!)
		mvConnectionLocalUpBody = cMath::MatrixMul(mtxInvBodyWorld.GetRotation(), GetUp());
		mvConnectionLocalUpBody.Normalize();

		///////////////////////////////////
		// Disable gravity or not
		if(alFlags & ePhysicsConnectionFlag_DisableBodyGravity)
		{
			mbConnectedBodyHadGravity = mpConnectedBody->GetGravity();
			if(mbConnectedBodyHadGravity)
				mpConnectedBody->SetGravity(false);
		}

		///////////////////////////////////
		// Connection affects character
		mbConnectionAffectChar = (alFlags & ePhysicsConnectionFlag_AffectChar) != 0;

		///////////////////////////////////
		//Local character connection point
		mvConnectionPosLocalToChar = avConnectionPos - mS.position;


		///////////////////////////////////
		// Align character rotation
		mbConnectionAlignCharacterRotation = (alFlags & ePhysicsConnectionFlag_AlignCharRotation) != 0;
		if(mbConnectionAlignCharacterRotation)
		{
			//Make sure it is local, with no rotation from body.
			mvConnectionCharLocalForward = cMath::MatrixMul(mtxInvBodyWorld.GetRotation(), mS.forward);

			//Character to connection vector
			cMatrixf mtxInvCharRot = cMath::MatrixInverse(mS.mtxMove.GetRotation());
			mvConnectionPosLocalToChar = cMath::MatrixMul(mtxInvCharRot, mvConnectionPosLocalToChar);
		}

		///////////////////////////////////
		// Depend on character rotation

		//Do not allow connection dependent on rotation when this is true!
		if(mbConnectionAlignCharacterRotation)
			mbConnectionDependOnCharRotation = false;
		else
			mbConnectionDependOnCharRotation = (alFlags & ePhysicsConnectionFlag_DependOnCharRotation) != 0;

		if(mbConnectionDependOnCharRotation)
		{
			//If body is aligned according to character rotation, then we need to make sure vectors are local when rotation is 0,
			//hence we multiply with inverse rotation.
			UpdateMoveMatrix();
			cMatrixf mtxInvCharRot = cMath::MatrixInverse(mS.mtxMove.GetRotation());

			//Character to connection vector
			mvConnectionPosLocalToChar = cMath::MatrixMul(mtxInvCharRot, mvConnectionPosLocalToChar);

			//Connection to body normal
			mvConnectionNormalLocalToBody = cMath::MatrixMul(mtxInvCharRot, mvConnectionNormalLocalToBody);
		}


		////////////////////////////////////////
		//If collision between connected character and body is allowed.
		mbConnectionCollision = (alFlags & ePhysicsConnectionFlag_AllowCollision) != 0;

		mbConnectedBodyIsActive = mpConnectedBody->IsActive();

	}

	//-----------------------------------------------------------------------

	void iCharacterBody::DisconnectBody()
	{
		if(mpAux){ ohpl_h14_disconnect_body(HostOp, reinterpret_cast<size_t>(this), &mE); return; }

		if(mpConnectedBody == NULL) return;

		mpConnectedBody->RemoveConnectedCharacter(this);

		if(mbConnectedBodyHadGravity)
			mpConnectedBody->SetGravity(true);

		mpConnectedBody = NULL;
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
		if(mpAux)
		{
			OhplH14Conn conn = MakeConn();
			cVector3f vPos;
			ohpl_h14_body_connection_pos(BodyVt(), &conn, reinterpret_cast<OhplV3*>(&vPos));
			return vPos;
		}
		if(mpConnectedBody == NULL) return 0;

		if(mbConnectionDependOnBodyRotation)
			return cMath::MatrixMul(mpConnectedBody->GetWorldMatrix(), mvConnectionPosLocalToBody);
		else
			return mpConnectedBody->GetWorldPosition() + mvConnectionPosLocalToBody;
	}

	cVector3f iCharacterBody::GetCharConnectionPos()
	{
		if(mpAux)
		{
			OhplH14Conn conn = MakeConn();
			cVector3f vPos;
			ohpl_h14_char_connection_pos(&mS, &conn, reinterpret_cast<OhplV3*>(&vPos));
			return vPos;
		}
		if(mpConnectedBody == NULL) return 0;

		if(mbConnectionDependOnCharRotation || mbConnectionAlignCharacterRotation)
			return mS.position + cMath::MatrixMul3x3(mS.mtxMove, mvConnectionPosLocalToChar);
		else
			return mS.position + mvConnectionPosLocalToChar;
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

		if(mpAux)
		{
			OhplH14Conn conn = MakeConn();
			ohpl_h14_update_body_connection(mpAux, BodyVt(), &mS, &conn, IsOnGround(), afTimeStep);
			return;
		}

		//Nothing to do on static bodies.
		if(mpConnectedBody->GetMass() == 0) return;

		cVector3f vTotalTorque=0;
		cVector3f vTotalForce=0;

		//Get the maximum force can be made by the character
		float fMaxTotalCharForce = mfMaxConnectionForce;
		if(IsOnGround()==false)
		{
			float fGravitySize = mS.customGravityActive ? mS.customGravity.Length() : mpWorld->GetGravity().Length();
			fMaxTotalCharForce = fGravitySize * GetMass();
		}
		float fMaxRelativeCharForce = fMaxTotalCharForce / mpConnectedBody->GetMass();
		float fMaxRelativeCharTorque = mfMaxConnectionTorque / mpConnectedBody->GetMass(); //Not really correct


		//////////////////////////////////
		// Linear ConnectionPoint Correction

		//Set up PID (move this SetConnectedBody?)
		mBodyConnectionPointPid.p = 200.0f;
		mBodyConnectionPointPid.i = 0.0f;
		mBodyConnectionPointPid.d = 10.0f;

		//Get the body position of the connection positions
		cVector3f vBodyConnection = GetBodyConnectionPos();
		cVector3f vCharConnection = GetCharConnectionPos();

		cVector3f vError = vCharConnection - vBodyConnection;
		cVector3f vForce = mBodyConnectionPointPid.Output(vError, afTimeStep);

		//Make sure this force is not too much for player, do this here to get correct torque!
		cMath::Vector3ClampToLength(vForce, fMaxRelativeCharForce);

		vTotalForce += vForce;

		//Only use torque if connection depends on rotation!
		if(mbConnectionDependOnBodyRotation)
		{
			cVector3f vTorque = mpConnectedBody->GetTorqueFromForceAtPosition(vForce, vBodyConnection);
			vTotalTorque += vTorque;
		}

		//get the normal form connection to body, this will change if connections depends on body rotation.
		cVector3f vNormalToBody = mvConnectionNormalLocalToBody;
		if(mbConnectionDependOnCharRotation)
		{
			vNormalToBody = cMath::MatrixMul(mS.mtxMove.GetRotation(), vNormalToBody);
		}


		//////////////////////////////////
		// Linear Center Correction
		if(mbConnectionAlignCenter)
		{
			//Setup bid
			mBodyCenterPid.p = 200.0f;
			mBodyCenterPid.i = 0.0f;
			mBodyCenterPid.d = 10.0f;

			cVector3f vCenter = mpConnectedBody->GetWorldPosition();
			cVector3f vWantedPosition = vCharConnection + vNormalToBody * mfConnectionDistanceToBody;

			vError = vWantedPosition - vCenter;
			vForce = mBodyCenterPid.Output(vError, afTimeStep);

			vTotalForce += vForce;
		}


		//////////////////////////////////
		// Angular correction
		if(mbConnectionAlignRotation)
		{
			//////////////////////////////////
			// The connection depends on rotation, meaning we need to rotate around the connection axis
            if(mbConnectionDependOnBodyRotation)
			{
				//Set up pid
				mBodyOneAxisAngularPid.p = 20.0f;
				mBodyOneAxisAngularPid.i = 0.0f;
				mBodyOneAxisAngularPid.d = 2.5f;

				//The current direction of up, given that up was == character up when connected.
				cVector3f vBodyUp = cMath::MatrixMul(mpConnectedBody->GetWorldMatrix().GetRotation(), mvConnectionLocalUpBody);

				//The right vector if connection to body is forward.
				cVector3f vBodyRight = cMath::Vector3Cross(vNormalToBody, GetUp());

				float fError =  cMath::Vector3Angle(vBodyUp, GetUp());

				//Get by checking how the up is compared to right, we can get the sign of the rotation.
				// ">" since the error is in the opposite direction, ie the way want to rotate in!
				if(cMath::Vector3Dot(vBodyRight, vBodyUp)>0) fError = -fError;

				//Log("Error: %f\n", cMath::ToDeg(fError));
				float fTorqueSize = mBodyOneAxisAngularPid.Output(fError, afTimeStep);

				cVector3f vTorque = vNormalToBody * fTorqueSize;
				cMath::Vector3ClampToLength(vTorque, fMaxRelativeCharTorque);

				vTotalTorque += vTorque;
			}
			////////////////////////////////////
			//Rotate around all axes
			else
			{
				//Set up pid
				mBodyAngularPid.p = 20.0f;
				mBodyAngularPid.i = 0.0f;
				mBodyAngularPid.d = 2.5f;

				cVector3f vCurrentRotation = cMath::MatrixToEulerAngles(mpConnectedBody->GetLocalMatrix().GetRotation(), eEulerRotationOrder_XYZ);
				cVector3f vError = mvConnectionBodyStartRotation - vCurrentRotation;

				cVector3f vTorque = mBodyAngularPid.Output(vError, afTimeStep);
				cMath::Vector3ClampToLength(vTorque, fMaxRelativeCharTorque);

				vTotalTorque += vTorque;
			}
		}

		//////////////////////////////////
		// Apply forces
		const float fMaxTorque = 10.0f;
		const float fMaxForce = 100.0f;

		//Make sure that the force is not too large before multiplying with mass
		cMath::Vector3ClampToLength(vTotalForce,fMaxForce);
		cMath::Vector3ClampToLength(vTotalForce,fMaxTotalCharForce);

		vTotalForce = vTotalForce * mpConnectedBody->GetMass();
		mpConnectedBody->AddForce(vTotalForce);

		//Make sure that the force is not too large before multiplying with intertia and mass
		cMath::Vector3ClampToLength(vTotalTorque, fMaxTorque);

		vTotalTorque =  cMath::MatrixMul(mpConnectedBody->GetInertiaMatrix(),vTotalTorque);
		mpConnectedBody->AddTorque(vTotalTorque);
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::UpdateCharacterConnection(float afTimeStep)
	{
		//If no connection, do nothing
		if(mpConnectedBody == NULL) return;

		if(mpAux)
		{
			OhplH14Conn conn = MakeConn();
			cVector3f vForce;
			if(ohpl_h14_update_character_connection(mpAux, BodyVt(), &mS, &conn, afTimeStep, reinterpret_cast<OhplV3*>(&vForce)))
				AddForce(vForce);
			return;
		}

		//Check if character is affected.
		if(mbConnectionAffectChar==false) return;

		//////////////////////////////////////
		// Align rotation (Only works for yaw for the time being)
		if(mbConnectionAlignCharacterRotation)
		{
			//-1 since normal is toward char otherwise.
			cVector3f vWantedForward = cMath::MatrixMul(mpConnectedBody->GetWorldMatrix().GetRotation(), mvConnectionCharLocalForward);

			cVector3f vWantedForwardXZ = cMath::Vector3Normalize(cVector3f(vWantedForward.x, 0, vWantedForward.z));

			//Get the angle from character to the body connection point.
			float fWantedYaw = cMath::Vector3Angle(cVector3f(0,0,-1), vWantedForwardXZ);
			if(vWantedForwardXZ.x >0) fWantedYaw = -fWantedYaw;

			//Offset that angle to get yaw
			mS.yaw = fWantedYaw;

			UpdateMoveMatrix();
		}

		//////////////////////////////////////
		// Align character to connection

		//Set up PID (move this SetConnectedBody?)
		mCharConnectionPointPid.p = 100.0f;
		mCharConnectionPointPid.i = 0.0f;
		mCharConnectionPointPid.d = 0.0f;

		//Get the body position of the connection positions
		cVector3f vBodyConnection = GetBodyConnectionPos();
		cVector3f vCharConnection = GetCharConnectionPos();

		cVector3f vError = vBodyConnection - vCharConnection;
		cVector3f vForce = mCharConnectionPointPid.Output(vError, afTimeStep);

        AddForce(vForce * GetMass());
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


	void iCharacterBody::UpdateForcePushing(float afTimeStep)
	{
		cCollideData collideData;
		collideData.SetMaxSize(32);

		cCollideData extraCollideData;
		extraCollideData.SetMaxSize(1);

		cBoundingVolume boundingVolume = mpCurrentShape->GetBoundingVolume();
		boundingVolume.SetPosition(mS.position);

		cMatrixf mtxTransform = cMath::MatrixTranslate(mS.position);

		//////////////////////////////
        // Iterate bodies
		mvTempBodies.resize(0);
		mpWorld->GetBodiesInBV(&boundingVolume, &mvTempBodies);

		for(size_t i=0; i<mvTempBodies.size(); ++i)
		{
			iPhysicsBody *pBody = mvTempBodies[i];

			if(pBody->IsActive()==false) continue;
            if(pBody->GetMass() == 0) continue;
			if(pBody->IsCharacter() && mbCollideCharacter==false) continue;
			if(pBody->GetCollideCharacter()==false) continue;
			if(iPhysicsBody::CollideFlagsMatch(mS.collideFlags, pBody->GetCollideFlags())==false) continue;

			if(cMath::CheckBVIntersection(boundingVolume,*pBody->GetBoundingVolume())==false)
			{
				continue;
			}

			collideData.SetMaxSize(32);
			bool bRet = mpWorld->CheckShapeCollision(pBody->GetShape(),pBody->GetLocalMatrix(),
													mpCurrentShape,mtxTransform,
													collideData, 32, true);

			if(bRet)
			{
				float fNumOfPoints = (float)collideData.mlNumOfPoints;

				float fMaxForce = mfMaxContactForcePerMassUnit * pBody->GetMass() / fNumOfPoints;

				float fC = mfConstantContactForceMul / fNumOfPoints * mS.mass;
				float fV = mfVelocityContactForceMul / fNumOfPoints * mS.mass;

				////////////////////////
				//Iterate contact points and add a spring force.
				for(int i=0; i< collideData.mlNumOfPoints; i++)
				{
					cCollidePoint &point = collideData.mvContactPoints[i];

					cVector3f vBodyVelocityAtPos = pBody->GetVelocityAtPosition(point.mvPoint);

					cVector3f vForce = point.mvNormal * point.mfDepth * fC -
											  point.mvNormal * cMath::Vector3Dot(vBodyVelocityAtPos,point.mvNormal)  * fV;

					float fLength = vForce.Length();
					if(fLength > fMaxForce)
					{
						vForce = (vForce / fLength) * fMaxForce;
					}

					pBody->AddForceAtPosition(vForce, point.mvPoint);
				}
			}
    	}
	}

	//-----------------------------------------------------------------------


	void iCharacterBody::UpdateCamera()
	{
		if(mpCamera==NULL) return;
		if(mpAux)
		{
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
			return;
		}
		if(!mbCameraUpdateActive)
		{
			mlstCameraPos.clear();
			return;
		}

		//////////////////
		//Get the camera pos add.
		cVector3f vAdd=0;
		vAdd.y += mvCameraPosAdd.y;
		vAdd += mS.right * mvCameraPosAdd.x;
		vAdd += mS.forward * mvCameraPosAdd.z;

		float fMainHeight = mvShapes[0]->GetSize().y;

		///////////////////////////////
		//Do NOT do any smoothing
		if(mlCameraSmoothPosNum <=0 || !mbCameraUseSmoothing)
		{
			cVector3f vPos = mS.position + cVector3f(0,fMainHeight - mS.size.y/2.0f,0);
			mpCamera->SetPosition(vPos + vAdd);
		}
		///////////////////////////////
		//Smooth the camera position
		else
		{
			//////////////////////////////
			//Add the newest position.
			cVector3f vHeadPos = mS.position + cVector3f(0,fMainHeight - mS.size.y/2.0f,0);
			mlstCameraPos.push_back(vHeadPos);

			//If to too large remove the oldest.
			if((int)mlstCameraPos.size() > mlCameraSmoothPosNum)
			{
				mlstCameraPos.erase(mlstCameraPos.begin());
			}

			float fPositionNum = (float)mlstCameraPos.size();

			/////////////////////////////////////////
			//Add all positions and devide by the number of em.
			//that way we get the average
			cVector3f vTotalPos(0,0,0);
			tVector3fListIt it=mlstCameraPos.begin();
			for(; it!= mlstCameraPos.end(); ++it)
			{
				vTotalPos += *it;
			}

			cVector3f vPos = vTotalPos / fPositionNum;

			//////////////////////////////////
			// Set up new position
			cVector3f vFirstSize = mvShapes[0]->GetSize();

			//cVector3f vHeadPos = (vPos - cVector3f(0,mS.size.y/2.0f,0)) + cVector3f(0,vFirstSize.y,0);

			mpCamera->SetPosition(vPos + vAdd);
		}

		//No need to smooth this yaw.
		mpCamera->SetYaw(mS.yaw);
	}

	//-----------------------------------------------------------------------


	void iCharacterBody::UpdateEntity()
	{
		if(mpEntity==NULL) return;
		if(mpAux)
		{
			OhplH14EntIn in = {
				mS.position, mS.mtxMove, m_mtxEntityOffset, m_mtxEntityPostOffset,
				mS.yaw, mlEntitySmoothPosNum, mlEntitySmoothYPosNum, mbEntitySmoothYPos
			};
			cMatrixf mtxEntity;
			ohpl_h14_update_entity(mpAux, &in, &mtxEntity);
			mpEntity->SetMatrix(mtxEntity);
			return;
		}

		///////////////////////////////
		//Y Position Smoothing
		float fSmoothY = mS.position.y;
		if(mbEntitySmoothYPos)
		{
			fSmoothY =0.0f;
			mlstEntityYPositions.push_back(mS.position.y);
			if((int)mlstEntityYPositions.size() > mlEntitySmoothYPosNum)
				mlstEntityYPositions.pop_front();

			//Iterate values and get average
			for(tFloatListIt it = mlstEntityYPositions.begin(); it != mlstEntityYPositions.end(); ++it)
			{
				fSmoothY += *it;
			}
			fSmoothY = fSmoothY / (float)mlstEntityYPositions.size();
		}

		///////////////////////////////
		//Do NOT do any smoothing
		if(mlEntitySmoothPosNum <=0)
		{
			cMatrixf mtxEntity = cMath::MatrixRotateY(mS.yaw);
			mtxEntity.SetTranslation(cVector3f(mS.position.x, fSmoothY, mS.position.z));

			mpEntity->SetMatrix(cMath::MatrixMul(mtxEntity,m_mtxEntityOffset));
		}
		//Smooth the entity position
		else
		{
			/////////////////////////////////
			//Add the newest position.
			mlstEntityPos.push_back(mS.position);

			//If to too large remove the oldest.
			if((int)mlstEntityPos.size() > mlEntitySmoothPosNum)
			{
				mlstEntityPos.erase(mlstEntityPos.begin());
			}

			float fPositionNum = (float)mlstEntityPos.size();

			//////////////////////////////////
			//Add all positions and devide by the number of em.
			//that way we get the average
			cVector3f vTotalPos(0,0,0);
			tVector3fListIt it=mlstEntityPos.begin();
			for(; it!= mlstEntityPos.end(); ++it)
			{
				vTotalPos += *it;
			}

			cVector3f vPos = vTotalPos / fPositionNum;
			if(mbEntitySmoothYPos) vPos.y = fSmoothY;

			//////////////////////////////////
			// Set up new matrix
			cMatrixf mtxEntity = mS.mtxMove;
			mtxEntity.SetTranslation(0);

			mtxEntity = cMath::MatrixMul(m_mtxEntityPostOffset,mtxEntity);
			mtxEntity.SetTranslation(mtxEntity.GetTranslation() + vPos);

			mtxEntity = cMath::MatrixMul(mtxEntity,m_mtxEntityOffset);

			mpEntity->SetMatrix(mtxEntity);
		}
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
		if(mpAux)
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
			return;
		}
		if(mpGravityAttachedBody==NULL)
		{
			////////////////////////
			//If the character was just attached, do somethings
			if(mpGravityLastAttachedBody != NULL)
			{
				mS.gravityAttachmentVelocity = mvGravityAttachmentBodyVelocity;
			}

			return;
		}

		mS.gravityAttachmentVelocity =0;

		//Log("Attched body: %s\n", mpGravityAttachedBody->GetName().c_str());

		/////////////////////////////
		// If the character was not just attached, update the position according to attachment body
		if(mpGravityLastAttachedBody == mpGravityAttachedBody)
		{
			///////////
			// Transform the current position according to entire matrix difference since previous matrix
			if(mpGravityAttachedBody->GetGravityAttachmentRotation())
			{
				//Transform the position
				cMatrixf mtxInvPrevWorld = cMath::MatrixInverse(m_mtxGravityAttachedPrevMatrix);
				cMatrixf mtxDelta = cMath::MatrixMul(mpGravityAttachedBody->GetWorldMatrix(),mtxInvPrevWorld);

				mS.position = cMath::MatrixMul(mtxDelta, mS.position);

				//Rotate the body (only along Y)
				if(mbRotateYawWhenGravityAttached)
				{
					cMatrixf mtxRotDelta = cMath::MatrixMul(mpGravityAttachedBody->GetWorldMatrix().GetRotation(),
																mtxInvPrevWorld.GetRotation());

					cVector3f vForward = cMath::MatrixMul(mtxRotDelta, cVector3f(0,0,-1));
					vForward.y =0; //only get x,z part.
					float fLength = vForward.Length();
					if(fLength > 0.0001f)
					{

						vForward = vForward / fLength;	//Normalize

						float fAngle = acos( -vForward.z );	//simply forward dot (0,0,-1)
						if(vForward > 0) fAngle = -fAngle;	//Determine direction.

						mS.yaw += fAngle;
						UpdateMoveMatrix();
					}
				}


			}
			///////////
			// Transform only using positon delta
			else
			{
				mS.position +=	mpGravityAttachedBody->GetWorldMatrix().GetTranslation() -
								m_mtxGravityAttachedPrevMatrix.GetTranslation();
			}


		}

		//mvGravityAttachmentBodyVelocity = mpGravityAttachedBody->GetLinearVelocity();
		if(mpGravityAttachedBody->GetGravityAttachmentVelocity())
		{
			switch(mpGravityAttachedBody->GetGravityAttachmentVelocityAxes())
			{
			case eVelocityAxes_XZ:
				mvGravityAttachmentBodyVelocity.x = mpGravityAttachedBody->GetVelocityAtPosition(GetFeetPosition()).x;
				mvGravityAttachmentBodyVelocity.y = 0;
				mvGravityAttachmentBodyVelocity.z = mpGravityAttachedBody->GetVelocityAtPosition(GetFeetPosition()).z;
				break;

			case eVelocityAxes_Y:
				mvGravityAttachmentBodyVelocity.x = 0;
				mvGravityAttachmentBodyVelocity.y = mpGravityAttachedBody->GetVelocityAtPosition(GetFeetPosition()).y;
				mvGravityAttachmentBodyVelocity.z = 0;
				break;

			case eVelocityAxes_XYZ:
				mvGravityAttachmentBodyVelocity = mpGravityAttachedBody->GetVelocityAtPosition(GetFeetPosition());
				break;
			}


		}
		else
			mvGravityAttachmentBodyVelocity = 0;

		m_mtxGravityAttachedPrevMatrix = mpGravityAttachedBody->GetWorldMatrix();
	}

	//-----------------------------------------------------------------------

	void iCharacterBody::EnableBodiesAroundCharacter()
	{
		if(mS.position == mS.lastPosition) return;

		//Make size a little larger so that bodies get enabled too.
		cBoundingVolume tempBv;
		tempBv.SetSize(mS.size * 1.1f);
		tempBv.SetPosition(mS.position);

        mpWorld->EnableBodiesInBV(&tempBv, true);
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
