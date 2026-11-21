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

#include "physics/Physics.h"

#include "physics/PhysicsWorld.h"
#include "physics/SurfaceData.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"

#include "physics/RustPhysics.h"
#include "physics/RustPhysBook.h"
#include "physics/PhysicsWorld.h"

namespace hpl {

	size_t cPhysics::HostOp(int32_t alOp, OhplH15Wa *a)
	{
		cPhysics *pPhysics = reinterpret_cast<cPhysics*>(a->p[0]);
		cSurfaceData *pSurface = reinterpret_cast<cSurfaceData*>(a->p[1]);
		tString sArg;
		if(alOp == 103 || alOp == 106) sArg = tString(reinterpret_cast<const char*>(a->p[1]), a->p[2]);
		switch(alOp)
		{
		case 101: return reinterpret_cast<size_t>(hplNew( iPhysicsWorld, () ));
		case 102: hplDelete(reinterpret_cast<iPhysicsWorld*>(a->p[1])); return 0;
		case 103: return reinterpret_cast<size_t>(hplNew( cSurfaceData, (sArg, pPhysics, pPhysics->mpResources) ));
		case 104: hplDelete(pSurface); return 0;
		case 105: reinterpret_cast<cSurfaceData*>(a->p[0])->ToMaterial(reinterpret_cast<iPhysicsWorld*>(a->p[1])); return 0;
		case 106: Error("Couldn't load XML file '%s'!\n", sArg.c_str()); return 0;
		case 107: return reinterpret_cast<size_t>(pSurface->RustState());
		case 108:
		{
			tString sVal(reinterpret_cast<const char*>(a->p[2]), a->p[3]);
			if(a->i[0] == 0) pSurface->SetUserVar(sVal);
			else if(a->i[0] == 1) pSurface->GetStepType(sVal);
			else if(a->i[0] == 2) pSurface->SetScrapeSoundName(sVal);
			else pSurface->SetRollSoundName(sVal);
			return 0;
		}
		}
		return 0;
	}

	cPhysics::cPhysics()  : iUpdateable("HPL_Physics")
	{

		static_assert(offsetof(cPhysics, mRs) == 96 && offsetof(cPhysics, mS) == 104, "rust/src/physics/cxx_physics.rs offsets");
		mRs = ohpl_h15p_new(HostOp, reinterpret_cast<size_t>(this), &mS);
	}

	cPhysics::~cPhysics()
	{
		Log("Exiting Physics Module\n");
		Log("--------------------------------------------------------\n");

		ohpl_h15p_free(mRs);
		Log("--------------------------------------------------------\n\n");
	}

	void cPhysics::Init(cResources *apResources)
	{
		mpResources = apResources;
	}

	cSurfaceDataIterator cPhysics::GetSurfaceDataIterator()
	{
		m_mapSurfaceData.clear();
		for(size_t i=0, n=ohpl_h15p_surface_len(mRs); i<n; ++i)
		{
			OhplStr name; size_t obj;
			if(ohpl_h15p_surface_at(mRs, i, &name, &obj)) m_mapSurfaceData[tString(name.p, name.n)] = reinterpret_cast<cSurfaceData*>(obj);
		}
		return cSurfaceDataIterator(&m_mapSurfaceData);
	}

}
