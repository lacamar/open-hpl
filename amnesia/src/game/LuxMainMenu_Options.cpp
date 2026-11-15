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

#include "LuxMainMenu_Options.h"

#include "RustMenuHost.h"

cLuxMainMenu_Options::cLuxMainMenu_Options(cGuiSet *apGuiSet, cGuiSkin *apGuiSkin) : iLuxMainMenuWindow(apGuiSet, apGuiSkin)
{
	OhplA10aHostInit();
	mlRust = ohpl_a10a_opt_new((size_t)apGuiSet);

	if(getenv("OHPL_A10A_DUMP"))
	{
		CreateGui();
		SetActive(true);
		OhplA10aDumpWindow("options", mpWindow);
		SetActive(false);
	}
}

cLuxMainMenu_Options::~cLuxMainMenu_Options()
{
	ohpl_a10a_opt_free(mlRust);
}

void cLuxMainMenu_Options::CreateGui()
{
	mpWindow = (cWidgetWindow*)ohpl_a10a_opt_create_gui(mlRust);
}

void cLuxMainMenu_Options::ExitPressed()
{
	ohpl_a10a_opt_exit_pressed(mlRust);
}

void cLuxMainMenu_Options::OnSetActive(bool abX)
{
	ohpl_a10a_opt_on_set_active(mlRust, abX);
}
