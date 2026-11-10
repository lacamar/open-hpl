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

#include "scene/AnimationState.h"

#include "math/Math.h"
#include "graphics/Animation.h"
#include "resources/AnimationManager.h"

#include "system/LowLevelSystem.h"


namespace hpl {

	cAnimationState::cAnimationState(cAnimation* apAnimation, const tString &asName,
									cAnimationManager *apAnimationManager)
	{
		mpAnimation = apAnimation;
		mpAnimationManager = apAnimationManager;
		msName = asName;
		ohpl_anim_core_init(&mCore, mpAnimation->GetLength());
	}

	cAnimationState::~cAnimationState()
	{
		STLDeleteAll(mvEvents);

		//if no animation manger, then it means that the animation should not be destroyed by the state.
		if(mpAnimationManager)
		{
			mpAnimationManager->Destroy(mpAnimation);
		}
	}

	void cAnimationState::Update(float afTimeStep) { ohpl_anim_update(&mCore, afTimeStep); }

	bool cAnimationState::IsOver() { return ohpl_anim_is_over(&mCore); }

	void cAnimationState::FadeIn(float afTime) { ohpl_anim_fade_in(&mCore, afTime); }
	void cAnimationState::FadeOut(float afTime) { ohpl_anim_fade_out(&mCore, afTime); }

	// Rebirth cAnimationState::FadeSpeed 0xf60240
	void cAnimationState::FadeSpeed(float afDest, float afTime) { ohpl_anim_fade_speed(&mCore, afDest, afTime); }

	void cAnimationState::SetTimePosition(float afPosition) { ohpl_anim_set_time(&mCore, afPosition); }

	void cAnimationState::SetRelativeTimePosition(float afPosition) { ohpl_anim_set_relative_time(&mCore, afPosition); }
	float cAnimationState::GetRelativeTimePosition() { return ohpl_anim_relative_time(&mCore); }

	void cAnimationState::SetActive(bool abActive) { ohpl_anim_set_active(&mCore, abActive); }

	void cAnimationState::AddTimePosition(float afAdd) { ohpl_anim_add_time(&mCore, afAdd); }

	cAnimation* cAnimationState::GetAnimation()
	{
		return mpAnimation;
	}

	cAnimationEvent *cAnimationState::CreateEvent()
	{
		cAnimationEvent *pEvent = hplNew( cAnimationEvent, () );
		pEvent->mfTime =0;
		pEvent->mType = eAnimationEventType_LastEnum;
		pEvent->msValue = "";
		mvEvents.push_back(pEvent);

		return pEvent;
	}

	cAnimationEvent *cAnimationState::GetEvent(int alIdx)
	{
		return mvEvents[alIdx];
	}

	int cAnimationState::GetEventNum()
	{
		return (int)mvEvents.size();
	}
}
