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
#include "input/ActionKeyboard.h"
#include "input/ActionMouseButton.h"

#if USE_XINPUT
#include "impl/GamepadXInput.h"
#endif

#include "engine/Engine.h"
#include "impl/RustBackends.h"

namespace hpl 
{
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

		mpRegistry = OhplBackendRustRequested() ? ohpl_input_reg_new() : NULL;
		if(mpRegistry) ohpl_input_reg_set_devices(mpRegistry, mpKeyboard->RustHandle(), mpMouse->RustHandle());

		RefreshGamepads();
	}

	//-----------------------------------------------------------------------
	
	cInput::~cInput()
	{
		Log("Exiting Input Module\n");
		Log("--------------------------------------------------------\n");

		STLMapDeleteAll(m_mapActions);
		if(mpRegistry)
		{
			while(void *pAction = ohpl_input_reg_pop_registered(mpRegistry)) hplDelete(static_cast<cAction*>(pAction));
			ohpl_input_reg_set_devices(mpRegistry, NULL, NULL);
			ohpl_input_reg_set_pads(mpRegistry, NULL, 0);
		}

		if(mpKeyboard)hplDelete(mpKeyboard);
		if(mpMouse)hplDelete(mpMouse);
		if(mpRegistry) ohpl_input_reg_free(mpRegistry);

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

		#if USE_XINPUT
		for(int i=0; i < 4; ++i)
		{
			bool bConnected = cGamepadXInput::IsConnected(i);

			if(bConnected == false && cGamepadXInput::GetWasConnected(i))
			{
				cEngine::SetDeviceWasRemoved();
			}
			else if(bConnected && cGamepadXInput::GetWasConnected(i) == false)
			{
				cEngine::SetDeviceWasPlugged();
			}
		}
		#endif
		
		for(tInputDeviceListIt it = mlstInputDevices.begin(); it!= mlstInputDevices.end();++it)
		{
			(*it)->Update();
		}

		mpLowLevelInput->EndInputUpdate();

		if(mpRegistry)
		{
			ohpl_input_reg_update(mpRegistry, afTimeStep);
			return;
		}

		for(tActionMapIt it = m_mapActions.begin(); it!= m_mapActions.end();++it)
		{
			it->second->Update(afTimeStep);
		}
	}
	
	//-----------------------------------------------------------------------
	
	cAction * cInput::CreateAction(const tString& asName, int alId)
	{
		cAction *pAction = hplNew( cAction, (asName, alId,this) );
		if(mpRegistry) return pAction;

		tActionMap::value_type val = tActionMap::value_type(asName,pAction);
		m_mapActions.insert(val);

		tActionIdMap::value_type val2 = tActionIdMap::value_type(alId,pAction);
		m_mapActionIds.insert(val2);
		
		return pAction;
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
		if(!mpRegistry) return;
		std::vector<const OhplGamepad*> vPads;
		for(tGamepadListIt it = mlstGamepads.begin(); it!=mlstGamepads.end(); ++it) vPads.push_back((*it)->RustHandle());
		ohpl_input_reg_set_pads(mpRegistry, vPads.data(), vPads.size());
	}

	void cInput::RefreshGamepads()
	{
		if(mpRegistry) ohpl_input_reg_set_pads(mpRegistry, NULL, 0);
		tGamepadListIt it = mlstGamepads.begin();
		for(; it!=mlstGamepads.end(); ++it)
			mlstInputDevices.remove(*it);
		STLDeleteAll(mlstGamepads);

#if USE_XINPUT
		// First try with XInput
		for(int i=0; i < 4; ++i)
		{
			bool bConnected = cGamepadXInput::IsConnected(i);

			if(bConnected)
			{
				iGamepad* pGamepad = hplNew( cGamepadXInput, (i) );

				mlstGamepads.push_back(pGamepad);
				mlstInputDevices.push_back(pGamepad);
			}

			cGamepadXInput::SetWasConnected(i, bConnected);
		}
#else
#ifndef USE_SDL2
        // SDL2 supports hot plugging so no need to re-init
		mpLowLevelInput->DropGamepadSupport();
		mpLowLevelInput->InitGamepadSupport();
#endif
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
#endif
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
		if(mpRegistry)
		{
			uint64_t lHandle = ohpl_input_reg_find_name(mpRegistry, (const unsigned char*)asName.data(), asName.size());
			return static_cast<cAction*>(ohpl_input_reg_user(mpRegistry, lHandle));
		}
		tActionMapIt it = m_mapActions.find(asName);
		if(it==m_mapActions.end())return NULL;

		return it->second;
	}

	cAction* cInput::GetAction(int alId)
	{
		if(mpRegistry) return static_cast<cAction*>(ohpl_input_reg_user(mpRegistry, ohpl_input_reg_find_id(mpRegistry, alId)));
		tActionIdMapIt it = m_mapActionIds.find(alId);
		if(it==m_mapActionIds.end()) return NULL;

		return it->second;
	}

	//-----------------------------------------------------------------------

	void cInput::DestroyAction(cAction *apAction)
	{
		if(mpRegistry)
		{
			if(apAction)
			{
				ohpl_input_reg_destroy(mpRegistry, apAction->GetRustHandle());
				hplDelete(apAction);
			}
			return;
		}
		for(tActionMapIt it = m_mapActions.begin(); it!= m_mapActions.end();++it)
		{
			if(it->second == apAction)
			{
				m_mapActions.erase(it);
				break;
			}
		}

		for(tActionIdMapIt it = m_mapActionIds.begin(); it!= m_mapActionIds.end();++it)
		{
			if(it->second == apAction)
			{
				m_mapActionIds.erase(it);
				break;
			}
		}

		if(apAction) hplDelete(apAction);
	}

	//-----------------------------------------------------------------------

	bool cInput::CheckForInput()
	{
		if(mpRegistry) return ohpl_input_reg_first_down(mpRegistry, NULL, NULL);
		//////////////////////
		//Keyboard
		for(int i=0; i< eKey_LastEnum; ++i)
		{
			if(mpKeyboard->KeyIsDown((eKey)i)) return true;
		}

		//////////////////////
		//Mouse
		for(int i=0; i< eMouseButton_LastEnum; ++i)
		{
			if(mpMouse->ButtonIsDown((eMouseButton)i)) return true;
		}
		
		return false;
	}

	//-----------------------------------------------------------------------

	void cInput::ResetActionsToCurrentState()
	{
		if(mpRegistry)
		{
			ohpl_input_reg_reset_all(mpRegistry);
			return;
		}
		for(tActionMapIt it = m_mapActions.begin(); it!= m_mapActions.end();++it)
		{
			cAction *pAction = it->second;
			pAction->ResetToCurrentState();			
		}

	}

	//-----------------------------------------------------------------------
	
	iSubAction* cInput::InputToSubAction()
	{
		iSubAction *pSubAction=NULL;

		if(mpRegistry)
		{
			uint32_t lKind;
			int32_t lId;
			if(!ohpl_input_reg_first_down(mpRegistry, &lKind, &lId)) return NULL;
			if(lKind == eOhplSub_Key) return hplNew( cActionKeyboard, (this,(eKey)lId));
			return hplNew( cActionMouseButton, (this,(eMouseButton)lId));
		}
		
		//////////////////////
		//Keyboard
		for(int i=0; i< eKey_LastEnum; ++i)
		{
			if(mpKeyboard->KeyIsDown((eKey)i))
			{
				pSubAction = hplNew( cActionKeyboard, (this,(eKey)i));
				break;
			}
		}

		//////////////////////
		//Mouse
		if(pSubAction==NULL)
		{
			for(int i=0; i< eMouseButton_LastEnum; ++i)
			{
				if(mpMouse->ButtonIsDown((eMouseButton)i))
				{
					pSubAction = hplNew( cActionMouseButton, (this,(eMouseButton)i));					
					break;
				}
			}
		}

		return pSubAction;
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
