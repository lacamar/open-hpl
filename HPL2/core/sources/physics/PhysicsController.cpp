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

#include "physics/PhysicsController.h"

#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsJoint.h"
#include "physics/RustPhysics.h"

namespace hpl {

	iPhysicsController::iPhysicsController(const tString &asName, iPhysicsWorld *apWorld)
	{
		msName = asName;

		mpWorld = apWorld;

		mpBody = NULL;
		mpJoint = NULL;

		mbActive = false;

		mpPid = ohpl_phys_pid_new();
		ohpl_phys_pid_set_error_num(mpPid, 10);

		mbMulMassWithOutput = false;

		mfMaxOutput = 0;

		mbLogInfo = false;

		mbPaused = false;
	}

	iPhysicsController::~iPhysicsController()
	{
		ohpl_phys_pid_free(mpPid);
	}

	void iPhysicsController::SetPidIntegralSize(int alSize)
	{
		ohpl_phys_pid_set_error_num(mpPid, alSize);
	}

	void iPhysicsController::Update(float afTimeStep)
	{
		OhplCtrlParams params = {
			reinterpret_cast<size_t>(this), reinterpret_cast<size_t>(mpBody), reinterpret_cast<size_t>(mpJoint),
			mbActive, mbPaused, mbLogInfo, mbMulMassWithOutput,
			(int32_t)mType, (int32_t)mInputType, (int32_t)mInputAxis, (int32_t)mOutputType, (int32_t)mOutputAxis, (int32_t)mEndType,
			mfA, mfB, mfC, mfDestValue, mfMaxOutput,
			OhplS(msNextController)
		};
		ohpl_phys_ctrl_update(OhplPhysHost(), &params, mpPid, afTimeStep);
	}

	void iPhysicsController::SetActive(bool abX)
	{
		if(abX == mbActive) return;

		ohpl_phys_pid_reset(mpPid);

		mbActive = abX;
	}
}
