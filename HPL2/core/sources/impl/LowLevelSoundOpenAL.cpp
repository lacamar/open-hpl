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


#include "impl/LowLevelSoundOpenAL.h"
#include "system/String.h"
#include "impl/OpenALSoundData.h"
#include "impl/OpenALSoundEnvironment.h"

#include "math/Math.h"

#include "system/LowLevelSystem.h"

namespace hpl {

	//-----------------------------------------------------------------------

	cSoundDeviceIdentifierOpenAL::cSoundDeviceIdentifierOpenAL(int alID, const tString& asName, bool abDefault)
	{
		mlID = alID;
		msName = asName;
		mbDefault = abDefault;
	}

	//-----------------------------------------------------------------------

	static void OhplL5DeviceSink(void* apCtx, int32_t alId, const uint8_t* apName, size_t alLen, bool abDefault)
	{
		tSoundDeviceVec* pVec = (tSoundDeviceVec*)apCtx;
		pVec->push_back(hplNew(cSoundDeviceIdentifierOpenAL,(alId, tString((const char*)apName, alLen), abDefault)));
	}

	void iLowLevelSound::PopulateAvailableSoundDevices(tSoundDeviceVec& avSoundDeviceVec)
	{
		STLDeleteAll(avSoundDeviceVec);

		OhplL5InstallLog();
		ohpl_l5_devices(OhplL5DeviceSink, &avSoundDeviceVec);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cLowLevelSoundOpenAL::cLowLevelSoundOpenAL()
	{
		mvFormats[0] = "WAV";
		mvFormats[1] = "OGG";
		mvFormats[2] = "";
		mbInitialized = false;
		mbEnvAudioEnabled = false;
		mlCurrentSoundDevID = -1;
		mpRust = NULL;
		OhplL5InstallLog();
		mpRust = ohpl_l5_low_new();
	}

	//-----------------------------------------------------------------------

	cLowLevelSoundOpenAL::~cLowLevelSoundOpenAL()
	{
		ohpl_l5_low_free(mpRust);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHOD
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iSoundData* cLowLevelSoundOpenAL::LoadSoundData(const tString& asName, const tWString& asFilePath,
												const tString& asType, bool abStream,bool abLoopStream)
	{
		cOpenALSoundData* pSoundData = hplNew( cOpenALSoundData, (asName,abStream) );
		pSoundData->SetLoopStream(abLoopStream);
		
		if(pSoundData->CreateFromFile(asFilePath)==false)
		{
			hplDelete(pSoundData);
			return NULL;
		}
		
		return pSoundData;
	}

	//-----------------------------------------------------------------------

	void cLowLevelSoundOpenAL::GetSupportedFormats(tStringList &alstFormats)
	{
		int lPos = 0;

		while(mvFormats[lPos]!="")
		{
			alstFormats.push_back(mvFormats[lPos]);
			lPos++;
		}
	}
	//-----------------------------------------------------------------------
	
	void cLowLevelSoundOpenAL::UpdateSound(float afTimeStep)
	{
		ohpl_l5_low_update(mpRust);
	}
	
	//-----------------------------------------------------------------------

	void cLowLevelSoundOpenAL::SetListenerAttributes(const cVector3f &avPos,const cVector3f &avVel,
							const cVector3f &avForward,const cVector3f &avUp)
	{
		mvListenerPosition = avPos;
		mvListenerVelocity = avVel;
		mvListenerForward = avForward;
		mvListenerUp = avUp;

		mvListenerRight = cMath::Vector3Cross(mvListenerForward,mvListenerUp);

//		m_mtxListener = cMatrixf(
//				-mvListenerRight.x, -mvListenerRight.y,-mvListenerRight.z, avPos.x,
//				-mvListenerUp.x, -mvListenerUp.y,-mvListenerUp.z, avPos.y,
//				-mvListenerForward.x, -mvListenerForward.y,-mvListenerForward.z, avPos.z,
//				0, 0,0, 1
//			);
		m_mtxListener = cMatrixf::Identity;
		m_mtxListener.SetRight(mvListenerRight);
		m_mtxListener.SetUp(mvListenerUp);
		m_mtxListener.SetForward(mvListenerForward*-1);
		m_mtxListener = cMath::MatrixInverse(m_mtxListener);
		m_mtxListener.SetTranslation(mvListenerPosition);

		cVector3f vNegForward = avForward*(-1);
		ohpl_l5_low_set_listener(mpRust, avPos.v, avVel.v, vNegForward.v, avUp.v);
	}

	//-----------------------------------------------------------------------

	void cLowLevelSoundOpenAL::SetListenerPosition(const cVector3f &avPos)
	{
		mvListenerPosition = avPos;

		cVector3f vNegForward = mvListenerForward*(-1);
		ohpl_l5_low_set_listener(mpRust, avPos.v, mvListenerVelocity.v, vNegForward.v, mvListenerUp.v);
	}
	
	//-----------------------------------------------------------------------
		
	void cLowLevelSoundOpenAL::SetListenerAttenuation (bool abEnabled)
	{
		mbListenerAttenuation = abEnabled;
	}

	//-----------------------------------------------------------------------

	void cLowLevelSoundOpenAL::SetSetRolloffFactor(float afFactor)
	{
		ohpl_l5_low_set_rolloff(mpRust, afFactor);
	}
	
	//-----------------------------------------------------------------------

	void cLowLevelSoundOpenAL::SetVolume(float afVolume)
	{
		mfVolume = afVolume;

		ohpl_l5_low_set_volume(mpRust, afVolume);
	}

	//-----------------------------------------------------------------------

	/*void cLowLevelSoundOpenAL::LogSoundStatus()
	{
		//mbLogSounds = !mbLogSounds;
		//OAL_SetLoggingEnabled ( mbLogSounds );
	}
*/
	//-----------------------------------------------------------------------

	void cLowLevelSoundOpenAL::Init(int alSoundDeviceID, bool abUseEnvAudio,int alMaxChannels, 
									int alStreamUpdateFreq, bool abUseThreading, bool abUseVoiceManagement,
									int alMaxMonoSourceHint, int alMaxStereoSourceHint,
									int alStreamingBufferSize, int alStreamingBufferCount, bool abEnableLowLevelLog)
	{
		InitRust(alSoundDeviceID, abUseEnvAudio, alMaxChannels, alStreamUpdateFreq, abUseThreading, abUseVoiceManagement,
				alMaxMonoSourceHint, alMaxStereoSourceHint, alStreamingBufferSize, alStreamingBufferCount, abEnableLowLevelLog);
	}

	//-----------------------------------------------------------------------


	static void OhplL5ToDevices(const tSoundDeviceVec& avDevices, std::vector<OhplL5Device>& avOut)
	{
		for(size_t i=0;i<avDevices.size();++i)
		{
			OhplL5Device dev;
			dev.id = avDevices[i]->GetID();
			dev.name = (const uint8_t*)avDevices[i]->GetName().data();
			dev.name_len = avDevices[i]->GetName().size();
			dev.is_default = avDevices[i]->IsDefault();
			avOut.push_back(dev);
		}
	}

	void cLowLevelSoundOpenAL::InitRust(int alSoundDeviceID, bool abUseEnvAudio,int alMaxChannels,
									int alStreamUpdateFreq, bool abUseThreading, bool abUseVoiceManagement,
									int alMaxMonoSourceHint, int alMaxStereoSourceHint,
									int alStreamingBufferSize, int alStreamingBufferCount, bool abEnableLowLevelLog)
	{
		mbLogSounds = abEnableLowLevelLog;
		ohpl_l5_low_prelude(abEnableLowLevelLog);

		std::vector<OhplL5Device> vAll, vValid;
		OhplL5ToDevices(GetAvailableSoundDevices(), vAll);
		OhplL5ToDevices(GetFilteredSoundDevices(), vValid);

		OhplL5InitArgs args;
		args.device_id = alSoundDeviceID;
		args.use_env_audio = abUseEnvAudio;
		args.max_channels = alMaxChannels;
		args.stream_update_freq = alStreamUpdateFreq;
		args.use_threading = abUseThreading;
		args.use_voice_management = abUseVoiceManagement;
		args.max_mono_hint = alMaxMonoSourceHint;
		args.max_stereo_hint = alMaxStereoSourceHint;
		args.stream_buffer_size = alStreamingBufferSize;
		args.stream_buffer_count = alStreamingBufferCount;
		args.enable_log = abEnableLowLevelLog;

		OhplL5InitResult res = ohpl_l5_low_init(mpRust, &args, vAll.data(), vAll.size(), vValid.data(), vValid.size(),
												getenv("OPENHPL_SOUND_DEVICE"));
		mbInitialized = res.initialized;
		if(mbInitialized==false) return;

		mlCurrentSoundDevID = res.device_id;
		mbEnvAudioEnabled = ohpl_l5_low_env_enabled(mpRust);

		mvListenerForward = cVector3f(0,0,1);
		mvListenerUp = cVector3f(0,1,0);

		SetVolume(1.0f);
	}

	//-----------------------------------------------------------------------

	void cLowLevelSoundOpenAL::SetEnvVolume( float afEnvVolume )
	{
		float fVolume;
		if(ohpl_l5_low_set_env_volume(mpRust, afEnvVolume, &fVolume)) mfEnvVolume = fVolume;
	}

	//-----------------------------------------------------------------------

	iSoundDeviceIdentifier* cLowLevelSoundOpenAL::GetCurrentSoundDevice()
	{
		if(mlCurrentSoundDevID<0 || mlCurrentSoundDevID>=(int)mvSoundDevices.size())
			return GetFirstDefaultDevice();

		return mvSoundDevices[mlCurrentSoundDevID];
	}

	//-----------------------------------------------------------------------
	
	iSoundEnvironment* cLowLevelSoundOpenAL::LoadSoundEnvironment(const tString &asFilePath)
	{
		if (!mbEnvAudioEnabled)
			return NULL;
		
		/////////////////////////////////////////////
		///Check if sound already exist
		iSoundEnvironment *pSE = GetSoundEnvironmentFromFileName(asFilePath);
		if(pSE) return pSE;

		/////////////////////////////////////////////
		///Create new and load from file
		cOpenALSoundEnvironment* pSoundEnv = hplNew(cOpenALSoundEnvironment,());

		if (pSoundEnv->CreateFromFile(asFilePath)==false)
		{
			hplDelete(pSoundEnv);
			return NULL;
		}

		pSoundEnv->SetFileName(cString::ToLowerCase(asFilePath));

		mlstSoundEnv.push_back(pSoundEnv);

		//Log(" Created %d '%s'\n",pSoundEnv, pSoundEnv->GetName().c_str());

		return pSoundEnv;

	}

	//-----------------------------------------------------------------------
	
	void cLowLevelSoundOpenAL::SetSoundEnvironment ( iSoundEnvironment* apSoundEnv )
	{
		if (!mbEnvAudioEnabled)
			return;

		OhplL5EnvParams params;
		if(apSoundEnv) ((cOpenALSoundEnvironment*)apSoundEnv)->ToRust(params);
		ohpl_l5_low_set_env(mpRust, apSoundEnv ? &params : NULL);
	}

	//-----------------------------------------------------------------------

	void cLowLevelSoundOpenAL::FadeSoundEnvironment( iSoundEnvironment* apSourceSoundEnv, iSoundEnvironment* apDestSoundEnv, float afT )
	{
		if (!mbEnvAudioEnabled)
			return;

		OhplL5EnvParams src, dst;
		if(apSourceSoundEnv) ((cOpenALSoundEnvironment*)apSourceSoundEnv)->ToRust(src);
		if(apDestSoundEnv) ((cOpenALSoundEnvironment*)apDestSoundEnv)->ToRust(dst);
		ohpl_l5_low_fade_env(mpRust, apSourceSoundEnv ? &src : NULL, apDestSoundEnv ? &dst : NULL, afT);
	}

	//-----------------------------------------------------------------------

	///////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	///////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iSoundDeviceIdentifier* cLowLevelSoundOpenAL::GetFirstDefaultDevice()
	{
		std::vector<OhplL5Device> vAll, vValid;
		OhplL5ToDevices(GetAvailableSoundDevices(), vAll);
		OhplL5ToDevices(GetFilteredSoundDevices(), vValid);
		int lId = ohpl_l5_first_default(vAll.data(), vAll.size(), vValid.data(), vValid.size());
		const tSoundDeviceVec& vDevices = GetAvailableSoundDevices();
		for(size_t i=0;i<vDevices.size();++i)
			if(vDevices[i]->GetID()==lId) return vDevices[i];
		return NULL;
	}

}
