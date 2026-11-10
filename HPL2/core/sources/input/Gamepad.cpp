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

#include "input/Gamepad.h"
#include "input/InputTypes.h"
#include "input/RustInput.h"

namespace hpl
{
	
	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iGamepad::iGamepad(tString asName, int alIndex) : iInputDevice(asName, eInputDeviceType_Gamepad)
	{
		mlIndex = alIndex;	
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	eGamepadAxisRange iGamepad::GetAxisRangeFromValue(float afX)
	{
		return (eGamepadAxisRange)ohpl_input_pad_axis_range_from_value(afX);
	}

	tString iGamepad::ButtonToString(eGamepadButton aButton)
	{
		return OhplInputName(ohpl_input_pad_button_name, aButton);
	}

	//-----------------------------------------------------------------------

	eGamepadButton iGamepad::StringToButton(const tString& asButton)
	{
		return (eGamepadButton)OhplInputFromName(ohpl_input_pad_button_from_name, asButton);
	}

	//-----------------------------------------------------------------------

	tString iGamepad::AxisToString(eGamepadAxis aAxis)
	{
		return OhplInputName(ohpl_input_pad_axis_name, aAxis);
	}

	//-----------------------------------------------------------------------

	eGamepadAxis iGamepad::StringToAxis(const tString& asAxis)
	{
		return (eGamepadAxis)OhplInputFromName(ohpl_input_pad_axis_from_name, asAxis);
	}

	//-----------------------------------------------------------------------

	tString iGamepad::AxisRangeToString(eGamepadAxisRange aRange)
	{
		return OhplInputName(ohpl_input_pad_axis_range_name, aRange);
	}

	//-----------------------------------------------------------------------

	eGamepadAxisRange iGamepad::StringToAxisRange(const tString& asRange)
	{
		return (eGamepadAxisRange)OhplInputFromName(ohpl_input_pad_axis_range_from_name, asRange);
	}

	//-----------------------------------------------------------------------

	tString iGamepad::HatToString(eGamepadHat aHat)
	{
		return OhplInputName(ohpl_input_pad_hat_name, aHat);
	}

	//-----------------------------------------------------------------------

	eGamepadHat iGamepad::StringToHat(const tString& asHat)
	{
		return (eGamepadHat)OhplInputFromName(ohpl_input_pad_hat_from_name, asHat);
	}

	//-----------------------------------------------------------------------

	tString iGamepad::HatStateToString(eGamepadHatState aHatState)
	{
		return OhplInputName(ohpl_input_pad_hat_state_name, aHatState);
	}

	//-----------------------------------------------------------------------

	eGamepadHatState iGamepad::StringToHatState(const tString& asHatState)
	{
		return (eGamepadHatState)OhplInputFromName(ohpl_input_pad_hat_state_from_name, asHatState);
	}

	//-----------------------------------------------------------------------

	tString iGamepad::BallToString(eGamepadBall aBall)
	{
		return OhplInputName(ohpl_input_pad_ball_name, aBall);
	}

	//-----------------------------------------------------------------------

	eGamepadBall iGamepad::StringToBall(const tString& asBall)
	{
		return (eGamepadBall)OhplInputFromName(ohpl_input_pad_ball_from_name, asBall);
	}

	//-----------------------------------------------------------------------

}
