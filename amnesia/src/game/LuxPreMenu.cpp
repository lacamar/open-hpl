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

#include "LuxPreMenu.h"

#include "LuxMapHandler.h"
#include "LuxInputHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxConfigHandler.h"
#include "LuxDebugHandler.h"
#include "LuxLoadScreenHandler.h"
#include "RustLuxMenu2.h"


//-----------------------------------------------------------------------

static eFontAlign ToFontAlign(const tString& asX)
{
	switch(ohpl_ga7b_pm_parse_align((const unsigned char*)asX.data(), asX.size()))
	{
	case 2: return eFontAlign_Center;
	case 1: return eFontAlign_Right;
	case 0: return eFontAlign_Left;
	}

	Error("The font alignment '%s' does not exist!\n", asX.c_str());
	return eFontAlign_Left;
}

//-----------------------------------------------------------------------

cLuxPreMenuSection::cLuxPreMenuSection()
{
}

cLuxPreMenuSection::~cLuxPreMenuSection()
{
	STLDeleteAll(mlstTextElements);
}

//-----------------------------------------------------------------------

void cLuxPreMenuSection::AddTextElement(cLuxPreMenuTextElement* apText)
{
	if(apText) mlstTextElements.push_back(apText);
}

//-----------------------------------------------------------------------

bool cLuxPreMenuSection::HasTextElements()
{
	return mlstTextElements.empty()==false;
}

//-----------------------------------------------------------------------

cGuiGfxElement* cLuxPreMenuSection::CreateBackground(cGui* apGui, cTextureManager* apTexMgr)
{
	cGuiGfxElement* pBG = NULL;
	// Load background, or create black rect if no file is loaded
	if(msBackgroundFile=="")
	{
		pBG  = apGui->CreateGfxFilledRect(mBackgroundColor, eGuiMaterial_Alpha);
	}
	else
	{
		iTexture* pTex = apTexMgr->Create2D(msBackgroundFile, false, eTextureType_Rect);
		if(pTex) pBG = apGui->CreateGfxTexture(pTex, true, eGuiMaterial_Alpha);
	}

	return pBG;
}

//-----------------------------------------------------------------------

bool cLuxPreMenuSection::Load(cXmlElement* apElement, const cVector2f& avGuiSetSize)
{
	if(apElement==NULL)
		return false;

	mBackgroundColor = apElement->GetAttributeColor("Color", cColor(0,1));
	msBackgroundFile = apElement->GetAttributeString("Image", ""); 
	mfTime = apElement->GetAttributeFloat("Time", 2);
	mbShowFirstStartOnly = apElement->GetAttributeBool("ShowFirstStartOnly", false);
	mbHasGammaSettings = apElement->GetAttributeBool("HasGammaSettings", false);
	
	msMusic = apElement->GetAttributeString("Music", "");
	mfMusicVolume = apElement->GetAttributeFloat("MusicVolume", 0);
	mfMusicFadeTime = apElement->GetAttributeFloat("MusicFadeTime", 0);

	mbAllowSkipping = apElement->GetAttributeBool("AllowSkipping", true);
				
	//////////////////////////////
	// Retrieve text elements
	cXmlNodeListIterator itText = apElement->GetChildIterator();
	while(itText.HasNext())
	{
		cXmlElement* pTextElement = itText.Next()->ToElement();

		// Create and set up text element
		cLuxPreMenuTextElement* pText = hplNew(cLuxPreMenuTextElement,());
		if(pText->Load(pTextElement, avGuiSetSize))
			AddTextElement(pText);
		else
			hplDelete(pText);
	}

	return true;
}

//-----------------------------------------------------------------------

bool cLuxPreMenuTextElement::Load(cXmlElement* apElement, const cVector2f& avGuiSetSize)
{
	if(apElement==NULL)
		return false;

	// Load text data
	tString sCat = apElement->GetAttributeString("TextCat");
	tString sEntry = apElement->GetAttributeString("TextEntry");
	
	msText = kTranslate(sCat, sEntry);
	mvPos = apElement->GetAttributeVector3f("Pos") + cVector3f(0,0,1);
	mvFrameSize = apElement->GetAttributeVector2f("FrameSize");
	mvFontSize = apElement->GetAttributeVector2f("FontSize", cVector2f(0));
	mColor = apElement->GetAttributeColor("Color");
	mAlign = ToFontAlign(apElement->GetAttributeString("Align", "Left"));

	mfTime = apElement->GetAttributeFloat("time", 2);

	return true;
}

//-----------------------------------------------------------------------

