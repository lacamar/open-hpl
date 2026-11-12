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

#include "LuxMainMenu_CustomStory.h"

#include "LuxInputHandler.h"
#include "LuxSaveHandler.h"
#include "LuxProgressLogHandler.h"
#include "RustLuxMenu2.h"



//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxMainMenu_CustomStory::cLuxMainMenu_CustomStory(cGuiSet* apGuiSet, cGuiSkin* apGuiSkin) : iLuxMainMenuWindow(apGuiSet, apGuiSkin)
{
	mvWindowSize = cVector2f(600,440);

	mpStory = NULL;
}

//-----------------------------------------------------------------------

cLuxMainMenu_CustomStory::~cLuxMainMenu_CustomStory()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMainMenu_CustomStory::CreateGui()
{
	//////////////////////////
	//Window
	mpWindow = mpGuiSet->CreateWidgetWindow(eWidgetWindowButtonFlag_None,cVector3f(0,0,5),mvWindowSize, _W(""));
	//mpWindow->AddCallback(eGuiMessage_OnUpdate, this, kGuiCallback(WindowOnUpdate));

	float fBorderSize = 15;
	cVector3f vPos = cVector3f(fBorderSize, 60+fBorderSize, 0.1f);

	mpIPicture = mpGuiSet->CreateWidgetImage("", cVector3f(6,75,0.05f), cVector2f(588,360), eGuiMaterial_Alpha, false, mpWindow);
	mpIPicture->SetColorMul(cColor(1,0.65f));
	
	mpLAuthor = mpGuiSet->CreateWidgetLabel(vPos+cVector3f(50,-50,0), 0, _W(" "), mpWindow);
	cWidgetFrame* pFDescription = mpGuiSet->CreateWidgetFrame(vPos + cVector3f(0,10,0), cVector2f(420,345), false, mpWindow, false, true);
	mpLDesc = mpGuiSet->CreateWidgetLabel(0, pFDescription->GetSize()-cVector2f(20,10), _W(""), pFDescription);
	mpLDesc->SetWordWrap(true);

	mvButtons.clear();

	tWStringVec vLabels;
	vLabels.push_back(kTranslate("MainMenu","Continue"));
	vLabels.push_back(kTranslate("CustomStory","Start"));
	vLabels.push_back(kTranslate("MainMenu","Load Game"));
	vLabels.push_back(kTranslate("Global","Cancel"));

	float fButtonWidth = 0;
	cWidgetButton* pButton = mpGuiSet->CreateWidgetButton(0,cVector2f(fButtonWidth,30),_W(""), mpWindow);

	//////////////////////////////////////////////////////////////
	// Get longest label string and set button width accordingly
	std::vector<float> vLabelLengths;
	for(int i=0;i<(int)vLabels.size();++i)
	{
		const tWString& sLabel = vLabels[i];

		vLabelLengths.push_back(pButton->GetDefaultFontType()->GetLength(pButton->GetDefaultFontSize(),
																		sLabel.c_str()));
	}

	//////////////////////////
	//Buttons
	fButtonWidth = ohpl_ga7b_pg_button_width(vLabelLengths.data(), vLabelLengths.size());
	float fButtonHeight = 30.0f;
	float fButtonSepp = 3;

	bool bSaveGameExists = gpBase->mpSaveHandler->SaveFileExists();

	vPos.x = mpWindow->GetSize().x - fButtonWidth-fButtonSepp-5;
	vPos.y = mpWindow->GetSize().y*0.5f - 25 - 10;

	//Continue
	pButton->SetPosition(vPos);
	pButton->SetSize(cVector2f(fButtonWidth, fButtonHeight));
	pButton->SetText(vLabels[0]);
	pButton->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(PressContinue));

	mvButtons.push_back(pButton);
	
	vPos.y += fButtonHeight + fButtonSepp*2;

	//Start
	pButton  = mpGuiSet->CreateWidgetButton(vPos,cVector2f(fButtonWidth,fButtonHeight),vLabels[1],mpWindow);
	pButton->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(PressStart));

	mvButtons.push_back(pButton);

	vPos.y += fButtonHeight + fButtonSepp*2;

	//Load game
	pButton  = mpGuiSet->CreateWidgetButton(vPos,cVector2f(fButtonWidth,fButtonHeight),vLabels[2],mpWindow);
	pButton->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(PressLoadGame));

	mvButtons.push_back(pButton);

	vPos.y += fButtonHeight + fButtonSepp*2;

	//Back
	pButton  = mpGuiSet->CreateWidgetButton(vPos,cVector2f(fButtonWidth,fButtonHeight),vLabels[3],mpWindow);
	pButton->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(PressBack));

	mvButtons.push_back(pButton);
}

//-----------------------------------------------------------------------

