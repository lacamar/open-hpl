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

#include "gui/GuiPopUpUIKeyboard.h"

#include "gui/GuiTypes.h"

#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "system/String.h"

#include "math/Math.h"

#include "graphics/FontData.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"

#include "gui/WidgetTextBox.h"
#include "gui/WidgetWindow.h"
#include "gui/WidgetButton.h"
#include "gui/WidgetFrame.h"
#include "gui/WidgetSlider.h"
#include "gui/WidgetLabel.h"

#include "gui/RustWidget3.h"

namespace hpl {

	/////////////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	/////////////////////////////////////////////////////////////////////////////////



	//-------------------------------------------------------------------------------

	cGuiPopUpUIKeyboard::cGuiPopUpUIKeyboard(cWidgetTextBox* apTarget, 
												void *apCallbackObject, tGuiCallbackFunc apCallback) : iGuiPopUp(apTarget->GetSet(), true, 0)
	{
		//mvPos = avPos;
		//mpCallbackObject = apCallbackObject;
		//mpCallback = apCallback;

		mpTargetTextBox = apTarget;
		mbShift = false;

		if(OhplBackendRustRequested())
		{
			OhplH21Host host = H21Host();
			mR = ohpl_h21_kb_new(&host, (uintptr_t)apTarget);
			mpWindow = (cWidgetWindow*)ohpl_h21_window(mR);
			return;
		}

		msBackUpText = mpTargetTextBox->GetText();

		mpTargetTextBox->OnUIKeyboardOpen();

		mbShift = false;

		SetNextFocusWidget(static_cast<iWidget*>(apTarget->GetUserData()));
		
		Init();
	}

    //-------------------------------------------------------------------------------

	cGuiPopUpUIKeyboard::~cGuiPopUpUIKeyboard()
	{
		if(mR)
		{
			OhplH21Host host = H21Host();
			ohpl_h21_destroy(&host, mR);
			mR = NULL;
			return;
		}
		mpTargetTextBox->OnUIKeyboardClose();
	}

	//-------------------------------------------------------------------------------

	/////////////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	/////////////////////////////////////////////////////////////////////////////////


	//-------------------------------------------------------------------------------

	/////////////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	/////////////////////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------------------

	void cGuiPopUpUIKeyboard::SetUpKey(iWidget* apKeyWidget, eKey aKey, int alUnicode, int alUnicodeUpper)
	{
		tGuiCallbackFunc pCallback;

		if(aKey==eKey_Return)
		{
			pCallback = kGuiCallback(Enter_OnPress);
		}
		else if(aKey==eKey_None)
		{
			pCallback = kGuiCallback(Cancel_OnPress);
		}
		else
		{
			pCallback = kGuiCallback(Key_OnPress);

			mlstKeys.push_back( cUIKey(aKey, alUnicode, alUnicodeUpper));
			apKeyWidget->SetUserData(&mlstKeys.back());
		}

		apKeyWidget->AddCallback(eGuiMessage_ButtonPressed, this, pCallback);

		mlstKeyWidgets.push_back(apKeyWidget);
	}

	//-------------------------------------------------------------------------------

	bool cGuiPopUpUIKeyboard::Key_OnPress(iWidget* apWidget, const cGuiMessageData &aData)
	{
		cUIKey* pKey = static_cast<cUIKey*>(apWidget->GetUserData());

		cKeyPress key(pKey->mKey, pKey->mvUnicode[mbShift], 0);
		
		return mpTargetTextBox->ProcessMessage(eGuiMessage_KeyPress, cGuiMessageData(key), true, true);
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpUIKeyboard, Key_OnPress);

	//-------------------------------------------------------------------------------

	bool cGuiPopUpUIKeyboard::Enter_OnPress(iWidget* apWidget, const cGuiMessageData &aData)
	{
		SelfDestruct();
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpUIKeyboard, Enter_OnPress);

	//-------------------------------------------------------------------------------

