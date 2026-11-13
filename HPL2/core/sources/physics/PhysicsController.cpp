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
#include "physics/RustPhysBook.h"

namespace hpl {

	iPhysicsController::iPhysicsController(const tString &asName, iPhysicsWorld *apWorld)
	{
		msName = asName;

		mpWorld = apWorld;

		if(OhplBackendRustRequested())
		{
			mpPid = ohpl_h15c_init(&mS, reinterpret_cast<size_t>(this));
			return;
		}
		mS.self = this;

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
		ohpl_phys_ctrl_update(OhplPhysHost(), reinterpret_cast<const OhplCtrlParams*>(&mS), mpPid, afTimeStep);
	}

	void iPhysicsController::SetActive(bool abX)
	{
		if(abX == mbActive) return;

		ohpl_phys_pid_reset(mpPid);

		mbActive = abX;
	}
}