void cLuxMainMenu_CustomStory::SetCurrentStory(cLuxCustomStorySettings* apStory)
{
	if(mpStory==apStory)
		return;

	if(apStory==NULL)
	{
		gpBase->SetCustomStory(NULL);
		return;
	}

	mpStory = apStory;
	mpStory->SetActive();

	tWString sBy = kTranslate("CustomStory", "By");
	tWString sName, sAuthor, sDescription;

	sName = mpStory->msName;
	sAuthor = mpStory->msAuthor;
	sDescription = kTranslate("CustomStoryMain", "Description");

	uint32_t lFallbacks = ohpl_ga7b_pg_story_fallbacks(sName==_W(""), sAuthor==_W(""), sDescription==_W(""));

	// Retrieve story name
	if(lFallbacks & eOhplGa7bFb_Name)
		sName = kTranslate("CustomStory", "NoName");

	// Retrieve author name
	if(lFallbacks & eOhplGa7bFb_Author)
		sAuthor = kTranslate("CustomStory", "NoAuthor");

	// Retrieve description
	if(lFallbacks & eOhplGa7bFb_Desc)
		sDescription = kTranslate("CustomStory", "NoDescription");

	// Create picture
	if(mpStory->msImgFile!="")
	{
		cGuiGfxElement* pGfx = mpIPicture->GetImage();
		if(pGfx)
		{
			mpGui->DestroyGfx(pGfx);
			pGfx = NULL;
		}

		iTexture* pTex = gpBase->mpEngine->GetResources()->GetTextureManager()->Create2D(mpStory->msImgFile, false, eTextureType_Rect);
		if(pTex)
			pGfx = mpGui->CreateGfxTexture(pTex, true, eGuiMaterial_Alpha);

		mpIPicture->SetImage(pGfx);
	}
		

	mpWindow->SetText(sName);
	mpLAuthor->SetText(sBy + _W(" ") + sAuthor);
	mpLDesc->SetText(sDescription);

	////////////////////////////////////////////////////
	// Set up buttons (positioning and visibility)
	if(mvButtons.empty())
		return;

	bool bSaveFileExists = gpBase->mpSaveHandler->SaveFileExists();

	cVector3f vButtonPos = mvButtons[0]->GetLocalPosition();
	std::vector<float> vHeights;
	for(int i=0;i<(int)mvButtons.size();++i)
		vHeights.push_back(mvButtons[i]->GetSize().y);
	std::vector<unsigned char> vActive(vHeights.size());
	std::vector<float> vY(vHeights.size());
	ohpl_ga7b_pg_story_buttons(bSaveFileExists, vButtonPos.y, vHeights.data(), vHeights.size(), (bool*)vActive.data(), vY.data());

	for(int i=0;i<(int)mvButtons.size();++i)
	{
        cWidgetButton* pButton = mvButtons[i];

		pButton->SetEnabled(vActive[i]);
		pButton->SetVisible(vActive[i]);

		if(vActive[i]==false)
			continue;

		vButtonPos.y = vY[i];
		pButton->SetPosition(vButtonPos);
	}
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMainMenu_CustomStory::OnSetActive(bool abX)
{
}

//-----------------------------------------------------------------------

bool cLuxMainMenu_CustomStory::PressContinue(iWidget* apWidget, const cGuiMessageData& aData)
{
	gpBase->mpInputHandler->ChangeState(eLuxInputState_Game);

	if(gpBase->mpUserConfig==NULL)
	{
		gpBase->CreateProfile(gpBase->msDefaultProfileName);
		gpBase->SetProfile(gpBase->msDefaultProfileName);
		gpBase->InitUserConfig();
	}

	gpBase->mpEngine->GetUpdater()->SetContainer("Default");
	gpBase->mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset);

	gpBase->mpSaveHandler->AutoLoad(true);

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_CustomStory, PressContinue);

//-----------------------------------------------------------------------

bool cLuxMainMenu_CustomStory::PressStart(iWidget* apWidget, const cGuiMessageData& aData)
{
	//////////////////////////////////////////////////////
	// Create save folder if not already present
	tWString sProfileCustomSaveFolder = gpBase->msMainProfileSavePath + _W("custom");
	bool bCustomFolderExists = cPlatform::FolderExists(sProfileCustomSaveFolder);
	uint32_t lFolders = ohpl_ga7b_pg_start_folders(bCustomFolderExists, bCustomFolderExists && cPlatform::FolderExists(gpBase->msProfileSavePath));
	if(lFolders & eOhplGa7bFolder_Custom)
		cPlatform::CreateFolder(sProfileCustomSaveFolder);
	if(lFolders & eOhplGa7bFolder_Profile)
		cPlatform::CreateFolder(gpBase->msProfileSavePath);

	gpBase->StartCustomStory();

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_CustomStory, PressStart);

