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

		if(OhplBackendRustRequested())
		{
			OhplL5InstallLog();
			for(int i=0;i<3;i++)
			{
				mfPosition[i] = 0;
				mfVelocity[i] = 0;
			}
			mpRust = ohpl_l5_chan_new(mlChannel);
			return;
		}

        for(int i=0;i<3;i++)
		{
			mfPosition[i] = 0;
			mfVelocity[i] = 0;
		}

		OAL_Source_SetAttributes ( mlChannel, mfPosition, mfVelocity );
		OAL_Source_SetFilterType(mlChannel, eOALFilterType_BandPass);
		OAL_Source_SetSpatialize(mlChannel, false);
//		SetAffectedByEnv(true);
//		SetFilterGainHF(0.01f);
		
//		SetFiltering(true,0x3);
//		OAL_Source_SetMinMaxDistance ( mlChannel, 100000.0f, 200000.f );
	}
	
	//-----------------------------------------------------------------------

	cOpenALSoundChannel::~cOpenALSoundChannel()
	{
		if(mpRust)
			ohpl_l5_chan_free(mpRust);
		else if(mlChannel>=0)
			OAL_Source_Stop ( mlChannel );

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
		if(mpRust) ohpl_l5_chan_stop(mpRust);
		else OAL_Source_Stop ( mlChannel );
		mlChannel = -1;

		mbStopUsed = true;
	}
	
	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetPaused(bool abX)
	{
//		Log("Setting %s %spaused! - Source %d\n",mpData->GetName().c_str(),abX?"":"un", mlChannel);
		
		mbPaused = abX;
		if(mpRust) ohpl_l5_chan_set_paused(mpRust, abX);
		else OAL_Source_SetPaused ( mlChannel, abX );

//		if(mbPaused)
//		{
			//Log("Setting %s paused!\n",mpData->GetName().c_str());
//		}
	}
	
	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetSpeed(float afSpeed)
	{
		mfSpeed = afSpeed;
		
		if(mpRust) ohpl_l5_chan_set_pitch(mpRust, afSpeed);
		else OAL_Source_SetPitch ( mlChannel, afSpeed );
	}
	
	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetVolume(float afVolume)
	{
		mfVolume = afVolume;
		
		if(mpRust) ohpl_l5_chan_set_volume(mpRust, mfVolume);
		else OAL_Source_SetVolume(mlChannel, mfVolume);
	}
	
	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetLooping (bool abLoop)
	{
		mbLooping = abLoop;

		if(mpRust) ohpl_l5_chan_set_loop(mpRust, abLoop);
		else OAL_Source_SetLoop ( mlChannel, abLoop );
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
		if(mpRust) ohpl_l5_chan_set_spatialize(mpRust, ab3D && !mbPositionRelative);
		else OAL_Source_SetSpatialize(mlChannel, ab3D && !mbPositionRelative);
	}
	
	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetPositionIsRelative(bool abRelative)
	{
		mbPositionRelative = abRelative;
		if(mpRust) ohpl_l5_chan_set_spatialize(mpRust, mb3D && !abRelative);
		else OAL_Source_SetSpatialize(mlChannel, mb3D && !abRelative);
	}

	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetPosition(const cVector3f &avPos)
	{
		mvPosition = avPos;

		if(mpRust) ohpl_l5_chan_set_attributes(mpRust, mvPosition.v, mvVelocity.v);
		else OAL_Source_SetAttributes ( mlChannel, mvPosition.v, mvVelocity.v );
	}
	
	//-----------------------------------------------------------------------

	void cOpenALSoundChannel::SetVelocity(const cVector3f &avVel)
	{
		mvVelocity = avVel;
		
		if(mpRust) ohpl_l5_chan_set_attributes(mpRust, mvPosition.v, mvVelocity.v);
		else OAL_Source_SetAttributes ( mlChannel, mvPosition.v, mvVelocity.v );
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
		if(mpRust) return ohpl_l5_chan_is_playing(mpRust);
		return OAL_Source_IsPlaying( mlChannel );
	}
	

	//-----------------------------------------------------------------------

	bool cOpenALSoundChannel::IsBufferUnderrun()
	{ 
		if(mpRust) return ohpl_l5_chan_is_underrun(mpRust);
		return OAL_Source_IsBufferUnderrun(mlChannel);
	}
	double cOpenALSoundChannel::GetElapsedTime()
	{ 
		if(mpRust) return ohpl_l5_chan_elapsed(mpRust);
		return OAL_Source_GetElapsedTime(mlChannel);
	}
	double cOpenALSoundChannel::GetTotalTime()
	{ 
		if(mpRust) return ohpl_l5_chan_total(mpRust);
		return OAL_Source_GetTotalTime(mlChannel);
	}
	void cOpenALSoundChannel::SetElapsedTime(double afTime)
	{
		if(mpRust) { ohpl_l5_chan_set_elapsed(mpRust, afTime); return; }
		if(afTime < 0) afTime =0;
		if(afTime > GetTotalTime()) afTime = GetTotalTime() - 0.0001;

		OAL_Source_SetElapsedTime(mlChannel, afTime);
	}

	//-----------------------------------------------------------------------
	
	void cOpenALSoundChannel::SetPriority(int alX)
	{
		if(mpRust) { ohpl_l5_chan_set_priority(mpRust, alX, mlPriorityModifier); return; }
		int lPrio = alX+mlPriorityModifier;
		if(lPrio>255)lPrio = 255;

		OAL_Source_SetPriority ( mlChannel, lPrio );
	}
	
	//-----------------------------------------------------------------------
	
	int cOpenALSoundChannel::GetPriority()
	{
		if(mpRust) return ohpl_l5_chan_priority(mpRust);
		return OAL_Source_GetPriority ( mlChannel );
	}
	
	//-----------------------------------------------------------------------

	void cOpenALSoundChannel::SetAffectedByEnv(bool abAffected)
	{
//		if (!(gpGame->GetSound()->GetLowLevel()->IsEnvAudioAvailable()))
//			return;
        
		iSoundChannel::SetAffectedByEnv(abAffected);

		if(mpRust) { ohpl_l5_chan_set_aux_slot(mpRust, mbAffectedByEnv); return; }

		if (mbAffectedByEnv)
			OAL_Source_SetAuxSendSlot(mlChannel,0,0);
		else
			OAL_Source_SetAuxSendSlot(mlChannel,0,-1);
	}

	void cOpenALSoundChannel::SetFiltering(bool abEnabled, int alFlags)
	{
//		if (!(gpGame->GetSound()->GetLowLevel()->IsEnvAudioAvailable()))
//			return;
        
		if(mpRust) ohpl_l5_chan_set_filtering(mpRust, abEnabled, alFlags);
		else OAL_Source_SetFiltering(mlChannel,abEnabled, alFlags);
	}

	void cOpenALSoundChannel::SetFilterGain(float afGain)
	{
//		if (!(gpGame->GetSound()->GetLowLevel()->IsEnvAudioAvailable()))
//			return;
        
		if(mpRust) ohpl_l5_chan_set_filter_gain(mpRust, afGain);
		else OAL_Source_SetFilterGain(mlChannel, afGain);
	}
	
	void cOpenALSoundChannel::SetFilterGainHF( float afGainHF)
	{
//		if (!(gpGame->GetSound()->GetLowLevel()->IsEnvAudioAvailable()))
//			return;
        
		if(mpRust) ohpl_l5_chan_set_filter_gain_hf(mpRust, afGainHF);
		else OAL_Source_SetFilterGainHF(mlChannel, afGainHF);
	}

	void cOpenALSoundChannel::SetFilterGainLF(float afGainLF)
	{
		if(mpRust) ohpl_l5_chan_set_filter_gain_lf(mpRust, afGainLF);
		else OAL_Source_SetFilterGainLF(mlChannel, afGainLF);
	}

	void cOpenALSoundChannel::SetEnvSendGain(float afGain)
	{
		if(mpRust) ohpl_l5_chan_set_env_send_gain(mpRust, afGain);
		else OAL_Source_SetAuxSendGain(mlChannel, afGain);
	}

}
