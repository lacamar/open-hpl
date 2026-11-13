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

#include "gui/WidgetTabFrame.h"

#include "gui/GuiTypes.h"

#include "gui/Widget.h"
#include "gui/WidgetFrame.h"
#include "gui/WidgetButton.h"
#include "gui/GuiGfxElement.h"

#include "gui/GuiSet.h"
#include "gui/GuiSkin.h"
#include "gui/RustWidget2.h"

#include <cstring>
#include <vector>

#include "graphics/FontData.h"

#include "math/Math.h"



namespace hpl
{

	//////////////////////////////////////////////////////////////////
	// Tab Label
	//////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------
	//-------------------------------------------------------------------
	//-------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////


	//-------------------------------------------------------------------

	cWidgetTabLabel::cWidgetTabLabel(cGuiSet* apSet, cGuiSkin* apSkin) : iWidget(eWidgetType_Label, apSet, apSkin), mpGfxIcon(mH.icon)
	{
		ohpl_h20_tl_init(&gH20Host, &mH, (uintptr_t)(iWidget*)this, &mB);
		mpGfxIcon = NULL;

		LoadGraphics();
	}

	//-------------------------------------------------------------------

	cWidgetTabLabel::~cWidgetTabLabel()
	{
	}

	//-------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////

	
	bool cWidgetTabLabel::OnMouseDown(const cGuiMessageData& aData)
	{
		return ohpl_h20_tl_on_mouse_down(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

	void cWidgetTabLabel::OnChangeText()
	{
		ohpl_h20_tl_on_change_text(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

	bool cWidgetTabLabel::OnGetUINavFocus(const cGuiMessageData& aData)
	{
		return ohpl_h20_tl_on_get_ui_nav_focus(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

	void cWidgetTabLabel::OnDraw(float afTimeStep, cGuiClipRegion* apClipRegion)
	{
		ohpl_h20_tl_on_draw(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

	void cWidgetTabLabel::OnLoadGraphics()
	{
		// TODO : Add nice gfx here
		
		// Debug
		//mpGfxIcon = mvGfxCorners[0];
	}

	//-------------------------------------------------------------------
	//-------------------------------------------------------------------
	//-------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////
	// Tab
	//////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------
	//-------------------------------------------------------------------
	//-------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////


	cWidgetTab::cWidgetTab(cWidgetTabFrame* apParent) : iWidget(eWidgetType_Tab, apParent->GetSet(), apParent->GetSet()->GetSkin()),
		mlIndex(mH.index), mbOnTop(mH.onTop), mpParentTabFrame(mH.frame), mpTabLabel(mH.label), mpFrame(mH.inner)
	{
		ohpl_h20_tab_init(&gH20Host, &mH, (uintptr_t)(iWidget*)this, (uintptr_t)(iWidget*)apParent, &apParent->mH, &mB);
		LoadGraphics();

		mpTabLabel = NULL;
		mpFrame = NULL;

		mpParentTabFrame = apParent;
		mlIndex = mpParentTabFrame->GetTabNum();
	}

	//-------------------------------------------------------------------

	cWidgetTab::~cWidgetTab()
	{
		if(mpSet->IsDestroyingSet()==false)
			ohpl_h20_tab_destroy(&gH20Host, &mH);
	}



	//-------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------

	void cWidgetTab::AttachChild(iWidget* apChild)
	{
		ohpl_h20_tab_attach_child(&gH20Host, &mH, (uintptr_t)apChild, false);
	}

	//-------------------------------------------------------------------

	void cWidgetTab::RemoveChild(iWidget* apChild)
	{
		ohpl_h20_tab_attach_child(&gH20Host, &mH, (uintptr_t)apChild, true);
	}

	//-------------------------------------------------------------------

	void cWidgetTab::SetLabelPosX(float afPosX)
	{
		cVector3f vNewPos = mpTabLabel->GetLocalPosition();
		vNewPos.x = afPosX;
		mpTabLabel->SetPosition(vNewPos);
	}

	//-------------------------------------------------------------------

	void cWidgetTab::SetIcon(cGuiGfxElement* apIcon)
	{
		ohpl_h20_tab_set_icon(&gH20Host, &mH, (uintptr_t)apIcon);
	}

	//-------------------------------------------------------------------

	void cWidgetTab::SetOnTop(bool abX)
	{
		ohpl_h20_tab_set_on_top(&gH20Host, &mH, abX);
	}

	//-------------------------------------------------------------------

	bool cWidgetTab::PointIsInside(const cVector2f& avPoint, bool abOnlyClipped)
	{
		return ohpl_h20_tab_point_is_inside(&gH20Host, &mH, avPoint.x, avPoint.y, abOnlyClipped);
	}

	//-------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------

	void cWidgetTab::OnChangeText()
	{
		ohpl_h20_tab_on_change_text(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

	void cWidgetTab::OnChangeSize()
	{
		ohpl_h20_tab_on_change_size(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

	void cWidgetTab::OnInit()
	{
		ohpl_h20_tab_on_init(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

	void cWidgetTab::OnDraw(float afTimeStep, cGuiClipRegion* apClipRegion)
	{
		ohpl_h20_tab_on_draw(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

	void cWidgetTab::OnLoadGraphics()
	{
	}

	//-------------------------------------------------------------------
	//-------------------------------------------------------------------
	//-------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	// Tab Frame
	//////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------
	//-------------------------------------------------------------------
	//-------------------------------------------------------------------

	//-------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------

	cWidgetTabFrame::cWidgetTabFrame(cGuiSet* apSet, cGuiSkin* apSkin) : iWidget(eWidgetType_TabFrame, apSet, apSkin),
		mpTopTab(mH.top), mfBackgroundZ(mH.bgZ), mfForegroundZ(mH.fgZ), mState(mH.state), mbHoriScrollEnabled(mH.hori), mbVertScrollEnabled(mH.vert)
	{
		ohpl_h20_tf_init(&gH20Host, &mH, (uintptr_t)(iWidget*)this, &mB);
		mfBackgroundZ = 0;
		mfForegroundZ = 0.9f;

		memset(&mState, 0, sizeof(mState));

		mpTopTab = NULL;

		mbHoriScrollEnabled = true;
		mbVertScrollEnabled = true;

		LoadGraphics();

		SetClipActive(true);
	}

	//-------------------------------------------------------------------

	cWidgetTabFrame::~cWidgetTabFrame()
	{
		if(mpSet->IsDestroyingSet()==false)
		{
			ohpl_h20_tf_destroy(&gH20Host, &mH);
		}
		ohpl_h20_tf_free(&mH);
	}

	//-------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------

	cWidgetTab* cWidgetTabFrame::AddTab(const tWString& asTabCaption)
	{
		return (cWidgetTab*)ohpl_h20_tf_add_tab(&gH20Host, &mH, asTabCaption.data(), asTabCaption.size());
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::ClearTabs()
	{
		ohpl_h20_tf_clear_tabs(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

	int cWidgetTabFrame::GetTabNum()
	{
		return ohpl_h20_tf_tab_num(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

	cWidgetTab* cWidgetTabFrame::GetTab(int alIdx)
	{
		return (cWidgetTab*)ohpl_h20_tf_tab(&gH20Host, &mH, alIdx);
	}

	//-------------------------------------------------------------------

	cWidgetTab* cWidgetTabFrame::GetTab(const tWString& asTabCaption)
	{
		for(int i=0;i<GetTabNum(); ++i)
		{
			cWidgetTab* pTab = GetTab(i);
			if(pTab->GetText()==asTabCaption)
				return pTab;
		}

		return NULL;
	}

	//-------------------------------------------------------------------

	//-------------------------------------------------------------------

	int cWidgetTabFrame::GetTabOnTopIndex()
	{
		return ohpl_h20_tf_tab_on_top_index(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::SetTabOnTop(cWidgetTab* apTab)
	{
		ohpl_h20_tf_set_tab_on_top(&gH20Host, &mH, (uintptr_t)apTab);
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::SetTabOnTopByIndex(int alIdx)
	{
		ohpl_h20_tf_set_tab_on_top_by_index(&gH20Host, &mH, alIdx);
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::SetHorizontalScrollEnabled(bool abX)
	{
		ohpl_h20_tf_set_hori_scroll(&gH20Host, &mH, abX);
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::SetVerticalScrollEnabled(bool abX)
	{
		ohpl_h20_tf_set_vert_scroll(&gH20Host, &mH, abX);
	}

	//-------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------

	bool cWidgetTabFrame::OnUIButtonPress(const cGuiMessageData& aData)
	{
		return ohpl_h20_tf_on_ui_button_press(&gH20Host, &mH, &aData);
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::UpdateTabVisibility()
	{
		ohpl_h20_tf_update_tab_visibility(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::SetScrollButtonsHidden(bool abX)
	{
		ohpl_h20_tf_set_scroll_buttons_hidden(&gH20Host, &mH, abX);
	}
    
	//-------------------------------------------------------------------

	void cWidgetTabFrame::OnInit()
	{
		ohpl_h20_tf_on_init(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::OnChangeSize()
	{
		ohpl_h20_tf_on_change_size(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::OnLoadGraphics()
	{
		ohpl_h20_tf_on_load_graphics(&gH20Host, &mH);
	}

	//-------------------------------------------------------------------

};

