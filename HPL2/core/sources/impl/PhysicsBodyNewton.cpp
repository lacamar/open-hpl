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

#include "impl/PhysicsBodyNewton.h"

#include "impl/CollideShapeNewton.h"
#include "impl/PhysicsWorldNewton.h"
#include "impl/PhysicsMaterialNewton.h"
#include "system/LowLevelSystem.h"
#include "graphics/LowLevelGraphics.h"
#include "math/Math.h"
#include "scene/Node3D.h"
#include <algorithm>
#include <vector>
#include "impl/RustNewton.h"

namespace hpl {

	bool cPhysicsBodyNewton::mbUseCallback = true;

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cPhysicsBodyNewton::cPhysicsBodyNewton(const tString &asName,iPhysicsWorld *apWorld,iCollideShape *apShape) 
	: iPhysicsBody(asName,apWorld, apShape)
	{
		cPhysicsWorldNewton *pWorldNewton = static_cast<cPhysicsWorldNewton*>(apWorld);
		cCollideShapeNewton *pShapeNewton = static_cast<cCollideShapeNewton*>(apShape);
		
		mpNewtonWorld = pWorldNewton->GetNewtonWorld();
		cMatrixf mtxIdentity = cMatrixf::Identity;
		mpNewtonBody = NewtonCreateBody(pWorldNewton->GetNewtonWorld(), 
										pShapeNewton->GetNewtonCollision(), &mtxIdentity.m[0][0]);

		mpCallback = hplNew( cPhysicsBodyNewtonCallback, () );

		AddCallback(mpCallback);

		// Setup the callbacks and set this body as user data
		// This is so that the transform gets updated and
		// to add gravity, forces and user sink.
		NewtonBodySetForceAndTorqueCallback(mpNewtonBody,OnUpdateCallback);
		NewtonBodySetTransformCallback(mpNewtonBody, OnTransformCallback);
		NewtonBodySetUserData(mpNewtonBody, this);

		//Set default property settings
		mbGravity = true;
		
		mfMaxLinearSpeed =0;
		mfMaxAngularSpeed =0;
		mfMass =0;

		mfAutoDisableLinearThreshold = 0.01f;
		mfAutoDisableAngularThreshold = 0.01f;
		mlAutoDisableNumSteps = 10;

		//Clear the force accumulators
		mvTotalForce = cVector3f(0,0,0);
		mvTotalTorque = cVector3f(0,0,0);
		
		//Log("Creating newton body '%s' %d\n",msName.c_str(),(size_t)this);
	}

	//-----------------------------------------------------------------------

