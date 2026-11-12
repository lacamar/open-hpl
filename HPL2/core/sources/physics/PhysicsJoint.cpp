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

#include "physics/PhysicsJoint.h"

#include "physics/CollideShape.h"
#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsController.h"
#include "system/LowLevelSystem.h"

#include "sound/Sound.h"
#include "scene/SoundEntity.h"
#include "sound/SoundHandler.h"
#include "sound/SoundChannel.h"

#include "scene/World.h"
#include "scene/Scene.h"

#include "engine/Engine.h"

#include "math/Math.h"

#include "engine/ScriptFuncs.h"
#include "physics/RustPhysics.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iPhysicsJoint::iPhysicsJoint(const tString &asName, iPhysicsBody *apParentBody, iPhysicsBody *apChildBody, 
		iPhysicsWorld *apWorld,const cVector3f &avPivotPoint, const cVector3f &avPinDir) 
		: msName(asName), mpParentBody(apParentBody), mpChildBody(apChildBody), mpWorld(apWorld)
	{
		mlUniqueID = -1;

		mMaxLimit.msSound = "";
		mMinLimit.msSound = "";

		if(apParentBody) {
			apParentBody->AddJoint(this);
			m_mtxParentBodySetup = apParentBody->GetLocalMatrix();
		}
		else{
			m_mtxParentBodySetup = cMatrixf::Identity;
		}

		mRsBook = OhplBackendRustRequested() ? ohpl_h15_joint_new() : 0;
		m_mtxPrevChild = cMatrixf::Identity;
		m_mtxPrevParent = cMatrixf::Identity;
		
		apChildBody->AddJoint(this);
		m_mtxChildBodySetup = apChildBody->GetLocalMatrix();

		mvPivotPoint = avPivotPoint;
		mvStartPivotPoint = avPivotPoint;
		
		mvPinDir = avPinDir;
		mvStartPinDir = avPinDir;
		
		if(mpParentBody)
		{
			cMatrixf m_mtxInvParent = cMath::MatrixInverse(mpParentBody->GetLocalMatrix());
			mvLocalPivot = cMath::MatrixMul(m_mtxInvParent,avPivotPoint);
			mvLocalPinDir = cMath::MatrixMul(m_mtxInvParent.GetRotation(),avPinDir);
		}
		else
		{
			mvLocalPivot = avPivotPoint;
			mvLocalPinDir = avPinDir;
		}

		msMoveSound = "";

		mbHasCollided = false;
		mpSound = NULL;
		mlSoundID = -1;
		
		mpCallback = NULL;

		mbAutoDeleteCallback = false;

		mpUserData = NULL;

		mbBreakable = false;
		mfBreakForce = 0;
		msBreakSound = "";
		mbBroken = false;

		mbStickyMinLimit = false;
		mbStickyMaxLimit = false;

		mlLimitStepCount =0;

		mlSpeedCount =0;

		mbLimitAutoSleep = false;
		mbAllowPositionReset = false;
		mbPositionResetPending = false;
		mfLimitAutoSleepDist = 0.02f;
		mlLimitAutoSleepNumSteps = 10;

		//Log("Created joint '%s'\n",msName.c_str());
	}
	
	iPhysicsJoint::~iPhysicsJoint()
	{
		if(mbAutoDeleteCallback && mpCallback) hplDelete(mpCallback);

		//Destroy all controllers.
		tPhysicsControllerMapIt it = m_mapControllers.begin();
		for(; it != m_mapControllers.end(); ++it)
		{
			mpWorld->DestroyController(it->second);
		}

		if(mpChildBody) mpChildBody->RemoveJoint(this);
		if(mpParentBody) mpParentBody->RemoveJoint(this);

		if(mpSound) mpWorld->GetWorld()->DestroySoundEntity(mpSound);

		if(mRsBook) ohpl_h15_joint_free(mRsBook);

		//Log("Deleted joint '%s'\n",msName.c_str());
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	/**
	 * This should only be used by PhysicsBody.
	 */
	void iPhysicsJoint::RemoveBody(iPhysicsBody *apBody)
	{
		if(mpParentBody == apBody) mpParentBody = NULL;
		if(mpChildBody == apBody) mpChildBody = NULL;
	}

	//-----------------------------------------------------------------------
	
	void iPhysicsJoint::AddController(iPhysicsController *apController)
	{
		
		//Add controller top map
		m_mapControllers.insert(tPhysicsControllerMap::value_type(apController->GetName(), apController));

		//Set properties
		apController->SetBody(mpChildBody);
		apController->SetJoint(this);
	}

	//-----------------------------------------------------------------------

	iPhysicsController* iPhysicsJoint::GetController(const tString &asName)
	{
		tPhysicsControllerMapIt it = m_mapControllers.find(asName);
		if(it == m_mapControllers.end()) return NULL;

		return it->second;
	}

	//-----------------------------------------------------------------------

	bool iPhysicsJoint::ChangeController(const tString &asName)
	{
		iPhysicsController *pNewCtrl = GetController(asName);
		if(pNewCtrl==NULL) return false;
		
		tPhysicsControllerMapIt it = m_mapControllers.begin();
		for(; it != m_mapControllers.end(); ++it)
		{
			iPhysicsController *pCtrl = it->second;

			if(pCtrl == pNewCtrl)
			{
				pCtrl->SetActive(true);	
			}
			else
			{
				pCtrl->SetActive(false);
			}
		}

		return true;
	}

	//-----------------------------------------------------------------------

	void iPhysicsJoint::SetAllControllersPaused(bool abX)
	{
		tPhysicsControllerMapIt it = m_mapControllers.begin();
		for(; it != m_mapControllers.end(); ++it)
		{
			iPhysicsController *pCtrl = it->second;
			
			pCtrl->SetPaused(abX);
		}
	}

	//-----------------------------------------------------------------------

	cPhysicsControllerIterator iPhysicsJoint::GetControllerIterator()
	{
		return cPhysicsControllerIterator(&m_mapControllers);
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iPhysicsJoint::OnMaxLimit()
	{
		OhplLimitEffect effect = { OhplS(mMaxLimit.msSound), mMaxLimit.mfMinSpeed, mMaxLimit.mfMaxSpeed };
		ohpl_phys_joint_on_limit(OhplPhysHost(), reinterpret_cast<size_t>(this), true, &effect);
	}

	//-----------------------------------------------------------------------

	void iPhysicsJoint::OnMinLimit()
	{
		OhplLimitEffect effect = { OhplS(mMinLimit.msSound), mMinLimit.mfMinSpeed, mMinLimit.mfMaxSpeed };
		ohpl_phys_joint_on_limit(OhplPhysHost(), reinterpret_cast<size_t>(this), false, &effect);
	}
	
	//-----------------------------------------------------------------------



	//-----------------------------------------------------------------------
	
	bool iPhysicsJoint::OnPhysicsUpdate()
	{
		if(mbPositionResetPending) ResetPosition();
		mbPositionResetPending = false;

		OhplMoveParams params = {
			mfMinMoveSpeed, mfMinMoveFreq, mfMinMoveFreqSpeed, mfMinMoveVolume,
			mfMaxMoveFreq, mfMaxMoveFreqSpeed, mfMaxMoveVolume, mfMiddleMoveSpeed, mfMiddleMoveVolume,
			(int32_t)mMoveSpeedType, OhplS(msMoveSound)
		};
		OhplJointState state = {
			reinterpret_cast<size_t>(mpParentBody), reinterpret_cast<size_t>(mpChildBody), reinterpret_cast<size_t>(mpWorld),
			mvPivotPoint, mvPinDir, mvLocalPivot, mvLocalPinDir,
			m_mtxPrevChild, m_mtxPrevParent,
			reinterpret_cast<size_t>(mpSound), mlSoundID, mlSpeedCount
		};

		bool bUpdated = ohpl_phys_joint_update(OhplPhysHost(), &params, &state, mRsBook);

		mvPivotPoint = state.pivot;
		mvPinDir = state.pinDir;
		m_mtxPrevChild = state.prevChild;
		m_mtxPrevParent = state.prevParent;
		mpSound = reinterpret_cast<cSoundEntity*>(state.sound);
		mlSoundID = state.soundId;
		mlSpeedCount = state.speedCount;

		return bUpdated;
	}

	//-----------------------------------------------------------------------

	void iPhysicsJoint::SetSound(cSoundEntity *apSound)
	{
		mpSound = apSound;
        if(apSound)
			mlSoundID = apSound->GetCreationID();
		else
			mlSoundID = -1;
	}
	
	//-----------------------------------------------------------------------
	


	//-----------------------------------------------------------------------

	void iPhysicsJoint::OnNoLimit()
	{
		mbHasCollided = false;
	}

	//-----------------------------------------------------------------------

	void iPhysicsJoint::Break()
	{
		mbBroken = true;
		mbBreakable = true;
	}

	//-----------------------------------------------------------------------

	bool iPhysicsJoint::CheckBreakage()
	{
		return ohpl_phys_joint_check_breakage(OhplPhysHost(), reinterpret_cast<size_t>(this), mbBreakable, mbBroken, mfBreakForce, OhplS(msBreakSound));
	}
	
	//-----------------------------------------------------------------------

	void iPhysicsJoint::CheckLimitAutoSleep(iPhysicsJoint *apJoint,
											const float afMin, const float afMax,
											const float afDist)
	{
		const bool bSleep = apJoint->mRsBook
			? ohpl_h15_joint_limit_auto_sleep(apJoint->mRsBook, apJoint->mbLimitAutoSleep, apJoint->mfLimitAutoSleepDist,
											apJoint->mlLimitAutoSleepNumSteps, afMin, afMax, afDist)
			: ohpl_phys_joint_limit_auto_sleep(apJoint->mbLimitAutoSleep, apJoint->mfLimitAutoSleepDist,
											apJoint->mlLimitAutoSleepNumSteps, &apJoint->mlLimitStepCount,
											afMin, afMax, afDist);
		if(bSleep)
		{
			apJoint->mpChildBody->DisableAfterSimulation();
		}
	}

	//-----------------------------------------------------------------------
}
