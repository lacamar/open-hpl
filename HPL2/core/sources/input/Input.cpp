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

#include "input/Input.h"
#include "system/LowLevelSystem.h"
#include "input/Mouse.h"
#include "input/Keyboard.h"
#include "input/Gamepad.h"
#include "input/LowLevelInput.h"
#include "input/Action.h"


#include "engine/Engine.h"

namespace hpl 
{
	cAction::cAction(const tString& asName,int alId, cInput *apInput) {
		msName = asName; mlId = alId;
		mpInput = apInput;
		mpReg = mpInput->GetRustRegistry();
		mlHandle = ohpl_input_reg_create(mpReg, (const unsigned char*)asName.data(), asName.size(), alId, this);
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cInput::cInput(iLowLevelInput *apLowLevelInput) : iUpdateable("HPL_Input")
	{
		mpLowLevelInput = apLowLevelInput;

		mpKeyboard = mpLowLevelInput->CreateKeyboard();
		mpMouse = mpLowLevelInput->CreateMouse();

		mlstInputDevices.push_back(mpMouse);
		mlstInputDevices.push_back(mpKeyboard);

		mpRegistry = ohpl_input_reg_new();
		ohpl_input_reg_set_devices(mpRegistry, mpKeyboard->RustHandle(), mpMouse->RustHandle());

		RefreshGamepads();
	}

	//-----------------------------------------------------------------------
	
	cInput::~cInput()
	{
		Log("Exiting Input Module\n");
		Log("--------------------------------------------------------\n");

		while(void *pAction = ohpl_input_reg_pop_registered(mpRegistry)) hplDelete(static_cast<cAction*>(pAction));
		ohpl_input_reg_set_devices(mpRegistry, NULL, NULL);
		ohpl_input_reg_set_pads(mpRegistry, NULL, 0);

		if(mpKeyboard)hplDelete(mpKeyboard);
		if(mpMouse)hplDelete(mpMouse);
		ohpl_input_reg_free(mpRegistry);

		Log("--------------------------------------------------------\n\n");
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS				
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cInput::Update(float afTimeStep)
	{
		mpLowLevelInput->BeginInputUpdate();

		
		for(tInputDeviceListIt it = mlstInputDevices.begin(); it!= mlstInputDevices.end();++it)
		{
			(*it)->Update();
		}

		mpLowLevelInput->EndInputUpdate();

		ohpl_input_reg_update(mpRegistry, afTimeStep);
	}
	
	//-----------------------------------------------------------------------
	
	cAction * cInput::CreateAction(const tString& asName, int alId)
	{
		return hplNew( cAction, (asName, alId,this) );
	}
	
	//-----------------------------------------------------------------------
	
	bool cInput::IsTriggerd(const tString& asName)
	{
		cAction *pAction = GetAction(asName);
		if(pAction==NULL){return false;}//Log("doesn't exist!!!");return false;}

		return pAction->IsTriggerd();
	}

	bool cInput::IsTriggerd(int alId)
	{
		cAction *pAction = GetAction(alId);
		if(pAction==NULL){return false;}

		return pAction->IsTriggerd();
	}

	//-----------------------------------------------------------------------

	bool cInput::WasTriggerd(const tString& asName)
	{
		cAction *pAction = GetAction(asName);
		if(pAction==NULL)return false;

		return pAction->WasTriggerd();
	}
	
	bool cInput::WasTriggerd(int alId)
	{
		cAction *pAction = GetAction(alId);
		if(pAction==NULL)return false;

		return pAction->WasTriggerd();
	}

	//-----------------------------------------------------------------------
	
	bool cInput::BecameTriggerd(const tString& asName)
	{
		cAction *pAction = GetAction(asName);
		if(pAction==NULL)return false;

		return pAction->BecameTriggerd();
	}

	bool cInput::BecameTriggerd(int alId)
	{
		cAction *pAction = GetAction(alId);
		if(pAction==NULL)return false;

		return pAction->BecameTriggerd();
	}

	//-----------------------------------------------------------------------

	bool cInput::DoubleTriggerd(const tString& asName, float afLimit)
	{
		cAction *pAction = GetAction(asName);
		if(pAction==NULL)return false;

		return pAction->DoubleTriggerd(afLimit);
	}
	bool cInput::DoubleTriggerd(int alId, float afLimit)
	{
		cAction *pAction = GetAction(alId);
		if(pAction==NULL)return false;

		return pAction->DoubleTriggerd(afLimit);
	}

	//-----------------------------------------------------------------------

	iKeyboard* cInput::GetKeyboard()
	{
		return mpKeyboard;
	}

	//-----------------------------------------------------------------------

	iMouse* cInput::GetMouse()
	{
		return mpMouse;
	}

	//-----------------------------------------------------------------------

	void cInput::SyncRustDevices()
	{
		std::vector<const OhplGamepad*> vPads;
		for(tGamepadListIt it = mlstGamepads.begin(); it!=mlstGamepads.end(); ++it) vPads.push_back((*it)->RustHandle());
		ohpl_input_reg_set_pads(mpRegistry, vPads.data(), vPads.size());
	}

	void cInput::RefreshGamepads()
	{
		ohpl_input_reg_set_pads(mpRegistry, NULL, 0);
		tGamepadListIt it = mlstGamepads.begin();
		for(; it!=mlstGamepads.end(); ++it)
			mlstInputDevices.remove(*it);
		STLDeleteAll(mlstGamepads);

		for(int i=0; i<mpLowLevelInput->GetPluggedGamepadNum(); ++i)
		{
			iGamepad* pGamepad = mpLowLevelInput->CreateGamepad(i);

			if(pGamepad)
			{
				if(pGamepad->GetGamepadName() == "")
				{
					hplDelete(pGamepad);
				}
				else
				{
					mlstGamepads.push_back(pGamepad);
					mlstInputDevices.push_back(pGamepad);
				}
			}
		}
		SyncRustDevices();
	}

	int cInput::GetGamepadNum()
	{
		return (int)mlstGamepads.size();
	}

	iGamepad* cInput::GetGamepad(int alIdx)
	{
		tGamepadListIt it = mlstGamepads.begin();
		for(size_t i=0; it!=mlstGamepads.end(); ++i, ++it)
		{
			if(i==alIdx)
				return *it;
		}

		return NULL;
	}
	
	//-----------------------------------------------------------------------

	cAction* cInput::GetAction(const tString& asName)
	{
		uint64_t lHandle = ohpl_input_reg_find_name(mpRegistry, (const unsigned char*)asName.data(), asName.size());
		return static_cast<cAction*>(ohpl_input_reg_user(mpRegistry, lHandle));
	}

	cAction* cInput::GetAction(int alId)
	{
		return static_cast<cAction*>(ohpl_input_reg_user(mpRegistry, ohpl_input_reg_find_id(mpRegistry, alId)));
	}

	//-----------------------------------------------------------------------

	void cInput::DestroyAction(cAction *apAction)
	{
		if(apAction)
		{
			ohpl_input_reg_destroy(mpRegistry, apAction->GetRustHandle());
			hplDelete(apAction);
		}
	}

	//-----------------------------------------------------------------------

	bool cInput::CheckForInput()
	{
		return ohpl_input_reg_first_down(mpRegistry, NULL, NULL);
	}

	//-----------------------------------------------------------------------

	void cInput::ResetActionsToCurrentState()
	{
		ohpl_input_reg_reset_all(mpRegistry);
	}

	//-----------------------------------------------------------------------
	
	iSubAction* cInput::InputToSubAction()
	{
		uint32_t lKind;
		int32_t lId;
		if(!ohpl_input_reg_first_down(mpRegistry, &lKind, &lId)) return NULL;
		OhplSubSpec spec = {lKind == eOhplSub_Key ? (uint32_t)eOhplSub_Key : (uint32_t)eOhplSub_Mouse, 0, lId, 0, 0, 0};
		return hplNew( iSubAction, (spec) );
	}

	//-----------------------------------------------------------------------

	void cInput::AppDeviceWasPlugged()
	{
		RefreshGamepads();
	}

	void cInput::AppDeviceWasRemoved()
	{
		RefreshGamepads();
	}

	//-----------------------------------------------------------------------


    bool cInput::isQuitMessagePosted()
    {
        return mpLowLevelInput->isQuitMessagePosted();
    }

    void cInput::resetQuitMessagePosted()
    {
        mpLowLevelInput->resetQuitMessagePosted();
    }

    //-----------------------------------------------------------------------

}
