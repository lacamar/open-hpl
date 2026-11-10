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

#include "gui/WidgetButton.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include "system/String.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"
#include "gui/RustWidget.h"

#include "graphics/FontData.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cWidgetButton::cWidgetButton(cGuiSet *apSet, cGuiSkin *apSkin) : iWidget(eWidgetType_Button,apSet, apSkin)
	{
		mbToggleable = false;
		mbPressed = false;
		mpImage = NULL;

		mfTimer = 0;
		mbRepeatActive = false;
		mfRepeatPeriod = 1;
		mlRepeatTimes = 0;
	}

	//-----------------------------------------------------------------------

	cWidgetButton::~cWidgetButton()
	{
		if(mpImage && mbDestroyImage)
		{
			mpGui->DestroyGfx(mpImage);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cWidgetButton::SetImage(cGuiGfxElement *apImage, bool abDestroyImage)
	{
		mpImage = apImage;
		mbDestroyImage = abDestroyImage;
	}

	//-----------------------------------------------------------------------

	void cWidgetButton::SetRepeatFreq(float afFreq)
	{
		mfRepeatPeriod = ohpl_wd_button_repeat_period(afFreq);
	}

	void cWidgetButton::SetPressed(bool abX, bool abGenCallback)
	{
		if(mbToggleable)
		{
			mbPressed = abX;
			cGuiMessageData data;
			if(abGenCallback)
				ProcessMessage(eGuiMessage_ButtonPressed, data);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetButton::OnLoadGraphics()
	{
		/////////////////////////
		//Up
		mpGfxBackgroundUp = mpSkin->GetGfx(eGuiSkinGfx_ButtonUpBackground);

		mvGfxBordersUp[0] = mpSkin->GetGfx(eGuiSkinGfx_ButtonUpBorderRight);
		mvGfxBordersUp[1] = mpSkin->GetGfx(eGuiSkinGfx_ButtonUpBorderLeft);
		mvGfxBordersUp[2] = mpSkin->GetGfx(eGuiSkinGfx_ButtonUpBorderUp);
		mvGfxBordersUp[3] = mpSkin->GetGfx(eGuiSkinGfx_ButtonUpBorderDown);

		mvGfxCornersUp[0] = mpSkin->GetGfx(eGuiSkinGfx_ButtonUpCornerLU);
		mvGfxCornersUp[1] = mpSkin->GetGfx(eGuiSkinGfx_ButtonUpCornerRU);
		mvGfxCornersUp[2] = mpSkin->GetGfx(eGuiSkinGfx_ButtonUpCornerRD);
		mvGfxCornersUp[3] = mpSkin->GetGfx(eGuiSkinGfx_ButtonUpCornerLD);

		/////////////////////////
		//Down
		mpGfxBackgroundDown = mpSkin->GetGfx(eGuiSkinGfx_ButtonDownBackground);

		mvGfxBordersDown[0] = mpSkin->GetGfx(eGuiSkinGfx_ButtonDownBorderRight);
		mvGfxBordersDown[1] = mpSkin->GetGfx(eGuiSkinGfx_ButtonDownBorderLeft);
		mvGfxBordersDown[2] = mpSkin->GetGfx(eGuiSkinGfx_ButtonDownBorderUp);
		mvGfxBordersDown[3] = mpSkin->GetGfx(eGuiSkinGfx_ButtonDownBorderDown);

		mvGfxCornersDown[0] = mpSkin->GetGfx(eGuiSkinGfx_ButtonDownCornerLU);
		mvGfxCornersDown[1] = mpSkin->GetGfx(eGuiSkinGfx_ButtonDownCornerRU);
		mvGfxCornersDown[2] = mpSkin->GetGfx(eGuiSkinGfx_ButtonDownCornerRD);
		mvGfxCornersDown[3] = mpSkin->GetGfx(eGuiSkinGfx_ButtonDownCornerLD);
	}

	//-----------------------------------------------------------------------

	void cWidgetButton::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		OhplWdButtonDraw d;
		d.global = GetGlobalPosition();
		d.size = mvSize;
		d.pressed = mbPressed;
		d.enabled = IsEnabled();
		d.has_image = mpImage != NULL;
		d.has_default_font = mpDefaultFontType != NULL;
		d.pressed_offset = mbPressed ? mpSkin->GetAttribute(eGuiSkinAttribute_ButtonPressedContentOffset) : cVector3f(0);
		d.font_size = mvDefaultFontSize;
		d.font_color = mDefaultFontColor;
		d.color_mul = mColorMul;
		d.has_background = (mbPressed ? mpGfxBackgroundDown : mpGfxBackgroundUp) != NULL;

		OhplWdGfxTable table;
		if(mbPressed)	table.SetFrame(mpGfxBackgroundDown, mvGfxBordersDown, mvGfxCornersDown);
		else			table.SetFrame(mpGfxBackgroundUp, mvGfxBordersUp, mvGfxCornersUp);
		table.Set(kOhplWdGfxImage, mpImage);

		OhplWdOp ops[kOhplWdMaxOps];
		size_t lCount = ohpl_wd_button_draw(ops, kOhplWdMaxOps, &d, table.m);
		const tWString* texts[1] = { &msText };
		iFontData* fonts[1] = { mpDefaultFontType };
		OhplWdRun(mpSet, ops, lCount, table, texts, fonts);
	}

	//-----------------------------------------------------------------------

	void cWidgetButton::OnUpdate(float afTimeStep)
	{
		OhplWdButtonAction act;
		OhplWdButtonState state = GetRustState();
		ohpl_wd_button_update(&state, afTimeStep, &act);
		mfTimer = act.state.timer;
		mlRepeatTimes = act.state.repeat_times;
		if(act.fire)
			ProcessMessage(eGuiMessage_ButtonPressed, mPressedData);
	}

	//-----------------------------------------------------------------------
	
	bool cWidgetButton::OnMouseMove(const cGuiMessageData& aData)
	{
		return true;
	}

	//-----------------------------------------------------------------------

	OhplWdButtonState cWidgetButton::GetRustState()
	{
		OhplWdButtonState state;
		state.toggleable = mbToggleable;
		state.pressed = mbPressed;
		state.repeat_active = mbRepeatActive;
		state.repeat_times = mlRepeatTimes;
		state.timer = mfTimer;
		state.repeat_period = mfRepeatPeriod;
		return state;
	}

	//-----------------------------------------------------------------------

	bool cWidgetButton::OnMouseDown(const cGuiMessageData& aData)
	{
		OhplWdButtonAction act;
		OhplWdButtonState state = GetRustState();
		ohpl_wd_button_mouse_down(&state, IsEnabled(), aData.mlVal, &act);
		if(act.ret==false) return false;

		mbPressed = act.state.pressed;
		if(act.fire)
			ProcessMessage(eGuiMessage_ButtonPressed,aData);
		if(act.store_data)
			mPressedData = aData;
		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetButton::OnMouseUp(const cGuiMessageData& aData)
	{
		OhplWdButtonAction act;
		OhplWdButtonState state = GetRustState();
		ohpl_wd_button_mouse_up(&state, IsEnabled(), aData.mlVal, GetMouseIsOver(), &act);
		if(act.fire)
			ProcessMessage(eGuiMessage_ButtonPressed,aData);
		mbPressed = act.state.pressed;
		mlRepeatTimes = act.state.repeat_times;
		return act.ret;
	}

	//-----------------------------------------------------------------------

	bool cWidgetButton::OnMouseEnter(const cGuiMessageData& aData)
	{
		return false;
	}
	
	//-----------------------------------------------------------------------
	
	bool cWidgetButton::OnMouseLeave(const cGuiMessageData& aData)
	{
		if(mbToggleable==false)
			mbPressed = false;

		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetButton::OnLostFocus(const cGuiMessageData& aData)
	{
		if(mbToggleable==false)
			mbPressed = false;

		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetButton::OnUIButtonPress(const cGuiMessageData& aData)
	{
		if(HasFocus())
		{
			if(ohpl_wd_ui_primary(aData.mlVal))
				return OnMouseDown(cGuiMessageData(eGuiMouseButton_Left));
		}
		
		return false;
	}

	bool cWidgetButton::OnUIButtonRelease(const cGuiMessageData& aData)
	{
		if(HasFocus())
		{
			if(ohpl_wd_ui_primary(aData.mlVal))
			{
				// Lil hack: make widget think mouse is over.
				bool bPrevMouseOver = mbMouseIsOver;
				mbMouseIsOver = true;
				{
					OnMouseUp(cGuiMessageData(eGuiMouseButton_Left));
				}
				mbMouseIsOver = bPrevMouseOver;

				return true;
			}
		}

		return false;
	}

	//-----------------------------------------------------------------------

}
