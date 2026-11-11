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

	cWidgetComboBox::cWidgetComboBox(cGuiSet *apSet, cGuiSkin *apSkin) : iWidget(eWidgetType_ComboBox,apSet, apSkin)
	{
		mfButtonWidth = mpSkin->GetAttribute(eGuiSkinAttribute_ComboBoxButtonWidth).x;
		mfSliderWidth = mpSkin->GetAttribute(eGuiSkinAttribute_ComboBoxSliderWidth).x;

		mpText = NULL;
		mpButton = NULL;
		mpSlider = NULL;

		mCombo = ohpl_w2_combo_init();
	}

	//-----------------------------------------------------------------------

	cWidgetComboBox::~cWidgetComboBox()
	{
		if(mpSet->IsDestroyingSet()==false)
		{
			mpSet->DestroyWidget(mpText);
			mpSet->DestroyWidget(mpButton);
			mpSet->DestroyWidget(mpSlider);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetComboBox::SetSelectedItem(int alX, bool abMoveList, bool abGenCallback)
	{
		OhplW2SliderSet slider;
		if(ohpl_w2_combo_select1(&mCombo, alX, abMoveList, GetItemNum(), &slider)==false) return;

		if(slider.set) mpSlider->SetValue(slider.value);

		slider = ohpl_w2_combo_select2(&mCombo, abMoveList);
		if(slider.set) mpSlider->SetValue(slider.value);

		if(mCombo.selected >=0 && mCombo.selected < GetItemNum())
			SetText(mvItems[mCombo.selected]->GetText());
		else
			SetText(_W(""));

		if(abGenCallback)
			ProcessMessage(eGuiMessage_SelectionChange, cGuiMessageData(mCombo.selected));
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::SetCanEdit(bool abX)
	{
		mpText->SetCanEdit(abX);
	}

	bool cWidgetComboBox::GetCanEdit()
	{
		return mpText->GetCanEdit();
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::SetMaxShownItems(int alX)
	{
		mCombo.max_items = alX;

		UpdateProperties();
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::SetDefaultFontSize(const cVector2f& avSize)
	{
		iWidget::SetDefaultFontSize(avSize);
		mpText->SetDefaultFontSize(avSize);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetComboBox::UpdateProperties()
	{
		if(ohpl_w2_combo_update(&mCombo, mvDefaultFontSize.y, (int)mvItems.size()))
			SetSelectedItem((int)mvItems.size()-1);

		OnChangeSize();
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::OpenMenu()
	{
		if(ohpl_w2_combo_can_open(&mCombo, (int)mvItems.size())==false) return;

		mpSet->PushAttentionWidget();
		mpSet->PushFocusedWidget();
		mpSet->PushDefaultFocusNavWidget();

		mpSet->SetAttentionWidget(this);

		cVector2f vBorders[2] = {mvGfxBorders[1]->GetActiveSize(), mvGfxBorders[2]->GetActiveSize()};
		float fGlobalY = GetGlobalPosition().y;
		OhplW2ComboOpen open;
		ohpl_w2_combo_open(&mCombo, (int)mvItems.size(), &mvSize, vBorders, fGlobalY, mpSet->GetPopUpZ(),
						   mpSet->GetVirtualSize().y, &open);

		if(open.slider)
		{
			mpSlider->SetEnabled(true);
			mpSlider->SetVisible(true);

			mpSlider->SetPosition(open.slider_pos);
			mpSlider->SetSize(open.slider_size);

			mpSet->SetFocusedWidget(this);
		}
		else
		{
			mpSet->SetFocusedWidget(this);
		}
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::CloseMenu()
	{
		if(ohpl_w2_combo_close(&mCombo)==false) return;

		mpSet->PopAttentionWidget();
		mpSet->PopFocusedWidget();
		mpSet->PopDefaultFocusNavWidget();

		mpSlider->SetEnabled(false);
		mpSlider->SetVisible(false);

		mbClipsGraphics = false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::ButtonPress(iWidget* apWidget, const cGuiMessageData& aData)
	{
		cVector3f vLocalPos(0);
		if(aData.mlVal==eGuiMouseButton_Left && mCombo.menu_open)
			vLocalPos = WorldToLocalPosition(aData.mvPos);

		switch(ohpl_w2_combo_button_press(&mCombo, aData.mlVal, &vLocalPos, mvSize.x))
		{
		case 1:
			return OnMouseDown(aData) && OnMouseUp(aData);
		case 2:
			CloseMenu();
			return true;
		case 3:
			OpenMenu();
			mCombo.clicked_after_open = false;
			return true;
		}
		return false;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetComboBox,ButtonPress)

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::SliderMove(iWidget* apWidget, const cGuiMessageData& aData)
	{
		mCombo.first_item = aData.mlVal;

		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetComboBox,SliderMove)

	bool cWidgetComboBox::SliderLostFocus(iWidget* apWidget, const cGuiMessageData& aData)
	{
		if(mCombo.menu_open && GetMouseIsOver()==false)
		{
			CloseMenu();
		}

		return false;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetComboBox,SliderLostFocus)

	//-----------------------------------------------------------------------

	void cWidgetComboBox::OnInit()
	{
		mpText = mpSet->CreateWidgetTextBox(0,mvSize,_W(""),this);
		mpText->SetText(msText);
		mpText->SetCanEdit(false);

		mpButton = mpSet->CreateWidgetButton(0,0,_W(""),this);
		mpButton->SetImage(mpSkin->GetGfx(eGuiSkinGfx_ComboBoxButtonIcon), false);
		mpButton->AddCallback(eGuiMessage_MouseDown, this, kGuiCallback(ButtonPress));

		mpSlider = mpSet->CreateWidgetSlider(eWidgetSliderOrientation_Vertical,0,0,0,this);
		mpSlider->AddCallback(eGuiMessage_SliderMove,this,kGuiCallback(SliderMove));
		mpSlider->AddCallback(eGuiMessage_LostFocus,this,kGuiCallback(SliderLostFocus));
		mpSlider->AddCallback(eGuiMessage_UIArrowPress, this, kGuiCallback(SliderOnUIArrowPress));
		mpSlider->AddCallback(eGuiMessage_UIButtonPress, this, kGuiCallback(SliderOnUIButtonPress));
		mpSlider->AddCallback(eGuiMessage_UIButtonRelease, this, kGuiCallback(SliderOnUIButtonRelease));
		mpSlider->SetEnabled(false);
		mpSlider->SetVisible(false);



		AddCallback(eGuiMessage_OnDraw,this, kGuiCallback(DrawText));

		OnChangeSize();
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::OnChangeSize()
	{
		if(mpText && mpButton && mpSlider)
		{
			mpText->SetSize(mvSize);
			mvSize = mpText->GetSize();

			cVector2f vBackSize = mpText->GetBackgroundSize();

			OhplW2ComboLayout layout;
			ohpl_w2_combo_layout(&mvSize, &vBackSize, mfButtonWidth, &layout);
			mpButton->SetSize(layout.button_size);
			mpButton->SetPosition(layout.button_pos);

			mpSlider->SetBarValueSize(mCombo.max_items);
			mpSlider->SetMaxValue((int)mvItems.size() - mCombo.max_items);
		}
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::OnChangeText()
	{
		if(mpText) mpText->SetText(msText);
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::OnLoadGraphics()
	{
		mpGfxBackground = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxBackground);

		mpGfxSelection = mpSkin->GetGfx(eGuiSkinGfx_TextBoxSelectedTextBack);

		mvGfxBorders[0] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxBorderRight);
		mvGfxBorders[1] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxBorderLeft);
		mvGfxBorders[2] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxBorderUp);
		mvGfxBorders[3] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxBorderDown);

		mvGfxCorners[0] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxCornerLU);
		mvGfxCorners[1] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxCornerRU);
		mvGfxCorners[2] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxCornerRD);
		mvGfxCorners[3] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxCornerLD);
	}

	//-----------------------------------------------------------------------

	void cWidgetComboBox::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		if(mCombo.menu_open)
		{
			///////////////////////////////
			// Set up base as clip region
			cGuiClipRegion *pPrevClipRegion = mpSet->GetCurrentClipRegion();
			mpSet->SetCurrentClipRegion(mpSet->GetBaseClipRegion());

			cVector3f vGlobal = GetGlobalPosition();
			OhplW2ComboDraw draw;
			ohpl_w2_combo_draw(&mCombo, &vGlobal, mvSize.x, &draw);

			////////////////////////////
			//Draw background
			mpSet->DrawGfx(mpGfxBackground, draw.bg_pos, draw.bg_size);

			////////////////////////////////
			// Draw borders
			DrawBordersAndCorners(	NULL, mvGfxBorders, mvGfxCorners,
									draw.border_pos, draw.border_size);

			///////////////////////////////
			// Reset clip region
			mpSet->SetCurrentClipRegion(pPrevClipRegion);
		}
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::DrawText(iWidget* apWidget, const cGuiMessageData& aData)
	{
		if(mCombo.menu_open==false) return false;

		///////////////////////////////
		// Set up base as clip region
		cGuiClipRegion *pPrevClipRegion = mpSet->GetCurrentClipRegion();
		mpSet->SetCurrentClipRegion(mpSet->GetBaseClipRegion());

		///////////////////////////////
		// Draw text entries
		cVector3f vGlobal = GetGlobalPosition();
		std::vector<OhplW2RowOp> vOps(mvItems.size()*2+2);
		size_t lCount = ohpl_w2_combo_rows(&mCombo, &vGlobal, mvSize.x, mvDefaultFontSize.y, (int)mvItems.size(),
										   vOps.data(), vOps.size());
		for(size_t i=0; i<lCount; ++i)
		{
			const OhplW2RowOp& op = vOps[i];
			switch(op.kind)
			{
			case kOhplW2OpSelection:
				mpSet->DrawGfx(mpGfxSelection, op.pos, op.size);
				break;
			case kOhplW2OpTextHighlight:
				DrawDefaultTextHighlight(mvItems[op.item]->GetText(),op.pos,eFontAlign_Left);
				break;
			case kOhplW2OpText:
				DrawDefaultText(mvItems[op.item]->GetText(),op.pos,eFontAlign_Left);
				break;
			}
		}

		///////////////////////////////
		// Reset clip region
		mpSet->SetCurrentClipRegion(pPrevClipRegion);


		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetComboBox,DrawText)

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::OnMouseMove(const cGuiMessageData& aData)
	{
		if(mCombo.menu_open==false) return false;

		if(GetMouseIsOver()==false) return false;

		cVector3f vLocalPos = WorldToLocalPosition(aData.mvPos);

		return ohpl_w2_combo_mouse_move(&mCombo, true, &vLocalPos, mvSize.x, mpSlider->IsEnabled(),
										mvGfxCorners[0]->GetActiveSize().y, mvDefaultFontSize.y, (int)mvItems.size());
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::OnMouseDown(const cGuiMessageData& aData)
	{
		int lWheel;
		if(ohpl_w2_combo_mouse_down_pre(&mCombo, aData.mlVal, &lWheel)==false) return false;

		int lSliderValue = mpSlider->GetValue();
		int lValueAdd = mpSlider->GetButtonValueAdd();

		if(lWheel < 0)
			mpSlider->SetValue(lSliderValue-lValueAdd);
		else if(lWheel > 0)
			mpSlider->SetValue(lSliderValue+lValueAdd);
		else
		{
			cVector3f vLocal = WorldToLocalPosition(aData.mvPos);

			if(ohpl_w2_combo_mouse_down_closes(&mCombo, vLocal.y))
			{
				CloseMenu();
			}
		}

		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::OnMouseUp(const cGuiMessageData& aData)
	{
		if(ohpl_w2_combo_mouse_up_pre(&mCombo, aData.mlVal)==false) return false;

		cVector3f vLocal = WorldToLocalPosition(aData.mvPos);

		if(ohpl_w2_combo_mouse_up_selects(&mCombo, vLocal.y))
		{
			SetSelectedItem(mCombo.mouse_over);
			CloseMenu();
		}

		return true;
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
		if(mCombo.menu_open==false) return false;

		int lSet;
		bool bHasSet;
		bool bRet = ohpl_w2_combo_ui_arrow(&mCombo, aData.mlVal, GetItemNum(), mpSlider->GetValue(),
										   mpSlider->GetButtonValueAdd(), &lSet, &bHasSet);
		if(bHasSet)
			mpSlider->SetValue(lSet);

		return bRet;
	}

	bool cWidgetComboBox::OnUIButtonPress(const cGuiMessageData& aData)
	{
		OhplW2ComboUiButton button = ohpl_w2_combo_ui_button(&mCombo, HasFocus(), aData.mlVal);

		switch(button.action)
		{
		case 1:
			CloseMenu();
			break;
		case 2:
			OpenMenu();
			break;
		case 3:
			SetSelectedItem(mCombo.mouse_over, true, true);
			CloseMenu();
			if(button.clear_open_by_ui)
				mCombo.open_by_ui = false;
			break;
		}

		return button.ret;
	}

	bool cWidgetComboBox::OnUIButtonRelease(const cGuiMessageData& aData)
	{
		if(HasFocus())
			return aData.mlVal==eUIButton_Primary;

		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::OnLostFocus(const cGuiMessageData& aData)
	{
		if(mCombo.menu_open && mCombo.open_by_ui==false)
		{
			cVector3f vLocal = WorldToLocalPosition(aData.mvPos);
			if(ohpl_w2_combo_lost_focus(&mCombo, &vLocal, mvSize.x))
				CloseMenu();
		}

		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::PointIsInside(const cVector2f& avPoint, bool abOnlyClipped)
	{
		if(mCombo.menu_open==false && CheckPointInsideClippingParent(avPoint)==false)
			return false;

		if(abOnlyClipped && mbClipsGraphics==false) return true;

		cVector3f vMousePos = WorldToLocalPosition(cVector3f(avPoint));

		return ohpl_w2_combo_point_inside(&mCombo, &vMousePos, &mvSize);
	}

	//-----------------------------------------------------------------------

	bool cWidgetComboBox::SliderOnUIArrowPress(iWidget* apWidget, const cGuiMessageData& aData)
	{
		return ohpl_w2_combo_slider_arrow(&mCombo, aData.mlVal, GetItemNum());
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetComboBox, SliderOnUIArrowPress);

	bool cWidgetComboBox::SliderOnUIButtonPress(iWidget* apWidget, const cGuiMessageData& aData)
	{
		if(aData.mlVal==eUIButton_Primary)
		{
			SetSelectedItem(mCombo.mouse_over);
			CloseMenu();

			mCombo.open_by_ui = false;
			mpSet->SetFocusedWidget(this);
		}

		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetComboBox, SliderOnUIButtonPress);

	bool cWidgetComboBox::SliderOnUIButtonRelease(iWidget* apWidget, const cGuiMessageData& aData)
	{
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetComboBox, SliderOnUIButtonRelease);

	//-----------------------------------------------------------------------


}
