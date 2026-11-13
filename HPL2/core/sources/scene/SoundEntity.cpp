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

#include "scene/SoundEntity.h"

#include "sound/SoundHandler.h"
#include "sound/SoundChannel.h"
#include "sound/SoundEntityData.h"
#include "sound/LowLevelSound.h"
#include "sound/Sound.h"
#include "sound/SoundData.h"
#include "resources/SoundEntityManager.h"
#include "math/Math.h"

#include "engine/Engine.h"
#include "scene/Scene.h"
#include "scene/World.h"
#include "scene/RustWorld.h"
#include "scene/RustSceneH12.h"
#include "impl/RustBackends.h"

namespace hpl {

	tSoundEntityGlobalCallbackList cSoundEntity::mlstGobalCallbacks;
	tSoundEntityEventFactory cSoundEntity::mpEventFactory = NULL;

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cSoundEntity::cSoundEntity(const tString& asName,cSoundEntityData *apData,
								cSoundEntityManager *apSoundEntityManager,
								cWorld *apWorld,
								cSoundHandler *apSoundHandler, bool abRemoveWhenOver, int alCreationID)
		:iEntity3D(asName)
	{
		mlCreationID = alCreationID;

		//Log("Created sound entity '%s'\n",msName.c_str());

		mpSoundHandler = apSoundHandler;
		mpSoundEntityManager = apSoundEntityManager;
		mpWorld = apWorld;

		mpData = apData;
		mpData->PreloadSounds();

		for(int i=0; i<3; i++)
		{
			mvSoundEntries[i] = NULL;
			mvSoundEntryID[i]=-1;
		}

		mfVolume = mpData->GetVolume();

		mfMinDistance = mpData->GetMinDistance();
		mfMaxDistance = mpData->GetMaxDistance();

		mBoundingVolume.SetSize(mfMaxDistance*2);

		mbLog = false;
		
		mbForcePlayAsGUISound = false;

		mpSoundCallback = hplNew( cSoundEntityChannelCallback, () );
		mpSoundCallback->mpEntity = this;

		const bool bSilent = mpSoundHandler->GetSilent();
		const OhplSoundCfg cfg = GetCfg();
		ohpl_snd_init(&cfg, abRemoveWhenOver, bSilent, &mState);
		if(bSilent) mfVolume = 0;

		//if( mpData->GetName() == "scrape_wood.snt") mbLog = true;
		if(mbLog)Log("Created %d\n", this);

		mpEvent = mpEventFactory ? mpEventFactory(this) : NULL;
	}
	
	cSoundEntity::~cSoundEntity()
	{
		hplDelete(mpEvent);
		if(mbLog)Log("Deleting %d\n", this);
		for(int i=0; i<3; i++)
		{
			if(mpSoundHandler->IsValid(mvSoundEntries[i], mvSoundEntryID[i]))
			{
				mvSoundEntries[i]->SetCallBack(NULL); //Reset callback, just in case!
				mvSoundEntries[i]->Stop();
			}
		}

		hplDelete(mpSoundCallback);

		//Do _not_ delete sound entity data files now. 
		//Just dec user count and the manager can be cleared of unused files when you want to.
		mpData->DecUserCount();
	}

