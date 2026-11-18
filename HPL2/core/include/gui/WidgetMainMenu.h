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

#ifndef HPL_WIDGET_MAIN_MENU_H
#define HPL_WIDGET_MAIN_MENU_H

#include "gui/WidgetMenu.h"

namespace hpl {

	// Rust kind (rust/src/gui/gw_menu.rs)
	class cWidgetMainMenu : public iWidgetMenu
	{
	public:
		cWidgetMainMenu(cGuiSet *apSet, cGuiSkin *apSkin) : iWidgetMenu(eWidgetType_MainMenu, apSet, apSkin)
		{ ohpl_gw_set_kind((uintptr_t)this, kOhplGwMainMenu, 0, 0); }
	};

};
#endif // HPL_WIDGET_MAIN_MENU_H
