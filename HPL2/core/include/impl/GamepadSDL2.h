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

#ifndef HPL_GAMEPAD_SDL2_H
#define HPL_GAMEPAD_SDL2_H

#include "impl/LowLevelInputSDL.h"
#include "impl/RustInputSDL.h"
#include "system/Platform.h"

#include <vector>
#include <list>
#include "system/SystemTypes.h"
#include "input/Gamepad.h"
#include "input/RustInput.h"
#include "impl/RustL1.h"
//#include "input/InputTypes.h"

#include "SDL2/SDL.h"

namespace hpl {

	class cLowLevelInputSDL;

	class cGamepadSDL2 : public iGamepad
	{
	public:
		cGamepadSDL2(cLowLevelInputSDL *apLowLevelInputSDL, int alIndex);
		~cGamepadSDL2();

		tString GetGamepadName() { return msGamepadName; }

		int GetNumButtons();
		int GetNumAxes();
		int GetNumHats();
		int GetNumBalls();

		void Update();

		void ClearState();

		float GetTimeSinceLastActive();

		//Gamepad specific
		bool HasInputUpdates();
		cGamepadInputData GetInputUpdate();

		bool ButtonIsDown(eGamepadButton aButton);
		cGamepadInputData GetButton();
		bool ButtonIsPressed();
		bool ButtonIsReleased();
		cGamepadInputData GetReleasedButton();

		float GetAxisValue(eGamepadAxis aAxis);
		float GetAxisDeadZoneRadiusValue();
		void SetAxisDeadZoneRadiusValue(float afValue);
		bool AxesUpdated();
		cGamepadInputData GetUpdatedAxis();

		eGamepadHatState GetHatCurrentState(eGamepadHat aHat);
		bool HatIsInState(eGamepadHat aHat, eGamepadHatState aState);
		bool HatsChanged();
		cGamepadInputData GetHatState();

		cVector2l GetBallAbsPos(eGamepadBall aBall);
		cVector2l GetBallRelPos(eGamepadBall aBall);

		void SetRumble(float afValue, int alMillisec);
		const OhplGamepad* RustHandle(){ return mpRust;}
		//void SetColorLED(const cColor& aColor){}
		//cColor GetColorLED(){ return cColor(0,0); }
	private:
		tString msGamepadName;

		OhplGamepad *mpRust;
		std::vector<OhplInputEvent> mvEvents;

		SDL_JoystickID		mlInstance;
		cLowLevelInputSDL	*mpLowLevelInputSDL;
		OhplL1Pad* mpRustPad;
	};


static inline void L1NameSink(void *apCtx, const unsigned char *apData, size_t alLen) { static_cast<tString*>(apCtx)->assign(reinterpret_cast<const char*>(apData), alLen); }
inline cGamepadSDL2::cGamepadSDL2(cLowLevelInputSDL *apLowLevelInputSDL, int alIndex) : iGamepad("SDL Portable Gamepad", alIndex) { mpLowLevelInputSDL = apLowLevelInputSDL; size_t lButtons = 0, lAxes = 0; OhplL1PadInfo info; mpRustPad = ohpl_l1_pad_open(mlIndex, &info, L1NameSink, &msGamepadName); mlInstance = info.instance; lButtons = info.buttons; lAxes = info.axes; mpRust = ohpl_input_pad_new(mlIndex, mlInstance, lButtons, lAxes, SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_AXIS_MAX); }
inline cGamepadSDL2::~cGamepadSDL2() { if(mpRustPad) ohpl_l1_pad_free(mpRustPad); ohpl_input_pad_free(mpRust); }
inline int cGamepadSDL2::GetNumButtons() { return (int)ohpl_input_pad_num_buttons(mpRust); }
inline int cGamepadSDL2::GetNumAxes() { return (int)ohpl_input_pad_num_axes(mpRust); }
inline int cGamepadSDL2::GetNumHats() { return 0; }
inline int cGamepadSDL2::GetNumBalls() { return 0; }
inline void cGamepadSDL2::ClearState() { ohpl_input_pad_clear_state(mpRust); }
inline float cGamepadSDL2::GetTimeSinceLastActive() { return ohpl_input_pad_time_since_active(mpRust, cPlatform::GetApplicationTime()); }
inline void cGamepadSDL2::Update() { OhplConvertEvents(mpLowLevelInputSDL->mlstEvents, mvEvents); ohpl_input_pad_update(mpRust, mvEvents.data(), mvEvents.size(), cPlatform::GetApplicationTime()); }
inline bool cGamepadSDL2::HasInputUpdates() { return ohpl_input_pad_has_updates(mpRust); }
inline cGamepadInputData cGamepadSDL2::GetInputUpdate() { cGamepadInputData input; ohpl_input_pad_pop_update(mpRust, &input); return input; }
inline bool cGamepadSDL2::ButtonIsDown(eGamepadButton aButton) { return ohpl_input_pad_button_is_down(mpRust, aButton); }
inline cGamepadInputData cGamepadSDL2::GetButton() { cGamepadInputData button; ohpl_input_pad_pop_button(mpRust, &button); return button; }
inline bool cGamepadSDL2::ButtonIsPressed() { return ohpl_input_pad_button_is_pressed(mpRust); }
inline cGamepadInputData cGamepadSDL2::GetReleasedButton() { cGamepadInputData button; ohpl_input_pad_pop_released(mpRust, &button); return button; }
inline bool cGamepadSDL2::ButtonIsReleased() { return ohpl_input_pad_button_is_released(mpRust); }
inline float cGamepadSDL2::GetAxisValue(eGamepadAxis aAxis) { return ohpl_input_pad_axis_value(mpRust, aAxis); }
inline float cGamepadSDL2::GetAxisDeadZoneRadiusValue() { return ohpl_input_pad_dead_zone(); }
inline void cGamepadSDL2::SetAxisDeadZoneRadiusValue(float afValue) { ohpl_input_pad_set_dead_zone(afValue); }
inline cGamepadInputData cGamepadSDL2::GetUpdatedAxis() { cGamepadInputData axis; ohpl_input_pad_pop_axis(mpRust, &axis); return axis; }
inline bool cGamepadSDL2::AxesUpdated() { return ohpl_input_pad_axes_updated(mpRust); }
inline eGamepadHatState cGamepadSDL2::GetHatCurrentState(eGamepadHat aHat) { return eGamepadHatState_Centered; }
inline bool cGamepadSDL2::HatIsInState(eGamepadHat aHat, eGamepadHatState aState) { return (GetHatCurrentState(aHat)&aState)!=0; }
inline bool cGamepadSDL2::HatsChanged() { return false; }
inline cGamepadInputData cGamepadSDL2::GetHatState() { return cGamepadInputData(); }
inline cVector2l cGamepadSDL2::GetBallAbsPos(eGamepadBall aBall) { return cVector2l(0,0); }
inline cVector2l cGamepadSDL2::GetBallRelPos(eGamepadBall aBall) { return cVector2l(0,0); }
inline void cGamepadSDL2::SetRumble(float afValue, int alMillisec) { if(mpRustPad) ohpl_l1_pad_rumble(mpRustPad, afValue, alMillisec); }

};

#endif // HPL_GAMEPAD_SDL2_H
