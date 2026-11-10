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

#include "gui/WidgetFrame.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"
#include "gui/WidgetSlider.h"
#include "gui/RustWidget.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cWidgetFrame::cWidgetFrame(cGuiSet *apSet, cGuiSkin *apSkin, bool abHScrollBar, bool abVScrollBar) : iWidget(eWidgetType_Frame,apSet, apSkin)
	{
		mbClipsGraphics = true;

		mbHScrollBar = abHScrollBar;
		mbVScrollBar = abVScrollBar;

		mbDrawFrame = false;
		
		mbDrawBackground = false;
		mfBackgroundZ = -0.5;
		mBackGroundColor = cColor(1,1);

		mpHSlider = NULL;
		mpVSlider = NULL;
		mvMinWidgetCoord = 999999.9f;
		mvMaxWidgetCoord = -999999.9f;
		if(abHScrollBar)
		{
			mpHSlider = mpSet->CreateWidgetSlider(eWidgetSliderOrientation_Horizontal,0,cVector2f(0,18),0, this);
			mpHSlider->SetAffectedByScroll(false);
			mpHSlider->AddCallback(eGuiMessage_SliderMove, this, kGuiCallback(OnSliderMove));
			mpHSlider->SetVisible(false);
			mpHSlider->SetEnabled(false);
		}
		if(abVScrollBar)
		{
			mpVSlider = mpSet->CreateWidgetSlider(eWidgetSliderOrientation_Vertical,0,cVector2f(18,0),0, this);
			mpVSlider->SetAffectedByScroll(false);
			mpVSlider->AddCallback(eGuiMessage_SliderMove, this, kGuiCallback(OnSliderMove));
			mpVSlider->SetVisible(false);
			mpVSlider->SetEnabled(false);
		}

		mvAutoScrollMargin = 0;

		mbScrollBarsNeedUpdate = true;
		mbScrollUpdated = true;
	}

	//-----------------------------------------------------------------------

	cWidgetFrame::~cWidgetFrame()
	{
		if(mpSet->IsDestroyingSet()==false)
		{
			if(mpHSlider)
				mpSet->DestroyWidget(mpHSlider);
			if(mpVSlider)
				mpSet->DestroyWidget(mpVSlider);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetFrame::OnAttachChild(iWidget* apChild)
	{
		mbScrollBarsNeedUpdate = true;
	}

	//-----------------------------------------------------------------------

	void cWidgetFrame::OnRemoveChild(iWidget* apChild)
	{
		mbScrollBarsNeedUpdate = true;
	}

	//-----------------------------------------------------------------------

	void cWidgetFrame::ScrollToPosition(const cVector2f& avPos)
	{
		if(mpHSlider)
			mpHSlider->SetValue(cMath::RoundToInt(avPos.x));
		if(mpVSlider)
			mpVSlider->SetValue(cMath::RoundToInt(avPos.y));
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool cWidgetFrame::OnSliderMove(iWidget* apWidget, const cGuiMessageData& aData)
	{
		if(apWidget==mpHSlider || apWidget==mpVSlider)
			mbScrollUpdated = true;
		
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetFrame,OnSliderMove);

	//-----------------------------------------------------------------------

	void cWidgetFrame::OnLoadGraphics()
	{
		mpGfxBackground = mpSkin->GetGfx(eGuiSkinGfx_FrameBackground);

		mvGfxBorders[0] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderRight);
		mvGfxBorders[1] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderLeft);
		mvGfxBorders[2] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderUp);
		mvGfxBorders[3] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderDown);

		mvGfxCorners[0] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerLU);
		mvGfxCorners[1] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerRU);
		mvGfxCorners[2] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerRD);
		mvGfxCorners[3] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerLD);
	}

	//-----------------------------------------------------------------------

	void cWidgetFrame::OnInit()
	{
	}

	//-----------------------------------------------------------------------

	void cWidgetFrame::OnChangeSize()
	{
		OhplWdSliderLayout layout;
		ohpl_wd_frame_slider_layout(&mvSize, mpHSlider && mpHSlider->IsVisible(), mpVSlider && mpVSlider->IsVisible(), &layout);

		if(mpHSlider)
		{
			mpHSlider->SetSize(layout.h_size);
			mpHSlider->SetPosition(layout.h_pos);
		}
		if(mpVSlider)
		{
			mpVSlider->SetSize(layout.v_size);
			mpVSlider->SetPosition(layout.v_pos);
		}

		mbScrollBarsNeedUpdate = true;
	}

	//-----------------------------------------------------------------------

	void cWidgetFrame::OnChildUpdate(iWidget* apChild)
	{
		mbScrollBarsNeedUpdate = true;
	}

	//-----------------------------------------------------------------------

	void cWidgetFrame::OnUpdate(float afTimeStep)
	{
		cVector3f vOffset = GetScrollAmount();

		if(mbScrollBarsNeedUpdate && (mpHSlider || mpVSlider) && IsVisible())
		{
			mbScrollBarsNeedUpdate=false;

			std::vector<OhplWdChildBox> vBoxes;
			tWidgetListIt it = mlstChildren.begin();
			for(;it!=mlstChildren.end();++it)
			{
				iWidget* pChild = *it;
				if(pChild==mpHSlider || pChild==mpVSlider)
					continue;

				OhplWdChildBox box;
				box.pos = pChild->GetLocalPosition();
				box.size = pChild->GetSize();
				box.visible = pChild->IsVisible();
				vBoxes.push_back(box);
			}

			OhplWdScrollIn in;
			in.size = mvSize;
			in.margin = mvAutoScrollMargin;
			in.offset = vOffset;
			in.max_coord = mvMaxWidgetCoord;
			in.h_size = mpHSlider ? mpHSlider->GetSize() : cVector2f(0);
			in.v_size = mpVSlider ? mpVSlider->GetSize() : cVector2f(0);
			in.has_h = mpHSlider != NULL;
			in.has_v = mpVSlider != NULL;

			OhplWdScrollPlan plan;
			ohpl_wd_frame_scroll_plan(&in, vBoxes.empty() ? NULL : &vBoxes[0], vBoxes.size(), &plan);
			mvMaxWidgetCoord = plan.max_coord;

			if(mpHSlider)
			{
				mpHSlider->SetMaxValue(plan.h.max_value);
				mpHSlider->SetBarValueSize(plan.h.bar_value_size);
				mpHSlider->SetButtonValueAdd(plan.h.button_add);
				mpHSlider->SetBarClickValueAdd(mpHSlider->GetBarValueSize());
				if(plan.h.set_value)
					mpHSlider->SetValue(plan.h.value);
				mpHSlider->SetEnabled(plan.h.active);
				mpHSlider->SetVisible(plan.h.active);
			}

			if(mpVSlider)
			{
				mpVSlider->SetMaxValue(plan.v.max_value);
				mpVSlider->SetBarValueSize(plan.v.bar_value_size);
				mpVSlider->SetButtonValueAdd(plan.v.button_add);
				mpVSlider->SetBarClickValueAdd(mpVSlider->GetBarValueSize());
				if(plan.v.set_value)
					mpVSlider->SetValue(plan.v.value);
				mpVSlider->SetEnabled(plan.v.active);
				mpVSlider->SetVisible(plan.v.active);
			}
		}

		if(mbScrollUpdated)
		{
			mbScrollUpdated = false;

			if(mpHSlider) vOffset.x = (float)mpHSlider->GetValue();
			if(mpVSlider) vOffset.y = (float)mpVSlider->GetValue();
		}

		SetScrollAmount(vOffset);
	}

	//-----------------------------------------------------------------------

	void cWidgetFrame::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		OhplWdFrameDraw d;
		d.global = GetGlobalPosition();
		d.size = mvSize;
		d.draw_frame = mbDrawFrame;
		d.draw_background = mbDrawBackground;
		d.background_z = mfBackgroundZ;
		d.background_color = mBackGroundColor;

		OhplWdGfxTable table;
		table.SetFrame(NULL, mvGfxBorders, mvGfxCorners);

		OhplWdOp ops[kOhplWdMaxOps];
		size_t lCount = ohpl_wd_frame_draw(ops, kOhplWdMaxOps, &d, table.m);
		OhplWdRun(mpSet, ops, lCount, table, NULL, NULL);
	}

	//-----------------------------------------------------------------------

	void cWidgetFrame::OnDrawAfterClip(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		OhplWdFrameDraw d;
		d.global = GetGlobalPosition();
		d.size = mvSize;
		d.draw_frame = mbDrawFrame;
		d.draw_background = mbDrawBackground;
		d.background_z = mfBackgroundZ;
		d.background_color = mBackGroundColor;

		OhplWdGfxTable table;
		table.Set(0, mpGfxBackground);

		OhplWdOp ops[kOhplWdMaxOps];
		size_t lCount = ohpl_wd_frame_draw_background(ops, kOhplWdMaxOps, &d, table.m);
		OhplWdRun(mpSet, ops, lCount, table, NULL, NULL);
	}

	//-----------------------------------------------------------------------
	
	bool cWidgetFrame::OnMouseMove(const cGuiMessageData& aData)
	{
		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetFrame::OnMouseDown(const cGuiMessageData& aData)
	{
		if(aData.mlVal&eGuiMouseButton_Left ||
		   aData.mlVal&eGuiMouseButton_Right)
			return true;

		if(mpVSlider==NULL || mpVSlider->IsVisible()==false)
			return false;

		int lValue;
		if(ohpl_wd_frame_wheel(aData.mlVal, mpVSlider->GetValue(), mpVSlider->GetButtonValueAdd(), &lValue))
			mpVSlider->SetValue(lValue);

		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetFrame::OnMouseUp(const cGuiMessageData& aData)
	{
		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetFrame::OnMouseEnter(const cGuiMessageData& aData)
	{
		return false;
	}
	
	//-----------------------------------------------------------------------
	
	bool cWidgetFrame::OnMouseLeave(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------


}
