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

#include "gui/GuiPopUpMessageBox.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include "graphics/FontData.h"
#include "gui/RustWidget3.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"

#include "gui/WidgetButton.h"
#include "gui/WidgetLabel.h"
#include "gui/WidgetWindow.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cGuiPopUpMessageBox::cGuiPopUpMessageBox(cGuiSet *apSet,
											const tWString& asLabel, const tWString& asText,
											const tWString& asButton1, const tWString& asButton2,
											void *apCallbackObject, tGuiCallbackFunc apCallback) 
						: iGuiPopUp(apSet, false, 0)
	{
		//////////////////////////
		// Set up variables
		mpCallback = apCallback;
		mpCallbackObject = apCallbackObject;
		mvButtons[0] = mvButtons[1] = NULL;
		mpLabel = NULL;

		if(OhplBackendRustRequested())
		{
			OhplH21Host host = H21Host();
			mR = ohpl_h21_msgbox_new(&host, reinterpret_cast<const uint32_t*>(asLabel.data()), asLabel.size(), reinterpret_cast<const uint32_t*>(asText.data()), asText.size(),
									 reinterpret_cast<const uint32_t*>(asButton1.data()), asButton1.size(), reinterpret_cast<const uint32_t*>(asButton2.data()), asButton2.size(),
									 (uintptr_t)apCallbackObject, reinterpret_cast<uintptr_t>(apCallback));
			mpWindow = (cWidgetWindow*)ohpl_h21_window(mR);
			return;
		}

		cGuiSkinFont *pFont = mpSkin->GetFont(eGuiSkinFont_Default);
		
		float fLabelLength = pFont->mpFont->GetLength(pFont->mvSize,asLabel.c_str());
		float fTextLength = pFont->mpFont->GetLength(pFont->mvSize,asText.c_str());

		cVector2f vVirtSize = mpSet->GetVirtualSize();

		OhplW3MsgBoxLayout layout;
		ohpl_w3_msgbox_layout(fLabelLength, fTextLength, pFont->mvSize.y, &vVirtSize, asButton2 != _W(""), &layout);
		float fWindowWidth = layout.window_size.x;
		cVector3f vPos = layout.window_pos;

		//////////////////////////
		// Window
		mpWindow->SetText(asLabel);
		mpWindow->SetPosition(vPos);
		mpWindow->SetSize(layout.window_size);
		
		//////////////////////////
		// Buttons
		if(asButton2 == _W(""))
		{
			vPos = layout.button0_pos;
			mvButtons[0] = mpSet->CreateWidgetButton(vPos,cVector2f(80,30),asButton1,mpWindow);
			mvButtons[0]->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(ButtonPress));
			mvButtons[0]->AddCallback(eGuiMessage_UIButtonPress,this, kGuiCallback(GamepadButtonPress));
			mvButtons[0]->SetGlobalUIInputListener(true);

			mvButtons[1] = NULL;
		}
		else
		{
			vPos = layout.button0_pos;
			mvButtons[0] = mpSet->CreateWidgetButton(vPos,cVector2f(80,30),asButton1,mpWindow);
			mvButtons[0]->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(ButtonPress));
			mvButtons[0]->AddCallback(eGuiMessage_UIButtonPress,this, kGuiCallback(GamepadButtonPress));
			mvButtons[0]->SetGlobalUIInputListener(true);

			vPos = layout.button1_pos;
			mvButtons[1] = mpSet->CreateWidgetButton(vPos,cVector2f(80,30),asButton2,mpWindow);
			mvButtons[1]->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(ButtonPress));
			mvButtons[1]->AddCallback(eGuiMessage_UIButtonPress,this, kGuiCallback(GamepadButtonPress));
			mvButtons[1]->SetGlobalUIInputListener(true);
			
			mvButtons[0]->SetFocusNavigation(eUIArrow_Right, mvButtons[1]);
			mvButtons[1]->SetFocusNavigation(eUIArrow_Left, mvButtons[0]);

		}

		SetUpDefaultFocus(mvButtons[0]);
		
		//////////////////////////
		// Label
		vPos = layout.label_pos;
		mpLabel = mpSet->CreateWidgetLabel(vPos,layout.label_size,
											asText,mpWindow);

		SetUpDefaultFocus(mvButtons[0]);
	}

	//-----------------------------------------------------------------------

	cGuiPopUpMessageBox::~cGuiPopUpMessageBox()
	{
		if(mR)
		{
			OhplH21Host host = H21Host();
			ohpl_h21_destroy(&host, mR);
			mR = NULL;
			return;
		}
		if(mvButtons[0]) mpSet->DestroyWidget(mvButtons[0]);
		if(mvButtons[1]) mpSet->DestroyWidget(mvButtons[1]);
		if(mpLabel) mpSet->DestroyWidget(mpLabel);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	bool cGuiPopUpMessageBox::ButtonPress(iWidget* apWidget,const cGuiMessageData& aData)
	{
		int lButton = apWidget == mvButtons[0] ? 0 : 1;

		RunCallback(mpCallbackObject, mpCallback, apWidget, cGuiMessageData(lButton), true);

		SelfDestruct();
		
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpMessageBox,ButtonPress)

	
	//-----------------------------------------------------------------------

	bool cGuiPopUpMessageBox::GamepadButtonPress(iWidget* apWidget,const cGuiMessageData& aData)
	{
		bool bSecond;
		if(ohpl_w3_msgbox_gamepad(aData.mlVal, mvButtons[1]!=NULL, &bSecond)==false) return false;

		if(bSecond) apWidget = mvButtons[1];

		return ButtonPress(apWidget, aData);
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpMessageBox,GamepadButtonPress)

	
	//-----------------------------------------------------------------------


}
