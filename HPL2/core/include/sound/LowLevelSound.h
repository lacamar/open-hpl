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

#ifndef HPL_LOWLEVELSOUND_H
#define HPL_LOWLEVELSOUND_H

#include "system/SystemTypes.h"
#include "math/MathTypes.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "sound/SoundEnvironment.h"

namespace hpl {

	class iSoundData;
	class iSoundEnvironment;
	//class iSoundFilter;

	typedef std::list<iSoundEnvironment*> tSoundEnvList;
	typedef tSoundEnvList::iterator tSoundEnvListIt;

	//---------------------------------------

	class iSoundDeviceIdentifier
	{
	public:
		virtual ~iSoundDeviceIdentifier(){}
		
		virtual int	GetID()=0;
		virtual const tString& GetName()=0;
		virtual bool IsDefault()=0;
	};

	//---------------------------------------

	typedef std::vector<iSoundDeviceIdentifier*> tSoundDeviceVec;

	//---------------------------------------
	
	class iLowLevelSound
	{
	public:
		iLowLevelSound() : mfVolume(1), mfEnvVolume(0), mbListenerAttenuation(true), mbHardwareAcc(false), mbEnvAudioEnabled(false) {}
		virtual ~iLowLevelSound()
		{
			STLDeleteAll(mvSoundDevices);
			STLDeleteAll(mlstSoundEnv);
		}
		
		/**
		 * Get the formats supported
		 * \param &alstFormats 
		 */
		virtual void GetSupportedFormats(tStringList &alstFormats)=0;
		
		virtual iSoundData* LoadSoundData(const tString& asName,const tWString& asFilePath,
											const tString& asType, bool abStream,bool abLoopStream)=0;

		virtual void UpdateSound(float afTimeStep)=0;

		virtual void SetListenerAttributes (const cVector3f &avPos,const cVector3f &avVel,
										const cVector3f &avForward,const cVector3f &avUp)=0;
		virtual void SetListenerPosition(const cVector3f &avPos)=0;
		const cMatrixf& GetListenerMatrix(){ return m_mtxListener;}

		virtual void SetListenerAttenuation (bool abEnabled)=0;

		virtual void SetSetRolloffFactor(float afFactor)=0;

		virtual void SetVolume(float afVolume)=0;
		
		cVector3f& GetListenerPosition(){ return mvListenerPosition;}
		cVector3f& GetListenerVelocity(){ return mvListenerVelocity;}
		cVector3f& GetListenerForward() { return mvListenerForward;}
		cVector3f& GetListenerUp() { return mvListenerUp; }
		bool GetListenerAttenuation(){ return mbListenerAttenuation;}
		
		float GetVolume(){ return mfVolume;}

		//virtual void LogSoundStatus() {}
		virtual void Init(int alSoundDeviceID, bool abUseEnvAudio,int alMaxChannels, 
					int alStreamUpdateFreq, bool abUseThreading, bool abUseVoiceManagement,
					int alMaxMonoSourceHint, int alMaxStereoSourceHint,
					int alStreamingBufferSize, int alStreamingBufferCount, bool abEnableLowLevelLog)=0;
		
		bool IsHardwareAccelerated ()	{ return mbHardwareAcc; }
		bool IsEnvAudioAvailable ()		{ return mbEnvAudioEnabled; }

		virtual void SetEnvVolume(float afVolume)=0;
		float GetEnvVolume () { return mfEnvVolume; }

		virtual iSoundEnvironment* LoadSoundEnvironment (const tString& asFilePath)=0;

		virtual void SetSoundEnvironment ( iSoundEnvironment* apSoundEnv )=0;
		virtual void FadeSoundEnvironment( iSoundEnvironment* apSourceSoundEnv, iSoundEnvironment* apDestSoundEnv, float afT )=0;

		iSoundEnvironment* GetSoundEnvironmentFromFileName (const tString& asName)
		{
			tString sLowName = cString::ToLowerCase(asName);
			for (tSoundEnvListIt SEIt = mlstSoundEnv.begin(); SEIt != mlstSoundEnv.end(); ++SEIt)
			{
				iSoundEnvironment* pSoundEnv = *SEIt;
				if(sLowName == pSoundEnv->GetFileName()) return pSoundEnv;
			}
			return NULL;
		}
		//void DestroySoundEnvironment( iSoundEnvironment* apSoundEnv);

		virtual iSoundDeviceIdentifier* GetCurrentSoundDevice()=0;

		//static tStringVec GetAvailableSoundDevices();
		static void SetSoundDeviceNameFilter(const tString& asFilter) { mbSoundDeviceNameFilterChanged = true; msSoundDeviceNameFilter = asFilter; }
		static void PopulateAvailableSoundDevices(tSoundDeviceVec& avSoundDeviceVec);
		static const tSoundDeviceVec& GetAvailableSoundDevices()
		{
			if(mvSoundDevices.empty()) PopulateAvailableSoundDevices(mvSoundDevices);
			return mvSoundDevices;
		}
		static const tSoundDeviceVec& GetFilteredSoundDevices()
		{
			if(mbSoundDeviceNameFilterChanged)
			{
				mbSoundDeviceNameFilterChanged = false;
				mvFilteredSoundDevices.clear();

				GetAvailableSoundDevices();

				tString sLowCaseFilter = cString::ToLowerCase(msSoundDeviceNameFilter);
				for(int i=0;i<(int)mvSoundDevices.size();++i)
				{
					iSoundDeviceIdentifier* pSndDev = mvSoundDevices[i];
					tString sLowCaseName = cString::ToLowerCase(pSndDev->GetName());
					if(cString::GetFirstStringPos(sLowCaseName, sLowCaseFilter)!=-1) mvFilteredSoundDevices.push_back(pSndDev);
				}
			}
			return mvFilteredSoundDevices;
		}

	protected:
		float mfVolume;
		float mfEnvVolume;
		bool mbListenerAttenuation;

		bool mbHardwareAcc;
		bool mbEnvAudioEnabled;
		
		cVector3f mvListenerUp;
		cVector3f mvListenerForward;
		cVector3f mvListenerRight;
		cVector3f mvListenerPosition;
		cVector3f mvListenerVelocity;

		cMatrixf m_mtxListener;

		static inline tSoundDeviceVec mvSoundDevices;
		static inline tString msSoundDeviceNameFilter = "";
		static inline bool mbSoundDeviceNameFilterChanged = true;
		static inline tSoundDeviceVec mvFilteredSoundDevices;

		tSoundEnvList mlstSoundEnv;
	};
};
#endif // HPL_LOWLEVELSOUND_H
