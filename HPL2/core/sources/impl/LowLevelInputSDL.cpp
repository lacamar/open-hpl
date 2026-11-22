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

#include "impl/LowLevelInputSDL.h"

#include "impl/MouseSDL.h"
#include "impl/KeyboardSDL.h"
#include "impl/GamepadSDL2.h"
#include "impl/RustL1.h"

#include "system/LowLevelSystem.h"
#include "graphics/LowLevelGraphics.h"

#include "engine/Engine.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_syswm.h"


namespace hpl {

	static bool L1EventFilter(void *apCtx, const SDL_Event *apEvent)
	{
		cLowLevelInputSDL *pInput = static_cast<cLowLevelInputSDL*>(apCtx);
		return pInput->mEventFilter && pInput->mEventFilter(*apEvent);
	}

	static void L1EventPush(void *apCtx, const SDL_Event *apEvent)
	{
		static_cast<cLowLevelInputSDL*>(apCtx)->mlstEvents.push_back(*apEvent);
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cLowLevelInputSDL::cLowLevelInputSDL(iLowLevelGraphics *apLowLevelGraphics)
        : mpLowLevelGraphics(apLowLevelGraphics), mbQuitMessagePosted(false)
	{
		LockInput(true);
		RelativeMouse(false);
		ohpl_l1_input_init();
	}

	//-----------------------------------------------------------------------

	cLowLevelInputSDL::~cLowLevelInputSDL()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHOD
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cLowLevelInputSDL::LockInput(bool abX)
	{
		mpLowLevelGraphics->SetWindowGrab(abX);
	}

	void cLowLevelInputSDL::RelativeMouse(bool abX)
	{
		mpLowLevelGraphics->SetRelativeMouse(abX);
	}
	
	//-----------------------------------------------------------------------

	void cLowLevelInputSDL::BeginInputUpdate()
	{
		mlstEvents.clear();
		unsigned lFlags = ohpl_l1_poll(L1EventFilter, L1EventPush, this);
		if(lFlags & kOhplL1PollPlugged) cEngine::SetDeviceWasPlugged();
		if(lFlags & kOhplL1PollRemoved) cEngine::SetDeviceWasRemoved();
		if(lFlags & kOhplL1PollQuit) mbQuitMessagePosted = true;
		for(const SDL_Event &ev : mlstInjected)
			if(ohpl_l1_filter_keeps(L1EventFilter, this, &ev)) mlstEvents.push_back(ev);
		mlstInjected.clear();
	}

	//-----------------------------------------------------------------------

	void cLowLevelInputSDL::EndInputUpdate()
	{
		
	}

	//-----------------------------------------------------------------------

	void cLowLevelInputSDL::InitGamepadSupport()
	{
	}

	void cLowLevelInputSDL::DropGamepadSupport()
	{
	}

	int cLowLevelInputSDL::GetPluggedGamepadNum()
	{
		return ohpl_l1_joystick_count();
	}

	//-----------------------------------------------------------------------
	
	iMouse* cLowLevelInputSDL::CreateMouse()
	{
		return hplNew( cMouseSDL,(this));
	}
	
	//-----------------------------------------------------------------------
	
	iKeyboard* cLowLevelInputSDL::CreateKeyboard()
	{
		return hplNew( cKeyboardSDL,(this) );
	}

	//-----------------------------------------------------------------------

	iGamepad* cLowLevelInputSDL::CreateGamepad(int alIndex)
	{
		return hplNew( cGamepadSDL2, (this, alIndex) );
	}
	
	//-----------------------------------------------------------------------

    bool cLowLevelInputSDL::isQuitMessagePosted()
    {
        return mbQuitMessagePosted;
    }
    
    void cLowLevelInputSDL::resetQuitMessagePosted()
    {
        mbQuitMessagePosted = false;
    }

	//-----------------------------------------------------------------------
    
}
