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

#ifndef HPL_WIDGET_CHECK_BOX_H
#define HPL_WIDGET_CHECK_BOX_H

#include "gui/Widget.h"
#include "gui/RustGuiGw.h"

namespace hpl {

	class cGuiSkinFont;

	// Rust kind (rust/src/gui/gw_kinds.rs)
	class cWidgetCheckBox : public iWidget
	{
	public:
		cWidgetCheckBox(cGuiSet *apSet, cGuiSkin *apSkin) : iWidget(eWidgetType_CheckBox, apSet, apSkin) { ohpl_gw_set_kind((uintptr_t)this, kOhplGwCheck, 0, 0); }

		void SetChecked(bool abX, bool abGenCallback=true){ ohpl_gw_check_set_checked((uintptr_t)this, abX, abGenCallback); }
		bool IsChecked(){return ohpl_gw_check_checked((uintptr_t)this);}

		void SetDefaultFontType(iFontData *apFont){ ohpl_gw_set_font_type((uintptr_t)this, apFont); }
		void SetDefaultFontColor(const cColor& aColor){ ohpl_gw_set_font_color((uintptr_t)this, &aColor); }
		void SetDefaultFontSize(const cVector2f& avSize){ ohpl_gw_set_font_size((uintptr_t)this, avSize.x, avSize.y); }
	};

};
#endif // HPL_WIDGET_CHECK_BOX_H
