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

#include "sound/SoundHandler.h"

#include "resources/Resources.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Platform.h"
#include "math/Math.h"
#include "sound/LowLevelSound.h"
#include "sound/SoundChannel.h"
#include "resources/SoundManager.h"
#include "sound/SoundData.h"
#include "sound/SoundEntityData.h"
#include "resources/SoundEntityManager.h"

#include "scene/World.h"
#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// RAY CALLBACK
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cSoundRayCallback::Reset()
	{
		mbHasCollided = false;
	}

	//-----------------------------------------------------------------------

	bool cSoundRayCallback::BeforeIntersect(iPhysicsBody *pBody)
	{
		if(pBody->GetBlocksSound()) return true;

		return false;
	}
	
	//-----------------------------------------------------------------------

	bool cSoundRayCallback::OnIntersect(iPhysicsBody *pBody,cPhysicsRayParams *apParams)
	{
		mbHasCollided = true;
		return false;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// SOUND ENTRY
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	static iSoundChannel* Ch(void *ap){ return static_cast<iSoundChannel*>(ap); }
	static cSoundHandler* Handler(void *ap){ return static_cast<cSoundHandler*>(ap); }

	static const OhplSoundChannelOps gChannelOps = {
		[](void *c){ Ch(c)->Play(); },
		[](void *c){ Ch(c)->Stop(); },
		[](void *c, bool x){ Ch(c)->SetPaused(x); },
		[](void *c){ return Ch(c)->GetPaused(); },
		[](void *c){ return Ch(c)->IsPlaying(); },
		[](void *c){ return Ch(c)->GetStopUsed(); },
		[](void *c){ return Ch(c)->GetSpeed(); },
		[](void *c, float x){ Ch(c)->SetSpeed(x); },
		[](void *c){ return Ch(c)->GetVolume(); },
		[](void *c, float x){ Ch(c)->SetVolume(x); },
		[](void *c){ return Ch(c)->GetLooping(); },
		[](void *c, bool x){ Ch(c)->SetLooping(x); },
		[](void *c, bool x){ Ch(c)->Set3D(x); },
		[](void *c, int x){ Ch(c)->SetPriority(x); },
		[](void *c, int x){ Ch(c)->SetPriorityModifier(x); },
		[](void *c, bool x){ Ch(c)->SetAffectedByEnv(x); },
		[](void *c){ return Ch(c)->GetPositionIsRelative(); },
		[](void *c, bool x){ Ch(c)->SetPositionIsRelative(x); },
		[](void *c, cVector3f *o){ *o = Ch(c)->GetPosition(); },
		[](void *c, const cVector3f *v){ Ch(c)->SetPosition(*v); },
		[](void *c, cVector3f *o){ *o = Ch(c)->GetRelPosition(); },
		[](void *c, const cVector3f *v){ Ch(c)->SetRelPosition(*v); },
		[](void *c){ return Ch(c)->GetMinDistance(); },
		[](void *c, float x){ Ch(c)->SetMinDistance(x); },
		[](void *c){ return Ch(c)->GetMaxDistance(); },
		[](void *c, float x){ Ch(c)->SetMaxDistance(x); },
		[](void *c){ return Ch(c)->GetBlockVolumeMul(); },
		[](void *c){ return Ch(c)->GetElapsedTime(); },
		[](void *c){ return Ch(c)->GetTotalTime(); },
		[](void *c, double x){ Ch(c)->SetElapsedTime(x); },
	};

	static const OhplSoundHostOps gHostOps = {
		[](void *h, uint32_t t){ return Handler(h)->GetGlobalSpeed((eSoundEntryType)t); },
		[](void *h, uint32_t t){ return Handler(h)->GetGlobalVolume((eSoundEntryType)t); },
		[](void *h){ return Handler(h)->GetSilent(); },
		[](void *h, const cVector3f *p){ return Handler(h)->CheckSoundIsBlocked(*p); },
		[](void *h, cVector3f *o){ *o = Handler(h)->GetLowLevelSound()->GetListenerPosition(); },
		[](void *h, cMatrixf *o){ *o = Handler(h)->GetLowLevelSound()->GetListenerMatrix(); },
	};

	const OhplSoundChannelOps* SoundChannelOps(){ return &gChannelOps; }

	//-----------------------------------------------------------------------

	cSoundEntry::cSoundEntry(	const tString& asName, iSoundChannel* apSound, float afVolume, 
								eSoundEntryType aType, bool ab3D, 
								bool abStream, int alId,
								cSoundHandler *apSoundHandler)
	{
		msName = cString::ToLowerCase(asName);
		mpSound = apSound;
		mlId = alId;
		mpSoundHandler = apSoundHandler;
		mpCallback = NULL;
		ohpl_snd_entry_init(&mS, afVolume, (uint32_t)aType, ab3D, abStream);
	}

	//-----------------------------------------------------------------------

	cSoundEntry::~cSoundEntry()
	{
		if(mpSound)
		{
			mpSound->Stop();
			hplDelete( mpSound );
		}
	}

	//-----------------------------------------------------------------------
	
	bool cSoundEntry::Update(float afTimeStep)
	{
		int lState = ohpl_snd_entry_update(&mS, afTimeStep, &gChannelOps, mpSound, &gHostOps, mpSoundHandler);
		if(lState == 1) return true;

		if(lState == 2 && mpCallback) mpCallback->OnPriorityRelease();

		mpSound->Stop();
		return false;
	}

	//-----------------------------------------------------------------------

	void cSoundEntry::Stop()
	{
		ohpl_snd_entry_stop(&mS, &gChannelOps, mpSound);
	}

	//-----------------------------------------------------------------------

	void cSoundEntry::SetPaused(bool abX)
	{
		mpSound->SetPaused(abX);
	}

	//-----------------------------------------------------------------------

	void cSoundEntry::FadeVolumeMulTo(float afDestMul, float afSpeed)
	{
		ohpl_snd_entry_fade_volume(&mS, afDestMul, afSpeed);
	}

	void cSoundEntry::FadeSpeedMulTo(float afDestMul, float afSpeed)
	{
		ohpl_snd_entry_fade_speed(&mS, afDestMul, afSpeed);
	}

	void cSoundEntry::FadeOut(float afSpeed)
	{
		ohpl_snd_entry_fade_out(&mS, afSpeed);
	}

	void cSoundEntry::FadeIn(float afVolumeMul,float afSpeed)
	{
		ohpl_snd_entry_fade_in(&mS, afVolumeMul, afSpeed);
	}

	//-----------------------------------------------------------------------

	
	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cSoundHandler::cSoundHandler(iLowLevelSound* apLowLevelSound, cResources* apResources)
	{
		mpLowLevelSound = apLowLevelSound;
		mpResources = apResources;

		mpWorld = NULL;

		mlCount =0;
		mlIdCount = 0;

		mbSilent = false;

		for(int i=0; i<32; ++i)
		{
			mfGlobalVolume[i] = 1;
			mfGlobalSpeed[i] = 1;
		}
	}
	
	//-----------------------------------------------------------------------

	cSoundHandler::~cSoundHandler()
	{
		STLDeleteAll(m_lstSoundEntries);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cSoundHandler::Update(float afTimeStep)
	{
		///////////////////////////////////////////////
		// Update global volume and speed
		
		for(int i=0; i<32; ++i)
		{
			mfGlobalVolume[i] = mGlobalVolumeHandler.CalcResults(1u<<i,eMultipleSettingsCalcType_Min,1.0f);
			mfGlobalSpeed[i] = mGlobalSpeedHandler.CalcResults(1u<<i,eMultipleSettingsCalcType_Min,1.0f);
		}

		mGlobalVolumeHandler.Update(afTimeStep);
		mGlobalSpeedHandler.Update(afTimeStep);

		///////////////////////////////////////////////
		// Update entries
		tSoundEntryListIt it = m_lstSoundEntries.begin();
		for(; it != m_lstSoundEntries.end();)
		{
			cSoundEntry *pEntry = *it;

			if(pEntry->Update(afTimeStep) == false)
			{
				hplDelete( pEntry );
				//m_lstSoundEntriesPool.push_back(pEntry);
				it = m_lstSoundEntries.erase(it);
			}
			else
			{
				++it;
			}
		}

		mlCount++;
	}
	
	//-----------------------------------------------------------------------

	cSoundEntry* cSoundHandler::Play(	const tString& asName,bool abLoop,float afVolume,const cVector3f& avPos,  
										float afMinDist,float afMaxDist, 
										eSoundEntryType aEntryType,bool abRelative, 
										bool ab3D,int alPriorityModifier, bool abStream, bool *apNotEnoughChannels)
	{
		if(asName == "") return NULL;

		int lDistPrio = ohpl_snd_play_priority(&avPos, &mpLowLevelSound->GetListenerPosition(), afMinDist, afMaxDist, ab3D, abRelative);
			
		///////////////////////////////
		//Create sound channel
		if(apNotEnoughChannels) *apNotEnoughChannels = false;
		bool bNotEnoughChannels;
		iSoundChannel *pSound = CreateChannel(asName,lDistPrio + alPriorityModifier, abStream,&bNotEnoughChannels);
		if(pSound == NULL)
		{
			if(apNotEnoughChannels) *apNotEnoughChannels = bNotEnoughChannels;

			if(bNotEnoughChannels==false)
				Error("Can't find sound '%s'!\n",asName.c_str());
			else
				Warning("Could not start sound '%s', too many sounds playing!\n",asName.c_str());
			
			return NULL;
		}

		OhplSoundPlayParams Params = { avPos, afMinDist, afMaxDist, alPriorityModifier, (uint32_t)aEntryType, abLoop, abRelative, ab3D, mbSilent };
		ohpl_snd_play_setup(&Params, lDistPrio, &mpLowLevelSound->GetListenerMatrix(), &gChannelOps, pSound);

		////////////////////////
		// Create entry
		cSoundEntry *pEntry = hplNew( cSoundEntry, (asName,pSound,afVolume,aEntryType, ab3D, false,mlIdCount,this) );
		/*cSoundEntry *pEntry = NULL;
		if(m_lstSoundEntriesPool.empty())
		{
			pEntry = hplNew( cSoundEntry, (asName,pSound,afVolume,aEntryType, ab3D, false,mlIdCount,this) );
		}
		else
		{
			pEntry = m_lstSoundEntriesPool.back();
			m_lstSoundEntriesPool.pop_back();

			pEntry->Setup(asName,pSound,afVolume,aEntryType, ab3D, false,mlIdCount);
		}*/

		m_lstSoundEntries.push_back(pEntry);
		mlstRecentStarts.push_back(cString::ToString((int)cPlatform::GetApplicationTime()) + "|" + asName + "|" + cString::ToString(afVolume) + "|" +
								   (abLoop ? "1" : "0") + "|" + (ab3D ? "1" : "0") + "|" + (abRelative ? "1" : "0") + "|" +
								   cString::ToString(ab3D && !abRelative ? cMath::Vector3Dist(avPos, mpLowLevelSound->GetListenerPosition()) : avPos.Length()) + "|" +
								   cString::ToString(afMinDist) + "|" + cString::ToString(afMaxDist));
		if(mlstRecentStarts.size() > 256) mlstRecentStarts.pop_front();

		mlIdCount++;

		return pEntry;
	}

	//-----------------------------------------------------------------------

	cSoundEntry* cSoundHandler::Play3D(	const tString& asName,bool abLoop,float afVolume,const cVector3f& avPos,  
										float afMinDist,float afMaxDist, 
										eSoundEntryType aEntryType, bool abRelative,
										int alPriorityModifier, bool abStream, bool *apNotEnoughChannels)
	{
		return Play(asName,abLoop,afVolume,avPos,afMinDist,afMaxDist,aEntryType,abRelative,true,
					alPriorityModifier, abStream,apNotEnoughChannels);
	}

	//-----------------------------------------------------------------------

	cSoundEntry* cSoundHandler::PlayGui(const tString& asName,bool abLoop,float afVolume,const cVector3f& avPos,eSoundEntryType aEntryType, bool *apNotEnoughChannels)
	{
		return Play(asName,abLoop,afVolume,avPos,1.0f,1000.0f,aEntryType,true,false,0,false,apNotEnoughChannels);
	}

	//-----------------------------------------------------------------------

	cSoundEntry* cSoundHandler::PlayGuiStream(const tString& asFileName,bool abLoop,float afVolume,const cVector3f& avPos,eSoundEntryType aEntryType, bool *apNotEnoughChannels)
	{
		return Play(asFileName,abLoop,afVolume,avPos,1.0f,1000.0f,aEntryType,true,false,0,true,apNotEnoughChannels);
	}


	//-----------------------------------------------------------------------

	cSoundEntry* cSoundHandler::PlaySoundEntityGui(	const tString& asName,bool abLoop,float afVolume,
													eSoundEntryType aEntryType,
													const cVector3f& avPos, bool *apNotEnoughChannels)
	{
		cSoundEntityData *pData = mpResources->GetSoundEntityManager()->CreateSoundEntity(asName);
		if(pData == NULL) return NULL;

		tString sSoundName = pData->GetRandomSoundName(eSoundEntityType_Main,true);
		if(sSoundName=="") return NULL;
		
		if(pData->GetStream())
		{
			return PlayGuiStream(sSoundName,abLoop,afVolume * pData->GetVolume(),avPos,aEntryType,apNotEnoughChannels);
		}
		else
		{
			return PlayGui(sSoundName,abLoop,pData->GetVolume() * pData->GetVolume(),avPos,aEntryType,apNotEnoughChannels);
		}
	}
	
	//-----------------------------------------------------------------------

	bool cSoundHandler::Stop(const tString& asName)
	{
		cSoundEntry *pEntry = GetEntry(asName);
		if(pEntry) 
		{
			pEntry->Stop();
		}
		else
		{
			return false;
		}

		return true;
	}
	
	//-----------------------------------------------------------------------

	bool cSoundHandler::StopAllExcept(const tString& asName)
	{
		return false;
	}
	
	//-----------------------------------------------------------------------

	void cSoundHandler::StopAll(tFlag mTypes)
	{
		tSoundEntryListIt it = m_lstSoundEntries.begin();
		for(; it != m_lstSoundEntries.end(); ++it)
		{
			cSoundEntry *pEntry = *it;

            if(pEntry->GetType() & mTypes)
			{
				pEntry->Stop();
			}
		}
	}

	//-----------------------------------------------------------------------

	void cSoundHandler::PauseAll(tFlag mTypes)
	{
		tSoundEntryListIt it = m_lstSoundEntries.begin();
		for(; it != m_lstSoundEntries.end(); ++it)
		{
			cSoundEntry *pEntry = *it;

			if(pEntry->GetType() & mTypes)
			{
				pEntry->SetPaused(true);
			}
		}
	}
	//-----------------------------------------------------------------------

	void cSoundHandler::ResumeAll(tFlag mTypes)
	{
		tSoundEntryListIt it = m_lstSoundEntries.begin();
		for(; it != m_lstSoundEntries.end(); ++it)
		{
			cSoundEntry *pEntry = *it;

			if(pEntry->GetType() & mTypes)
			{
				pEntry->SetPaused(false);
			}
		}
	}

	//-----------------------------------------------------------------------

	void cSoundHandler::FadeOutAll(tFlag mTypes,float afFadeSpeed, bool abDisableStop)
	{
		tSoundEntryListIt it = m_lstSoundEntries.begin();
		for(; it != m_lstSoundEntries.end(); ++it)
		{
			cSoundEntry *pEntry = *it;

			if(pEntry->GetType() & mTypes)
			{
				pEntry->FadeOut(afFadeSpeed);
				if(abDisableStop) pEntry->SetStopDisabled(true);
			}
		}
	}
	
	//-----------------------------------------------------------------------

	bool cSoundHandler::IsPlaying(const tString& asName)
	{
		cSoundEntry *pEntry = GetEntry(asName);
		
		if(pEntry) return pEntry->GetChannel()->IsPlaying();
		
		return false;
	}
	//-----------------------------------------------------------------------
	
	bool cSoundHandler::IsValid(cSoundEntry *apEntry, int alID)
	{
		tSoundEntryListIt it = m_lstSoundEntries.begin();
		for(; it != m_lstSoundEntries.end(); ++it)
		{
			cSoundEntry *pEntry = *it;

			if(pEntry == apEntry)
			{
				if(pEntry->GetId() == alID)	return true;
				else						return false;
			}
		}

		return false;
	}

	//-----------------------------------------------------------------------
	
	int cSoundHandler::SetGlobalVolume(float afVolume, tFlag aAffectedTypes, int alId)
	{
		if(alId < 0) alId = mGlobalVolumeHandler.CreateEntry();
		
		cMultipleSettingsHandler::cGSEntry* pEntry = mGlobalVolumeHandler.GetEntry(alId, true);
				
		pEntry->SetTypes(aAffectedTypes);
		pEntry->SetValAndDest(afVolume);

        return alId;
	}
	
	//-----------------------------------------------------------------------
	
	int cSoundHandler::SetGlobalSpeed(float afSpeed, tFlag aAffectedTypes, int alId)
	{
		if(alId < 0) alId = mGlobalSpeedHandler.CreateEntry();

		cMultipleSettingsHandler::cGSEntry* pEntry = mGlobalSpeedHandler.GetEntry(alId, true);

		pEntry->SetTypes(aAffectedTypes);
		pEntry->SetValAndDest(afSpeed);

		return alId;
	}

	//-----------------------------------------------------------------------

	int cSoundHandler::FadeGlobalVolume(float afDestVolume, float afSpeed,tFlag aAffectedTypes, int alId, bool abDestroyIdAtDest)
	{	
		if(alId < 0) alId = mGlobalVolumeHandler.CreateEntry();

		cMultipleSettingsHandler::cGSEntry* pEntry = mGlobalVolumeHandler.GetEntry(alId, true);

		pEntry->SetTypes(aAffectedTypes);
		pEntry->FadeTo(afDestVolume,afSpeed,abDestroyIdAtDest);

		return alId;
	}

	int cSoundHandler::FadeGlobalSpeed(float afDestSpeed, float afSpeed,tFlag aAffectedTypes, int alId, bool abDestroyIdAtDest)
	{
		if(alId < 0) alId = mGlobalSpeedHandler.CreateEntry();

		cMultipleSettingsHandler::cGSEntry* pEntry = mGlobalSpeedHandler.GetEntry(alId, true);

		pEntry->SetTypes(aAffectedTypes);
		pEntry->FadeTo(afDestSpeed,afSpeed,abDestroyIdAtDest);

		return alId;
	}

	//-----------------------------------------------------------------------

	float cSoundHandler::GetGlobalVolume(eSoundEntryType aType)
	{
		return aType ? mfGlobalVolume[__builtin_ctz((unsigned)aType)] : 1.0f;
	}

	float cSoundHandler::GetGlobalSpeed(eSoundEntryType aType)
	{
		return aType ? mfGlobalSpeed[__builtin_ctz((unsigned)aType)] : 1.0f;
	}

	//-----------------------------------------------------------------------

	void cSoundHandler::SetWorld(cWorld *apWorld)
	{
		mpWorld = apWorld;
	}

	//-----------------------------------------------------------------------
	
	tSoundEntryList* cSoundHandler::GetEntryList()
	{
		return &m_lstSoundEntries;
	}

	//-----------------------------------------------------------------------

	bool cSoundHandler::CheckSoundIsBlocked(const cVector3f& avSoundPosition)
	{
		if(mpWorld==NULL || mpWorld->GetPhysicsWorld()==NULL) return false;

		iPhysicsWorld *pPhysicsWorld = mpWorld->GetPhysicsWorld();

		mSoundRayCallback.Reset();

		pPhysicsWorld->CastRay(	&mSoundRayCallback,avSoundPosition,
								mpLowLevelSound->GetListenerPosition(),
								false,false,false,true);
		
		return mSoundRayCallback.HasCollided();
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSoundEntry* cSoundHandler::GetEntry(const tString& asName)
	{
		tString sLowName = cString::ToLowerCase(asName);
		
		tSoundEntryListIt it = m_lstSoundEntries.begin();
		for(; it != m_lstSoundEntries.end(); ++it)
		{
			cSoundEntry *pEntry = *it;

			if(pEntry->GetName() == sLowName)
			{
				return pEntry;
			}
		}		

		return NULL;
	}
	
	//-----------------------------------------------------------------------
	
	iSoundChannel* cSoundHandler::CreateChannel(const tString& asName, int alPriority, bool abStream, bool *apNotEnoughChannels)
	{
		if(apNotEnoughChannels) *apNotEnoughChannels = false;

		////////////////////////
		//Load the data
		iSoundData* pData = mpResources->GetSoundManager()->CreateSoundData(asName,abStream);
		if(pData == NULL)
		{
			Error("Could not load sound '%s'\n", asName.c_str());
			return NULL;
		}
		
		/////////////////////////
		//Create sound channel
		iSoundChannel* pSound = pData->CreateChannel(alPriority);
		if(pSound == NULL)
		{
			//Need to destroy channel else it will never be deleted!
			if(abStream) mpResources->GetSoundManager()->Destroy(pData);

			if(apNotEnoughChannels) *apNotEnoughChannels = true;
		}
		
		return pSound;
	}
	
	//-----------------------------------------------------------------------

}