cWidgetLabel* cLuxPreMenuTextElement::CreateLabel(cGuiSet* apSet)
{
	if(apSet==NULL)	return NULL;

	cWidgetLabel* pLabel = apSet->CreateWidgetLabel(mvPos, mvFrameSize, msText);
	pLabel->SetDefaultFontColor(mColor);
	pLabel->SetDefaultFontSize(mvFontSize);
	pLabel->SetTextAlign(mAlign);
	pLabel->SetWordWrap(true);

	return pLabel;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPreMenu::cLuxPreMenu() : iLuxUpdateable("LuxPreMenu")
{
	///////////////////////////////
	//Get Engine
	mpGraphics = gpBase->mpEngine->GetGraphics();
	mpGui = gpBase->mpEngine->GetGui();
	mpGuiSet = mpGui->CreateSet("PreMenu", NULL);
	mpGuiSet->SetDrawMouse(false);//Init

	///////////////////////
	// Load settings
	mvScreenSize = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();

	mvGuiSetCenterSize = cVector2f(800, 600);

	LuxCalcGuiSetScreenOffset(mvGuiSetCenterSize, mvGuiSetSize, mvGuiSetOffset);
	mvGuiSetStartPos = cVector3f(-mvGuiSetOffset.x,-mvGuiSetOffset.y,0);

	// full-bleed at scale 1: exempt from GuiScale
	mpGuiSet->SetVirtualSize(mvGuiSetSize, -1000,1000, mvGuiSetOffset, true);

	///////////////////////////////
	//Create Viewport
	mpViewport = gpBase->mpEngine->GetScene()->CreateViewport();
	mpViewport->SetActive(false);
	mpViewport->SetVisible(false);
	mpGuiSet->SetActive(false);
	mpGuiSet->SetDrawMouse(false);//Init

	mpViewport->AddGuiSet(mpGuiSet);

	///////////////////////////////
	//Setup variables
	Reset();

	mpBlackFade = mpGui->CreateGfxFilledRect(cColor(0,1), eGuiMaterial_Alpha, true);
	ohpl_ga7b_pm_new(&mS);


	///////////////////////////////////////
	// Load skin and sections
	LoadPreMenuSections();

	///////////////////////////////////////
	// Create continue button
	tWString sButtonLabel = kTranslate("PreMenu", "Continue");

	mpBContinue = mpGuiSet->CreateWidgetButton(0,0, sButtonLabel, NULL);
	iFontData* pFont = mpBContinue->GetDefaultFontType();

	float fLength = pFont->GetLength(cVector2f(pFont->GetHeight()), sButtonLabel.c_str()) + 20;

	cVector2f vSize = cVector2f(fLength, pFont->GetHeight()) + 10;
	cVector3f vPos = (cVector3f(mvGuiSetCenterSize-vSize) + cVector3f(0,-10,0.1f)) * cVector3f(0.5f, 1, 1);

	mpBContinue->SetPosition(vPos);
	mpBContinue->SetSize(vSize);

	mpBContinue->AddCallback(eGuiMessage_ButtonPressed, this, kGuiCallback(Continue_Pressed));
	mpBContinue->AddCallback(eGuiMessage_UIButtonPress, this, kGuiCallback(Continue_UIPressed));
	mpBContinue->AddCallback(eGuiMessage_UIArrowPress, this, kGuiCallback(Gamma_UIArrowPressed));
	mpBContinue->SetGlobalUIInputListener(true);
	mpBContinue->SetVisible(false);

	///////////////////////////////////////
	// Create gamma widgets
	{
		cVector2f vCenter = mvGuiSetCenterSize*0.5f;
		mpGGamma = mpGuiSet->CreateWidgetDummy(0);

		// Preview image
		mpIGammaPreview = mpGuiSet->CreateWidgetImage("menu_gamma.tga", 0, -1, eGuiMaterial_Alpha, false, mpGGamma);

		vSize = mpIGammaPreview->GetSize() * 1.25f;
		//vPos = (cVector3f(mvGuiSetCenterSize-vSize) + cVector3f(0,40,0.1f)) * cVector3f(0.5f, 0.5f, 1);

		mpIGammaPreview->SetPosition(cVector3f(0,0,0.1f));
		mpIGammaPreview->SetSize(vSize);

		mpIGammaPreview->SetVisible(false);
		mpIGammaPreview->SetEnabled(false);

		// Value label
		mpLGamma = mpGuiSet->CreateWidgetLabel(0, 0, _W(""), mpGGamma);

		mpLGamma->SetVisible(false);
		mpLGamma->SetEnabled(false);

		// Slider
		mfGammaMinValue = 0.3f;
		mfGammaMaxValue = 2.0f;
		mfGammaStep = 0.05f;

		vSize.y = 25;

		mpSGamma = mpGuiSet->CreateWidgetSlider(eWidgetSliderOrientation_Horizontal, vPos, vSize, 10, mpGGamma);
		mpSGamma->AddCallback(eGuiMessage_SliderMove, this, kGuiCallback(Gamma_ChangeValue));

		int vSlider[2];
		ohpl_ga7b_pm_gamma_slider(vSlider);
		mpSGamma->SetMaxValue(vSlider[0]);
		mpSGamma->SetBarValueSize(vSlider[1]);

		// Set initial value
		SetGammaValueToInput(gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection(), true);

		mpSGamma->SetVisible(false);
		mpSGamma->SetEnabled(false);

		vPos = cVector3f(0,0,0.1f);
		mpIGammaPreview->SetPosition(vPos);

		vPos.y += mpIGammaPreview->GetSize().y + 10;
		mpLGamma->SetPosition(vPos);

		vPos.y += 25.0f;
		mpSGamma->SetPosition(vPos);

		vPos = cVector3f(vCenter-(mpGGamma->GetSize()*0.5f)) + cVector3f(0,20,0.1f);
		mpGGamma->SetPosition(vPos);
	}

	mpGuiSet->SetDefaultFocusNavWidget(mpBContinue);
	mpGuiSet->SetFocusedWidget(mpBContinue);
}

//-----------------------------------------------------------------------

cLuxPreMenu::~cLuxPreMenu()
{
	//Destroy data
	STLDeleteAll(mvSections);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPreMenu::OnEnterContainer(const tString& asOldContainer)
{
	//Unlock input if not in window
	if(gpBase->mpConfigHandler->mbFullscreen==false) {
		gpBase->mpEngine->GetInput()->GetLowLevel()->LockInput(false);
	}
	gpBase->mpEngine->GetInput()->GetLowLevel()->RelativeMouse(false);

	////////////////////////////
	//Set up states and viewport
	gpBase->mpInputHandler->ChangeState(eLuxInputState_PreMenu);

	mpViewport->SetActive(true);
	mpViewport->SetVisible(true);

	mpGuiSet->SetActive(true);
	mpGui->SetFocus(mpGuiSet);

	///////////////////////////////
	//Load data
	ohpl_ga7b_pm_enter(&mS);
	mpCurrentBackground = NULL;
	mpCurrentSection = NULL;
}

//-----------------------------------------------------------------------

void cLuxPreMenu::OnLeaveContainer(const tString& asNewContainer)
{
	//Unlock input if not in window
	if (gpBase->mpDebugHandler->GetDebugWindowActive()==false)
	{
		if(	gpBase->mpConfigHandler->mbFullscreen==false)
		{
			gpBase->mpEngine->GetInput()->GetLowLevel()->LockInput(true);
		}
		gpBase->mpEngine->GetInput()->GetLowLevel()->RelativeMouse(true);
	}
	
	//Turn off viewport and sets
    mpViewport->SetActive(false);
	mpViewport->SetVisible(false);

	mpGuiSet->SetActive(false);

	gpBase->mpLoadScreenHandler->DrawMenuScreen();
}

//-----------------------------------------------------------------------

void cLuxPreMenu::Update(float afTimeStep)
{
	UpdateActions(afTimeStep);
	UpdateState();

	////////////////////////////////////////////////////////
	// Exit premenu
	if(mS.mbExit)
	{
		if(gpBase->mbShowMenu)
		{
			gpBase->mpEngine->GetUpdater()->SetContainer("MainMenu");
		}
		else
		{
			gpBase->mpInputHandler->ChangeState(eLuxInputState_Game);

			//Create (if not existing) and set profile.
			gpBase->CreateProfile(gpBase->msDefaultProfileName);
			gpBase->SetProfile(gpBase->msDefaultProfileName);

			//Load user config
			if(gpBase->InitUserConfig()==false)
			{
				gpBase->Exit();
				return;
			}

			gpBase->mpEngine->GetUpdater()->SetContainer("Default");

			//Load map and start game.
			//By using "" user config values are used.
			gpBase->StartGame("", "", "");
		}
	}
}

//-----------------------------------------------------------------------

void cLuxPreMenu::SetGammaValueToInput(float afGamma, bool abGenCallback)
{
	mpSGamma->SetValue(ohpl_ga7b_pm_gamma_to_slider(afGamma, mpSGamma->GetMaxValue()), abGenCallback);
}

//-----------------------------------------------------------------------

bool cLuxPreMenu::Gamma_ChangeValue(iWidget* apWidget, const cGuiMessageData& aData)
{
	float fGamma = ohpl_ga7b_pm_slider_to_gamma(mpSGamma->GetValue(), mpSGamma->GetMaxValue());

	mpLGamma->SetText(kTranslate("OptionsMenu","Gamma") + _W(": ") + cString::ToStringW(fGamma, 2, true));
	gpBase->mpEngine->GetGraphics()->GetLowLevel()->SetGammaCorrection(fGamma);

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxPreMenu, Gamma_ChangeValue);

//-----------------------------------------------------------------------

bool cLuxPreMenu::Continue_Pressed(iWidget* apWidget, const cGuiMessageData& aData)
{
	mS.mlState = eLuxPreMenuState_FadeOut;

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxPreMenu, Continue_Pressed);

//-----------------------------------------------------------------------------

bool cLuxPreMenu::Continue_UIPressed(iWidget* apWidget, const cGuiMessageData& aData)
{
	if(aData.mlVal == eUIButton_Primary)
	{
		mS.mlState = eLuxPreMenuState_FadeOut;

		return true;
	}
	return false;
}
kGuiCallbackDeclaredFuncEnd(cLuxPreMenu, Continue_UIPressed);

//-----------------------------------------------------------------------------

bool cLuxPreMenu::Gamma_UIArrowPressed(iWidget* apWidget, const cGuiMessageData& aData)
{
	int lStep = ohpl_ga7b_pm_gamma_arrow(aData.mlVal);
	if(lStep != 0)
	{
		mpSGamma->SetValue(mpSGamma->GetValue() + lStep, true);
	}
	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxPreMenu, Gamma_UIArrowPressed);

//-----------------------------------------------------------------------

void cLuxPreMenu::ButtonPressed()
{
	ohpl_ga7b_pm_button_pressed(&mS, mpCurrentSection != NULL, mpCurrentSection ? mpCurrentSection->mbAllowSkipping : true);
}

//-----------------------------------------------------------------------

void cLuxPreMenu::OnDraw(float afFrameTime)
{
	///////////////////////////////////////
	// Draw fade rect and background
	mpGuiSet->DrawGfx(mpBlackFade, mvGuiSetStartPos + cVector3f(0,0,2), mvGuiSetSize, cColor(1, mS.mfAlpha));
	mpGuiSet->DrawGfx(mpBlackFade, mvGuiSetStartPos - cVector3f(0,0,2), mvGuiSetSize, cColor(1, 1));

	if(mpCurrentBackground)
	{
		//Draw the image with correct ratio, always filling out height-wise.
		cVector2f vSize = mpCurrentBackground->GetImageSize();
		float vRect[2];
		ohpl_ga7b_pm_background_rect(vSize.x, vSize.y, vRect);

		mpGuiSet->DrawGfx(mpCurrentBackground, cVector3f(vRect[0],0,0), cVector2f(vRect[1], 600),cColor(1,1));
	}
}

//-----------------------------------------------------------------------

void cLuxPreMenu::AppLostInputFocus()
{
	if(gpBase->mpMapHandler->MapIsLoaded()==false)
	{
		cMusicHandler* pMusHdlr = gpBase->mpEngine->GetSound()->GetMusicHandler();
		pMusHdlr->Pause();
	}
}

//-----------------------------------------------------------------------

void cLuxPreMenu::AppGotInputFocus()
{
	if(gpBase->mpMapHandler->MapIsLoaded()==false)
	{
		cMusicHandler* pMusHdlr = gpBase->mpEngine->GetSound()->GetMusicHandler();
		pMusHdlr->Resume();
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPreMenu::UpdateActions(float afTimeStep)
{
	ohpl_ga7b_pm_update_actions(&mS, afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxPreMenu::UpdateState()
{
	cTextureManager* pTexMgr = gpBase->mpEngine->GetResources()->GetTextureManager();

	std::vector<unsigned char> vFirstOnly(mvSections.size());
	for(size_t i=0; i<mvSections.size(); ++i)
		vFirstOnly[i] = mvSections[i]->ShowFirstStartOnly();

	OhplGa7bStep step;
	ohpl_ga7b_pm_update_state(&mS, gpBase->mpConfigHandler->mbFirstStart, (const bool*)vFirstOnly.data(), vFirstOnly.size(), &step);

	switch(step.mlAct)
	{
	case eOhplGa7bPreAct_SetTimer:
		mS.mfTimer = mpCurrentSection->GetTime();
		break;
	case eOhplGa7bPreAct_Final:
	case eOhplGa7bPreAct_Section:
		if(mpCurrentBackground)
		{
			mpGui->DestroyGfx(mpCurrentBackground);
			mpCurrentBackground = NULL;
		}

		for(int i=0;i<(int)mvCurrentLabels.size();++i)
		{
			mpGuiSet->DestroyWidget(mvCurrentLabels[i]);
		}
		mvCurrentLabels.clear();

		if(step.mlAct == eOhplGa7bPreAct_Final)
		{
			mpCurrentSection = NULL;
		}
		else
		{
			mpCurrentSection = mvSections[step.mlSection];

			/////////////////////////////////
			// Set up current section
			if(mpCurrentSection)
			{
				bool bHasTextElements = mpCurrentSection->HasTextElements();
				bool bGuiSetActive = (bHasTextElements || mpCurrentSection->HasGammaSettings());

				////////////////////////////////////////////
				// Set up Background
				mpCurrentBackground = mpCurrentSection->CreateBackground(mpGui, pTexMgr);

				////////////////////////////////////////////
				// Set up Gui elements
				mpLGamma->SetVisible(mpCurrentSection->HasGammaSettings());
				mpLGamma->SetEnabled(mpCurrentSection->HasGammaSettings());
				mpSGamma->SetVisible(mpCurrentSection->HasGammaSettings());
				mpSGamma->SetEnabled(mpCurrentSection->HasGammaSettings());

				mpIGammaPreview->SetVisible(mpCurrentSection->HasGammaSettings());
				mpIGammaPreview->SetEnabled(mpCurrentSection->HasGammaSettings());

				mpBContinue->SetVisible(bGuiSetActive);
				mpBContinue->SetEnabled(bGuiSetActive);
				mpGuiSet->SetDrawMouse(bGuiSetActive);

				////////////////////////////////////////////
				// Set up text
				const tPreMenuTextList& lstText = mpCurrentSection->GetTextElements();
				tPreMenuTextList::const_iterator itText = lstText.begin();
				for(;itText!=lstText.end();++itText)
				{
					cLuxPreMenuTextElement* pText = *itText;

					// One label per element, word wrap enabled
					cWidgetLabel* pLabel = pText->CreateLabel(mpGuiSet);
					mvCurrentLabels.push_back(pLabel);
				}

				////////////////////////////////////////////
				// Play music
				if(mpCurrentSection->msMusic != "")
				{
					cMusicHandler* pMusHandler = gpBase->mpEngine->GetSound()->GetMusicHandler();

					float fFadeSpeed = ohpl_ga7b_pm_music_fade_speed(mpCurrentSection->mfMusicFadeTime);
					pMusHandler->Play(mpCurrentSection->msMusic, mpCurrentSection->mfMusicVolume, fFadeSpeed, true, false);
				}
			}
		}
		break;
	case eOhplGa7bPreAct_CheckShow:
		ohpl_ga7b_pm_check_show(&mS, mpCurrentSection->HasTextElements(), mpCurrentSection->HasGammaSettings());
		break;
	}
}

//-----------------------------------------------------------------------

void cLuxPreMenu::LoadPreMenuSections()
{
	///////////////////////////////////////////////
	// First clear any previously loaded stuff
	cResources* pRes = gpBase->mpEngine->GetResources();

	//////////////////////////////////////////////////////////////////////////////
	// Check file pointed by config file and retrieve image files from there
	iXmlDocument* pDoc = pRes->GetLowLevel()->CreateXmlDocument();
	tWString sConfigPath = gpBase->msPreMenuConfigPath;
#ifdef USERDIR_RESOURCES
	if (gpBase->msUserResourceDir.length()
		&& cPlatform::FileExists(gpBase->msUserResourceDir + sConfigPath)) {
		sConfigPath = gpBase->msUserResourceDir + sConfigPath;
	}
#endif
	if(pDoc->CreateFromFile(sConfigPath))
	{
		/////////////////////////////////////////////////////
		// Set up skin
		tString sSkinFile = pDoc->GetAttributeString("skin", "gui_default.skin");
		mpGuiSet->SetSkin(mpGui->CreateSkin(sSkinFile));

		cXmlNodeListIterator it = pDoc->GetChildIterator();
		while(it.HasNext())
		{
			cXmlElement* pSectionElement = it.Next()->ToElement();

			cLuxPreMenuSection* pSection = hplNew(cLuxPreMenuSection, ());
			if(pSection->Load(pSectionElement, mvGuiSetCenterSize))
				mvSections.push_back(pSection);
			else
				hplDelete(pSection);
		}
	}

	pRes->DestroyXmlDocument(pDoc);
}

//-----------------------------------------------------------------------
