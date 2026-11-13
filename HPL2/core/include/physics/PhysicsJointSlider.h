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

	//-----------------------------------

	class iPhysicsJointSlider : public iPhysicsJoint
	{
	#ifdef __GNUC__
		typedef iPhysicsJoint __super;
	#endif
	public:
		iPhysicsJointSlider(const tString &asName, iPhysicsBody *apParentBody, iPhysicsBody *apChildBody,
			iPhysicsWorld *apWorld,const cVector3f &avPivotPoint,const cVector3f &avPinDir);
		~iPhysicsJointSlider();
		/**
		 * Set the maximum distance the bodies can be from each other, relative to the start dist between them
		 * This is true if pin points towards the child.
		 * In other words, distance increases as the distance between start pivot and current pivot
		 * increases in the opposite direction of the pin.
		*/
		/**
		* Set the minimum distance the bodies can be from each other, relative to the start dist between them
		* This is true if pin points towards the child.
		*/


		void SetMaxDistance(float afX);
		void SetMinDistance(float afX);
		float GetMaxDistance();
		float GetMinDistance();

		cVector3f GetVelocity();
		cVector3f GetAngularVelocity();
		float GetForceSize();

		float GetDistance();
		float GetAngle();

		ePhysicsJointType GetType(){ return ePhysicsJointType_Slider;}

	protected:
		float mfPreviousDist;

		float mfMaxDistance;
		float mfMinDistance;
		cVector3f mvPin;
	};
};
#endif // HPL_PHYSICS_JOINT_SLIDER_H
