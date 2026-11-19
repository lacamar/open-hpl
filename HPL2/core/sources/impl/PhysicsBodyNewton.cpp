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

#include "physics/PhysicsBody.h"

#include "physics/CollideShape.h"
#include "physics/PhysicsWorld.h"
#include "physics/PhysicsMaterial.h"
#include "system/LowLevelSystem.h"
#include "graphics/LowLevelGraphics.h"
#include "math/Math.h"
#include "scene/Node3D.h"
#include <algorithm>
#include <vector>
#include "impl/RustNewton.h"
#include "impl/RustNewtonBackend.h"

namespace hpl {

	bool iPhysicsBody::mbUseCallback = true;

	void iPhysicsBody::DeleteLowLevel()
	{
		ohpl_ntb_destroy(mS->rust);
		hplDelete(mS->callback);
	}

	// CALLBACK METHODS

    void cPhysicsBodyNewtonCallback::OnTransformUpdate(iEntity3D * apEntity)
	{
		if(iPhysicsBody::mbUseCallback==false) return;
		iPhysicsBody *pBody = static_cast<iPhysicsBody*>(apEntity);
		ohpl_ntb_set_matrix(pBody->mS->rust, &apEntity->GetLocalMatrix());
	}

	// PUBLIC METHODS

	void iPhysicsBody::SetMaterial(iPhysicsMaterial* apMaterial)
	{
		mS->material = apMaterial;
		if(apMaterial == NULL) return;
		ohpl_ntb_set_material(mS->rust, static_cast<iPhysicsMaterial*>(mS->material)->GetId());
	}

	void iPhysicsBody::SetLinearVelocity(const cVector3f &avVel)
	{
		ohpl_ntb_set_velocity(mS->rust, &avVel);
	}
	cVector3f iPhysicsBody::GetLinearVelocity() const
	{
		cVector3f vVel;
		ohpl_ntb_velocity(mS->rust, &vVel);
		return vVel;
	}

	void iPhysicsBody::SetAngularVelocity(const cVector3f &avVel)
	{
		ohpl_ntb_set_omega(mS->rust, &avVel);
	}
	cVector3f iPhysicsBody::GetAngularVelocity() const
	{
		cVector3f vVel;
		ohpl_ntb_omega(mS->rust, &vVel);
		return vVel;
	}

	void iPhysicsBody::SetLinearDamping(float afDamping)
	{
		ohpl_ntb_set_linear_damping(mS->rust, afDamping);
	}
	float iPhysicsBody::GetLinearDamping() const
	{
		return ohpl_ntb_linear_damping(mS->rust);
	}

	void iPhysicsBody::SetAngularDamping(float afDamping)
	{
		ohpl_ntb_set_angular_damping(mS->rust, afDamping);
	}
	float iPhysicsBody::GetAngularDamping() const
	{
		return ohpl_ntb_angular_damping(mS->rust);
	}

	void iPhysicsBody::SetMaxLinearSpeed(float afSpeed)
	{
		ohpl_ntb_set_max_linear(mS->rust, afSpeed);
	}
	float iPhysicsBody::GetMaxLinearSpeed() const
	{
		return ohpl_ntb_max_linear(mS->rust);
	}

	void iPhysicsBody::SetMaxAngularSpeed(float afDamping)
	{
		ohpl_ntb_set_max_angular(mS->rust, afDamping);
	}
	float iPhysicsBody::GetMaxAngularSpeed() const
	{
		return ohpl_ntb_max_angular(mS->rust);
	}

	cVector3f iPhysicsBody::GetInertiaVector()
	{
		cVector3f vInertia;
		ohpl_ntb_inertia(mS->rust, &vInertia);
		return vInertia;
	}

	cMatrixf iPhysicsBody::GetInertiaMatrix()
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

	void  iPhysicsBody::SetMass(float afMass)
	{
		ohpl_ntb_set_mass(mS->rust, static_cast<iCollideShape*>(mS->shape)->GetNewtonCollision(), afMass);
	}
	float iPhysicsBody::GetMass() const
	{
		return ohpl_ntb_mass(mS->rust);
	}

	void  iPhysicsBody::SetMassCentre(const cVector3f& avCentre)
	{
		ohpl_ntb_set_mass_centre(mS->rust, &avCentre);
	}

	cVector3f iPhysicsBody::GetMassCentre() const
	{
		cVector3f vCentre;
		ohpl_ntb_mass_centre(mS->rust, &vCentre);
		return vCentre;
	}

	void iPhysicsBody::AddForce(const cVector3f &avForce)
	{
		ohpl_ntb_add_force(mS->rust, &avForce);
		Enable();
	}

	void iPhysicsBody::AddForceAtPosition(const cVector3f &avForce, const cVector3f &avPos)
	{
		ohpl_ntb_add_force_at_position(mS->rust, &avForce, &avPos, &GetLocalMatrix());
		Enable();
	}

	void iPhysicsBody::AddTorque(const cVector3f &avTorque)
	{
		ohpl_ntb_add_torque(mS->rust, &avTorque);
		Enable();
	}

