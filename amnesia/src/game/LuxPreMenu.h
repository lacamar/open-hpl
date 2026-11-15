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

#ifndef LUX_PRE_MENU_H
#define LUX_PRE_MENU_H

#include "LuxA10bHost.h"

class cLuxPreMenu : public iLuxUpdateable
{
public:
	cLuxPreMenu() : iLuxUpdateable("LuxPreMenu"), mR(this, eOhplA10bKind_PreMenu) { mR.mH = ohpl_a10b_pm_new(&mR.mHost, &mR.mH); }
	~cLuxPreMenu() { ohpl_a10b_pm_delete(mR.mH); }

	void Update(float afTimeStep) { ohpl_a10b_pm_update(&mR.mHost, mR.mH, afTimeStep); }
	void OnEnterContainer(const tString& asOldContainer) { ohpl_a10b_pm_enter(&mR.mHost, mR.mH); }
	void OnLeaveContainer(const tString& asNewContainer) { ohpl_a10b_pm_leave(&mR.mHost, mR.mH); }
	void OnDraw(float afFrameTime) { ohpl_a10b_pm_draw(&mR.mHost, mR.mH); }
	void ButtonPressed() { ohpl_a10b_pm_button_pressed(&mR.mHost, mR.mH); }
	void AppLostInputFocus() { ohpl_a10b_pm_focus(&mR.mHost, false); }
	void AppGotInputFocus() { ohpl_a10b_pm_focus(&mR.mHost, true); }

	cGuiSet* GetSet() { return (cGuiSet*)ohpl_a10b_pm_set(mR.mH); }
	bool IsContinueButtonVisible() { return ohpl_a10b_pm_continue_visible(&mR.mHost, mR.mH); }

private:
	cOhplA10bShell mR;
};

#endif
