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

#include "gui/WidgetMainMenu.h"
#include "gui/GuiSet.h"

#include "gui/WidgetMenuItem.h"

#include "graphics/FontData.h"
#include "gui/RustWidget3.h"
#include "impl/RustBackends.h"

#include "system/LowLevelSystem.h"



namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	cWidgetMainMenu::cWidgetMainMenu(cGuiSet *apSet, cGuiSkin *apSkin) : iWidgetMenu(eWidgetType_MainMenu,apSet,apSkin)
	{
		ohpl_h20_mm_init(&gH20Host, &mH, (uintptr_t)(iWidget*)this, &mB);
		LoadGraphics();
	}

	cWidgetMainMenu::~cWidgetMainMenu()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetMainMenu::UpdateMenuItemsPos(cWidgetMenuItem* apNewItem)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_mm_update_items_pos(&gH20Host, &mH, (uintptr_t)(iWidget*)apNewItem);
			return;
		}

		size_t lNumItems = mvMenuItems.size();

		/////////////////////////////////////
		// If not the first item added, get previous one lentgh and pos,
		// and set it as new item's position.
		cVector3f vPos;
		bool bHasPrev = lNumItems > 1;
		float fPrevX = 0, fPrevW = 0;
		if(bHasPrev)
		{
			fPrevX = mvMenuItems[lNumItems-2]->GetLocalPosition().x;
			fPrevW = mvMenuItems[lNumItems-2]->GetSize().x;
		}
		ohpl_w3_main_item_pos(mfItemVPadding, bHasPrev, fPrevX, fPrevW, mfItemSeparation, &vPos);

		apNewItem->SetPosition(vPos);
	}

	//-----------------------------------------------------------------------

	void cWidgetMainMenu::UpdateMenuItemsSize(cWidgetMenuItem* apNewItem)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_mm_update_items_size(&gH20Host, &mH, (uintptr_t)(iWidget*)apNewItem);
			return;
		}

		//////////////////////////////////////
		// Set item text length as items size 
		float fLength = apNewItem->GetTextLength();

		OhplW3MainItemSize size;
		ohpl_w3_main_item_size(fLength, mfItemTextPadding, mpDefaultFont->mvSize.y, mfItemVPadding, &size);
        apNewItem->SetSize(size.size);

		// Also set text offset
		SetTextPos(size.text_pos);
	}

	//-----------------------------------------------------------------------

	void cWidgetMainMenu::OnChildMenuHide()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_mm_on_child_menu_hide(&gH20Host, &mH);
			return;
		}

		mpSet->SetAttentionWidget(mpPrevAttention);
	}
    
	//-----------------------------------------------------------------------

	void cWidgetMainMenu::Hide()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_mm_hide(&gH20Host, &mH);
			return;
		}

		///////////////////////////////////////////////////
		// In MainMenu, topmost menu should never be hidden, so hide submenues only
		if(IsSubmenuOpen())
		{
			mpHighlightedItem->GetChildMenu()->Hide();
			mpHighlightedItem = NULL;
		}

		mbMustHide = false;
	}

	//-----------------------------------------------------------------------

	void cWidgetMainMenu::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_mm_on_draw(&gH20Host, &mH);
			return;
		}

		////////////////////////////////
		// Draw background 
		mpSet->DrawGfx( mpGfxBackground,GetGlobalPosition(),mvSize-cVector2f(0,1));
		//mpSet->DrawGfx( mpGfxBackground, GetGlobalPosition()+cVector3f(0,mvSize.y-1,0.05f), cVector2f(mvSize.x,1), cColor(0.5f,1));

	}

	//-----------------------------------------------------------------------

	void cWidgetMainMenu::OnUpdate(float afTimeStep)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_mm_on_update(&gH20Host, &mH);
			return;
		}

		if(mbMustHide)
		{
			Hide();
			mbMustHide = false;
		}
	}

	//-----------------------------------------------------------------------

	void cWidgetMainMenu::OnInit()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_mm_on_init(&gH20Host, &mH);
			return;
		}

		/////////////////////////////////////////////////////////////////////
		// If has parent, use parent width. If not, use screen width (temp)

		eWidgetType type = mpParent->GetType();
		int lParentKind = type==eWidgetType_Root ? kOhplW3ParentRoot :
						  (type==eWidgetType_Window ? kOhplW3ParentWindow : kOhplW3ParentOther);
		OhplW3MainInit init;
		ohpl_w3_main_init(lParentKind, mpSet->GetVirtualSize().x, mpParent->GetSize().x, mpDefaultFont->mvSize.y, &init);

		SetSize(init.size);

		mpParent->SetChildrenOffset(init.children_offset);
		SetPosition(GetLocalPosition()+init.pos_delta);
	}

	//-----------------------------------------------------------------------

	void cWidgetMainMenu::OnLoadGraphics()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_mm_on_load_graphics(&gH20Host, &mH);
			return;
		}

		mpGfxBackground = mpSkin->GetGfx(eGuiSkinGfx_ButtonUpBackground);

		mfItemTextPadding = mpSkin->GetAttribute(eGuiSkinAttribute_MainMenuItemTextPadding).x;
		mfItemVPadding = mpSkin->GetAttribute(eGuiSkinAttribute_MainMenuItemVPadding).x;
		mfItemSeparation = mpSkin->GetAttribute(eGuiSkinAttribute_MainMenuItemSeparation).x;
	}

	//-----------------------------------------------------------------------

};
