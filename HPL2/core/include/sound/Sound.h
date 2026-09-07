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

#ifndef HPL_SOUND_H
#define HPL_SOUND_H

#include "engine/Updateable.h"

namespace hpl {
	
	class iLowLevelSound;
	class cResources;
	class cSoundHandler;
	class cMusicHandler;

	class cSound : public iUpdateable
	{
	public:
		cSound(iLowLevelSound *apLowLevelSound);
		~cSound();

		void Init(	cResources *apResources, int alSoundDeviceID, bool abUseEnvAudio, int alMaxChannels, 
						int alStreamUpdateFreq, bool abUseThreading, bool abUseVoiceManagement,
						int alMaxMonoSourceHint, int alMaxStereoSourceHint,
						int alStreamingBufferSize, int alStreamingBufferCount, bool abEnableLowLevelLog);

		void Update(float afTimeStep);

		// Shared engine-wide fix (applies to every game module, not just one): the
		// window/input-focus tracking in cEngine::CheckAndBroadcastFocusChange() already
		// fires these on every real focus transition (SDL_WINDOW_INPUT_FOCUS gained/lost)
		// via cUpdater::RunMessage() to every globally-registered iUpdateable, and cSound
		// is one (see cEngine::Init()'s AddGlobalUpdate(mpSound)) - so overriding these two
		// is enough to silence/restore all audio on alt-tab with no new event plumbing.
		void AppLostInputFocus();
		void AppGotInputFocus();

		iLowLevelSound* GetLowLevel(){ return mpLowLevelSound;}
		cSoundHandler* GetSoundHandler(){ return mpSoundHandler; }
		cMusicHandler* GetMusicHandler(){ return mpMusicHandler; }

	private:
		iLowLevelSound *mpLowLevelSound;
		cResources* mpResources;
		cSoundHandler* mpSoundHandler;
		cMusicHandler* mpMusicHandler;

		bool mbMutedByFocusLoss;
		float mfPreFocusMuteVolume;
	};

};
#endif // HPL_SOUND_H
