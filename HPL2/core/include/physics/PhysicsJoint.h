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

#ifndef HPL_PHYSICS_JOINT_H
#define HPL_PHYSICS_JOINT_H

#include <map>
#include "system/SystemTypes.h"
#include "math/MathTypes.h"

#include "engine/SaveGame.h"

#include "physics/PhysicsController.h"
#include "physics/RustPhysBook.h"
#include "impl/RustNewtonBackend.h"

namespace hpl {

	class iPhysicsBody;
	class iPhysicsWorld;
	class cSoundEntity;
	class iPhysicsJoint;
	class iPhysicsController;

	typedef std::map<tString, iPhysicsController*> tPhysicsControllerMap;
	typedef tPhysicsControllerMap::iterator tPhysicsControllerMapIt;

	typedef cSTLMapIterator<iPhysicsController*, tPhysicsControllerMap, tPhysicsControllerMapIt> cPhysicsControllerIterator;

	enum ePhysicsJointType
	{
		ePhysicsJointType_Ball,
		ePhysicsJointType_Hinge,
		ePhysicsJointType_Slider,
		ePhysicsJointType_Screw,
		ePhysicsJointType_LastEnum
	};

	enum ePhysicsJointSpeed
	{
		ePhysicsJointSpeed_Linear,
		ePhysicsJointSpeed_Angular,
		ePhysicsJointSpeed_LastEnum
	};

	class iPhysicsBody;
	class iPhysicsWorld;
	class cSoundEntity;
	class iPhysicsJointCallback;

	// Scalar and pointer state of iPhysicsJoint, owned by rust/src/physics/shell.rs; the members are references into it.
	struct OhplJointShellState
	{
		uint64_t shell;
		iPhysicsBody *parent;
		iPhysicsBody *child;
		iPhysicsWorld *world;
		cSoundEntity *sound;
		iPhysicsJointCallback *callback;
		void *userData;
		cMatrixf parentSetup, childSetup, prevChild, prevParent;
		cVector3f pinDir, pivot, startPivot, localPivot, localPinDir, startPinDir;
		float minMoveSpeed, minMoveFreq, minMoveFreqSpeed, minMoveVolume, maxMoveFreq, maxMoveFreqSpeed, maxMoveVolume, middleMoveSpeed, middleMoveVolume;
		float breakForce, limitAutoSleepDist;
		int32_t uniqueId, soundId, limitAutoSleepNumSteps, speedCount, limitStepCount;
		int32_t moveSpeedType;
		bool stickyMin, stickyMax, allowPositionReset, positionResetPending, breakable, broken, limitAutoSleep, hasCollided, autoDeleteCallback;
	};
	static_assert(sizeof(OhplJointShellState) == 464, "joint state layout");

	class cJointLimitEffect : public iSerializable
	{
		kSerializableClassInit(cJointLimitEffect)
	public:
		tString msSound;
		float mfMinSpeed;
		float mfMaxSpeed;
	};

	class iPhysicsJointCallback
	{
	public:
		virtual ~iPhysicsJointCallback(){}

		virtual void OnMinLimit(iPhysicsJoint *apJoint)=0;
		virtual void OnMaxLimit(iPhysicsJoint *apJoint)=0;

		//Ugly trick to support joint script callback.
		virtual bool IsScript(){ return false;}
	};

	class iPhysicsJoint
	{
	friend struct cRustPhysicsAccess;
	#ifdef __GNUC__
		typedef iSaveObject __super;
	#endif
	public:
		iPhysicsJoint(	const tString &asName, iPhysicsBody *apParentBody, iPhysicsBody *apChildBody,
						iPhysicsWorld *apWorld,const cVector3f &avPivotPoint, const cVector3f &avPinDir);
		virtual ~iPhysicsJoint();

		const tString& GetName(){ return msName;}

		void SetUniqueID(int alID){mS->uniqueId = alID;}
		int GetUniqueID(){return mS->uniqueId;}

		iPhysicsBody * GetParentBody(){ return mS->parent;}
		iPhysicsBody * GetChildBody(){ return mS->child;}

		void RemoveBody(iPhysicsBody *apBody){ if(mS->parent == apBody) mS->parent = NULL; if(mS->child == apBody) mS->child = NULL;}

		const cVector3f& GetPivotPoint(){ return mS->pivot;}
		const cVector3f& GetPinDir(){ return mS->pinDir;}

		virtual ePhysicsJointType GetType()=0;

		void SetCollideBodies(bool abX){ ohpl_ntj_set_collide(mhRust, abX);}
		bool GetCollideBodies(){ return ohpl_ntj_collide(mhRust);}

		void SetStiffness(float afX){ ohpl_ntj_set_stiffness(mhRust, afX);}
		float GetStiffness(){ return ohpl_ntj_stiffness(mhRust);}

		virtual cVector3f GetVelocity()=0;
		virtual cVector3f GetAngularVelocity()=0;
		virtual float GetForceSize()=0;

		virtual float GetDistance()=0;
		virtual float GetAngle()=0;

		cJointLimitEffect* GetMaxLimit(){ return &mMaxLimit;}
		cJointLimitEffect* GetMinLimit(){ return &mMinLimit;}

		void SetMoveSound(tString &asName) {msMoveSound = asName;}

