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

#ifndef HPL_SOUND_ENTITY_MANAGER_H
#define HPL_SOUND_ENTITY_MANAGER_H

#include "resources/ResourceManager.h"

#include <map>

namespace hpl {

	class cSound;
	class cResources;
	class cSoundEntityData;
	
	class cSoundEntityManager : public iResourceManager
	{
	public:
		cSoundEntityManager(cSound* apSound,cResources *apResources);
		~cSoundEntityManager();

		void Preload(const tString& asFile);

		cSoundEntityData* CreateSoundEntity(const tString& asName);

		void AddCustomSoundEntity(const tString& asName, cSoundEntityData *apData);
		typedef cSoundEntityData* (*tCustomSoundEntityResolver)(const tString& asName);
		void SetCustomResolver(tCustomSoundEntityResolver apResolver){ mpCustomResolver = apResolver; }
		
		void Destroy(iResourceBase* apResource);
		void Unload(iResourceBase* apResource);

	private:
		cSound* mpSound;
		cResources* mpResources;
		std::map<tString, cSoundEntityData*> m_mapCustom;
		tCustomSoundEntityResolver mpCustomResolver = NULL;
	};

};
#endif // HPL_SOUND_ENTITY_MANAGER_H
