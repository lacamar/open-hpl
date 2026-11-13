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

#include "physics/PhysicsJoint.h"

#include "physics/CollideShape.h"
#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsController.h"
#include "system/LowLevelSystem.h"

#include "sound/Sound.h"
#include "scene/SoundEntity.h"
#include "sound/SoundHandler.h"
#include "sound/SoundChannel.h"

#include "scene/World.h"
#include "scene/Scene.h"

#include "engine/Engine.h"

#include "math/Math.h"

#include "engine/ScriptFuncs.h"
#include "physics/RustPhysics.h"
#include "physics/RustPhysBook.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	size_t iPhysicsJoint::HostOp(int32_t alOp, OhplH15Wa *a)
	{
		iPhysicsJoint *pJ = reinterpret_cast<iPhysicsJoint*>(a->p[0]);
		iPhysicsController *pC = reinterpret_cast<iPhysicsController*>(a->p[0]);
		const tString *ps = NULL;
		switch(alOp)
		{
		case 401: reinterpret_cast<iPhysicsBody*>(a->p[0])->AddJoint(reinterpret_cast<iPhysicsJoint*>(a->p[1])); return 0;
		case 402: *reinterpret_cast<cMatrixf*>(a->p[1]) = reinterpret_cast<iPhysicsBody*>(a->p[0])->GetLocalMatrix(); return 0;
		case 403: reinterpret_cast<iPhysicsBody*>(a->p[0])->RemoveJoint(reinterpret_cast<iPhysicsJoint*>(a->p[1])); return 0;
		case 404: ps = &pC->GetName(); break;
		case 405: pC->SetBody(reinterpret_cast<iPhysicsBody*>(a->p[1])); return 0;
		case 406: pC->SetJoint(reinterpret_cast<iPhysicsJoint*>(a->p[1])); return 0;
		case 407: pC->SetActive(a->i[0] != 0); return 0;
		case 408: pC->SetPaused(a->i[0] != 0); return 0;
		case 409: reinterpret_cast<iPhysicsWorld*>(a->p[0])->DestroyController(reinterpret_cast<iPhysicsController*>(a->p[1])); return 0;
		case 410: reinterpret_cast<iPhysicsWorld*>(a->p[0])->GetWorld()->DestroySoundEntity(reinterpret_cast<cSoundEntity*>(a->p[1])); return 0;
		case 411: pJ->ResetPosition(); return 0;
		case 412: ps = &pJ->msMoveSound; break;
		case 413: ps = &pJ->msBreakSound; break;
		case 414: hplDelete(reinterpret_cast<iPhysicsJointCallback*>(a->p[0])); return 0;
		}
		if(ps){ a->s = ps->c_str(); a->n = ps->size(); }
		return 0;
	}

	iPhysicsJoint::iPhysicsJoint(const tString &asName, iPhysicsBody *apParentBody, iPhysicsBody *apChildBody,
		iPhysicsWorld *apWorld,const cVector3f &avPivotPoint, const cVector3f &avPinDir)
		: msName(asName)
	{
		ohpl_h15j_new(HostOp, reinterpret_cast<size_t>(this), &mS, reinterpret_cast<size_t>(OhplPhysHost()), reinterpret_cast<size_t>(apParentBody),
			reinterpret_cast<size_t>(apChildBody), reinterpret_cast<size_t>(apWorld), OhplV3Pod{avPivotPoint.x, avPivotPoint.y, avPivotPoint.z},
			OhplV3Pod{avPinDir.x, avPinDir.y, avPinDir.z});

		iPhysicsWorld *pNWorld = apWorld;
		mpNewtonWorld = pNWorld->GetNewtonWorld();
		if(apParentBody==NULL)
			mpNewtonParentBody = NULL;
		else
			mpNewtonParentBody = apParentBody->GetNewtonBody();
		mpNewtonChildBody = apChildBody->GetNewtonBody();
		mhRustWorld = pNWorld->RustHandle();
	}

	iPhysicsJoint::~iPhysicsJoint()
	{
		if(mpChildBody || mpParentBody)
		{
			ohpl_ntj_destroy(mhRustWorld, mhRust);
		}
		ohpl_h15j_drop(mS.shell, reinterpret_cast<size_t>(this));
	}

	//-----------------------------------------------------------------------

void iPhysicsJoint::SetCollideBodies(bool abX)
		{
			ohpl_ntj_set_collide(mhRust, abX);
		}

