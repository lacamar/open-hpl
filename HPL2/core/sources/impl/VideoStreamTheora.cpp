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

#include "impl/VideoStreamTheora.h"

#include "system/LowLevelSystem.h"
#include "math/Math.h"
#include "graphics/Texture.h"
#include "system/String.h"

namespace hpl {

	cVideoStreamTheora_Loader::cVideoStreamTheora_Loader()
	{
		AddSupportedExtension("dds");

		OhplL5InstallLog();
		mpRust = ohpl_l5_vloader_new();
	}

	cVideoStreamTheora_Loader::~cVideoStreamTheora_Loader()
	{
		ohpl_l5_vloader_free(mpRust);
	}

	iVideoStream* cVideoStreamTheora_Loader::LoadVideo(const tWString& asFile)
	{ 
		tString sName = cString::To8Char(cString::GetFileNameW(asFile));
		cVideoStreamTheora *pVideo = hplNew( cVideoStreamTheora, (sName,this) );
		
		if(pVideo->LoadFromFile(asFile)==false)
		{
			hplDelete(pVideo);
			return NULL;
		}
		return pVideo;
	}

	cVideoStreamTheora::cVideoStreamTheora(const tString& asName, cVideoStreamTheora_Loader* apLoader) : iVideoStream(asName, _W(""))
	{
		mbLooping = false;
		mbPaused = false;
		OhplL5InstallLog();
		mpRust = ohpl_l5_video_new(apLoader->mpRust);
	}

	cVideoStreamTheora::~cVideoStreamTheora()
	{
		ohpl_l5_video_free(mpRust);
	}

	bool cVideoStreamTheora::LoadFromFile(const tWString& asFilePath)
	{
		SetFullPath(asFilePath);

		msFilePath = asFilePath;
		if(ohpl_l5_video_load(mpRust, (const uint32_t*)asFilePath.c_str(), asFilePath.size())==false) return false;
		int32_t lWidth, lHeight;
		ohpl_l5_video_size(mpRust, &lWidth, &lHeight);
		mvSize = cVector2l(lWidth, lHeight);
		return true;
	}

	void cVideoStreamTheora::Update(float afTimeStep)
	{
		ohpl_l5_video_update(mpRust, afTimeStep);
	}

	void cVideoStreamTheora::Play()
	{
		ohpl_l5_video_play(mpRust);
	}

	void cVideoStreamTheora::Stop()
	{
		ohpl_l5_video_stop(mpRust);
	}

	void cVideoStreamTheora::Pause(bool abX)
	{
		mbPaused = abX;
		ohpl_l5_video_pause(mpRust, abX);
	}

	void cVideoStreamTheora::SetLoop(bool abX)
	{
		mbLooping = abX;
		ohpl_l5_video_set_loop(mpRust, abX);
	}

	void cVideoStreamTheora::CopyToTexture(iTexture *apTexture)
	{
		const uint8_t* pFrame = ohpl_l5_video_take_frame(mpRust);
		if(pFrame) apTexture->SetRawData(0,0,cVector3l(mvSize.x,mvSize.y,1) ,ePixelFormat_RGB,(unsigned char*)pFrame);
	}
}
