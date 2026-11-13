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
#include "impl/RustNewtonBackend.h"

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
		mhRust = ohpl_ntb_create(pWorldNewton->RustHandle(), pShapeNewton->GetNewtonCollision(), (size_t)this, &mpNewtonBody);

		mpCallback = hplNew( cPhysicsBodyNewtonCallback, () );

		AddCallback(mpCallback);

		//Set default property settings
		mbGravity = true;
		
		mfMaxLinearSpeed =0;
		mfMaxAngularSpeed =0;
		mfMass =0;

		mfAutoDisableLinearThreshold = 0.01f;
		mfAutoDisableAngularThreshold = 0.01f;
		mlAutoDisableNumSteps = 10;

		//Log("Creating newton body '%s' %d\n",msName.c_str(),(size_t)this);
	}

	//-----------------------------------------------------------------------

	cPhysicsBodyNewton::~cPhysicsBodyNewton()
	{
	}

	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::DeleteLowLevel()
	{
		ohpl_ntb_destroy(mhRust);
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
		cPhysicsBodyNewton *pBody = static_cast<cPhysicsBodyNewton*>(apEntity);
		ohpl_ntb_set_matrix(pBody->mhRust, &apEntity->GetLocalMatrix());
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
		ohpl_ntb_set_material(mhRust, static_cast<cPhysicsMaterialNewton*>(mpMaterial)->GetId());
	}

	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetLinearVelocity(const cVector3f &avVel)
	{
		ohpl_ntb_set_velocity(mhRust, &avVel);
	}
	cVector3f cPhysicsBodyNewton::GetLinearVelocity() const
	{
		cVector3f vVel;
		ohpl_ntb_velocity(mhRust, &vVel);
		return vVel;
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetAngularVelocity(const cVector3f &avVel)
	{
		ohpl_ntb_set_omega(mhRust, &avVel);
	}
	cVector3f cPhysicsBodyNewton::GetAngularVelocity() const
	{
		cVector3f vVel;
		ohpl_ntb_omega(mhRust, &vVel);
		return vVel;
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetLinearDamping(float afDamping)
	{
		ohpl_ntb_set_linear_damping(mhRust, afDamping);
	}
	float cPhysicsBodyNewton::GetLinearDamping() const
	{
		return ohpl_ntb_linear_damping(mhRust);
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetAngularDamping(float afDamping)
	{
		ohpl_ntb_set_angular_damping(mhRust, afDamping);
	}
	float cPhysicsBodyNewton::GetAngularDamping() const
	{
		return ohpl_ntb_angular_damping(mhRust);
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetMaxLinearSpeed(float afSpeed)
	{
		mfMaxLinearSpeed = afSpeed;
		ohpl_ntb_set_max_linear(mhRust, afSpeed);
	}
	float cPhysicsBodyNewton::GetMaxLinearSpeed() const
	{
		return ohpl_ntb_max_linear(mhRust);
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetMaxAngularSpeed(float afDamping)
	{
		mfMaxAngularSpeed = afDamping;
		ohpl_ntb_set_max_angular(mhRust, afDamping);
	}
	float cPhysicsBodyNewton::GetMaxAngularSpeed() const
	{
		return ohpl_ntb_max_angular(mhRust);
	}

	//-----------------------------------------------------------------------

	cVector3f cPhysicsBodyNewton::GetInertiaVector()
	{
		cVector3f vInertia;
		ohpl_ntb_inertia(mhRust, &vInertia);
		return vInertia;
	}

	//-----------------------------------------------------------------------

	cMatrixf cPhysicsBodyNewton::GetInertiaMatrix()
	{
		cVector3f vI = GetInertiaVector();
		cMatrixf mtxRot = GetLocalMatrix().GetRotation();
		cMatrixf mtxTransRot = mtxRot.GetTranspose();
		cMatrixf mtxI(	vI.x,0,	  0,	0,
						0,	 vI.y,0,	0,
						0,	 0,	  vI.z,	0,
						0,	 0,	  0,	1);
		return cMath::MatrixMul(cMath::MatrixMul(mtxRot,mtxI), mtxTransRot);
	}

	//-----------------------------------------------------------------------

	void  cPhysicsBodyNewton::SetMass(float afMass)
	{
		ohpl_ntb_set_mass(mhRust, static_cast<cCollideShapeNewton*>(mpShape)->GetNewtonCollision(), afMass);
		mfMass = afMass;
	}
	float cPhysicsBodyNewton::GetMass() const
	{
		return ohpl_ntb_mass(mhRust);
	}

	void  cPhysicsBodyNewton::SetMassCentre(const cVector3f& avCentre)
	{
		ohpl_ntb_set_mass_centre(mhRust, &avCentre);
	}

	cVector3f cPhysicsBodyNewton::GetMassCentre() const
	{
		cVector3f vCentre;
		ohpl_ntb_mass_centre(mhRust, &vCentre);
		return vCentre;
	}
	
	//-----------------------------------------------------------------------
	
	void cPhysicsBodyNewton::AddForce(const cVector3f &avForce)
	{
		ohpl_ntb_add_force(mhRust, &avForce);
		Enable();
	}

	void cPhysicsBodyNewton::AddForceAtPosition(const cVector3f &avForce, const cVector3f &avPos)
	{
		ohpl_ntb_add_force_at_position(mhRust, &avForce, &avPos, &GetLocalMatrix());
		Enable();
	}

	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::AddTorque(const cVector3f &avTorque)
	{
		ohpl_ntb_add_torque(mhRust, &avTorque);
		Enable();
	}

	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::AddImpulse(const cVector3f &avImpulse)
	{
		ohpl_ntb_add_impulse(mhRust, &avImpulse, &GetWorldMatrix());
	}
	void cPhysicsBodyNewton::AddImpulseAtPosition(const cVector3f &avImpulse, const cVector3f &avPos)
	{
		ohpl_ntb_add_impulse_at(mhRust, &avImpulse, &avPos);
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::Sleep()
	{
		ohpl_ntb_set_freeze(mhRust, 1);
	}

	void cPhysicsBodyNewton::Enable()
	{
		mlFreezeCount = 0;
		ohpl_ntb_set_freeze(mhRust, 0);
	}
	bool cPhysicsBodyNewton::GetEnabled() const
	{
		return ohpl_ntb_enabled(mhRust);
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetAutoDisable(bool abEnabled)
	{
		ohpl_ntb_set_auto_sleep(mhRust, abEnabled);
	}
	bool cPhysicsBodyNewton::GetAutoDisable() const
	{
		return ohpl_ntb_auto_sleep(mhRust);
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
		ohpl_ntb_set_continuous(mhRust, abOn);
	}

	bool cPhysicsBodyNewton::GetContinuousCollision()
	{
		return ohpl_ntb_continuous(mhRust);
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsBodyNewton::SetGravity(bool abEnabled)
	{
		mbGravity = abEnabled;
		ohpl_ntb_set_gravity(mhRust, abEnabled);
	}
	bool cPhysicsBodyNewton::GetGravity() const
	{
		return ohpl_ntb_gravity(mhRust);
	}

	//-----------------------------------------------------------------------


	////////////////////////////////////////////

	void cPhysicsBodyNewton::RenderDebugGeometry(iLowLevelGraphics *apLowLevel,const cColor &aColor)
	{
		mpWorld->RenderShapeDebugGeometry(mpShape,GetLocalMatrix(),apLowLevel, aColor);
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


}
