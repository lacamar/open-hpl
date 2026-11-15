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

#ifndef LUX_CREDITS_H
#define LUX_CREDITS_H

#include "LuxA10bHost.h"

class cLuxCredits : public iLuxUpdateable
{
public:
	cLuxCredits() : iLuxUpdateable("LuxCredits"), mR(this, eOhplA10bKind_Other) { mR.mH = ohpl_a10b_cr_new(&mR.mHost); }
	~cLuxCredits() { ohpl_a10b_cr_delete(mR.mH); }

	void LoadFonts() { ohpl_a10b_cr_load_fonts(&mR.mHost, mR.mH); }
	void Reset() { ohpl_a10b_cr_reset(&mR.mHost, mR.mH); }
	void Update(float afTimeStep) { ohpl_a10b_cr_update(&mR.mHost, mR.mH, afTimeStep); }
	void AddKeyPart(int alKey) { ohpl_a10b_cr_add_key(&mR.mHost, mR.mH, alKey); }
	void Setup(const tString& asMusic, bool abLoopMusic, const tString& asTextCat, const tString& asTextEntry, int alEndNum)
	{
		ohpl_a10b_cr_setup(&mR.mHost, mR.mH, asMusic.data(), asMusic.size(), abLoopMusic, asTextCat.data(), asTextCat.size(),
							asTextEntry.data(), asTextEntry.size(), alEndNum);
	}
	void OnEnterContainer(const tString& asOldContainer) { ohpl_a10b_cr_enter(&mR.mHost, mR.mH); }
	void OnLeaveContainer(const tString& asNewContainer) { ohpl_a10b_cr_leave(&mR.mHost, mR.mH); }
	void OnDraw(float afFrameTime) { ohpl_a10b_cr_draw(&mR.mHost, mR.mH); }
	void ExitPressed() { ohpl_a10b_cr_exit_pressed(&mR.mHost, mR.mH); }
	void AppLostInputFocus() { ohpl_a10b_cr_focus(&mR.mHost, false); }
	void AppGotInputFocus() { ohpl_a10b_cr_focus(&mR.mHost, true); }

private:
	cOhplA10bShell mR;
};

#endif
