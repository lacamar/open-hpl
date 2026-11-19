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

#ifndef HPL_WIDGET_MENU_H
#define HPL_WIDGET_MENU_H

#include "gui/Widget.h"
#include "gui/RustGuiGw.h"

namespace hpl {

	class cGuiSet;
	class cGuiSkin;
	class cWidgetMenuItem;

	// Rust kinds (rust/src/gui/gw_menu.rs); the context and main menus share this base
	class iWidgetMenu : public iWidget
	{
	public:
		iWidgetMenu(eWidgetType aeMenuType,cGuiSet *apSet, cGuiSkin *apSkin) : iWidget(aeMenuType, apSet, apSkin) {}
		virtual ~iWidgetMenu() {}

		cWidgetMenuItem* AddMenuItem(const tWString &asText){ return (cWidgetMenuItem*)ohpl_gw_menu_add_item((uintptr_t)this, (const uint32_t*)asText.data(), asText.size()); }
		void AddSeparator(){ ohpl_gw_menu_add_separator((uintptr_t)this); }
		void ClearMenuItems(){ ohpl_gw_menu_clear((uintptr_t)this); }
		void SetHighlightedItem(cWidgetMenuItem* apItem){ ohpl_gw_menu_set_highlight((uintptr_t)this, (uintptr_t)apItem); }
	};

};
#endif // HPL_WIDGET_MENU_H
