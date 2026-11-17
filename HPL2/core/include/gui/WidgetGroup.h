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

#ifndef HPL_WIDGET_GROUP_H
#define HPL_WIDGET_GROUP_H

#include "gui/Widget.h"
#include "gui/RustGuiGw.h"

namespace hpl {

	class cWidgetLabel;
	class cGuiGfxElement;

	// Rust kind (rust/src/gui/gw_kinds.rs)
	class cWidgetGroup : public iWidget
	{
	public:
		cWidgetGroup(cGuiSet* apSet, cGuiSkin *apSkin) : iWidget(eWidgetType_Group, apSet, apSkin) { ohpl_gw_set_kind((uintptr_t)this, kOhplGwGroup, 0, 0); }

		void SetHeaderText(const tWString& asText){ SetText(asText); }
		tWString GetHeaderText(){ size_t n; const uint32_t* p = ohpl_gw_group_header((uintptr_t)this, &n); return tWString((const wchar_t*)p, n); }

		void SetDefaultFontSize(const cVector2f& avSize){ ohpl_gw_set_font_size((uintptr_t)this, avSize.x, avSize.y); }
	};

};
#endif // HPL_WIDGET_GROUP_H