bool iPhysicsJoint::GetCollideBodies()
		{
			return ohpl_ntj_collide(mhRust);
		}

void iPhysicsJoint::SetStiffness(float afX)
		{
			ohpl_ntj_set_stiffness(mhRust, afX);
		}

float iPhysicsJoint::GetStiffness()
		{
			return ohpl_ntj_stiffness(mhRust);
		}

cMatrixf iPhysicsJoint::GetMatrixFromPinAndPivot(const cVector3f& avPinDir,const cVector3f& avPivot)
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

void iPhysicsJoint::CalculateGlobalMatrix(const cMatrixf& a_mtxLocalMatrix0, const cMatrixf& a_mtxLocalMatrix1, cMatrixf& a_mtxMatrix0, cMatrixf& a_mtxMatrix1)
		{
			cMatrixf mtxMatrix0 = mpChildBody->GetLocalMatrix();
			cMatrixf mtxMatrix1 = mpParentBody ? mpParentBody->GetLocalMatrix() : cMatrixf::Identity;

			a_mtxMatrix0 = cMath::MatrixMul(mtxMatrix0, a_mtxLocalMatrix0);
			a_mtxMatrix1 = cMath::MatrixMul(mtxMatrix1, a_mtxLocalMatrix1);
		}

void iPhysicsJoint::CalculateLocalMatrix(const cMatrixf& a_mtxPinsAndPivotFrame, cMatrixf& a_mtxLocalMatrix0, cMatrixf& a_mtxLocalMatrix1)
		{
			cMatrixf mtxMatrix0 = mpChildBody->GetLocalMatrix();
			cMatrixf mtxMatrix1 = mpParentBody ? mpParentBody->GetLocalMatrix() : cMatrixf::Identity;

			// calculate the relative matrix of the pin and pivot on each body
			a_mtxLocalMatrix0 = cMath::MatrixMul( cMath::MatrixInverse(mtxMatrix0), a_mtxPinsAndPivotFrame);
			a_mtxLocalMatrix1 = cMath::MatrixMul( cMath::MatrixInverse(mtxMatrix1), a_mtxPinsAndPivotFrame);
		}

const OhplL4JointOps* iPhysicsJoint::RustOps()
		{
			static const OhplL4JointOps ops = { RustLimitEvent, RustCheckAutoSleep, RustNoLimit, RustMatrices, RustPositionCheck };
			return &ops;
		}

void iPhysicsJoint::RustLimitEvent(size_t j, int32_t kind)
		{
			iPhysicsJoint* p = Self(j);
			if(kind == 0) p->OnMinLimit();
			else if(kind == 1) p->OnMaxLimit();
			else p->OnNoLimit();
		}

void iPhysicsJoint::RustCheckAutoSleep(size_t j, float afMin, float afMax, float afValue)
		{
			CheckLimitAutoSleep(Self(j), afMin, afMax, afValue);
		}

