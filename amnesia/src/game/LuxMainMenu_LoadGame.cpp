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

#include "LuxMainMenu_LoadGame.h"

#include "LuxSaveHandler.h"
#include "LuxInputHandler.h"
#include "LuxProgressLogHandler.h"
#include "RustLuxMenu2.h"


//////////////////////////////////////////////
// TODO - add a snapshot view?

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxMainMenu_LoadGame::cLuxMainMenu_LoadGame(cGuiSet *apGuiSet, cGuiSkin *apGuiSkin) : iLuxMainMenuWindow(apGuiSet, apGuiSkin)
{
	mvWindowSize = cVector2f(600,440);
}

//-----------------------------------------------------------------------

cLuxMainMenu_LoadGame::~cLuxMainMenu_LoadGame()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMainMenu_LoadGame::CreateGui()
{
	//////////////////////////
	//Window
	mpWindow = mpGuiSet->CreateWidgetWindow(eWidgetWindowButtonFlag_None,cVector3f(0,0,5),mvWindowSize,kTranslate("LoadGame","LoadGameTitle"));
	mpWindow->AddCallback(eGuiMessage_OnUpdate, this, kGuiCallback(WindowOnUpdate));

	float fBorderSize = 15;
	cVector3f vPos = cVector3f(fBorderSize, 60+fBorderSize, 0.1f);

	//////////////////////////
	//Saved game list
	mpLBSavedGames = mpGuiSet->CreateWidgetListBox(vPos+cVector3f(0,0,1), cVector2f(mvWindowSize.x-fBorderSize*2,300), mpWindow);
	mpLBSavedGames->AddCallback(eGuiMessage_SelectionDoubleClick, this, kGuiCallback(PressOK));
	mpLBSavedGames->AddCallback(eGuiMessage_CheckChange, this, kGuiCallback(LoadSelectionClickChange));
	mpLBSavedGames->AddCallback(eGuiMessage_GetUINavFocus, this, kGuiCallback(LockLoadList));
	mpLBSavedGames->AddCallback(eGuiMessage_UIButtonPress, this, kGuiCallback(UIPressList));


	////////////////////////////////
	//Saved game snapshot container
	mpISavedGameSnapShot = mpGuiSet->CreateWidgetImage("", mpLBSavedGames->GetSize().x+10, cVector2f(200), eGuiMaterial_Alpha, false, mpLBSavedGames);

	//////////////////////////
	//Buttons
	float fButtonWidth = 80;
	float fButtonSepp = 3;

	vPos.x = mpWindow->GetSize().x - fButtonWidth*2-fButtonSepp-5;
	vPos.y = mpWindow->GetSize().y - 25 - 10;

	std::vector<iWidget*> vButtons;

	// Load Game
	cWidgetButton* pButton  = mpGuiSet->CreateWidgetButton(vPos,cVector2f(fButtonWidth,30),kTranslate("Global","OK"),mpWindow);
	pButton->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(PressOK));
	pButton->AddCallback(eGuiMessage_UIButtonPress,this, kGuiCallback(UIPress));
	mpLoadButton = pButton;
	vButtons.push_back(pButton);

	//Cancel
	vPos.x += fButtonWidth + fButtonSepp;
	pButton = mpGuiSet->CreateWidgetButton(vPos,cVector2f(fButtonWidth,30),kTranslate("Global","Cancel"),mpWindow);
	pButton->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(PressCancel));
	pButton->AddCallback(eGuiMessage_UIButtonPress,this, kGuiCallback(UIPressCancel));
	vButtons.push_back(pButton);

	////////////////////////////////////////
	// Set up focus navigation
	mpLBSavedGames->SetFocusNavigation(eUIArrow_Down, vButtons[0]);

	vButtons[0]->SetFocusNavigation(eUIArrow_Right, vButtons[1]);
	vButtons[1]->SetFocusNavigation(eUIArrow_Left, vButtons[0]);
	for(size_t i=0; i<vButtons.size(); ++i)
		vButtons[i]->SetFocusNavigation(eUIArrow_Up, mpLBSavedGames);
}

//-----------------------------------------------------------------------

void cLuxMainMenu_LoadGame::ExitPressed()
{
	if(mpGuiSet->PopUpIsActive()) return;

	gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_LastEnum);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMainMenu_LoadGame::OnSetActive(bool abX)
{
	if(abX)
	{
		PopulateSavedGameList();

		mpGuiSet->SetDefaultFocusNavWidget(mpLBSavedGames);
		mpGuiSet->SetFocusedWidget(mpLBSavedGames);

		if(mpLBSavedGames->GetItemNum()>0)
		{
			mpLBSavedGames->SetIsLocked(true);
		}
	}
}

//-----------------------------------------------------------------------

void cLuxMainMenu_LoadGame::PopulateSavedGameList()
{
	mpLBSavedGames->ClearItems();
	mvSavedGameFileNames.clear();

	static_assert(sizeof(cDate) == 8*sizeof(int), "cDate layout");

	tWStringList lstSavedGameFiles;
	cPlatform::FindFilesInDir(lstSavedGameFiles, gpBase->msProfileSavePath, _W("*.sav"));

	std::vector<tWString> vFiles;
	std::vector<cDate> vDates;
	tWStringListIt it = lstSavedGameFiles.begin();
	for(;it!=lstSavedGameFiles.end();++it)
	{
		const tWString& sSavedGameFile = *it;
		tWString sPath = gpBase->msProfileSavePath + sSavedGameFile;
		vDates.push_back(cPlatform::FileModifiedDate(sPath));
		vFiles.push_back(sSavedGameFile);
	}

	std::vector<uint32_t> vOrder(vFiles.size());
	ohpl_ga7b_pg_sort_saves(vDates.data(), vDates.size(), vOrder.data());

	for(size_t i = 0; i < vOrder.size(); ++i)
	{
		const tWString& sSavedGameFile = vFiles[vOrder[i]];
		mpLBSavedGames->AddItem(gpBase->mpSaveHandler->GetProperSaveName(sSavedGameFile));
		mvSavedGameFileNames.push_back(sSavedGameFile);
	}
}

