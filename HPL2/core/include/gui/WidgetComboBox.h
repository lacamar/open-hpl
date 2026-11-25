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

#ifndef HPL_WIDGET_COMBO_BOX_H
#define HPL_WIDGET_COMBO_BOX_H

#include "gui/Widget.h"
#include "gui/WidgetSlider.h"
#include "gui/WidgetBaseClasses.h"
#include "gui/RustGuiGw.h"
#include "gui/RustWidget2.h"
#include "gui/RustGuiH20.h"

namespace hpl {

	class cGuiSkinFont;

	class cWidgetTextBox;
	class cWidgetButton;
	class cWidgetSlider;

	// Rust kind (rust/src/gui/gw_h20.rs, logic in h20_combo.rs)
	class cWidgetComboBox : public iWidget
	{
	public:
		cWidgetComboBox(cGuiSet *apSet, cGuiSkin *apSkin) : iWidget(eWidgetType_ComboBox, apSet, apSkin)
		{ ohpl_gw_set_kind_ctr((uintptr_t)this, kOhplGwCombo, 0, 0); }
		virtual ~cWidgetComboBox() {}

		void SetSelectedItem(int alX,bool abMoveList=false, bool abGenCallback=true){ ohpl_h20_cb_set_selected_item(&H(), alX, abMoveList, abGenCallback); }
		int GetSelectedItem(){ return H().combo.selected;}

		void SetCanEdit(bool abX){ ohpl_h20_cb_set_can_edit(&H(), abX); }
		bool GetCanEdit(){ return ohpl_h20_cb_can_edit(&H()); }

		void SetMaxShownItems(int alX){ ohpl_h20_cb_set_max_shown_items(&H(), alX); }
		int GetMaxShownItems(){ return H().combo.max_items;}


		OHPL_ITEM_CONTAINER_API

	protected:
		OhplH20Combo& H(){ return *(OhplH20Combo*)ohpl_gw_state((uintptr_t)this); }
	};

};
#endif // HPL_WIDGET_COMBO_BOX_H
