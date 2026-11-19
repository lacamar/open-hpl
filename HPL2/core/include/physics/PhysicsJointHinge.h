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

#ifndef HPL_PHYSICS_JOINT_HINGE_H
#define HPL_PHYSICS_JOINT_HINGE_H

#include "physics/PhysicsJoint.h"

namespace hpl {

	class iPhysicsJointHinge : public iPhysicsJoint
	{
	public:
		iPhysicsJointHinge(const tString &asName, iPhysicsBody *apParentBody, iPhysicsBody *apChildBody,
			iPhysicsWorld *apWorld,const cVector3f &avPivotPoint,const cVector3f &avPinDir);
		~iPhysicsJointHinge();

		void SetMaxAngle(float afAngle){ ohpl_ntj_set_max(mhRust, afAngle);}
		void SetMinAngle(float afAngle){ ohpl_ntj_set_min(mhRust, afAngle);}
		float GetMaxAngle(){ return ohpl_ntj_max(mhRust);}
		float GetMinAngle(){ return ohpl_ntj_min(mhRust);}

		cVector3f GetVelocity(){ return cVector3f(0,0,0);}
		cVector3f GetAngularVelocity();
		float GetForceSize(){ return ohpl_ntj_hinge_force_size(mhRust);}

		float GetDistance(){ return 0;}
		float GetAngle(){ return ohpl_ntj_hinge_angle(mhRust);}

		ePhysicsJointType GetType(){ return ePhysicsJointType_Hinge;}

	protected:
		void ResetPosition();
	};
};
#endif // HPL_PHYSICS_JOINT_HINGE_H
