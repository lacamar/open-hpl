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

#ifndef HPL_PHYSICS_ROPE_H
#define HPL_PHYSICS_ROPE_H

#include "physics/PhysicsTypes.h"
#include "system/SystemTypes.h"
#include "math/MathTypes.h"
#include "math/PidController.h"

#include "physics/VerletParticle.h"

namespace hpl {

	class iLowLevelGraphics;

	class iPhysicsWorld;
	class iPhysicsBody;
	class iCollideShape;
	class cVerletParticle;
		
	class iPhysicsRope;

	class cSoundEntity;
	
	//------------------------------------------
	
	typedef OhplRopeAttachment cPhysicsRopeAttachment;

	//------------------------------------------

	class iPhysicsRope : public iVerletParticleContainer
	{
	public:
		iPhysicsRope(const tString &asName, iPhysicsWorld *apWorld, const cVector3f &avStartPos, const cVector3f &avEndPos);
		virtual ~iPhysicsRope();

		/////////////////////////////
		// General
		void UpdateBeforeSimulate(float afTimeStep);
		void UpdateAfterSimulate(float afTimeStep);

		void RemoveAttachedBody(iPhysicsBody *apBody, bool abRemoveContainerFromBody=true);

        
		/////////////////////////////
		// Motor
		void SetPhysicsActive(bool abX){ mS.physicsActive = abX; }
		bool GetPhysicsActive(){ return mS.physicsActive; }

        void SetMotorActive(bool abX);
		bool GetMotorActive(){ return mS.motorActive; }

		void SetMotorWantedLength(float afX){ mS.motorWantedLength = afX; }
		void SetMotorSpeedMul(float afX){ mS.motorSpeedMul = afX; }
		void SetMotorMaxSpeed(float afX){ mS.motorMaxSpeed = afX; }
		void SetMotorMinSpeed(float afX){ mS.motorMinSpeed = afX; }

		float GetMotorWantedLength(){ return mS.motorWantedLength;}
		float GetMotorSpeedMul(){ return mS.motorSpeedMul;}
		float GetMotorMaxSpeed(){ return mS.motorMaxSpeed;}
		float GetMotorMinSpeed(){ return mS.motorMinSpeed;}

		void SetMotorSound(const tString& asSound) { msMotorSound = asSound; }
		void SetMotorSoundStartSpeed(float afX) { mfMotorSoundStartSpeed = afX; }
		void SetMotorSoundStopSpeed(float afX) { mfMotorSoundStopSpeed = afX; }

		const tString& GetMotorSound() { return msMotorSound;}
		float GetMotorSoundStartSpeed() { return mfMotorSoundStartSpeed;}
		float GetMotorSoundStopSpeed() { return mfMotorSoundStopSpeed;}


		/////////////////////////////
		// Auto Move
		void SetAutoMoveActive(bool abX);
		void SetAutoMoveAcc(float afX){ mS.autoMoveAcc = afX;}
		void SetAutoMoveMaxSpeed(float afX){ mS.autoMoveMaxSpeed = afX;}
		void SetAutoMoveSpeed(float afX){ mS.autoMoveSpeed = afX;}

		bool GetAutoMoveActive();
		float GetAutoMoveAcc(){ return mS.autoMoveAcc;}
		float GetAutoMoveMaxSpeed(){ return mS.autoMoveMaxSpeed;}
		float GetAutoMoveSpeed(){ return mS.autoMoveSpeed;}
        
		/////////////////////////////
		// Properties
		cVerletParticle* GetStartParticle(){ return mlstParticles.front();}
		cVerletParticle* GetEndParticle(){ return mlstParticles.back();}

		void SetAttachedStartBody(iPhysicsBody *apBody);
		void SetAttachedEndBody(iPhysicsBody *apBody);

		iPhysicsBody* GetAttachedStartBody(){ return mS.attached[0].mpBody;}
		iPhysicsBody* GetAttachedEndBody(){ return mS.attached[1].mpBody;}

		cPhysicsRopeAttachment* GetAttachment(int alIdx){ return &mS.attached[alIdx]; }
		
		void SetTotalLength(float afX);
		float GetTotalLength(){ return mS.totalLength;}

		void SetMinTotalLength(float afX);
		void SetMaxTotalLength(float afX);
		float GetMinTotalLength(){ return mS.minTotalLength;}
		float GetMaxTotalLength(){ return mS.maxTotalLength;}

		void SetSegmentLength(float afX);
		float GetSegmentLength(){ return mS.segmentLength;}
		float GetFirstSegmentLength(){ return mS.firstSegmentLength;}

		void SetMaxIterations(int alX){ mS.maxIterations = alX;}
		int GetMaxIterations(){ return mS.maxIterations;}

		void SetStrength(float afX){ mS.strength  = afX;}
		float GetStrength(){ return mS.strength;}

		void SetStiffness(float afX){ mS.stiffness  = afX;}
		float GetStiffness(){ return mS.stiffness;}

		void SetCollideAttachments(bool abX){ mS.collideAttachments  = abX;}
		bool GetCollideAttachments(){ return mS.collideAttachments;}

		/////////////////////////////
		// Debug
		void UpdateConstraints();

		void RenderDebug(iLowLevelGraphics *apLowLevel);

	protected:
		bool CheckParticleBodyCollision(iPhysicsBody *apBody);
		bool CheckSpecificDataSleeping();
		bool CheckSpecificDataAwake();
		void SetSpecificDataSleeping(bool abSleeping);
		
		void UpdateMotor(float afTimeStep);
		
		
		void UpdateMotorSound(float afVel);
		OhplRopeVt MakeVt();

		void BuildRopeParticles();

		void SetAttachedBody(int alIdx, cVerletParticle *apParticle, iPhysicsBody *apBody);
		
		cVector3f GetStartDirection();

		cPidControllerVec3 mForcePid[2];



		int mlUniqueID;


		tString msMotorSound;
		float mfMotorSoundStartSpeed;
		float mfMotorSoundStopSpeed;
		cSoundEntity *mpMotorSoundEntity;
		int mlMotorSoundEntityID;





		
	};
};
#endif // HPL_PHYSICS_ROPE_H
