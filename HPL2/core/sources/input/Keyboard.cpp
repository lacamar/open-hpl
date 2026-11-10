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

#include "input/Keyboard.h"
#include "input/InputTypes.h"
#include "input/RustInput.h"

namespace hpl
{
	
	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iKeyboard::iKeyboard(tString asName) : iInputDevice(asName, eInputDeviceType_Keyboard)
	{
	
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	int iKeyboard::GetModifier()
	{
		int lMod = 0;
		lMod |= (KeyIsDown(eKey_RightCtrl) || KeyIsDown(eKey_LeftCtrl))? eKeyModifier_Ctrl : 0;
		lMod |= (KeyIsDown(eKey_RightShift) || KeyIsDown(eKey_LeftShift))? eKeyModifier_Shift : 0;
		lMod |= (KeyIsDown(eKey_RightAlt) || KeyIsDown(eKey_LeftAlt))? eKeyModifier_Alt : 0;

		return lMod;
	}

	//-----------------------------------------------------------------------

	tString iKeyboard::KeyToString(eKey aKey)
	{
		return OhplInputName(ohpl_input_key_name, aKey);
	}

	//-----------------------------------------------------------------------

	tString iKeyboard::ModifierKeyToString(eKeyModifier aModKey)
	{
		return OhplInputName(ohpl_input_modifier_name, aModKey);
	}

	//-----------------------------------------------------------------------

	eKey iKeyboard::StringToKey(const tString& asKey)
	{
		return (eKey)OhplInputFromName(ohpl_input_key_from_name, asKey);
	}

	//-----------------------------------------------------------------------
}
