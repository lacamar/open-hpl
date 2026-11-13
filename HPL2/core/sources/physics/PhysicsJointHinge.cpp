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

#include "physics/PhysicsJointHinge.h"

#include "system/LowLevelSystem.h"

#include "physics/PhysicsBody.h"
#include "physics/PhysicsWorld.h"

#include "math/Math.h"

namespace hpl {

	iPhysicsJointHinge::iPhysicsJointHinge(const tString &asName,
		iPhysicsBody *apParentBody, iPhysicsBody *apChildBody,
		iPhysicsWorld *apWorld,const cVector3f &avPivotPoint, const cVector3f &avPinDir)
		: iPhysicsJoint(asName,apParentBody,apChildBody,apWorld,avPivotPoint, avPinDir)
	{
		mfMaxAngle = 0;
		mfMinAngle = 0;

		mfPreviousAngle =0;

		cMatrixf mtxPinAndPivot = GetMatrixFromPinAndPivot(mvPinDir, mvPivotPoint);
		CalculateLocalMatrix (mtxPinAndPivot, m_mtxLocalPinPivot0, m_mtxLocalPinPivot1);
		mhRust = ohpl_ntj_create_hinge(mhRustWorld, mpNewtonChildBody, mpNewtonParentBody, (size_t)static_cast<iPhysicsJoint*>(this), RustOps(),
										&m_mtxLocalPinPivot0, &m_mtxLocalPinPivot1, &mpNewtonJoint);
	}

	iPhysicsJointHinge::~iPhysicsJointHinge()
	{

	}

	// PUBLIC METHODS

	void iPhysicsJointHinge::SetMaxAngle(float afAngle)
	{
		mfMaxAngle = afAngle;
		ohpl_ntj_set_max(mhRust, afAngle);
	}
	void iPhysicsJointHinge::SetMinAngle(float afAngle)
	{
		mfMinAngle = afAngle;
		ohpl_ntj_set_min(mhRust, afAngle);
	}
	float iPhysicsJointHinge::GetMaxAngle()
	{
		return ohpl_ntj_max(mhRust);
	}
	float iPhysicsJointHinge::GetMinAngle()
	{
		return ohpl_ntj_min(mhRust);
	}

	cVector3f iPhysicsJointHinge::GetVelocity()
	{
		return cVector3f(0,0,0);
	}

	cVector3f iPhysicsJointHinge::GetAngularVelocity()
	{
		if(mpParentBody)
		{
			cVector3f vAngularVel0 = mpChildBody->GetAngularVelocity();
			cVector3f vAngularVel1 = mpParentBody->GetAngularVelocity();

			return GetPinDir() * cMath::Vector3Dot(vAngularVel0 - vAngularVel1, GetPinDir());
		}
		else
		{
			return GetPinDir() * cMath::Vector3Dot(mpChildBody->GetAngularVelocity(), GetPinDir());
		}
	}

	float iPhysicsJointHinge::GetForceSize()
	{
		return ohpl_ntj_hinge_force_size(mhRust);
	}

	float iPhysicsJointHinge::GetDistance()
	{
		return 0;
	}
	float iPhysicsJointHinge::GetAngle()
	{
		cMatrixf mtxPinPivot0;
		cMatrixf mtxPinPivot1;

		CalculateGlobalMatrix (	m_mtxLocalPinPivot0, m_mtxLocalPinPivot1, mtxPinPivot0, mtxPinPivot1);

		cMatrixf mtxPinPivot0Inv = mtxPinPivot0.GetTranspose();
		cMatrixf mtxPinPivot1Inv = mtxPinPivot1.GetTranspose();

		float fSinAngle = cMath::Vector3Dot( cMath::Vector3Cross(mtxPinPivot0Inv.GetRight(), mtxPinPivot1Inv.GetRight()),
											mtxPinPivot0Inv.GetUp());
		float fCosAngle = cMath::Vector3Dot(mtxPinPivot0Inv.GetRight(), mtxPinPivot1Inv.GetRight());
		float fAngle = atan2(fSinAngle, fCosAngle);

		return fAngle;
	}

	void iPhysicsJointHinge::ResetPosition()
	{
		cMatrixf mtxParent = mpParentBody ? mpParentBody->GetLocalMatrix() : cMatrixf::Identity;
		mpChildBody->SetMatrix(cMath::MatrixMul(cMath::MatrixMul(mtxParent, m_mtxLocalPinPivot1), cMath::MatrixInverse(m_mtxLocalPinPivot0)));
		mpChildBody->SetLinearVelocity(0);
		mpChildBody->SetAngularVelocity(0);
	}

}