	bool cGuiPopUpUIKeyboard::Cancel_OnPress(iWidget* apWidget, const cGuiMessageData &aData)
	{
		mpTargetTextBox->SetText(msBackUpText);
		
		SelfDestruct();

		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpUIKeyboard, Cancel_OnPress);

	//-------------------------------------------------------------------------------

	bool cGuiPopUpUIKeyboard::Button_Pressed(iWidget* apWidget, const cGuiMessageData &aData)
	{
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpUIKeyboard, Button_Pressed);

	//-------------------------------------------------------------------------------

	bool cGuiPopUpUIKeyboard::WindowClose(iWidget* apWidget, const cGuiMessageData& aData)
	{
		SelfDestruct();
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpUIKeyboard, WindowClose);


	//-------------------------------------------------------------------------------

	bool cGuiPopUpUIKeyboard::Window_OnUIButtonPress(iWidget* apWidget, const cGuiMessageData& aData)
	{
		switch(ohpl_w3_kb_ui_press(aData.mlVal))
		{
		case kOhplW3KbCancelAct:
			return Cancel_OnPress(apWidget, aData);
		case kOhplW3KbDelete:
			return mpTargetTextBox->ProcessMessage(eGuiMessage_KeyPress, cGuiMessageData(cKeyPress(eKey_BackSpace, 0, 0)));
		case kOhplW3KbClear:
			mpTargetTextBox->SetText(_W(""));
			return true;
		case kOhplW3KbShiftOn:
			SetShiftActive(true);
			return true;
		default:
			break;
		}

		return false;
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpUIKeyboard, Window_OnUIButtonPress);


	//-------------------------------------------------------------------------------

	bool cGuiPopUpUIKeyboard::Window_OnUIButtonRelease(iWidget* apWidget, const cGuiMessageData& aData)
	{
		if(ohpl_w3_kb_ui_release(aData.mlVal))
		{
			SetShiftActive(false);
			return true;
		}

		return false;
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpUIKeyboard, Window_OnUIButtonRelease);


	//-------------------------------------------------------------------------------


