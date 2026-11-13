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

#ifndef HPL_PHYSICS_JOINT_NEWTON_H
#define HPL_PHYSICS_JOINT_NEWTON_H

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#endif
#include <Newton.h>
#include "impl/PhysicsWorldNewton.h"
#include "impl/PhysicsBodyNewton.h"
#include "math/Math.h"
#include "system/LowLevelSystem.h"
#include "impl/RustNewtonBackend.h"
#include <cmath>

namespace hpl {

	template <typename T>
	class iPhysicsJointNewton : public T
	{
	public:
		iPhysicsJointNewton(const tString &asName, iPhysicsBody *apParentBody, iPhysicsBody *apChildBody,
						iPhysicsWorld *apWorld,const cVector3f &avPivotPoint,const cVector3f &avPinDir)
		: T(asName, apParentBody, apChildBody, apWorld,avPivotPoint,avPinDir)
		{
			cPhysicsWorldNewton *pNWorld = static_cast<cPhysicsWorldNewton*>(apWorld);

			mpNewtonWorld = pNWorld->GetNewtonWorld();

			cPhysicsBodyNewton *pNParent = static_cast<cPhysicsBodyNewton*>(apParentBody);
			cPhysicsBodyNewton *pNChild = static_cast<cPhysicsBodyNewton*>(apChildBody);

			if(apParentBody==NULL)
				mpNewtonParentBody = NULL;
			else
				mpNewtonParentBody = pNParent->GetNewtonBody();

			mpNewtonChildBody = pNChild->GetNewtonBody();
			mhRustWorld = pNWorld->RustHandle();
		}

		virtual ~iPhysicsJointNewton()
		{
			//Skip this for now and let newton handle it..
			//Log("Destroying newton joint!\n");
			if(this->mpChildBody || this->mpParentBody)
			{
				ohpl_ntj_destroy(mhRustWorld, mhRust);
			}
		}

		//-------------------------------------------

		void SetCollideBodies(bool abX)
		{
			ohpl_ntj_set_collide(mhRust, abX);
		}

		bool GetCollideBodies()
		{
			return ohpl_ntj_collide(mhRust);
		}

		//-------------------------------------------

		void SetStiffness(float afX)
		{
			ohpl_ntj_set_stiffness(mhRust, afX);
		}
		float GetStiffness()
		{
			return ohpl_ntj_stiffness(mhRust);
		}

		//-------------------------------------------

	protected:
		//-------------------------------------------	

		cMatrixf GetMatrixFromPinAndPivot(const cVector3f& avPinDir,const cVector3f& avPivot)
		{
			cMatrixf mtxPinAndPivot = cMatrixf::Identity;
			cVector3f vUp = cMath::Vector3Normalize(avPinDir);
			cVector3f vTemp = cMath::Vector3Normalize(cVector3f(1) - vUp);
			if(vTemp == vUp) vTemp = cMath::Vector3Normalize(vTemp * cVector3f(1.5f, 0.5f, 2.0f)); //Make sure all does not become wierd in the special case.

			cVector3f vRight = cMath::Vector3Normalize( cMath::Vector3Cross(vUp, vTemp ));
			cVector3f vForward = cMath::Vector3Normalize(cMath::Vector3Cross(vUp, vRight));

			mtxPinAndPivot.SetUp(vUp);
			mtxPinAndPivot.SetRight(vRight);
			mtxPinAndPivot.SetForward(vForward);
			mtxPinAndPivot = mtxPinAndPivot.GetTranspose();

			mtxPinAndPivot.SetTranslation(avPivot);

			return mtxPinAndPivot;
		}


		//-------------------------------------------
		
