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

#include "LuxLoadScreenHandler.h"

#include "LuxHelpFuncs.h"
#include "LuxInputHandler.h"
#include "RustLuxMenu2.h"


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxLoadScreenHandler::cLuxLoadScreenHandler() : iLuxUpdateable("LuxLoadScreenHandler")
{
	///////////////////////////////
	//Get Engine
	mpGui = gpBase->mpEngine->GetGui();
	mpScene = gpBase->mpEngine->GetScene();
	mpGraphics = gpBase->mpEngine->GetGraphics();

	///////////////////////////////
	//Setup GUI stuff
	ohpl_ga7b_ls_new(&mS);

	mpGuiSkin = mpGui->CreateSkin("gui_main_menu.skin");
	mpGuiSet = mpGui->CreateSet("LoadScreen",mpGuiSkin);

	mvGuiSetCenterSize = cVector2f(800, 600);
	LuxCalcGuiSetScreenOffset(mvGuiSetCenterSize, mvGuiSetSize, mvGuiSetOffset);
	mvGuiSetStartPos = cVector3f(-mvGuiSetOffset.x,-mvGuiSetOffset.y,0);

	// fixed 800x600 layout: exempt from GuiScale
	mpGuiSet->SetVirtualSize(mvGuiSetSize, -1000,1000, mvGuiSetOffset, true);
	mpGuiSet->SetActive(false);

	///////////////////////////////
	//Create Viewport
	mpViewport = mpScene->CreateViewport();
	mpViewport->SetActive(false);
	mpViewport->SetVisible(false);

	mpViewport->AddGuiSet(mpGuiSet);

	///////////////////////////////
	//Load graphics
	mpWhiteGfx = mpGui->CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Alpha);
	mpCurrentImage = NULL;

	///////////////////////////////
	//Load config
	cConfigFile *pMenuCfg = gpBase->mpMenuCfg;

	mpFontDefault = NULL;

	mvLoadingFontSize = gpBase->mpMenuCfg->GetVector2f("LoadScreen","LoadingFontSize", 0);
	mvTextFontSize = gpBase->mpMenuCfg->GetVector2f("LoadScreen","TextFontSize", 0);
	mfLoadingY = gpBase->mpMenuCfg->GetFloat("LoadScreen","LoadingY", 0);
	mfTextWithImageY = gpBase->mpMenuCfg->GetFloat("LoadScreen","TextWithImageY", 0);
	mfTextAloneY = gpBase->mpMenuCfg->GetFloat("LoadScreen","TextAloneY", 0);
	mfImageY = gpBase->mpMenuCfg->GetFloat("LoadScreen","ImageY", 0);
	mfTextMaxWidth = gpBase->mpMenuCfg->GetFloat("LoadScreen","TextMaxWidth", 0);

	mfFadeOutTime = gpBase->mpMenuCfg->GetFloat("LoadScreen","FadeOutTime", 0);
	mfTextDurationMul = gpBase->mpMenuCfg->GetFloat("LoadScreen","TextDurationMul", 0);
}

//-----------------------------------------------------------------------

cLuxLoadScreenHandler::~cLuxLoadScreenHandler()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::LoadFonts()
{
	tString sFontFile = gpBase->mpMenuCfg->GetString("LoadScreen","DefaultFont","");
	mpFontDefault = LoadFont(sFontFile);
}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::OnStart()
{
	
}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::Reset()
{
	msLoadTextCat = "";
	msLoadTextEntry = "";
	msLoadTextImage = "";
	ohpl_ga7b_ls_reset(&mS);


	msCurrentGameScreenTextEntry = "";

	msGameScreenEndSound = "";

	msCurrentGameScreenImage = "";

	if(mpCurrentImage)
	{
		mpGui->DestroyGfx(mpCurrentImage);
		mpCurrentImage = NULL;
		msCurrentImage = "";
	}
}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::Update(float afTimeStep)
{
	switch(mS.mlState)
	{
	case eLuxLoadScreenState_Game: UpdateGameState(afTimeStep); break;
	}
}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::OnEnterContainer(const tString& asOldContainer)
{
	gpBase->mpInputHandler->ChangeState(eLuxInputState_LoadScreen);

	mpViewport->SetActive(true);
	mpViewport->SetVisible(true);
	mpGuiSet->SetActive(true);
}

