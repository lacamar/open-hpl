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

#ifndef HPL_ANIMATION_STATE_H
#define HPL_ANIMATION_STATE_H

#include "math/MathTypes.h"
#include "system/SystemTypes.h"
#include "graphics/GraphicsTypes.h"

#include "engine/SaveGame.h"
#include "scene/RustMeshEntity.h"
#include "graphics/Animation.h"
#include "resources/AnimationManager.h"
#include "system/MemoryManager.h"

namespace hpl {

	class cAnimation;
	class cAnimationManager;

	//---------------------------------------------
	
	class cAnimationEvent
	{
	public:
		float mfTime;
		eAnimationEventType mType;
		tString msValue;
		tString msName;
		tString msDestSocket;
	};

	//---------------------------------------------

	class cAnimationState
	{
	public:
		cAnimationState(cAnimation* apAnimation,const tString &asName,
						cAnimationManager *apAnimationManager) : msName(asName), mpAnimationManager(apAnimationManager), mpAnimation(apAnimation)
		{
			ohpl_anim_core_init(&mCore, apAnimation->GetLength());
		}
		~cAnimationState()
		{
			STLDeleteAll(mvEvents);
			if(mpAnimationManager) mpAnimationManager->Destroy(mpAnimation);
		}

        const tString& GetName(){ return msName;}

		void Update(float afTimeStep){ ohpl_anim_update(&mCore, afTimeStep);}

		bool DataIsInMeshFile(){return mpAnimationManager==NULL;}

		bool IsFading(){ return mCore.fade_step != 0;}

		/**
		 * If the animation has reached the end.
		 */
		bool IsOver(){ return ohpl_anim_is_over(&mCore);}

		void FadeIn(float afTime){ ohpl_anim_fade_in(&mCore, afTime);}
		void FadeOut(float afTime){ ohpl_anim_fade_out(&mCore, afTime);}

		void SetLength(float afLength){ mCore.length = afLength;}
		float GetLength(){ return mCore.length;}

		void SetWeight(float afWeight){ mCore.weight = afWeight;}
		float GetWeight(){ return mCore.weight;}

        void SetSpeed(float afSpeed){ mCore.speed = afSpeed; mCore.speed_dest = afSpeed;}
		void FadeSpeed(float afDest, float afTime){ ohpl_anim_fade_speed(&mCore, afDest, afTime);}
		float GetSpeed(){ return mCore.speed;}

		void SetBaseSpeed(float afSpeed){ mCore.base_speed = afSpeed;}
		float GetBaseSpeed(){ return mCore.base_speed;}

		void SetTimePosition(float afPosition){ ohpl_anim_set_time(&mCore, afPosition);}
        float GetTimePosition(){ return mCore.time_pos;}
		float GetPreviousTimePosition(){ return mCore.prev_time_pos;}

		/**
		 * Set the relative postion. 0 = start, 1 = end
		 * \param afPosition
		 */
		void SetRelativeTimePosition(float afPosition){ ohpl_anim_set_relative_time(&mCore, afPosition);}

		/**
		* Get the relative postion. 0 = start, 1 = end
		*/
		float GetRelativeTimePosition(){ return ohpl_anim_relative_time(&mCore);}

		bool IsActive(){ return mCore.active;}
		void SetActive(bool abActive){ ohpl_anim_set_active(&mCore, abActive);}

		bool IsLooping(){ return mCore.looping;}
		void SetLoop(bool abLoop){ mCore.looping = abLoop;}

		bool IsPaused(){ return mCore.paused;}
		void SetPaused(bool abPaused){ mCore.paused = abPaused;}

		void SetSpecialEventTime(float afT){ mCore.special_event_time = afT;}
		float GetSpecialEventTime(){ return mCore.special_event_time;}
		bool IsAfterSpecialEvent(){ return mCore.time_pos > mCore.special_event_time;}
		bool IsBeforeSpecialEvent(){ return mCore.time_pos < mCore.special_event_time;}

		void AddTimePosition(float afAdd){ ohpl_anim_add_time(&mCore, afAdd);}

		cAnimation* GetAnimation(){ return mpAnimation;}

		cAnimationEvent *CreateEvent()
		{
			cAnimationEvent *pEvent = hplNew(cAnimationEvent,());
			pEvent->mfTime = 0;
			pEvent->mType = eAnimationEventType_LastEnum;
			mvEvents.push_back(pEvent);
			return pEvent;
		}
		cAnimationEvent *GetEvent(int alIdx){ return mvEvents[alIdx];}
		int GetEventNum(){ return (int)mvEvents.size();}

		float GetFadeStep(){ return mCore.fade_step;}
		void SetFadeStep(float afX){ mCore.fade_step = afX;}

		// Applied on top, not part of weight normalization
		bool IsLayer(){ return mCore.layer;}
		void SetLayer(bool abX){ mCore.layer = abX;}

		OhplAnimCore* GetCore(){ return &mCore;}

		// per entity: cAnimation tracks are shared between meshes with different skeletons
		std::vector<int> mvTrackNodeIndices;
	
	private:
		tString msName;

		cAnimationManager *mpAnimationManager;

		cAnimation* mpAnimation;

		std::vector<cAnimationEvent*> mvEvents;

		OhplAnimCore mCore;
	};

};
#endif // HPL_ANIMATION_STATE_H
