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

#ifndef HPL_SOUNDHANDLER_H
#define HPL_SOUNDHANDLER_H

#include "system/SystemTypes.h"
#include "math/MathTypes.h"
#include "sound/SoundTypes.h"
#include "sound/RustSound.h"
#include "engine/EngineTypes.h"

namespace hpl {

	class iLowLevelSound;
	class cResources;
	class cWorld;

	enum eSoundEntryType
	{
		eSoundEntryType_World = eFlagBit_0,
		eSoundEntryType_Gui =	eFlagBit_1,
		eSoundEntryType_All =	eFlagBit_All
	};

	//----------------------------------------

	/**
	 * A playing sound. The record is owned by the Rust handler; a cSoundEntry pointer is its address
	 * and is never constructed here.
	 */
	class cSoundEntry
	{
	public:
		tString GetName() const { size_t n; const unsigned char *p = ohpl_sndent_name(R(), &n); return tString((const char*)p, n); }
		eSoundEntryType GetType() const { return (eSoundEntryType)ohpl_sndent_type(R()); }
		int GetId() const { return ohpl_sndent_id(R()); }
		void* GetChannel() const { return ohpl_sndent_channel(R()); }

		bool IsFirstTime(){ return ohpl_sndent_get(R(), 1) != 0; }

		void Stop(){ ohpl_sndent_stop(R()); }
		void SetPaused(bool abX){ ohpl_sndent_set_paused(R(), abX); }

		void SetDefaultVolume(float afVolume){ ohpl_sndent_set(R(), 2, afVolume); }
		void SetDefaultSpeed(float afSpeed){ ohpl_sndent_set(R(), 3, afSpeed); }

		void SetVolumeMul(float afMul){ ohpl_sndent_set(R(), 0, afMul); }
		float GetVolumeMul() const { return ohpl_sndent_get(R(), 0); }
		void SetSpeedMul(float afMul){ ohpl_sndent_set(R(), 1, afMul); }

		void FadeVolumeMulTo(float afDestMul, float afSpeed){ ohpl_sndent_fade(R(), 0, afDestMul, afSpeed); }
		void FadeSpeedMulTo(float afDestMul, float afSpeed){ ohpl_sndent_fade(R(), 1, afDestMul, afSpeed); }

		void FadeOut(float afSpeed){ ohpl_sndent_fade(R(), 2, 0, afSpeed); }
		void FadeIn(float afVolumeMul,float afSpeed){ ohpl_sndent_fade(R(), 3, afVolumeMul, afSpeed); }

		void SetStopDisabled(bool abX){ ohpl_sndent_set(R(), 4, abX ? 1.0f : 0.0f); }
		bool GetStopDisabled(){ return ohpl_sndent_get(R(), 3) != 0; }

	private:
		OhplSndRec* R() const { return (OhplSndRec*)this; }
		cSoundEntry();
		~cSoundEntry();
	};

	//----------------------------------------

	class cSoundHandler;

	/** One of the handler's global volume/speed tables (kind 0 volume, 1 speed), kept in Rust. */
	class cSoundSettings
	{
	public:
		cMultipleSettingsHandler::cGSEntry* GetEntry(int alIdx, bool abForceCreation=true){ return (cMultipleSettingsHandler::cGSEntry*)ohpl_sndh_gs_entry(mpRs, mlKind, alIdx, abForceCreation); }
		int CreateEntry(){ return ohpl_sndh_gs_create(mpRs, mlKind); }
		void DestroyEntry(int alIdx){ ohpl_sndh_gs_destroy(mpRs, mlKind, alIdx); }
		bool EntryExists(int alIdx){ return ohpl_sndh_gs_exists(mpRs, mlKind, alIdx); }
		float CalcResults(tFlag aTypes, eMultipleSettingsCalcType aCalcType, float afDefault){ return ohpl_sndh_gs_calc(mpRs, mlKind, aTypes, aCalcType, afDefault); }

		OhplSndHandler *mpRs;
		size_t mlKind;
	};

