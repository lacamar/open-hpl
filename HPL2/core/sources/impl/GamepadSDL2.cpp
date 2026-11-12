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
#include "impl/GamepadSDL2.h"

#include "impl/LowLevelInputSDL.h"
#include "impl/RustInputSDL.h"
#include "impl/RustBackends.h"
#include "impl/RustL1.h"
#include "system/Platform.h"

namespace hpl {

	static void L1NameSink(void *apCtx, const unsigned char *apData, size_t alLen)
	{
		static_cast<tString*>(apCtx)->assign(reinterpret_cast<const char*>(apData), alLen);
	}

	cGamepadSDL2::cGamepadSDL2(cLowLevelInputSDL *apLowLevelInputSDL, int alIndex) : iGamepad("SDL Portable Gamepad", alIndex)
	{
		mpLowLevelInputSDL = apLowLevelInputSDL;
		mpHandle = NULL;
		mpHaptic = NULL;
		mpRustPad = NULL;
		mlInstance = 0;

		size_t lButtons = 0, lAxes = 0;
		if(OhplBackendRustRequested())
		{
			OhplL1PadInfo info;
			mpRustPad = ohpl_l1_pad_open(mlIndex, &info, L1NameSink, &msGamepadName);
			mlInstance = info.instance;
			lButtons = info.buttons;
			lAxes = info.axes;
		}
		else mpHandle = SDL_GameControllerOpen(mlIndex);
		if(mpHandle)
		{
			SDL_Joystick *joy = SDL_GameControllerGetJoystick(mpHandle);
			mlInstance = SDL_JoystickInstanceID(joy);
			msGamepadName = tString(SDL_GameControllerName(mpHandle));
			lButtons = SDL_CONTROLLER_BUTTON_MAX;
			lAxes = SDL_CONTROLLER_AXIS_MAX;

			mpHaptic = SDL_HapticOpenFromJoystick(joy);
			if(SDL_HapticRumbleInit(mpHaptic) != 0)
			{
				if(mpHaptic) SDL_HapticClose(mpHaptic);
				mpHaptic = NULL;
			}
		}
		mpRust = ohpl_input_pad_new(mlIndex, mlInstance, lButtons, lAxes, SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_AXIS_MAX);
	}

	cGamepadSDL2::~cGamepadSDL2()
	{
		if(mpRustPad) ohpl_l1_pad_free(mpRustPad);
		if(mpHaptic)
		{
			SDL_HapticRumbleStop(mpHaptic);
			SDL_HapticClose(mpHaptic);
		}
		if(mpHandle) SDL_GameControllerClose(mpHandle);
		ohpl_input_pad_free(mpRust);
	}

	int cGamepadSDL2::GetNumButtons()
	{
		return (int)ohpl_input_pad_num_buttons(mpRust);
	}

	int cGamepadSDL2::GetNumAxes()
	{
		return (int)ohpl_input_pad_num_axes(mpRust);
	}

	int cGamepadSDL2::GetNumHats()
	{
		return 0;
	}

	int cGamepadSDL2::GetNumBalls()
	{
		return 0;
	}

	void cGamepadSDL2::ClearState()
	{
		ohpl_input_pad_clear_state(mpRust);
	}

	float cGamepadSDL2::GetTimeSinceLastActive()
	{
		return ohpl_input_pad_time_since_active(mpRust, cPlatform::GetApplicationTime());
	}

	void cGamepadSDL2::Update()
	{
		OhplConvertEvents(mpLowLevelInputSDL->mlstEvents, mvEvents);
		ohpl_input_pad_update(mpRust, mvEvents.data(), mvEvents.size(), cPlatform::GetApplicationTime());
	}

	bool cGamepadSDL2::HasInputUpdates()
	{
		return ohpl_input_pad_has_updates(mpRust);
	}

	cGamepadInputData cGamepadSDL2::GetInputUpdate()
	{
		cGamepadInputData input;
		ohpl_input_pad_pop_update(mpRust, &input);
		return input;
	}

	bool cGamepadSDL2::ButtonIsDown(eGamepadButton aButton)
	{
		return ohpl_input_pad_button_is_down(mpRust, aButton);
	}

	cGamepadInputData cGamepadSDL2::GetButton()
	{
		cGamepadInputData button;
		ohpl_input_pad_pop_button(mpRust, &button);
		return button;
	}

	bool cGamepadSDL2::ButtonIsPressed()
	{
		return ohpl_input_pad_button_is_pressed(mpRust);
	}

	cGamepadInputData cGamepadSDL2::GetReleasedButton()
	{
		cGamepadInputData button;
		ohpl_input_pad_pop_released(mpRust, &button);
		return button;
	}

	bool cGamepadSDL2::ButtonIsReleased()
	{
		return ohpl_input_pad_button_is_released(mpRust);
	}

	float cGamepadSDL2::GetAxisValue(eGamepadAxis aAxis)
	{
		return ohpl_input_pad_axis_value(mpRust, aAxis);
	}

	float cGamepadSDL2::GetAxisDeadZoneRadiusValue()
	{
		return ohpl_input_pad_dead_zone();
	}

	void cGamepadSDL2::SetAxisDeadZoneRadiusValue(float afValue)
	{
		ohpl_input_pad_set_dead_zone(afValue);
	}

	cGamepadInputData cGamepadSDL2::GetUpdatedAxis()
	{
		cGamepadInputData axis;
		ohpl_input_pad_pop_axis(mpRust, &axis);
		return axis;
	}

	bool cGamepadSDL2::AxesUpdated()
	{
		return ohpl_input_pad_axes_updated(mpRust);
	}

	eGamepadHatState cGamepadSDL2::GetHatCurrentState(eGamepadHat aHat)
	{
		return eGamepadHatState_Centered;
	}

	bool cGamepadSDL2::HatIsInState(eGamepadHat aHat, eGamepadHatState aState)
	{
		return (GetHatCurrentState(aHat)&aState)!=0;
	}

	bool cGamepadSDL2::HatsChanged()
	{
		return false;
	}

	cGamepadInputData cGamepadSDL2::GetHatState()
	{
		return cGamepadInputData();
	}

	cVector2l cGamepadSDL2::GetBallAbsPos(eGamepadBall aBall)
	{
		return cVector2l(0,0);
	}

	cVector2l cGamepadSDL2::GetBallRelPos(eGamepadBall aBall)
	{
		return cVector2l(0,0);
	}

	void cGamepadSDL2::SetRumble(float afValue, int alMillisec)
	{
		if(mpRustPad) { ohpl_l1_pad_rumble(mpRustPad, afValue, alMillisec); return; }
		if(mpHaptic)
		{
			if(afValue > 0) SDL_HapticRumblePlay(mpHaptic, afValue, alMillisec);
			else			SDL_HapticRumbleStop(mpHaptic);
		}
	}

}
