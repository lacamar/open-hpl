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
#include "impl/RustBackends.h"

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

	void cWidgetMenuItem::ShowSubMenu()
	{
		if(mpChildMenu==NULL) return;

		cVector3f vParentGlobal = GetParentMenu()->GetGlobalPosition();
		cVector3f vChildPos;
		ohpl_w3_item_submenu_pos(ItemKind(mItemType), &vParentGlobal, &mvPosition, mvSize.y,
								 GetParentMenu()->GetSize().x, mpChildMenu->GetSize().x,
								 mpSet->GetVirtualSize().x, &vChildPos);

		//mpChildMenu->SetGlobalPosition(vChildPos);
		//mpChildMenu->SetEnabled(true);
		//mpChildMenu->SetVisible(true);
		mpSet->ShowContextMenu((cWidgetContextMenu*)mpChildMenu,vChildPos);		
	}
	//----------------------------------------------------------------

	void cWidgetMenuItem::OnInit()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_mi_on_init(&gH20Host, &mH);
			return;
		}

		cVector2f vSize;
		ohpl_w3_item_init_size(IsSeparator(), mfSeparatorHeight, mfItemVPadding, mpDefaultFont->mvSize.y,
							   GetParentMenu()->GetMenuItemWidth(), &vSize);
		SetSize(vSize);
	}

	//----------------------------------------------------------------

	void cWidgetMenuItem::OnUpdate(float afTimeStep)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_mi_on_update(&gH20Host, &mH, afTimeStep);
			return;
		}

		if(IsVisible() == false || (mpSet->GetAttentionWidget()!=NULL && IsConnectedTo(mpSet->GetAttentionWidget())==false))
			return;

		OhplW3ItemUpdate update = ohpl_w3_item_update(mfTimer, afTimeStep, mfOpenMenuTime, GetMouseIsOver(),
													  ItemKind(mItemType),
													  GetParentMenu()->GetHightlightedItem()==this, IsParent(),
													  IsMenuOpen());
		mfTimer = update.timer;
		if(update.show_submenu)
			ShowSubMenu();
	}

	//----------------------------------------------------------------

	void cWidgetMenuItem::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_mi_on_draw(&gH20Host, &mH);
			return;
		}

		OhplW3ItemDrawIn in;
		in.kind = ItemKind(mItemType);
		in.separator = IsSeparator();
		in.highlighted = (GetParentMenu()->GetHightlightedItem()==this);
		in.enabled = IsEnabled();
		in.widget_enabled = mbEnabled;
		in.is_parent = IsParent();
		in.checked = mbChecked;
		in.global = GetGlobalPosition();
		in.size = mvSize;
		in.separator_padding = mfSeparatorPadding;
		in.separator_height = mfSeparatorHeight;
		in.arrow_offset = GetParentMenu()->GetArrowOffset();
		in.check_offset = GetParentMenu()->GetCheckOffset();
		in.text_offset = GetParentMenu()->GetTextOffset();

		OhplW3MenuOp vOps[kOhplW3MaxItemOps];
		size_t lOps = ohpl_w3_item_draw(&in, vOps, kOhplW3MaxItemOps);
		cGuiGfxElement* vGfx[5] = { mpGfxLine, mpGfxSubMenuArrow, mpGfxChecked[0], mpGfxChecked[1], mpGfxSelection };
		for(size_t i=0; i<lOps && i<kOhplW3MaxItemOps; ++i)
		{
			const OhplW3MenuOp& op = vOps[i];
			if(op.kind==kOhplW3OpGfx)
			{
				if(op.has_size)
					mpSet->DrawGfx(vGfx[op.slot], op.pos, op.size);
				else
					mpSet->DrawGfx(vGfx[op.slot], op.pos);
			}
			else
			{
				cColor col = op.color==kOhplW3ColorDisabled ? mpSkin->GetFont(eGuiSkinFont_Disabled)->mColor :
							 (op.color==kOhplW3ColorHighlight ? mDefaultFontHighlightColor : mDefaultFontColor);
				DrawDefaultText(op.slot==kOhplW3TextShortcut ? msShortcutText : msText, op.pos, (eFontAlign)op.align, col);
			}
		}
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
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_mi_on_mouse_down(&gH20Host, &mH);
		}

		if(IsSeparator()) return false;
		///////////////////////////
		// If clicked, skip timer
		mfTimer = mfOpenMenuTime;

		return true;
	}

	//----------------------------------------------------------------

	bool cWidgetMenuItem::OnMouseUp(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_mi_on_mouse_up(&gH20Host, &mH, &aData);
		}

		OhplW3ItemUp up = ohpl_w3_item_mouse_up(IsSeparator(), GetParentMenu()->GetHightlightedItem()==this, IsParent());
		if(up.ret==false) return false;

		if(up.hide_topmost)
			GetParentMenu()->GetTopMostMenu()->Hide();

		ProcessMessage(eGuiMessage_ButtonPressed, aData, true, true);

		return true;
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
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_mi_on_mouse_move(&gH20Host, &mH);
		}

		bool bAttentionBlocks = mpSet->GetAttentionWidget()!=NULL && IsConnectedTo(mpSet->GetAttentionWidget())==false;
		if(ohpl_w3_item_mouse_move_highlights(IsVisible(), mpSet->GetDrawMouse(), bAttentionBlocks, GetMouseIsOver()))
			GetParentMenu()->SetHighlightedItem(this);
		return true;
	}

	//----------------------------------------------------------------

	bool cWidgetMenuItem::OnMouseLeave(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_mi_on_mouse_leave(&gH20Host, &mH);
		}

		bool bAttentionBlocks = mpSet->GetAttentionWidget()!=NULL && IsConnectedTo(mpSet->GetAttentionWidget())==false;
		OhplW3ItemLeave leave = ohpl_w3_item_mouse_leave(bAttentionBlocks, IsMenuOpen());
		if(leave.ret==false) return false;

		if(leave.clear_highlight)
			GetParentMenu()->SetHighlightedItem(NULL);

		if(leave.reset_timer)
			mfTimer = 0;

		return true;
	}

	//----------------------------------------------------------------
	
	bool cWidgetMenuItem::OnKeyPress(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_mi_on_key_press(&gH20Host, &mH, &aData);
		}

		eKey key = aData.mKeyPress.mKey;

		if(IsParent())
			return mpChildMenu->ProcessMessage(eGuiMessage_KeyPress, aData);

		return false;
	}

	//----------------------------------------------------------------

	void cWidgetMenuItem::OnLoadGraphics()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_mi_on_load_graphics(&gH20Host, &mH);
			return;
		}

		mpGfxSelection = mpSkin->GetGfx(eGuiSkinGfx_TextBoxSelectedTextBack);
		mpGfxSubMenuArrow = mpSkin->GetGfx(eGuiSkinGfx_MenuItemArrow);

		mpGfxChecked[0] = mpSkin->GetGfx(eGuiSkinGfx_MenuDisabledChecked); 
		mpGfxChecked[1] = mpSkin->GetGfx(eGuiSkinGfx_MenuEnabledChecked);

		mpGfxLine = mpSkin->GetGfx(eGuiSkinGfx_MenuSeparatorLine);

		mfSeparatorPadding = mpSkin->GetAttribute(eGuiSkinAttribute_MenuSeparatorPadding).x;
		mfSeparatorHeight = mpSkin->GetAttribute(eGuiSkinAttribute_MenuSeparatorHeight).x;
		mfItemHPadding = mpSkin->GetAttribute(eGuiSkinAttribute_ContextMenuItemHPadding).x;
		mfItemVPadding = mpSkin->GetAttribute(eGuiSkinAttribute_ContextMenuItemVPadding).x;
		mfItemSeparation = mpSkin->GetAttribute(eGuiSkinAttribute_ContextMenuItemSeparation).x;
		mfItemArrowIconSize = mpSkin->GetAttribute(eGuiSkinAttribute_ContextMenuItemArrowSize).x;
		mfItemCheckIconSize = mpSkin->GetAttribute(eGuiSkinAttribute_ContextMenuItemCheckSize).x;
		mfItemTextLeftPadding = mpSkin->GetAttribute(eGuiSkinAttribute_ContextMenuItemTextLeftPadding).x;
		mfItemTextRightPadding = mpSkin->GetAttribute(eGuiSkinAttribute_ContextMenuItemTextRightPadding).x;

		SetDefaultFontSize(12.5f);
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
