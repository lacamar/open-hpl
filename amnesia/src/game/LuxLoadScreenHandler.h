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

#ifndef LUX_LOAD_SCREEN_HANDLER_H
#define LUX_LOAD_SCREEN_HANDLER_H

#include "LuxA10bHost.h"

class cLuxLoadScreenHandler : public iLuxUpdateable
{
public:
	cLuxLoadScreenHandler() : iLuxUpdateable("LuxLoadScreenHandler"), mR(this, eOhplA10bKind_Other) { mR.mH = ohpl_a10b_ls_new(&mR.mHost); }
	~cLuxLoadScreenHandler() { ohpl_a10b_ls_delete(mR.mH); }

	void LoadFonts() { ohpl_a10b_ls_load_fonts(&mR.mHost, mR.mH); }
	void Reset() { ohpl_a10b_ls_reset(&mR.mHost, mR.mH); }
	void Update(float afTimeStep) { ohpl_a10b_ls_update(&mR.mHost, mR.mH, afTimeStep); }
	void OnEnterContainer(const tString& asOldContainer) { ohpl_a10b_ls_enter(&mR.mHost, mR.mH); }
	void OnLeaveContainer(const tString& asNewContainer) { ohpl_a10b_ls_leave(&mR.mHost, mR.mH); }
	void OnDraw(float afFrameTime) { ohpl_a10b_ls_draw(&mR.mHost, mR.mH); }

	/**
	* Called when game loading is done. Must be called after DrawGameScreen has been called!
	*/
	void GameScreenLoadDone(const tString& asEndSound, float afLoadTime)
	{
		ohpl_a10b_ls_load_done(&mR.mHost, mR.mH, asEndSound.data(), asEndSound.size(), afLoadTime);
	}

	void DrawGameScreen() { ohpl_a10b_ls_draw_game_screen(&mR.mHost, mR.mH); }
	void DrawMenuScreen() { ohpl_a10b_ls_draw_menu_screen(&mR.mHost, mR.mH); }
	void DrawBlankScreen() {}
	void ExitPressed() { ohpl_a10b_ls_exit_pressed(&mR.mHost, mR.mH); }

	/**
	* If alRandomNum > 1, then it will randomize between 1 and alRandom for each LoadScreen giving entry the suffix XX (eg 01). If <=1 then no suffix is added
	*/
	void SetupLoadText(const tString& asCat, const tString& asEntry, int alRandomNum, const tString& asImage)
	{
		ohpl_a10b_ls_setup(&mR.mHost, mR.mH, asCat.data(), asCat.size(), asEntry.data(), asEntry.size(), alRandomNum,
							asImage.data(), asImage.size());
	}

	tString GetLoadTextCat() { return Text(0); }
	tString GetLoadTextEntry() { return Text(1); }
	tString GetLoadTextImage() { return Text(2); }
	int GetLoadTextEntryRandomNum() { return ohpl_a10b_ls_rand_num(mR.mH); }
	int GetPrevTextNumBitFlags() { return ohpl_a10b_ls_prev_bits(mR.mH); }
	void SetPrevTextNumBitFlags(int alBits) { ohpl_a10b_ls_set_prev_bits(mR.mH, alBits); }

private:
	static void Sink(void *apCtx, const unsigned char *apData, size_t alLen) { ((tString*)apCtx)->assign((const char*)apData, alLen); }

	tString Text(int alWhich)
	{
		tString sText;
		ohpl_a10b_ls_text(mR.mH, alWhich, &Sink, &sText);
		return sText;
	}

	cOhplA10bShell mR;
};

#endif
