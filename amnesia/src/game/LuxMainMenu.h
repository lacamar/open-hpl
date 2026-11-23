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

#ifndef LUX_MAIN_MENU_H
#define LUX_MAIN_MENU_H

//----------------------------------------------

#include "LuxBase.h"
#include "RustLuxMenuA9b.h"

enum eLuxMainMenuWindow
{
	eLuxMainMenuWindow_Profiles,
	eLuxMainMenuWindow_Options,
	eLuxMainMenuWindow_KeyConfig,
	eLuxMainMenuWindow_LoadGame,
	eLuxMainMenuWindow_CustomStoryList,
	eLuxMainMenuWindow_CustomStory,

	//////////////
	// HARDMODE
	eLuxMainMenuWindow_StartGame,

	eLuxMainMenuWindow_LastEnum,
};

//----------------------------------------------

enum eLuxMainMenuExit
{
	eLuxMainMenuExit_QuitGame,
	eLuxMainMenuExit_ReturnToGame,
	eLuxMainMenuExit_StartGame,
	eLuxMainMenuExit_ContinueGame,
	eLuxMainMenuExit_QuitToMenu,
	eLuxMainMenuExit_QuitAndSave,
	eLuxMainMenuExit_LoadGame,
	
	eLuxMainMenuExit_LastEnum,
};

//----------------------------------------------

class cLuxMainMenu : public iLuxUpdateable
{
public:	
	cLuxMainMenu();
	~cLuxMainMenu();

	void LoadUserConfig(){}
	void SaveUserConfig(){}

	void LoadFonts() { ohpl_a9b_load_fonts(mlMe); }
	void OnClearFonts(){}
	
	void OnStart() { ohpl_a9b_dump_a9(mlMe); }
	void Update(float afTimeStep) { ohpl_a9b_update(mlMe, afTimeStep); }
	void Reset() { ohpl_a9b_reset(mlMe); }
	void OnQuit() { ohpl_a9b_on_quit(mlMe); }

	void OnEnterContainer(const tString& asOldContainer) { ohpl_a9b_enter(mlMe); }
	void OnLeaveContainer(const tString& asNewContainer) { ohpl_a9b_leave(mlMe); }

	void OnDraw(float afFrameTime) { ohpl_a9b_draw(mlMe); }
	void OnPostRender(float afFrameTime){}

	cGuiSet* GetSet() { return (cGuiSet*)ohpl_a9b_set(mlMe); }

	void SetWindowActive(eLuxMainMenuWindow aWindow) { ohpl_a9b_set_window(mlMe, aWindow); }

	void ExitPressed() { ohpl_a9b_exit_pressed(mlMe); }

	void AppLostInputFocus() { ohpl_a9b_focus(mlMe, 0); }
	void AppGotInputFocus() { ohpl_a9b_focus(mlMe, 1); }
#ifdef USE_GAMEPAD
	void AppDeviceWasPlugged() { ohpl_a9b_device_changed(mlMe); }
	void AppDeviceWasRemoved() { ohpl_a9b_device_changed(mlMe); }
#endif

private:
	uint64_t mlMe;
};

//----------------------------------------------


#endif // LUX_MAIN_MENU_H
