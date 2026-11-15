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

#ifndef LUX_DEMO_END_H
#define LUX_DEMO_END_H

#include "LuxA10bHost.h"

class cLuxDemoEnd : public iLuxUpdateable
{
public:
	cLuxDemoEnd() : iLuxUpdateable("LuxDemoEnd"), mR(this, eOhplA10bKind_DemoEnd) { mR.mH = ohpl_a10b_de_new(&mR.mHost); }
	~cLuxDemoEnd() { ohpl_a10b_de_delete(mR.mH); }

	void LoadFonts() { ohpl_a10b_de_load_fonts(&mR.mHost, mR.mH); }
	void Reset() { ohpl_a10b_de_reset(&mR.mHost, mR.mH); }
	void Update(float afTimeStep) { ohpl_a10b_de_update(&mR.mHost, mR.mH, afTimeStep); }
	void OnEnterContainer(const tString& asOldContainer) { ohpl_a10b_de_enter(&mR.mHost, mR.mH); }
	void OnLeaveContainer(const tString& asNewContainer) { ohpl_a10b_de_leave(&mR.mHost, mR.mH); }
	void OnDraw(float afFrameTime) { ohpl_a10b_de_draw(&mR.mHost, mR.mH); }
	void Exit(bool abShowWebsite) { ohpl_a10b_de_exit(&mR.mHost, mR.mH, abShowWebsite); }
	void AppLostInputFocus() { ohpl_a10b_de_focus(&mR.mHost, false); }
	void AppGotInputFocus() { ohpl_a10b_de_focus(&mR.mHost, true); }
	bool ShowOnAllExit() { return ohpl_a10b_de_show_on_all_exit(mR.mH); }

private:
	cOhplA10bShell mR;
};

#endif