void cLuxLoadScreenHandler::OnLeaveContainer(const tString& asNewContainer)
{
	mpViewport->SetActive(false);
	mpViewport->SetVisible(false);
	mpGuiSet->SetActive(false);
}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::OnDraw(float afFrameTime)
{
	switch(mS.mlState)
	{
	case eLuxLoadScreenState_Game: DrawGameState(afFrameTime); break;
	}
}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::GameScreenLoadDone(const tString& asEndSound ,float afLoadTime)
{
	/////////////////////////////
	// Init variables
	bool bHasText = msCurrentGameScreenTextEntry!="";
	float fTimeWanted = 0;
	if(bHasText)
	{
		tWString sLoadText = kTranslate(msLoadTextCat, msCurrentGameScreenTextEntry);
		fTimeWanted = gpBase->mpHelpFuncs->GetStringDuration(sLoadText)*mfTextDurationMul;
	}

	ohpl_ga7b_ls_load_done(&mS, bHasText, fTimeWanted, afLoadTime);
	msGameScreenEndSound = asEndSound;


	/////////////////////////////
	// Change State
	gpBase->mpEngine->GetUpdater()->SetContainer("LoadScreen");

}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::SetupLoadText(const tString& asCat, const tString& asEntry, int alRandomNum, const tString& asImage)
{
	bool bSame = msLoadTextCat == asCat && msLoadTextEntry==asEntry && msLoadTextImage == asImage && mS.mlRandNum == alRandomNum;
	ohpl_ga7b_ls_setup(&mS, bSame, alRandomNum);
	if(bSame)
	{
		return;
	}

	msLoadTextCat = asCat;
	msLoadTextEntry = asEntry;
	msLoadTextImage = asImage;
}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::DrawGameScreen()
{
	msCurrentGameScreenTextEntry = GetGameScreenTextEntry();
	msCurrentGameScreenImage = msLoadTextImage;

	mS.mfLoadingAlpha = 1.0f;
	DrawGameScreen(gpBase->mpHelpFuncs->GetSet());

	gpBase->mpHelpFuncs->DrawSetToScreen();

}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::DrawMenuScreen()
{
	tWString sLoading = kTranslate("General", "Loading");

	cGuiSet *pSet = gpBase->mpHelpFuncs->GetSet();

	//////////////////////
	//Load Image
	cGuiGfxElement *pImage = mpGui->CreateGfxTexture("menu_loading_screen.jpg", eGuiMaterial_Alpha, eTextureType_Rect);

	//////////////////////
	//Draw Image
	cVector2f vImageSize = pImage->GetImageSize();
	pSet->DrawGfx(pImage, cVector3f(ohpl_ga7b_ls_centered(vImageSize.x, 400), ohpl_ga7b_ls_centered(vImageSize.y, 300), 0));
	
	//////////////////////
	//Draw Text
	cVector2f vSize(20);
	cVector3f vPos(400, 530,1);
	pSet->DrawFont(sLoading, mpFontDefault, vPos, vSize, cColor(1,1), eFontAlign_Center);

	gpBase->mpHelpFuncs->DrawSetToScreen();

	//////////////////////
	//Destroy Image
	mpGui->DestroyGfx(pImage);
}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::DrawBlankScreen()
{

}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::ExitPressed()
{
	ohpl_ga7b_ls_exit_pressed(&mS);
	//Exit();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::Exit()
{
	if(mpCurrentImage)
	{
		mpGui->DestroyGfx(mpCurrentImage);
		mpCurrentImage = NULL;
		msCurrentImage = "";
	}

	gpBase->mpEngine->GetUpdater()->SetContainer("Default");
	gpBase->mpInputHandler->ChangeState(eLuxInputState_Game);

	if(mS.mlState == eLuxLoadScreenState_Game)
	{
		gpBase->mpHelpFuncs->PlayGuiSoundData(msGameScreenEndSound, eSoundEntryType_Gui);
	}
}

//-----------------------------------------------------------------------

tString cLuxLoadScreenHandler::GetGameScreenTextEntry()
{
	if(msLoadTextCat == "" || msLoadTextEntry == "") return "";

	tString sEntry = msLoadTextEntry;
	tString sSuffix;
	ohpl_ga7b_ls_text_suffix(&mS, OhplGa7bStrSink, &sSuffix);
	sEntry += sSuffix;
	return sEntry;
}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::LoadCurrentImage(const tString &asImage)
{
	if(mpCurrentImage && msCurrentImage == asImage) return;

	msCurrentImage = asImage;
	mpCurrentImage = mpGui->CreateGfxTexture(asImage, eGuiMaterial_Alpha, eTextureType_Rect);
}

//-----------------------------------------------------------------------


void cLuxLoadScreenHandler::UpdateGameState(float afTimeStep)
{
	if(ohpl_ga7b_ls_update_game(&mS, afTimeStep, mfFadeOutTime))
	{
		Exit();
	}
}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::DrawGameState(float afFrameTime)
{
	mpGuiSet->DrawGfx(mpWhiteGfx, cVector3f(mvGuiSetStartPos.x,mvGuiSetStartPos.y,-1), mvGuiSetSize, cColor(0, 1));

	DrawGameScreen(mpGuiSet);

	mpGuiSet->DrawGfx(mpWhiteGfx, cVector3f(mvGuiSetStartPos.x,mvGuiSetStartPos.y,40), mvGuiSetSize, cColor(0, 1.0f-mS.mfAlpha));
}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler::DrawGameScreen(cGuiSet *apSet)
{
	//////////////////////
	//Load Image
	LoadCurrentImage(msCurrentGameScreenImage);

	//////////////////////
	//Draw Image
	if(mpCurrentImage)
	{
		cVector3f vPos(0,mfImageY, 0.5f);
		vPos.x = ohpl_ga7b_ls_centered(mpCurrentImage->GetImageSize().x, 400);

		apSet->DrawGfx(mpCurrentImage, vPos,-1, cColor(1,1,1,mS.mfLoadingAlpha));
	}

	//////////////////////
	//Draw Text
	if(msCurrentGameScreenTextEntry != "")
	{
		///////////////////
		//Text
		tWStringVec vTextRows;
		mpFontDefault->GetWordWrapRows(mfTextMaxWidth, mvTextFontSize.y, mvTextFontSize, kTranslate(msLoadTextCat, msCurrentGameScreenTextEntry),&vTextRows);

		float fY = ohpl_ga7b_ls_text_y(mS.mfLoadingAlpha, mfTextWithImageY, mfTextAloneY);
		cVector3f vTextPos(400-mfTextMaxWidth/2,fY,1);
		for(size_t i=0; i<vTextRows.size(); ++i)
		{
			apSet->DrawFont(vTextRows[i], mpFontDefault, vTextPos, mvTextFontSize, cColor(1,1));
			vTextPos.y += mvTextFontSize.y+2;
		}

		///////////////////
		//Loading
		tWString sLoading = kTranslate("General", "Loading");
		cVector3f vPos(400, mfLoadingY,1);

		apSet->DrawFont(sLoading, mpFontDefault, vPos, mvLoadingFontSize, cColor(1,0,0,mS.mfLoadingAlpha), eFontAlign_Center);
	}
	//////////////////////
	//Draw loading only
	else
	{
		tWString sLoading = kTranslate("General", "Loading");
		cVector3f vPos(400, ohpl_ga7b_ls_centered(mvLoadingFontSize.y, 300),1);

		apSet->DrawFont(sLoading, mpFontDefault, vPos, mvLoadingFontSize, cColor(1,1), eFontAlign_Center);
	}
}

//-----------------------------------------------------------------------
