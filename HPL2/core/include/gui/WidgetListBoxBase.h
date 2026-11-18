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

#ifndef HPL_WIDGET_LIST_BOX_BASE_H
#define HPL_WIDGET_LIST_BOX_BASE_H

#include "gui/Widget.h"
#include "gui/WidgetBaseClasses.h"
#include "gui/RustGuiGw.h"
#include "gui/RustWidget2.h"
#include "gui/RustGuiH20.h"

namespace hpl {

	class cGuiSkinFont;

	class cWidgetSlider;

	enum eListBoxSelectType
	{
		eListBoxSelectType_Select,
		eListBoxSelectType_Toggle,
		eListBoxSelectType_FromLastCursor,
		eListBoxSelectType_FromAnchorPos,
	};

	//-----------------------------------------------------------------------

	// Rust kind (rust/src/gui/gw_h20.rs, logic in h20_list.rs)
	class iWidgetListBoxBase : public iWidget
	{
	public:
		iWidgetListBoxBase(cGuiSet *apSet, cGuiSkin *apSkin, int alKind=0)
			: iWidget(eWidgetType_ListBox, apSet, apSkin)
		{ ohpl_gw_set_kind_ctr((uintptr_t)this, kOhplGwList + alKind, 0, 0); }
		virtual ~iWidgetListBoxBase() {}

		void SetBackgroundZ(float afZ){ H().bgZ = afZ; }
		float GetBackgroundZ(){ return H().bgZ; }

		void SetSelectedItem(int alX,bool abMoveList=false,
							 bool abGenerateCallback=true,
							 eListBoxSelectType aType=eListBoxSelectType_Toggle,
							 bool abClearPrevious=true)
		{ ohpl_h20_lb_set_selected_item(&gH20Host, &H(), alX, abMoveList, abGenerateCallback, aType, abClearPrevious); }
		int GetSelectedItem(){ return ohpl_h20_lb_selected_item(&gH20Host, &H()); }

		int GetMultiSelectionNum(){ return ohpl_h20_lb_multi_selection_num(&gH20Host, &H()); }
		int GetMultiSelection(int alIdx){ return ohpl_h20_lb_multi_selection(&gH20Host, &H(), alIdx); }

		void SetAllowMultiSelection(bool abX) { H().list.allow_multi = abX; }
		bool GetAllowMultiSelection() { return H().list.allow_multi; }

		void ClearSelection(){ ohpl_h20_lb_clear_selection(&gH20Host, &H()); }

		cVector3f GetItemStartPos(){ cVector3f vPos; ohpl_h20_lb_item_start_pos(&gH20Host, &H(), &vPos); return vPos; }

		void SetCursorPos(int alPos, bool abMoveList, bool abClamp){ ohpl_h20_lb_set_cursor_pos(&gH20Host, &H(), alPos, abMoveList, abClamp); }
		int GetCursorPos() { return H().list.cursor_pos; }

		void SetAnchorPos(int alX) { H().list.anchor_pos = alX; }

		bool GetIsLocked() { return H().list.locked; }
		void SetIsLocked(bool abX) { H().list.locked = abX; }

		OHPL_ITEM_CONTAINER_API

	protected:
		OhplH20ListBase& H(){ return *(OhplH20ListBase*)ohpl_gw_state((uintptr_t)this); }
	};

};
#endif // HPL_WIDGET_LIST_BOX_BASE_H
