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

#include "impl/MouseSDL.h"

#include <cstdlib>

#include "SDL2/SDL.h"

#include "impl/LowLevelInputSDL.h"
#include "impl/RustInputSDL.h"

namespace hpl {

	cMouseSDL::cMouseSDL(cLowLevelInputSDL *apLowLevelInputSDL) : iMouse("SDL Portable Mouse")
	{
		mpLowLevelInputSDL = apLowLevelInputSDL;
		mpRust = ohpl_input_mouse_new();
	}

	cMouseSDL::~cMouseSDL()
	{
		ohpl_input_mouse_free(mpRust);
	}

	void cMouseSDL::Update()
	{
		//On first update make sure to clear all previous events.
		if(ohpl_input_mouse_take_first_time(mpRust))
		{
			if(OhplBackendRustRequested()) ohpl_l1_mouse_flush();
			else for(int i=0; i<10; ++i)
			{
				SDL_PumpEvents();
				int lX,lY;
				SDL_GetRelativeMouseState(&lX, &lY);
			}
		}

		static const bool bHeadless = getenv("OPENHPL_HEADLESS_SOCKET") != NULL;

		OhplConvertEvents(mpLowLevelInputSDL->mlstEvents, mvEvents);
		int lX,lY;
		if(OhplBackendRustRequested()) ohpl_l1_mouse_relative(&lX, &lY);
		else SDL_GetRelativeMouseState(&lX, &lY);
		OhplVec2l vRel = {lX, lY};
		ohpl_input_mouse_update(mpRust, mvEvents.data(), mvEvents.size(), bHeadless, vRel);
	}

	bool cMouseSDL::ButtonIsDown(eMouseButton mButton)
	{
		return ohpl_input_mouse_button_is_down(mpRust, mButton);
	}

	cVector2l cMouseSDL::GetAbsPosition()
	{
		OhplVec2l v = ohpl_input_mouse_abs(mpRust);
		return cVector2l(v.x, v.y);
	}

	cVector2l cMouseSDL::GetRelPosition()
	{
		OhplVec2l v = ohpl_input_mouse_take_rel(mpRust);
		return cVector2l(v.x, v.y);
	}

}
