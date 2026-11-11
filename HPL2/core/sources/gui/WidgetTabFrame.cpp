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

	cWidgetTabLabel::cWidgetTabLabel(cGuiSet* apSet, cGuiSkin* apSkin) : iWidget(eWidgetType_Label, apSet, apSkin)
	{
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
		cWidgetTabFrame* pFrame = mpParentTab->mpParentTabFrame;
		
		// The C++ loops forever (or reads past the tab vector) when the label cannot be scrolled into view
		for(int i=0; i<=(int)pFrame->mvTabs.size() && mvPosition.x + mvSize.x > pFrame->mvSize.x; ++i)
		{
			int lFirst = pFrame->mState.first_visible;
			pFrame->ArrowButton_Pressed(pFrame->mvArrowButtons[1], cGuiMessageData());
			if(pFrame->mState.first_visible == lFirst) break;
		}

		mpParentTab->GetParentTabFrame()->SetTabOnTop(mpParentTab);

		return true;
	}

	//-------------------------------------------------------------------

	void cWidgetTabLabel::OnChangeText()
	{
		cWidgetTabFrame* pTabFrame = mpParentTab->mpParentTabFrame;
		float fBordersWidth = pTabFrame->mvGfxTabLabelBorders[0]->GetImageSize().x + 
								pTabFrame->mvGfxTabLabelBorders[1]->GetImageSize().x;
		float fLength = mpDefaultFontType->GetLength(mpDefaultFont->mvSize, msText.c_str());
		float fNewWidth = ohpl_w2_tab_label_width(&pTabFrame->mState, fLength, fBordersWidth, mpGfxIcon!=NULL,
												  mpGfxIcon!=NULL ? mpGfxIcon->GetImageSize().x : 0);

		SetSize(cVector2f(fNewWidth,mvSize.y));
	}

	//-------------------------------------------------------------------

	bool cWidgetTabLabel::OnGetUINavFocus(const cGuiMessageData& aData)
	{
		mpParentTab->GetParentTabFrame()->SetTabOnTop(this->mpParentTab);

		return true;
	}

	//-------------------------------------------------------------------

	void cWidgetTabLabel::OnDraw(float afTimeStep, cGuiClipRegion* apClipRegion)
	{
		cWidgetTabFrame* pTabFrame = mpParentTab->mpParentTabFrame;
		cVector3f vGlobal = GetGlobalPosition();
		cVector3f vTextPos;
		ohpl_w2_tab_label_text_pos(&pTabFrame->mState, &vGlobal, mpGfxIcon!=NULL,
								   mpGfxIcon!=NULL ? mpGfxIcon->GetImageSize().x : 0, &vTextPos);
		
		cGuiGfxElement* pBG;

		if(mpParentTab->IsOnTop())
		{
			pBG = pTabFrame->mpGfxTabLabelBGActive;
		}
		else
		{
			pBG = pTabFrame->mpGfxTabLabelBG;
		}

		
		DrawDefaultText(msText, vTextPos, eFontAlign_Left);
		DrawBordersAndCorners(pBG, 
								pTabFrame->mvGfxTabLabelBorders, 
								pTabFrame->mvGfxTabLabelCorners, GetGlobalPosition(), mvSize);
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


	cWidgetTab::cWidgetTab(cWidgetTabFrame* apParent) : iWidget(eWidgetType_Tab, apParent->GetSet(), apParent->GetSet()->GetSkin())
	{
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
		{
			mpSet->DestroyWidget(mpTabLabel);
			mpSet->DestroyWidget(mpFrame);
		}
	}



	//-------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------

	void cWidgetTab::AttachChild(iWidget* apChild)
	{
		if(apChild==mpTabLabel || apChild==mpFrame)
			iWidget::AttachChild(apChild);
		else
            mpFrame->AttachChild(apChild);
	}

	//-------------------------------------------------------------------

	void cWidgetTab::RemoveChild(iWidget* apChild)
	{
		if(apChild==mpTabLabel || apChild==mpFrame)
			iWidget::RemoveChild(apChild);
		else
			mpFrame->RemoveChild(apChild);
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
		mpTabLabel->mpGfxIcon = apIcon;
		mpTabLabel->OnChangeText();
	}

	//-------------------------------------------------------------------

	void cWidgetTab::SetOnTop(bool abX)
	{
		mbOnTop = abX;
		cVector3f vPos = mpTabLabel->GetLocalPosition();
		vPos.y = ohpl_w2_tab_label_pos_y(&mpParentTabFrame->mState, abX);
		
		mpTabLabel->SetPosition(vPos);
		mpFrame->SetEnabled(abX);
		mpFrame->SetVisible(abX);
	}

	//-------------------------------------------------------------------

	bool cWidgetTab::PointIsInside(const cVector2f& avPoint, bool abOnlyClipped)
	{
		if(CheckPointInsideClippingParent(avPoint)==false)
			return false;

		if(abOnlyClipped && mbClipsGraphics==false) return true;

		bool bLabelVisible = mpTabLabel->IsVisible();
		cVector3f vLabelGlobalPos = mpTabLabel->GetGlobalPosition();
		cVector2f vLabelSize = mpTabLabel->GetSize();

		cVector3f vFrameGlobalPos = mpFrame->GetGlobalPosition();
		cVector2f vFrameSize = mpFrame->GetSize();

		return ohpl_w2_tab_point_inside(&avPoint, bLabelVisible, &vLabelGlobalPos, &vLabelSize, &vFrameGlobalPos, &vFrameSize);
	}

	//-------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------

	void cWidgetTab::OnChangeText()
	{
		if(mpTabLabel!=NULL) mpTabLabel->SetText(msText);
	}

	//-------------------------------------------------------------------

	void cWidgetTab::OnChangeSize()
	{
		if(mpTabLabel == NULL || mpFrame == NULL) return;

		OhplW2TabLayout l;
		ohpl_w2_tab_layout(&mvSize, mpDefaultFont->mvSize.y, mpParentTabFrame->mState.active_offset, &l);

		mpTabLabel->SetPosition(l.label_pos);
		
		mpFrame->SetPosition(l.frame_pos);
		mpFrame->SetSize(l.frame_size);
	}

	//-------------------------------------------------------------------

	void cWidgetTab::OnInit()
	{
		cVector2f vCorners[3] = {mpParentTabFrame->mvGfxTabCorners[0]->GetActiveSize(),
								 mpParentTabFrame->mvGfxTabCorners[1]->GetActiveSize(),
								 mpParentTabFrame->mvGfxTabCorners[2]->GetActiveSize()};
		OhplW2TabInit init;
		ohpl_w2_tab_init(&mvSize, mpDefaultFont->mvSize.y, mpParentTabFrame->mState.active_offset, vCorners, &init);
		float fLabelHeight = init.label_h;
		mpTabLabel = mpSet->CreateWidgetTabLabel(0,
												 cVector2f(0, fLabelHeight),
												 msText,
												 NULL);
		mpTabLabel->mpParentTab = this;

		mpFrame = mpSet->CreateWidgetFrame(0,
											init.frame_size,
											false,
											NULL,
											mpParentTabFrame->mbHoriScrollEnabled, mpParentTabFrame->mbVertScrollEnabled);
		mpFrame->SetClipActive(true);

		AttachChild(mpTabLabel);
		AttachChild(mpFrame);
		mpTabLabel->SetPosition(cVector3f(0, 0, 0.1f));

		mpFrame->SetPosition(init.frame_pos);

		SetOnTop(false);
	}

	//-------------------------------------------------------------------

	void cWidgetTab::OnDraw(float afTimeStep, cGuiClipRegion* apClipRegion)
	{
		DrawBordersAndCorners(mpParentTabFrame->mpGfxTabBG, 
								mpParentTabFrame->mvGfxTabBorders, 
								mpParentTabFrame->mvGfxTabCorners, 
								mpFrame->GetGlobalPosition(), mpFrame->GetSize(), true);
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

	cWidgetTabFrame::cWidgetTabFrame(cGuiSet* apSet, cGuiSkin* apSkin) : iWidget(eWidgetType_TabFrame, apSet, apSkin)
	{
		mfBackgroundZ = 0;
		mfForegroundZ = 0.9f;

		memset(&mState, 0, sizeof(mState));

		mpTopTab = NULL;

		mvArrowButtons[0] =NULL;
		mvArrowButtons[1] =NULL;

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
			for(int i=0;i<2;++i)
				mpSet->DestroyWidget(mvArrowButtons[i]);
			ClearTabs();
		}
	}

	//-------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------

	cWidgetTab* cWidgetTabFrame::AddTab(const tWString& asTabCaption)
	{
		////////////////////////////////////////////////////////////////////
		// Create new tab, if first tab created, set as topmost
		// If not, check if fits in frame, and show scroll buttons if needed.
		cWidgetTab* pTab = mpSet->CreateWidgetTab(mvPosition,mvSize,_W(""), this);

		pTab->SetText(asTabCaption);
		pTab->SetPosition(cVector3f(0,0,mfBackgroundZ));
				
		if(mvTabs.empty())
			SetTabOnTop(pTab);
		
		mvTabs.push_back(pTab);

		UpdateTabVisibility();

		return pTab;
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::ClearTabs()
	{
		for(tWidgetTabVectorIt it = mvTabs.begin(); it != mvTabs.end(); ++it)
			mpSet->DestroyWidget(*it);

		mvTabs.clear();
	}

	//-------------------------------------------------------------------

	int cWidgetTabFrame::GetTabNum()
	{
		return (int)mvTabs.size();
	}

	//-------------------------------------------------------------------

	cWidgetTab* cWidgetTabFrame::GetTab(int alIdx)
	{
		return mvTabs[alIdx];
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
		int lIndex = -1;
		for(int i=0;i<(int)mvTabs.size();++i)
		{
			if(mpTopTab==mvTabs[i])
			{
				lIndex = i;
				break;
			}
		}

		return lIndex;
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::SetTabOnTop(cWidgetTab* apTab)
	{
		/////////////////////////////////////////////
		// Move current top tab to the background, 
		// and then move apTab to top
		if(apTab == mpTopTab) return;

		cVector3f vNewPos;

		if(mpTopTab!=NULL)
		{
			vNewPos = mpTopTab->GetLocalPosition();
			vNewPos.z = mfBackgroundZ;
			mpTopTab->SetPosition(vNewPos);
			mpTopTab->SetOnTop(false);
		}

		mpTopTab = apTab;

		vNewPos = mpTopTab->GetLocalPosition();
		vNewPos.z = mfForegroundZ;
		mpTopTab->SetPosition(vNewPos);
		mpTopTab->SetOnTop(true);

		if(mvTabs.empty()==false)
			ProcessMessage(eGuiMessage_SelectionChange, cGuiMessageData(mpTopTab->GetIndex()));
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::SetTabOnTopByIndex(int alIdx)
	{
		if(alIdx<0 || alIdx>=(int)mvTabs.size())
			return;

		SetTabOnTop(mvTabs[alIdx]);
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::SetHorizontalScrollEnabled(bool abX)
	{
		if(mvTabs.empty())
			mbHoriScrollEnabled = abX;
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::SetVerticalScrollEnabled(bool abX)
	{
		if(mvTabs.empty())
			mbVertScrollEnabled = abX;
	}

	//-------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------

	bool cWidgetTabFrame::OnUIButtonPress(const cGuiMessageData& aData)
	{
		int lTabOnTopIndex = GetTabOnTopIndex();
		int lTarget;

		if(ohpl_w2_tab_page_target(aData.mlVal, lTabOnTopIndex, &lTarget)==false)
			return false;

		SetTabOnTopByIndex(lTarget);

		return true;
	}

	//-------------------------------------------------------------------

	bool cWidgetTabFrame::ArrowButton_Pressed(iWidget* apWidget, const cGuiMessageData& aData)
	{
		if(apWidget==mvArrowButtons[0])
			ohpl_w2_tab_arrow(&mState, false, (int)mvTabs.size());
		if(apWidget==mvArrowButtons[1])
			ohpl_w2_tab_arrow(&mState, true, (int)mvTabs.size());
		
		UpdateTabVisibility();
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetTabFrame, ArrowButton_Pressed);

	//-------------------------------------------------------------------

	void cWidgetTabFrame::UpdateTabVisibility()
	{
		size_t lNum = mvTabs.size();
		std::vector<float> vWidths(lNum);
		std::vector<OhplW2TabItem> vItems(lNum);
		for(size_t i=0; i<lNum; ++i)
			vWidths[i] = mvTabs[i]->mpTabLabel->GetSize().x;

		OhplW2TabArrows arrows;
		ohpl_w2_tab_visibility(&mState, mvSize.x, vWidths.data(), vItems.data(), lNum, &arrows);

		///////////////////////////////
		// Hide tabs at the left of the first visible
		for(size_t i=0; i<lNum; ++i)
		{
			if(vItems[i].visible) continue;
			if((int)i >= mState.first_visible) continue;

			mvTabs[i]->mpTabLabel->SetVisible(false);
			mvTabs[i]->mpTabLabel->SetEnabled(false);
		}

		////////////////////////////////
		// Show the rest and place them
		for(size_t i=0; i<lNum; ++i)
		{
			if(vItems[i].visible==false) continue;

			cWidgetTab* pTab = mvTabs[i];
			pTab->mpTabLabel->SetVisible(true);
			pTab->mpTabLabel->SetEnabled(true);
			pTab->SetLabelPosX(vItems[i].pos_x);
		}

		////////////////////////////////
		// Update arrow buttons
		mvArrowButtons[0]->SetEnabled(arrows.left_enabled);
		mvArrowButtons[1]->SetEnabled(arrows.right_enabled);

		mvArrowButtons[0]->SetVisible(arrows.shown);
		mvArrowButtons[1]->SetVisible(arrows.shown);
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::SetScrollButtonsHidden(bool abX)
	{
		for(int i=0; i<2; ++i)
		{
			cWidgetButton* pArrow = mvArrowButtons[i];

			pArrow->SetEnabled(!abX);
			pArrow->SetVisible(!abX);
		}

	}
    
	//-------------------------------------------------------------------

	void cWidgetTabFrame::OnInit()
	{
		////////////////////////////////
		// Init Scroll buttons

		for(int i = 0; i < 2; ++i)
		{
			mvArrowButtons[i] = mpSet->CreateWidgetButton(0,0,_W(""),this);
			mvArrowButtons[i]->SetImage(mvGfxArrows[i],false);
			mvArrowButtons[i]->SetSize(mState.button_size);
			mvArrowButtons[i]->AddCallback(eGuiMessage_ButtonPressed,this,kGuiCallback(ArrowButton_Pressed));	
		}

		OnChangeSize();
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::OnChangeSize()
	{
		if(mvArrowButtons[0]==NULL || mvArrowButtons[1]==NULL) return;
		for(int i = 0; i<2; ++i)
		{
			cVector3f vPos;
			ohpl_w2_tab_arrow_pos(&mState, mvSize.x, i, mfForegroundZ, &vPos);
			mvArrowButtons[i]->SetPosition(vPos);
		}

		for(int i=0; i<(int)mvTabs.size(); ++i)
			mvTabs[i]->SetSize(mvSize);

		UpdateTabVisibility();
	}

	//-------------------------------------------------------------------

	void cWidgetTabFrame::OnLoadGraphics()
	{
		//////////////////////////////////////////////
		// Attributes
		mState.button_size = mpSkin->GetAttribute(eGuiSkinAttribute_TabFrameButtonSize).x;
		mState.first_tab_offset = mpSkin->GetAttribute(eGuiSkinAttribute_FirstTabOffset).x;
		mState.tab_separation = mpSkin->GetAttribute(eGuiSkinAttribute_TabSeparation).x;

		mState.inactive_offset = mpSkin->GetAttribute(eGuiSkinAttribute_TabFrameLabelInactiveOffset).x;
		mState.active_offset = mpSkin->GetAttribute(eGuiSkinAttribute_TabFrameLabelActiveOffset).x;

		mState.icon_padding = mpSkin->GetAttribute(eGuiSkinAttribute_TabFrameIconPadding).x;
		mState.text_left_padding = mpSkin->GetAttribute(eGuiSkinAttribute_TabFrameTextRightPadding).x;
		mState.text_right_padding = mpSkin->GetAttribute(eGuiSkinAttribute_TabFrameTextLeftPadding).x;

		//////////////////////////////////////////////
		// Tab scroll buttons gfx
		mvGfxArrows[0] = mpSkin->GetGfx(eGuiSkinGfx_TabFrameArrowLeft);
		mvGfxArrows[1] = mpSkin->GetGfx(eGuiSkinGfx_TabFrameArrowRight);

		//////////////////////////////////////////////
		// Tab label gfx
		mvGfxTabLabelBorders[0] = mpSkin->GetGfx(eGuiSkinGfx_TabLabelBorderRight);
		mvGfxTabLabelBorders[1] = mpSkin->GetGfx(eGuiSkinGfx_TabLabelBorderLeft);
		mvGfxTabLabelBorders[2] = mpSkin->GetGfx(eGuiSkinGfx_TabLabelBorderUp);
		mvGfxTabLabelBorders[3] = mpSkin->GetGfx(eGuiSkinGfx_TabLabelBorderDown);

		mvGfxTabLabelCorners[0] = mpSkin->GetGfx(eGuiSkinGfx_TabLabelCornerLU);
		mvGfxTabLabelCorners[1] = mpSkin->GetGfx(eGuiSkinGfx_TabLabelCornerRU);
		mvGfxTabLabelCorners[2] = mpSkin->GetGfx(eGuiSkinGfx_TabLabelCornerRD);
		mvGfxTabLabelCorners[3] = mpSkin->GetGfx(eGuiSkinGfx_TabLabelCornerLD);
		
		mpGfxTabLabelBG = mpSkin->GetGfx(eGuiSkinGfx_TabLabelBackground);
		mpGfxTabLabelBGActive = mpSkin->GetGfx(eGuiSkinGfx_TabLabelBackgroundActive);
		
		//////////////////////////////////////////////
		// Tab Frame gfx
		mvGfxTabBorders[0] = mpSkin->GetGfx(eGuiSkinGfx_TabBorderRight);
		mvGfxTabBorders[1] = mpSkin->GetGfx(eGuiSkinGfx_TabBorderLeft);
		mvGfxTabBorders[2] = mpSkin->GetGfx(eGuiSkinGfx_TabBorderUp);
		mvGfxTabBorders[3] = mpSkin->GetGfx(eGuiSkinGfx_TabBorderDown);

		mvGfxTabCorners[0] = mpSkin->GetGfx(eGuiSkinGfx_TabCornerLU);
		mvGfxTabCorners[1] = mpSkin->GetGfx(eGuiSkinGfx_TabCornerRU);
		mvGfxTabCorners[2] = mpSkin->GetGfx(eGuiSkinGfx_TabCornerRD);
		mvGfxTabCorners[3] = mpSkin->GetGfx(eGuiSkinGfx_TabCornerLD);
		
		mpGfxTabBG = mpSkin->GetGfx(eGuiSkinGfx_TabBackground);
	}

	//-------------------------------------------------------------------

};

