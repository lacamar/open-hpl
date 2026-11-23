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

#include "resources/SoundEntityManager.h"

#include "system/String.h"
#include "sound/Sound.h"
#include "resources/Resources.h"
#include "resources/FileSearcher.h"
#include "sound/SoundEntityData.h"
#include "system/LowLevelSystem.h"
#include "sound/SoundHandler.h"
#include "resources/RustResMgr.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSoundEntityManager::cSoundEntityManager(cSound* apSound,cResources *apResources)
		: iResourceManager(apResources->GetFileSearcher(), apResources->GetLowLevel(),
							apResources->GetLowLevelSystem())
	{
		mpSound = apSound;
		mpResources = apResources;
		mpCustom = ohpl_rm_custom_new();
	}

	cSoundEntityManager::~cSoundEntityManager()
	{
		ohpl_h3f_shutdown(mpRs, H3Ops(), OHPL_H3S_SOUND_ENTITY);
		ohpl_rm_custom_free(mpCustom);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cSoundEntityManager::Preload(const tString& asFile)
	{
		ohpl_h3f_sound_entity_preload(mpRs, H3Ops(), RM_STR(asFile), mpCustomResolver != NULL);
	}

	//-----------------------------------------------------------------------

	void cSoundEntityManager::AddCustomSoundEntity(const tString& asName, cSoundEntityData *apData)
	{
		ohpl_h3f_sound_entity_add_custom(mpRs, H3Ops(), RM_STR(asName), static_cast<iResourceBase*>(apData));
	}

	cSoundEntityData* cSoundEntityManager::CreateSoundEntity(const tString& asName)
	{
		return H3Res<cSoundEntityData>(ohpl_h3f_sound_entity(mpRs, H3Ops(), RM_STR(asName), mpCustomResolver != NULL));
	}

	//-----------------------------------------------------------------------

	void cSoundEntityManager::Unload(iResourceBase* apResource)
	{

	}
	//-----------------------------------------------------------------------

	void cSoundEntityManager::Destroy(iResourceBase* apResource)
	{
		ohpl_h3f_destroy_standard(mpRs, H3Ops(), apResource);
	}

	void* cSoundEntityManager::H3Create(OhplH3Create &aCreate)
	{
		if(aCreate.kind == OHPL_H3_CUSTOM_RESOLVER) return static_cast<iResourceBase*>(mpCustomResolver(H3Name(aCreate.name)));
		cSoundEntityData *pSoundEntity = hplNew( cSoundEntityData, (H3Name(aCreate.name), mpResources,mpSound) );
		if(pSoundEntity->CreateFromFile(H3Wide(aCreate.path))) return static_cast<iResourceBase*>(pSoundEntity);
		hplDelete(pSoundEntity);
		return NULL;
	}

	int64_t cSoundEntityManager::H3Call(int alWhat, void *apObj, int64_t alArg)
	{
		if(alWhat == OHPL_H3C_PRELOAD_SOUNDS) H3Res<cSoundEntityData>(apObj)->PreloadSounds();
		return 0;
	}
}
