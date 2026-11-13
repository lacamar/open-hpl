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

#include "physics/PhysicsJointBall.h"

#include "physics/PhysicsBody.h"
#include "physics/PhysicsWorld.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iPhysicsJointBall::iPhysicsJointBall(const tString &asName,
						iPhysicsBody *apParentBody, iPhysicsBody *apChildBody,
						iPhysicsWorld *apWorld, const cVector3f &avPivotPoint, const cVector3f &avPinDir)
	: iPhysicsJoint(asName,apParentBody,apChildBody,apWorld,avPivotPoint,avPinDir)
	{
		mhRust = ohpl_ntj_create_ball(mhRustWorld, &avPivotPoint, mpNewtonChildBody, mpNewtonParentBody, &mpNewtonJoint);

		mfMaxConeAngle =0;
		mfMaxTwistAngle = 0;
	}

	//-----------------------------------------------------------------------

	iPhysicsJointBall::~iPhysicsJointBall()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iPhysicsJointBall::SetConeLimits(float afMaxConeAngle, float afMaxTwistAngle)
	{
		ohpl_ntj_ball_cone_limits(mhRust, &mvStartPinDir, afMaxConeAngle, afMaxTwistAngle);
		mfMaxConeAngle = afMaxConeAngle;
		mfMaxTwistAngle = afMaxTwistAngle;
	}

	cVector3f iPhysicsJointBall::GetAngles()
	{
		cVector3f vAngles;
		ohpl_ntj_ball_angles(mhRust, &vAngles);
		return vAngles;
	}

	//-----------------------------------------------------------------------

	cVector3f iPhysicsJointBall::GetVelocity()
	{
		return cVector3f(0,0,0);
	}
	cVector3f iPhysicsJointBall::GetAngularVelocity()
	{
		cVector3f vVel;
		ohpl_ntj_ball_omega(mhRust, &vVel);
		return vVel;
	}
	float iPhysicsJointBall::GetForceSize()
	{
		cVector3f vForce;
		ohpl_ntj_ball_force(mhRust, &vForce);
		return vForce.Length();
	}

	//-----------------------------------------------------------------------

	float iPhysicsJointBall::GetDistance()
	{
		return 0;
	}
	float iPhysicsJointBall::GetAngle()
	{
		return 0;
	}

	//-----------------------------------------------------------------------

}
