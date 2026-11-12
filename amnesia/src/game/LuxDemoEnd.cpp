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

#include "LuxDemoEnd.h"

#include "LuxHelpFuncs.h"
#include "LuxDebugHandler.h"
#include "LuxLoadScreenHandler.h"
#include "LuxMusicHandler.h"
#include "LuxInputHandler.h"
#include "RustLuxMenu2.h"


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxDemoEnd::cLuxDemoEnd() : iLuxUpdateable("LuxDemoEnd")
{
	///////////////////////////////
	//Get Engine
	mpGraphics = gpBase->mpEngine->GetGraphics();
	mpGui = gpBase->mpEngine->GetGui();
	mpGuiSet = mpGui->CreateSet("DemoEnd", NULL);

	///////////////////////
	// Load settings
	mvScreenSize = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();

	mvGuiSetCenterSize = cVector2f(800, 600);

	LuxCalcGuiSetScreenOffset(mvGuiSetCenterSize, mvGuiSetSize, mvGuiSetOffset);
	mvGuiSetStartPos = cVector3f(-mvGuiSetOffset.x,-mvGuiSetOffset.y,0);

	// fixed 800x600 layout: exempt from GuiScale
	mpGuiSet->SetVirtualSize(mvGuiSetSize, -1000,1000, mvGuiSetOffset, true);

	///////////////////////////////
	//Create Viewport
	mpViewport = gpBase->mpEngine->GetScene()->CreateViewport();
	mpViewport->SetActive(false);
	mpViewport->SetVisible(false);
	mpGuiSet->SetActive(false);
	mpGuiSet->SetDrawMouse(false);

	mpViewport->AddGuiSet(mpGuiSet);

	mpFontMessage = NULL;
	mpFontButton = NULL;

	mpGfxBackground = NULL;

	ohpl_ga7b_de_new(&mS);


	///////////////////////////////
	//Load Data
	tString sSkinFile = gpBase->mpDemoCfg->GetString("DemoEnd", "Skin", "gui_default.skin");
	mpGuiSet->SetSkin(mpGui->CreateSkin(sSkinFile));

	msBackgroundFile = gpBase->mpDemoCfg->GetString("DemoEnd", "BackgroundImage", "");

	mvMessageFontSize = gpBase->mpDemoCfg->GetVector2f("DemoEnd", "MessageFontSize" ,0);
	mMessageFontColor = gpBase->mpDemoCfg->GetColor("DemoEnd", "MessageFontColor", cColor(1,1));

	mvAvailableAtFontSize = gpBase->mpDemoCfg->GetVector2f("DemoEnd", "AvailableAtFontSize" ,0);
	mAvailableAtFontColor = gpBase->mpDemoCfg->GetColor("DemoEnd", "AvailableAtFontColor", cColor(1,1));

	mvButtonFontSize = gpBase->mpDemoCfg->GetVector2f("DemoEnd", "ButtonFontSize", 0);
	mButtonFontColor = gpBase->mpDemoCfg->GetColor("DemoEnd", "ButtonFontColor", cColor(1,1));

	mfTextWidth = gpBase->mpDemoCfg->GetFloat("DemoEnd", "TextWidth",0);
	mfTextY = gpBase->mpDemoCfg->GetFloat("DemoEnd", "TextY",0);

	mfAvailableAtY = gpBase->mpDemoCfg->GetFloat("DemoEnd", "AvailableAtY",0);

	mfBuyButtonY = gpBase->mpDemoCfg->GetFloat("DemoEnd", "BuyButtonY",0);
	mfExitButtonY = gpBase->mpDemoCfg->GetFloat("DemoEnd", "ExitButtonY",0);
	
	msDestinationURL = gpBase->mpDemoCfg->GetString("DemoEnd", "DestinationURL", "");

	mpBlackGfx = mpGui->CreateGfxFilledRect(cColor(0,1), eGuiMaterial_Alpha);

	mfFadeSpeed = gpBase->mpDemoCfg->GetFloat("DemoEnd","FadeSpeed",0);

	mbShowOnAllExit = gpBase->mpDemoCfg->GetBool("DemoEnd","ShowOnAllExit",false);
	
	//Create buttons label buttons
	cVector2f vButtonSize = cVector2f(150, mvButtonFontSize.y);

	cVector3f vButtonPos;
	vButtonPos.x = 400;// - vButtonSize.x/2;
	vButtonPos.z = 10;

	vButtonPos.y = mfBuyButtonY;
	mpLBuyNow = mpGuiSet->CreateWidgetLabel(vButtonPos, vButtonSize, kTranslate("Demo", "BuyNow"), NULL);
	
	vButtonPos.y = mfExitButtonY;
	mpLExit = mpGuiSet->CreateWidgetLabel(vButtonPos, vButtonSize, kTranslate("Demo", "Exit"), NULL);

	SetUpButtonLabel(mpLBuyNow, &mfBuyNowFade, kGuiCallback(BuyNowOnPressed));
	SetUpButtonLabel(mpLExit, &mfExitFade, kGuiCallback(ExitOnPressed));

	///////////////////////////////
	//Setup variables
	Reset();
}

