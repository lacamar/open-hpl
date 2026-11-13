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

#include <cstddef>
#include "physics/VerletParticle.h"

#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"
#include "impl/RustBackends.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// PARTICLE RAY CALLBACK
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cVerletParticleRayCallback::cVerletParticleRayCallback(iVerletParticleContainer *apContainer)
	{
		mpContainer = apContainer;

		mvIntersectPos =0;
		mvIntersectNormal =0;
	}

	//-----------------------------------------------------------------------

	void cVerletParticleRayCallback::Reset()
	{
		mfClosestDist = 9999999.0f;
		mbIntersected = false;
	}

	//-----------------------------------------------------------------------

	bool cVerletParticleRayCallback::BeforeIntersect(iPhysicsBody *pBody)
	{
		if(pBody->GetCollide()==false) return false;

        return mpContainer->CheckParticleBodyCollision(pBody);
	}

	//-----------------------------------------------------------------------

	bool cVerletParticleRayCallback::OnIntersect(iPhysicsBody *pBody,cPhysicsRayParams *apParams)
	{
		mbIntersected = true;

		if(apParams->mfDist < mfClosestDist)
		{
			mfClosestDist = apParams->mfDist;
			mvIntersectPos = apParams->mvPoint;
			mvIntersectNormal = apParams->mvNormal;
		}
		return true;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PARTICLE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cVerletParticle::cVerletParticle(iVerletParticleContainer *apContainer)
	{
		static_assert(sizeof(cVerletParticle) == 64 && offsetof(cVerletParticle, mvPosition) == 8 && offsetof(cVerletParticle, mvPrevPosition) == 20 &&
			offsetof(cVerletParticle, mvSmoothPosition) == 32 && offsetof(cVerletParticle, mvForce) == 44 && offsetof(cVerletParticle, mfInvMass) == 56, "particle layout");
		mpContainer = apContainer;

		mvPosition = 0;
		mvPrevPosition = 0;
		mfInvMass = 1;
	}

	cVerletParticle::cVerletParticle(iVerletParticleContainer *apContainer, const cVector3f& avPos, float afInvMass)
	{
		mpContainer = apContainer;

		mvPosition = avPos;
		mvPrevPosition = avPos;
		mfInvMass = afInvMass;
	}

	cVerletParticle::~cVerletParticle()
	{

	}

	//-----------------------------------------------------------------------

	void cVerletParticle::SetPosition(const cVector3f& avPos, bool abSetPrevPos)
	{
		mvPosition = avPos;
		if(abSetPrevPos) mvPrevPosition = mvPosition;

		mpContainer->SetSleeping(false);
	}

	//-----------------------------------------------------------------------



	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PARTICLE CONTAINER
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iVerletParticleContainer::iVerletParticleContainer(const tString &asName, iPhysicsWorld *apWorld)
	{
		msName = asName;

		mpWorld = apWorld;

		mpRayParticleCallback = hplNew( cVerletParticleRayCallback, (this));

		if(OhplBackendRustRequested()) return;

		mlUniqueID = -1;

		mS.gravityForce = mpWorld->GetGravity();

		mS.collide = true;

		mfDamping = 0.001f;
		mS.dampingMul = 1 - mfDamping;

		mS.slideAmount = 0.3f;
		mS.particleRadius = 0.05f;

		mS.sleeping = false;
		mS.sleepCheckCount =0;
		mS.sleepCheckTime = 1.0f / 3.0f;
		mS.sleepCount =0;
		mS.sleepMaxCount = 3;
		mS.sleepCheckSqrLimit = 0.0015f * 0.0015f;

		mS.updateCount =0;
	}

	iVerletParticleContainer::~iVerletParticleContainer()
	{

	}

	//-----------------------------------------------------------------------

	void iVerletParticleContainer::SetDamping(float afX)
	{
		afX = cMath::Clamp(afX,0,1);

		mfDamping = afX;
		mS.dampingMul = 1 - mfDamping;
	}


	//-----------------------------------------------------------------------

	void iVerletParticleContainer::SetParticleRadius(float afX)
	{
		if(mS.particleRadius == afX) return;

		mS.particleRadius = afX;

		SetSleeping(false);
	}

	//-----------------------------------------------------------------------

	void iVerletParticleContainer::SetSleeping(bool abX)
	{
		if(mS.sleeping == abX) return;

		mS.sleeping = abX;
		mS.sleepCount = 0;
        mS.sleepCheckCount = 0;

		SetSpecificDataSleeping(abX);
	}

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

	void cVerletParticle::UpdateMovement(float afTimeStep)
	{
		ohpl_verlet_update_movement(this, mpContainer->mS.dampingMul, OhplV3{mpContainer->mS.gravityForce.x, mpContainer->mS.gravityForce.y, mpContainer->mS.gravityForce.z}, afTimeStep);
	}

	//-----------------------------------------------------------------------

	void iVerletParticleContainer::UpdateLengthConstraint(cVerletParticle *apP1, cVerletParticle *apP2, float afLength, float afStiffness)
	{
		ohpl_verlet_length_constraint(apP1, apP2, afLength, afStiffness);
	}

	void iVerletParticleContainer::UpdateLengthConstraint(cVerletParticle *apP1, cVerletParticle *apP2, float afMinLength, float afMaxLength, float afStiffness)
	{
		ohpl_verlet_length_constraint_range(apP1, apP2, afMinLength, afMaxLength, afStiffness);
	}

	void iVerletParticleContainer::UpdateLengthConstraintStretch(cVerletParticle *apP1, cVerletParticle *apP2, float afLength, float afStiffness)
	{
		ohpl_verlet_length_constraint_stretch(apP1, apP2, afLength, afStiffness);
	}

	//-----------------------------------------------------------------------

	bool iVerletParticleContainer::CastParticleRay(const cVector3f &avStart, const cVector3f &avEnd, cVector3f *apPos, cVector3f *apNormal)
	{
		mpRayParticleCallback->Reset();
		mpWorld->CastRay(mpRayParticleCallback, avStart, avEnd, true,true,true, true);
		*apPos = mpRayParticleCallback->mvIntersectPos;
		*apNormal = mpRayParticleCallback->mvIntersectNormal;
		return mpRayParticleCallback->mbIntersected;
	}

	void iVerletParticleContainer::UpdateParticleCollisionConstraint(cVerletParticle *apPart, const cVector3f &avPrevPos, float afRadius)
	{
		ohpl_verlet_collision_constraint(
			[](void *c, OhplV3 aStart, OhplV3 aEnd, OhplV3 *apPos, OhplV3 *apNormal)
			{
				return static_cast<iVerletParticleContainer*>(c)->CastParticleRay(cVector3f(aStart.x, aStart.y, aStart.z), cVector3f(aEnd.x, aEnd.y, aEnd.z),
					reinterpret_cast<cVector3f*>(apPos), reinterpret_cast<cVector3f*>(apNormal));
			},
			this, &mS, apPart, OhplV3{avPrevPos.x, avPrevPos.y, avPrevPos.z}, afRadius);
	}

	//-----------------------------------------------------------------------

	cVerletParticle* iVerletParticleContainer::GetParticle(int alIdx)
	{
		// ponytail: O(n) list walk, vector storage if ropes get long
		if(alIdx<0 || alIdx>=(int)mlstParticles.size()) return NULL;
		return *std::next(mlstParticles.begin(), alIdx);
	}




	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

}
