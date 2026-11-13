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

#include "gui/WidgetTextBox.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include "graphics/FontData.h"

#include "system/String.h"
#include "system/Platform.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"

#include "gui/WidgetButton.h"

#include <cstring>
#include <vector>

namespace hpl {

	int cWidgetTextBox::mlDefaultDecimals = 3;

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cWidgetTextBox::cWidgetTextBox(cGuiSet *apSet, cGuiSkin *apSkin, eWidgetTextBoxInputType aType) : iWidget(eWidgetType_TextBox,apSet, apSkin),
		mpPrevAttention(mH.prevAttention), mState(mH.st), mbShowButtons(mH.showButtons)
	{
		ohpl_h20_tb_init(&gH20Host, &mH, (uintptr_t)(iWidget*)this, &mB, aType==eWidgetTextBoxInputType_Numeric);
		mInputType = aType;
		mbShowButtons = false;

		memset(&mState, 0, sizeof(mState));
		LoadGraphics();

		mpPointerGfx = mpSkin->GetGfx(eGuiSkinGfx_PointerText);
		mpPrevAttention = NULL;

		mState = ohpl_w2_tb_init(aType==eWidgetTextBoxInputType_Numeric);

		msIllegalChars = _W("");

		mbUIKeyboardOpen = false;
	}

	//-----------------------------------------------------------------------

	cWidgetTextBox::~cWidgetTextBox()
	{
		if(mpSet->IsDestroyingSet()==false)
		{
			ohpl_h20_tb_destroy_buttons(&gH20Host, &mH);
		}
		ohpl_h20_tb_free(&mH);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetDefaultFontSize(const cVector2f& avSize)
	{
		iWidget::SetDefaultFontSize(avSize);
		OnChangeSize();
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetMaxTextLength(int alLength)
	{
		ohpl_h20_tb_set_max_text_length(&gH20Host, &mH, alLength);
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetSelectedText(int alStart, int alCount)
	{
		ohpl_h20_tb_set_selected_text(&gH20Host, &mH, alStart, alCount);
	}

	//-----------------------------------------------------------------------

	cVector2f cWidgetTextBox::GetBackgroundSize()
	{
		cVector2f vSize;
		ohpl_h20_tb_background_size(&gH20Host, &mH, &vSize);
		return vSize;
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetMaxTextSizeNeg(float afX)
	{
		mState.max_size_neg = afX;

		OnChangeSize();
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetCanEdit(bool abX)
	{
		ohpl_h20_tb_set_can_edit(&gH20Host, &mH, abX);
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetNumericValue(float afX)
	{
		ohpl_h20_tb_set_numeric_value(&gH20Host, &mH, afX);
	}

	//-----------------------------------------------------------------------

	float cWidgetTextBox::GetNumericValue()
	{
		return ohpl_h20_tb_numeric_value(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetLowerBound(bool abX, float afValue)
	{
		mState.has_lower = abX;
		mState.lower = afValue;
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetUpperBound(bool abX, float afValue)
	{
		mState.has_upper = abX;
		mState.upper = afValue;
	}

	//-----------------------------------------------------------------------

	int cWidgetTextBox::GetDecimals()
	{
		if(mState.decimals<0)
			return mlDefaultDecimals;

		return mState.decimals;
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetIllegalChars(const tWString &asIllegalChars)
	{
		msIllegalChars = asIllegalChars;
		ohpl_h20_tb_set_illegal_chars(&gH20Host, &mH, asIllegalChars.data(), asIllegalChars.size());
	}

	const tWString& cWidgetTextBox::GetIllegalChars()
	{
		return msIllegalChars;
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetLegalCharCodeLimitEnabled(bool abX)
	{
		mState.limit_enabled = abX;
	}

	void cWidgetTextBox::SetLegalCharCodeMinLimit(wchar_t alX)
	{
		mState.limit_min = alX;
	}

	void cWidgetTextBox::SetLegalCharCodeMaxLimit(wchar_t alX)
	{
		mState.limit_max = alX;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetTextBox::OnChangeSize()
	{
		ohpl_h20_tb_on_change_size(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::OnChangeText()
	{
		ohpl_h20_tb_on_change_text(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::OnInit()
	{
		ohpl_h20_tb_on_init(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::OnLoadGraphics()
	{
		ohpl_h20_tb_on_load_graphics(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		ohpl_h20_tb_on_draw(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnMouseMove(const cGuiMessageData& aData)
	{
		return ohpl_h20_tb_on_mouse_move(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnMouseDown(const cGuiMessageData& aData)
	{
		return ohpl_h20_tb_on_mouse_down(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnMouseUp(const cGuiMessageData& aData)
	{
		return ohpl_h20_tb_on_mouse_up(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnMouseDoubleClick(const cGuiMessageData& aData)
	{
		return ohpl_h20_tb_on_mouse_double_click(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnMouseEnter(const cGuiMessageData& aData)
	{
		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnMouseLeave(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnGotFocus(const cGuiMessageData& aData)
	{
		return ohpl_h20_tb_on_got_focus(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnGotTabFocus(const cGuiMessageData& aData)
	{
		return ohpl_h20_tb_on_got_tab_focus(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnLostFocus(const cGuiMessageData& aData)
	{
		return ohpl_h20_tb_on_lost_focus(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnKeyPress(const cGuiMessageData& aData)
	{
		return ohpl_h20_tb_on_key_press(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnUIButtonPress(const cGuiMessageData& aData)
	{
		return ohpl_h20_tb_on_ui_button_press(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnEnter(const cGuiMessageData& aData)
	{
		return ohpl_h20_tb_on_enter(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::OnUIKeyboardOpen()
	{
		mbUIKeyboardOpen = true;
		ohpl_h20_tb_ui_keyboard_open(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::OnUIKeyboardClose()
	{
		mbUIKeyboardOpen = false;
		OnLostFocus(cGuiMessageData());
	}

	//-----------------------------------------------------------------------

}