//-----------------------------------------------------------------------

void cLuxMainMenu_LoadGame::LoadGame(int alIdx)
{
	ExitCallback(NULL, cGuiMessageData(0));

	gpBase->mpInputHandler->ChangeState(eLuxInputState_Game);

	gpBase->mpEngine->GetUpdater()->SetContainer("Default");
	gpBase->mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset);

	gpBase->mpProgressLogHandler->CreateAndResetLogFile();
	int lFile = ohpl_ga7b_pg_load_index(alIdx, mvSavedGameFileNames.size());
	if(lFile >= 0)
		gpBase->mpSaveHandler->LoadGameFromFile(gpBase->msProfileSavePath+mvSavedGameFileNames[lFile]);
}

//-----------------------------------------------------------------------

bool cLuxMainMenu_LoadGame::WindowOnUpdate(iWidget* apWidget, const cGuiMessageData& aData)
{
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_LoadGame, WindowOnUpdate);

//-----------------------------------------------------------------------

bool cLuxMainMenu_LoadGame::PressOK(iWidget* apWidget, const cGuiMessageData& aData)
{
	/////////////////////////////////////////////////////////////
	// Check if the list has a valid selection, and warn if not
	if(ohpl_ga7b_pg_press_ok(mpLBSavedGames->GetSelectedItem()) == 0)
		mpGuiSet->CreatePopUpMessageBox(kTranslate("Global", "Warning"), 
										kTranslate("LoadGame", "NoGame"), 
										kTranslate("Global","OK"), _W(""), 
										NULL, NULL);
	else
		mpGuiSet->CreatePopUpMessageBox(kTranslate("Global", "Warning"), 
										kTranslate("LoadGame", "LoadGameMessage"), 
										kTranslate("Global","OK"), kTranslate("Global","Cancel"), 
										this, kGuiCallback(LoadGameCallback));

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_LoadGame, PressOK);

//-----------------------------------------------------------------------

bool cLuxMainMenu_LoadGame::PressCancel(iWidget* apWidget, const cGuiMessageData& aData)
{
	ExitCallback(NULL, cGuiMessageData(0));
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_LoadGame, PressCancel);
//-----------------------------------------------------------------------

bool cLuxMainMenu_LoadGame::UIPressList(iWidget* apWidget, const cGuiMessageData& aData)
{
	switch(ohpl_ga7b_pg_ui_switch(aData.mlVal))
	{
	case eOhplGa7bUi_Secondary: return PressCancel(apWidget, aData);
	}

	return false;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_LoadGame, UIPressList);

//-----------------------------------------------------------------------

bool cLuxMainMenu_LoadGame::UIPress(iWidget* apWidget, const cGuiMessageData& aData)
{
	switch(ohpl_ga7b_pg_ui_switch(aData.mlVal))
	{
	case eOhplGa7bUi_Primary: return PressOK(apWidget, aData);
	case eOhplGa7bUi_Secondary: return PressCancel(apWidget, aData);
	}

	return false;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_LoadGame, UIPress);

bool cLuxMainMenu_LoadGame::UIPressCancel(iWidget* apWidget, const cGuiMessageData& aData)
{
	switch(ohpl_ga7b_pg_ui_switch(aData.mlVal))
	{
	case eOhplGa7bUi_Primary: return PressCancel(apWidget, aData);
	case eOhplGa7bUi_Secondary: return PressCancel(apWidget, aData);
	}

	return false;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_LoadGame, UIPressCancel);

//-----------------------------------------------------------------------

bool cLuxMainMenu_LoadGame::LoadGameCallback(iWidget* apWidget, const cGuiMessageData& aData)
{
	if(ohpl_ga7b_popup_ok(aData.mlVal)==false)
		return true;
	LoadGame(mpLBSavedGames->GetSelectedItem());

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_LoadGame, LoadGameCallback);

//-----------------------------------------------------------------------

bool cLuxMainMenu_LoadGame::ExitCallback(iWidget* apWidget, const cGuiMessageData& aData)
{
	int lTarget = ohpl_ga7b_pg_exit_target(gpBase->mpCustomStory!=NULL, ohpl_ga7b_popup_ok(aData.mlVal));
	if(lTarget < 0)
		return true;

	gpBase->mpMainMenu->SetWindowActive((eLuxMainMenuWindow)lTarget);

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_LoadGame, ExitCallback);

//-----------------------------------------------------------------------

bool cLuxMainMenu_LoadGame::LoadSelectionClickChange(iWidget* apWidget, const cGuiMessageData& aData)
{
	mpGuiSet->SetFocusedWidget(mpLoadButton);

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_LoadGame, LoadSelectionClickChange);

//-----------------------------------------------------------------------

bool cLuxMainMenu_LoadGame::LockLoadList(iWidget* apWidget, const cGuiMessageData& aData)
{
	mpLBSavedGames->SetIsLocked(true);

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_LoadGame, LockLoadList);

//----------------------------------------------------------------------