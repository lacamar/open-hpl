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
		// Newton 3.14 split NewtonCreateBody into Dynamic/Kinematic/AsymetricDynamic
		// variants and requires an initial transform up front (the old API defaulted
		// to identity and let the caller set the real transform afterward via the
		// normal engine transform-sync path, same as happens here).
		cMatrixf mtxIdentityTranspose = cMatrixf::Identity.GetTranspose();
		mpNewtonBody = NewtonCreateDynamicBody(pWorldNewton->GetNewtonWorld(),
										pShapeNewton->GetNewtonCollision(),
										&mtxIdentityTranspose.m[0][0]);

		mpCallback = hplNew( cPhysicsBodyNewtonCallback, () );

		AddCallback(mpCallback);

		// Setup the callbacks and set this body as user data
		// This is so that the transform gets updated and
		// to add gravity, forces and user sink.
		NewtonBodySetForceAndTorqueCallback(mpNewtonBody,OnUpdateCallback);
		NewtonBodySetTransformCallback(mpNewtonBody, OnTransformCallback);
		NewtonBodySetUserData(mpNewtonBody, this);

		// Newton 3.14 moved continuous collision from a per-material-pair setting to
		// a per-body one (see the comment in cPhysicsMaterialNewton::UpdateMaterials).
		// The old code enabled it unconditionally for every material pair, so
		// enabling it unconditionally on every body reproduces the same effect.
		NewtonBodySetContinuousCollisionMode(mpNewtonBody, 1);

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
		//Log(" Destroying newton body '%s' %d\n",msName.c_str(),(size_t)this);
	}

	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::DeleteLowLevel()
	{
		//Log(" Newton body %d\n", (size_t)mpNewtonBody);
		NewtonDestroyBody(mpNewtonBody);
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

		NewtonBodyGetMass(mpNewtonBody,&fMass, &fIxx, &fIyy, &fIzz);
		
		return cVector3f(fIxx, fIyy, fIzz);
	}

	//-----------------------------------------------------------------------

	cMatrixf cPhysicsBodyNewton::GetInertiaMatrix()
	{
		float fIxx, fIyy, fIzz, fMass;

		NewtonBodyGetMass(mpNewtonBody,&fMass, &fIxx, &fIyy, &fIzz);

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
	
	void cPhysicsBodyNewton::AddForce(const cVector3f &avForce)
	{
		mvTotalForce += avForce;
		Enable();

		//Log("Added force %s\n",avForce.ToString().c_str());
	}

	void cPhysicsBodyNewton::AddForceAtPosition(const cVector3f &avForce, const cVector3f &avPos)
	{
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
		mvTotalTorque += avTorque;
		Enable();
	}

	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::AddImpulse(const cVector3f &avImpulse)
	{
		// Newton 3.14's NewtonBodyAddImpulse takes an explicit timestep (it converts
		// the desired delta-velocity into a force applied over that timestep) - the
		// old 2.x API applied it over an implicit internal step. The world's fixed
		// simulation timestep is the correct value to pass here.
		float fTimeStep = mpWorld->GetMaxTimeStep();

		cVector3f vMassCentre = GetMassCentre();
		if(vMassCentre != cVector3f(0,0,0))
		{
			cVector3f vCentreOffset = cMath::MatrixMul( GetWorldMatrix().GetRotation(),
														vMassCentre);

			cVector3f vWorldPosition = GetWorldPosition() + vCentreOffset;
			NewtonBodyAddImpulse(mpNewtonBody, avImpulse.v, vWorldPosition.v, fTimeStep);
		}
		else
		{
			NewtonBodyAddImpulse(mpNewtonBody, avImpulse.v, GetWorldPosition().v, fTimeStep);
		}
	}
	void cPhysicsBodyNewton::AddImpulseAtPosition(const cVector3f &avImpulse, const cVector3f &avPos)
	{
		NewtonBodyAddImpulse(mpNewtonBody, avImpulse.v, avPos.v, mpWorld->GetMaxTimeStep());
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::Enable()
	{
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
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::OnUpdateCallback(const NewtonBody* apBody, dFloat afTimestep, int alThreadIndex)
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
		}
		
		////////////////////////////
		//Create some gravity
		if (pRigidBody->mbGravity)
		{
			cVector3f vGravity = pRigidBody->mpWorld->GetGravity();

			float fForce[3] = { pRigidBody->mfMass * vGravity.x, pRigidBody->mfMass * vGravity.y, pRigidBody->mfMass * vGravity.z};
			
			NewtonBodyAddForce(apBody, &fForce[0]);
		}

		////////////////////////////
		// Create Buoyancy
		//
		// Newton 3.14 removed the NewtonBodyAddBuoyancyForce convenience wrapper
		// entirely (only the lower-level NewtonConvexCollisionCalculateBuoyancyVolume
		// primitive - submerged volume + center of buoyancy for a convex shape against
		// a fluid plane - remains). This reimplements the same physical effect
		// (Archimedes' force applied at the center of buoyancy, plus linear/angular
		// drag scaled by how submerged the shape currently is) on top of that
		// primitive. It is a faithful reimplementation of the standard buoyancy
		// algorithm, not a byte-for-byte port of Newton 2.x's internal formula (which
		// is not available to compare against) - the mfDensity/mfLinearViscosity/
		// mfAngularViscosity values set on liquid areas may need re-tuning against
		// real game content.
		if (pRigidBody->mBuoyancy.mbActive && pRigidBody->mfBuoyancyDensityMul>0)
		{
			cCollideShapeNewton *pShapeNewton = static_cast<cCollideShapeNewton*>(pRigidBody->mpShape);
			const cPlanef &surface = pRigidBody->mBuoyancy.mSurface;
			dFloat fluidPlane[4] = { surface.a, surface.b, surface.c, surface.d };

			dFloat afBodyMatrix[16];
			NewtonBodyGetMatrix(apBody, afBodyMatrix);

			cVector3f vCenterOfBuoyancy;
			dFloat fSubmergedVolume = NewtonConvexCollisionCalculateBuoyancyVolume(
											pShapeNewton->GetNewtonCollision(), afBodyMatrix,
											fluidPlane, vCenterOfBuoyancy.v);

			if(fSubmergedVolume > 0.0f)
			{
				cVector3f vGravity = pRigidBody->mpWorld->GetGravity();
				float fDensity = pRigidBody->mBuoyancy.mfDensity * pRigidBody->mfBuoyancyDensityMul;
				float fTotalVolume = pShapeNewton->GetVolume();
				float fSubmergedFraction = fTotalVolume > 0.0f ?
					cMath::Min(fSubmergedVolume / fTotalVolume, 1.0f) : 0.0f;

				//Archimedes' force: opposes gravity, magnitude = weight of displaced fluid.
				cVector3f vBuoyancyForce = vGravity * (-fDensity * fSubmergedVolume);
				NewtonBodyAddForce(apBody, vBuoyancyForce.v);

				//Apply at the center of buoyancy rather than the body origin, by
				//converting the offset into an equivalent torque (same technique as
				//AddForceAtPosition above).
				cVector3f vCentreOffset = vCenterOfBuoyancy - pRigidBody->GetWorldPosition();
				cVector3f vBuoyancyTorque = cMath::Vector3Cross(vCentreOffset, vBuoyancyForce);
				NewtonBodyAddTorque(apBody, vBuoyancyTorque.v);

				//Fluid drag, scaled by mass and by how submerged the body currently is.
				cVector3f vLinearDrag = pRigidBody->GetLinearVelocity() *
					(-pRigidBody->mBuoyancy.mfLinearViscosity * fSubmergedFraction * pRigidBody->mfMass);
				cVector3f vAngularDrag = pRigidBody->GetAngularVelocity() *
					(-pRigidBody->mBuoyancy.mfAngularViscosity * fSubmergedFraction * pRigidBody->mfMass);
				NewtonBodyAddForce(apBody, vLinearDrag.v);
				NewtonBodyAddTorque(apBody, vAngularDrag.v);
			}
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
			cVector3f vVel = pRigidBody->GetLinearVelocity();
			float fSqrSpeed = vVel.Length();
			if (fSqrSpeed > pRigidBody->mfMaxLinearSpeed * pRigidBody->mfMaxLinearSpeed)
			{
				vVel = (vVel / sqrtf(fSqrSpeed)) * pRigidBody->mfMaxLinearSpeed;
				pRigidBody->SetLinearVelocity(vVel);
			}
		}
		// Angular
		if (pRigidBody->mfMaxAngularSpeed > 0)
		{
			cVector3f vVel = pRigidBody->GetAngularVelocity();
			float fSqrSpeed = vVel.Length();
			if (fSqrSpeed > pRigidBody->mfMaxAngularSpeed * pRigidBody->mfMaxAngularSpeed)
			{
				vVel = (vVel / sqrtf(fSqrSpeed)) * pRigidBody->mfMaxAngularSpeed;
				pRigidBody->SetAngularVelocity(vVel);
			}
		}

		//cVector3f vForce;
		//NewtonBodyGetForce(apBody,vForce.v);
		//Log("Engine force %s\n",pRigidBody->mvTotalForce.ToString().c_str());
		//Log("Engine force %s, body force: %s \n",pRigidBody->mvTotalForce.ToString().c_str(),
		//										vForce.ToString().c_str());
	}

	//-----------------------------------------------------------------------

}
