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

#include "LuxCredits.h"

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

cLuxCredits::cLuxCredits() : iLuxUpdateable("LuxCredits")
{
	///////////////////////////////
	//Get Engine
	mpGraphics = gpBase->mpEngine->GetGraphics();
	mpGui = gpBase->mpEngine->GetGui();
	mpGuiSet = mpGui->CreateSet("Credits", NULL);

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

	mpFontNormal = NULL;
	mpFontHeader = NULL;

	///////////////////////////////
	//Load Data
	mvNormalFontSize = gpBase->mpMenuCfg->GetVector2f("Credits","NormalFontSize",0);
	mvHeaderFontSize = gpBase->mpMenuCfg->GetVector2f("Credits","HeaderFontSize",0);
	mvTheEndFontSize = gpBase->mpMenuCfg->GetVector2f("Credits","TheEndFontSize",0);

	mpBlackGfx = mpGui->CreateGfxFilledRect(cColor(0,1), eGuiMaterial_Alpha);

	mfScrollSpeed = gpBase->mpMenuCfg->GetFloat("Credits","ScrollSpeed",0);
	mfFadeSpeed = gpBase->mpMenuCfg->GetFloat("Credits","FadeSpeed",0);

	ohpl_ga7b_cr_new(&mS);


	///////////////////////////////
	//Setup variables
	Reset();
}

//-----------------------------------------------------------------------

cLuxCredits::~cLuxCredits()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxCredits::LoadFonts()
{
	tString sFontNormal = gpBase->mpMenuCfg->GetString("Credits","NormalFont","");
	tString sFontHeader = gpBase->mpMenuCfg->GetString("Credits","HeaderFont","");
	mpFontNormal = LoadFont(sFontNormal);
	mpFontHeader = LoadFont(sFontHeader);
}

//-----------------------------------------------------------------------

void cLuxCredits::Reset()
{
	msMusic = "";
	mbLoopMusic = false;
	mvTextRows.clear();
	mvRows.clear();

	mbActive = false;

	ohpl_ga7b_cr_reset(&mS);


	mKeyBuffer.Clear();
}

//-----------------------------------------------------------------------

void cLuxCredits::OnEnterContainer(const tString& asOldContainer)
{
	////////////////////////////
	//Set up states and viewport
	gpBase->mpInputHandler->ChangeState(eLuxInputState_Credits);

	mpViewport->SetActive(true);
	mpViewport->SetVisible(true);

	mpGuiSet->SetActive(true);
	mpGui->SetFocus(mpGuiSet);

	///////////////////////////////
	//Play music
	if(msMusic != "")
	{
		cMusicHandler* pMusHandler = gpBase->mpEngine->GetSound()->GetMusicHandler();
		pMusHandler->Play(msMusic, 1.0f, 1.0f, mbLoopMusic, false);
	}
}

//-----------------------------------------------------------------------

void cLuxCredits::OnLeaveContainer(const tString& asNewContainer)
{
	//Turn off viewport and sets
    mpViewport->SetActive(false);
	mpViewport->SetVisible(false);

	mpGuiSet->SetActive(false);
}

//-----------------------------------------------------------------------


static tString DecryptCryptoStringCode(const tString &asEncStr, cBinaryBuffer &aKeyBuffer)
{
	size_t lBuffPos=0;
	tString sOutStr;

	aKeyBuffer.SetPos(0);
	for(size_t i=0; i<asEncStr.size(); ++i)
	{
		sOutStr +=  asEncStr[i] ^ aKeyBuffer.GetChar();
		if(++lBuffPos >= aKeyBuffer.GetSize()) aKeyBuffer.SetPos(0);
	}

	return sOutStr;
}

//-----------------------------------------------------------------------

void cLuxCredits::Update(float afTimeStep)
{
	uint32_t lUpdate = ohpl_ga7b_cr_update(&mS, afTimeStep, mvRows.data(), mvRows.size(), mfScrollSpeed, mfFadeSpeed);

	if(lUpdate & eOhplGa7bCr_StopMusic)
	{
		cMusicHandler *pMusicHandler = gpBase->mpEngine->GetSound()->GetMusicHandler();
		pMusicHandler->Stop(0.1f);
	}

	if(lUpdate & eOhplGa7bCr_Finish)
	{
		gpBase->mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset);
		gpBase->mpLoadScreenHandler->DrawMenuScreen();
		gpBase->mpEngine->GetUpdater()->SetContainer("MainMenu");
	}
}

//-----------------------------------------------------------------------

void cLuxCredits::AddKeyPart(int alKey)
{
	mKeyBuffer.AddInt32(alKey);
}

//-----------------------------------------------------------------------

