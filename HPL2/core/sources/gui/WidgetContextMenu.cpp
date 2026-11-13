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
		ohpl_h20_cm_hide(&gH20Host, &mH);
	}


	//-----------------------------------------------------------------------

	void cWidgetContextMenu::SavePrevAttentionWidget()
	{
		mpPrevAttention = mpSet->GetAttentionWidget();
	}

	//-----------------------------------------------------------------------

	float cWidgetContextMenu::GetMenuItemWidth()
	{
		return ohpl_h20_cm_item_width(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetContextMenu::UpdateMenuItemsPos(cWidgetMenuItem* apNewItem)
	{
		ohpl_h20_cm_update_items_pos(&gH20Host, &mH, (uintptr_t)(iWidget*)apNewItem);
	}

	//-----------------------------------------------------------------------

	void cWidgetContextMenu::UpdateMenuItemsSize(cWidgetMenuItem* apNewItem)
	{
		ohpl_h20_cm_update_items_size(&gH20Host, &mH, (uintptr_t)(iWidget*)apNewItem);
	}

	//-----------------------------------------------------------------------

	void cWidgetContextMenu::UpdateMenuWidth(float afItemTextWidth)
	{
		ohpl_h20_cm_update_width(&gH20Host, &mH, afItemTextWidth);
	}

	//-----------------------------------------------------------------------

	void cWidgetContextMenu::OnChildMenuHide()
	{
		mpSet->SetFocusedWidget(this);
	}

	//-----------------------------------------------------------------------
    
	void cWidgetContextMenu::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		ohpl_h20_cm_on_draw(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetContextMenu::OnUpdate(float afTimeStep)
	{
		ohpl_h20_cm_on_update(&gH20Host, &mH, afTimeStep);
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
		return ohpl_h20_cm_on_lost_focus(&gH20Host, &mH);
	}

	bool cWidgetContextMenu::OnGotFocus(const cGuiMessageData& aData)
	{
		return ohpl_h20_cm_on_got_focus(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void	cWidgetContextMenu::OnLoadGraphics()
	{
		ohpl_h20_cm_on_load_graphics(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	bool cWidgetContextMenu::OnKeyPress(const cGuiMessageData& aData)
	{
		return ohpl_h20_cm_on_key_press(&gH20Host, &mH, &aData);
	}

	//-----------------------------------------------------------------------

};
