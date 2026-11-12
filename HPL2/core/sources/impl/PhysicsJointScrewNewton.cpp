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

#include "impl/PhysicsJointScrewNewton.h"

#include "impl/PhysicsBodyNewton.h"
#include "impl/PhysicsWorldNewton.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cPhysicsJointScrewNewton::cPhysicsJointScrewNewton(const tString &asName, 
		iPhysicsBody *apParentBody, iPhysicsBody *apChildBody, 
		iPhysicsWorld *apWorld,const cVector3f &avPivotPoint, const cVector3f& avPinDir)
		: iPhysicsJointNewton<iPhysicsJointScrew>(asName,apParentBody,apChildBody,apWorld,avPivotPoint,avPinDir)
	{
		mvPin = avPinDir;
		mvPin.Normalize();

		if(OhplL4UseRust())
		{
			mhRust = ohpl_ntj_create_screw(mhRustWorld, &avPivotPoint, &avPinDir, mpNewtonChildBody, mpNewtonParentBody,
										(size_t)static_cast<iPhysicsJointNewton<iPhysicsJointScrew>*>(this), RustOps(), &mpNewtonJoint);
		}
		else
		{
		mpNewtonJoint = NewtonConstraintCreateCorkscrew(mpNewtonWorld, avPivotPoint.v, avPinDir.v, mpNewtonChildBody,
			mpNewtonParentBody);

		NewtonJointSetUserData(mpNewtonJoint, (void*) this);
		NewtonCorkscrewSetUserCallback(mpNewtonJoint,LimitCallback);
		}

		mfMaxDistance =0;
		mfMinDistance =0;

		mfPreviousDist =0;

		mvPinDir = avPinDir;
		mvPivotPoint = avPivotPoint;
	}

	//-----------------------------------------------------------------------

	cPhysicsJointScrewNewton::~cPhysicsJointScrewNewton()
	{

	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cPhysicsJointScrewNewton::SetMaxDistance(float afX)
	{
		if(OhplL4UseRust())
		{
			mfMaxDistance = afX;
			ohpl_ntj_set_max(mhRust, afX);
			return;
		}
		mfMaxDistance = afX;
	}
	void cPhysicsJointScrewNewton::SetMinDistance(float afX)
	{
		if(OhplL4UseRust())
		{
			mfMinDistance = afX;
			ohpl_ntj_set_min(mhRust, afX);
			return;
		}
		mfMinDistance = afX;
	}
	float cPhysicsJointScrewNewton::GetMaxDistance()
	{
		if(OhplL4UseRust())
		{
			return ohpl_ntj_max(mhRust);
		}
		return mfMaxDistance;
	}
	float cPhysicsJointScrewNewton::GetMinDistance()
	{
		if(OhplL4UseRust())
		{
			return ohpl_ntj_min(mhRust);
		}
		return mfMinDistance;
	}
	
	//-----------------------------------------------------------------------

	cVector3f cPhysicsJointScrewNewton::GetVelocity()
	{
		if(OhplL4UseRust())
		{
			float fSpeed = ohpl_ntj_axis(mhRust, 1);
			return mvPin * fSpeed;
		}
		float fSpeed = NewtonCorkscrewGetJointVeloc(mpNewtonJoint);
		return mvPin * fSpeed;
	}
	cVector3f cPhysicsJointScrewNewton::GetAngularVelocity()
	{
		return cVector3f(0,0,0);
	}
	float cPhysicsJointScrewNewton::GetForceSize()
	{
		if(OhplL4UseRust())
		{
			cVector3f vForce;
			ohpl_ntj_force(mhRust, &vForce);
			return vForce.Length();
		}
		cVector3f vForce;
		NewtonCorkscrewGetJointForce(mpNewtonJoint,&vForce.v[0]);
		return vForce.Length();
	}
	//-----------------------------------------------------------------------
	
	float cPhysicsJointScrewNewton::GetDistance()
	{
		if(OhplL4UseRust())
		{
			return ohpl_ntj_axis(mhRust, 0);
		}
		return NewtonCorkscrewGetJointPosit(mpNewtonJoint);
	}
	float cPhysicsJointScrewNewton::GetAngle()
	{
		if(OhplL4UseRust())
		{
			return ohpl_ntj_axis(mhRust, 2);
		}
		return NewtonCorkscrewGetJointAngle(mpNewtonJoint);
	}

	//-----------------------------------------------------------------------

	
	//////////////////////////////////////////////////////////////////////////
	// STATIC CALLBACKS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	unsigned cPhysicsJointScrewNewton::LimitCallback(const NewtonJoint* pScrew, NewtonHingeSliderUpdateDesc* pDesc)
	{
		cPhysicsJointScrewNewton* pScrewJoint = (cPhysicsJointScrewNewton*)NewtonJointGetUserData(pScrew);
		
		//pScrewJoint->OnPhysicsUpdate();

		float fDistance = NewtonCorkscrewGetJointPosit (pScrew);
		//Log("Dist: %f\n",fDistance);

		if(pScrewJoint->mfMinDistance == 0 && pScrewJoint->mfMaxDistance == 0) return 0;

		//Avoid oscillation
		CheckLimitAutoSleep(pScrewJoint, pScrewJoint->mfMinDistance,pScrewJoint->mfMaxDistance,fDistance);

		if (fDistance < pScrewJoint->mfMinDistance)
		{
			pScrewJoint->OnMinLimit();

			pDesc->m_accel = NewtonCorkscrewCalculateStopAccel (pScrew, pDesc, pScrewJoint->mfMinDistance);
			pDesc->m_minFriction =0;
			return 1;
		} 
		else if (fDistance > pScrewJoint->mfMaxDistance)
		{
			pScrewJoint->OnMaxLimit();

			pDesc->m_accel = NewtonCorkscrewCalculateStopAccel (pScrew, pDesc, pScrewJoint->mfMaxDistance);
			pDesc->m_maxFriction =0;
			return 1;
		}
		else
		{
			if(pScrewJoint->mpParentBody ==NULL || pScrewJoint->mpParentBody->GetMass()==0)
			{
				if( (pScrewJoint->mbStickyMaxLimit && pScrewJoint->mfPreviousDist > pScrewJoint->mfMaxDistance) || 
					(pScrewJoint->mbStickyMinLimit && pScrewJoint->mfPreviousDist < pScrewJoint->mfMinDistance) )
				{
					pScrewJoint->mpChildBody->SetAngularVelocity(0);
					pScrewJoint->mpChildBody->SetLinearVelocity(0);
				}
			}

			pScrewJoint->OnNoLimit();
		}

		pScrewJoint->mfPreviousDist = fDistance;

		return 0;
	}
	//-----------------------------------------------------------------------

}
