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

#include "gui/WidgetMenuItem.h"

#include "gui/Widget.h"
#include "gui/WidgetMenu.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"

#include "gui/GuiSet.h"
#include "gui/GuiSkin.h"

#include "graphics/FontData.h"
#include "gui/RustWidget3.h"

namespace hpl {
	static int ItemKind(eWidgetType aType)
	{
		return aType==eWidgetType_ContextMenu ? kOhplW3KindContext : (aType==eWidgetType_MainMenu ? kOhplW3KindMain : kOhplW3KindOther);
	}

	//----------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	//	CONSTRUCTORS
	//////////////////////////////////////////////////////////////////

	//----------------------------------------------------------------

	cWidgetMenuItem::cWidgetMenuItem(iWidgetMenu* apParent) : iWidget(eWidgetType_MenuItem, apParent->GetSet(), apParent->GetSet()->GetSkin()),
		mbChecked(mH.checked), mbDefault(mH.def), mfTimer(mH.timer), mfOpenMenuTime(mH.openTime), mpChildMenu(mH.child)
	{
		ohpl_h20_mi_init(&gH20Host, &mH, (uintptr_t)(iWidget*)this, &mB, ItemKind(apParent->GetType()));
		mbChecked = false;
		mItemType = apParent->GetType();

		mpChildMenu	= NULL;

		mfTimer = 0;
		mfOpenMenuTime = 0.5f;

		LoadGraphics();
	}

	cWidgetMenuItem::~cWidgetMenuItem()
	{
		if(mpSet->IsDestroyingSet()==false)
		{
			ClearMenuItems();
		}
	}

	//----------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	//	PUBLIC METHODS
	//////////////////////////////////////////////////////////////////

	//----------------------------------------------------------------

	cWidgetMenuItem*	cWidgetMenuItem::AddMenuItem( const tWString &asText, const tWString &asIconFilename )
	{
		//////////////////////////////////////////
		// Add a menu item. Create submenu if needed.
		if(mpChildMenu == NULL)
		{
			mpChildMenu = (iWidgetMenu*) mpSet->CreateWidgetContextMenu(0,0,_W(""), GetParentMenu());
			//mpChildMenu->SetEnabled(false);
			mpChildMenu->SetVisible(false);
			mpChildMenu->SetParentItem(this);
		}
		cWidgetMenuItem* pMenuItem = mpChildMenu->AddMenuItem(asText);

		return pMenuItem;
	}

	//----------------------------------------------------------------

	void cWidgetMenuItem::AddSeparator()
	{
		AddMenuItem(_W(""));
	}

	//----------------------------------------------------------------

	void cWidgetMenuItem::ClearMenuItems()
	{
		if(mpChildMenu)
		{
			mpSet->DestroyWidget(mpChildMenu);
			mpChildMenu=NULL;
		}
	}

	//----------------------------------------------------------------

	cGuiGlobalShortcut* cWidgetMenuItem::AddShortcut(int alKeyModifiers, eKey aKey, eGuiMessage aMsg, 
									  bool abBypassVisibility, bool abBypassEnabled)
	{
		cGuiGlobalShortcut* pShortcut = iWidget::AddShortcut(alKeyModifiers, aKey, eGuiMessage_ButtonPressed, true, false);

		tWString sAdd = cString::To16Char(mvShortcuts.back()->ToString());
		tWString sCur = msShortcutText;
		size_t lLen = ohpl_w3_join_shortcut(sCur.data(), sCur.size(), sAdd.data(), sAdd.size(), NULL, 0);
		msShortcutText.assign(lLen, _W('\0'));
		if(lLen) ohpl_w3_join_shortcut(sCur.data(), sCur.size(), sAdd.data(), sAdd.size(), &msShortcutText[0], lLen);

		GetParentMenu()->UpdateMenuWidth(GetTextLength());

		return pShortcut;
	}

	//----------------------------------------------------------------

