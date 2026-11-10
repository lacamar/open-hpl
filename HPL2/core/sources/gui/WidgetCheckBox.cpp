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

#include "gui/WidgetCheckBox.h"

#include "system/LowLevelSystem.h"

#include "system/String.h"

#include "math/Math.h"

#include "graphics/FontData.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"
#include "gui/WidgetLabel.h"
#include "gui/RustWidget.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cWidgetCheckBox::cWidgetCheckBox(cGuiSet *apSet, cGuiSkin *apSkin) : iWidget(eWidgetType_CheckBox,apSet, apSkin)
	{
        mbChecked = false;
		mbPressed = false;

		LoadGraphics();
		mpLabel = mpSet->CreateWidgetLabel(	0,-1,_W(""), this);
		mpLabel->SetAutogenerateSize(true);
	}

	//-----------------------------------------------------------------------

	cWidgetCheckBox::~cWidgetCheckBox()
	{
		if(mpSet->IsDestroyingSet()==false)
		{
			mpSet->DestroyWidget(mpLabel);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cWidgetCheckBox::SetChecked(bool abX, bool abGenCallback)
	{
		if(mbChecked == abX) return;

		mbChecked = abX;
        
		if(abGenCallback) {
			ProcessMessage(eGuiMessage_CheckChange, cGuiMessageData(mbChecked));
		}
	}
	
	//-----------------------------------------------------------------------

	void cWidgetCheckBox::SetDefaultFontColor(const cColor& aColor)
	{
		iWidget::SetDefaultFontColor(aColor);

		mpLabel->SetDefaultFontColor(aColor);
	}

	//-----------------------------------------------------------------------

	void cWidgetCheckBox::SetDefaultFontSize(const cVector2f& avSize)
	{
		iWidget::SetDefaultFontSize(avSize);

		mpLabel->SetDefaultFontSize(avSize);
		UpdateLabel();
	}

	//-----------------------------------------------------------------------

	void cWidgetCheckBox::SetDefaultFontType(iFontData* apFont)
	{
		iWidget::SetDefaultFontType(apFont);

		mpLabel->SetDefaultFontType(apFont);
		UpdateLabel();
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool cWidgetCheckBox::Label_MouseDown(iWidget *apWidget, const cGuiMessageData& aData)
	{
		bool bForward;
		bool bRet = ohpl_wd_check_label_forward(aData.mlVal, mbEnabled, &bForward);
		if(bForward)
			ProcessMessage(eGuiMessage_MouseDown,aData);
		return bRet;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetCheckBox, Label_MouseDown);

	//-----------------------------------------------------------------------

	bool cWidgetCheckBox::Label_MouseUp(iWidget *apWidget, const cGuiMessageData& aData)
	{
		bool bForward;
		bool bRet = ohpl_wd_check_label_forward(aData.mlVal, mbEnabled, &bForward);
		if(bForward)
			ProcessMessage(eGuiMessage_MouseUp,aData);
		return bRet;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetCheckBox, Label_MouseUp);

	//-----------------------------------------------------------------------

	void cWidgetCheckBox::UpdateLabel()
	{
		mpLabel->SetText(msText);
		OnChangeSize();
	}

	//-----------------------------------------------------------------------

	void cWidgetCheckBox::OnInit()
	{
		cVector3f vLabelPos;
		ohpl_wd_check_label_pos(mvGfxBox[0][0]->GetActiveSize().x, mvBoxSize.y, mvDefaultFontSize.y, &vLabelPos);
		mpLabel->SetPosition(vLabelPos);
		mpLabel->AddCallback(eGuiMessage_MouseDown,this,kGuiCallback(Label_MouseDown));
		mpLabel->AddCallback(eGuiMessage_MouseUp,this,kGuiCallback(Label_MouseUp));
	}

	//-----------------------------------------------------------------------

	void cWidgetCheckBox::OnLoadGraphics()
	{
		mvGfxBox[1][0] = mpSkin->GetGfx(eGuiSkinGfx_CheckBoxEnabledUnchecked);
		mvGfxBox[1][1] = mpSkin->GetGfx(eGuiSkinGfx_CheckBoxEnabledChecked);
		mvGfxBox[0][0] = mpSkin->GetGfx(eGuiSkinGfx_CheckBoxDisabledUnchecked);
		mvGfxBox[0][1] = mpSkin->GetGfx(eGuiSkinGfx_CheckBoxDisabledChecked);
	}
	
	//-----------------------------------------------------------------------

	void cWidgetCheckBox::OnChangeSize()
	{
		//if(mvSize.y < mvDefaultFontSize.y) mvSize.y = mvDefaultFontSize.y;
		//if(mvSize.x < mvGfxBox[0][0]->GetActiveSize().x) mvSize.x = mvGfxBox[0][0]->GetActiveSize().x;
		cVector2f vLabelSize = mpLabel->GetSize();
		OhplWdCheckLayout layout;
		ohpl_wd_check_layout(mvGfxBox[0][0]->GetActiveSize().x, mvDefaultFontSize.y, mpLabel->GetLocalPosition().x, &vLabelSize, &layout);
		mvBoxSize = layout.box_size;
		mvSize = layout.size;
	}

	//-----------------------------------------------------------------------

	void cWidgetCheckBox::OnChangeText()
	{
		UpdateLabel();
	}

	//-----------------------------------------------------------------------

	void cWidgetCheckBox::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		//Log("cWidgetCheckBox::OnDraw - text = %s - address:0x%x - Before DrawGfx ... ", cString::To8Char(msText).c_str(), this);
		////////////////////////////////
		// Box
		cVector3f vGlobal = GetGlobalPosition();
		cVector3f vBoxPos;
		ohpl_wd_check_box_pos(&vGlobal, mvBoxSize.y, mvGfxBox[0][0]->GetActiveSize().y, &vBoxPos);
		mpSet->DrawGfx( mvGfxBox[IsEnabled()][mbChecked], vBoxPos);

		//Log("After DrawGfx\n");
		/*
		////////////////////////////////
		// Text
		eGuiSkinFont font = IsEnabled() ? eGuiSkinFont_Default : eGuiSkinFont_Disabled;
		DrawSkinText(			msText,font, GetGlobalPosition() + 
							cVector3f(	mvGfxBox[0][0]->GetActiveSize().x +3.0f,
										mvSize.y/2 - mvDefaultFontSize.y/2,0),
							eFontAlign_Left);*/
	}

	//-----------------------------------------------------------------------
	
	bool cWidgetCheckBox::OnMouseMove(const cGuiMessageData& aData)
	{
		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetCheckBox::OnMouseDown(const cGuiMessageData& aData)
	{
		if(ohpl_wd_check_mouse_down(aData.mlVal)==false)
			return false;

		mbPressed = true;
		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetCheckBox::OnMouseUp(const cGuiMessageData& aData)
	{
		if(mbPressed) SetChecked(!mbChecked);
		
		mbPressed = false;
		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetCheckBox::OnMouseEnter(const cGuiMessageData& aData)
	{
		return false;
	}
	
	//-----------------------------------------------------------------------
	
	bool cWidgetCheckBox::OnMouseLeave(const cGuiMessageData& aData)
	{
		mbPressed = false;
		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetCheckBox::OnUIButtonPress(const cGuiMessageData& aData)
	{
		if(HasFocus())
		{
			if(ohpl_wd_ui_primary(aData.mlVal))
				return OnMouseDown(cGuiMessageData(eGuiMouseButton_Left));
		}
		
		return false;
	}

	bool cWidgetCheckBox::OnUIButtonRelease(const cGuiMessageData& aData)
	{
		if(HasFocus())
		{
			if(ohpl_wd_ui_primary(aData.mlVal))
			return OnMouseUp(cGuiMessageData(eGuiMouseButton_Left));
		}

		return false;
	}

	//-----------------------------------------------------------------------

}