//-----------------------------------------------------------------------

bool cLuxMainMenu_CustomStory::PressLoadGame(iWidget* apWidget, const cGuiMessageData& aData)
{
	gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_LoadGame);
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_CustomStory, PressLoadGame);

//-----------------------------------------------------------------------

bool cLuxMainMenu_CustomStory::PressBack(iWidget* apWidget, const cGuiMessageData& aData)
{
	SetCurrentStory(NULL);
	gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_CustomStoryList);

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_CustomStory, PressBack);

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxMainMenu_CustomStoryList::cLuxMainMenu_CustomStoryList(cGuiSet *apGuiSet, cGuiSkin *apGuiSkin, cLuxMainMenu_CustomStory* apWindow) : iLuxMainMenuWindow(apGuiSet, apGuiSkin)
{
	mvWindowSize = cVector2f(600,440);

	mpStoryWindow = apWindow;
}

//-----------------------------------------------------------------------

cLuxMainMenu_CustomStoryList::~cLuxMainMenu_CustomStoryList()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMainMenu_CustomStoryList::CreateGui()
{
	//////////////////////////
	//Window
	mpWindow = mpGuiSet->CreateWidgetWindow(eWidgetWindowButtonFlag_None,cVector3f(0,0,5),mvWindowSize,kTranslate("MainMenu","Custom Map"));
	mpWindow->AddCallback(eGuiMessage_OnUpdate, this, kGuiCallback(WindowOnUpdate));

	float fBorderSize = 15;
	cVector3f vPos = cVector3f(fBorderSize, 60+fBorderSize, 0.1f);

	//////////////////////////
	//Custom story list
	mpLBStories = mpGuiSet->CreateWidgetListBox(vPos+cVector3f(0,0,1), cVector2f((mvWindowSize.x-fBorderSize*2),300), mpWindow);
	mpLBStories->AddCallback(eGuiMessage_SelectionChange, this, kGuiCallback(SelectStory));
	mpLBStories->AddCallback(eGuiMessage_SelectionDoubleClick, this, kGuiCallback(PressOK));

	//////////////////////////
	//Buttons
	float fButtonWidth = 80;
	float fButtonSepp = 3;

	vPos.x = mpWindow->GetSize().x - fButtonWidth*2-fButtonSepp-5;
	vPos.y = mpWindow->GetSize().y - 25 - 10;

	//Start
	cWidgetButton* pButton  = mpGuiSet->CreateWidgetButton(vPos,cVector2f(fButtonWidth,30),kTranslate("Global","OK"),mpWindow);
	pButton->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(PressOK));

	//Cancel
	vPos.x += fButtonWidth + fButtonSepp;
	pButton = mpGuiSet->CreateWidgetButton(vPos,cVector2f(fButtonWidth,30),kTranslate("Global","Cancel"),mpWindow);
	pButton->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(PressCancel));
}

//-----------------------------------------------------------------------


void cLuxMainMenu_CustomStoryList::ExitPressed()
{
	if(mpGuiSet->PopUpIsActive()) return;

	gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_LastEnum);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMainMenu_CustomStoryList::OnSetActive(bool abX)
{
	if(abX)
		PopulateStoryList();
	else
		ClearStoryList();
}

//-----------------------------------------------------------------------

void cLuxMainMenu_CustomStoryList::PopulateStoryList()
{
	/////////////////////////////////////////////////
	// Clear list
	ClearStoryList();

#ifdef USERDIR_RESOURCES
	PopulateUserDirStoryList();
#endif

	tWString sPath = cString::AddSlashAtEndW(cString::To16Char(gpBase->msCustomStoryPath));

	tWStringList lstStoryDirs;
	cPlatform::FindFoldersInDir(lstStoryDirs, sPath, false);

	//////////////////////////////////////////////////////////////////////
	// Try to create a story using every dir under the custom story dir
	// the CreateFromPath method will take care of checking it is a valid entry
	tWStringListIt it = lstStoryDirs.begin();
	for(;it!=lstStoryDirs.end();++it)
	{
		const tWString& sStoryPath = sPath + *it;
		cLuxCustomStorySettings* pStory = hplNew(cLuxCustomStorySettings,());

		if(pStory->CreateFromPath(sStoryPath))
		{
			cWidgetItem* pItem = mpLBStories->AddItem(pStory->msName);
			pItem->SetUserData(pStory);
		}
		else
			hplDelete(pStory);
	}
}

//-----------------------------------------------------------------------