	cPhysicsBodyNewton::~cPhysicsBodyNewton()
	{
		if(mbForced)
		{
			std::vector<cPhysicsBodyNewton*> &vForced = static_cast<cPhysicsWorldNewton*>(mpWorld)->mvForcedBodies;
			vForced.erase(std::remove(vForced.begin(), vForced.end(), this), vForced.end());
		}
	}

	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::DeleteLowLevel()
	{
		//Log(" Newton body %d\n", (size_t)mpNewtonBody);
		NewtonDestroyBody(mpNewtonWorld,mpNewtonBody);
		//Log(" Callback\n");
		hplDelete(mpCallback);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CALLBACK METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

    void cPhysicsBodyNewtonCallback::OnTransformUpdate(iEntity3D * apEntity)
	{
		if(cPhysicsBodyNewton::mbUseCallback==false) return;

		cPhysicsBodyNewton *pRigidBody = static_cast<cPhysicsBodyNewton*>(apEntity);
		cMatrixf mtxTranspose = apEntity->GetLocalMatrix().GetTranspose();
		NewtonBodySetMatrix(pRigidBody->mpNewtonBody, &mtxTranspose.m[0][0]);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------
	
	void cPhysicsBodyNewton::SetMaterial(iPhysicsMaterial* apMaterial)
	{
		mpMaterial = apMaterial;

		if(apMaterial == NULL) return;

		cPhysicsMaterialNewton* pNewtonMat = static_cast<cPhysicsMaterialNewton*>(mpMaterial);

		NewtonBodySetMaterialGroupID(mpNewtonBody, pNewtonMat->GetId());
	}

	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetLinearVelocity(const cVector3f &avVel)
	{
		NewtonBodySetVelocity(mpNewtonBody, avVel.v);
	}
	cVector3f cPhysicsBodyNewton::GetLinearVelocity() const
	{
		cVector3f vVel;
		NewtonBodyGetVelocity(mpNewtonBody, vVel.v);
		return vVel;
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetAngularVelocity(const cVector3f &avVel)
	{
		NewtonBodySetOmega(mpNewtonBody, avVel.v);
	}
	cVector3f cPhysicsBodyNewton::GetAngularVelocity() const
	{
		cVector3f vVel;
		NewtonBodyGetOmega(mpNewtonBody, vVel.v);
		return vVel;
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetLinearDamping(float afDamping)
	{
		if(afDamping < 0.001f)afDamping = 0.001f;

		NewtonBodySetLinearDamping(mpNewtonBody,afDamping);
	}
	float cPhysicsBodyNewton::GetLinearDamping() const
	{
		return NewtonBodyGetLinearDamping(mpNewtonBody);
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetAngularDamping(float afDamping)
	{
		if(afDamping < 0.001f)afDamping = 0.001f;

		float fDamp[3] = {afDamping,afDamping,afDamping};
		NewtonBodySetAngularDamping(mpNewtonBody,fDamp);
	}
	float cPhysicsBodyNewton::GetAngularDamping() const
	{
		float fDamp[3];
		NewtonBodyGetAngularDamping(mpNewtonBody,fDamp);
		return fDamp[0];
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetMaxLinearSpeed(float afSpeed)
	{
		mfMaxLinearSpeed = afSpeed;
	}
	float cPhysicsBodyNewton::GetMaxLinearSpeed() const
	{
		return mfMaxLinearSpeed;
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetMaxAngularSpeed(float afDamping)
	{
		mfMaxAngularSpeed = afDamping;
	}
	float cPhysicsBodyNewton::GetMaxAngularSpeed() const
	{
		return mfMaxAngularSpeed;
	}

	//-----------------------------------------------------------------------

	cVector3f cPhysicsBodyNewton::GetInertiaVector()
	{
		float fIxx, fIyy, fIzz, fMass;

		NewtonBodyGetMassMatrix(mpNewtonBody,&fMass, &fIxx, &fIyy, &fIzz);
		
		return cVector3f(fIxx, fIyy, fIzz);
	}

	//-----------------------------------------------------------------------

	cMatrixf cPhysicsBodyNewton::GetInertiaMatrix()
	{
		float fIxx, fIyy, fIzz, fMass;

		NewtonBodyGetMassMatrix(mpNewtonBody,&fMass, &fIxx, &fIyy, &fIzz);

        cMatrixf mtxRot = GetLocalMatrix().GetRotation();
		cMatrixf mtxTransRot = mtxRot.GetTranspose();
		cMatrixf mtxI(	fIxx,0,	  0,	0,
						0,	 fIyy,0,	0,
						0,	 0,	  fIzz,	0,
						0,	 0,	  0,	1);

		return cMath::MatrixMul(cMath::MatrixMul(mtxRot,mtxI), mtxTransRot);
	}

	//-----------------------------------------------------------------------

	void  cPhysicsBodyNewton::SetMass(float afMass)
	{
		cCollideShapeNewton *pShapeNewton = static_cast<cCollideShapeNewton*>(mpShape);
		
		cVector3f vInertia;// = pShapeNewton->GetInertia(afMass);
		cVector3f vOffset;

		NewtonConvexCollisionCalculateInertialMatrix(pShapeNewton->GetNewtonCollision(),
													vInertia.v, vOffset.v);
		vInertia = vInertia * afMass;

		NewtonBodySetCentreOfMass(mpNewtonBody,vOffset.v);

		NewtonBodySetMassMatrix(mpNewtonBody, afMass, vInertia.x, vInertia.y, vInertia.z);
		mfMass = afMass;
	}
	float cPhysicsBodyNewton::GetMass() const
	{
		return mfMass;
	}

	void  cPhysicsBodyNewton::SetMassCentre(const cVector3f& avCentre)
	{
		NewtonBodySetCentreOfMass(mpNewtonBody,avCentre.v);
	}

	cVector3f cPhysicsBodyNewton::GetMassCentre() const
	{
		cVector3f vCentre;
		NewtonBodyGetCentreOfMass(mpNewtonBody,vCentre.v);
		return vCentre;
	}
	
	//-----------------------------------------------------------------------
	
	void cPhysicsBodyNewton::MarkForced()
	{
		if(mbForced) return;
		mbForced = true;
		static_cast<cPhysicsWorldNewton*>(mpWorld)->mvForcedBodies.push_back(this);
	}

	void cPhysicsBodyNewton::AddForce(const cVector3f &avForce)
	{
		MarkForced();
		mvTotalForce += avForce;
		Enable();

		//Log("Added force %s\n",avForce.ToString().c_str());
	}

	void cPhysicsBodyNewton::AddForceAtPosition(const cVector3f &avForce, const cVector3f &avPos)
	{
		MarkForced();
		mvTotalForce += avForce;

		cVector3f vLocalPos = avPos - GetLocalPosition();
		cVector3f vMassCentre = GetMassCentre();
		if(vMassCentre != cVector3f(0,0,0))
		{
			vMassCentre = cMath::MatrixMul(GetLocalMatrix().GetRotation(),vMassCentre);
			vLocalPos -= vMassCentre;
		}

		cVector3f vTorque = cMath::Vector3Cross(vLocalPos, avForce);

		mvTotalTorque += vTorque;
		Enable();

		//Log("Added force %s\n",avForce.ToString().c_str());
	}

	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::AddTorque(const cVector3f &avTorque)
	{
		MarkForced();
		mvTotalTorque += avTorque;
		Enable();
	}

	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::AddImpulse(const cVector3f &avImpulse)
	{
		cVector3f vMassCentre = GetMassCentre();
		if(vMassCentre != cVector3f(0,0,0))
		{
			cVector3f vCentreOffset = cMath::MatrixMul( GetWorldMatrix().GetRotation(),
														vMassCentre);

			cVector3f vWorldPosition = GetWorldPosition() + vCentreOffset;
			NewtonBodyAddImpulse(mpNewtonBody, avImpulse.v, vWorldPosition.v);
		}
		else
		{
			NewtonBodyAddImpulse(mpNewtonBody, avImpulse.v, GetWorldPosition().v);
		}
	}
	void cPhysicsBodyNewton::AddImpulseAtPosition(const cVector3f &avImpulse, const cVector3f &avPos)
	{
		NewtonBodyAddImpulse(mpNewtonBody, avImpulse.v, avPos.v);
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::Sleep()
	{
		NewtonBodySetFreezeState(mpNewtonBody, 1);
	}

	void cPhysicsBodyNewton::Enable()
	{
		mlFreezeCount = 0;
		NewtonBodySetFreezeState(mpNewtonBody, 0);
	}
	bool cPhysicsBodyNewton::GetEnabled() const
	{
		return NewtonBodyGetSleepState(mpNewtonBody) ==0?true: false;
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetAutoDisable(bool abEnabled)
	{
		NewtonBodySetAutoSleep(mpNewtonBody, abEnabled ? 1 : 0);
	}
	bool cPhysicsBodyNewton::GetAutoDisable() const
	{
		return NewtonBodyGetAutoSleep(mpNewtonBody) == 0 ? false : true;
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetAutoDisableLinearThreshold(float afThresold)
	{
		mfAutoDisableLinearThreshold = afThresold;
		/*NewtonBodySetFreezeTreshold(mpNewtonBody, mfAutoDisableLinearThreshold,
			mfAutoDisableAngularThreshold, mlAutoDisableNumSteps);*/
	}
	float cPhysicsBodyNewton::GetAutoDisableLinearThreshold() const
	{
		return mfAutoDisableLinearThreshold;
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetAutoDisableAngularThreshold(float afThresold)
	{
		mfAutoDisableAngularThreshold = afThresold;
		/*NewtonBodySetFreezeTreshold(mpNewtonBody, mfAutoDisableLinearThreshold,
			mfAutoDisableAngularThreshold, mlAutoDisableNumSteps);*/
	}
	float cPhysicsBodyNewton::GetAutoDisableAngularThreshold() const
	{
		return mfAutoDisableAngularThreshold;
	}

	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetAutoDisableNumSteps(int anNum)
	{
		mlAutoDisableNumSteps = anNum;
		/*NewtonBodySetFreezeTreshold(mpNewtonBody, mfAutoDisableLinearThreshold,
			mfAutoDisableAngularThreshold, mlAutoDisableNumSteps);*/
	}

	int cPhysicsBodyNewton::GetAutoDisableNumSteps() const
	{
		return mlAutoDisableNumSteps;
	}

	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetContinuousCollision(bool abOn)
	{
		NewtonBodySetContinuousCollisionMode(mpNewtonBody,abOn ? 1 : 0);
	}

	bool cPhysicsBodyNewton::GetContinuousCollision()
	{
		return NewtonBodyGetContinuousCollisionMode(mpNewtonBody)==1 ? true : false;
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetGravity(bool abEnabled)
	{
		mbGravity = abEnabled;	
	}
	bool cPhysicsBodyNewton::GetGravity() const
	{
		return mbGravity;
	}

	//-----------------------------------------------------------------------


	////////////////////////////////////////////

	void cPhysicsBodyNewton::RenderDebugGeometry(iLowLevelGraphics *apLowLevel,const cColor &aColor)
	{
		mpWorld->RenderShapeDebugGeometry(mpShape,GetLocalMatrix(),apLowLevel, aColor);
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::ClearForces()
	{
		mvTotalForce = cVector3f(0,0,0);
		mvTotalTorque = cVector3f(0,0,0);
		mbForced = false;
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// STATIC NEWTON CALLBACKS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	
	void cPhysicsBodyNewton::OnTransformCallback(const NewtonBody* apBody, const dFloat* apMatrix, int alThreadIndex)
	{
		cPhysicsBodyNewton* pRigidBody = (cPhysicsBodyNewton*) NewtonBodyGetUserData(apBody);

		pRigidBody->m_mtxLocalTransform.FromTranspose(apMatrix);

		mbUseCallback = false;
		pRigidBody->SetTransformUpdated(true);
		mbUseCallback = true;
	}

	//-----------------------------------------------------------------------
	
	//callback for buoyancy
	static cPlanef gSurfacePlane;
	static int BuoyancyPlaneCallback (const int alCollisionID, void *apContext, 
									const float* afGlobalSpaceMatrix, float* afGlobalSpacePlane)
	{
		afGlobalSpacePlane[0] = gSurfacePlane.a;
		afGlobalSpacePlane[1] = gSurfacePlane.b;
		afGlobalSpacePlane[2] = gSurfacePlane.c;
		afGlobalSpacePlane[3] = gSurfacePlane.d;
		return 1;   
	} 

	//-----------------------------------------------------------------------

	// HPL3 cCollideShapeNewton::CalculateBouyancyForces: lift from spheres packed in the shape, then drag
	bool cPhysicsBodyNewton::CalcHpl3Buoyancy(const cVector3f &avGravity, float afTimeStep)
	{
		cCollideShapeNewton *pShape = static_cast<cCollideShapeNewton*>(mpShape);
		cPhysicsBody_Buoyancy &b = mBuoyancy;
		if (mfMass == 0 || b.mfDensity * mfBuoyancyDensityMul <= 0)
			return false;

		const cMatrixf &mtx = GetLocalMatrix();
		int lSubNum = pShape->GetSubShapeNum();
		OhplNewtonSubShape stackSubs[8];
		std::vector<OhplNewtonSubShape> heapSubs;
		OhplNewtonSubShape *pSubs = stackSubs;
		if (lSubNum > 8)
		{
			heapSubs.resize(lSubNum);
			pSubs = heapSubs.data();
		}
		for (int i = 0; i < lSubNum; ++i)
		{
			cCollideShapeNewton *pSub = static_cast<cCollideShapeNewton*>(pShape->GetSubShape(i));
			float fR;
			const tVector3fVec &vPoints = pSub->GetBuoyancyPoints(fR);
			pSubs[i] = OhplNewtonSubShape{vPoints.empty() ? nullptr : &vPoints[0], vPoints.size(), fR, pSub->GetVolume()};
		}

		OhplNewtonBuoyIn in;
		in.mass = mfMass;
		in.density = b.mfDensity;
		in.densityMul = mfBuoyancyDensityMul;
		in.dragDensityMul = b.mfDragDensityMul;
		in.linearViscosity = b.mfLinearViscosity;
		in.angularViscosity = b.mfAngularViscosity;
		in.maxLinearSpeed = mfMaxLinearSpeed;
		in.maxAngularSpeed = mfMaxAngularSpeed;
		in.shapeVolume = pShape->GetVolume();
		in.timeStep = afTimeStep;
		in.noPrevVel = b.mbNoPrevVel;
		in.surface = b.mSurface;
		in.matrix = mtx;
		in.inverse = cMath::MatrixInverse(mtx);
		in.inertia = GetInertiaMatrix();
		in.massCentre = GetMassCentre();
		in.gravity = avGravity;
		in.linearVelocity = GetLinearVelocity();
		in.angularVelocity = GetAngularVelocity();
		in.prevLinearVel = b.mvPrevLinearVel;
		in.prevAngularVel = b.mvPrevAngularVel;

		OhplNewtonBuoyOut out;
		if (ohpl_newton_buoyancy(&in, pSubs, lSubNum, &out) == false)
			return false;

		if (out.dragUpdated)
		{
			b.mvPrevLinearVel = out.prevLinearVel;
			b.mvPrevAngularVel = out.prevAngularVel;
			b.mbNoPrevVel = out.noPrevVel;
			b.mfDragDensityMul = out.dragDensityMul;
		}
		mvBuoyancyForce = out.force;
		mvBuoyancyTorque = out.torque;
		return true;
	}

	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::OnUpdateCallback(NewtonBody* apBody, dFloat afTimestep, int alThreadIndex)
	{
		
		cPhysicsBodyNewton* pRigidBody = (cPhysicsBodyNewton*) NewtonBodyGetUserData(apBody);

		if(pRigidBody->IsActive()==false)
		{
			return;
		}
		
		//////////////////////////
		//Check if active
		if(pRigidBody->GetEnabled())
		{
			//If not in update list, add body.
			if(pRigidBody->IsInUpdateList()==false)
			{
				pRigidBody->GetWorld()->AddBodyToUpdateList(pRigidBody);	
			}
			if (pRigidBody->mlFreezeCount > 0)
				--pRigidBody->mlFreezeCount;
		}
		
		////////////////////////////
		//Create some gravity
		if (pRigidBody->mbGravity && pRigidBody->mlFreezeCount <= 0)
		{
			cVector3f vGravity = pRigidBody->mpWorld->GetGravity();

			float fForce[3] = { pRigidBody->mfMass * vGravity.x, pRigidBody->mfMass * vGravity.y, pRigidBody->mfMass * vGravity.z};
			
			NewtonBodyAddForce(apBody, &fForce[0]);
		}

		////////////////////////////
		// Create Buoyancy
		if (pRigidBody->mBuoyancy.mbActive && pRigidBody->mfBuoyancyDensityMul>0 && pRigidBody->mlFreezeCount <= 0 && mbHpl3Buoyancy)
		{
			if (pRigidBody->GetEnabled() == false || pRigidBody->CalcHpl3Buoyancy(pRigidBody->mpWorld->GetGravity(), afTimestep))
			{
				NewtonBodyAddForce(apBody, pRigidBody->mvBuoyancyForce.v);
				NewtonBodyAddTorque(apBody, pRigidBody->mvBuoyancyTorque.v);
			}
		}
		else if (pRigidBody->mBuoyancy.mbActive && pRigidBody->mfBuoyancyDensityMul>0 && pRigidBody->mlFreezeCount <= 0)
		{
			cVector3f vGravity = pRigidBody->mpWorld->GetGravity();

			gSurfacePlane = pRigidBody->mBuoyancy.mSurface;
			
			NewtonBodyAddBuoyancyForce( apBody, 
										pRigidBody->mBuoyancy.mfDensity * pRigidBody->mfBuoyancyDensityMul,
										pRigidBody->mBuoyancy.mfLinearViscosity,
										pRigidBody->mBuoyancy.mfAngularViscosity,
										vGravity.v, BuoyancyPlaneCallback,
										pRigidBody);
		}

		////////////////////////////
		// Add forces from calls to Addforce(..), etc
		NewtonBodyAddForce(apBody, pRigidBody->mvTotalForce.v);
		NewtonBodyAddTorque(apBody, pRigidBody->mvTotalTorque.v);

		////////////////////////////
		// Check so that all speeds are within thresholds
		// Linear
		if (pRigidBody->mfMaxLinearSpeed > 0)
		{
			cVector3f vVel = pRigidBody->GetLinearVelocity(), vClamped;
			if (ohpl_newton_clamp_speed(&vVel, pRigidBody->mfMaxLinearSpeed, &vClamped))
				pRigidBody->SetLinearVelocity(vClamped);
		}
		// Angular
		if (pRigidBody->mfMaxAngularSpeed > 0)
		{
			cVector3f vVel = pRigidBody->GetAngularVelocity(), vClamped;
			if (ohpl_newton_clamp_speed(&vVel, pRigidBody->mfMaxAngularSpeed, &vClamped))
				pRigidBody->SetAngularVelocity(vClamped);
		}

		//cVector3f vForce;
		//NewtonBodyGetForce(apBody,vForce.v);
		//Log("Engine force %s\n",pRigidBody->mvTotalForce.ToString().c_str());
		//Log("Engine force %s, body force: %s \n",pRigidBody->mvTotalForce.ToString().c_str(),
		//										vForce.ToString().c_str());
	}

	//-----------------------------------------------------------------------

}
