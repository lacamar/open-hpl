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

#include "impl/OpenALSoundChannel.h"
#include "impl/OpenALSoundData.h"
#include "resources/SoundManager.h"

#include "math/Math.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cOpenALSoundChannel::cOpenALSoundChannel(iSoundData* apData, int alChannel,cSoundManager* apSoundManger)
	: iSoundChannel(apData, apSoundManger)
	{
		mlChannel = alChannel;
		mpRust = NULL;

		OhplL5InstallLog();
		for(int i=0;i<3;i++)
		{
			mfPosition[i] = 0;
			mfVelocity[i] = 0;
		}
		mpRust = ohpl_l5_chan_new(mlChannel);
	}
	
	//-----------------------------------------------------------------------

	cOpenALSoundChannel::~cOpenALSoundChannel()
	{
		ohpl_l5_chan_free(mpRust);

		DestroyData();
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::Play()
	{
		//Log("Playing %s - Source %d Ref %d\n",mpData->GetName().c_str(), mlChannel & 0xFFF, (mlChannel & 0x7ffff000) >> 12);

		SetPaused(false);

		mbStopUsed = false;
	}
	
	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::Stop()
	{
		//Log("Stopping %s - Source %d Ref %d\n",mpData->GetName().c_str(), mlChannel & 0xFFF, (mlChannel & 0x7ffff000) >> 12);
		ohpl_l5_chan_stop(mpRust);
		mlChannel = -1;

		mbStopUsed = true;
	}
	
	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetPaused(bool abX)
	{
//		Log("Setting %s %spaused! - Source %d\n",mpData->GetName().c_str(),abX?"":"un", mlChannel);
		
		mbPaused = abX;
		ohpl_l5_chan_set_paused(mpRust, abX);

//		if(mbPaused)
//		{
			//Log("Setting %s paused!\n",mpData->GetName().c_str());
//		}
	}
	
	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetSpeed(float afSpeed)
	{
		mfSpeed = afSpeed;
		
		ohpl_l5_chan_set_pitch(mpRust, afSpeed);
	}
	
	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetVolume(float afVolume)
	{
		mfVolume = afVolume;
		
		ohpl_l5_chan_set_volume(mpRust, mfVolume);
	}
	
	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetLooping (bool abLoop)
	{
		mbLooping = abLoop;

		ohpl_l5_chan_set_loop(mpRust, abLoop);
	}

	//-----------------------------------------------------------------------

	void cOpenALSoundChannel::SetPan (float afPan)
	{
		//Log("Pan: %d\n", lPan);
//		cVector3f vPosition = mvPosition;
//		vPosition.x = 
//		OAL_Source_SetAttributes ( mlChannel, mvPosition.
//		FSOUND_SetPan(mlChannel, lPan);
	}
	
	//-----------------------------------------------------------------------

	void cOpenALSoundChannel::Set3D(bool ab3D)
	{
		mb3D = ab3D;
		ohpl_l5_chan_set_spatialize(mpRust, ab3D && !mbPositionRelative);
	}
	
	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetPositionIsRelative(bool abRelative)
	{
		mbPositionRelative = abRelative;
		ohpl_l5_chan_set_spatialize(mpRust, mb3D && !abRelative);
	}

	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetPosition(const cVector3f &avPos)
	{
		mvPosition = avPos;

		ohpl_l5_chan_set_attributes(mpRust, mvPosition.v, mvVelocity.v);
	}
	
	//-----------------------------------------------------------------------

	void cOpenALSoundChannel::SetVelocity(const cVector3f &avVel)
	{
		mvVelocity = avVel;
		
		ohpl_l5_chan_set_attributes(mpRust, mvPosition.v, mvVelocity.v);
	}
	
	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetMinDistance(float afMin)
	{
		mfMinDistance = afMin;
	}

	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetMaxDistance(float afMax)
	{
		mfMaxDistance = afMax;
	}
	//-----------------------------------------------------------------------
	
	bool cOpenALSoundChannel::IsPlaying()
	{
		return ohpl_l5_chan_is_playing(mpRust);
	}
	

	//-----------------------------------------------------------------------

	bool cOpenALSoundChannel::IsBufferUnderrun()
	{ 
		return ohpl_l5_chan_is_underrun(mpRust);
	}
	double cOpenALSoundChannel::GetElapsedTime()
	{ 
		return ohpl_l5_chan_elapsed(mpRust);
	}
	double cOpenALSoundChannel::GetTotalTime()
	{ 
		return ohpl_l5_chan_total(mpRust);
	}
	void cOpenALSoundChannel::SetElapsedTime(double afTime)
	{
		ohpl_l5_chan_set_elapsed(mpRust, afTime);
	}

	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetPriority(int alX)
	{
		ohpl_l5_chan_set_priority(mpRust, alX, mlPriorityModifier);
	}
	
	//-----------------------------------------------------------------------
	
	int cOpenALSoundChannel::GetPriority()
	{
		return ohpl_l5_chan_priority(mpRust);
	}
	
	//-----------------------------------------------------------------------

	void cOpenALSoundChannel::SetAffectedByEnv(bool abAffected)
	{
//		if (!(gpGame->GetSound()->GetLowLevel()->IsEnvAudioAvailable()))
//			return;
        
		iSoundChannel::SetAffectedByEnv(abAffected);

		ohpl_l5_chan_set_aux_slot(mpRust, mbAffectedByEnv);
	}

	void cOpenALSoundChannel::SetFiltering(bool abEnabled, int alFlags)
	{
//		if (!(gpGame->GetSound()->GetLowLevel()->IsEnvAudioAvailable()))
//			return;
        
		ohpl_l5_chan_set_filtering(mpRust, abEnabled, alFlags);
	}

	void cOpenALSoundChannel::SetFilterGain(float afGain)
	{
//		if (!(gpGame->GetSound()->GetLowLevel()->IsEnvAudioAvailable()))
//			return;
        
		ohpl_l5_chan_set_filter_gain(mpRust, afGain);
	}
	
	void cOpenALSoundChannel::SetFilterGainHF( float afGainHF)
	{
//		if (!(gpGame->GetSound()->GetLowLevel()->IsEnvAudioAvailable()))
//			return;
        
		ohpl_l5_chan_set_filter_gain_hf(mpRust, afGainHF);
	}

	void cOpenALSoundChannel::SetFilterGainLF(float afGainLF)
	{
		ohpl_l5_chan_set_filter_gain_lf(mpRust, afGainLF);
	}

	void cOpenALSoundChannel::SetEnvSendGain(float afGain)
	{
		ohpl_l5_chan_set_env_send_gain(mpRust, afGain);
	}

}
