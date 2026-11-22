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
#include "physics/RustPhysicsH14.h"

#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"
#include "impl/RustBackends.h"

namespace hpl {

	cVerletParticleRayCallback::cVerletParticleRayCallback(iVerletParticleContainer *apContainer)
	{
		static_assert(offsetof(cVerletParticleRayCallback, mbIntersected) == 16 && offsetof(cVerletParticleRayCallback, mvIntersectPos) == 20 &&
			offsetof(cVerletParticleRayCallback, mvIntersectNormal) == 32 && offsetof(cVerletParticleRayCallback, mfClosestDist) == 44 &&
			offsetof(cPhysicsRayParams, mfDist) == 4 && offsetof(cPhysicsRayParams, mvNormal) == 8 && offsetof(cPhysicsRayParams, mvPoint) == 20, "callback layout");
		mpContainer = apContainer;

		mvIntersectPos =0;
		mvIntersectNormal =0;
	}

	bool cVerletParticleRayCallback::BeforeIntersect(iPhysicsBody *pBody)
	{
		if(pBody->GetCollide()==false) return false;

        return mpContainer->CheckParticleBodyCollision(pBody);
	}

	// PARTICLE

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

	void cVerletParticle::SetPosition(const cVector3f& avPos, bool abSetPrevPos)
	{
		mvPosition = avPos;
		if(abSetPrevPos) mvPrevPosition = mvPosition;

		mpContainer->SetSleeping(false);
	}

	iVerletParticleContainer::iVerletParticleContainer(const tString &asName, iPhysicsWorld *apWorld)
	{
		static_assert(offsetof(iVerletParticleContainer, mS) == 8 && offsetof(iVerletParticleContainer, mlstParticles) == 96 &&
			offsetof(iVerletParticleContainer, mfDamping) == 124, "container layout");
		mS = ohpl_h14_rope_alloc();
		msName = asName;

		mpWorld = apWorld;

		mpRayParticleCallback = hplNew( cVerletParticleRayCallback, (this));
	}

	iVerletParticleContainer::~iVerletParticleContainer()
	{
		ohpl_h14_rope_release(mS);

	}

	void iVerletParticleContainer::SetParticleRadius(float afX)
	{
		if(mS->particleRadius == afX) return;

		mS->particleRadius = afX;

		SetSleeping(false);
	}

	void iVerletParticleContainer::SetSleeping(bool abX)
	{
		if(mS->sleeping == abX) return;

		mS->sleeping = abX;
		mS->sleepCount = 0;
        mS->sleepCheckCount = 0;

		SetSpecificDataSleeping(abX);
	}

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
			this, mS, apPart, OhplV3{avPrevPos.x, avPrevPos.y, avPrevPos.z}, afRadius);
	}

}
