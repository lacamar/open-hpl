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

#include "impl/KeyboardSDL.h"

#include "SDL2/SDL.h"

#include "impl/LowLevelInputSDL.h"
#include "impl/RustInputSDL.h"

namespace hpl {

	cKeyboardSDL::cKeyboardSDL(cLowLevelInputSDL *apLowLevelInputSDL) : iKeyboard("SDL Portable Keyboard")
	{
		mpLowLevelInputSDL = apLowLevelInputSDL;
		mpRust = ohpl_input_kb_new();
	}

	cKeyboardSDL::~cKeyboardSDL()
	{
		ohpl_input_kb_free(mpRust);
	}

	void cKeyboardSDL::Update()
	{
		OhplConvertEvents(mpLowLevelInputSDL->mlstEvents, mvEvents);
		ohpl_input_kb_update(mpRust, mvEvents.data(), mvEvents.size(), ohpl_l1_mod_state());
	}

	bool cKeyboardSDL::KeyIsDown(eKey aKey)
	{
		return ohpl_input_kb_key_is_down(mpRust, aKey);
	}

	cKeyPress cKeyboardSDL::GetKey()
	{
		cKeyPress key(eKey_None, 0, 0);
		ohpl_input_kb_pop_pressed(mpRust, &key);
		return key;
	}

	bool cKeyboardSDL::KeyIsPressed()
	{
		return ohpl_input_kb_key_is_pressed(mpRust);
	}

	cKeyPress cKeyboardSDL::GetReleasedKey()
	{
		cKeyPress key(eKey_None, 0, 0);
		ohpl_input_kb_pop_released(mpRust, &key);
		return key;
	}

	bool cKeyboardSDL::KeyIsReleased()
	{
		return ohpl_input_kb_key_is_released(mpRust);
	}

}
