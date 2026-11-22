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
			iPhysicsJoint* p = Self(j);
			if(ohpl_phys_joint_limit_auto_sleep(p->mS->limitAutoSleep, p->mS->limitAutoSleepDist,
												p->mS->limitAutoSleepNumSteps, &p->mS->limitStepCount,
												afMin, afMax, afValue))
				p->mS->child->DisableAfterSimulation();
		}

void iPhysicsJoint::RustNoLimit(size_t j, float afPrev, float afMin, float afMax, bool abSkip)
		{
			iPhysicsJoint* p = Self(j);
			if(p->mS->parent==NULL || p->mS->parent->GetMass()==0)
			{
				if( abSkip==false &&
					( (p->mS->stickyMax && afPrev > afMax) || (p->mS->stickyMin && afPrev < afMin) ) )
				{
					p->mS->child->SetAngularVelocity(0);
					p->mS->child->SetLinearVelocity(0);
				}
			}
			p->OnNoLimit();
		}

void iPhysicsJoint::RustMatrices(size_t j, cMatrixf* apChild, cMatrixf* apParent)
		{
			iPhysicsJoint* p = Self(j);
			*apChild = p->mS->child->GetLocalMatrix();
			*apParent = p->mS->parent ? p->mS->parent->GetLocalMatrix() : cMatrixf::Identity;
		}

void iPhysicsJoint::RustPositionCheck(size_t j, float afDistSqr)
		{
			iPhysicsJoint* p = Self(j);
			if(p->mS->allowPositionReset && p->mS->child->IsActive())
			{
				if(!(afDistSqr <= 0.0025f))
				{
					Error("Body %s jumped more than 0.05 units (%f)- restoring!\n", p->mS->child->GetName().c_str(), sqrtf(afDistSqr));
					p->mS->positionResetPending = true;
				}
			}
		}

	cPhysicsControllerIterator iPhysicsJoint::GetControllerIterator()
	{
		m_mapControllers.clear();
		for(size_t i=0, n=ohpl_h15j_controller_len(mS->shell); i<n; ++i)
		{
			OhplStr name; size_t obj;
			if(ohpl_h15j_controller_at(mS->shell, i, &name, &obj)) m_mapControllers[tString(name.p, name.n)] = reinterpret_cast<iPhysicsController*>(obj);
		}
		return cPhysicsControllerIterator(&m_mapControllers);
	}

	// PROTECTED METHODS

	void iPhysicsJoint::OnMaxLimit()
	{
		OhplLimitEffect effect = { OhplS(mMaxLimit.msSound), mMaxLimit.mfMinSpeed, mMaxLimit.mfMaxSpeed };
		ohpl_phys_joint_on_limit(OhplPhysHost(), reinterpret_cast<size_t>(this), true, &effect);
	}

	void iPhysicsJoint::OnMinLimit()
	{
		OhplLimitEffect effect = { OhplS(mMinLimit.msSound), mMinLimit.mfMinSpeed, mMinLimit.mfMaxSpeed };
		ohpl_phys_joint_on_limit(OhplPhysHost(), reinterpret_cast<size_t>(this), false, &effect);
	}

	void iPhysicsJoint::SetSound(cSoundEntity *apSound)
	{
		mS->sound = apSound;
        if(apSound)
			mS->soundId = apSound->GetCreationID();
		else
			mS->soundId = -1;
	}

}