		void SetMoveSpeedType(ePhysicsJointSpeed aType){ mS->moveSpeedType = aType;}
		void SetMinMoveSpeed(float afX){mS->minMoveSpeed = afX;}
		void SetMinMoveFreq(float afX){mS->minMoveFreq = afX; }
		void SetMinMoveFreqSpeed(float afX){mS->minMoveFreqSpeed = afX; }
		void SetMinMoveVolume(float afX){mS->minMoveVolume = afX; }
		void SetMaxMoveFreq(float afX){mS->maxMoveFreq = afX; }
		void SetMaxMoveVolume(float afX){mS->maxMoveVolume = afX; }
		void SetMaxMoveFreqSpeed(float afX){mS->maxMoveFreqSpeed = afX; }
		void SetMiddleMoveSpeed(float afX){mS->middleMoveSpeed = afX; }
		void SetMiddleMoveVolume(float afX){mS->middleMoveVolume = afX; }

		void SetCallback(iPhysicsJointCallback *apCallback, bool abAutoDelete){
						mS->callback = apCallback;
						mS->autoDeleteCallback = abAutoDelete;
		}

		iPhysicsJointCallback* GetCallback(){return mS->callback; }

		bool CheckBreakage(){ return ohpl_h15j_check_breakage(mS->shell, reinterpret_cast<size_t>(this));}

		void SetBreakable(bool abX){ mS->breakable = abX;}
		bool IsBreakable(){ return mS->breakable;}
		void SetBreakForce(float afForce){ mS->breakForce = afForce;}
		float GetBreakForce(){ return mS->breakForce;}
		void SetBreakSound(const tString &asSound){ msBreakSound = asSound;}

		void SetLimitAutoSleep(bool abX){ mS->limitAutoSleep = abX;}
		void SetLimitAutoSleepDist(float afX){ mS->limitAutoSleepDist = afX;}
		void SetLimitAutoSleepNumSteps(int alX){ mS->limitAutoSleepNumSteps = alX;}

		bool GetLimitAutoSleep(){ return mS->limitAutoSleep;}
		float GetLimitAutoSleepDist(){ return mS->limitAutoSleepDist;}
		int GetLimitAutoSleepNumSteps(){ return mS->limitAutoSleepNumSteps;}

		void SetStickyMinLimit(bool abX){ mS->stickyMin = abX;}
		void SetStickyMaxLimit(bool abX){ mS->stickyMax = abX;}
		bool GetStickyMinLimit(){ return mS->stickyMin;}
		bool GetStickyMaxLimit(){ return mS->stickyMax;}

		void SetAllowPositionReset(bool abX){ mS->allowPositionReset = abX;}
		bool GetAllowPositionReset(){ return mS->allowPositionReset;}
		virtual void ResetPosition(){}

		void Break() { mS->broken = true; mS->breakable = true; }

		bool IsBroken(){ return mS->broken;}

		void SetUserData(void *apUserData){ mS->userData = apUserData;}

		void AddController(iPhysicsController *apController){ ohpl_h15j_add_controller(mS->shell, reinterpret_cast<size_t>(this), reinterpret_cast<size_t>(apController));}
		iPhysicsController* GetController(const tString &asName){ return reinterpret_cast<iPhysicsController*>(ohpl_h15j_get_controller(mS->shell, asName.c_str(), asName.size()));}
		bool ChangeController(const tString &asName){ return ohpl_h15j_change_controller(mS->shell, asName.c_str(), asName.size());}
		cPhysicsControllerIterator GetControllerIterator();
		int ControllerCount(){ return (int)ohpl_h15j_controller_len(mS->shell);}
		iPhysicsController* ControllerAt(int alIdx);

		void SetAllControllersPaused(bool abX){ ohpl_h15j_set_all_paused(mS->shell, abX);}

		bool OnPhysicsUpdate(){ return ohpl_h15j_update(mS->shell, reinterpret_cast<size_t>(this));}

		void SetSound(cSoundEntity *apSound);
		cSoundEntity* GetSound(){ return mS->sound;}
		int GetSoundID(){ return mS->soundId;}

	protected:
		OhplJointShellState *mS;
		tString msName;
		tPhysicsControllerMap m_mapControllers;

		cJointLimitEffect mMaxLimit;
		cJointLimitEffect mMinLimit;

		tString msMoveSound;
		tString msBreakSound;

		uint64_t mhRust = 0;
		uint64_t mhRustWorld = 0;

		static size_t HostOp(int32_t alOp, OhplH15Wa *a);

		void OnMaxLimit();
		void OnMinLimit();
		void OnNoLimit() { mS->hasCollided = false; }

		static const OhplL4JointOps* RustOps();
		static iPhysicsJoint* Self(size_t j){ return reinterpret_cast<iPhysicsJoint*>(j);}
		static void RustLimitEvent(size_t j, int32_t kind);
		static void RustCheckAutoSleep(size_t j, float afMin, float afMax, float afValue);
		static void RustNoLimit(size_t j, float afPrev, float afMin, float afMax, bool abSkip);
		static void RustMatrices(size_t j, cMatrixf* apChild, cMatrixf* apParent);
		static void RustPositionCheck(size_t j, float afDistSqr);
	};
};
#endif // HPL_PHYSICS_JOINT_H