//-----------------------------------------------------------------------

cLuxDemoEnd::~cLuxDemoEnd()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxDemoEnd::LoadFonts()
{
	tString sFontMessage = gpBase->mpDemoCfg->GetString("DemoEnd","MessageFont","");
	tString sFontButton = gpBase->mpDemoCfg->GetString("DemoEnd", "ButtonFont", "");
	mpFontMessage = LoadFont(sFontMessage);
	mpFontButton = LoadFont(sFontButton);

	if(mpFontButton)
	{
		mpLBuyNow->SetDefaultFontType(mpFontButton);
		mpLExit->SetDefaultFontType(mpFontButton);
	}
}

//-----------------------------------------------------------------------

void cLuxDemoEnd::Reset()
{
	ohpl_ga7b_de_reset(&mS);

}

//-----------------------------------------------------------------------

void cLuxDemoEnd::OnEnterContainer(const tString& asOldContainer)
{
	////////////////////////////
	//Set up states and viewport
	gpBase->mpInputHandler->ChangeState(eLuxInputState_DemoEnd);

	mpViewport->SetActive(true);
	mpViewport->SetVisible(true);

	mpGuiSet->SetActive(true);
	mpGuiSet->SetDrawMouse(true);
	mpGui->SetFocus(mpGuiSet);

	///////////////////////////////
	//Fade out music
	cMusicHandler* pMusHandler = gpBase->mpEngine->GetSound()->GetMusicHandler();
	pMusHandler->Stop(0.3f);
	
	// Load background
	iTexture* pTex = gpBase->mpEngine->GetResources()->GetTextureManager()->Create2D(msBackgroundFile, false, eTextureType_Rect);
	if(pTex) mpGfxBackground = mpGui->CreateGfxTexture(pTex, true, eGuiMaterial_Alpha);

	// Set up message text
	tWString sText = kTranslate("Demo", "EndMessage");
	mvTextRows.clear();
	mpFontMessage->GetWordWrapRows(mfTextWidth, mvMessageFontSize.y+2,mvMessageFontSize,sText,&mvTextRows);
}

//-----------------------------------------------------------------------

void cLuxDemoEnd::OnLeaveContainer(const tString& asNewContainer)
{
	//Turn off viewport and sets
    mpViewport->SetActive(false);
	mpViewport->SetVisible(false);

	mpGuiSet->SetActive(false);

	//Destroy background graphics
	if(mpGfxBackground)
	{
		mpGui->DestroyGfx(mpGfxBackground);
		mpGfxBackground = NULL;
	}
}

//-----------------------------------------------------------------------

void cLuxDemoEnd::Update(float afTimeStep)
{
	uint32_t lUpdate = ohpl_ga7b_de_update(&mS, afTimeStep);

	if(lUpdate & eOhplGa7bDe_Exit)
	{
		gpBase->mpEngine->Exit();
		if(lUpdate & eOhplGa7bDe_Browser)
			cPlatform::OpenBrowserWindow(cString::To16Char(msDestinationURL));
	}
}

//-----------------------------------------------------------------------

void cLuxDemoEnd::Exit(bool abShowWebsite)
{
	ohpl_ga7b_de_exit(&mS, abShowWebsite);

	gpBase->mpInputHandler->ChangeState(eLuxInputState_Null);
	mpGuiSet->SetDrawMouse(false);
}

//-----------------------------------------------------------------------

void cLuxDemoEnd::OnDraw(float afFrameTime)
{
	//////////////////////////////////////////////
	// Background
	mpGuiSet->DrawGfx(mpBlackGfx, mvGuiSetStartPos, mvGuiSetSize);

	if(mpGfxBackground)
	{
		//Draw the image with correct ratio, always filling out height-wise.
		cVector2f vSize = mpGfxBackground->GetImageSize();
		float vRect[2];
		ohpl_ga7b_pm_background_rect(vSize.x, vSize.y, vRect);

		mpGuiSet->DrawGfx(mpGfxBackground, cVector3f(vRect[0],0,5), cVector2f(vRect[1], 600),cColor(1,1));
	}

	//////////////////////////////////////////////
	// Text
	if(mpFontMessage)
	{
		std::vector<float> vRowY(mvTextRows.size());
		ohpl_ga7b_de_message_rows(vRowY.size(), mfTextY, mvMessageFontSize.y, vRowY.data());

		for(size_t i=0; i< mvTextRows.size(); ++i)
		{
			////////////////////////
			// Text row
			if(mvTextRows[i].empty()==false)
			{
				mpGuiSet->DrawFont(mvTextRows[i], mpFontMessage, cVector3f(400.0f-mfTextWidth/2.0f,vRowY[i],10), mvMessageFontSize, mMessageFontColor,eFontAlign_Left);
			}
		}
		mpGuiSet->DrawFont( kTranslate("Demo", "AvailableAt"), mpFontMessage, cVector3f(400.0f,mfAvailableAtY,10), mvAvailableAtFontSize, mAvailableAtFontColor,eFontAlign_Center);
	}


	//////////////////////////////////////////////
	// Fade
	if(mS.mfFadeAlpha > 0)
	{
		mpGuiSet->DrawGfx(mpBlackGfx, mvGuiSetStartPos+cVector3f(0,0,20), mvGuiSetSize, cColor(1, mS.mfFadeAlpha));
	}
}

