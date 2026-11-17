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


#ifndef HPL_GUI_POP_UP_H
#define HPL_GUI_POP_UP_H

#include "gui/RustGuiG2.h"
#include "gui/GuiTypes.h"

namespace hpl {

	class cGuiSet;

	// Rust-owned (rust/src/gui/g2_popup.rs): `this` is the Rust allocation, created by cGuiSet::CreatePopUp*.
	class iGuiPopUp
	{
	public:
		void AddOnDestroyCallback(void *apCallbackObject, tGuiCallbackFunc apCallback){ ohpl_g2_popup_add_destroy_cb(this, (uintptr_t)apCallbackObject, reinterpret_cast<uintptr_t>(apCallback)); }

		void SetKillOnEscapeKey(bool abX){ ohpl_g2_popup_set_kill_on_escape(this, abX); }
		bool GetKillOnEscapeKey(){ return ohpl_g2_popup_kill_on_escape(this); }

		void SetNextFocusWidget(iWidget *apNewFocused){ ohpl_g2_popup_set_next_focus(this, (uintptr_t)apNewFocused); }

		cGuiSet* GetGuiSet(){ return ohpl_g2_popup_set(this); }
	private:
		iGuiPopUp();
	};

};
#endif // HPL_GUI_POP_UP_H
