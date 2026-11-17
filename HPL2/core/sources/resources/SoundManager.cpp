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

#include "resources/SoundManager.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "resources/Resources.h"
#include "sound/Sound.h"
#include "sound/SoundData.h"
#include "sound/LowLevelSound.h"
#include "resources/FileSearcher.h"
#include "resources/RustResMgr.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSoundManager::cSoundManager(cSound* apSound,cResources *apResources)
		: iResourceManager(apResources->GetFileSearcher(), apResources->GetLowLevel(),
							apResources->GetLowLevelSystem())
	{
		mpSound = apSound;
		mpResources = apResources;

		mpSound->GetLowLevel()->GetSupportedFormats(mlstFileFormats);
	}

	cSoundManager::~cSoundManager()
	{
		ohpl_h3f_shutdown(mpRs, H3Ops(), OHPL_H3S_SOUND);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iSoundData* cSoundManager::CreateSoundData(const tString& asName, bool abStream,bool abLoopStream)
	{
		cRmExts formats(mlstFileFormats);
		return H3Res<iSoundData>(ohpl_h3f_sound(mpRs, H3Ops(), RM_STR(asName), abStream, abLoopStream, formats.h3(), formats.size()));
	}

	//-----------------------------------------------------------------------

	void cSoundManager::Unload(iResourceBase* apResource)
	{

	}
	//-----------------------------------------------------------------------

	void cSoundManager::Destroy(iResourceBase* apResource)
	{
		ohpl_h3f_sound_destroy(mpRs, H3Ops(), apResource);
	}

	//-----------------------------------------------------------------------

	void cSoundManager::DestroyAll()
	{
		ohpl_h3f_sound_destroy_all(mpRs, H3Ops());
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	

	//-----------------------------------------------------------------------



	void* cSoundManager::H3Create(OhplH3Create &aCreate)
	{
		return static_cast<iResourceBase*>(mpSound->GetLowLevel()->LoadSoundData(H3Name(aCreate.name), H3Wide(aCreate.path), "", aCreate.i[0] != 0, aCreate.i[1] != 0));
	}

	int64_t cSoundManager::H3Call(int alWhat, void *apObj, int64_t alArg)
	{
		iSoundData *pData = H3Res<iSoundData>(apObj);
		if(alWhat == OHPL_H3C_IS_STREAM) return pData->IsStream();
		if(alWhat == OHPL_H3C_SOUND_SET_MGR) pData->SetSoundManager(mpResources->GetSoundManager());
		return 0;
	}
}