	/** Builds the window and sets up stuff
	 *
	 */
	void cGuiPopUpUIKeyboard::Init()
	{
		/////////////////////////////////
		// Init widgets

		// Main window
		mpWindow->SetText(_W(""));
		mpWindow->SetStatic(true);
		mpWindow->SetDrawLabel(false);
		mpWindow->SetGlobalUIInputListener(true);
		mpWindow->AddCallback(eGuiMessage_UIButtonPress, this, kGuiCallback(Window_OnUIButtonPress));
		mpWindow->AddCallback(eGuiMessage_UIButtonRelease, this, kGuiCallback(Window_OnUIButtonRelease));

		OhplW3KbKey vKeys[kOhplW3KbKeys];
		size_t lKeys = ohpl_w3_kb_keys(vKeys, kOhplW3KbKeys);
		for(size_t i=0; i<lKeys && i<kOhplW3KbKeys; ++i)
		{
			const OhplW3KbKey& k = vKeys[i];
			eKey key;
			tWString sText;
			switch(k.kind)
			{
			case kOhplW3KbDigit:
				key = (eKey)(eKey_0 + k.offset);
				sText = tWString(1, (wchar_t)k.lower);
				break;
			case kOhplW3KbLetter:
				key = (eKey)(eKey_A + k.offset);
				sText = tWString(1, (wchar_t)k.lower);
				break;
			case kOhplW3KbEnter:
				key = eKey_Return;
				sText = _W("Enter");
				break;
			case kOhplW3KbCancel:
				key = eKey_None;
				sText = _W("Cancel");
				break;
			default:
				key = eKey_Space;
				sText = _W("Space");
				break;
			}
			cWidgetButton* pKey = mpSet->CreateWidgetButton(0, k.size, sText, mpWindow);
			SetUpKey(pKey, key, k.lower, k.upper);
		}

		////////////////////////////////////////
		// Now arrange the keys in a nice grid
		std::vector<iWidget*> vWidgets(mlstKeyWidgets.begin(), mlstKeyWidgets.end());
		size_t lNum = vWidgets.size();
		std::vector<cVector2f> vSizes(lNum);
		for(size_t i=0; i<lNum; ++i)
			vSizes[i] = vWidgets[i]->GetSize();
		std::vector<cVector3f> vPositions(lNum);
		std::vector<int32_t> vRows(lNum);
		OhplW3KbLayout layout;
		ohpl_w3_kb_layout(vSizes.data(), lNum, vPositions.data(), vRows.data(), &layout);
		for(size_t i=0; i<lNum; ++i)
			vWidgets[i]->SetPosition(vPositions[i]);

		//////////////////////////////////////////////////////////
		// Position window right below the textbox
		cVector2f vWindowSize = layout.window_size;

		mpWindow->SetSize(vWindowSize);
		cVector3f vGlobalPos = mpTargetTextBox->GetGlobalPosition();
		cVector2f vTargetSize = mpTargetTextBox->GetSize();

		// This 8 is a magic number since I cannot access the activesize for the window border without hacking a bit.
		ohpl_w3_kb_window_pos(&vGlobalPos, &vTargetSize, vWindowSize.x, &vGlobalPos);
		mpWindow->SetGlobalPosition(vGlobalPos);

		mpSet->PositionWidgetInsideBounds(mpWindow);

		/////////////////////////////////////////////////////////////
		// Set up focus navigation
		std::vector<size_t> vRowLen(layout.rows, 0);
		for(size_t i=0; i<lNum; ++i)
			if((size_t)vRows[i] < vRowLen.size()) ++vRowLen[vRows[i]];
		std::vector<OhplW3KbNav> vNav(lNum);
		ohpl_w3_kb_nav(vRowLen.data(), vRowLen.size(), vNav.data(), lNum);
		for(size_t i=0; i<lNum; ++i)
		{
			const OhplW3KbNav& n = vNav[i];
			vWidgets[i]->SetFocusNavigation(eUIArrow_Up, n.up>=0 ? vWidgets[n.up] : NULL);
			vWidgets[i]->SetFocusNavigation(eUIArrow_Right, n.right>=0 ? vWidgets[n.right] : NULL);
			vWidgets[i]->SetFocusNavigation(eUIArrow_Down, n.down>=0 ? vWidgets[n.down] : NULL);
			vWidgets[i]->SetFocusNavigation(eUIArrow_Left, n.left>=0 ? vWidgets[n.left] : NULL);
		}
		mpSet->SetDefaultFocusNavWidget(vWidgets[0]);
		mpSet->SetFocusedWidget(vWidgets[0]);
	}

	//-------------------------------------------------------------------------------
	
	bool cGuiPopUpUIKeyboard::H21Ext(const OhplH21Cmd &c, OhplH21Ret &r)
	{
		if(c.op == kOhplH21TbKbOpen) { static_cast<cWidgetTextBox*>((iWidget*)c.w[0])->OnUIKeyboardOpen(); return true; }
		if(c.op == kOhplH21TbKbClose) { static_cast<cWidgetTextBox*>((iWidget*)c.w[0])->OnUIKeyboardClose(); return true; }
		return false;
	}

	//-----------------------------------------------------------------------

	void cGuiPopUpUIKeyboard::ClosePopUp()
	{
		SelfDestruct();
	}

	//-------------------------------------------------------------------------------

	void cGuiPopUpUIKeyboard::SetShiftActive(bool abX)
	{
		if(mbShift==abX) return;

		mbShift = abX;
		tWidgetListIt it = mlstKeyWidgets.begin();
		for(;it!=mlstKeyWidgets.end(); ++it)
		{
			iWidget* pKeyWidget = *it;
			cUIKey* pKey = static_cast<cUIKey*>(pKeyWidget->GetUserData());

			if(pKey) pKeyWidget->SetText(tWString(1, pKey->mvUnicode[mbShift]));
		}
	}

	//-------------------------------------------------------------------------------

};




