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

#ifndef HPL_SOUND_ENTITY_H
#define HPL_SOUND_ENTITY_H

#include <list>

#include "scene/Entity3D.h"
#include "sound/SoundTypes.h"
#include "sound/RustSound.h"
#include "sound/SoundHandler.h"
#include "sound/SoundEntityData.h"

namespace hpl {

	class cSoundEntityManager;
	class cWorld;

	class cSoundEntity;

	class iSoundEntityGlobalCallback
	{
	public:
		virtual void OnStart(cSoundEntity *apSoundEntity)=0;
	};

	class iSoundEntityEvent
	{
	public:
		virtual ~iSoundEntityEvent(){}
		virtual void Play()=0;
		virtual void Stop(bool abPlayEnd)=0;
		virtual void FadeIn(float afSpeed)=0;
		virtual void FadeOut(float afSpeed)=0;
		virtual bool IsStopped()=0;
		virtual void Update(float afTimeStep)=0;
		virtual void SetParam(const tString &asName, float afValue){}
		virtual void SetParam(int alIdx, float afValue){}
	};
	typedef iSoundEntityEvent* (*tSoundEntityEventFactory)(cSoundEntity *apEntity);

	typedef std::list<iSoundEntityGlobalCallback*> tSoundEntityGlobalCallbackList;
	typedef tSoundEntityGlobalCallbackList::iterator tSoundEntityGlobalCallbackListIt;

	//------------------------------------------

	/** The Entity3D shell of a sound entity; its state is a Rust object. */
	class cSoundEntity : public iEntity3D
	{
	#ifdef __GNUC__
		typedef iEntity3D __super;
	#endif
	public:
		cSoundEntity(const tString& asName,cSoundEntityData *apData,
						cSoundEntityManager *apSoundEntityManager,
						cWorld *apWorld,
						cSoundHandler *apSoundHandler, bool abRemoveWhenOver, int alCreationID)
			:iEntity3D(asName), mpData(apData)
		{
			mpRs = ohpl_sndse_new(apSoundHandler->Rs(), apData->Rs(), apData, this, apWorld, abRemoveWhenOver, alCreationID);
			mBoundingVolume.SetSize(ohpl_sndse_get(mpRs, 2)*2);
			if(mpEventFactory) ohpl_sndse_set_event(mpRs, mpEventFactory(this));
		}
		~cSoundEntity(){ ohpl_sndse_free(mpRs); }

		void Play(bool abPlayStart=true){ ohpl_sndse_play(mpRs, abPlayStart); }
		void Stop(bool abPlayEnd=true){ ohpl_sndse_stop(mpRs, abPlayEnd); }

		void FadeIn(float afSpeed){ ohpl_sndse_fade_in(mpRs, afSpeed); }
		void FadeOut(float afSpeed){ ohpl_sndse_fade_out(mpRs, afSpeed); }

        bool IsStopped(){ return ohpl_sndse_is_stopped(mpRs); }
		bool IsFadingOut(){ return ohpl_sndse_flag(mpRs, 0) != 0; }
		bool GetRemoveWhenOver(){ return ohpl_sndse_flag(mpRs, 1) != 0; }

		void SetVolume(float afX){ ohpl_sndse_set(mpRs, 0, afX);}
		float GetVolume(){ return ohpl_sndse_get(mpRs, 0);}

		float GetMinDistance(){ return ohpl_sndse_get(mpRs, 1);}
		float GetMaxDistance(){ return ohpl_sndse_get(mpRs, 2);}
		void SetMinDistance(float afX){ ohpl_sndse_set(mpRs, 1, afX);}
		void SetMaxDistance(float afX){ ohpl_sndse_set(mpRs, 2, afX);}

		void SetForcePlayAsGUISound(bool abX){ ohpl_sndse_set_force_gui(mpRs, abX);}
		bool GetForcePlayAsGUISound(){ return ohpl_sndse_flag(mpRs, 2) != 0;}

		/**
		 * Gets the sound entry. return can be NULL whether validity is checked or not, best is to ALWAYS check validity, unless the entry has been validated in same code block.
		 */
		cSoundEntry* GetSoundEntry(eSoundEntityType aType, bool abCheckEntryValidity){ return (cSoundEntry*)ohpl_sndse_entry(mpRs, aType, abCheckEntryValidity); }

        //Entity implementation
		void UpdateLogic(float afTimeStep){ ohpl_sndse_update(mpRs, afTimeStep); }

		tString GetEntityType(){ return "SoundEntity";}

		cSoundEntityData* GetData(){ return mpData;}

		int GetCreationID(){ return ohpl_sndse_flag(mpRs, 3); }

		static tSoundEntityGlobalCallbackList& GlobalCallbacks(){ static tSoundEntityGlobalCallbackList lst; return lst; }
		static void AddGlobalCallback(iSoundEntityGlobalCallback *apCallback){ GlobalCallbacks().push_back(apCallback); }
		static void RemoveGlobalCallback(iSoundEntityGlobalCallback *apCallback){ GlobalCallbacks().remove(apCallback); }

		static void SetEventFactory(tSoundEntityEventFactory apFactory){ mpEventFactory = apFactory; }
		iSoundEntityEvent* GetEvent(){ return (iSoundEntityEvent*)ohpl_sndse_event(mpRs); }
		void SetParam(const tString &asName, float afValue){ if(iSoundEntityEvent *pEvent = GetEvent()) pEvent->SetParam(asName, afValue); }
		void SetParam(int alIdx, float afValue){ if(iSoundEntityEvent *pEvent = GetEvent()) pEvent->SetParam(alIdx, afValue); }

		OhplSndEnt* RsPod() const { return mpRs;}

	private:
		OhplSndEnt *mpRs;
		cSoundEntityData *mpData;

		static inline tSoundEntityEventFactory mpEventFactory = NULL;
	};

};
#endif // HPL_SOUND_ENTITY_H
