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

#ifndef HPL_ACTION_H
#define HPL_ACTION_H

#include "input/Input.h"
#include "system/LowLevelSystem.h"

#include "system/SystemTypes.h"
#include "input/InputTypes.h"
#include "input/RustInput.h"

namespace hpl {


	class iSubAction
	{
	public:
		iSubAction(const OhplSubSpec &aSpec) : mSpec(aSpec) {}

		const OhplSubSpec& OhplSpec(){ return mSpec; }
		tString GetInputType(){ return OhplSubText(ohpl_input_sub_type, mSpec); }
		tString GetInputName(){ return OhplSubText(ohpl_input_sub_name, mSpec); }

	private:
		OhplSubSpec mSpec;
	};

	//---------------------------------------------------------

	typedef std::vector<iSubAction*> tSubActionVec;
	typedef tSubActionVec::iterator tSubActionVecIt;

	class cInput;

	//---------------------------------------------------------

	class cAction
	{
	public:
		cAction(const tString& asName,int alId, cInput *apInput);
		~cAction();

		void AddKey(eKey aKey);
		void AddMouseButton(eMouseButton aButton);
		void AddGamepadButton(int alPadIndex, eGamepadButton aButton);
		void AddGamepadAxis(int alPadIndex, eGamepadAxis aAxis, eGamepadAxisRange aRange, float afMinThreshold, float afMaxThreshold=1.0f); 
		void AddGamepadHat(int alPadIndex, eGamepadHat aHat, eGamepadHatState aHatState);

		void AddSubAction(const OhplSubSpec &aSpec);

		void ClearSubActions();

		/**
		* returns true if the action is triggered now
		*/
		bool IsTriggerd();

		/**
		* returns true if the action just was triggered, else false
		*/
		bool WasTriggerd();

		/**
		* returns true if the action just became triggered, else false
		*/
		bool BecameTriggerd();

		/**
		* returns true if the action just was double triggered (double clicked), else false
		*/
		bool DoubleTriggerd(float afLimit);

		/**
		 * Resets the action to the current state, so no actions are triggered.
		 */
		void ResetToCurrentState();

		/**
		*Update the Action, called by cInput
		*/
		void Update(float afTimeStep);

		iSubAction *GetSubAction(int alIdx){ return mvSubActions[alIdx];}
		int GetSubActionNum(){ return (int)mvSubActions.size();}

		
		const tString& GetName(){ return msName;}
		int GetId(){ return mlId; }
		uint64_t GetRustHandle(){ return mlHandle; }
	private:
		tString msName;
		int mlId;
		cInput *mpInput;

		OhplInputReg *mpReg;
		uint64_t mlHandle;

		tSubActionVec mvSubActions;
		
	};

	//---------------------------------------------------------


inline cAction::cAction(const tString& asName,int alId, cInput *apInput) { msName = asName; mlId = alId; mpInput = apInput; mpReg = mpInput->GetRustRegistry(); mlHandle = ohpl_input_reg_create(mpReg, (const unsigned char*)asName.data(), asName.size(), alId, this); }
inline cAction::~cAction() { STLDeleteAll(mvSubActions); }
inline void cAction::AddKey(eKey aKey) { AddSubAction({eOhplSub_Key, 0, (int)aKey, 0, 0, 0}); }
inline void cAction::AddMouseButton(eMouseButton aButton) { AddSubAction({eOhplSub_Mouse, 0, (int)aButton, 0, 0, 0}); }
inline void cAction::AddGamepadButton(int alPadIndex, eGamepadButton aButton) { AddSubAction({eOhplSub_PadButton, alPadIndex, (int)aButton, 0, 0, 0}); }
inline void cAction::AddGamepadAxis(int alPadIndex, eGamepadAxis aAxis, eGamepadAxisRange aRange, float afMinThreshold, float afMaxThreshold) { AddSubAction({eOhplSub_PadAxis, alPadIndex, (int)aAxis, (int)aRange, afMinThreshold, afMaxThreshold}); }
inline void cAction::AddGamepadHat(int alPadIndex, eGamepadHat aHat, eGamepadHatState aHatState) { AddSubAction({eOhplSub_PadHat, alPadIndex, (int)aHat, (int)aHatState, 0, 0}); }
inline void cAction::AddSubAction(const OhplSubSpec &aSpec) { mvSubActions.push_back(hplNew( iSubAction, (aSpec) )); ohpl_input_reg_add_sub(mpReg, mlHandle, &aSpec); }
inline void cAction::ClearSubActions() { ohpl_input_reg_clear_subs(mpReg, mlHandle); STLDeleteAll(mvSubActions); }
inline bool cAction::IsTriggerd() { return ohpl_input_reg_is_down(mpReg, mlHandle); }
inline bool cAction::WasTriggerd() { return ohpl_input_reg_was_triggered(mpReg, mlHandle); }
inline bool cAction::BecameTriggerd() { return ohpl_input_reg_became_triggered(mpReg, mlHandle); }
inline bool cAction::DoubleTriggerd(float afLimit) { return ohpl_input_reg_double_triggered(mpReg, mlHandle, afLimit); }
inline void cAction::ResetToCurrentState() { ohpl_input_reg_reset(mpReg, mlHandle); }
inline void cAction::Update(float afTimeStep) { ohpl_input_reg_update_action(mpReg, mlHandle, afTimeStep); }

};
#endif // HPL_ACTION_H
