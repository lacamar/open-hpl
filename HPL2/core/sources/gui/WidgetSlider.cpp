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

#include "gui/WidgetSlider.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include "system/String.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"

#include "gui/WidgetButton.h"

#include "graphics/FontData.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cWidgetSlider::cWidgetSlider(cGuiSet *apSet, cGuiSkin *apSkin, 
								eWidgetSliderOrientation aOrientation) 
					: iWidget(eWidgetType_Slider,apSet, apSkin)
	{
		mSlider = ohpl_w2_slider_init(aOrientation == eWidgetSliderOrientation_Vertical,
									  mpSkin->GetAttribute(eGuiSkinAttribute_SliderButtonSize).x);

		for(int i=0; i<2; ++i) mvButtons[i] = NULL;

		LoadGraphics();
	}

	//-----------------------------------------------------------------------

	cWidgetSlider::~cWidgetSlider()
	{
		if(mpSet->IsDestroyingSet()==false)
		{
			for(int i=0; i<2; ++i) mpSet->DestroyWidget(mvButtons[i]);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cWidgetSlider::SetValue(int alValue, bool abGenCallback)
	{ 
		int lNew;
		if(ohpl_w2_slider_set_value(&mSlider, alValue, &lNew)==false) return;

		if(abGenCallback) 
		{
			ProcessMessage(eGuiMessage_SliderMove, cGuiMessageData(mSlider.value));
			if(mSlider.pressed==false)
				ProcessMessage(eGuiMessage_SliderRelease, cGuiMessageData(mSlider.value));
		}
		UpdateBarProperties();
	}

	void cWidgetSlider::SetMaxValue(int alMax)
	{ 
		if(ohpl_w2_slider_set_max(&mSlider, alMax)) UpdateBarProperties();
	}

	void cWidgetSlider::SetButtonValueAdd(int alAdd)
	{ 
		mSlider.button_add = alAdd;
	}

	void cWidgetSlider::SetBarClickValueAdd(int alAdd)
	{
		mSlider.click_add = alAdd;
	}

	void cWidgetSlider::SetBarValueSize(int alSize)
	{ 
		if(ohpl_w2_slider_set_bar_size(&mSlider, alSize)) UpdateBarProperties();
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cWidgetSlider::UpdateBarProperties()
	{
		cVector2f vCorner0 = mvGfxCorners[0]->GetActiveSize();
		cVector2f vCorner3 = mvGfxCorners[3]->GetActiveSize();
		cVector3f vGlobal = GetGlobalPosition();
		ohpl_w2_slider_update_bar(&mSlider, &mvSize, &vGlobal, &vCorner0, &vCorner3);
	}
	
	//-----------------------------------------------------------------------

	bool cWidgetSlider::ArrowButtonDown(iWidget* apWidget, const cGuiMessageData& aData)
	{	
		int i;
		for(i=0; i<2; ++i) if(mvButtons[i] == apWidget) break;

		if(i==0) SetValue(mSlider.value-mSlider.button_add);
		if(i==1) SetValue(mSlider.value+mSlider.button_add);
		
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetSlider,ArrowButtonDown)

	//-----------------------------------------------------------------------

	void cWidgetSlider::OnInit()
	{
		mvButtons[0] = mpSet->CreateWidgetButton(0,0,_W(""), this);
		mvButtons[1] = mpSet->CreateWidgetButton(0,0,_W(""), this);

		for(int i=0; i<2;i++)
		{
			mvButtons[i]->SetImage(mvGfxArrow[i],false);
			
			mvButtons[i]->AddCallback(eGuiMessage_MouseDown,this,kGuiCallback(ArrowButtonDown));
		}

		OnChangeSize();
	}
	
	//-----------------------------------------------------------------------

	void cWidgetSlider::OnLoadGraphics()
	{
		/////////////////////////
		//Background, corners and borders
		mpGfxButtonBackground = mpSkin->GetGfx(eGuiSkinGfx_SliderHandleBackground);

		mvGfxBorders[0] = mpSkin->GetGfx(eGuiSkinGfx_SliderHandleBorderRight);
		mvGfxBorders[1] = mpSkin->GetGfx(eGuiSkinGfx_SliderHandleBorderLeft);
		mvGfxBorders[2] = mpSkin->GetGfx(eGuiSkinGfx_SliderHandleBorderUp);
		mvGfxBorders[3] = mpSkin->GetGfx(eGuiSkinGfx_SliderHandleBorderDown);

		mvGfxCorners[0] = mpSkin->GetGfx(eGuiSkinGfx_SliderHandleCornerLU);
		mvGfxCorners[1] = mpSkin->GetGfx(eGuiSkinGfx_SliderHandleCornerRU);
		mvGfxCorners[2] = mpSkin->GetGfx(eGuiSkinGfx_SliderHandleCornerRD);
		mvGfxCorners[3] = mpSkin->GetGfx(eGuiSkinGfx_SliderHandleCornerLD);
		
		if(mSlider.vertical)
		{
			mvGfxArrow[0] = mpSkin->GetGfx(eGuiSkinGfx_SliderVertArrowUp);
			mvGfxArrow[1] = mpSkin->GetGfx(eGuiSkinGfx_SliderVertArrowDown);
			mpGfxBackground = mpSkin->GetGfx(eGuiSkinGfx_SliderVertBackground);
		}
		else
		{
			mvGfxArrow[0] = mpSkin->GetGfx(eGuiSkinGfx_SliderHoriArrowUp);
			mvGfxArrow[1] = mpSkin->GetGfx(eGuiSkinGfx_SliderHoriArrowDown);
			mpGfxBackground = mpSkin->GetGfx(eGuiSkinGfx_SliderHoriBackground);
		}

		mSlider.button_size = mpSkin->GetAttribute(eGuiSkinAttribute_SliderButtonSize).x;
	}

	//-----------------------------------------------------------------------

	void cWidgetSlider::OnChangeSize()
	{
		if(mvButtons[0] == NULL || mvButtons[1] == NULL) return;

		OhplW2PosSize vButtons[2];
		ohpl_w2_slider_buttons(mSlider.vertical, &mvSize, mSlider.button_size, vButtons);
		for(int i=0; i<2; ++i)
		{
			mvButtons[i]->SetPosition(vButtons[i].pos);
			mvButtons[i]->SetSize(vButtons[i].size);
		}

		UpdateBarProperties();
	}
	
	//-----------------------------------------------------------------------

	void cWidgetSlider::OnChangePosition()
	{
		UpdateBarProperties();
	}

	//-----------------------------------------------------------------------

	void cWidgetSlider::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{	
		cVector3f vGlobal = GetGlobalPosition();
		OhplW2SliderDraw d;
		ohpl_w2_slider_draw(&mSlider, &mvSize, &vGlobal, &d);

		mpSet->DrawGfx(mpGfxBackground, d.track_pos, d.track_size);

		DrawBordersAndCorners(	mpGfxButtonBackground, mvGfxBorders, mvGfxCorners, 
								d.bar_pos, d.bar_size);
	}

	//-----------------------------------------------------------------------
	
	bool cWidgetSlider::OnMouseMove(const cGuiMessageData& aData)
	{
		if(mSlider.pressed)
		{
			cVector3f vLocal = WorldToLocalPosition(aData.mvPos);
			cVector3f vGlobal = GetGlobalPosition();
			int lVal;
			if(ohpl_w2_slider_mouse_move(&mSlider, &vLocal, &vGlobal, &lVal))
				ProcessMessage(eGuiMessage_SliderMove, cGuiMessageData(mSlider.value));
		}

		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetSlider::OnMouseDown(const cGuiMessageData& aData)
	{
		if((aData.mlVal & eGuiMouseButton_WheelUp) || (aData.mlVal & eGuiMouseButton_WheelDown))
		{
			return false;
		}

		cVector3f vLocal = WorldToLocalPosition(aData.mvPos);
		cVector3f vGlobal = GetGlobalPosition();
		int lVal;
		bool bFire = false;
		bool bRet = ohpl_w2_slider_mouse_down(&mSlider, aData.mlVal, &aData.mvPos, &vLocal, &vGlobal, &lVal, &bFire);
		if(bFire)
			ProcessMessage(eGuiMessage_SliderMove, cGuiMessageData(mSlider.value));
		
		return bRet;
	}

	//-----------------------------------------------------------------------

	bool cWidgetSlider::OnMouseUp(const cGuiMessageData& aData)
	{
		if(ohpl_w2_slider_release(&mSlider))
		{
			UpdateBarProperties();
		}

		ProcessMessage(eGuiMessage_SliderRelease, cGuiMessageData(mSlider.value));
		
		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetSlider::OnMouseEnter(const cGuiMessageData& aData)
	{
		return false;
	}
	
	//-----------------------------------------------------------------------
	
	bool cWidgetSlider::OnMouseLeave(const cGuiMessageData& aData)
	{
		return false;
	}

	bool cWidgetSlider::OnMouseDoubleClick(const cGuiMessageData& aData)
	{
		return false;
	}

	bool cWidgetSlider::OnLostFocus(const cGuiMessageData& aData)
	{
		if(ohpl_w2_slider_release(&mSlider))
		{
			UpdateBarProperties();
		}

		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetSlider::OnUIArrowPress(const cGuiMessageData& aData)
	{
		int lButton = ohpl_w2_slider_arrow(mSlider.vertical, aData.mlVal);
		if(lButton < 0) return false;

		return ArrowButtonDown(mvButtons[lButton], aData);
	}

	bool cWidgetSlider::OnUIArrowRelease(const cGuiMessageData& aData)
	{
		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetSlider::OnUIButtonPress(const cGuiMessageData& aData)
	{
		return false;
	}

	bool cWidgetSlider::OnUIButtonRelease(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------


}