void cLuxCredits::Setup(const tString& asMusic, bool abLoopMusic, const tString& asTextCat, const tString& asTextEntry, int alEndNum)
{
	msMusic = asMusic;
	mbLoopMusic = abLoopMusic;
	mS.mlEndNum = alEndNum;



	tWString sText = kTranslate(asTextCat, asTextEntry);
	mpFontNormal->GetWordWrapRows(750, 19,17,sText,&mvTextRows);

	mvRows.clear();
	for(size_t i=0; i<mvTextRows.size(); ++i)
	{
		OhplGa7bRow row = { (uint32_t)mvTextRows[i].size(), mvTextRows[i].empty() ? 0u : (uint32_t)mvTextRows[i][0] };
		mvRows.push_back(row);
	}

}

//-----------------------------------------------------------------------

void cLuxCredits::ExitPressed()
{
	//If start scroll fade out, stop music.
	if(ohpl_ga7b_cr_exit_pressed(&mS))
	{
		cMusicHandler *pMusicHandler = gpBase->mpEngine->GetSound()->GetMusicHandler();
		pMusicHandler->Stop(0.1f);
	}
}

//-----------------------------------------------------------------------

void cLuxCredits::OnDraw(float afFrameTime)
{
	//////////////////////////////////////////////
	// Background
	mpGuiSet->DrawGfx(mpBlackGfx, mvGuiSetStartPos, mvGuiSetSize);

	//////////////////////////////////////////////
	// Fade
	if(mS.mfFadeAlpha > 0)
	{
		mpGuiSet->DrawGfx(mpBlackGfx, mvGuiSetStartPos+cVector3f(0,0,20), mvGuiSetSize, cColor(1, mS.mfFadeAlpha));
	}
	
	//////////////////////////////////////////////
	// STATE 0 - CREDITS
	if(mS.mlState == 0 || mS.mlState==1)
	{
		std::vector<OhplGa7bDrawRow> vDraw(mvRows.size());
		size_t lDrawNum = ohpl_ga7b_cr_draw_rows(mS.mfY, mvRows.data(), mvRows.size(), mvNormalFontSize.y+2, mvHeaderFontSize.y+2, vDraw.data(), vDraw.size());

		for(size_t i=0; i< lDrawNum; ++i)
		{
			const OhplGa7bDrawRow &row = vDraw[i];
			//Normal
			if(row.mbHeader==false)
				mpGuiSet->DrawFont(mvTextRows[row.mlRow], mpFontNormal, cVector3f(400,row.mfY,10), mvNormalFontSize, cColor(1,row.mfAlpha),eFontAlign_Center);
			//Header
			else
				mpGuiSet->DrawFont(cString::SubW(mvTextRows[row.mlRow],1), mpFontHeader, cVector3f(400,row.mfY,10), mvHeaderFontSize, cColor(0.8f,row.mfAlpha),eFontAlign_Center);
		}
	}
	//////////////////////////////////////////////
	// STATE 1 - SECRET CODE
	else if(mS.mlState >=2)
	{	
		mpGuiSet->DrawFont(kTranslate("General", "TheEnd"), mpFontHeader, cVector3f(400,290,10),mvTheEndFontSize,cColor(1,1),eFontAlign_Center);

		//Secret code
		int lEndCode = ohpl_ga7b_cr_end_code(mS.mlEndNum, gpBase->mbPTestActivated);
		if(lEndCode >= 0)
		{
			tWString sCode = _W("");
			if(lEndCode==0)	sCode = _W("tyr299");
			if(lEndCode==1)	sCode = _W("odn314");
			if(lEndCode==2)	sCode = _W("lke271");


			mpGuiSet->DrawFont(sCode, mpFontNormal, cVector3f(400,560,10),15,cColor(0.65f,1),eFontAlign_Center);
		}
		else if(gpBase->mbPTestActivated)
		{
			//mKeyBuffer.SetPos(0);
			//int lCRC = mKeyBuffer.GetCRC(kCRCKey);
			//if(lCRC != kKeyBufferCRC)
			//	mpGuiSet->DrawFont(_W("Well done, but far from perfect..."), mpFontNormal, cVector3f(400,560,10),11,cColor(0.65f,1),eFontAlign_Center);
		}
	}
}

//-----------------------------------------------------------------------

void cLuxCredits::AppLostInputFocus()
{
	cMusicHandler* pMusHdlr = gpBase->mpEngine->GetSound()->GetMusicHandler();
	pMusHdlr->Pause();
}

//-----------------------------------------------------------------------

void cLuxCredits::AppGotInputFocus()
{
	cMusicHandler* pMusHdlr = gpBase->mpEngine->GetSound()->GetMusicHandler();
	pMusHdlr->Resume();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------


//-----------------------------------------------------------------------

