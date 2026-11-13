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

#include "impl/PhysicsJointSliderNewton.h"

#include "impl/PhysicsBodyNewton.h"
#include "impl/PhysicsWorldNewton.h"

#include "system/LowLevelSystem.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cPhysicsJointSliderNewton::cPhysicsJointSliderNewton(const tString &asName, 
		iPhysicsBody *apParentBody, iPhysicsBody *apChildBody, 
		iPhysicsWorld *apWorld,const cVector3f &avPivotPoint, const cVector3f& avPinDir)
		: iPhysicsJointNewton<iPhysicsJointSlider>(asName,apParentBody,apChildBody,apWorld,avPivotPoint,avPinDir)
	{
		mhRust = ohpl_ntj_create_slider(mhRustWorld, &avPivotPoint, &avPinDir, mpNewtonChildBody, mpNewtonParentBody,
									(size_t)static_cast<iPhysicsJointNewton<iPhysicsJointSlider>*>(this), RustOps(), &mpNewtonJoint);

		mfMaxDistance =0;
		mfMinDistance =0;

		mfPreviousDist =0;
	}

	//-----------------------------------------------------------------------

	cPhysicsJointSliderNewton::~cPhysicsJointSliderNewton()
	{

	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cPhysicsJointSliderNewton::SetMaxDistance(float afX)
	{
		mfMaxDistance = afX;
		ohpl_ntj_set_max(mhRust, afX);
	}
	void cPhysicsJointSliderNewton::SetMinDistance(float afX)
	{
		mfMinDistance = afX;
		ohpl_ntj_set_min(mhRust, afX);
	}
	float cPhysicsJointSliderNewton::GetMaxDistance()
	{
		return ohpl_ntj_max(mhRust);
	}
	float cPhysicsJointSliderNewton::GetMinDistance()
	{
		return ohpl_ntj_min(mhRust);
	}
	
	//-----------------------------------------------------------------------

	cVector3f cPhysicsJointSliderNewton::GetVelocity()
	{
		float fSpeed = ohpl_ntj_axis(mhRust, 1);
		return GetPinDir() * fSpeed;
	}
	cVector3f cPhysicsJointSliderNewton::GetAngularVelocity()
	{
		return cVector3f(0,0,0);
	}
	float cPhysicsJointSliderNewton::GetForceSize()
	{
		cVector3f vForce;
		ohpl_ntj_force(mhRust, &vForce);
		return vForce.Length();
	}


	//-----------------------------------------------------------------------

	float cPhysicsJointSliderNewton::GetDistance()
	{
		return ohpl_ntj_axis(mhRust, 0);
	}
	float cPhysicsJointSliderNewton::GetAngle()
	{
		return 0;
	}
	
	//-----------------------------------------------------------------------
	
}