	class cSoundHandler
	{
	public:
		cSoundHandler(iLowLevelSound* apLowLevelSound, cResources* apResources)
		{
			mpRs = ohpl_sndh_new(OhplSndHostTable(), apLowLevelSound, apResources);
			mpResources = apResources;
			mVolume.mpRs = mSpeed.mpRs = mpRs;
			mVolume.mlKind = 0;
			mSpeed.mlKind = 1;
		}
		~cSoundHandler(){ ohpl_sndh_free(mpRs); }

		OhplSndHandler* Rs(){ return mpRs; }

		void Update(float afTimeStep){ ohpl_sndh_update(mpRs, afTimeStep); }

		/**
		 * Plays a sound
		 * \param asName Name of the file to load. 
		 * \param abLoop If the sound shall be looping
		 * \param afVolume The start volume of the sound
		 * \param avPos The position to play it at
		 * \param afMinDist Inside this distance max volume (afVolume) is heard, after it is lowered, only used if 3D.
		 * \param afMaxDist Beyond this distance volume is 0
		 * \param mEntryType The type of sound. Use to determine what effects affect it.
		 * \param abRelative If position is relative to listener
		 * \param ab3D If the sound uses 3D calculations to determine volume and other world effects.
		 * \param alPriorityModifier The priority of the sound, if 3D then this number is added to a calculated priority based on distance from listener.
		 * \return 
		 */
		cSoundEntry* Play(	const tString& asName,bool abLoop,float afVolume,const cVector3f& avPos,  
							float afMinDist,float afMaxDist, eSoundEntryType aEntryType, 
							bool abRelative, bool ab3D, int alPriorityModifier, bool abStream,
							bool *apNotEnoughChannels=NULL)
		{
			return (cSoundEntry*)ohpl_sndh_play(mpRs, (const unsigned char*)asName.data(), asName.size(), abLoop, afVolume, &avPos, afMinDist, afMaxDist,
												(uint32_t)aEntryType, abRelative, ab3D, alPriorityModifier, abStream, apNotEnoughChannels);
		}
		
		cSoundEntry* Play3D(	const tString& asName,bool abLoop,float afVolume,const cVector3f& avPos,  
								float afMinDist,float afMaxDist, 
								eSoundEntryType aEntryType = eSoundEntryType_World, 
								bool abRelative=false,
								int alPriorityModifier=0, bool abStream=false, bool *apNotEnoughChannels=NULL)
		{
			return Play(asName,abLoop,afVolume,avPos,afMinDist,afMaxDist,aEntryType,abRelative,true,alPriorityModifier, abStream,apNotEnoughChannels);
		}
		
		cSoundEntry* PlayGuiStream(	const tString& asFileName,bool abLoop,float afVolume,const cVector3f& avPos=cVector3f(0,0,1),
									eSoundEntryType aEntryType = eSoundEntryType_Gui, bool *apNotEnoughChannels=NULL)
		{
			return Play(asFileName,abLoop,afVolume,avPos,1.0f,1000.0f,aEntryType,true,false,0,true,apNotEnoughChannels);
		}

		cSoundEntry* PlayGui(	const tString& asName,bool abLoop,float afVolume,const cVector3f& avPos=cVector3f(0,0,1),
								eSoundEntryType aEntryType = eSoundEntryType_Gui, bool *apNotEnoughChannels=NULL)
		{
			return Play(asName,abLoop,afVolume,avPos,1.0f,1000.0f,aEntryType,true,false,0,false,apNotEnoughChannels);
		}

		cSoundEntry* PlaySoundEntityGui(const tString& asName,bool abLoop,float afVolume,
										eSoundEntryType aEntryType = eSoundEntryType_Gui,
										const cVector3f& avPos=cVector3f(0,0,1), bool *apNotEnoughChannels=NULL);

		iLowLevelSound* GetLowLevelSound(){ return (iLowLevelSound*)ohpl_sndh_lowlevel(mpRs); }

