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

#include "physics/PhysicsRope.h"

#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"

#include "scene/World.h"
#include "scene/SoundEntity.h"

#include "sound/Sound.h"
#include "sound/SoundHandler.h"

#include "system/LowLevelSystem.h"
#include "graphics/LowLevelGraphics.h"

#include "math/Math.h"
#include "impl/RustBackends.h"
#include "physics/RustPhysicsH14.h"

namespace hpl {

	size_t iPhysicsRope::HostOp(int32_t alOp, OhplH15Wa *a)
	{
		iPhysicsBody *pBody = reinterpret_cast<iPhysicsBody*>(a->p[0]);
		iPhysicsRope *pRope = reinterpret_cast<iPhysicsRope*>(a->p[0]);
		switch(alOp)
		{
		case 601: { cVector3f v = pBody->GetAngularVelocity(); a->rv[0] = v.x; a->rv[1] = v.y; a->rv[2] = v.z; return 0; }
		case 602: { cVector3f v = pBody->GetLinearVelocity(); a->rv[0] = v.x; a->rv[1] = v.y; a->rv[2] = v.z; return 0; }
		case 603: return pBody->GetEnabled();
		case 604: pBody->Enable(); return 0;
		case 605: pBody->AddAttachedVerletContainer(reinterpret_cast<iPhysicsRope*>(a->p[1])); return 0;
		case 606: pBody->RemoveAttachedVerletContainer(reinterpret_cast<iPhysicsRope*>(a->p[1])); return 0;
		case 607: pRope->SetSleeping(false); return 0;
		case 608: *reinterpret_cast<cMatrixf*>(a->p[1]) = pBody->GetLocalMatrix(); return 0;
		case 609: { cVector3f v = reinterpret_cast<cVerletParticle*>(a->p[0])->GetPosition(); a->rv[0] = v.x; a->rv[1] = v.y; a->rv[2] = v.z; return 0; }
		case 610: pRope->mForcePid[a->p[1]].Reset(); return 0;
		}
		return 0;
	}

	iPhysicsRope::iPhysicsRope(const tString &asName,iPhysicsWorld *apWorld, const cVector3f &avStartPos , const cVector3f &avEndPos)
		: iVerletParticleContainer(asName, apWorld)
	{
		//Add start and end particles.
		mlstParticles.push_front(	hplNew( cVerletParticle, (this,avStartPos, 0) ) );
		mlstParticles.push_back(	hplNew( cVerletParticle, (this,avEndPos, 1) ) );
		RefreshParticleCache();

		for(int i=0; i< 2;++i) mForcePid[i].SetErrorNum(10);

		cVector3f vGravity = mpWorld->GetGravity();
		ohpl_h14_rope_init(&mS, OhplV3{vGravity.x, vGravity.y, vGravity.z}, OhplV3{avStartPos.x, avStartPos.y, avStartPos.z},
			OhplV3{avEndPos.x, avEndPos.y, avEndPos.z});
	}

	iPhysicsRope::~iPhysicsRope()
	{
		for(size_t i=0;i<2;++i)
		{
			if(mS.attached[i].mpBody)
			{
				mS.attached[i].mpBody->RemoveAttachedVerletContainer(this);
			}
		}

		STLDeleteAll(mlstParticles);
		hplDelete(mpRayParticleCallback);
	}

	// PUBLIC METHODS

	void iPhysicsRope::SetMotorActive(bool abX)
	{
		ohpl_h14_rope_set_motor(HostOp, reinterpret_cast<size_t>(this), &mS, abX);
	}

	void iPhysicsRope::SetAutoMoveActive(bool abX)
	{
		ohpl_h14_rope_set_auto_move(HostOp, reinterpret_cast<size_t>(this), &mS, abX);
	}

	void iPhysicsRope::RemoveAttachedBody(iPhysicsBody *apBody, bool abRemoveContainerFromBody)
	{
		ohpl_h14_rope_remove_attached(HostOp, reinterpret_cast<size_t>(this), &mS, reinterpret_cast<size_t>(apBody), abRemoveContainerFromBody);
	}

	void iPhysicsRope::SetTotalLength(float afX)
	{
		if(mS.totalLength == afX) return;

		mS.totalLength = afX;

		BuildRopeParticles();
		RefreshParticleCache();
		SetSleeping(false);
	}

	void iPhysicsRope::SetSegmentLength(float afX)
	{
		if(afX<0 || mS.segmentLength == afX) return;

        mS.segmentLength = afX;

		BuildRopeParticles();
		RefreshParticleCache();
		SetSleeping(false);
	}

