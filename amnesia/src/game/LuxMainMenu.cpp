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

#include "LuxMainMenu.h"

#include "RustMenuHost.h"

cLuxMainMenu::cLuxMainMenu() : iLuxUpdateable("LuxDebugHandler")
{
	OhplA9RegisterHost();
	OhplA10aHostInit();
	mlMe = ohpl_a9b_new();

	if(getenv("OHPL_A10A_DUMP"))
	{
		const char *vNames[2] = { "options", "keyconfig" };
		const int vWindows[2] = { eLuxMainMenuWindow_Options, eLuxMainMenuWindow_KeyConfig };
		for(int i = 0; i < 2; ++i)
		{
			OhplA10aDumpWindow(vNames[i], (iWidget*)ohpl_a9b_dump_window(mlMe, vWindows[i], 1));
			ohpl_a9b_dump_window(mlMe, vWindows[i], 0);
		}
	}
}

cLuxMainMenu::~cLuxMainMenu()
{
	ohpl_a9b_free(mlMe);
}

void cLuxMainMenu::OnStart()
{
	if(const char *sDump = getenv("OHPL_A9_MENUDUMP"))
	{
		LoadFonts();
		ohpl_a9b_create_gui(mlMe);
		DumpA9(sDump);
	}
}