	float cWidgetMenuItem::GetTextLength()
	{
		const cVector2f& vFontSize = mpDefaultFont->mvSize;
		return mpDefaultFontType->GetLength(vFontSize, msText.c_str()) + 
			mpDefaultFontType->GetLength(vFontSize, msShortcutText.c_str());
	}

	//----------------------------------------------------------------

	//////////////////////////////////////////////////////////////////
	//	PROTECTED METHODS
	//////////////////////////////////////////////////////////////////

	//----------------------------------------------------------------

	bool cWidgetMenuItem::IsMenuOpen()
	{
		if(IsParent())
			return (mpChildMenu->IsVisible());

		return false;
	}

	//----------------------------------------------------------------

	void cWidgetMenuItem::OnInit()
	{
		ohpl_h20_mi_on_init(&gH20Host, &mH);
	}

	//----------------------------------------------------------------

	void cWidgetMenuItem::OnUpdate(float afTimeStep)
	{
		ohpl_h20_mi_on_update(&gH20Host, &mH, afTimeStep);
	}

	//----------------------------------------------------------------

	void cWidgetMenuItem::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		ohpl_h20_mi_on_draw(&gH20Host, &mH);
	}

	//----------------------------------------------------------------

	void cWidgetMenuItem::OnChangeSize()
	{
		/*
		SetArrowPos(cVector3f(mvSize.x - (mfItemHPadding + mfItemArrowIconSize),
												 mfItemArrowIconSize*0.5f,
												 0.2f));

		// Set text and check icon offsets, these should not change
		SetTextPos(cVector3f(	mfItemHPadding + mfItemCheckIconSize + mfItemTextLeftPadding,
								mfItemVPadding, 
								0.2f));
		SetCheckPos(cVector3f(	mfItemHPadding,
								mfItemVPadding,
								0.2f));
								*/
	}

	//----------------------------------------------------------------

	bool cWidgetMenuItem::OnMouseDown(const cGuiMessageData& aData)
	{
		return ohpl_h20_mi_on_mouse_down(&gH20Host, &mH);
	}

	//----------------------------------------------------------------

	bool cWidgetMenuItem::OnMouseUp(const cGuiMessageData& aData)
	{
		return ohpl_h20_mi_on_mouse_up(&gH20Host, &mH, &aData);
	}

	//----------------------------------------------------------------

	bool cWidgetMenuItem::OnMouseEnter(const cGuiMessageData& aData)
	{
		//mfTimer = 0;

		return true;
	}

	//----------------------------------------------------------------

	bool cWidgetMenuItem::OnMouseMove(const cGuiMessageData& aData)
	{
		return ohpl_h20_mi_on_mouse_move(&gH20Host, &mH);
	}

	//----------------------------------------------------------------

	bool cWidgetMenuItem::OnMouseLeave(const cGuiMessageData& aData)
	{
		return ohpl_h20_mi_on_mouse_leave(&gH20Host, &mH);
	}

	//----------------------------------------------------------------
	
	bool cWidgetMenuItem::OnKeyPress(const cGuiMessageData& aData)
	{
		return ohpl_h20_mi_on_key_press(&gH20Host, &mH, &aData);
	}

	//----------------------------------------------------------------

	void cWidgetMenuItem::OnLoadGraphics()
	{
		ohpl_h20_mi_on_load_graphics(&gH20Host, &mH);
	}

	//----------------------------------------------------------------

	bool cWidgetMenuItem::OnLostFocus(const cGuiMessageData& aData)
	{
		////////////////////////////////////////
		// If loses focus, try to hide topmost menu
		//GetParentMenu()->GetTopMostMenu()->SetMustHide(true);
		return true;
	}

	//----------------------------------------------------------------

	bool cWidgetMenuItem::OnGotFocus(const cGuiMessageData& aData)
	{
		////////////////////////////////////////
		// If gets focus, keep topmost menu up
		//GetParentMenu()->GetTopMostMenu()->SetMustHide(false);
		return true;
	}

	//----------------------------------------------------------------

};
