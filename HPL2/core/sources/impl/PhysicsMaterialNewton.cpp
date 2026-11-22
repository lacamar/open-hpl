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

#include "physics/PhysicsMaterial.h"

#include "physics/PhysicsWorld.h"
#include "impl/RustNewton.h"
#include "impl/RustNewtonBackend.h"

#include "system/LowLevelSystem.h"
#include <algorithm>
#include <vector>

namespace hpl {

	iPhysicsMaterial::iPhysicsMaterial(const tString &asName, iPhysicsWorld *apWorld,int alMatId)
		: mpWorld(apWorld), msName(asName), mbPreloaded(false), mpSurfaceData(NULL)
	{
		static_assert(offsetof(iPhysicsMaterial, mpSurfaceData) == 48 && offsetof(iPhysicsMaterial, mlMaterialId) == 64 && offsetof(iPhysicsMaterial, mFrictionMode) == 68 && sizeof(OhplNewtonMatProps) == 20, "rust/src/physics/cxx_newton.rs material offsets");
		iPhysicsWorld *pNWorld = static_cast<iPhysicsWorld*>(mpWorld);

		mpNewtonWorld = pNWorld->GetNewtonWorld();

		if(alMatId==-1)
		{
			mlMaterialId = ohpl_ntm_create_group(pNWorld->RustHandle());
		}
		else
		{
			mlMaterialId = alMatId;
		}

		//Setup default properties
		mFrictionMode = ePhysicsMaterialCombMode_Average;
		mElasticityMode  = ePhysicsMaterialCombMode_Average;

		mfElasticity = 0.5f;
		mfStaticFriction = 0.3f;
		mfKineticFriction = 0.3f;

		//Log(" Created physics material '%s' with Newton id %d\n",asName.c_str(),mlMaterialId);
	}


}
