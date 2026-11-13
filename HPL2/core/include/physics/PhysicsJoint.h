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

class NewtonJoint;
class NewtonWorld;
class NewtonBody;

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

		void SetUniqueID(int alID){mlUniqueID = alID;}
		int GetUniqueID(){return mlUniqueID;}

		iPhysicsBody * GetParentBody(){ return mpParentBody;}
		iPhysicsBody * GetChildBody(){ return mpChildBody;}

		void RemoveBody(iPhysicsBody *apBody);

		const cVector3f& GetPivotPoint(){ return mvPivotPoint;}
		const cVector3f& GetPinDir(){ return mvPinDir;}

		virtual ePhysicsJointType GetType()=0;

		void SetCollideBodies(bool abX);
		bool GetCollideBodies();

		void SetStiffness(float afX);
		float GetStiffness();

		virtual cVector3f GetVelocity()=0;
		virtual cVector3f GetAngularVelocity()=0;
		virtual float GetForceSize()=0;

		virtual float GetDistance()=0;
		virtual float GetAngle()=0;

		cJointLimitEffect* GetMaxLimit(){ return &mMaxLimit;}
		cJointLimitEffect* GetMinLimit(){ return &mMinLimit;}

		void SetMoveSound(tString &asName) {msMoveSound = asName;}

		void SetMoveSpeedType(ePhysicsJointSpeed aType){ mMoveSpeedType = aType;}
		void SetMinMoveSpeed(float afX){mfMinMoveSpeed = afX;}
		void SetMinMoveFreq(float afX){mfMinMoveFreq = afX; }
		void SetMinMoveFreqSpeed(float afX){mfMinMoveFreqSpeed = afX; }
		void SetMinMoveVolume(float afX){mfMinMoveVolume = afX; }
		void SetMaxMoveFreq(float afX){mfMaxMoveFreq = afX; }
		void SetMaxMoveVolume(float afX){mfMaxMoveVolume = afX; }
		void SetMaxMoveFreqSpeed(float afX){mfMaxMoveFreqSpeed = afX; }
		void SetMiddleMoveSpeed(float afX){mfMiddleMoveSpeed = afX; }
		void SetMiddleMoveVolume(float afX){mfMiddleMoveVolume = afX; }

		void SetCallback(iPhysicsJointCallback *apCallback, bool abAutoDelete){
						mpCallback = apCallback;
						mbAutoDeleteCallback = abAutoDelete;
		}

		iPhysicsJointCallback* GetCallback(){return mpCallback; }

		bool CheckBreakage();

		void SetBreakable(bool abX){ mbBreakable = abX;}
		bool IsBreakable(){ return mbBreakable;}
		void SetBreakForce(float afForce){ mfBreakForce = afForce;}
		float GetBreakForce(){ return mfBreakForce;}
		void SetBreakSound(const tString &asSound){ msBreakSound = asSound;}

		void SetLimitAutoSleep(bool abX){ mbLimitAutoSleep = abX;}
		void SetLimitAutoSleepDist(float afX){ mfLimitAutoSleepDist = afX;}
		void SetLimitAutoSleepNumSteps(int alX){ mlLimitAutoSleepNumSteps = alX;}

		bool GetLimitAutoSleep(){ return mbLimitAutoSleep;}
		float GetLimitAutoSleepDist(){ return mfLimitAutoSleepDist;}
		int GetLimitAutoSleepNumSteps(){ return mlLimitAutoSleepNumSteps;}

		void SetStickyMinLimit(bool abX){ mbStickyMinLimit = abX;}
		void SetStickyMaxLimit(bool abX){ mbStickyMaxLimit = abX;}
		bool GetStickyMinLimit(){ return mbStickyMinLimit;}
		bool GetStickyMaxLimit(){ return mbStickyMaxLimit;}

		void SetAllowPositionReset(bool abX){ mbAllowPositionReset = abX;}
		bool GetAllowPositionReset(){ return mbAllowPositionReset;}
		virtual void ResetPosition(){}

		void Break() { mbBroken = true; mbBreakable = true; }

		bool IsBroken(){ return mbBroken;}

		void SetUserData(void *apUserData){ mpUserData = apUserData;}

		void AddController(iPhysicsController *apController);
		iPhysicsController* GetController(const tString &asName);
		bool ChangeController(const tString &asName);
		cPhysicsControllerIterator GetControllerIterator();
		int ControllerCount();
		iPhysicsController* ControllerAt(int alIdx);

		void SetAllControllersPaused(bool abX);

		bool OnPhysicsUpdate();

		void SetSound(cSoundEntity *apSound);
		cSoundEntity* GetSound(){ return mpSound;}
		int GetSoundID(){ return mlSoundID;}

	protected:
		OhplJointShellState mS = OhplJointShellState();
		tString msName;
		int &mlUniqueID = mS.uniqueId;

		iPhysicsBody *&mpParentBody = mS.parent;
		iPhysicsBody *&mpChildBody = mS.child;
		iPhysicsWorld *&mpWorld = mS.world;

		cVector3f &mvPinDir = mS.pinDir;
		cVector3f &mvPivotPoint = mS.pivot;

		cVector3f &mvStartPinDir = mS.startPinDir;

		bool &mbStickyMinLimit = mS.stickyMin;
		bool &mbStickyMaxLimit = mS.stickyMax;
		bool &mbAllowPositionReset = mS.allowPositionReset;
		bool &mbPositionResetPending = mS.positionResetPending;

		tPhysicsControllerMap m_mapControllers;

		cJointLimitEffect mMaxLimit;
		cJointLimitEffect mMinLimit;

		tString msMoveSound;

		float &mfMinMoveSpeed = mS.minMoveSpeed;
		float &mfMinMoveFreq = mS.minMoveFreq;
		float &mfMinMoveFreqSpeed = mS.minMoveFreqSpeed;
		float &mfMinMoveVolume = mS.minMoveVolume;
		float &mfMaxMoveFreq = mS.maxMoveFreq;
		float &mfMaxMoveFreqSpeed = mS.maxMoveFreqSpeed;
		float &mfMaxMoveVolume = mS.maxMoveVolume;
		float &mfMiddleMoveSpeed = mS.middleMoveSpeed;
		float &mfMiddleMoveVolume = mS.middleMoveVolume;
		ePhysicsJointSpeed &mMoveSpeedType = reinterpret_cast<ePhysicsJointSpeed&>(mS.moveSpeedType);

		bool &mbBreakable = mS.breakable;
		float &mfBreakForce = mS.breakForce;
		tString msBreakSound;
		bool &mbBroken = mS.broken;

		bool &mbLimitAutoSleep = mS.limitAutoSleep;
		float &mfLimitAutoSleepDist = mS.limitAutoSleepDist;
		int &mlLimitAutoSleepNumSteps = mS.limitAutoSleepNumSteps;

		cSoundEntity *&mpSound = mS.sound;
		int &mlSoundID = mS.soundId;
		bool &mbHasCollided = mS.hasCollided;

		iPhysicsJointCallback *&mpCallback = mS.callback;
		bool &mbAutoDeleteCallback = mS.autoDeleteCallback;

		int &mlLimitStepCount = mS.limitStepCount;
		static size_t HostOp(int32_t alOp, OhplH15Wa *a);

		void *&mpUserData = mS.userData;

		static void CheckLimitAutoSleep(iPhysicsJoint *apJoint, const float afMin, const float afMax,
										const float afDist);

		void OnMaxLimit();
		void OnMinLimit();
		void OnNoLimit() { mbHasCollided = false; }

		cMatrixf GetMatrixFromPinAndPivot(const cVector3f& avPinDir,const cVector3f& avPivot);
		void CalculateGlobalMatrix (const cMatrixf& a_mtxLocalMatrix0, const cMatrixf& a_mtxLocalMatrix1, cMatrixf& a_mtxMatrix0, cMatrixf& a_mtxMatrix1);
		void CalculateLocalMatrix (const cMatrixf& a_mtxPinsAndPivotFrame, cMatrixf& a_mtxLocalMatrix0, cMatrixf& a_mtxLocalMatrix1);
		static const OhplL4JointOps* RustOps();
		static iPhysicsJoint* Self(size_t j){ return reinterpret_cast<iPhysicsJoint*>(j);}
		static void RustLimitEvent(size_t j, int32_t kind);
		static void RustCheckAutoSleep(size_t j, float afMin, float afMax, float afValue);
		static void RustNoLimit(size_t j, float afPrev, float afMin, float afMax, bool abSkip);
		static void RustMatrices(size_t j, cMatrixf* apChild, cMatrixf* apParent);
		static void RustPositionCheck(size_t j, float afDistSqr);

		NewtonJoint* mpNewtonJoint;
		NewtonWorld* mpNewtonWorld;
		uint64_t mhRust = 0;
		uint64_t mhRustWorld = 0;
		NewtonBody* mpNewtonParentBody;
		NewtonBody* mpNewtonChildBody;

	};
};
#endif // HPL_PHYSICS_JOINT_H
