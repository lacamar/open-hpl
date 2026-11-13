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

#include "impl/PhysicsJointHingeNewton.h"

#include "system/LowLevelSystem.h"

#include "impl/PhysicsBodyNewton.h"
#include "impl/PhysicsWorldNewton.h"

#include "math/Math.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cPhysicsJointHingeNewton::cPhysicsJointHingeNewton(const tString &asName, 
		iPhysicsBody *apParentBody, iPhysicsBody *apChildBody, 
		iPhysicsWorld *apWorld,const cVector3f &avPivotPoint, const cVector3f &avPinDir)
		: iPhysicsJointNewton<iPhysicsJointHinge>(asName,apParentBody,apChildBody,apWorld,avPivotPoint, avPinDir)
	{
		mfMaxAngle = 0;
		mfMinAngle = 0;

		mfPreviousAngle =0;

		cMatrixf mtxPinAndPivot = GetMatrixFromPinAndPivot(mvPinDir, mvPivotPoint);
		CalculateLocalMatrix (mtxPinAndPivot, m_mtxLocalPinPivot0, m_mtxLocalPinPivot1);
		mhRust = ohpl_ntj_create_hinge(mhRustWorld, mpNewtonChildBody, mpNewtonParentBody, (size_t)static_cast<iPhysicsJointNewton<iPhysicsJointHinge>*>(this), RustOps(),
										&m_mtxLocalPinPivot0, &m_mtxLocalPinPivot1, &mpNewtonJoint);
	}

	//-----------------------------------------------------------------------

	cPhysicsJointHingeNewton::~cPhysicsJointHingeNewton()
	{

	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cPhysicsJointHingeNewton::SetMaxAngle(float afAngle)
	{
		mfMaxAngle = afAngle;
		ohpl_ntj_set_max(mhRust, afAngle);
	}
	void cPhysicsJointHingeNewton::SetMinAngle(float afAngle)
	{
		mfMinAngle = afAngle;
		ohpl_ntj_set_min(mhRust, afAngle);
	}
	float cPhysicsJointHingeNewton::GetMaxAngle()
	{
		return ohpl_ntj_max(mhRust);
	}
	float cPhysicsJointHingeNewton::GetMinAngle()
	{
		return ohpl_ntj_min(mhRust);
	}

	//-----------------------------------------------------------------------

	cVector3f cPhysicsJointHingeNewton::GetVelocity()
	{
		return cVector3f(0,0,0);
	}

	cVector3f cPhysicsJointHingeNewton::GetAngularVelocity()
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

	//-----------------------------------------------------------------------

	float cPhysicsJointHingeNewton::GetForceSize()
	{
		return ohpl_ntj_hinge_force_size(mhRust);
	}

	//-----------------------------------------------------------------------

	float cPhysicsJointHingeNewton::GetDistance()
	{
		return 0;
	}
	float cPhysicsJointHingeNewton::GetAngle()
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

	
	//-----------------------------------------------------------------------
	
	void cPhysicsJointHingeNewton::ResetPosition()
	{
		cMatrixf mtxParent = mpParentBody ? mpParentBody->GetLocalMatrix() : cMatrixf::Identity;
		mpChildBody->SetMatrix(cMath::MatrixMul(cMath::MatrixMul(mtxParent, m_mtxLocalPinPivot1), cMath::MatrixInverse(m_mtxLocalPinPivot0)));
		mpChildBody->SetLinearVelocity(0);
		mpChildBody->SetAngularVelocity(0);
	}
	
	//-----------------------------------------------------------------------

}
