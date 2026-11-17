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

#ifndef HPL_WIDGET_WINDOW_H
#define HPL_WIDGET_WINDOW_H

#include "gui/Widget.h"
#include "gui/RustGuiGw.h"
#include <list>
#include <vector>

namespace hpl {

	class cGuiSkinFont;
	class cWidgetButton;

	// Rust kind (rust/src/gui/gw_kinds.rs)
	class cWidgetWindow : public iWidget
	{
	public:
		cWidgetWindow(cGuiSet *apSet, cGuiSkin *apSkin, tWidgetWindowButtonFlag alFlags = 0)
			: iWidget(eWidgetType_Window, apSet, apSkin) { ohpl_gw_set_kind((uintptr_t)this, kOhplGwWindow, alFlags, 0); }

		void SetStatic(bool abX){ ohpl_gw_window_set_static((uintptr_t)this, abX); }
		bool GetStatic(){return ohpl_gw_window_static((uintptr_t)this);}

		void SetDrawLabel(bool abX){ ohpl_gw_window_set_draw_label((uintptr_t)this, abX); }

		void SetCloseButtonDisablesWindow(bool abX){ ohpl_gw_window_set_close_disables((uintptr_t)this, abX); }
		bool GetCloseButtonDisablesWindow(){ return ohpl_gw_window_close_disables((uintptr_t)this); }

		void SetEscapeKeyClosesWindow(bool abX) { ohpl_gw_window_set_escape_closes((uintptr_t)this, abX); }
		bool GetEscapeKeyClosesWindow() { return ohpl_gw_window_escape_closes((uintptr_t)this); }
	};

};
#endif // HPL_WIDGET_WINDOW_H
