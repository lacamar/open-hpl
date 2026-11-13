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

#include "gui/WidgetListBoxBase.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"

#include "gui/WidgetSlider.h"

#include <vector>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iWidgetListBoxBase::iWidgetListBoxBase(cGuiSet *apSet, cGuiSkin *apSkin, int alKind) : iWidget(eWidgetType_ListBox,apSet, apSkin),
		mfBackgroundZ(mH.bgZ), mfSliderWidth(mH.sliderW), mList(mH.list)
	{
		ohpl_h20_lb_init(&gH20Host, &mH, (uintptr_t)(iWidget*)this, (uintptr_t)(iWidgetItemContainer*)this, &mB, alKind);

		mbClipsGraphics = true;

		mfBackgroundZ = -0.5;

		mfSliderWidth = mpSkin->GetAttribute(eGuiSkinAttribute_ListBoxSliderWidth).x;

	}

	//-----------------------------------------------------------------------

	iWidgetListBoxBase::~iWidgetListBoxBase()
	{
		if(mpSet->IsDestroyingSet()==false)
		{
			ohpl_h20_lb_destroy_slider(&gH20Host, &mH);
		}
		ohpl_h20_lb_free(&mH);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::SetSelectedItem(int alX, bool abMoveList, bool abGenerateCallback, eListBoxSelectType aType,
												bool abClearPrevious)
	{
		ohpl_h20_lb_set_selected_item(&gH20Host, &mH, alX, abMoveList, abGenerateCallback, aType, abClearPrevious);
	}

	//-----------------------------------------------------------------------

	int iWidgetListBoxBase::GetSelectedItem()
	{
		return ohpl_h20_lb_selected_item(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	int iWidgetListBoxBase::GetMultiSelection(int alIdx)
	{
		return ohpl_h20_lb_multi_selection(&gH20Host, &mH, alIdx);
	}

	//-----------------------------------------------------------------------

	int iWidgetListBoxBase::GetMultiSelectionNum()
	{
		return ohpl_h20_lb_multi_selection_num(&gH20Host, &mH);
	}
	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::ClearSelection()
	{
		ohpl_h20_lb_clear_selection(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	cVector3f iWidgetListBoxBase::GetItemStartPos()
	{
		cVector3f vPos;
		ohpl_h20_lb_item_start_pos(&gH20Host, &mH, &vPos);
		return vPos;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::UpdateProperties()
	{
		ohpl_h20_lb_update_properties(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::OnChangeSize()
	{
		ohpl_h20_lb_on_change_size(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::OnInit()
	{
		ohpl_h20_lb_on_init(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::AddItemToSelection(cWidgetItem* apItem)
	{
		ohpl_h20_lb_add_item_to_selection(&gH20Host, &mH, (uintptr_t)apItem);
	}

	void iWidgetListBoxBase::RemoveItemFromSelection(cWidgetItem* apItem)
	{
		ohpl_h20_lb_remove_item_from_selection(&gH20Host, &mH, (uintptr_t)apItem);
	}

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::OnLoadGraphics()
	{
		ohpl_h20_lb_on_load_graphics(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		ohpl_h20_lb_on_draw(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::OnDrawAfterClip(float afTimeStep, cGuiClipRegion* apClipRegion)
	{
		ohpl_h20_lb_on_draw_after_clip(&gH20Host, &mH, (uintptr_t)apClipRegion);
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnMouseMove(const cGuiMessageData& aData)
	{
		return true;
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnMouseDown(const cGuiMessageData& aData)
	{
		return ohpl_h20_lb_on_mouse_down(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnMouseUp(const cGuiMessageData& aData)
	{
		return true;
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnMouseDoubleClick(const cGuiMessageData& aData)
	{
		return ohpl_h20_lb_on_mouse_double_click(&gH20Host, &mH, &aData);
	}


	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnMouseEnter(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnMouseLeave(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnKeyPress(const cGuiMessageData& aData)
	{
		return ohpl_h20_lb_on_key_press(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnUIArrowPress(const cGuiMessageData& aData)
	{
		return ohpl_h20_lb_on_ui_arrow_press(&gH20Host, &mH, &aData);
	}

	bool iWidgetListBoxBase::OnUIArrowRelease(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnUIButtonPress(const cGuiMessageData& aData)
	{
		return ohpl_h20_lb_on_ui_button_press(&gH20Host, &mH, &aData);
	}

	bool iWidgetListBoxBase::OnUIButtonRelease(const cGuiMessageData& aData)
	{
		return ohpl_h20_lb_on_ui_button_release(&gH20Host, &mH);
	}

	bool iWidgetListBoxBase::OnUIButtonDoublePress(const cGuiMessageData& aData)
	{
		return ohpl_h20_lb_on_ui_button_double_press(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::SetCursorPos(int alPos, bool abMoveList, bool abClamp)
	{
		ohpl_h20_lb_set_cursor_pos(&gH20Host, &mH, alPos, abMoveList, abClamp);
	}

	//-----------------------------------------------------------------------


}
