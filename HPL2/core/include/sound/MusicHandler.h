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

#ifndef HPL_MUSICHANDLER_H
#define HPL_MUSICHANDLER_H

#include "system/SystemTypes.h"
#include "sound/RustSound.h"

namespace hpl {

	class iLowLevelSound;
	class cResources;

	class cMusicLock
	{
	public:
		cMusicLock() : msFileName(""), mfVolume(0) {}

		tString msFileName;
		float mfVolume;
		bool mbLoop;
	};

	/** The current song, a Rust record: the header fields are the ones the game reads and writes. */
	class cMusicEntry : public OhplMusicSong
	{
	public:
		tString GetFileName(){ size_t n; const unsigned char *p = ohpl_music_song_name(this, &n); return tString((const char*)p, n); }
	private:
		cMusicEntry();
	};

	class cMusicHandler
	{
	public:
		cMusicHandler(iLowLevelSound* apLowLevelSound, cResources* apResources) : mpRust(ohpl_music_new(OhplSndHostTable(), apResources)) {}
		~cMusicHandler(){ ohpl_music_free(mpRust); }

		/**
		 * Play a song. Playing a song that is already playing updates it's properties.
		 * \param asFileName file to be played
		 * \param afVolume volume to be played at
		 * \param afFadeStepSize volume increse/decrease per app step when fading to new volume.
		 * \param abLoop If the music should be looped or not.
		 * \return
		 */
		bool Play(const tString& asFileName,float afVolume, float afFadeStepSize, bool abLoop, bool abResume)
		{
			return ohpl_music_play(mpRust, (const unsigned char*)asFileName.data(), asFileName.size(), afVolume, afFadeStepSize, abLoop, abResume);
		}

		/**
		 * Stop playing the current music.
		 * \param afFadeStepSize volume increse/decrease per app step when fading volume to 0.
		 */
		void Stop(float afFadeStepSize){ ohpl_music_stop(mpRust, afFadeStepSize); }
		void Pause(){ ohpl_music_set_paused(mpRust, true); }
		void Resume(){ ohpl_music_set_paused(mpRust, false); }

		void FadeVolumeMul(float afDest, float afSpeed){ ohpl_music_fade_volume_mul(mpRust, afDest, afSpeed); }
		void SetVolumeMul(float afMul){ ohpl_music_set_volume_mul(mpRust, afMul); }
		float GetVolumeMul(){ return ohpl_music_volume_mul(mpRust);}

		/**
		 * No more music can be played when locked. Latest song that has been tried to be palyed is saved in lock.
		 * \param apLock
		 */
		void Lock(cMusicLock* apLock){ ohpl_music_lock(mpRust, apLock); }
		/**
		 * Remove lock.
		 */
		void UnLock(){ ohpl_music_lock(mpRust, NULL); }
		tString GetCurrentSongName(){ cMusicEntry *pSong = GetCurrentSong(); return pSong ? pSong->GetFileName() : tString(""); }
		float GetCurrentSongVolume(){ cMusicEntry *pSong = GetCurrentSong(); return pSong ? pSong->mfVolume : 0; }

		cMusicEntry* GetCurrentSong(){ return static_cast<cMusicEntry*>(ohpl_music_main_song(mpRust)); }

		void Update(float afTimeStep){ ohpl_music_update(mpRust, afTimeStep); }

		void ResetResumeData(){ ohpl_music_reset_resume(mpRust); }

	private:
		OhplMusic* mpRust;
	};

};
#endif // HPL_MUSICHANDLER_H