	void iPhysicsRope::RenderDebug(iLowLevelGraphics *apLowLevel)
	{
		cVector3f vPrevPos =0;
		bool bFirst=true;
		int lCount =0;
		for(tVerletParticleListIt it = mlstParticles.begin(); it != mlstParticles.end(); ++it, ++lCount)
		{
			cVerletParticle *pPart = *it;

			apLowLevel->DrawSphere(pPart->GetPosition(), mS.particleRadius,cColor(1,0,0,1));

			if(bFirst==false)
			{
				apLowLevel->DrawLine(vPrevPos,pPart->GetPosition(),cColor(1,1));
			}

			bFirst=false;
			vPrevPos = pPart->GetPosition();
		}
	}

	// PRIVATE METHODS

	void iPhysicsRope::UpdateBeforeSimulate(float afTimeStep)
	{
		OhplRopeVt vt = MakeVt();
		ohpl_rope_update_before(&vt, &mS, afTimeStep);
	}

	void iPhysicsRope::UpdateAfterSimulate(float afTimeStep)
	{
		OhplRopeVt vt = MakeVt();
		ohpl_rope_update_after(&vt, &mS);
	}

	void iPhysicsRope::UpdateConstraints()
	{
		OhplRopeVt vt = MakeVt();
		ohpl_rope_update_constraints(&vt, &mS);
	}

	void iPhysicsRope::UpdateMotorSound(float fVel)
	{
		// Update sound
		if(mpMotorSoundEntity==NULL && fVel==0) return;

		cWorld *pNormalWorld = mpWorld->GetWorld();
		cSoundHandler *pSoundHandler = pNormalWorld->GetSound()->GetSoundHandler();
		if(pSoundHandler->GetSilent()) return;

		if(mpMotorSoundEntity && pNormalWorld->SoundEntityExists(mpMotorSoundEntity, mlMotorSoundEntityID)==false)
		{
			mpMotorSoundEntity = NULL;
		}

		//Sound is playing. Check if speed is under the stop speed
		if(mpMotorSoundEntity && cMath::Abs(fVel) < mfMotorSoundStopSpeed)
		{
			mpMotorSoundEntity->FadeOut(10.0f);
			mpMotorSoundEntity = NULL;
		}
		//Sound is NOT playing. See if one should be started.
		else if(msMotorSound != "" && mpMotorSoundEntity==NULL && cMath::Abs(fVel) > mfMotorSoundStartSpeed)
		{
			mpMotorSoundEntity = pNormalWorld->CreateSoundEntity(msName+"_MotorSound",msMotorSound,true);
			if(mpMotorSoundEntity)
			{
				mlMotorSoundEntityID = mpMotorSoundEntity->GetCreationID();

				mpMotorSoundEntity->SetIsSaved(false);
				mpMotorSoundEntity->SetPosition(GetStartParticle()->GetPosition());
				mpMotorSoundEntity->FadeIn(5.0f);
			}
		}
	}

	OhplRopeVt iPhysicsRope::MakeVt()
	{
		OhplRopeVt vt = {
			this,
			[](void *c, size_t *apNum) -> cVerletParticle* const* { iPhysicsRope *p = static_cast<iPhysicsRope*>(c); *apNum = p->mvParticleCache.size(); return p->mvParticleCache.data(); },
			[](void *c) { return static_cast<iPhysicsRope*>(c)->CheckSpecificDataSleeping(); },
			[](void *c) { return static_cast<iPhysicsRope*>(c)->CheckSpecificDataAwake(); },
			[](void *c, bool abX) { static_cast<iPhysicsRope*>(c)->SetSleeping(abX); },
			[](void *c, float afX) { static_cast<iPhysicsRope*>(c)->SetTotalLength(afX); },
			[](void *c, float afVel) { static_cast<iPhysicsRope*>(c)->UpdateMotorSound(afVel); },
			[](void *c, OhplV3 aStart, OhplV3 aEnd, OhplV3 *apPos, OhplV3 *apNormal)
			{
				return static_cast<iPhysicsRope*>(c)->CastParticleRay(cVector3f(aStart.x, aStart.y, aStart.z), cVector3f(aEnd.x, aEnd.y, aEnd.z),
					reinterpret_cast<cVector3f*>(apPos), reinterpret_cast<cVector3f*>(apNormal));
			},
			[](size_t b, cMatrixf *apOut) { *apOut = reinterpret_cast<iPhysicsBody*>(b)->GetLocalMatrix(); },
			[](size_t b) { cVector3f v = reinterpret_cast<iPhysicsBody*>(b)->GetLocalPosition(); return OhplV3{v.x, v.y, v.z}; },
			[](size_t b) { cVector3f v = reinterpret_cast<iPhysicsBody*>(b)->GetMassCentre(); return OhplV3{v.x, v.y, v.z}; },
			[](size_t b, cMatrixf *apOut) { *apOut = reinterpret_cast<iPhysicsBody*>(b)->GetInertiaMatrix(); },
			[](size_t b) { return reinterpret_cast<iPhysicsBody*>(b)->GetMass(); },
			[](size_t b, OhplV3 v) { reinterpret_cast<iPhysicsBody*>(b)->AddForce(cVector3f(v.x, v.y, v.z)); },
			[](size_t b, OhplV3 v) { reinterpret_cast<iPhysicsBody*>(b)->AddTorque(cVector3f(v.x, v.y, v.z)); },
			[](void *c, int32_t alIdx, float afP, float afD, OhplV3 aError, float afTimeStep)
			{
				cPidControllerVec3 &pid = static_cast<iPhysicsRope*>(c)->mForcePid[alIdx];
				pid.p = afP;
				pid.i = 0;
				pid.d = afD;
				cVector3f vForce = pid.Output(cVector3f(aError.x, aError.y, aError.z), afTimeStep);
				return OhplV3{vForce.x, vForce.y, vForce.z};
			},
		};
		return vt;
	}

