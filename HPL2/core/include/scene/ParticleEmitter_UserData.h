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

#ifndef HPL_PARTICLE_EMITTER_USER_DATA_H
#define HPL_PARTICLE_EMITTER_USER_DATA_H

#include "scene/ParticleEmitter.h"
#include "physics/PhysicsTypes.h"


namespace hpl {

	class cXmlElement;

	enum ePEStartPosType
	{
		ePEStartPosType_Box,
		ePEStartPosType_Sphere,
		ePEStartPosType_LastEnum,
	};
	
	enum ePEDeathType
	{
		ePEDeathType_Age,
		ePEDeathType_Color,
		ePEDeathType_LastEnum,
	};

	enum ePEGravityType
	{
		ePEGravityType_None,		//No gravity added
		ePEGravityType_Vector,		//Vector is used.
		ePEGravityType_Center,		//Y axis used. heading equals center.
		ePEGravityType_LastEnum,
	};

	enum ePESubDivType
	{
		ePESubDivType_Random,
		ePESubDivType_Animation,
		ePESubDivType_LastEnum,
	};

	enum ePEPartSpinType
	{
		ePEPartSpinType_Constant,
		ePEPartSpinType_Movement,
		ePEPartSpinType_LastEnum,
	};

	
	//------------------------------------

	class iPhysicsWorld;

	//------------------------------------

	class cParticleEmitterData_UserData : public iParticleEmitterData, public iPhysicsRayCallback
	{
	public:
    	cParticleEmitterData_UserData(const tString &asName,cResources* apResources,cGraphics *apGraphics);
		~cParticleEmitterData_UserData(){ ohpl_h12_ps_free(mpDesc); }

		iParticleEmitter* Create(tString asName, cVector3f avSize);

		void LoadFromElement(cXmlElement *apElement, bool abLoadMaterials=true);

		bool CheckCollision(const cVector3f& avStart, const cVector3f &avEnd,
									iPhysicsWorld *apPhysicsWorld,
									cVector3f *apNormalVec,
									cVector3f *apPosVec);

	private:
		friend struct OhplH12PsProbe;
		bool OnIntersect(iPhysicsBody *pBody,cPhysicsRayParams *apParams);

		const OhplH12PsDesc& D() const { return *ohpl_h12_ps_desc(mpDesc); }

		OhplH12PsBox *mpDesc;

		float mfShortestDist;
		bool mbIntersected;
		cVector3f *mpIntersectNormal;
		cVector3f *mpIntersectPos;
	};
	
	//----------------------------------------------------------

};

#endif // HPL_PARTICLE_EMITTER_USER_DATA_H

