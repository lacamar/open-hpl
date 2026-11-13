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

#include "gui/WidgetComboBox.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"

#include "gui/WidgetTextBox.h"
#include "gui/WidgetButton.h"
#include "gui/WidgetSlider.h"

#include <vector>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cWidgetComboBox::cWidgetComboBox(cGuiSet *apSet, cGuiSkin *apSkin) : iWidget(eWidgetType_ComboBox,apSet, apSkin),
		mCombo(mH.combo), mfButtonWidth(mH.buttonW), mfSliderWidth(mH.sliderW)
	{
		ohpl_h20_cb_init(&gH20Host, &mH, (uintptr_t)(iWidget*)this, (uintptr_t)(iWidgetItemContainer*)this, &mB);

		mfButtonWidth = mpSkin->GetAttribute(eGuiSkinAttribute_ComboBoxButtonWidth).x;
		mfSliderWidth = mpSkin->GetAttribute(eGuiSkinAttribute_ComboBoxSliderWidth).x;

		mCombo = ohpl_w2_combo_init();
	}

	//-----------------------------------------------------------------------

	cWidgetComboBox::~cWidgetComboBox()
	{
		if(mpSet->IsDestroyingSet()==false)
			ohpl_h20_cb_destroy_children(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetComboBox::SetSelectedItem(int alX, bool abMoveList, bool abGenCallback)
	{
		ohpl_h20_cb_set_selected_item(&gH20Host, &mH, alX, abMoveList, abGenCallback);
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::SetCanEdit(bool abX)
	{
		ohpl_h20_cb_set_can_edit(&gH20Host, &mH, abX);
	}

	bool cWidgetComboBox::GetCanEdit()
	{
		return ohpl_h20_cb_can_edit(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::SetMaxShownItems(int alX)
	{
		ohpl_h20_cb_set_max_shown_items(&gH20Host, &mH, alX);
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::SetDefaultFontSize(const cVector2f& avSize)
	{
		iWidget::SetDefaultFontSize(avSize);
		ohpl_h20_cb_set_default_font_size(&gH20Host, &mH, avSize.x, avSize.y);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetComboBox::UpdateProperties()
	{
		ohpl_h20_cb_update_properties(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::OnInit()
	{
		ohpl_h20_cb_on_init(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::OnChangeSize()
	{
		ohpl_h20_cb_on_change_size(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::OnChangeText()
	{
		ohpl_h20_cb_on_change_text(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::OnLoadGraphics()
	{
		ohpl_h20_cb_on_load_graphics(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		ohpl_h20_cb_on_draw(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::OnMouseMove(const cGuiMessageData& aData)
	{
		return ohpl_h20_cb_on_mouse_move(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::OnMouseDown(const cGuiMessageData& aData)
	{
		return ohpl_h20_cb_on_mouse_down(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::OnMouseUp(const cGuiMessageData& aData)
	{
		return ohpl_h20_cb_on_mouse_up(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::OnMouseEnter(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::OnMouseLeave(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::OnUIArrowPress(const cGuiMessageData& aData)
	{
		return ohpl_h20_cb_on_ui_arrow_press(&gH20Host, &mH, &aData);
	}

	bool cWidgetComboBox::OnUIButtonPress(const cGuiMessageData& aData)
	{
		return ohpl_h20_cb_on_ui_button_press(&gH20Host, &mH, &aData);
	}

	bool cWidgetComboBox::OnUIButtonRelease(const cGuiMessageData& aData)
	{
		return ohpl_h20_cb_on_ui_button_release(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::OnLostFocus(const cGuiMessageData& aData)
	{
		return ohpl_h20_cb_on_lost_focus(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::PointIsInside(const cVector2f& avPoint, bool abOnlyClipped)
	{
		return ohpl_h20_cb_point_is_inside(&gH20Host, &mH, avPoint.x, avPoint.y, abOnlyClipped);
	}

	//-----------------------------------------------------------------------

}
