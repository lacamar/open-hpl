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

#include "gui/WidgetContextMenu.h"
#include "gui/GuiSet.h"

#include "gui/WidgetMenuItem.h"
#include "gui/GuiGfxElement.h"

#include "graphics/FontData.h"
#include "gui/RustWidget3.h"
#include "impl/RustBackends.h"

#include "system/LowLevelSystem.h"



namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	cWidgetContextMenu::cWidgetContextMenu(cGuiSet *apSet, cGuiSkin *apSkin) : iWidgetMenu(eWidgetType_ContextMenu,apSet,apSkin),
		mfTimer(mH.timer), mfHideTime(mH.hideTime)
	{
		ohpl_h20_cm_init(&gH20Host, &mH, (uintptr_t)(iWidget*)this, &mB);
		mfTimer = 0;
		mfHideTime = 0;
	}

	cWidgetContextMenu::~cWidgetContextMenu()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetContextMenu::AddSeparator()
	{
		AddMenuItem(_W(""));
	}

	//-----------------------------------------------------------------------

	void cWidgetContextMenu::Hide()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_cm_hide(&gH20Host, &mH);
			return;
		}

		//////////////////////////////////////////
		// This does the following:
		// - Closes any submenu that is being shown at the moment
		// - Resets the selection
		// - If this is the topmost menu, clear the AttentionWidget
		// - And hides the menu

		OhplW3CtxHide plan = ohpl_w3_ctx_hide_plan(IsSubmenuOpen(), GetParentMenu()!=NULL);
		if(plan.hide_child)
			mpHighlightedItem->GetChildMenu()->Hide();

        SetHighlightedItem(NULL);

		if(plan.notify_parent)
			GetParentMenu()->OnChildMenuHide();
		else
		{
			mpSet->SetAttentionWidget(mpPrevAttention);
			mpSet->DecContextMenuZ();
		}

		//SetEnabled(false);
		SetVisible(false);

		// Reset Timer
		mfTimer = 0;
	}


	//-----------------------------------------------------------------------

	void cWidgetContextMenu::SavePrevAttentionWidget()
	{
		mpPrevAttention = mpSet->GetAttentionWidget();
	}

	//-----------------------------------------------------------------------

	float cWidgetContextMenu::GetMenuItemWidth()
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_cm_item_width(&gH20Host, &mH);
		}

		return ohpl_w3_ctx_item_width(mvSize.x, mvGfxBorders[0]->GetImageSize().x, mvGfxBorders[1]->GetImageSize().x);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetContextMenu::UpdateMenuItemsPos(cWidgetMenuItem* apNewItem)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_cm_update_items_pos(&gH20Host, &mH, (uintptr_t)(iWidget*)apNewItem);
			return;
		}

		cVector3f vPos;
		ohpl_w3_ctx_item_pos(mvSize.y, mvGfxBorders[2]->GetImageSize().y, mvGfxBorders[0]->GetImageSize().x,
							 mvMenuItems.size(), &vPos);

		apNewItem->SetPosition(vPos);
	}

	//-----------------------------------------------------------------------

	void cWidgetContextMenu::UpdateMenuItemsSize(cWidgetMenuItem* apNewItem)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_cm_update_items_size(&gH20Host, &mH, (uintptr_t)(iWidget*)apNewItem);
			return;
		}

		float fTextLength = apNewItem->GetTextLength();

		UpdateMenuWidth(fTextLength);

		float fSeparation = 0;
		size_t lNumItems = mvMenuItems.size();

		if(lNumItems>1)
		{
			cWidgetMenuItem *pPreviousItem = mvMenuItems[lNumItems-2];
			fSeparation = ohpl_w3_ctx_separation(pPreviousItem->IsSeparator(), apNewItem->IsSeparator(), mfItemSeparation);
		}
		else
		{
			cVector2f vEmpty;
			ohpl_w3_ctx_empty_size(mvSize.x, mvGfxBorders[2]->GetImageSize().y, mvGfxBorders[3]->GetImageSize().y, &vEmpty);
			SetSize(vEmpty);
		}

		cVector2f vGrown;
		ohpl_w3_ctx_grown_size(&mvSize, apNewItem->GetSize().y, fSeparation, &vGrown);
		SetSize(vGrown);
	}

	//-----------------------------------------------------------------------

	void cWidgetContextMenu::UpdateMenuWidth(float afItemTextWidth)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_cm_update_width(&gH20Host, &mH, afItemTextWidth);
			return;
		}

		OhplW3CtxMetrics met = { mfItemHPadding, mfItemCheckIconSize, mfItemTextLeftPadding, mfItemTextRightPadding,
								 mfItemArrowIconSize, mfItemVPadding,
								 mvGfxBorders[0]->GetImageSize().x, mvGfxBorders[1]->GetImageSize().x };
		OhplW3CtxWidth width;
		ohpl_w3_ctx_update_width(&met, afItemTextWidth, GetMenuItemWidth(), mvSize.y, &width);

		if(width.resize_items)
		{
			for(tWidgetMenuItemVectorIt it = mvMenuItems.begin(); it != mvMenuItems.end(); ++it)
			{
				cWidgetMenuItem* pItem = *it;

				pItem->SetSize(cVector2f(width.item_width, pItem->GetSize().y) );
			}
		}

		SetSize(width.menu_size);

		SetArrowPos(width.arrow_pos);

		SetTextPos(width.text_pos);
		SetCheckPos(width.check_pos);
	}

	//-----------------------------------------------------------------------

	void cWidgetContextMenu::OnChildMenuHide()
	{
		mpSet->SetFocusedWidget(this);
	}

	//-----------------------------------------------------------------------
    
	void cWidgetContextMenu::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_cm_on_draw(&gH20Host, &mH);
			return;
		}

		////////////////////////////////
		// Background and borders
		DrawBordersAndCorners(	 mpGfxBackground, mvGfxBorders, mvGfxCorners, 
								GetGlobalPosition(),
								mvSize);

	}

	//-----------------------------------------------------------------------

	void cWidgetContextMenu::OnUpdate(float afTimeStep)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_cm_on_update(&gH20Host, &mH, afTimeStep);
			return;
		}

		////////////////////////////////////////////////
		// Checks if should hide (ie if parent menu highlights some item other 
		// than this menu's parent item, or if SetMustHide(true) has been called)

		OhplW3CtxUpdate update = ohpl_w3_ctx_update(mfTimer, afTimeStep, mfHideTime, IsVisible(),
													mpParentItem!=NULL && GetParentMenu()->GetHightlightedItem()!=mpParentItem);
		mfTimer = update.timer;
		if(update.hide_timer)
		{
			Hide();
		}

		if(mbMustHide)
		{
			Hide();
			mbMustHide=false;
		}
	}

	//-----------------------------------------------------------------------

	void cWidgetContextMenu::OnInit()
	{
		SetVisible(false);
		//SetEnabled(false);
	}

	//-----------------------------------------------------------------------

	bool cWidgetContextMenu::OnMouseEnter(const cGuiMessageData& aData)
	{
     	return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetContextMenu::OnMouseLeave(const cGuiMessageData& aData)
	{
		if(IsSubmenuOpen()==false)
			SetHighlightedItem(NULL);
        return true;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	// These should work as follows (needs Oldwidget->OnLostFocus, NewWidget->OnGotFocus order to work):
	// - If a Context menu or submenu loses focus, try to hide topmost (would hide all submenus)
	// - If next focused widget belongs to the Context menu, keep it enabled ( by revoking last OnLostFocus action )
	// - If next focused widget doesnt belong to the menu, this will be closed

	bool cWidgetContextMenu::OnLostFocus(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_cm_on_lost_focus(&gH20Host, &mH);
		}

		iWidget* pFocus = mpSet->GetFocusedWidget();
		bool bConnectedToFocus = pFocus ? IsConnectedTo(pFocus, false) : false;
		bool bFocusConnectedToMenu = pFocus ? pFocus->IsConnectedTo(this, false) : false;
		if(ohpl_w3_ctx_lost_focus_hides(pFocus==NULL, bConnectedToFocus, bFocusConnectedToMenu))
			GetTopMostMenu()->SetMustHide(true);

		return true;
	}

	bool cWidgetContextMenu::OnGotFocus(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_cm_on_got_focus(&gH20Host, &mH);
		}

		mfTimer=0;

		return true;
	}

	//-----------------------------------------------------------------------

	void	cWidgetContextMenu::OnLoadGraphics()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_cm_on_load_graphics(&gH20Host, &mH);
			return;
		}

		mpGfxBackground = mpSkin->GetGfx(eGuiSkinGfx_ButtonUpBackground);

		mvGfxBorders[0] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxBorderRight);
		mvGfxBorders[1] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxBorderLeft);
		mvGfxBorders[2] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxBorderUp);
		mvGfxBorders[3] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxBorderDown);

		mvGfxCorners[0] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxCornerLU);
		mvGfxCorners[1] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxCornerRU);
		mvGfxCorners[2] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxCornerRD);
		mvGfxCorners[3] = mpSkin->GetGfx(eGuiSkinGfx_ComboBoxCornerLD);

		mfItemHPadding = mpSkin->GetAttribute(eGuiSkinAttribute_ContextMenuItemHPadding).x;
		mfItemVPadding = mpSkin->GetAttribute(eGuiSkinAttribute_ContextMenuItemVPadding).x;
		mfItemSeparation = mpSkin->GetAttribute(eGuiSkinAttribute_ContextMenuItemSeparation).x;
		mfItemArrowIconSize = mpSkin->GetAttribute(eGuiSkinAttribute_ContextMenuItemArrowSize).x;
		mfItemCheckIconSize = mpSkin->GetAttribute(eGuiSkinAttribute_ContextMenuItemCheckSize).x;
		mfItemTextLeftPadding = mpSkin->GetAttribute(eGuiSkinAttribute_ContextMenuItemTextLeftPadding).x;
		mfItemTextRightPadding = mpSkin->GetAttribute(eGuiSkinAttribute_ContextMenuItemTextRightPadding).x;
	}

	//-----------------------------------------------------------------------

	bool cWidgetContextMenu::OnKeyPress(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_cm_on_key_press(&gH20Host, &mH, &aData);
		}

		if(ohpl_w3_ctx_key_hides(aData.mKeyPress.mKey))
		{
			Hide();
			return true;
		}

		for(int i=0;i<(int)mvMenuItems.size();++i)
		{
			if(mvMenuItems[i]->ProcessMessage(eGuiMessage_KeyPress, aData))
				break;
		}
		return true;
	}

	//-----------------------------------------------------------------------

};