void iPhysicsJoint::RustNoLimit(size_t j, float afPrev, float afMin, float afMax, bool abSkip)
		{
			iPhysicsJoint* p = Self(j);
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

void iPhysicsJoint::RustMatrices(size_t j, cMatrixf* apChild, cMatrixf* apParent)
		{
			iPhysicsJoint* p = Self(j);
			*apChild = p->mpChildBody->GetLocalMatrix();
			*apParent = p->mpParentBody ? p->mpParentBody->GetLocalMatrix() : cMatrixf::Identity;
		}

void iPhysicsJoint::RustPositionCheck(size_t j, float afDistSqr)
		{
			iPhysicsJoint* p = Self(j);
			if(p->mbAllowPositionReset && p->mpChildBody->IsActive())
			{
				if(!(afDistSqr <= 0.0025f))
				{
					Error("Body %s jumped more than 0.05 units (%f)- restoring!\n", p->mpChildBody->GetName().c_str(), sqrtf(afDistSqr));
					p->mbPositionResetPending = true;
				}
			}
		}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	/**
	 * This should only be used by PhysicsBody.
	 */
	void iPhysicsJoint::RemoveBody(iPhysicsBody *apBody)
	{
		if(mpParentBody == apBody) mpParentBody = NULL;
		if(mpChildBody == apBody) mpChildBody = NULL;
	}

	//-----------------------------------------------------------------------

	void iPhysicsJoint::AddController(iPhysicsController *apController)
	{
		ohpl_h15j_add_controller(mS.shell, reinterpret_cast<size_t>(this), reinterpret_cast<size_t>(apController));
	}

	//-----------------------------------------------------------------------

	iPhysicsController* iPhysicsJoint::GetController(const tString &asName)
	{
		return reinterpret_cast<iPhysicsController*>(ohpl_h15j_get_controller(mS.shell, asName.c_str(), asName.size()));
	}

	//-----------------------------------------------------------------------

	bool iPhysicsJoint::ChangeController(const tString &asName)
	{
		return ohpl_h15j_change_controller(mS.shell, asName.c_str(), asName.size());
	}

	//-----------------------------------------------------------------------

	void iPhysicsJoint::SetAllControllersPaused(bool abX)
	{
		ohpl_h15j_set_all_paused(mS.shell, abX);
	}

	//-----------------------------------------------------------------------

	int iPhysicsJoint::ControllerCount()
	{
		return (int)ohpl_h15j_controller_len(mS.shell);
	}

	iPhysicsController* iPhysicsJoint::ControllerAt(int alIdx)
	{
		OhplStr name; size_t obj = 0;
		ohpl_h15j_controller_at(mS.shell, alIdx, &name, &obj);
		return reinterpret_cast<iPhysicsController*>(obj);
	}

	cPhysicsControllerIterator iPhysicsJoint::GetControllerIterator()
	{
		m_mapControllers.clear();
		for(size_t i=0, n=ohpl_h15j_controller_len(mS.shell); i<n; ++i)
		{
			OhplStr name; size_t obj;
			if(ohpl_h15j_controller_at(mS.shell, i, &name, &obj)) m_mapControllers[tString(name.p, name.n)] = reinterpret_cast<iPhysicsController*>(obj);
		}
		return cPhysicsControllerIterator(&m_mapControllers);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iPhysicsJoint::OnMaxLimit()
	{
		OhplLimitEffect effect = { OhplS(mMaxLimit.msSound), mMaxLimit.mfMinSpeed, mMaxLimit.mfMaxSpeed };
		ohpl_phys_joint_on_limit(OhplPhysHost(), reinterpret_cast<size_t>(this), true, &effect);
	}

	//-----------------------------------------------------------------------

	void iPhysicsJoint::OnMinLimit()
	{
		OhplLimitEffect effect = { OhplS(mMinLimit.msSound), mMinLimit.mfMinSpeed, mMinLimit.mfMaxSpeed };
		ohpl_phys_joint_on_limit(OhplPhysHost(), reinterpret_cast<size_t>(this), false, &effect);
	}

	//-----------------------------------------------------------------------



	//-----------------------------------------------------------------------

	bool iPhysicsJoint::OnPhysicsUpdate()
	{
		return ohpl_h15j_update(mS.shell, reinterpret_cast<size_t>(this));
	}

	//-----------------------------------------------------------------------

	void iPhysicsJoint::SetSound(cSoundEntity *apSound)
	{
		mpSound = apSound;
        if(apSound)
			mlSoundID = apSound->GetCreationID();
		else
			mlSoundID = -1;
	}

	//-----------------------------------------------------------------------



	//-----------------------------------------------------------------------

	void iPhysicsJoint::OnNoLimit()
	{
		mbHasCollided = false;
	}

	//-----------------------------------------------------------------------

	void iPhysicsJoint::Break()
	{
		mbBroken = true;
		mbBreakable = true;
	}

	//-----------------------------------------------------------------------

	bool iPhysicsJoint::CheckBreakage()
	{
		return ohpl_h15j_check_breakage(mS.shell, reinterpret_cast<size_t>(this));
	}

	//-----------------------------------------------------------------------

	void iPhysicsJoint::CheckLimitAutoSleep(iPhysicsJoint *apJoint,
											const float afMin, const float afMax,
											const float afDist)
	{
		const bool bSleep = ohpl_phys_joint_limit_auto_sleep(apJoint->mbLimitAutoSleep, apJoint->mfLimitAutoSleepDist,
											apJoint->mlLimitAutoSleepNumSteps, &apJoint->mlLimitStepCount,
											afMin, afMax, afDist);
		if(bSleep)
		{
			apJoint->mpChildBody->DisableAfterSimulation();
		}
	}

	//-----------------------------------------------------------------------
}