	void iPhysicsBody::AddImpulse(const cVector3f &avImpulse)
	{
		ohpl_ntb_add_impulse(mS->rust, &avImpulse, &GetWorldMatrix());
	}
	void iPhysicsBody::AddImpulseAtPosition(const cVector3f &avImpulse, const cVector3f &avPos)
	{
		ohpl_ntb_add_impulse_at(mS->rust, &avImpulse, &avPos);
	}

	void iPhysicsBody::Sleep()
	{
		ohpl_ntb_set_freeze(mS->rust, 1);
	}

	void iPhysicsBody::Enable()
	{
		mS->freezeCount = 0;
		ohpl_ntb_set_freeze(mS->rust, 0);
	}
	bool iPhysicsBody::GetEnabled() const
	{
		return ohpl_ntb_enabled(mS->rust);
	}

	void iPhysicsBody::SetAutoDisable(bool abEnabled)
	{
		ohpl_ntb_set_auto_sleep(mS->rust, abEnabled);
	}
	bool iPhysicsBody::GetAutoDisable() const
	{
		return ohpl_ntb_auto_sleep(mS->rust);
	}

	void iPhysicsBody::SetAutoDisableLinearThreshold(float afThresold)
	{
		ohpl_ntb_set_auto_linear(mS->rust, afThresold);
	}
	float iPhysicsBody::GetAutoDisableLinearThreshold() const
	{
		return ohpl_ntb_auto_linear(mS->rust);
	}

	void iPhysicsBody::SetAutoDisableAngularThreshold(float afThresold)
	{
		ohpl_ntb_set_auto_angular(mS->rust, afThresold);
	}
	float iPhysicsBody::GetAutoDisableAngularThreshold() const
	{
		return ohpl_ntb_auto_angular(mS->rust);
	}

	void iPhysicsBody::SetAutoDisableNumSteps(int anNum)
	{
		ohpl_ntb_set_auto_steps(mS->rust, anNum);
	}

	int iPhysicsBody::GetAutoDisableNumSteps() const
	{
		return ohpl_ntb_auto_steps(mS->rust);
	}

	void iPhysicsBody::SetContinuousCollision(bool abOn)
	{
		ohpl_ntb_set_continuous(mS->rust, abOn);
	}

	bool iPhysicsBody::GetContinuousCollision()
	{
		return ohpl_ntb_continuous(mS->rust);
	}

	void iPhysicsBody::SetGravity(bool abEnabled)
	{
		ohpl_ntb_set_gravity(mS->rust, abEnabled);
	}
	bool iPhysicsBody::GetGravity() const
	{
		return ohpl_ntb_gravity(mS->rust);
	}

	void iPhysicsBody::RenderDebugGeometry(iLowLevelGraphics *apLowLevel,const cColor &aColor)
	{
		mS->world->RenderShapeDebugGeometry(mS->shape,GetLocalMatrix(),apLowLevel, aColor);
	}

	// HPL3 iCollideShape::CalculateBouyancyForces: lift from spheres packed in the shape, then drag
	bool iPhysicsBody::CalcHpl3Buoyancy(const cVector3f &avGravity, float afTimeStep, cVector3f *apForce, cVector3f *apTorque)
	{
		iCollideShape *pShape = static_cast<iCollideShape*>(mS->shape);
		OhplL4Buoy b = BuoyState();
		float fMass = ohpl_ntb_mass(mS->rust);
		if (fMass == 0 || b.density * mS->buoyancyDensityMul <= 0)
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
			iCollideShape *pSub = static_cast<iCollideShape*>(pShape->GetSubShape(i));
			float fR;
			const tVector3fVec &vPoints = pSub->GetBuoyancyPoints(fR);
			pSubs[i] = OhplNewtonSubShape{vPoints.empty() ? nullptr : &vPoints[0], vPoints.size(), fR, pSub->GetVolume()};
		}

		OhplNewtonBuoyIn in;
		in.mass = fMass;
		in.density = b.density;
		in.densityMul = mS->buoyancyDensityMul;
		in.dragDensityMul = b.dragDensityMul;
		in.linearViscosity = b.linearViscosity;
		in.angularViscosity = b.angularViscosity;
		in.maxLinearSpeed = GetMaxLinearSpeed();
		in.maxAngularSpeed = GetMaxAngularSpeed();
		in.shapeVolume = pShape->GetVolume();
		in.timeStep = afTimeStep;
		in.noPrevVel = b.noPrevVel;
		in.surface = b.surface;
		in.matrix = mtx;
		in.inverse = cMath::MatrixInverse(mtx);
		in.inertia = GetInertiaMatrix();
		in.massCentre = GetMassCentre();
		in.gravity = avGravity;
		in.linearVelocity = GetLinearVelocity();
		in.angularVelocity = GetAngularVelocity();
		in.prevLinearVel = b.prevLinearVel;
		in.prevAngularVel = b.prevAngularVel;

		OhplNewtonBuoyOut out;
		if (ohpl_newton_buoyancy(&in, pSubs, lSubNum, &out) == false)
			return false;

		if (out.dragUpdated)
		{
			ohpl_ntb_set_buoy_drag(mS->rust, &out.prevLinearVel, &out.prevAngularVel, out.noPrevVel, out.dragDensityMul);
		}
		*apForce = out.force;
		*apTorque = out.torque;
		return true;
	}

}
