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

#include "LuxCompletionCountHandler.h"
#include "RustLuxFx.h"

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxCompletionCountHandler::cLuxCompletionCountHandler() : iLuxUpdateable("LuxCompletionCountHandler")
{
	mlQuestCompletionValue = gpBase->mpGameCfg->GetInt("Quest", "QuestCompletionValue",0);
	mlItemCompletionValue = gpBase->mpGameCfg->GetInt("Quest", "ItemCompletionValue",0);
	mlNoteCompletionValue = gpBase->mpGameCfg->GetInt("Quest", "NoteCompletionValue",0);
	mlDiaryCompletionValue = gpBase->mpGameCfg->GetInt("Quest", "DiaryCompletionValue",0);
	mlFlashbackCompletionValue = gpBase->mpGameCfg->GetInt("Quest", "FlashbackCompletionValue",0);
	mlChestCompletionValue = gpBase->mpGameCfg->GetInt("Quest", "ChestCompletionValue",0);

	mpFont = NULL;
}

//-----------------------------------------------------------------------

cLuxCompletionCountHandler::~cLuxCompletionCountHandler()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxCompletionCountHandler::LoadFonts()
{
	tString sFontFile = gpBase->mpMenuCfg->GetString("CompletionCounter","DefaultFont","");
	mpFont = LoadFont(sFontFile);
}

void cLuxCompletionCountHandler::OnStart()
{
	
}

//-----------------------------------------------------------------------

void cLuxCompletionCountHandler::Reset()
{
	OhplGa9bCompletion c = ToRust();
	ohpl_ga9b_completion_reset(&c);
	FromRust(c);
}

OhplGa9bCompletion cLuxCompletionCountHandler::ToRust()
{
	OhplGa9bCompletion c = { mbActive, mfAlpha, mfDelay, mfNewAmount, mfDisplayedAmount, mfDisableDelay };
	return c;
}

void cLuxCompletionCountHandler::FromRust(const OhplGa9bCompletion &c)
{
	mbActive = c.active != 0;
	mfAlpha = c.alpha;
	mfDelay = c.delay;
	mfNewAmount = c.newAmount;
	mfDisplayedAmount = c.displayed;
	mfDisableDelay = c.disableDelay;
}

//-----------------------------------------------------------------------

void cLuxCompletionCountHandler::Update(float afTimeStep)
{
	OhplGa9bCompletion c = ToRust();
	ohpl_ga9b_completion_update(&c, afTimeStep);
	FromRust(c);
}

//-----------------------------------------------------------------------

void cLuxCompletionCountHandler::OnDraw(float afFrameTime)
{
	return;
	
	float fPrecent = mfDisplayedAmount * 100;
	gpBase->mpGameHudSet->DrawFont(mpFont, cVector3f(400,20,1),21,cColor(1,mfAlpha), eFontAlign_Right,eGuiMaterial_FontNormal,
									_W("%.1f%%"), fPrecent);
	gpBase->mpGameHudSet->DrawFont(mpFont, cVector3f(400,20,1),21,cColor(1,mfAlpha), eFontAlign_Left,eGuiMaterial_FontNormal,
									_W(" %ls"), kTranslate("CompletionCount", "Completed").c_str());
	
}

//-----------------------------------------------------------------------

void cLuxCompletionCountHandler::OnMapExit(cLuxMap *apMap)
{
	Reset();
}

//-----------------------------------------------------------------------

void cLuxCompletionCountHandler::ShowCompletionIncrease(float afPrevius, float afNew, float afDelay)
{
	OhplGa9bCompletion c = ToRust();
	ohpl_ga9b_completion_show(&c, afPrevius, afNew, afDelay);
	FromRust(c);
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
