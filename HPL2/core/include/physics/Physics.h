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

#ifndef HPL_Physics_H
#define HPL_Physics_H

#include <list>
#include <map>
#include "engine/Updateable.h"
#include "system/SystemTypes.h"
#include "physics/PhysicsMaterial.h"
#include "physics/RustPhysBook.h"

namespace hpl {

	class iPhysicsWorld;
	class cSurfaceData;
	class cResources;

	typedef std::map<tString, cSurfaceData*> tSurfaceDataMap;
	typedef tSurfaceDataMap::iterator tSurfaceDataMapIt;

	typedef cSTLMapIterator<cSurfaceData*, tSurfaceDataMap, tSurfaceDataMapIt> cSurfaceDataIterator;

	class cPhysics : public iUpdateable
	{
	public:
		cPhysics();
		~cPhysics();

		void Init(cResources *apResources);

		void Update(float afTimeStep);

		iPhysicsWorld* CreateWorld(bool abAddSurfaceData);
		void DestroyWorld(iPhysicsWorld* apWorld);

		cSurfaceData *CreateSurfaceData(const tString& asName);
		cSurfaceData *GetSurfaceData(const tString& asName);
		bool LoadSurfaceData(const tString& asFile);

		static size_t HostOp(int32_t alOp, OhplH15Wa *a);

		void SetImpactDuration(float afX){ mfImpactDuration = afX;}
		float GetImpactDuration(){ return mfImpactDuration;}

		void SetMaxImpacts(int alX){ mlMaxImpacts = alX;}
		int GetMaxImpacts(){ return mlMaxImpacts;}
		int GetNumOfImpacts();

		bool CanPlayImpact();
		void AddImpact();

		cSurfaceDataIterator GetSurfaceDataIterator();

		void SetDebugLog(bool abX){ mbLog = abX;}
		bool GetDebugLog(){ return mbLog;}

	private:
		cResources *mpResources;

		tSurfaceDataMap m_mapSurfaceData;

		uint64_t mRs;
		OhplPhysState mS;
		float &mfImpactDuration = mS.impactDuration;
		int &mlMaxImpacts = mS.maxImpacts;
		bool &mbLog = mS.log;
	};

};
#endif // HPL_Physics_H
