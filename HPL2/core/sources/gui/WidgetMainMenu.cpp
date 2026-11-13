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
		ohpl_h20_mm_update_items_pos(&gH20Host, &mH, (uintptr_t)(iWidget*)apNewItem);
	}

	//-----------------------------------------------------------------------

	void cWidgetMainMenu::UpdateMenuItemsSize(cWidgetMenuItem* apNewItem)
	{
		ohpl_h20_mm_update_items_size(&gH20Host, &mH, (uintptr_t)(iWidget*)apNewItem);
	}

	//-----------------------------------------------------------------------

	void cWidgetMainMenu::OnChildMenuHide()
	{
		ohpl_h20_mm_on_child_menu_hide(&gH20Host, &mH);
	}
    
	//-----------------------------------------------------------------------

	void cWidgetMainMenu::Hide()
	{
		ohpl_h20_mm_hide(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetMainMenu::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		ohpl_h20_mm_on_draw(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetMainMenu::OnUpdate(float afTimeStep)
	{
		ohpl_h20_mm_on_update(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetMainMenu::OnInit()
	{
		ohpl_h20_mm_on_init(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

	void cWidgetMainMenu::OnLoadGraphics()
	{
		ohpl_h20_mm_on_load_graphics(&gH20Host, &mH);
	}

	//-----------------------------------------------------------------------

};
