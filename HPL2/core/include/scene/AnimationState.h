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
						cAnimationManager *apAnimationManager);
		~cAnimationState();

        const tString& GetName(){ return msName;}

		void Update(float afTimeStep);

		bool DataIsInMeshFile(){return mpAnimationManager==NULL;}

		bool IsFading(){ return mCore.fade_step != 0;}

		/**
		 * If the animation has reached the end.
		 */
		bool IsOver();

		void FadeIn(float afTime);
		void FadeOut(float afTime);

		void SetLength(float afLength){ mCore.length = afLength;}
		float GetLength(){ return mCore.length;}

		void SetWeight(float afWeight){ mCore.weight = afWeight;}
		float GetWeight(){ return mCore.weight;}

        void SetSpeed(float afSpeed){ mCore.speed = afSpeed; mCore.speed_dest = afSpeed;}
		void FadeSpeed(float afDest, float afTime);
		float GetSpeed(){ return mCore.speed;}

		void SetBaseSpeed(float afSpeed){ mCore.base_speed = afSpeed;}
		float GetBaseSpeed(){ return mCore.base_speed;}

		void SetTimePosition(float afPosition);
        float GetTimePosition(){ return mCore.time_pos;}
		float GetPreviousTimePosition(){ return mCore.prev_time_pos;}

		/**
		 * Set the relative postion. 0 = start, 1 = end
		 * \param afPosition
		 */
		void SetRelativeTimePosition(float afPosition);

		/**
		* Get the relative postion. 0 = start, 1 = end
		*/
		float GetRelativeTimePosition();

		bool IsActive(){ return mCore.active;}
		void SetActive(bool abActive);

		bool IsLooping(){ return mCore.looping;}
		void SetLoop(bool abLoop){ mCore.looping = abLoop;}

		bool IsPaused(){ return mCore.paused;}
		void SetPaused(bool abPaused){ mCore.paused = abPaused;}

		void SetSpecialEventTime(float afT){ mCore.special_event_time = afT;}
		float GetSpecialEventTime(){ return mCore.special_event_time;}
		bool IsAfterSpecialEvent(){ return mCore.time_pos > mCore.special_event_time;}
		bool IsBeforeSpecialEvent(){ return mCore.time_pos < mCore.special_event_time;}

		void AddTimePosition(float afAdd);

		cAnimation* GetAnimation();

		cAnimationEvent *CreateEvent();
		cAnimationEvent *GetEvent(int alIdx);
		int GetEventNum();

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
