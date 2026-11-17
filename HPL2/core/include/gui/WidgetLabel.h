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

#ifndef HPL_WIDGET_LABEL_H
#define HPL_WIDGET_LABEL_H

#include "gui/Widget.h"
#include "gui/RustGuiGw.h"

namespace hpl {

	class cGuiSkinFont;

	// Rust kind (rust/src/gui/gw_kinds.rs)
	class cWidgetLabel : public iWidget
	{
	public:
		cWidgetLabel(cGuiSet *apSet, cGuiSkin *apSkin) : iWidget(eWidgetType_Label, apSet, apSkin) { ohpl_gw_set_kind((uintptr_t)this, kOhplGwLabel, 0, 0); }

		void SetTextAlign(eFontAlign aType){ ohpl_gw_label_set_align((uintptr_t)this, aType); }
		eFontAlign GetTextAlign(){ return (eFontAlign)ohpl_gw_label_align((uintptr_t)this); }

		bool GetWordWrap(){ return ohpl_gw_label_word_wrap((uintptr_t)this); }
		void SetWordWrap(bool abX){ ohpl_gw_label_set_word_wrap((uintptr_t)this, abX); }

		void SetMaxTextLength(int alLength){ ohpl_gw_label_set_max_length((uintptr_t)this, alLength); }
		int GetMaxTextLength(){ return ohpl_gw_label_max_length((uintptr_t)this); }

		void SetAutogenerateSize(bool abX){ ohpl_gw_label_set_autogen((uintptr_t)this, abX); }
		bool GetAutogenerateSize(){ return ohpl_gw_label_autogen((uintptr_t)this); }

		void SetDefaultFontSize(const cVector2f& avSize){ ohpl_gw_set_font_size((uintptr_t)this, avSize.x, avSize.y); }

		void SetDrawBackGround(bool abX) { ohpl_gw_label_set_draw_bg((uintptr_t)this, abX); }
		bool GetDrawBackGround() { return ohpl_gw_label_draw_bg((uintptr_t)this); }

		void SetBackGroundColor(const cColor &aColor){ ohpl_gw_label_set_bg_color((uintptr_t)this, &aColor); }
		const cColor& GetBackGroundColor(){ return *ohpl_gw_label_bg_color((uintptr_t)this); }

		void SetScrollWaitTime(float afX) { ohpl_gw_label_set_scroll_wait((uintptr_t)this, afX); }
		float GetScrollWaitTime() { return ohpl_gw_label_scroll_wait((uintptr_t)this); }
		void SetScrollOffset(float afX) { ohpl_gw_label_set_scroll_offset((uintptr_t)this, afX); }
		void SetScrollSpeedMul(float afX) { ohpl_gw_label_set_scroll_speed((uintptr_t)this, afX); }
		float GetScrollSpeedMul() { return ohpl_gw_label_scroll_speed((uintptr_t)this); }
	};

};
#endif // HPL_WIDGET_LABEL_H