//-----------------------------------------------------------------------

void cLuxDemoEnd::AppLostInputFocus()
{
	cMusicHandler* pMusHdlr = gpBase->mpEngine->GetSound()->GetMusicHandler();
	pMusHdlr->Pause();
}

//-----------------------------------------------------------------------

void cLuxDemoEnd::AppGotInputFocus()
{
	cMusicHandler* pMusHdlr = gpBase->mpEngine->GetSound()->GetMusicHandler();
	pMusHdlr->Resume();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxDemoEnd::SetUpButtonLabel(cWidgetLabel* apLabel, float* apFadeValue, tGuiCallbackFunc apCallback)
{
	///////////////////////////////////////////
	// Add callbacks
	apLabel->AddCallback(eGuiMessage_OnUpdate, this, kGuiCallback(ButtonLabelOnUpdate));
	apLabel->AddCallback(eGuiMessage_OnDraw, this, kGuiCallback(ButtonLabelOnDraw));
	apLabel->AddCallback(eGuiMessage_MouseEnter, this, kGuiCallback(ButtonLabelOnMouseEnter));
	apLabel->AddCallback(eGuiMessage_MouseLeave, this, kGuiCallback(ButtonLabelOnMouseLeave));
	apLabel->AddCallback(eGuiMessage_MouseUp, this, apCallback);

	// Set up fading helper
	*apFadeValue = 0.0f;
	apLabel->SetUserData(apFadeValue);

	// Set up size, color and other stuff
	apLabel->SetDefaultFontSize(mvButtonFontSize);
	apLabel->SetDefaultFontColor(mButtonFontColor);
	apLabel->SetTextAlign(eFontAlign_Center);
	apLabel->SetAutogenerateSize(true);

	// Center label in position
	cVector3f vPos = apLabel->GetLocalPosition() - cVector3f(apLabel->GetSize().x*0.5f, 0, 0);
    apLabel->SetPosition(vPos);
}

//-----------------------------------------------------------------------

bool cLuxDemoEnd::BuyNowOnPressed(iWidget* apWidget, const cGuiMessageData& aData)
{
	Exit(true);
	
	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxDemoEnd, BuyNowOnPressed);

//-----------------------------------------------------------------------

bool cLuxDemoEnd::ExitOnPressed(iWidget* apWidget, const cGuiMessageData& aData)
{
	Exit(false);
	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxDemoEnd, ExitOnPressed);

//-----------------------------------------------------------------------

bool cLuxDemoEnd::ButtonLabelOnUpdate(iWidget* apWidget, const cGuiMessageData& aData)
{
	bool bMouseOver = apWidget->GetUserValue()==1;
	float* pFade = (float*)apWidget->GetUserData();

	float vBase[4] = { mButtonFontColor.r, mButtonFontColor.g, mButtonFontColor.b, mButtonFontColor.a };
	float vCol[4];
	ohpl_ga7b_de_button_color(pFade, bMouseOver, aData.mfVal, vBase, vCol);

	apWidget->SetDefaultFontColor(cColor(vCol[0], vCol[1], vCol[2], vCol[3]));

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxDemoEnd, ButtonLabelOnUpdate);

//-----------------------------------------------------------------------

bool cLuxDemoEnd::ButtonLabelOnDraw(iWidget* apWidget, const cGuiMessageData& aData)
{
	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxDemoEnd, ButtonLabelOnDraw);

//-----------------------------------------------------------------------

bool cLuxDemoEnd::ButtonLabelOnMouseEnter(iWidget* apWidget, const cGuiMessageData& aData)
{
	apWidget->SetUserValue(1);
	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxDemoEnd, ButtonLabelOnMouseEnter);

//-----------------------------------------------------------------------

bool cLuxDemoEnd::ButtonLabelOnMouseLeave(iWidget* apWidget, const cGuiMessageData& aData)
{
	apWidget->SetUserValue(0);
	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxDemoEnd, ButtonLabelOnMouseLeave);

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