	bool iPhysicsRope::CheckParticleBodyCollision(iPhysicsBody *apBody)
	{
		return ohpl_h14_rope_check_collision(&mS, reinterpret_cast<size_t>(apBody));
	}

	bool iPhysicsRope::CheckSpecificDataSleeping()
	{
		return ohpl_h14_rope_check_sleeping(HostOp, &mS);
	}

	bool iPhysicsRope::CheckSpecificDataAwake()
	{
		return ohpl_h14_rope_check_awake(HostOp, &mS);
	}

	void iPhysicsRope::SetSpecificDataSleeping(bool abSleeping)
	{
		ohpl_h14_rope_set_specific_sleeping(HostOp, &mS, abSleeping);
	}

	void iPhysicsRope::BuildRopeParticles()
	{
		//If not updated, clear data
		if(mS.hasUpdated==false)
		{
			int lRemoveCount = (int)mlstParticles.size() -2;
			tVerletParticleListIt it = mlstParticles.begin(); it++;
			for(; lRemoveCount>0; --lRemoveCount)
			{
				cVerletParticle* pPart = *it;
				it = mlstParticles.erase(it);
				hplDelete(pPart);
			}
		}

		//Set up
		int lParticleNum = (int)mlstParticles.size() - 2;

		//Calculate number of particles needed and update vars
		OhplH14RopePlan plan;
		ohpl_h14_rope_plan(mS.totalLength, mS.segmentLength, &plan);
		int lWantedNum = plan.wanted;
		mS.firstSegmentLength = plan.firstSegment;

		//Log("Wanted num: %d | total len: %f | first set len: %f | seg len: %f \n", lWantedNum, mS.totalLength, mS.firstSegmentLength, mS.segmentLength);

		if(lWantedNum == lParticleNum) return;

		//Remove particles if needed
		if(lWantedNum < lParticleNum)
		{
			tVerletParticleListIt it = mlstParticles.begin();
			it++;
			int lRemoveCount= lParticleNum - lWantedNum;
			for(; lRemoveCount>0; --lRemoveCount)
			{
				cVerletParticle* pPart = *it;
				it = mlstParticles.erase(it);
				hplDelete(pPart);
			}
		}
		//Add particles if needed
		else if(lWantedNum > lParticleNum)
		{
			cVector3f vDir = GetStartDirection();
			int lAddCount= lWantedNum - lParticleNum;

			cVector3f vStartPos = GetStartParticle()->GetPosition();

			float fLength = mS.firstSegmentLength + mS.segmentLength*((float)lAddCount-1);

			tVerletParticleListIt it = mlstParticles.begin();
			it++;

			for(; lAddCount>0; --lAddCount)
			{
				OhplV3 r = ohpl_h14_rope_particle_pos(OhplV3{vStartPos.x, vStartPos.y, vStartPos.z}, OhplV3{vDir.x, vDir.y, vDir.z}, fLength);
				cVector3f vPos(r.x, r.y, r.z);
				//Log("Adding %d pos: %s length: %f\n", lAddCount, vPos.ToString().c_str(), fLength);
				fLength -= mS.segmentLength;

				it = mlstParticles.insert(it, hplNew(cVerletParticle, (this,vPos, 1)) );
			}
		}
	}

	void iPhysicsRope::SetAttachedBody(int alIdx, cVerletParticle *apParticle, iPhysicsBody *apBody)
	{
		ohpl_h14_rope_set_attached(HostOp, reinterpret_cast<size_t>(this), &mS, alIdx, reinterpret_cast<size_t>(apParticle), reinterpret_cast<size_t>(apBody));
	}

	cVector3f iPhysicsRope::GetStartDirection()
	{
		tVerletParticleListIt it = mlstParticles.begin();
		cVerletParticle *pP1 = *it;
		it++;
		cVerletParticle *pP2 = *it;

		return cMath::Vector3Normalize(pP2->GetPosition() - pP1->GetPosition());
	}

}