	///////////////////////////////////////
	//DEBUG when using pooling test
	/*void cSoundEntity::Setup(const tString& asName,cSoundEntityData *apData, bool abRemoveWhenOver)
	{
		/////////////////////////////
		// Entity
		msName = asName;
		mbIsActive = true;

		mpParentNode = NULL;

		m_mtxLocalTransform = cMatrixf::Identity;
		m_mtxWorldTransform = cMatrixf::Identity;

		mbTransformUpdated = true;

		mlCount = 0;

		msSourceFile = "";

		mbApplyTransformToBV = true;
		mbUpdateBoundingVolume = true;

		mpParent = NULL;

		mlIteratorCount =-1;

		mbIsSaved = true;
		mlUniqueID = -1;

		/////////////////////////////
		// Sound ENtityh 
		mbRemoveWhenOver = abRemoveWhenOver;

		mpData = apData;
		mpData->PreloadSounds();

		for(int i=0; i<3; i++)
		{
			mvSoundEntries[i] = NULL;
			mvSoundEntryID[i]=-1;
		}

		mfIntervalCount = mpData->GetInterval();

		mfVolume = mpData->GetVolume();

		mfMinDistance = mpData->GetMinDistance();
		mfMaxDistance = mpData->GetMaxDistance();

		mBoundingVolume.SetSize(mfMaxDistance*2);

		mbStopped = false; //If the sound should be stopped
		mbStarted = false; //If the sound started playing-

		mbSkipStartEnd = false; //If the end and start sounds should be skipped.

		mbPrioRemove = false; //If the sounds was removed because too many where playing

		mbFadingOut = false; //If the sound is fading out.

		mbOutOfRange = false; //If the sound is out of range.

		mbLog = false;

		mfSleepCount = 0;

		mbForcePlayAsGUISound = false;

		mpSoundCallback->mpEntity = this;

		if(mpSoundHandler->GetSilent())
		{
			mbStarted = true;
			mbStopped = true;
			mfVolume =0;
			mbSkipStartEnd = true;
			mbRemoveWhenOver = true;
		}

		//if( mpData->GetName() == "scrape_wood.snt") mbLog = true;
		if(mbLog)Log("Created %d\n", this);
	}*/
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CALLABCK
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cSoundEntityChannelCallback::OnPriorityRelease()
	{
		ohpl_snd_priority_release(&mpEntity->mState);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSoundEntry* cSoundEntity::GetSoundEntry(eSoundEntityType aType, bool abCheckEntryValidity)
	{
		if(abCheckEntryValidity && mvSoundEntries[aType])
		{
			if(mpSoundHandler->IsValid(mvSoundEntries[aType], mvSoundEntryID[aType])==false)
			{
				return NULL;
			}
		}

		return mvSoundEntries[aType];
	}

	//-----------------------------------------------------------------------
	void cSoundEntity::Play(bool abPlayStart)
	{
		if(mpEvent){ mState.stopped = false; mpEvent->Play(); return; }

		OhplSoundVt vt = GetVt();
		const OhplSoundCfg cfg = GetCfg();
		ohpl_snd_play(&mState, &vt, &cfg, abPlayStart);
	}

	//-----------------------------------------------------------------------

	void cSoundEntity::Stop(bool abPlayEnd)
	{
		if(mpEvent){ mState.stopped = true; mpEvent->Stop(abPlayEnd); return; }

		OhplSoundVt vt = GetVt();
		const OhplSoundCfg cfg = GetCfg();
		ohpl_snd_stop(&mState, &vt, &cfg, abPlayEnd);
	}

	//-----------------------------------------------------------------------

	void cSoundEntity::FadeIn(float afSpeed)
	{
		if(mpEvent){ mState.stopped = false; mState.fading_out = false; mpEvent->FadeIn(afSpeed); return; }

		OhplSoundVt vt = GetVt();
		const OhplSoundCfg cfg = GetCfg();
		ohpl_snd_fade_in(&mState, &vt, &cfg, afSpeed);
	}

	//-----------------------------------------------------------------------

	void cSoundEntity::FadeOut(float afSpeed)
	{
		if(mpEvent){ mState.fading_out = true; mpEvent->FadeOut(afSpeed); return; }

		OhplSoundVt vt = GetVt();
		ohpl_snd_fade_out(&mState, &vt, afSpeed);
	}

	//-----------------------------------------------------------------------

	bool cSoundEntity::IsStopped()
	{
		if(mpEvent) return mpEvent->IsStopped();
		return ohpl_snd_is_stopped(&mState, mvSoundEntries[eSoundEntityType_Stop] != NULL);
	}

	bool cSoundEntity::IsFadingOut()
	{
		return mState.fading_out;
	}

	//-----------------------------------------------------------------------

	bool cSoundEntity::GetRemoveWhenOver()
	{
		return mState.remove_when_over;
	}

	//-----------------------------------------------------------------------

	void cSoundEntity::UpdateLogic(float afTimeStep)
	{
		if(mpEvent){ mpEvent->Update(afTimeStep); return; }

		OhplSoundVt vt = GetVt();
		const OhplSoundCfg cfg = GetCfg();
		ohpl_snd_update(&mState, &vt, &cfg, afTimeStep, mfMaxDistance);
	}

	//-----------------------------------------------------------------------

	void cSoundEntity::AddGlobalCallback(iSoundEntityGlobalCallback *apCallback)
	{
		mlstGobalCallbacks.push_back(apCallback);
	}

	//-----------------------------------------------------------------------

	void cSoundEntity::RemoveGlobalCallback(iSoundEntityGlobalCallback *apCallback)
	{
		tSoundEntityGlobalCallbackListIt it = mlstGobalCallbacks.begin();
		for(; it != mlstGobalCallbacks.end(); ++it)
		{
			if(apCallback == *it){
				mlstGobalCallbacks.erase(it);
				return;
			}
		}
	}
	
	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	OhplSoundCfg cSoundEntity::GetCfg()
	{
		OhplSoundCfg cfg;
		cfg.interval = mpData->GetInterval();
		cfg.random = mpData->GetRandom();
		cfg.looping = mpData->GetLoop();
		cfg.use_3d = mpData->GetUse3D();
		cfg.has_start = mpData->HasSound(eSoundEntityType_Start);
		return cfg;
	}

	//-----------------------------------------------------------------------

	struct cSoundEntityHost
	{
		static cSoundEntity* E(void *apCtx){ return static_cast<cSoundEntity*>(apCtx); }

		static bool Silent(void *c){ return E(c)->mpSoundHandler->GetSilent(); }
		static bool HasEntry(void *c, int32_t t){ return E(c)->mvSoundEntries[t] != NULL; }
		static bool IsValid(void *c, int32_t t)
		{
			cSoundEntity *e = E(c);
			return e->mpSoundHandler->IsValid(e->mvSoundEntries[t], e->mvSoundEntryID[t]);
		}
		static void Clear(void *c, int32_t t){ E(c)->mvSoundEntries[t] = NULL; }
		static void StopEntry(void *c, int32_t t){ E(c)->mvSoundEntries[t]->Stop(); }
		static void FadeIn(void *c, int32_t t, float afSpeed){ E(c)->mvSoundEntries[t]->FadeIn(1, afSpeed); }
		static void FadeOut(void *c, int32_t t, float afSpeed){ E(c)->mvSoundEntries[t]->FadeOut(afSpeed); }
		static void UpdatePosition(void *c, int32_t t)
		{
			cSoundEntity *e = E(c);
			cSoundEntry *pEntry = e->mvSoundEntries[t];
			if(!pEntry->GetChannel()->GetPositionIsRelative())
				pEntry->GetChannel()->SetPosition(e->GetWorldPosition());
		}
		static int32_t PlaySound(void *c, int32_t t, bool abLoop){ return E(c)->PlaySound((eSoundEntityType)t, abLoop); }
		static void OnStart(void *c)
		{
			tSoundEntityGlobalCallbackListIt it = cSoundEntity::mlstGobalCallbacks.begin();
			for(; it != cSoundEntity::mlstGobalCallbacks.end(); ++it)
			{
				iSoundEntityGlobalCallback *pCallback = *it;
				pCallback->OnStart(E(c));
			}
		}
		static bool Emitter(void *c){ return E(c)->mpWorld->IsSoundEmitter(); }
		static float ListenerSqrDist(void *c){ return E(c)->GetListenerSqrLength(); }
	};

	OhplSoundVt cSoundEntity::GetVt()
	{
		OhplSoundVt vt;
		vt.ctx = this;
		vt.silent = cSoundEntityHost::Silent;
		vt.has_entry = cSoundEntityHost::HasEntry;
		vt.is_valid = cSoundEntityHost::IsValid;
		vt.clear_entry = cSoundEntityHost::Clear;
		vt.stop_entry = cSoundEntityHost::StopEntry;
		vt.fade_in = cSoundEntityHost::FadeIn;
		vt.fade_out = cSoundEntityHost::FadeOut;
		vt.update_position = cSoundEntityHost::UpdatePosition;
		vt.play_sound = cSoundEntityHost::PlaySound;
		vt.on_start = cSoundEntityHost::OnStart;
		vt.emitter = cSoundEntityHost::Emitter;
		vt.listener_sqr_dist = cSoundEntityHost::ListenerSqrDist;
		return vt;
	}

	//-----------------------------------------------------------------------

	// 0 played, 1 no sound name, 2 failed, 3 failed for lack of channels
	int cSoundEntity::PlaySound(eSoundEntityType aType,bool abLoop)
	{
		tString sSoundName = mpData->GetRandomSoundName(aType,true); //TODO: Add a var instead of null!
		if(sSoundName == "") return 1;

		bool bNotEnoughChannels = false;

		if(OhplBackendRustRequested())
		{
			const uint32_t lRoute = ohpl_h12b_snd_play_route(mbForcePlayAsGUISound, mpData->GetUse3D());
			const bool bEntityRange = (lRoute & 4) != 0;
			mvSoundEntries[aType] = mpSoundHandler->Play3D(
										sSoundName, abLoop,
										mfVolume, bEntityRange ? GetWorldPosition() : cVector3f(0,0,1),
										bEntityRange ? mfMinDistance : 100.0f, bEntityRange ? mfMaxDistance : 200.0f,
										(lRoute & 1) ? eSoundEntryType_Gui : eSoundEntryType_World, (lRoute & 2) != 0, mpData->GetPriority(),
										mpData->GetStream(), &bNotEnoughChannels);
		}
		else
		if(mbForcePlayAsGUISound)
		{
			mvSoundEntries[aType] = mpSoundHandler->Play3D(
										sSoundName, abLoop,
										mfVolume, cVector3f(0,0,1),
										100.0f, 200.0f,	//Just set something so that sound is always inside min.
										eSoundEntryType_Gui,true, mpData->GetPriority(),
										mpData->GetStream(), &bNotEnoughChannels);
		}
		else
		{
			if(mpData->GetUse3D())
			{
				mvSoundEntries[aType] = mpSoundHandler->Play3D(
										sSoundName, abLoop,
										mfVolume, GetWorldPosition(),
										mfMinDistance, mfMaxDistance,
										eSoundEntryType_World,false, mpData->GetPriority(),
										mpData->GetStream(), &bNotEnoughChannels);
			}
			else
			{
				mvSoundEntries[aType] = mpSoundHandler->Play3D(
										sSoundName, abLoop,
										mfVolume, cVector3f(0,0,1),
										100.0f, 200.0f,	//Just set something so that sound is always inside min.
										eSoundEntryType_World,true, mpData->GetPriority(),
										mpData->GetStream(), &bNotEnoughChannels);
			}
		}

		if(mvSoundEntries[aType]==NULL)
		{
			if(bNotEnoughChannels==false)
				Error("Couldn't play sound '%s' for sound entity %s\n",sSoundName.c_str(),msName.c_str());

			return bNotEnoughChannels ? 3 : 2;
		}

		mvSoundEntryID[aType] = mvSoundEntries[aType]->GetId();

		iSoundChannel *pChannel = mvSoundEntries[aType]->GetChannel();
		pChannel->SetBlockable(mpData->GetBlockable());
		pChannel->SetBlockVolumeMul(mpData->GetBlockVolumeMul());
		pChannel->SetPriorityModifier(mpData->GetPriority());

		if(aType==eSoundEntityType_Main)
		{
			mvSoundEntries[aType]->SetCallBack(mpSoundCallback);
		}

		return 0;
	}

	//-----------------------------------------------------------------------

	float cSoundEntity::GetListenerSqrLength()
	{
		cVector3f vListenerPos = mpWorld->GetSound()->GetLowLevel()->GetListenerPosition();
		return cMath::Vector3DistSqr(vListenerPos,GetWorldPosition());
	}

	//-----------------------------------------------------------------------

}