		void CalculateGlobalMatrix (const cMatrixf& a_mtxLocalMatrix0, const cMatrixf& a_mtxLocalMatrix1, cMatrixf& a_mtxMatrix0, cMatrixf& a_mtxMatrix1)
		{
			cMatrixf mtxMatrix0 = this->mpChildBody->GetLocalMatrix();
			cMatrixf mtxMatrix1 = this->mpParentBody ? this->mpParentBody->GetLocalMatrix() : cMatrixf::Identity;

			a_mtxMatrix0 = cMath::MatrixMul(mtxMatrix0, a_mtxLocalMatrix0);
			a_mtxMatrix1 = cMath::MatrixMul(mtxMatrix1, a_mtxLocalMatrix1);
		}
		
		//-------------------------------------------

		void CalculateLocalMatrix (const cMatrixf& a_mtxPinsAndPivotFrame, cMatrixf& a_mtxLocalMatrix0, cMatrixf& a_mtxLocalMatrix1)
		{
			cMatrixf mtxMatrix0 = this->mpChildBody->GetLocalMatrix();
			cMatrixf mtxMatrix1 = this->mpParentBody ? this->mpParentBody->GetLocalMatrix() : cMatrixf::Identity;

			// calculate the relative matrix of the pin and pivot on each body
			a_mtxLocalMatrix0 = cMath::MatrixMul( cMath::MatrixInverse(mtxMatrix0), a_mtxPinsAndPivotFrame);
			a_mtxLocalMatrix1 = cMath::MatrixMul( cMath::MatrixInverse(mtxMatrix1), a_mtxPinsAndPivotFrame);
		}
		
		//-------------------------------------------

		static const OhplL4JointOps* RustOps()
		{
			static const OhplL4JointOps ops = { RustLimitEvent, RustCheckAutoSleep, RustNoLimit, RustMatrices, RustPositionCheck };
			return &ops;
		}

		static iPhysicsJointNewton<T>* Self(size_t j) { return reinterpret_cast<iPhysicsJointNewton<T>*>(j); }

		static void RustLimitEvent(size_t j, int32_t kind)
		{
			iPhysicsJointNewton<T>* p = Self(j);
			if(kind == 0) p->OnMinLimit();
			else if(kind == 1) p->OnMaxLimit();
			else p->OnNoLimit();
		}

		static void RustCheckAutoSleep(size_t j, float afMin, float afMax, float afValue)
		{
			T::CheckLimitAutoSleep(Self(j), afMin, afMax, afValue);
		}

		static void RustNoLimit(size_t j, float afPrev, float afMin, float afMax, bool abSkip)
		{
			iPhysicsJointNewton<T>* p = Self(j);
			if(p->mpParentBody==NULL || p->mpParentBody->GetMass()==0)
			{
				if( abSkip==false &&
					( (p->mbStickyMaxLimit && afPrev > afMax) || (p->mbStickyMinLimit && afPrev < afMin) ) )
				{
					p->mpChildBody->SetAngularVelocity(0);
					p->mpChildBody->SetLinearVelocity(0);
				}
			}
			p->OnNoLimit();
		}

		static void RustMatrices(size_t j, cMatrixf* apChild, cMatrixf* apParent)
		{
			iPhysicsJointNewton<T>* p = Self(j);
			*apChild = p->mpChildBody->GetLocalMatrix();
			*apParent = p->mpParentBody ? p->mpParentBody->GetLocalMatrix() : cMatrixf::Identity;
		}

		static void RustPositionCheck(size_t j, float afDistSqr)
		{
			iPhysicsJointNewton<T>* p = Self(j);
			if(p->mbAllowPositionReset && p->mpChildBody->IsActive())
			{
				if(!(afDistSqr <= 0.0025f))
				{
					Error("Body %s jumped more than 0.05 units (%f)- restoring!\n", p->mpChildBody->GetName().c_str(), sqrtf(afDistSqr));
					p->mbPositionResetPending = true;
				}
			}
		}
		//-------------------------------------------

		NewtonJoint* mpNewtonJoint;
		NewtonWorld* mpNewtonWorld;
		uint64_t mhRust = 0;
		uint64_t mhRustWorld = 0;
		NewtonBody* mpNewtonParentBody;
		NewtonBody* mpNewtonChildBody;
	};
};
#endif // HPL_PHYSICS_JOINT_NEWTON_H
