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

#include "input/Action.h"
#include "system/LowLevelSystem.h"

#include "input/Input.h"
#include "input/ActionKeyboard.h"
#include "input/ActionMouseButton.h"
#include "input/ActionGamepadButton.h"
#include "input/ActionGamepadAxis.h"
#include "input/ActionGamepadHat.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// SUB ACTION
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iSubAction::Update(float afTimeStep)
	{
		UpdateLogic(afTimeStep);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cAction::cAction(const tString& asName,int alId, cInput *apInput)
	{
		msName = asName;
		mlId = alId;
		mpInput = apInput;

		mState = ohpl_input_action_new();
	}

	//-----------------------------------------------------------------------

	cAction::~cAction()
	{
		STLDeleteAll(mvSubActions);
	}
	
	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cAction::AddKey(eKey aKey)
	{
		AddSubAction( hplNew(cActionKeyboard, (mpInput,aKey) ));
	}
	
	void cAction::AddMouseButton(eMouseButton aButton)
	{
		AddSubAction( hplNew(cActionMouseButton, (mpInput,aButton) ));
	}

	void cAction::AddGamepadButton(int alPadIndex, eGamepadButton aButton)
	{
		AddSubAction( hplNew(cActionGamepadButton, (mpInput, alPadIndex, aButton)));
	}
	
	void cAction::AddGamepadAxis(int alPadIndex, eGamepadAxis aAxis, eGamepadAxisRange aRange, float afMinThreshold, float afMaxThreshold)
	{
		AddSubAction( hplNew(cActionGamepadAxis, (mpInput, alPadIndex, aAxis, aRange, afMinThreshold, afMaxThreshold)));
	}

	void cAction::AddGamepadHat(int alPadIndex, eGamepadHat aHat, eGamepadHatState aHatState)
	{
		AddSubAction( hplNew(cActionGamepadHat, (mpInput, alPadIndex, aHat, aHatState)));
	}

	//-----------------------------------------------------------------------

	void cAction::AddSubAction(iSubAction *apSubAction)
	{
		mvSubActions.push_back(apSubAction);
	}

	//-----------------------------------------------------------------------

	void cAction::ClearSubActions()
	{
		STLDeleteAll(mvSubActions);
	}

	//-----------------------------------------------------------------------

	bool cAction::IsTriggerd()
	{
		return ohpl_input_action_is_down(&mState);
	}

	//-----------------------------------------------------------------------

	bool cAction::WasTriggerd()
	{
		return ohpl_input_action_was_triggered(&mState);
	}

	//-----------------------------------------------------------------------
	
	bool cAction::BecameTriggerd()
	{
		return ohpl_input_action_became_triggered(&mState);
	}

	//-----------------------------------------------------------------------

	bool cAction::DoubleTriggerd(float afLimit)
	{
		return ohpl_input_action_double_triggered(&mState, afLimit);
	}

	//-----------------------------------------------------------------------

	void cAction::ResetToCurrentState()
	{
		ohpl_input_action_reset(&mState, AnySubActionTriggerd());
	}

	//-----------------------------------------------------------------------

	void cAction::Update(float afTimeStep)
	{
		ohpl_input_action_update(&mState, AnySubActionTriggerd(), afTimeStep);
	}

	//-----------------------------------------------------------------------

	bool cAction::AnySubActionTriggerd()
	{
		bool bAny = false;
		for(size_t i=0; i< mvSubActions.size(); ++i)
		{
			if(mvSubActions[i]->IsTriggerd()) bAny = true;
		}
		return bAny;
	}

	//-----------------------------------------------------------------------

}
