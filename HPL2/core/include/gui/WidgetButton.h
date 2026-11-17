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

#ifndef HPL_WIDGET_BUTTON_H
#define HPL_WIDGET_BUTTON_H

#include "gui/Widget.h"
#include "gui/RustGuiGw.h"

namespace hpl {

	class cGuiSkinFont;

	// Rust kind (rust/src/gui/gw_kinds.rs)
	class cWidgetButton : public iWidget
	{
	public:
		cWidgetButton(cGuiSet *apSet, cGuiSkin *apSkin) : iWidget(eWidgetType_Button, apSet, apSkin) { ohpl_gw_set_kind((uintptr_t)this, kOhplGwButton, 0, 0); }

		void SetImage(cGuiGfxElement *apImage, bool abDestroyImage=true){ ohpl_gw_button_set_image((uintptr_t)this, apImage, abDestroyImage); }
		cGuiGfxElement *GetImage(){return ohpl_gw_button_image((uintptr_t)this);}

		void SetRepeatActive(bool abX) { ohpl_gw_button_set_repeat_active((uintptr_t)this, abX); }
		bool GetRepeatActive() { return ohpl_gw_button_repeat_active((uintptr_t)this); }

		void SetRepeatFreq(float afFreq){ ohpl_gw_button_set_repeat_freq((uintptr_t)this, afFreq); }

		void SetToggleable(bool abX) { ohpl_gw_button_set_toggleable((uintptr_t)this, abX); }
		bool IsToggleable() { return ohpl_gw_button_toggleable((uintptr_t)this); }

		void SetPressed(bool abX, bool abGenCallback=true){ ohpl_gw_button_set_pressed((uintptr_t)this, abX, abGenCallback); }
		bool IsPressed() { return ohpl_gw_button_pressed((uintptr_t)this); }
	};

};
#endif // HPL_WIDGET_BUTTON_H
