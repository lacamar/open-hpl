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

#ifndef HPL_SOUND_CHANNEL_H
#define HPL_SOUND_CHANNEL_H

#include <list>
#include "sound/SoundData.h"
#include "math/MathTypes.h"


namespace hpl {
	class iSoundData;
	class cSoundManager;
	
	// Rust-owned (rust/src/backend/media/snd.rs ChanShadow); layouts must match
	struct OhplL5ChanShadow
	{
		float mfSpeed, mfVolume, mfPan, mfMaxDistance, mfMinDistance, mfBlockVolumeMul;
		cVector3f mvVelocity, mvPosition, mvRelPosition;
		int mlPriorityModifier;
		bool mbLooping, mbPaused, mbPositionRelative, mb3D, mbBlockable, mbAffectedByEnv, mbStopUsed;
	};
	static_assert(sizeof(OhplL5ChanShadow) == 72, "OhplL5ChanShadow");

	//--------------------------------------------

	class iSoundChannel
	{
	public:
		iSoundChannel(iSoundData* apData,cSoundManager* apSoundManger) : mS(NULL)
		{
			mpData = apData;
			mpData->IncUserCount();
			mpSoundManger = apSoundManger;
		}
		virtual ~iSoundChannel(){}

		virtual void Play()=0;
		virtual void Stop()=0;
		
		virtual void SetPaused(bool abX)=0;
		virtual void SetSpeed(float afSpeed)=0;
		virtual void SetVolume (float afVolume)=0; 
		virtual void SetLooping (bool abLoop)=0;
		virtual void SetPan (float afPan)=0;
		virtual void Set3D(bool ab3D)=0;

		virtual void SetPriority(int alX)=0;
		virtual int GetPriority()=0;

		void SetPriorityModifier(int alX){ 
			mS->mlPriorityModifier = alX;
			SetPriority(GetPriority());
		}
		int GetPriorityModifier(){ return mS->mlPriorityModifier;}

		virtual void SetPositionIsRelative(bool abRelative)=0;
		virtual void SetPosition(const cVector3f &avPos)=0;
		void SetRelPosition(const cVector3f &avPos){ mS->mvRelPosition = avPos;}

		virtual void SetVelocity(const cVector3f &avVel)=0;
		
		virtual void SetMinDistance(float fMin)=0;
		virtual void SetMaxDistance(float fMax)=0;

		virtual bool IsPlaying()=0;
		virtual bool IsBufferUnderrun()=0;
		virtual double GetElapsedTime()=0;
		virtual double GetTotalTime()=0;
		virtual void SetElapsedTime(double afTime)=0;

		bool GetPaused(){return mS->mbPaused;}
		float GetSpeed(){return mS->mfSpeed;}
		float GetVolume (){return mS->mfVolume;}
		bool GetLooping (){return mS->mbLooping;}
		float GetPan (){return mS->mfPan;}
		bool Get3D (){return mS->mb3D;}

		bool GetStopUsed(){ return mS->mbStopUsed;}

		bool GetBlockable(){ return mS->mbBlockable;}
		void SetBlockable(bool abX){ mS->mbBlockable = abX;}
		void SetBlockVolumeMul(float afX){ mS->mfBlockVolumeMul = afX;}
		float GetBlockVolumeMul(){ return mS->mfBlockVolumeMul;}

		bool GetPositionIsRelative(){return mS->mbPositionRelative;}
		
		const cVector3f& GetRelPosition(){ return mS->mvRelPosition;}
		const cVector3f& GetPosition(){ return mS->mvPosition;}
		const cVector3f& GetVelocity (){ return mS->mvVelocity;}
		
		float GetMinDistance(){ return mS->mfMinDistance;}
		float GetMaxDistance(){ return mS->mfMaxDistance;}

		iSoundData* GetData(){return mpData;}

		//virtual void SetFiltering ( iFilter* apFilter, int alFlags ) = 0;
		virtual void SetAffectedByEnv ( bool abAffected )  { mS->mbAffectedByEnv = abAffected; }
		virtual void SetFiltering ( bool abEnabled, int alFlags ) = 0;
		virtual void SetFilterGain(float afGain) =0;
		virtual void SetFilterGainHF(float afGainHF)=0;
		virtual void SetFilterGainLF(float afGainLF)=0;
		virtual void SetEnvSendGain(float afGain)=0;
		
	protected:
		void DestroyData();

		iSoundData* mpData;
		cSoundManager* mpSoundManger;

		OhplL5ChanShadow* mS;
	};

	typedef std::list<iSoundChannel*> tSoundChannelList;
	typedef tSoundChannelList::iterator tSoundChannelListIt;
};
#endif // HPL_SOUND_CHANNEL_H
