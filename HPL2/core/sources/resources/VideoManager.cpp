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

#include "resources/VideoManager.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "resources/Resources.h"
#include "resources/FileSearcher.h"
#include "graphics/VideoStream.h"
#include "resources/VideoLoaderHandler.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cVideoManager::cVideoManager(cGraphics* apGraphics,cResources *apResources)
		: iResourceManager(apResources->GetFileSearcher(), apResources->GetLowLevel(),
							apResources->GetLowLevelSystem())
	{
		mpGraphics = apGraphics;
		mpResources = apResources;
	}

	cVideoManager::~cVideoManager()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iVideoStream* cVideoManager::CreateVideo(const tString& asName)
	{
		if(mpRs) return H3Res<iVideoStream>(ohpl_h3f_video(mpRs, H3Ops(), RM_STR(asName)));
		BeginLoad(asName);

		tWString sPath = mpFileSearcher->GetFilePath(asName);
		if(sPath == _W("")) 
		{
			EndLoad();
			Error("Video file '%s' could not be found!\n",asName.c_str());
			return NULL;
		}

		iVideoStream *pVideo = mpResources->GetVideoLoaderHandler()->LoadVideo(sPath);
		if(pVideo == NULL)
		{
			EndLoad();
			Error("Could not load video '%s'\n",asName.c_str());
			return NULL;
		}
		pVideo->SetFullPath(sPath);

		AddResource(pVideo);

		EndLoad();
		return pVideo;
	}

	//-----------------------------------------------------------------------

	void cVideoManager::Unload(iResourceBase* apResource)
	{

	}
	//-----------------------------------------------------------------------

	void cVideoManager::Destroy(iResourceBase* apResource)
	{
		if(mpRs) { ohpl_h3f_video_destroy(mpRs, H3Ops(), apResource); return; }
		if(apResource)
		{
			RemoveResource(apResource);
			hplDelete(apResource);
		}
	}

	//-----------------------------------------------------------------------

	void cVideoManager::Update(float afTimeStep)
	{
		if(mpRs) { ohpl_h3f_video_update(mpRs, H3Ops(), H3Bits(afTimeStep)); return; }
		tResourceBaseMapIt it = m_mapResources.begin();
		for(; it != m_mapResources.end(); ++it)
		{
			iResourceBase *pBase = it->second;
			iVideoStream *pVideo = static_cast<iVideoStream*>(pBase);

			pVideo->Update(afTimeStep);
		}

	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

	void* cVideoManager::H3Create(OhplH3Create &aCreate)
	{
		tWString sPath = H3Wide(aCreate.path);
		iVideoStream *pVideo = mpResources->GetVideoLoaderHandler()->LoadVideo(sPath);
		if(pVideo == NULL) return NULL;
		pVideo->SetFullPath(sPath);
		return static_cast<iResourceBase*>(pVideo);
	}

	int64_t cVideoManager::H3Call(int alWhat, void *apObj, int64_t alArg)
	{
		if(alWhat == OHPL_H3C_UPDATE) H3Res<iVideoStream>(apObj)->Update(H3Float(alArg));
		return 0;
	}
}
