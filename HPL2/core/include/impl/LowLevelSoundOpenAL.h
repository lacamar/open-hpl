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
#ifndef HPL_LOWLEVELSOUND_OPENAL_H
#define HPL_LOWLEVELSOUND_OPENAL_H

#include "sound/LowLevelSound.h"
#include "impl/OpenALSoundData.h"
#include "impl/OpenALSoundEnvironment.h"
#include "impl/RustL5.h"

namespace hpl 
{
	class cSoundDeviceIdentifierOpenAL : public iSoundDeviceIdentifier
	{
	public:
		cSoundDeviceIdentifierOpenAL(int alID, const tString& asName, bool abDefault) : mlID(alID), msName(asName), mbDefault(abDefault) {}
		int GetID() { return mlID; }
		const tString& GetName() { return msName; }
		bool IsDefault() { return mbDefault; }

	private:
		int mlID;
		tString msName;
		bool mbDefault;
	};

	class cLowLevelSoundOpenAL : public iLowLevelSound
	{
	public:
		cLowLevelSoundOpenAL() : mlCurrentSoundDevID(-1) { OhplL5InstallLog(); mpRust = ohpl_l5_low_new(); }
		~cLowLevelSoundOpenAL();

		void GetSupportedFormats(tStringList &alstFormats){ alstFormats.push_back("WAV"); alstFormats.push_back("OGG"); }

		iSoundData* LoadSoundData(const tString& asName, const tWString& asFilePath, const tString&, bool abStream, bool abLoopStream)
		{
			cOpenALSoundData* pData = hplNew( cOpenALSoundData, (asName, abStream) );
			pData->SetLoopStream(abLoopStream);
			if(pData->CreateFromFile(asFilePath)) return pData;
			hplDelete(pData);
			return NULL;
		}

		void UpdateSound(float){ ohpl_l5_low_update(mpRust); }

		void SetListenerAttributes(const cVector3f &avPos, const cVector3f &avVel, const cVector3f &avForward, const cVector3f &avUp)
		{
			mvListenerPosition = avPos;
			mvListenerVelocity = avVel;
			mvListenerForward = avForward;
			mvListenerUp = avUp;
			ohpl_l5_low_set_listener_full(mpRust, avPos.v, avVel.v, avForward.v, avUp.v, mvListenerRight.v, &m_mtxListener.m[0][0]);
		}

		void SetListenerPosition(const cVector3f &avPos)
		{
			mvListenerPosition = avPos;
			cVector3f vNeg = mvListenerForward * -1;
			ohpl_l5_low_set_listener(mpRust, avPos.v, mvListenerVelocity.v, vNeg.v, mvListenerUp.v);
		}

		void SetSetRolloffFactor(float f){ ohpl_l5_low_set_rolloff(mpRust, f); }
		void SetListenerAttenuation(bool b){ mbListenerAttenuation = b; }
		void SetVolume(float f){ mfVolume = f; ohpl_l5_low_set_volume(mpRust, f); }

		void Init(int alSoundDeviceID, bool abUseEnvAudio, int alMaxChannels, int alStreamUpdateFreq, bool abUseThreading,
				bool abUseVoiceManagement, int alMaxMonoSourceHint, int alMaxStereoSourceHint,
				int alStreamingBufferSize, int alStreamingBufferCount, bool abEnableLowLevelLog)
		{
			ohpl_l5_low_prelude(abEnableLowLevelLog);
			std::vector<OhplL5Device> vAll, vValid;
			ToRust(GetAvailableSoundDevices(), vAll);
			ToRust(GetFilteredSoundDevices(), vValid);
			OhplL5InitArgs args = { alSoundDeviceID, abUseEnvAudio, alMaxChannels, alStreamUpdateFreq, abUseThreading,
				abUseVoiceManagement, alMaxMonoSourceHint, alMaxStereoSourceHint, alStreamingBufferSize, alStreamingBufferCount,
				abEnableLowLevelLog };
			OhplL5InitResult res = ohpl_l5_low_init(mpRust, &args, vAll.data(), vAll.size(), vValid.data(), vValid.size(),
													getenv("OPENHPL_SOUND_DEVICE"));
			if(!res.initialized) return;
			mlCurrentSoundDevID = res.device_id;
			mbEnvAudioEnabled = ohpl_l5_low_env_enabled(mpRust);
			mvListenerForward = cVector3f(0,0,1);
			mvListenerUp = cVector3f(0,1,0);
			SetVolume(1.0f);
		}

		void SetEnvVolume(float f)
		{
			float fOut;
			if(ohpl_l5_low_set_env_volume(mpRust, f, &fOut)) mfEnvVolume = fOut;
		}

		iSoundEnvironment* LoadSoundEnvironment(const tString &asFilePath)
		{
			if(!mbEnvAudioEnabled) return NULL;
			if(iSoundEnvironment *pOld = GetSoundEnvironmentFromFileName(asFilePath)) return pOld;
			cOpenALSoundEnvironment* pEnv = hplNew(cOpenALSoundEnvironment,());
			if(!pEnv->CreateFromFile(asFilePath)){ hplDelete(pEnv); return NULL; }
			pEnv->SetFileName(cString::ToLowerCase(asFilePath));
			mlstSoundEnv.push_back(pEnv);
			return pEnv;
		}

		void SetSoundEnvironment(iSoundEnvironment* apEnv)
		{
			if(mbEnvAudioEnabled) ohpl_l5_low_set_env(mpRust, Params(apEnv));
		}

		void FadeSoundEnvironment(iSoundEnvironment* apSrc, iSoundEnvironment* apDst, float afT)
		{
			if(mbEnvAudioEnabled) ohpl_l5_low_fade_env(mpRust, Params(apSrc), Params(apDst), afT);
		}

		iSoundDeviceIdentifier* GetCurrentSoundDevice()
		{
			if(mlCurrentSoundDevID < 0 || mlCurrentSoundDevID >= (int)mvSoundDevices.size()) return GetFirstDefaultDevice();
			return mvSoundDevices[mlCurrentSoundDevID];
		}

	private:
		static const OhplL5EnvParams* Params(iSoundEnvironment* apEnv){ return apEnv ? &((cOpenALSoundEnvironment*)apEnv)->Params() : NULL; }

		static void ToRust(const tSoundDeviceVec& avDevices, std::vector<OhplL5Device>& avOut)
		{
			for(iSoundDeviceIdentifier* pDev : avDevices)
				avOut.push_back({ pDev->GetID(), (const uint8_t*)pDev->GetName().data(), pDev->GetName().size(), pDev->IsDefault() });
		}

		iSoundDeviceIdentifier* GetFirstDefaultDevice()
		{
			std::vector<OhplL5Device> vAll, vValid;
			ToRust(GetAvailableSoundDevices(), vAll);
			ToRust(GetFilteredSoundDevices(), vValid);
			int lId = ohpl_l5_first_default(vAll.data(), vAll.size(), vValid.data(), vValid.size());
			for(iSoundDeviceIdentifier* pDev : GetAvailableSoundDevices())
				if(pDev->GetID() == lId) return pDev;
			return NULL;
		}

		OhplL5Low* mpRust;
		int mlCurrentSoundDevID;
	};
};
#endif // HPL_LOWLEVELSOUND_OPENAL_H
