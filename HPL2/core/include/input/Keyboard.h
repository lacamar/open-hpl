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

#ifndef HPL_KEYBOARD_H
#define HPL_KEYBOARD_H

#include "input/RustInput.h"

#include "input/InputTypes.h"
#include "input/InputDevice.h"

struct OhplKeyboard;

namespace hpl {

	//------------------------------

	class iKeyboard : public iInputDevice
	{
	public:
		iKeyboard(tString asName);
		virtual ~iKeyboard(){}
		
		/**
		 * 
		 * \param aKey The key to check 
		 * \return true if pressed else false 
		 */
		virtual bool KeyIsDown(eKey aKey)=0;
		/**
		 * Can be checked many times to see all key presses
		 * \return key that is currently pressed. eKey_NONE is no key. 
		 */
		virtual cKeyPress GetKey()=0;
		/**
		 *
		 * \return If ANY key is pressed
		 */
		virtual bool KeyIsPressed()=0;
		/**
		 * \todo Implement!
		 * \return The current modifiers.
		 */
		virtual int GetModifier();
		/**
		 *
		 * \return key that has been released
		 */
		virtual cKeyPress GetReleasedKey()=0;
		/**
		 *
		 * \return If ANY key is released
		 */
		virtual bool KeyIsReleased()=0;
		/**
		 * \param eKey The key to change to string.
		 * \return The name of the key as a string. 
		 */
		virtual tString KeyToString(eKey);

		virtual const OhplKeyboard* RustHandle(){ return 0;}
		/**
		 * \param eKeyModifier The mod key to change to string.
		 * \return The name of the mod key as a string. 
		 */
		virtual tString ModifierKeyToString(eKeyModifier);
		/**
		 * \param tString Name of the key
		 * \return enum of the key.
		 */
		virtual eKey StringToKey(const tString&);
	};


inline iKeyboard::iKeyboard(tString asName) : iInputDevice(asName, eInputDeviceType_Keyboard) { }
inline int iKeyboard::GetModifier() { int lMod = 0; lMod |= (KeyIsDown(eKey_RightCtrl) || KeyIsDown(eKey_LeftCtrl))? eKeyModifier_Ctrl : 0; lMod |= (KeyIsDown(eKey_RightShift) || KeyIsDown(eKey_LeftShift))? eKeyModifier_Shift : 0; lMod |= (KeyIsDown(eKey_RightAlt) || KeyIsDown(eKey_LeftAlt))? eKeyModifier_Alt : 0; return lMod; }
inline tString iKeyboard::KeyToString(eKey aKey) { return OhplInputName(ohpl_input_key_name, aKey); }
inline tString iKeyboard::ModifierKeyToString(eKeyModifier aModKey) { return OhplInputName(ohpl_input_modifier_name, aModKey); }
inline eKey iKeyboard::StringToKey(const tString& asKey) { return (eKey)OhplInputFromName(ohpl_input_key_from_name, asKey); }

};

#endif // HPL_KEYBOARD_H
