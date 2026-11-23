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

#ifndef HPL_VIDEO_STREAM_THEORA_H
#define HPL_VIDEO_STREAM_THEORA_H

#include "graphics/VideoStream.h"
#include "resources/VideoLoader.h"

#include "impl/RustL5.h"
#include "graphics/Texture.h"
#include "system/String.h"

namespace hpl {

	//-----------------------------------------
	class  cVideoStreamTheora_Loader;

	class cVideoStreamTheora : public iVideoStream
	{
	public:
		cVideoStreamTheora(const tString& asName, cVideoStreamTheora_Loader* apLoader);
		~cVideoStreamTheora() { ohpl_l5_video_free(mpRust); }

		bool LoadFromFile(const tWString& asFilePath)
		{
			SetFullPath(asFilePath);
			msFilePath = asFilePath;
			if(!ohpl_l5_video_load(mpRust, (const uint32_t*)asFilePath.c_str(), asFilePath.size())) return false;
			int32_t w, h;
			ohpl_l5_video_size(mpRust, &w, &h);
			mvSize = cVector2l(w, h);
			return true;
		}

		void Update(float afTimeStep) { ohpl_l5_video_update(mpRust, afTimeStep); }
		void Play() { ohpl_l5_video_play(mpRust); }
		void Stop() { ohpl_l5_video_stop(mpRust); }
		void Pause(bool abX) { mbPaused = abX; ohpl_l5_video_pause(mpRust, abX); }
		bool IsPaused(){ return mbPaused;}
		void SetLoop(bool abX) { mbLooping = abX; ohpl_l5_video_set_loop(mpRust, abX); }
		bool IsLooping(){ return mbLooping;}
		void CopyToTexture(iTexture *apTexture)
		{
			if(const uint8_t* p = ohpl_l5_video_take_frame(mpRust))
				apTexture->SetRawData(0, 0, cVector3l(mvSize.x, mvSize.y, 1), ePixelFormat_RGB, (unsigned char*)p);
		}

	private:
		OhplL5Video *mpRust;

		bool mbLooping;
		bool mbPaused;
	};

	//-----------------------------------------

	class cVideoStreamTheora_Loader : public iVideoLoader
	{
	friend class cVideoStreamTheora;
	public:
		cVideoStreamTheora_Loader()
		{
			AddSupportedExtension("dds");
			OhplL5InstallLog();
			mpRust = ohpl_l5_vloader_new();
		}
		~cVideoStreamTheora_Loader() { ohpl_l5_vloader_free(mpRust); }

		iVideoStream* LoadVideo(const tWString& asName)
		{
			cVideoStreamTheora* pVideo = hplNew(cVideoStreamTheora, (cString::To8Char(cString::GetFileNameW(asName)), this));
			if(pVideo->LoadFromFile(asName)) return pVideo;
			hplDelete(pVideo);
			return NULL;
		}
	
	private:
		OhplL5VLoader* mpRust;
	};

	inline cVideoStreamTheora::cVideoStreamTheora(const tString& asName, cVideoStreamTheora_Loader* apLoader) : iVideoStream(asName, _W(""))
	{
		mbLooping = mbPaused = false;
		OhplL5InstallLog();
		mpRust = ohpl_l5_video_new(apLoader->mpRust);
	}

};
#endif // HPL_VIDEO_STREAM_THEORA_H
