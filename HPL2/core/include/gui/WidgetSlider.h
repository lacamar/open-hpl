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

#ifndef HPL_WIDGET_SLIDER_H
#define HPL_WIDGET_SLIDER_H

#include "gui/Widget.h"
#include "gui/RustGuiGw.h"

namespace hpl {

	class cGuiSkinFont;
	class cWidgetButton;

	// Rust kind (rust/src/gui/gw_kinds.rs)
	class cWidgetSlider : public iWidget
	{
	public:
		cWidgetSlider(cGuiSet *apSet, cGuiSkin *apSkin, eWidgetSliderOrientation aOrientation)
			: iWidget(eWidgetType_Slider, apSet, apSkin) { ohpl_gw_set_kind((uintptr_t)this, kOhplGwSlider, aOrientation, 0); }

		int GetValue(){ return ohpl_gw_slider_value((uintptr_t)this); }
		void SetValue(int alValue, bool abGenCallback=true){ ohpl_gw_slider_set_value((uintptr_t)this, alValue, abGenCallback); }

		int GetMaxValue(){ return ohpl_gw_slider_max((uintptr_t)this); }
		void SetMaxValue(int alMax){ ohpl_gw_slider_set_max((uintptr_t)this, alMax); }

		int GetButtonValueAdd(){ return ohpl_gw_slider_button_add((uintptr_t)this); }
		void SetButtonValueAdd(int alAdd){ ohpl_gw_slider_set_button_add((uintptr_t)this, alAdd); }

		int GetBarClickValueAdd() { return ohpl_gw_slider_click_add((uintptr_t)this); }
		void SetBarClickValueAdd(int alAdd){ ohpl_gw_slider_set_click_add((uintptr_t)this, alAdd); }

		int GetBarValueSize(){ return ohpl_gw_slider_bar_size((uintptr_t)this); }
		void SetBarValueSize(int alSize){ ohpl_gw_slider_set_bar_size((uintptr_t)this, alSize); }

		float GetButtonSize() const{ return ohpl_gw_slider_button_size((uintptr_t)this); }
	};

};
#endif // HPL_WIDGET_SLIDER_H
