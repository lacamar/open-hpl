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
#ifndef HPL_OPENAL_SOUND_CHANNEL_H
#define HPL_OPENAL_SOUND_CHANNEL_H

#include "sound/SoundChannel.h"
#include "sound/SoundData.h"
#include "impl/RustL5.h"

namespace hpl {

	class cOpenALSoundChannel : public iSoundChannel
	{
	public:
		cOpenALSoundChannel(iSoundData* apData, int alChannel, cSoundManager* apSoundManger)
			: iSoundChannel(apData, apSoundManger) { OhplL5InstallLog(); mpRust = ohpl_l5_chan_new(alChannel); }
		~cOpenALSoundChannel();

		void Play(){ SetPaused(false); mbStopUsed = false; }
		void Stop(){ ohpl_l5_chan_stop(mpRust); mbStopUsed = true; }

		void SetPaused(bool b){ mbPaused = b; ohpl_l5_chan_set_paused(mpRust, b); }
		void SetSpeed(float f){ mfSpeed = f; ohpl_l5_chan_set_pitch(mpRust, f); }
		void SetVolume(float f){ mfVolume = f; ohpl_l5_chan_set_volume(mpRust, f); }
		void SetLooping(bool b){ mbLooping = b; ohpl_l5_chan_set_loop(mpRust, b); }
		void SetPan(float){}
		void Set3D(bool b){ mb3D = b; ohpl_l5_chan_set_spatialize(mpRust, b && !mbPositionRelative); }

		void SetPriority(int i){ ohpl_l5_chan_set_priority(mpRust, i, mlPriorityModifier); }
		int GetPriority(){ return ohpl_l5_chan_priority(mpRust); }

		void SetPositionIsRelative(bool b){ mbPositionRelative = b; ohpl_l5_chan_set_spatialize(mpRust, mb3D && !b); }
		void SetPosition(const cVector3f &v){ mvPosition = v; ohpl_l5_chan_set_attributes(mpRust, v.v, mvVelocity.v); }
		void SetVelocity(const cVector3f &v){ mvVelocity = v; ohpl_l5_chan_set_attributes(mpRust, mvPosition.v, v.v); }

		void SetMinDistance(float f){ mfMinDistance = f; }
		void SetMaxDistance(float f){ mfMaxDistance = f; }

		bool IsPlaying(){ return ohpl_l5_chan_is_playing(mpRust); }
		bool IsBufferUnderrun(){ return ohpl_l5_chan_is_underrun(mpRust); }
		double GetElapsedTime(){ return ohpl_l5_chan_elapsed(mpRust); }
		double GetTotalTime(){ return ohpl_l5_chan_total(mpRust); }
		void SetElapsedTime(double t){ ohpl_l5_chan_set_elapsed(mpRust, t); }

		void SetAffectedByEnv(bool b){ iSoundChannel::SetAffectedByEnv(b); ohpl_l5_chan_set_aux_slot(mpRust, mbAffectedByEnv); }
		void SetFiltering(bool b, int flags){ ohpl_l5_chan_set_filtering(mpRust, b, flags); }
		void SetFilterGain(float f){ ohpl_l5_chan_set_filter_gain(mpRust, f); }
		void SetFilterGainHF(float f){ ohpl_l5_chan_set_filter_gain_hf(mpRust, f); }
		void SetFilterGainLF(float f){ ohpl_l5_chan_set_filter_gain_lf(mpRust, f); }
		void SetEnvSendGain(float f){ ohpl_l5_chan_set_env_send_gain(mpRust, f); }

	private:
		OhplL5Channel* mpRust;
	};
};
#endif // HPL_OPENAL_CHANNEL_H
