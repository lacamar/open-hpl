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

};
#endif // HPL_ACTION_H
