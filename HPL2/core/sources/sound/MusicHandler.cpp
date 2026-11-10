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
#include "sound/MusicHandler.h"
#include "resources/Resources.h"
#include "system/LowLevelSystem.h"
#include "sound/LowLevelSound.h"
#include "sound/SoundChannel.h"
#include "sound/SoundData.h"
#include "resources/SoundManager.h"

namespace hpl {

	const OhplSoundChannelOps* SoundChannelOps();

	//-----------------------------------------------------------------------

	static cResources* Res(void *ap){ return static_cast<cResources*>(ap); }

	static const OhplMusicHostOps gMusicHostOps = {
		SoundChannelOps(),
		[](void*) -> OhplMusicSong* { return hplNew( cMusicEntry, () ); },
		[](void*, OhplMusicSong *apSong)
		{
			hplDelete(apSong->mpStream);
			hplDelete(static_cast<cMusicEntry*>(apSong));
		},
		[](void *apRes, const unsigned char *asName, size_t alLen, bool abLoop) -> void*
		{
			tString sName((const char*)asName, alLen);
			iSoundData* pData = Res(apRes)->GetSoundManager()->CreateSoundData(sName,true,abLoop);
			if(pData==NULL){
				Error("Couldn't load music '%s'\n",sName.c_str());
				return NULL;
			}

			iSoundChannel *pStream = pData->CreateChannel(256);
			if(pStream == NULL)
			{
				Res(apRes)->GetSoundManager()->Destroy(pData);
				Error("Couldn't stream music '%s'!\n",sName.c_str());
				return NULL;
			}
			return pStream;
		},
		[](void*, OhplMusicSong *apSong, const unsigned char *asName, size_t alLen)
		{
			static_cast<cMusicEntry*>(apSong)->msFileName.assign((const char*)asName, alLen);
		},
		[](void*, void *apLock, const unsigned char *asName, size_t alLen, float afVolume, bool abLoop)
		{
			cMusicLock *pLock = static_cast<cMusicLock*>(apLock);
			pLock->msFileName.assign((const char*)asName, alLen);
			pLock->mfVolume = afVolume;
			pLock->mbLoop = abLoop;
		},
	};

	//-----------------------------------------------------------------------

	cMusicHandler::cMusicHandler(iLowLevelSound* apLowLevelSound, cResources* apResources)
	{
		mpLowLevelSound = apLowLevelSound;
		mpResources = apResources;
		mpRust = ohpl_music_new();
	}

	//-----------------------------------------------------------------------

	cMusicHandler::~cMusicHandler()
	{
		ohpl_music_free(mpRust, &gMusicHostOps, mpResources);
	}

	//-----------------------------------------------------------------------

	bool cMusicHandler::Play(const tString& asFileName,float afVolume, float afFadeStepSize, bool abLoop, bool abResume)
	{
		return ohpl_music_play(mpRust, &gMusicHostOps, mpResources, (const unsigned char*)asFileName.data(), asFileName.size(),
							   afVolume, afFadeStepSize, abLoop, abResume);
	}

	//-----------------------------------------------------------------------

	void cMusicHandler::Stop(float afFadeStepSize)
	{
		ohpl_music_stop(mpRust, &gMusicHostOps, mpResources, afFadeStepSize);
	}

	void cMusicHandler::Pause()
	{
		ohpl_music_set_paused(mpRust, &gMusicHostOps, mpResources, true);
	}

	void cMusicHandler::Resume()
	{
		ohpl_music_set_paused(mpRust, &gMusicHostOps, mpResources, false);
	}

	//-----------------------------------------------------------------------

	void cMusicHandler::Lock(cMusicLock* apLock)
	{
		ohpl_music_lock(mpRust, apLock);
	}

	void cMusicHandler::UnLock()
	{
		ohpl_music_lock(mpRust, NULL);
	}

	//-----------------------------------------------------------------------

	void cMusicHandler::FadeVolumeMul(float afDest, float afSpeed)
	{
		ohpl_music_fade_volume_mul(mpRust, afDest, afSpeed);
	}

	void cMusicHandler::SetVolumeMul(float afMul)
	{
		ohpl_music_set_volume_mul(mpRust, afMul);
	}

	//-----------------------------------------------------------------------

	tString cMusicHandler::GetCurrentSongName()
	{
		cMusicEntry *pSong = GetCurrentSong();
		return pSong ? pSong->msFileName : "";
	}

	float cMusicHandler::GetCurrentSongVolume()
	{
		cMusicEntry *pSong = GetCurrentSong();
		return pSong ? pSong->mfVolume : 0;
	}

	cMusicEntry* cMusicHandler::GetCurrentSong()
	{
		return static_cast<cMusicEntry*>(ohpl_music_main_song(mpRust));
	}

	//-----------------------------------------------------------------------

	void cMusicHandler::Update(float afTimeStep)
	{
		ohpl_music_update(mpRust, &gMusicHostOps, mpResources, afTimeStep);
	}

	void cMusicHandler::ResetResumeData()
	{
		ohpl_music_reset_resume(mpRust);
	}

	//-----------------------------------------------------------------------

}