#ifdef USERDIR_RESOURCES
void cLuxMainMenu_CustomStoryList::PopulateUserDirStoryList()
{
	tWString sPath = cString::AddSlashAtEndW(gpBase->msUserResourceDir + cString::To16Char(gpBase->msCustomStoryPath));

	tWStringList lstStoryDirs;
	cPlatform::FindFoldersInDir(lstStoryDirs, sPath, false);

	//////////////////////////////////////////////////////////////////////
	// Try to create a story using every dir under the custom story dir
	// the CreateFromPath method will take care of checking it is a valid entry
	tWStringListIt it = lstStoryDirs.begin();
	for(;it!=lstStoryDirs.end();++it)
	{
		const tWString& sStoryPath = sPath + *it;
		cLuxCustomStorySettings* pStory = hplNew(cLuxCustomStorySettings,());

		if(pStory->CreateFromPath(sStoryPath))
		{
			cWidgetItem* pItem = mpLBStories->AddItem(pStory->msName);
			pItem->SetUserData(pStory);
		}
		else
			hplDelete(pStory);

	}
}
#endif


//-----------------------------------------------------------------------

void cLuxMainMenu_CustomStoryList::ClearStoryList()
{
	for(int i=0;i<mpLBStories->GetItemNum();++i)
	{
		cWidgetItem* pItem = mpLBStories->GetItem(i);

		cLuxCustomStorySettings* pStory = (cLuxCustomStorySettings*)pItem->GetUserData();
		hplDelete(pStory);
	}

	mpLBStories->ClearItems();
}

//-----------------------------------------------------------------------

void cLuxMainMenu_CustomStoryList::LoadStory(int alIdx)
{
	/*cWidgetItem* pItem = mpLBStories->GetItem(alIdx);
	if(pItem==NULL)
		return;

	gpBase->mpInputHandler->ChangeState(eLuxInputState_Game);

	gpBase->mpEngine->GetUpdater()->SetContainer("Default");
	gpBase->mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset);

	gpBase->mpProgressLogHandler->CreateAndResetLogFile();

	ExitCallback(NULL, cGuiMessageData(0));

	gpBase->StartGame("", "", "");
	*/
}

//-----------------------------------------------------------------------

bool cLuxMainMenu_CustomStoryList::WindowOnUpdate(iWidget* apWidget, const cGuiMessageData& aData)
{
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_CustomStoryList, WindowOnUpdate);

//-----------------------------------------------------------------------

bool cLuxMainMenu_CustomStoryList::SelectStory(iWidget* apWidget, const cGuiMessageData& aData)
{
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_CustomStoryList, SelectStory);

//-----------------------------------------------------------------------

bool cLuxMainMenu_CustomStoryList::PressOK(iWidget* apWidget, const cGuiMessageData& aData)
{
	/////////////////////////////////////////////////////////////
	// Check if the list has a valid selection, and warn if not
	if(ohpl_ga7b_pg_press_ok(mpLBStories->GetSelectedItem()) == 0)
		mpGuiSet->CreatePopUpMessageBox(kTranslate("Global", "Warning"), 
										kTranslate("CustomStory", "NoValidStory"), 
										kTranslate("Global","OK"), _W(""), 
										NULL, NULL);
	else
		/*mpGuiSet->CreatePopUpMessageBox(kTranslate("Global", "Warning"), 
										kTranslate("CustomStory", "StartCustomQuestion"), 
										kTranslate("Global","OK"), kTranslate("Global","Cancel"), 
										this, kGuiCallback(LoadStoryCallback));
										*/

	{
		cWidgetItem* pItem = mpLBStories->GetItem(mpLBStories->GetSelectedItem());
		cLuxCustomStorySettings* pStory = (cLuxCustomStorySettings*)pItem->GetUserData();
		mpStoryWindow->SetCurrentStory(pStory);

		gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_CustomStory);
	}

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_CustomStoryList, PressOK);

//-----------------------------------------------------------------------

bool cLuxMainMenu_CustomStoryList::PressCancel(iWidget* apWidget, const cGuiMessageData& aData)
{
	ExitCallback(NULL, cGuiMessageData(0));
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_CustomStoryList, PressCancel);

//-----------------------------------------------------------------------

bool cLuxMainMenu_CustomStoryList::LoadStoryCallback(iWidget* apWidget, const cGuiMessageData& aData)
{
	if(ohpl_ga7b_popup_ok(aData.mlVal)==false)
		return true;

	LoadStory(mpLBStories->GetSelectedItem());

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_CustomStoryList, LoadStoryCallback);

//-----------------------------------------------------------------------

bool cLuxMainMenu_CustomStoryList::ExitCallback(iWidget* apWidget, const cGuiMessageData& aData)
{
	bool bOkPressed = ohpl_ga7b_popup_ok(aData.mlVal);
	if(bOkPressed==false)
	{
		gpBase->SetCustomStory(NULL);
		return true;
	}

	gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_LastEnum);

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_CustomStoryList, ExitCallback);

//-----------------------------------------------------------------------
