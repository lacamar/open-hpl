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

#include "gui/GuiPopUp.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"

#include "gui/WidgetWindow.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iGuiPopUp::iGuiPopUp(cGuiSet *apSet, bool abAddCloseButton, const cVector2f& avPopUpSize)
	{
		mpSet = apSet;
		mpSkin = mpSet->GetSkin();
		mR = NULL;
		mbDestroyQueued = false;
		mpWindow = NULL;
		mpDestroyCallbackObject = NULL;
		mpDestroyCallback = NULL;
		if(OhplBackendRustRequested()) return;

		mpSet->mlPopupCount++;

		cVector2f vVirtualSize = mpSet->GetVirtualSize();
		cVector3f vPos;
		ohpl_w3_popup_pos(&vVirtualSize, &avPopUpSize, mpSet->GetPopUpZ(), &vPos);
		mpSet->IncPopUpZ();

		mpWindow = mpSet->CreateWidgetWindow(abAddCloseButton?eWidgetWindowButtonFlag_ButtonClose:eWidgetWindowButtonFlag_None,
											 vPos, avPopUpSize, _W(""), NULL);
		mpWindow->SetCloseButtonDisablesWindow(false);
		mpWindow->AddCallback(eGuiMessage_WindowClose, this, kGuiCallback(Window_OnClose));

		////////////////////////
		// Set up Attention and focus

		ohpl_w3_popup_init((size_t)mpSet->GetAttentionWidget(), (size_t)mpSet->GetFocusedWidget(),
						   (size_t)mpSet->GetDefaultFocusNavWidget(), &mPopUp);

		mpSet->PushAttentionWidget();
		mpSet->PushFocusedWidget();
		mpSet->PushDefaultFocusNavWidget();
				
		mpSet->SetAttentionWidget(mpWindow);

		SetKillOnEscapeKey(true);

		mpDestroyCallbackObject = NULL;
		mpDestroyCallback = NULL;
	}

	//-----------------------------------------------------------------------

	iGuiPopUp::~iGuiPopUp()
	{
		if(OhplBackendRustRequested()) return;

		mpSet->mlPopupCount--;

		mpSet->PopAttentionWidget();
		mpSet->PopFocusedWidget();
		mpSet->PopDefaultFocusNavWidget();
		
		if(mPopUp.att_changed)
			mpSet->SetAttentionWidget((iWidget*)mPopUp.new_attention);
		if(mPopUp.focus_changed)
			mpSet->SetFocusedWidget((iWidget*)mPopUp.new_focus);
		if(mPopUp.default_focus_changed)
			mpSet->SetDefaultFocusNavWidget((iWidget*)mPopUp.new_default_focus);
		
		if(mpDestroyCallbackObject && mpDestroyCallback) {
			mpDestroyCallback(mpDestroyCallbackObject, NULL, cGuiMessageData());
		}

		if(mpWindow) mpSet->DestroyWidget(mpWindow, true);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void iGuiPopUp::AddOnDestroyCallback(void *apCallbackObject, tGuiCallbackFunc apCallback)
	{
		mpDestroyCallbackObject = apCallbackObject;
		mpDestroyCallback = apCallback;
		if(mR) ohpl_h21_set_destroy_callback(mR, (uintptr_t)apCallbackObject, reinterpret_cast<uintptr_t>(apCallback));
	}

	void iGuiPopUp::SetKillOnEscapeKey(bool abX)
	{
		if(mR)
		{
			OhplH21Host host = H21Host();
			ohpl_h21_set_kill_on_escape(&host, mR, abX);
			return;
		}
		mpWindow->SetEscapeKeyClosesWindow(abX);
	}

	bool iGuiPopUp::GetKillOnEscapeKey()
	{
		if(mR)
		{
			OhplH21Host host = H21Host();
			return ohpl_h21_kill_on_escape(&host, mR);
		}
		return mpWindow->GetEscapeKeyClosesWindow();
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool iGuiPopUp::Window_OnClose(iWidget* apWidget, const cGuiMessageData& aData)
	{
		OnCloseSpecific();
		SelfDestruct();

		return true;
	}
	kGuiCallbackDeclaredFuncEnd(iGuiPopUp, Window_OnClose);

	//-----------------------------------------------------------------------
	
	void iGuiPopUp::SelfDestruct()
	{
		if(mbDestroyQueued) return;
		mbDestroyQueued = true;
		mpSet->DestroyPopUp(this);
	}

	//-----------------------------------------------------------------------

	bool iGuiPopUp::RunCallback(void* apObj, tGuiCallbackFunc apCallback, 
							iWidget* apWidget, const cGuiMessageData& aData, bool abRunFocusChangeChecks)
	{
		if(apObj==NULL || apCallback==NULL) return false;

		size_t vPre[3] = { 0, 0, 0 };

		if(abRunFocusChangeChecks)
		{
			vPre[0] = (size_t)mpSet->GetAttentionWidget();
			vPre[1] = (size_t)mpSet->GetFocusedWidget();
			vPre[2] = (size_t)mpSet->GetDefaultFocusNavWidget();
		}

		bool bRet = apCallback(apObj, apWidget, aData);

		if(abRunFocusChangeChecks)
		{
			size_t vPost[3] = { (size_t)mpSet->GetAttentionWidget(), (size_t)mpSet->GetFocusedWidget(),
								(size_t)mpSet->GetDefaultFocusNavWidget() };
			ohpl_w3_popup_after_callback(&mPopUp, vPre, vPost);
		}

		return bRet;
	}

	//-----------------------------------------------------------------------

	void iGuiPopUp::SetUpDefaultFocus(iWidget* apWidget)
	{
		mpSet->SetDefaultFocusNavWidget(apWidget);
		mpSet->SetFocusedWidget(apWidget);
	}
}