		void SetSilent(bool abX){ ohpl_sndh_set_silent(mpRs, abX); }
		bool GetSilent(){ return ohpl_sndh_silent(mpRs); }
		
		bool Stop(const tString& asName){ return ohpl_sndh_stop(mpRs, (const unsigned char*)asName.data(), asName.size()); }
		
		void StopAll(tFlag mTypes){ ohpl_sndh_each(mpRs, mTypes, 0, 0, false); }
		void PauseAll(tFlag mTypes){ ohpl_sndh_each(mpRs, mTypes, 1, 0, false); }
		void ResumeAll(tFlag mTypes){ ohpl_sndh_each(mpRs, mTypes, 2, 0, false); }
		void FadeOutAll(tFlag mTypes,float afFadeSpeed, bool abDisableStop){ ohpl_sndh_each(mpRs, mTypes, 3, afFadeSpeed, abDisableStop); }

		bool IsPlaying(const tString& asName){ return ohpl_sndh_is_playing(mpRs, (const unsigned char*)asName.data(), asName.size()); }

		bool IsValid(cSoundEntry *apEntry, int alID){ return ohpl_sndh_is_valid(mpRs, (const OhplSndRec*)apEntry, alID); }
		
		/**
		 * Set the global volume for a specific type(s). If alId >= 0 then the settings will be applied to that id, else a free id will be found. 
		 * \param afVolume Global Volume to be set.
		 * \param mAffectedTypes Type(s) affected by this setting.
		 * \param alId The Id to the setting applies to, if < 0, the the a free one is found.
		 * \return if alId >=0, alId is returned, else the lowest free id.
		 */
		int SetGlobalVolume(float afVolume, tFlag mAffectedTypes, int alId){ return ohpl_sndh_gs_set(mpRs, 0, afVolume, mAffectedTypes, alId); }
		int SetGlobalSpeed(float afSpeed,tFlag mAffectedTypes, int alId){ return ohpl_sndh_gs_set(mpRs, 1, afSpeed, mAffectedTypes, alId); }

		int FadeGlobalVolume(float afDestVolume, float afSpeed,tFlag mAffectedTypes, int alId, bool abDestroyIdAtDest){ return ohpl_sndh_gs_fade(mpRs, 0, afDestVolume, afSpeed, mAffectedTypes, alId, abDestroyIdAtDest); }
		int FadeGlobalSpeed(float afDestSpeed, float afSpeed,tFlag mAffectedTypes, int alId, bool abDestroyIdAtDest){ return ohpl_sndh_gs_fade(mpRs, 1, afDestSpeed, afSpeed, mAffectedTypes, alId, abDestroyIdAtDest); }

		float GetGlobalVolume(eSoundEntryType aType){ return ohpl_sndh_global_volume(mpRs, (uint32_t)aType); }
		float GetGlobalSpeed(eSoundEntryType aType){ return ohpl_sndh_global_speed(mpRs, (uint32_t)aType); }

		cSoundSettings* GetGlobalVolumeSettingsHandler(){ return &mVolume;}
		cSoundSettings* GetGlobalSpeedSettingsHandler(){ return &mSpeed;}

		void SetWorld(cWorld *apWorld){ ohpl_sndh_set_world(mpRs, apWorld); }

		size_t GetEntryNum(){ return ohpl_sndh_count(mpRs); }
		cSoundEntry* GetEntryAt(size_t alIdx){ return (cSoundEntry*)ohpl_sndh_entry_at(mpRs, alIdx); }
		size_t GetRecentStartNum(){ return ohpl_sndh_recent_count(mpRs); }
		tString GetRecentStart(size_t alIdx){ size_t n; const unsigned char *p = ohpl_sndh_recent(mpRs, alIdx, &n); return tString((const char*)p, n); }

	private:
		OhplSndHandler *mpRs;
		cResources *mpResources;
		cSoundSettings mVolume;
		cSoundSettings mSpeed;
	};

};
#endif // HPL_SOUNDHANDLER_H
