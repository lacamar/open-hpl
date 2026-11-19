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

#ifndef HPL_PHYSICS_JOINT_SLIDER_H
#define HPL_PHYSICS_JOINT_SLIDER_H

#include "physics/PhysicsJoint.h"

namespace hpl {

	class iPhysicsJointSlider : public iPhysicsJoint
	{
	public:
		iPhysicsJointSlider(const tString &asName, iPhysicsBody *apParentBody, iPhysicsBody *apChildBody,
			iPhysicsWorld *apWorld,const cVector3f &avPivotPoint,const cVector3f &avPinDir);
		~iPhysicsJointSlider();

		void SetMaxDistance(float afX){ ohpl_ntj_set_max(mhRust, afX);}
		void SetMinDistance(float afX){ ohpl_ntj_set_min(mhRust, afX);}
		float GetMaxDistance(){ return ohpl_ntj_max(mhRust);}
		float GetMinDistance(){ return ohpl_ntj_min(mhRust);}

		cVector3f GetVelocity(){ cVector3f vVel; ohpl_ntj_pin_velocity(mhRust, &vVel); return vVel;}
		cVector3f GetAngularVelocity(){ return cVector3f(0,0,0);}
		float GetForceSize(){ cVector3f vForce; ohpl_ntj_force(mhRust, &vForce); return vForce.Length();}

		float GetDistance(){ return ohpl_ntj_axis(mhRust, 0);}
		float GetAngle(){ return 0;}

		ePhysicsJointType GetType(){ return ePhysicsJointType_Slider;}
	};
};
#endif // HPL_PHYSICS_JOINT_SLIDER_H
