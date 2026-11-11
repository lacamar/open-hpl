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

#include "LuxJournal.h"

#include "LuxMapHandler.h"
#include "LuxInputHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxPlayer.h"
#include "LuxHelpFuncs.h"
#include "LuxEffectHandler.h"
#include "LuxProgressLogHandler.h"
#include "LuxMap.h"
#include "LuxInventory.h"
#include "LuxHintHandler.h"

#include "LuxAchievementHandler.h"

static_assert(eUIButton_Primary == 1 && eUIButton_Secondary == 2, "ohpl_ga8_jrn button bits");
static_assert(eUIArrow_Up == 1 && eUIArrow_Right == 2 && eUIArrow_Down == 4 && eUIArrow_Left == 8, "ohpl_ga8_jrn arrow bits");

static tString BackgroundImage(int alState)
{
	OhplGa8Str s = ohpl_ga8_jrn_background(alState);
	return tString((const char*)s.p, s.n);
}

static void OhplGa8Entries(const tString &asName, int alKind, int alNum, tString &asNameEntry, tString &asTextEntry)
{
	ohpl_ga8_jrn_entries(alKind, OhplGa8S(asName), alNum, OhplGa8StrSink, &asNameEntry, &asTextEntry);
}

static void OhplGa8ImageFiles(const tString &asImage, tString &asLarge, tString &asIcon)
{
	ohpl_ga8_jrn_image_files(OhplGa8S(asImage), OhplGa8StrSink, &asLarge, &asIcon);
}

struct cOhplGa8NarrationErrors
{
	size_t mlCount;
};

static void OhplGa8NarrationError(void *apCtx, const unsigned char *apRow, size_t alLen)
{
	cOhplGa8NarrationErrors *pErrors = (cOhplGa8NarrationErrors*)apCtx;
	Error("Voice lines '%s' has bad syntax! Found %d arguments!\n", std::string((const char*)apRow, alLen).c_str(), (int)pErrors->mlCount);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// GOBAL STRUCTS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// WIDGET DATA
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxJournalTextData::Update(float afTimeStep)
{
	mfEffectfAlpha = ohpl_ga8_jrn_hover_alpha(mfEffectfAlpha, mpWidget->GetMouseIsOver() || mpWidget->HasFocus(), afTimeStep);
}

//////////////////////////////////////////////////////////////////////////
// LIST ENTRY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxJournal_ListEntry::AddWidget(iWidget *apWidget)
{
	mlstWidgets.push_back(apWidget);
}

void cLuxJournal_ListEntry::SetVisible(bool abX)
{
	tWidgetListIt it = mlstWidgets.begin(); 
	for(; it != mlstWidgets.end(); ++it)
	{
		iWidget *pWidget = *it;
		pWidget->SetEnabled(abX);
		pWidget->SetVisible(abX);
	}
}

//-----------------------------------------------------------------------

void cLuxJournal_ListPage::SetVisible(bool abX)
{
	for(size_t i=0; i<mvEntries.size(); ++i)
	{
		mvEntries[i].SetVisible(abX);
	}

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// STATE DATA
//////////////////////////////////////////////////////////////////////////

cLuxJournalStateData::cLuxJournalStateData(cLuxJournal *apJournal, eLuxJournalState aState)
{
	mpJournal = apJournal;
	mState = aState;

	mpRootWidget = mpJournal->mpGuiSet->CreateWidgetDummy();

	Reset();
}

//-----------------------------------------------------------------------

void cLuxJournalStateData::Reset()
{
	mfAlpha =0;
}

//-----------------------------------------------------------------------

void cLuxJournalStateData::OnEnter()
{
	if(mState == eLuxJournalState_Diaries || mState == eLuxJournalState_Notes || mState == eLuxJournalState_QuestLog)
	{
		int lIdx = mpJournal->GetNoteListIndex(mState);
		
		mpJournal->SetNoteListPage(mpJournal->mlCurrentNoteListPage[lIdx], mState);	
	}
	
	mpJournal->mpGuiSet->SetDefaultFocusNavWidget(mpJournal->mpWidgetDefaultNav[mState]);
	mpJournal->mpGuiSet->SetFocusedWidget(mpJournal->mpWidgetDefaultNav[mState]);
}

//-----------------------------------------------------------------------

void cLuxJournalStateData::DestroySessionWidgets()
{
	for(tWidgetListIt it = mlstSessionWidgets.begin(); it != mlstSessionWidgets.end(); ++it)
	{
		mpJournal->mpGuiSet->DestroyWidget(*it);
	}	
	mlstSessionWidgets.clear();
}

//-----------------------------------------------------------------------

void cLuxJournalStateData::Update(float afTimeStep)
{
	/////////////////////
	//Alpha
	mfAlpha = ohpl_ga8_jrn_state_alpha(mfAlpha, mState == mpJournal->mCurrentState, afTimeStep);

	/////////////////////
	//Session widgets
	for(tWidgetListIt it = mlstSessionWidgets.begin(); it != mlstSessionWidgets.end(); ++it)
	{
		iWidget *pWidget = *it;
		cColor col = pWidget->GetColorMul();
		col.a = mpJournal->mfAlpha * mfAlpha;
		pWidget->SetColorMul(col);
	}
}

//-----------------------------------------------------------------------

void cLuxJournalStateData::OnDraw(float afFrameTime)
{
	float fAlpha = mfAlpha * mpJournal->mfAlpha;

	///////////////////////
	//Note open
	if(	mState==eLuxJournalState_OpenNote || 
		mState == eLuxJournalState_OpenDiary  || 
		mState == eLuxJournalState_OpenNarratedDiary)
	{
		size_t lPageNum = ohpl_ga8_jrn_page_count(mpJournal->mpPages);
		if(lPageNum==0) return;

		/////////////////////////
		// Header
		cVector3f vHeaderPos(400, mpJournal->mfNoteHeaderStartY, 3);

		mpJournal->mpGuiSet->DrawFont(mpJournal->msHeader, mpJournal->mpFontMenu,vHeaderPos, mpJournal->mfNoteHeaderFontSize, cColor(1,fAlpha),eFontAlign_Center);

		/////////////////////////
		// Text
		if((size_t)mpJournal->mlCurrentNotePage < lPageNum)
		{
			size_t lPage = (size_t)mpJournal->mlCurrentNotePage;
			size_t lRowNum = ohpl_ga8_jrn_row_count(mpJournal->mpPages, lPage);

			cVector3f vPos(400 - mpJournal->mfNoteTextWidth/2, mpJournal->mfNoteTextStartY, 3);

			vPos.y = ohpl_ga8_jrn_page_start_y(lRowNum, mpJournal->mfNoteRowDist);

			for(size_t i=0; i<lRowNum; ++i)
			{
				size_t lLen;
				const unsigned *pRow = ohpl_ga8_jrn_row(mpJournal->mpPages, lPage, i, &lLen);
				mpJournal->mpGuiSet->DrawFont(tWString((const wchar_t*)pRow, lLen), mpJournal->mpFontDefault, vPos, mpJournal->mvNoteFontSize, cColor(1,fAlpha));
				vPos.y += mpJournal->mfNoteRowDist;
			}
		}
	}

	/////////////////////////
	// Background Image
	if(mpJournal->mpStateBackgroundGfx && mpJournal->mCurrentState == mState)
	{
		cVector2f vSize = mpJournal->mpStateBackgroundGfx->GetActiveSize();

		mpJournal->mpGuiSet->DrawGfx(mpJournal->mpStateBackgroundGfx, cVector3f(400-vSize.x/2, 300-vSize.y/2, 1), -1, cColor(1, fAlpha));
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// DIARY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxDiaryContainer::~cLuxDiaryContainer()
{
	STLDeleteAll(mvDiaries);
};

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxJournal::cLuxJournal() : iLuxUpdateable("LuxJournal")
{
	///////////////////////////////
	//Get Engine
	mpGui = gpBase->mpEngine->GetGui();
	mpScene = gpBase->mpEngine->GetScene();
	mpGraphics = gpBase->mpEngine->GetGraphics();

	///////////////////////////////
	//Setup GUI stuff
	mpGuiSkin = mpGui->CreateSkin("gui_main_menu.skin");
	mpGuiSet = mpGui->CreateSet("Inventory", mpGuiSkin);
	mpGuiSet->SetDrawMouse(false);//Init
	
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
	//Load settings
	mvScreenSize = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();

	mfNoteTextWidth = gpBase->mpMenuCfg->GetFloat("Journal","NoteTextWidth",0);
	mlNoteMaxPageRows = gpBase->mpMenuCfg->GetInt("Journal","NoteMaxPageRows",0);
	mvNoteFontSize = gpBase->mpMenuCfg->GetVector2f("Journal","NoteFontSize",0);
	mfNoteRowDist = gpBase->mpMenuCfg->GetFloat("Journal","NoteRowDist",0);
	mfNoteHeaderFontSize = gpBase->mpMenuCfg->GetVector2f("Journal","NoteHeaderFontSize",0);
	mfNoteHeaderStartY = gpBase->mpMenuCfg->GetFloat("Journal","NoteHeaderStartY",0);
	mfNoteTextStartY = gpBase->mpMenuCfg->GetFloat("Journal","NoteTextStartY",0);

	mfBackTextY = 530;
	mvBackTextFontSize = 28;

	mfMaxNoteListY = 450;
	mfNoteListHeaderY = 40;



	///////////////////////////////
	//Load state data
	mvStateData.resize(eLuxJournalState_LastEnum);
	for(size_t i=0; i<mvStateData.size(); ++i)
	{
		mvStateData[i] = hplNew(cLuxJournalStateData, (this, (eLuxJournalState)i) );
	}

	///////////////////////////////
	//Load Data
	mpWhiteGfx = mpGui->CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Alpha);

	cParserVarContainer programVars;
	mpEffectProgram = mpGraphics->CreateGpuProgramFromShaders("InventoryEffect","inventory_screen_effect_vtx.glsl", "inventory_screen_effect_frag.glsl", &programVars);

	mpFontDefault = NULL;
	mpFontMenu = NULL;

	mpPages = ohpl_ga8_jrn_pages_new();
}

//-----------------------------------------------------------------------

cLuxJournal::~cLuxJournal()
{
	STLDeleteAll(mvNotes);
	STLDeleteAll(mvQuestNotes);
	STLDeleteAll(mvDiaryContainers);
	STLDeleteAll(mvStateData);

	ohpl_ga8_jrn_pages_free(mpPages);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxJournal::OnClearFonts()
{
}

void cLuxJournal::LoadFonts()
{
	tString sFontFile = gpBase->mpMenuCfg->GetString("Journal","DefaultFont","");
	tString sFontMenu = gpBase->mpMenuCfg->GetString("Journal","MenuFont","");
	mpFontDefault = LoadFont(sFontFile);
	mpFontMenu = LoadFont(sFontMenu);
}

//-----------------------------------------------------------------------

void cLuxJournal::OnStart()
{
	
}

//-----------------------------------------------------------------------

void cLuxJournal::Reset()
{
	///////////////////////////////
	//Reset variables
	mfAlpha =0;	

	mbActive = false;

	mfMouseOverPulse =0;

	mpStateBackgroundGfx = NULL;

	mbForceInstantExit = false;
	mbOpenedFromInventory = false;

	mlLastReadTextEntry = -1;
	mlLastReadTextType = -1;

	mpVoiceEntry = NULL;

	for(int i=0; i<eLuxJournalState_LastEnum; ++i)
	{
		mpImageForward[i] = NULL;	
		mpImageBackward[i] = NULL;
	}

	for(int i=0; i<3; ++i)
		mlCurrentNoteListPage[i] =0;


	STLDeleteAll(mvNotes);
	STLDeleteAll(mvQuestNotes);
	STLDeleteAll(mvDiaryContainers);

	ResetSessionVars();
}

//-----------------------------------------------------------------------

void cLuxJournal::OnGameStart()
{

}

//-----------------------------------------------------------------------

void cLuxJournal::Update(float afTimeStep)
{
	/////////////////////////
	// Update alpha
	switch(ohpl_ga8_jrn_update_alpha(&mfAlpha, mbActive, mbOpenedFromInventory, afTimeStep))
	{
	case eOhplGa8Exit_ToJournal:
		gpBase->mpInventory->SetEnterFromJournal(true);
		gpBase->mpEngine->GetUpdater()->SetContainer("Inventory");
		break;
	case eOhplGa8Exit_ToDefault:
		gpBase->mpEngine->GetUpdater()->SetContainer("Default");
		gpBase->mpInputHandler->ChangeState(eLuxInputState_Game);
		break;
	}

	/////////////////////////
	// Update Mouse over pulse
	mfMouseOverPulse += afTimeStep;
	

	/////////////////////////
	// Update state data
	for(size_t i=0; i<mvStateData.size(); ++i)
	{
		mvStateData[i]->Update(afTimeStep);
	}

	/////////////////////////
	// Update widget data
	for(tLuxJournalWidgetDataIt it = mlstSessionWidgetData.begin(); it != mlstSessionWidgetData.end(); ++it)
	{
		iLuxJournalWidgetData *pData = *it;

        pData->Update(afTimeStep);
	}

	/////////////////////
	//Check if narration voice is finnished
	if(mCurrentState == eLuxJournalState_OpenNarratedDiary && mbActive)
	{
		cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
		bool bVoiceValid = mpVoiceEntry!=NULL && pSoundHandler->IsValid(mpVoiceEntry,mlVoiceEntryID);
		if(ohpl_ga8_jrn_narration_done(mCurrentState, mbActive, bVoiceValid))
		{
			mpVoiceEntry = NULL;
		
			ExitPressed(false);
		}
	}

	////////////////////////
	//Update extra effects
	gpBase->mpEffectHandler->GetSanityGainFlash()->Update(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxJournal::OnEnterContainer(const tString& asOldContainer)
{
	////////////////////////
	//Create stuff
	CreateBackground();
	CreateGui();

	////////////////////////////
	//Set up states and viewport
	mbActive = true;

	gpBase->mpInputHandler->ChangeState(eLuxInputState_Journal);

	mpViewport->SetActive(true);
	mpViewport->SetVisible(true);
	mpGuiSet->SetActive(true);

#ifdef USE_GAMEPAD
	if(gpBase->mpInputHandler->IsGamepadPresent() == false)
	{
		mpGuiSet->SetDrawMouse(true);
		mpGuiSet->SetMouseMovementEnabled(true);
	}
	else if(gpBase->mpInputHandler->IsGamepadPresent())
	{
		mpGuiSet->SetDrawMouse(false);
		mpGuiSet->SetMouseMovementEnabled(false);
	}
#else
	mpGuiSet->SetDrawMouse(true);
#endif

	mpGuiSet->ResetMouseOver();
	mpGui->SetFocus(mpGuiSet);
	
	//gpBase->mpMapHandler->PauseSoundsAndMusic();

	/////////////////////
	//Effects
	gpBase->mpHelpFuncs->PlayGuiSoundData("journal_open", eSoundEntryType_Gui);

	gpBase->mpMapHandler->GetCurrentMap()->GetPhysicsWorld()->FadeoutAllLoopSounds(1.0f);

	gpBase->mpPlayer->StopTerrorSound();

	///////////////////////////
	//Pause voices and turn down volume on world sounds
	if(mbOpenedFromInventory == false)
	{
		gpBase->mpEffectHandler->GetPlayVoice()->PauseCurrentVoices();
		
		cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
		pSoundHandler->FadeGlobalVolume(0.5f, 0.5f,eSoundEntryType_World,eLuxGlobalVolumeType_GameMenu, false);
	}

	/////////////////////
	//Reset variables
	ResetSessionVars();

	/////////////////////
	//Load main background
	if(BackgroundImage(mCurrentState)!="")
		SetStateBackgroundGfx(BackgroundImage(mCurrentState));
}

//-----------------------------------------------------------------------

void cLuxJournal::OnLeaveContainer(const tString& asNewContainer)
{
	///////////////////////////
	//Pause voices and turn down volume on world sounds
	if(mbOpenedFromInventory == false)
	{
		gpBase->mpEffectHandler->GetPlayVoice()->UnpauseCurrentVoices();

		cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
		pSoundHandler->FadeGlobalVolume(1.0f, 0.5f,eSoundEntryType_World,eLuxGlobalVolumeType_GameMenu, false);
	}
	
	////////////////////////////
	//Set up states
	mpViewport->SetActive(false);
	mpViewport->SetVisible(false);
	mpGuiSet->SetActive(false);

	DestroyGui();
	DestroyBackground();

	mbForceInstantExit = false;
	mbOpenedFromInventory = false;

}

//-----------------------------------------------------------------------

void cLuxJournal::OnDraw(float afFrameTime)
{
	////////////////////////
	//Draw background
	if(mpScreenGfx && mfAlpha<1) 
		mpGuiSet->DrawGfx(mpScreenGfx,mvGuiSetStartPos,mvGuiSetSize);

	if(mpScreenBgGfx)
		mpGuiSet->DrawGfx(mpScreenBgGfx,mvGuiSetStartPos+cVector3f(0,0,0.2f),mvGuiSetSize,cColor(1, mfAlpha));


	for(size_t i=0; i<mvStateData.size(); ++i)
	{
		if(mvStateData[i]->mfAlpha > 0)
			mvStateData[i]->OnDraw(afFrameTime);
	}

	//////////////////////////////////7
	//Fade
	if(mbOpenedFromInventory)
	{
		mpGuiSet->DrawGfx(mpWhiteGfx,mvGuiSetStartPos+cVector3f(0,0,20),mvGuiSetSize,cColor(0, ohpl_ga8_jrn_fade_alpha(mfAlpha)));
	}

	////////////////////////
	//Draw extra effects
	gpBase->mpEffectHandler->GetSanityGainFlash()->DrawFlash(mpGuiSet, afFrameTime);
}

//-----------------------------------------------------------------------

void cLuxJournal::ExitPressed(bool abInstantExit)
{
	int lNewState;
	switch(ohpl_ga8_jrn_exit_pressed(mbActive, abInstantExit, mbForceInstantExit, mCurrentState, mlCurrentNotePage, ohpl_ga8_jrn_page_count(mpPages), &lNewState))
	{
	////////////////////
	//Narrated diary open
	case eOhplGa8JPress_NextPage:
		SetNotePage(lNewState);
		break;
	////////////////////
	//Exit the journal directly
	case eOhplGa8JPress_ExitHint:
		Exit();
		gpBase->mpHelpFuncs->PlayGuiSoundData("journal_close", eSoundEntryType_Gui);

		gpBase->mpHintHandler->Add("RecentlyReadText", kTranslate("Hints", "RecentlyReadText"), -1);
		break;
	case eOhplGa8JPress_Exit:
		Exit();
		gpBase->mpHelpFuncs->PlayGuiSoundData("journal_close", eSoundEntryType_Gui);
		break;
	////////////////////
	//Go to state above.
	case eOhplGa8JPress_ChangeState:
		ChangeState((eLuxJournalState)lNewState);
		break;
	}
}

//-----------------------------------------------------------------------

void cLuxJournal::Exit()
{
	if(mbActive==false) return;

	if(mbForceInstantExit)
	{
		gpBase->mpProgressLogHandler->AddLog(eLuxProgressLogLevel_Low, "Forced Instant Exit in Journal (player finished reading).");
	}

	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	if(mpVoiceEntry && pSoundHandler->IsValid(mpVoiceEntry,mlVoiceEntryID))
	{
		mpVoiceEntry->FadeOut(3.0f);
	}
	mpVoiceEntry = NULL;

	gpBase->mpInputHandler->ChangeState(eLuxInputState_Null);

	mpGuiSet->SetDrawMouse(false);

	mpGui->SetFocus(NULL);

	mbActive = false;
}

//-----------------------------------------------------------------------

cLuxNote* cLuxJournal::AddNote(const tString& asNameAndTextEntry, const tString& asImage)
{
	cLuxNote *pNote = hplNew( cLuxNote, () );

	OhplGa8Entries(asNameAndTextEntry, 0, 0, pNote->msNameEntry, pNote->msTextEntry);
	OhplGa8ImageFiles(asImage, pNote->msImageFile, pNote->msIconFile);

    mvNotes.push_back(pNote);

	switch(ohpl_ga8_jrn_archivist(OhplGa8S(gpBase->msGameName), mvNotes.size()))
	{
	case eOhplGa8Archivist_Tdd:
		if(gpBase->mpMainConfig->GetBool("Main", "MasterArchivis_Justine", false))
		{
			gpBase->mpAchievementHandler->UnlockAchievement(eLuxAchievement_MasterArchivist);
		}

		gpBase->mpMainConfig->SetBool("Main", "MasterArchivis_TDD", true);
		gpBase->mpMainConfig->Save();
		break;
	case eOhplGa8Archivist_Justine:
		if(gpBase->mpMainConfig->GetBool("Main", "MasterArchivis_TDD", false))
		{
			gpBase->mpAchievementHandler->UnlockAchievement(eLuxAchievement_MasterArchivist);
		}

		gpBase->mpMainConfig->SetBool("Main", "MasterArchivis_Justine", true);
		gpBase->mpMainConfig->Save();
		break;
	}

		return pNote;
}

//-----------------------------------------------------------------------

cLuxDiary* cLuxJournal::AddDiary(const tString& asNameAndTextEntry, const tString& asImage, int &alCurrentEntryIdx)
{
	cLuxDiary *pDiary = hplNew( cLuxDiary, () );
	
    cLuxDiaryContainer *pContainer = CreateDiaryContainer(asNameAndTextEntry);
	int lNum = (int)pContainer->mvDiaries.size()+1;

	alCurrentEntryIdx = lNum;

	OhplGa8Entries(asNameAndTextEntry, 1, lNum, pDiary->msNameEntry, pDiary->msTextEntry);
	OhplGa8ImageFiles(asImage, pDiary->msImageFile, pDiary->msIconFile);

	pContainer->mvDiaries.push_back(pDiary);

	return pDiary;
}

//-----------------------------------------------------------------------

bool cLuxJournal::AddQuestNote(const tString& asName, const tString& asNameAndTextEntry)
{
	for(size_t i=0; i<mvQuestNotes.size(); ++i)
	{
		if(mvQuestNotes[i]->msName == asName) return false;
	}

	cLuxQuestNote *pQuestNote = hplNew( cLuxQuestNote, () );

	pQuestNote->msName = asName;
	OhplGa8Entries(asNameAndTextEntry, 2, 0, pQuestNote->msNameEntry, pQuestNote->msTextEntry);
	pQuestNote->mbActive = true;

	mvQuestNotes.push_back(pQuestNote);

	return true;
}

//-----------------------------------------------------------------------

bool cLuxJournal::DisableQuestNote(const tString& asName)
{
	cLuxQuestNote *pQuest = GetQuestNote(asName);
	switch(ohpl_ga8_jrn_disable_quest(pQuest!=NULL, pQuest ? pQuest->mbActive : false))
	{
	case eOhplGa8Quest_Disabled:
		pQuest->mbActive = false;
		return true;
	case eOhplGa8Quest_AlreadyOff:
		return false;
	}

	Error("Quest '%s' does not exist!\n", asName.c_str());
	return false;

		/*std::vector<cLuxQuestNote*>::iterator it = mvQuestNotes.begin();
	for(; it != mvQuestNotes.end(); ++it)
	{
		cLuxQuestNote* pQuest = *it;
		if(pQuest->msName == asName)
		{
			hplDelete(pQuest);
            mvQuestNotes.erase(it);
			return;
		}
	}*/
}

//-----------------------------------------------------------------------

cLuxQuestNote* cLuxJournal::GetQuestNote(const tString& asName)
{
	for(size_t i=0; i<mvQuestNotes.size(); ++i)
	{
		if(mvQuestNotes[i]->msName == asName)
		{
			return mvQuestNotes[i];
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------

void cLuxJournal::ChangeState(eLuxJournalState aState)
{
	mCurrentState = aState;
	
	if(BackgroundImage(aState) != "")
		SetStateBackgroundGfx(BackgroundImage(aState));

	for(size_t i=0; i<mvStateData.size(); ++i)
	{
		cWidgetDummy *pRoot = mvStateData[i]->mpRootWidget;
		if(i == mCurrentState)	pRoot->SetVisible(true);
		else					pRoot->SetVisible(false);
	}

	mvStateData[mCurrentState]->OnEnter();
}

//-----------------------------------------------------------------------

void cLuxJournal::OpenNote(cLuxNote *apNote, bool abNarration)
{
	///////////////////////////
	// Get the entry number of the diary
	mlLastReadTextType =0;
	mlLastReadTextCat =-1;
	std::vector<size_t> vNotePtrs(mvNotes.size());
	for(size_t i=0; i<mvNotes.size(); ++i) vNotePtrs[i] = (size_t)mvNotes[i];
	mlLastReadTextEntry = ohpl_ga8_jrn_find_index(vNotePtrs.data(), vNotePtrs.size(), (size_t)apNote);

	///////////////////////////
	// Open the diary
	if(abNarration)
	{
		ChangeState(eLuxJournalState_OpenNarratedDiary);
		LoadNarrationText(kTranslate("Journal", apNote->msNameEntry), kTranslate("Journal", apNote->msTextEntry) );
	}
	else
	{
		ChangeState(eLuxJournalState_OpenNote);
		LoadText(kTranslate("Journal", apNote->msNameEntry), kTranslate("Journal", apNote->msTextEntry) );
	}
	
	SetStateBackgroundGfx(apNote->msImageFile);
}

//-----------------------------------------------------------------------

void cLuxJournal::OpenDiary(cLuxDiary *apDiary, bool abNarration)
{
	SetDiaryAsLastRead(apDiary);
		
	///////////////////////////
	// Open the diary
	if(abNarration)
	{
		ChangeState(eLuxJournalState_OpenNarratedDiary);
		LoadNarrationText(kTranslate("Journal", apDiary->msNameEntry), kTranslate("Journal", apDiary->msTextEntry) );
	}
	else
	{
		ChangeState(eLuxJournalState_OpenDiary);
		LoadText(kTranslate("Journal", apDiary->msNameEntry), kTranslate("Journal", apDiary->msTextEntry) );
	}
	
	SetStateBackgroundGfx(apDiary->msImageFile);
	
}
//-----------------------------------------------------------------------

void cLuxJournal::SetDiaryAsLastRead(cLuxDiary *apDiary)
{
	///////////////////////////
	// Get the cat and entry of the diary
	mlLastReadTextType = 1;
	std::vector<size_t> vCounts(mvDiaryContainers.size());
	std::vector<size_t> vDiaryPtrs;
	for(size_t cat=0; cat<mvDiaryContainers.size(); ++cat)
	{
		cLuxDiaryContainer* pCont = mvDiaryContainers[cat];
		vCounts[cat] = pCont->mvDiaries.size();
		for(size_t entry=0; entry<pCont->mvDiaries.size(); ++entry) vDiaryPtrs.push_back((size_t)pCont->mvDiaries[entry]);
	}
	ohpl_ga8_jrn_find_diary(vCounts.data(), vCounts.size(), vDiaryPtrs.data(), vDiaryPtrs.size(), (size_t)apDiary, &mlLastReadTextCat, &mlLastReadTextEntry);
}

//-----------------------------------------------------------------------

void cLuxJournal::OpenLastReadText()
{
	std::vector<size_t> vCounts(mvDiaryContainers.size());
	for(size_t cat=0; cat<mvDiaryContainers.size(); ++cat) vCounts[cat] = mvDiaryContainers[cat]->mvDiaries.size();

	switch(ohpl_ga8_jrn_last_read(mlLastReadTextType, mlLastReadTextCat, mlLastReadTextEntry, mvNotes.size(), vCounts.data(), vCounts.size()))
	{
	///////////////////
	//Note
	case 1:
		SetForceInstantExit(true);
		gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
		OpenNote(mvNotes[mlLastReadTextEntry], false);
		break;
	///////////////////
	//Diary
	case 2:
		SetForceInstantExit(true);
		gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
		OpenDiary(mvDiaryContainers[mlLastReadTextCat]->mvDiaries[mlLastReadTextEntry], false);
		break;
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxDiaryContainer* cLuxJournal::CreateDiaryContainer(const tString& asType)
{
	for(size_t i=0; i<mvDiaryContainers.size(); ++i)
	{
		cLuxDiaryContainer *pCont = mvDiaryContainers[i];
		if(pCont->msType==asType) return pCont;
	}

	cLuxDiaryContainer *pCont = hplNew(cLuxDiaryContainer, () );
	pCont->msType = asType;
	mvDiaryContainers.push_back(pCont);
	return pCont;
}

//-----------------------------------------------------------------------

void cLuxJournal::SetupLabel(cWidgetLabel *apLabel, const cVector2f& avSize, int alIdx, eLuxJournalState aState, iFontData *apFont, eFontAlign aFontAlign)
{
	if(apFont==NULL) apFont = mpFontMenu;

	apLabel->SetDefaultFontColor(cColor(1,1));
	apLabel->SetDefaultFontSize(avSize);
	apLabel->SetTextAlign(aFontAlign);
	apLabel->SetUserValue(alIdx);
	apLabel->SetDefaultFontType(apFont);
	AddSessionWidget(aState, apLabel);

	cLuxJournalTextData *pData = hplNew(cLuxJournalTextData, (apLabel, aState));
	apLabel->SetUserData(pData);
	mlstSessionWidgetData.push_back(pData);
}

//-----------------------------------------------------------------------

void cLuxJournal::SetupImage(cWidgetImage *apImage, int alIdx, eLuxJournalState aState)
{
	apImage->SetUserValue(alIdx);
	AddSessionWidget(aState, apImage);

	cLuxJournalTextData *pData = hplNew(cLuxJournalTextData, (apImage, aState));
	apImage->SetUserData(pData);
	mlstSessionWidgetData.push_back(pData);
}

//-----------------------------------------------------------------------

void cLuxJournal::SetupNavigationWidgets(eLuxJournalState aState, int alListIndex, int alForwardIndex, int alBackwardIndex, cWidgetDummy *apRoot)
{
	cVector2f vSize(65,46);
	float vNavPos[3], fBackX;
	ohpl_ga8_jrn_nav_positions(mfNoteTextWidth, mfNoteTextStartY, mlNoteMaxPageRows, mfNoteRowDist, vNavPos, &fBackX);
	cVector3f vPos = cVector3f(vNavPos[0], vNavPos[1], vNavPos[2]);

	iWidget* pFwd = NULL;
	iWidget* pRet = NULL;
	iWidget* pBck = NULL;

	//Forward
	mpImageForward[aState] = mpGuiSet->CreateWidgetImage("journal/note_forward.tga",vPos,vSize,eGuiMaterial_Alpha,false,apRoot);
	mpImageForward[aState]->AddCallback(eGuiMessage_OnDraw,this, kGuiCallback(ImageButtonOnDraw));
	mpImageForward[aState]->AddCallback(eGuiMessage_MouseDown,this, kGuiCallback(NoteArrowClick));
	SetupImage(mpImageForward[aState], alForwardIndex, aState);
	pFwd = mpImageForward[aState];

	//Return
	if(aState == eLuxJournalState_Diaries || aState == eLuxJournalState_Notes || aState == eLuxJournalState_QuestLog)
	{
		cWidgetLabel* pLabel = mpGuiSet->CreateWidgetLabel(cVector3f(350,mfBackTextY,3) , cVector2f(100, 24),kTranslate("Journal","MainBack"), apRoot);
		SetupLabel(pLabel,mvBackTextFontSize, aState, aState, mpFontMenu);
		pLabel->AddCallback(eGuiMessage_MouseDown,this, kGuiCallback(NoteBackClick));
		pLabel->AddCallback(eGuiMessage_OnDraw,this, kGuiCallback(ListTextOnDraw));
		pLabel->AddCallback(eGuiMessage_UIButtonPress, this, kGuiCallback(UIListenerJournalPress));

		pRet = pLabel;
	}
	else
	{
		cWidgetImage* pImage = mpGuiSet->CreateWidgetImage("journal/note_return.tga",0,-1,eGuiMaterial_Alpha,false,apRoot);

		if(aState != eLuxJournalState_OpenNarratedDiary)
		{
			pImage->SetPosition(cVector3f(400 - pImage->GetSize().x/2.0f, mfBackTextY, 3));
			pImage->AddCallback(eGuiMessage_OnDraw,this, kGuiCallback(ImageButtonOnDraw));
			pImage->AddCallback(eGuiMessage_MouseDown,this, kGuiCallback(NoteBackClick));
			pImage->AddCallback(eGuiMessage_UIButtonPress, this, kGuiCallback(UIListenerJournalPress));
			SetupImage(pImage, aState, aState);
		}
		else
		{
			pImage->SetVisible(false);
		}

		pRet = pImage;
	}

	pRet->AddCallback(eGuiMessage_UIButtonPress, this, kGuiCallback(NoteBackUIButtonPress));
	pRet->SetGlobalUIInputListener(true);
	mpWidgetReturn[aState] = pRet;


	//Backward
	vPos.x = fBackX;
	mpImageBackward[aState] = mpGuiSet->CreateWidgetImage("journal/note_backward.tga",vPos,vSize,eGuiMaterial_Alpha,false,apRoot);
	mpImageBackward[aState]->AddCallback(eGuiMessage_OnDraw,this, kGuiCallback(ImageButtonOnDraw));
	mpImageBackward[aState]->AddCallback(eGuiMessage_MouseDown,this, kGuiCallback(NoteArrowClick));
	SetupImage(mpImageBackward[aState], alBackwardIndex, aState);

	pBck = mpImageBackward[aState];


	cWidgetDummy* pUIListener = mpGuiSet->CreateWidgetDummy(0, apRoot);
	pUIListener->SetUserValue(aState);
	pUIListener->SetGlobalUIInputListener(true);
	pUIListener->AddCallback(eGuiMessage_UIArrowPress, this, kGuiCallback(UIListenerArrowPress));

	////////////////////////////////////////////////////
	// Set up focus navigation
	{
		/*
		pBck->SetFocusNavigation(eUIArrow_Right, pRet);
		pRet->SetFocusNavigation(eUIArrow_Left, pBck);
		pRet->SetFocusNavigation(eUIArrow_Right, pFwd);
		pFwd->SetFocusNavigation(eUIArrow_Left, pRet);

		if(alListIndex!=-1)
		{
			for(size_t i=0; i<mvNoteListPages[alListIndex].size(); ++i)
			{
				cLuxJournal_ListPage *pListPage = &mvNoteListPages[alListIndex][i];
				cLuxJournal_ListEntry* pLastEntry = &pListPage->mvEntries.back();
				
				iWidget* pLabel = pLastEntry->mlstWidgets.front();
		
				pLabel->SetFocusNavigation(eUIArrow_Down, pRet);
			}
		}
		*/
	}
}

//-----------------------------------------------------------------------

void cLuxJournal::AddSessionWidget(eLuxJournalState aState, iWidget *apWidget)
{
	mvStateData[aState]->mlstSessionWidgets.push_back(apWidget);
}

//-----------------------------------------------------------------------

void cLuxJournal::ResetSessionVars()
{
	SetStateBackgroundGfx("");
	mCurrentState = eLuxJournalState_Main;

    mlCurrentNotePage =0;
	
	for(size_t i=0; i<mvStateData.size(); ++i)
	{
		mvStateData[i]->Reset();
	}
}

//-----------------------------------------------------------------------

void cLuxJournal::SetStateBackgroundGfx(const tString& asFile)
{
	if(mpStateBackgroundGfx)
	{
		mpGui->DestroyGfx(mpStateBackgroundGfx);
		mpStateBackgroundGfx = NULL;
	}
	
	if(asFile != "")
		mpStateBackgroundGfx = mpGui->CreateGfxTexture(asFile,eGuiMaterial_Alpha, eTextureType_Rect);
}

//-----------------------------------------------------------------------

void cLuxJournal::LoadText(const tWString &asName, const tWString &asText)
{
	msHeader = asName;

	tWStringVec sTempRows;
	mpFontDefault->GetWordWrapRows(mfNoteTextWidth, 20, 20,asText, &sTempRows);

	std::vector<const unsigned*> vRows(sTempRows.size());
	std::vector<size_t> vLens(sTempRows.size());
	for(size_t i=0; i<sTempRows.size(); ++i)
	{
		vRows[i] = (const unsigned*)sTempRows[i].data();
		vLens[i] = sTempRows[i].size();
	}
	ohpl_ga8_jrn_load_text(mpPages, vRows.data(), vLens.data(), vRows.size(), mlNoteMaxPageRows);

	SetNotePage(0);
}

//-----------------------------------------------------------------------

void cLuxJournal::LoadNarrationText(const tWString &asName ,const tWString &asText)
{
	msHeader = asName;

	tWStringVec sDraftRows;
	mpFontDefault->GetWordWrapRows(mfNoteTextWidth, 20, 20,asText, &sDraftRows);

	std::vector<const unsigned*> vRows(sDraftRows.size());
	std::vector<size_t> vLens(sDraftRows.size());
	for(size_t i=0; i<sDraftRows.size(); ++i)
	{
		vRows[i] = (const unsigned*)sDraftRows[i].data();
		vLens[i] = sDraftRows[i].size();
	}
	cOhplGa8NarrationErrors errors;
	errors.mlCount = 0;
	ohpl_ga8_jrn_load_narration(mpPages, vRows.data(), vLens.data(), vRows.size(), &errors.mlCount, OhplGa8NarrationError, &errors);

	SetNotePage(0);
}

//-----------------------------------------------------------------------

void cLuxJournal::SetNotePage(int alPageNum)
{
	mlCurrentNotePage = alPageNum;

	///////////////////////////////
	//No arrows for narrated diary.
	//Play voice instead!
	if(mCurrentState == eLuxJournalState_OpenNarratedDiary)
	{
		if(ohpl_ga8_jrn_narrated_plays(alPageNum, ohpl_ga8_jrn_page_count(mpPages)))
		{
			cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
			if(mpVoiceEntry && pSoundHandler->IsValid(mpVoiceEntry, mlVoiceEntryID))
			{
				mpVoiceEntry->FadeOut(3.0f);
			}

			OhplGa8Str sVoice = ohpl_ga8_jrn_voice(mpPages, alPageNum);
			mpVoiceEntry = pSoundHandler->PlayGuiStream(tString((const char*)sVoice.p, sVoice.n),false, 1.0f);
			if(mpVoiceEntry)
				mlVoiceEntryID = mpVoiceEntry->GetId();
		}

		return;
	}

	///////////////////////////////
	//Set up arrows
	int lX =  mCurrentState;
	int lBack, lFwd;
	ohpl_ga8_jrn_note_arrows(mlCurrentNotePage, ohpl_ga8_jrn_page_count(mpPages), &lBack, &lFwd);
	mpImageBackward[lX]->SetEnabled(lBack != 0);
	mpImageBackward[lX]->SetVisible(lBack != 0);

	mpImageForward[lX]->SetEnabled(lFwd != 0);
	mpImageForward[lX]->SetVisible(lFwd != 0);
}

//-----------------------------------------------------------------------

int cLuxJournal::GetNoteListIndex(eLuxJournalState aState)
{
	return ohpl_ga8_jrn_list_index(aState);
}

//-----------------------------------------------------------------------

void cLuxJournal::SetNoteListPage(int alPageNum, eLuxJournalState aState)
{
	int lIdx = GetNoteListIndex(aState);

	mlCurrentNoteListPage[lIdx] = alPageNum;


	std::vector<cLuxJournal_ListPage> &vPages = mvNoteListPages[lIdx];
	int lNoPages, lBack, lFwd, lDefaultReturn;
	ohpl_ga8_jrn_list_page(mlCurrentNoteListPage[lIdx], vPages.size(), aState, &lNoPages, &lBack, &lFwd, &lDefaultReturn);
	if(lNoPages)
	{
		//No pages, disable both arrows.
		mpImageBackward[aState]->SetEnabled(false);
		mpImageBackward[aState]->SetVisible(false);

		mpImageForward[aState]->SetEnabled(false);
		mpImageForward[aState]->SetVisible(false);

		mpWidgetDefaultNav[aState] = mpWidgetReturn[aState];

		return;
	}


	///////////////////////////////
	//Set only current page visible
	for(size_t i=0; i<vPages.size(); ++i)
	{
		vPages[i].SetVisible(mlCurrentNoteListPage[lIdx] == i);
	}

	//////////////////////////////////////////////////////////////////
	// Set up last touches in focus navigation
	iWidget* pFirstNote = vPages[mlCurrentNoteListPage[lIdx]].mvEntries.front().mlstWidgets.front();
	iWidget* pLastNote = vPages[mlCurrentNoteListPage[lIdx]].mvEntries.back().mlstWidgets.front();

	if(lDefaultReturn==0)
		mpWidgetDefaultNav[aState] = pFirstNote;
	else
		mpWidgetDefaultNav[aState] = mpWidgetReturn[aState];

	//mpWidgetReturn[aState]->SetFocusNavigation(eUIArrow_Up, pLastNote);
	//mpImageBackward[aState]->SetFocusNavigation(eUIArrow_Up, pLastNote);
	//mpImageForward[aState]->SetFocusNavigation(eUIArrow_Up, pLastNote);

	mpGuiSet->SetDefaultFocusNavWidget(mpWidgetDefaultNav[aState]);
	mpGuiSet->SetFocusedWidget(mpWidgetDefaultNav[aState]);
	
	///////////////////////////////
	//Set up arrows
	mpImageBackward[aState]->SetEnabled(lBack != 0);
	mpImageBackward[aState]->SetVisible(lBack != 0);

	mpImageForward[aState]->SetEnabled(lFwd != 0);
	mpImageForward[aState]->SetVisible(lFwd != 0);
}



//-----------------------------------------------------------------------

void cLuxJournal::CreateGui()
{
	CreateMainGui();
	CreateNotesGui();
	CreateDiariesGui();
	CreateOpenNoteGui();
	CreateQuestNotesGui();

	ChangeState(eLuxJournalState_Main);
}

//-----------------------------------------------------------------------

void cLuxJournal::DestroyGui()
{
	for(size_t i=0; i<mvStateData.size(); ++i)
	{
		mvStateData[i]->DestroySessionWidgets();
	}
    
	STLDeleteAll(mlstSessionWidgetData);

	SetStateBackgroundGfx("");
}

//-----------------------------------------------------------------------

void cLuxJournal::CreateMainGui()
{
	cWidgetDummy *pState = mvStateData[eLuxJournalState_Main]->mpRootWidget;
	
	/////////////////////////
	// State select
	cVector3f vStartPos = cVector3f(400, 240, 1);
	cWidgetLabel *pLabel = NULL;
	float fSize = 30;

	std::vector<iWidget*> vLabels;

	//Notes
	pLabel = mpGuiSet->CreateWidgetLabel(cVector3f(300, 200, 3),cVector2f(200, 25),kTranslate("Journal","Notes"), pState);
	SetupLabel(pLabel,fSize,0, eLuxJournalState_Main);
	pLabel->AddCallback(eGuiMessage_MouseDown,this, kGuiCallback(MainMenuTextClick));
	pLabel->AddCallback(eGuiMessage_OnDraw,this, kGuiCallback(MainMenuTextOnDraw));
	vLabels.push_back(pLabel);

	mpWidgetDefaultNav[eLuxJournalState_Main] = pLabel;
	
	//Diaries
	pLabel = mpGuiSet->CreateWidgetLabel(cVector3f(300, 250, 3),cVector2f(200, 25),kTranslate("Journal","Diaries"), pState);
	SetupLabel(pLabel,fSize,1, eLuxJournalState_Main);
	pLabel->AddCallback(eGuiMessage_MouseDown,this, kGuiCallback(MainMenuTextClick));
	pLabel->AddCallback(eGuiMessage_OnDraw,this, kGuiCallback(MainMenuTextOnDraw));
	vLabels.push_back(pLabel);
	
	//Quest Log
	pLabel = mpGuiSet->CreateWidgetLabel(cVector3f(300, 300, 3),cVector2f(200, 25),kTranslate("Journal","Quest Log"), pState);
	SetupLabel(pLabel,fSize,2, eLuxJournalState_Main);
	pLabel->AddCallback(eGuiMessage_MouseDown,this, kGuiCallback(MainMenuTextClick));
	pLabel->AddCallback(eGuiMessage_OnDraw,this, kGuiCallback(MainMenuTextOnDraw));
	vLabels.push_back(pLabel);

	//Back
	pLabel = mpGuiSet->CreateWidgetLabel(cVector3f(300, mfBackTextY, 3),cVector2f(200, 25),kTranslate("Journal","MainBack"), pState);
	SetupLabel(pLabel,mvBackTextFontSize, 3, eLuxJournalState_Main);
	pLabel->AddCallback(eGuiMessage_MouseDown,this, kGuiCallback(MainMenuTextClick));
	pLabel->AddCallback(eGuiMessage_OnDraw,this, kGuiCallback(MainMenuTextOnDraw));
	vLabels.push_back(pLabel);


	//////////////////////////////////////////////////////////////////////
	// Set up navigation / Label callbacks
	{
		for(size_t i=0; i<vLabels.size(); ++i)
		{
			iWidget* pWidget = vLabels[i];

			pWidget->AddCallback(eGuiMessage_MouseDown,this, kGuiCallback(MainMenuTextClick));
			pWidget->AddCallback(eGuiMessage_UIButtonPress,this, kGuiCallback(MainMenuUIButtonPress));
			pWidget->AddCallback(eGuiMessage_OnDraw,this, kGuiCallback(MainMenuTextOnDraw));



			int lPrev = i-1;
			int lNext = i+1;
			
			if(lPrev>=0)
				pWidget->SetFocusNavigation(eUIArrow_Up, vLabels[lPrev]);
			if(lNext<vLabels.size())
				pWidget->SetFocusNavigation(eUIArrow_Down,vLabels[lNext]);
		}

		mpWidgetDefaultNav[eLuxJournalState_Main] = vLabels[0];
	}
}

//-----------------------------------------------------------------------


void cLuxJournal::CreateNotesGui()
{
	cWidgetDummy *pRoot = mvStateData[eLuxJournalState_Notes]->mpRootWidget;

	/////////////////////////
	// vars setup
	cVector3f vStartPos = cVector3f(250, 100, 3);
	cWidgetLabel *pLabel = NULL;
	cWidgetImage *pImage = NULL;
	eLuxJournalState state = eLuxJournalState_Notes;
	int lListIdx = 0;


	mvNoteListPages[lListIdx].clear();

	/////////////////////////////
	// Header
	pLabel = mpGuiSet->CreateWidgetLabel(cVector3f(250, mfNoteListHeaderY, 3),cVector2f(300, 32),kTranslate("Journal","Notes"), pRoot);
	SetupLabel(pLabel,28, 0, eLuxJournalState_Notes);


	/////////////////////////////
	// No Notes
	if(mvNotes.empty())
	{
		vStartPos.y = 270;
		pLabel = mpGuiSet->CreateWidgetLabel(vStartPos,cVector2f(300, 24),kTranslate("Journal","NotesEmpty"), pRoot);
		SetupLabel(pLabel,24, 0, eLuxJournalState_Notes,mpFontDefault);
	}
	/////////////////////////////
	// Has notes
	else
	{
		std::vector<size_t> vEntryPage(mvNotes.size());
		std::vector<float> vEntryY(mvNotes.size());
		size_t lPageNum = ohpl_ga8_jrn_layout_list(mvNotes.size(), vEntryPage.data(), vEntryY.data());
		for(size_t p=0; p<lPageNum; ++p) mvNoteListPages[lListIdx].push_back(cLuxJournal_ListPage());

		for(size_t i=0; i<mvNotes.size(); ++i)
		{
			cLuxNote *pNote = mvNotes[i];

			//Check if new page is needed!
			cLuxJournal_ListPage *pListPage = &mvNoteListPages[lListIdx][vEntryPage[i]];
			vStartPos.y = vEntryY[i];

			//Add label
			pLabel = mpGuiSet->CreateWidgetLabel(vStartPos,cVector2f(300, 24),kTranslate("Journal",pNote->msNameEntry), pRoot);
			SetupLabel(pLabel,24, (int)i, eLuxJournalState_Notes, mpFontDefault, eFontAlign_Left);
			pLabel->AddCallback(eGuiMessage_MouseDown,this, kGuiCallback(NoteTextClick));
			pLabel->AddCallback(eGuiMessage_OnDraw,this, kGuiCallback(ListTextOnDraw));

			pLabel->AddCallback(eGuiMessage_UIButtonPress,this, kGuiCallback(JournalItemUIButtonPress));

			//Add icon
			pImage = mpGuiSet->CreateWidgetImage(pNote->msIconFile, vStartPos + cVector3f(-25,2,-0.05f), cVector2f(20, 20),eGuiMaterial_Alpha, false, pRoot);
			SetupImage(pImage,(int)i,eLuxJournalState_Notes);

			//Add list entry
			pListPage->mvEntries.push_back(cLuxJournal_ListEntry());
			pListPage->mvEntries.back().AddWidget(pLabel);
			pListPage->mvEntries.back().AddWidget(pImage);
		}
	}

	for(size_t i=0; i<mvNoteListPages[lListIdx].size(); ++i)
	{
		cLuxJournal_ListPage *pListPage = &mvNoteListPages[lListIdx][i];
		for(size_t j=0; j<pListPage->mvEntries.size(); ++j)
		{
			cLuxJournal_ListEntry* pListEntry = &pListPage->mvEntries[j];
			int lPrev = j-1;
			int lNext = j+1;

			iWidget* pLabel = pListEntry->mlstWidgets.front();
			
			if(lPrev>=0)
				pLabel->SetFocusNavigation(eUIArrow_Up, pListPage->mvEntries[lPrev].mlstWidgets.front());
			if(lNext<pListPage->mvEntries.size())
				pLabel->SetFocusNavigation(eUIArrow_Down, pListPage->mvEntries[lNext].mlstWidgets.front());
		}
	}

	////////////////////////
	// Navigation
	SetupNavigationWidgets(state, lListIdx, 2, 3, pRoot);


	////////////////////////
	// Set start page
	mlCurrentNoteListPage[lListIdx] = ohpl_ga8_jrn_start_page(mvNoteListPages[lListIdx].size());
}

//-----------------------------------------------------------------------

void cLuxJournal::CreateDiariesGui()
{
	cWidgetDummy *pRoot = mvStateData[eLuxJournalState_Diaries]->mpRootWidget;
	
	/////////////////////////
	// vars setup
	cVector3f vStartPos = cVector3f(250, 100, 3);
	cWidgetLabel *pLabel = NULL;
	cWidgetImage *pImage = NULL;
	eLuxJournalState state = eLuxJournalState_Diaries;
	int lListIdx = 1;

	mvNoteListPages[lListIdx].clear();

	/////////////////////////////
	// Header
	pLabel = mpGuiSet->CreateWidgetLabel(cVector3f(250, mfNoteListHeaderY, 3),cVector2f(300, 32),kTranslate("Journal","Diaries"), pRoot);
	SetupLabel(pLabel,28, 0, eLuxJournalState_Diaries);


	/////////////////////////////
	// No Diaries
	if(mvDiaryContainers.empty())
	{
		vStartPos.y = 270;
		pLabel = mpGuiSet->CreateWidgetLabel(vStartPos,cVector2f(300, 24),kTranslate("Journal","DiariesEmpty"), pRoot);
		SetupLabel(pLabel,24, 0, eLuxJournalState_Diaries,mpFontDefault);
	}
	/////////////////////////////
	// Has Diaries
	else
	{
		size_t lDiaryNum = 0;
		for(size_t cont=0; cont<mvDiaryContainers.size(); ++cont) lDiaryNum += mvDiaryContainers[cont]->mvDiaries.size();
		std::vector<size_t> vEntryPage(lDiaryNum);
		std::vector<float> vEntryY(lDiaryNum);
		size_t lPageNum = ohpl_ga8_jrn_layout_list(lDiaryNum, vEntryPage.data(), vEntryY.data());
		for(size_t p=0; p<lPageNum; ++p) mvNoteListPages[lListIdx].push_back(cLuxJournal_ListPage());
		size_t lFlatIdx = 0;

		for(size_t cont=0; cont<mvDiaryContainers.size(); ++cont)
		{
			cLuxDiaryContainer *pCont = mvDiaryContainers[cont];

			for(size_t i=0; i<pCont->mvDiaries.size(); ++i)
			{
				cLuxDiary *pDiary = pCont->mvDiaries[i];

				//Check if new page is needed!
				cLuxJournal_ListPage *pListPage = &mvNoteListPages[lListIdx][vEntryPage[lFlatIdx]];
				vStartPos.y = vEntryY[lFlatIdx];
				++lFlatIdx;

				pLabel = mpGuiSet->CreateWidgetLabel(vStartPos,cVector2f(300, 24),kTranslate("Journal",pDiary->msNameEntry), pRoot);
				SetupLabel(pLabel,24, (int)i, eLuxJournalState_Diaries, mpFontDefault, eFontAlign_Left);
				pLabel->AddCallback(eGuiMessage_MouseDown,this, kGuiCallback(DiaryTextClick));

				pLabel->AddCallback(eGuiMessage_UIButtonPress,this, kGuiCallback(JournalItemUIButtonPress));

				pLabel->AddCallback(eGuiMessage_OnDraw,this, kGuiCallback(ListTextOnDraw));

				cLuxJournalTextData* pData =  (cLuxJournalTextData*)pLabel->GetUserData();
				pData->mpExtraData = pDiary;

				pImage = mpGuiSet->CreateWidgetImage(pDiary->msIconFile, vStartPos - cVector3f(23,0,1), cVector2f(20, 20),eGuiMaterial_Alpha, false, pRoot);
				SetupImage(pImage,(int)i,eLuxJournalState_Diaries);

				//Add list entry
				pListPage->mvEntries.push_back(cLuxJournal_ListEntry());
				pListPage->mvEntries.back().AddWidget(pLabel);
				pListPage->mvEntries.back().AddWidget(pImage);
			}
		}
	}

	for(size_t i=0; i<mvNoteListPages[lListIdx].size(); ++i)
	{
		cLuxJournal_ListPage *pListPage = &mvNoteListPages[lListIdx][i];
		for(size_t j=0; j<pListPage->mvEntries.size(); ++j)
		{
			cLuxJournal_ListEntry* pListEntry = &pListPage->mvEntries[j];
			int lPrev = j-1;
			int lNext = j+1;

			iWidget* pLabel = pListEntry->mlstWidgets.front();
			
			if(lPrev>=0)
				pLabel->SetFocusNavigation(eUIArrow_Up, pListPage->mvEntries[lPrev].mlstWidgets.front());
			if(lNext<pListPage->mvEntries.size())
				pLabel->SetFocusNavigation(eUIArrow_Down, pListPage->mvEntries[lNext].mlstWidgets.front());
		}
	}

	////////////////////////
	// Navigation
	SetupNavigationWidgets(state, lListIdx, 2, 3, pRoot);


	////////////////////////
	// Set start page
	mlCurrentNoteListPage[lListIdx] = ohpl_ga8_jrn_start_page(mvNoteListPages[lListIdx].size());
}

//-----------------------------------------------------------------------

void cLuxJournal::CreateQuestNotesGui()
{
	cWidgetDummy *pRoot = mvStateData[eLuxJournalState_QuestLog]->mpRootWidget;

	/////////////////////////
	// vars setup
	cVector3f vStartPos = cVector3f(150, 100, 3);
	cWidgetLabel *pLabel = NULL;
	eLuxJournalState state = eLuxJournalState_QuestLog;
	int lListIdx = 2;

	mvNoteListPages[lListIdx].clear();

	/////////////////////////////
	// Header
	pLabel = mpGuiSet->CreateWidgetLabel(cVector3f(250, mfNoteListHeaderY, 3),cVector2f(300, 32),kTranslate("Journal","Quest Log"), pRoot);
	SetupLabel(pLabel,28, 0, eLuxJournalState_QuestLog);


	/////////////////////////////
	// Check if there are any active quests
	bool bHasActiveQuests = false;
	for(size_t i=0; i<mvQuestNotes.size(); ++i)
	{
		cLuxQuestNote *pQuest = mvQuestNotes[i];
		if(pQuest->mbActive)
		{
			bHasActiveQuests = true;
			break;
		}
	}

	/////////////////////////////
	// No Quests
	if(bHasActiveQuests==false)
	{
		vStartPos.y = 270;
		pLabel = mpGuiSet->CreateWidgetLabel(vStartPos,cVector2f(300, 24),kTranslate("Journal","QuestsEmpty"), pRoot);
		SetupLabel(pLabel,24, 0, eLuxJournalState_QuestLog,mpFontDefault);
	}
	/////////////////////////////
	// Has Quests
	else
	{
		std::vector<int> vShown;
		std::vector<tWStringVec> vAllRows;
		std::vector<size_t> vRowNums;
		for(int i=(int)mvQuestNotes.size()-1; i>=0; --i)
		{
			cLuxQuestNote *pQuest = mvQuestNotes[i];
			if(pQuest->mbActive==false) continue;

			tWStringVec vRows;
			mpFontDefault->GetWordWrapRows(500, 21, 19, kTranslate("Journal",pQuest->msTextEntry),&vRows);
			vShown.push_back(i);
			vRowNums.push_back(vRows.size());
			vAllRows.push_back(vRows);
		}

		std::vector<size_t> vEntryPage(vShown.size());
		std::vector<float> vEntryY(vShown.size());
		size_t lPageNum = ohpl_ga8_jrn_layout_quests(vRowNums.data(), vRowNums.size(), vEntryPage.data(), vEntryY.data());
		for(size_t p=0; p<lPageNum; ++p) mvNoteListPages[lListIdx].push_back(cLuxJournal_ListPage());

		for(size_t q=0; q<vShown.size(); ++q)
		{
			int i = vShown[q];
			const tWStringVec &vRows = vAllRows[q];
			cLuxJournal_ListPage *pListPage = &mvNoteListPages[lListIdx][vEntryPage[q]];
			vStartPos.y = vEntryY[q];

			//Add list entry
			pListPage->mvEntries.push_back(cLuxJournal_ListEntry());

			//Start sign ("-")
			pLabel = mpGuiSet->CreateWidgetLabel(vStartPos - cVector3f(15,0,1),cVector2f(12, 24),_W("-"), pRoot);
			SetupLabel(pLabel,19, (int)i, eLuxJournalState_QuestLog, mpFontDefault, eFontAlign_Left);
			pListPage->mvEntries.back().AddWidget(pLabel);

			//Text
			for(size_t j=0; j<vRows.size(); ++j)
			{
				pLabel = mpGuiSet->CreateWidgetLabel(vStartPos,cVector2f(500, 24),vRows[j], pRoot);
				SetupLabel(pLabel,19, (int)i, eLuxJournalState_QuestLog, mpFontDefault, eFontAlign_Left);
				pListPage->mvEntries.back().AddWidget(pLabel);
				vStartPos.y += 21;
			}
		}
	}

	////////////////////////
	// Navigation
	SetupNavigationWidgets(state, lListIdx, 2, 3, pRoot);


	////////////////////////
	// Set start page
	mlCurrentNoteListPage[lListIdx] = ohpl_ga8_jrn_start_page(mvNoteListPages[lListIdx].size());
}

//-----------------------------------------------------------------------

void cLuxJournal::CreateOpenNoteGui()
{
	////////////////////////////////
	// Diary and note
	for(int i=0; i<3; ++i)
	{
		eLuxJournalState state = i==0 ? eLuxJournalState_OpenNote : (i == 1 ? eLuxJournalState_OpenDiary : eLuxJournalState_OpenNarratedDiary);
		
		cWidgetDummy *pRoot = mvStateData[state]->mpRootWidget;
		
		cWidgetLabel *pImage = NULL;
		cWidgetLabel *pLabel = NULL;

		////////////////////////
		// Navigation
		SetupNavigationWidgets(state, -1, 0,1, pRoot);

		mpWidgetDefaultNav[state] = mpWidgetReturn[state];
	}

	////////////////////////////////
	// Narrated note
	cWidgetDummy *pRoot = mvStateData[eLuxJournalState_OpenNarratedDiary]->mpRootWidget;
	
	cWidgetFrame *pClickFrame = mpGuiSet->CreateWidgetFrame(mvGuiSetStartPos, mvGuiSetSize, false, pRoot);
	//pClickFrame->AddCallback(eGuiMessage_MouseDown,this, kGuiCallback(NoteClickFrameClick));
	pClickFrame->AddCallback(eGuiMessage_UIButtonPress,this, kGuiCallback(NoteClickFrameClick));
	pClickFrame->SetGlobalUIInputListener(true);

	mpImageBackward[eLuxJournalState_OpenNarratedDiary]->SetVisible(false);
	mpImageBackward[eLuxJournalState_OpenNarratedDiary]->SetEnabled(false);
	mpImageForward[eLuxJournalState_OpenNarratedDiary]->SetVisible(false);;
	mpImageForward[eLuxJournalState_OpenNarratedDiary]->SetVisible(false);;
}

//-----------------------------------------------------------------------

void cLuxJournal::CreateBackground()
{
	CreateScreenTextures();
	RenderBackgroundImage();
}

//-----------------------------------------------------------------------

void cLuxJournal::CreateScreenTextures()
{
	iLowLevelGraphics *pLowGfx = mpGraphics->GetLowLevel();
	cVector3l vTexSize = pLowGfx->GetScreenSizeInt();
	vTexSize.z = 0;

	mpScreenTexture = mpGraphics->CreateTexture("Screen",eTextureType_Rect,eTextureUsage_RenderTarget);
	mpScreenTexture->CreateFromRawData(vTexSize,ePixelFormat_RGBA,NULL);
	mpScreenTexture->SetWrapSTR(eTextureWrap_ClampToEdge);

	mpScreenBgTexture = mpGraphics->CreateTexture("ScreenBlur",eTextureType_Rect,eTextureUsage_RenderTarget);
	mpScreenBgTexture->CreateFromRawData(vTexSize,ePixelFormat_RGBA,NULL);

	mpScreenGfx = mpGui->CreateGfxTexture(mpScreenTexture,false,eGuiMaterial_Diffuse);
	mpScreenBgGfx = mpGui->CreateGfxTexture(mpScreenBgTexture,false,eGuiMaterial_Alpha);
}

//-----------------------------------------------------------------------

void cLuxJournal::RenderBackgroundImage()
{
	iLowLevelGraphics *pLowGfx = mpGraphics->GetLowLevel();

	//////////////////////////////
	// Create frame buffers
	iFrameBuffer *pEffectBuffer  = mpGraphics->CreateFrameBuffer("InventoryEffectbuffer");
	pEffectBuffer->SetTexture2D(0,mpScreenBgTexture);
	pEffectBuffer->CompileAndValidate();

	//////////////////////////////
	// Render

	//Render scene again without gui.
	gpBase->mpHelpFuncs->RenderBackgroundScreen(false);


	//Set up main states
	pLowGfx->SetBlendActive(false);
	pLowGfx->SetDepthTestActive(false);
	pLowGfx->SetDepthWriteActive(false);

	pLowGfx->SetOrthoProjection(mvScreenSize,-1000,1000);
	pLowGfx->SetIdentityMatrix(eMatrix_ModelView);

	//Copy screen to screen texture
	pLowGfx->CopyFrameBufferToTexure(mpScreenTexture,0,pLowGfx->GetScreenSizeInt(),0);

	//Bind shader and draw
	mpEffectProgram->Bind();
	pLowGfx->SetCurrentFrameBuffer(pEffectBuffer);

	pLowGfx->SetTexture(0,mpScreenTexture);

	pLowGfx->DrawQuad(0,mvScreenSize,cVector2f(0, mvScreenSize.y),cVector2f(mvScreenSize.x,0),cColor(1,1));
	mpEffectProgram->UnBind();

	//Copy a copy of the full gui with all HUD!
	pLowGfx->SetCurrentFrameBuffer(NULL);
	pLowGfx->SetTexture(0,NULL);

	gpBase->mpHelpFuncs->RenderBackgroundScreen(true);
	pLowGfx->CopyFrameBufferToTexure(mpScreenTexture,0,pLowGfx->GetScreenSizeInt(),0);



	///////////////////////
	// Exit

	//Render states
	pLowGfx->SetTexture(0,NULL);
	pLowGfx->SetCurrentFrameBuffer(NULL);
	pLowGfx->SetDepthTestActive(true);

	//Flush the rendering
	pLowGfx->FlushRendering();
	pLowGfx->WaitAndFinishRendering();

	//Destroy data
	mpGraphics->DestroyFrameBuffer(pEffectBuffer);
}	

//-----------------------------------------------------------------------

void cLuxJournal::DestroyBackground()
{
	if(mpScreenGfx) mpGui->DestroyGfx(mpScreenGfx);
	if(mpScreenTexture) mpGraphics->DestroyTexture(mpScreenTexture);
	if(mpScreenBgGfx) mpGui->DestroyGfx(mpScreenBgGfx);
	if(mpScreenBgTexture) mpGraphics->DestroyTexture(mpScreenBgTexture);

	mpScreenGfx = NULL;
	mpScreenTexture = NULL;
	mpScreenBgTexture = NULL;
	mpScreenBgGfx = NULL;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CALLBACK METHODS
//////////////////////////////////////////////////////////////////////////


bool cLuxJournal::MainMenuTextClick(iWidget* apWidget, const cGuiMessageData& aData)
{
	//Notes
	if(apWidget->GetUserValue()==0)
	{
		ChangeState(eLuxJournalState_Notes);
		mpWidgetDefaultNav[eLuxJournalState_Main] = apWidget;
	}
	//Diaries
	else if(apWidget->GetUserValue()==1)
	{
		ChangeState(eLuxJournalState_Diaries);
		mpWidgetDefaultNav[eLuxJournalState_Main] = apWidget;
	}
	//Quest Log
	else if(apWidget->GetUserValue()==2)
	{
		ChangeState(eLuxJournalState_QuestLog);
		mpWidgetDefaultNav[eLuxJournalState_Main] = apWidget;
	}
	//Back
	else if(apWidget->GetUserValue()==3)
	{
		ExitPressed(false);
	}
	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, MainMenuTextClick);

//-----------------------------------------------------------------------

bool cLuxJournal::MainMenuUIButtonPress(iWidget* apWidget, const cGuiMessageData& aData)
{
	if(aData.mlVal==eUIButton_Secondary)
	{
		if(UIListenerButtonPress(apWidget, aData) == false)
		{
			return NoteBackClick(apWidget, cGuiMessageData(eGuiMouseButton_Left));
		}

		return true;
	}
	
	if(aData.mlVal==eUIButton_Primary)
		return MainMenuTextClick(apWidget, aData);

	return false;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, MainMenuUIButtonPress);

//-----------------------------------------------------------------------

bool cLuxJournal::MainMenuTextOnDraw(iWidget* apWidget, const cGuiMessageData& aData)
{
	cLuxJournalTextData *pData = (cLuxJournalTextData*)apWidget->GetUserData();

	if(pData->mfEffectfAlpha >0)
	{
		float fT = 0.3f + (sin(mfMouseOverPulse*2.5f)+1)*0.7f*0.5f;
		float fXAdd = 1 + (1-fT)*5.0f;

		cVector3f vPos = apWidget->GetGlobalPosition() - cVector3f(fXAdd/2,2,1);
		vPos.x += apWidget->GetSize().x/2;

		cVector2f vSize = apWidget->GetDefaultFontSize() + cVector2f(fXAdd,4);

		float fAlpha = apWidget->GetColorMul().a*fT *pData->mfEffectfAlpha;
		mpGuiSet->DrawFont(	apWidget->GetText(), mpFontMenu, vPos, vSize, cColor(1,0,0, fAlpha),
							eFontAlign_Center);
	}
	
	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, MainMenuTextOnDraw);

//-----------------------------------------------------------------------

bool cLuxJournal::ListTextOnDraw(iWidget* apWidget, const cGuiMessageData& aData)
{
	cLuxJournalTextData *pData = (cLuxJournalTextData*)apWidget->GetUserData();
	cWidgetLabel *pLabel = static_cast<cWidgetLabel*>(apWidget);

	if(pData->mfEffectfAlpha >0)
	{
		float fT = 0.3f + (sin(mfMouseOverPulse*2.5f)+1)*0.7f*0.5f;
		float fXAdd = 1 + (1-fT)*5.0f;

		cVector2f vSize = apWidget->GetDefaultFontSize() + cVector2f(fXAdd,4);
		float fAlpha = apWidget->GetColorMul().a*fT *pData->mfEffectfAlpha;
		
		//////////////////////
		//Centered text
		if(pLabel->GetTextAlign() == eFontAlign_Center)
		{
			cVector3f vPos = apWidget->GetGlobalPosition() - cVector3f(fXAdd/2,2,1);
			vPos.x += apWidget->GetSize().x/2;
			
			mpGuiSet->DrawFont(	apWidget->GetText(), apWidget->GetDefaultFontType(), vPos, vSize, cColor(1,0,0, fAlpha), eFontAlign_Center);
		}
		//////////////////////
		//Left Aligned
		else if(pLabel->GetTextAlign() == eFontAlign_Left)
		{
			float fSizeAdd =	apWidget->GetDefaultFontType()->GetLength(vSize, apWidget->GetText().c_str()) - 
								apWidget->GetDefaultFontType()->GetLength(apWidget->GetDefaultFontSize(), apWidget->GetText().c_str());

			cVector3f vPos = apWidget->GetGlobalPosition() - cVector3f(fSizeAdd/2,2,1);

			mpGuiSet->DrawFont(	apWidget->GetText(), apWidget->GetDefaultFontType(), vPos, vSize, cColor(1,0,0, fAlpha), eFontAlign_Left);
		}

		
	}

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, ListTextOnDraw);

//-----------------------------------------------------------------------

bool cLuxJournal::NoteTextClick(iWidget* apWidget, const cGuiMessageData& aData)
{
	OpenNote(mvNotes[apWidget->GetUserValue()], false);

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, NoteTextClick);

//-----------------------------------------------------------------------

bool cLuxJournal::NoteBackClick(iWidget* apWidget, const cGuiMessageData& aData)
{
	//if(UIListenerButtonPress(apWidget, aData))
	//{
	//	return true;
	//}

	if(aData.mlVal != eUIButton_Primary && aData.mlVal != eUIButton_Secondary)
		return true;

	ExitPressed(false);

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, NoteBackClick);

//-----------------------------------------------------------------------

bool cLuxJournal::UIListenerJournalPress(iWidget* apWidget, const cGuiMessageData& aData)
{
	if(aData.mlVal == eUIButton_Primary)
		UIListenerButtonPress(apWidget, aData);

	else if (aData.mlVal == eUIButton_Secondary)
		ExitPressed(false);

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, UIListenerJournalPress);

//-----------------------------------------------------------------------

bool cLuxJournal::DiaryTextClick(iWidget* apWidget, const cGuiMessageData& aData)
{
	cLuxJournalTextData* pData = (cLuxJournalTextData*)apWidget->GetUserData();

	OpenDiary((cLuxDiary*)pData->mpExtraData, false);

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, DiaryTextClick);

//-----------------------------------------------------------------------

bool cLuxJournal::ImageButtonOnDraw(iWidget* apWidget, const cGuiMessageData& aData)
{
	cLuxJournalTextData *pData = (cLuxJournalTextData*)apWidget->GetUserData();
	cWidgetImage *pImage = static_cast<cWidgetImage*>(apWidget);
	
	if(pData->mfEffectfAlpha >0)
	{
		/*float fT = 0.3f + (sin(mfMouseOverPulse*2.5f)+1)*0.7f*0.5f;
		float fAdd = 1 + (1-fT)*16.0f;

		cVector3f vPos = pImage->GetGlobalPosition() - cVector3f(fAdd/2,fAdd/2,1);
		
		cVector2f vSize = pImage->GetImage()->GetActiveSize() + cVector2f(fAdd,fAdd);

		float fAlpha = apWidget->GetColorMul().a*fT *pData->mfEffectfAlpha;
		mpGuiSet->DrawGfx(	pImage->GetImage(), vPos, vSize, cColor(1,0,0, fAlpha));*/
	}

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, ImageButtonOnDraw);

//-----------------------------------------------------------------------

bool cLuxJournal::NoteArrowClick(iWidget* apWidget, const cGuiMessageData& aData)
{
	///////////////////////////7
	// Open Note

	//Forward
	if(apWidget->GetUserValue()==0)		
	{
		SetNotePage(mlCurrentNotePage+1);
	}
	//Backward
	else if(apWidget->GetUserValue()==1)
	{
		SetNotePage(mlCurrentNotePage-1);
	}
	///////////////////////////7
	// Note List

	//Forward
	else if(apWidget->GetUserValue()==2) 
	{
		SetNoteListPage(mlCurrentNoteListPage[GetNoteListIndex(mCurrentState)]+1, mCurrentState);
	}
	//Backward
	else if(apWidget->GetUserValue()==3)
	{
		SetNoteListPage(mlCurrentNoteListPage[GetNoteListIndex(mCurrentState)]-1, mCurrentState);
	}

	///////////////////////////7
	// Play sound
	gpBase->mpHelpFuncs->PlayGuiSoundData("journal_page", eSoundEntryType_Gui);
	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, NoteArrowClick);

//-----------------------------------------------------------------------

bool cLuxJournal::NoteClickFrameClick(iWidget* apWidget, const cGuiMessageData& aData)
{
	ExitPressed(false);
	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, NoteClickFrameClick);

//-----------------------------------------------------------------------

bool cLuxJournal::JournalItemUIButtonPress(iWidget* apWidget, const cGuiMessageData& aData)
{
	if(aData.mlVal==eUIButton_Primary)
	{
		cLuxJournalTextData* pData = static_cast<cLuxJournalTextData*>(apWidget->GetUserData());
		eLuxJournalState type = pData->mType;

		switch(type)
		{
		case eLuxJournalState_Diaries:
			return DiaryTextClick(apWidget, aData);
		case eLuxJournalState_Notes:
			return NoteTextClick(apWidget, aData);
		}
	}

	return false;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, JournalItemUIButtonPress);

//-----------------------------------------------------------------------

bool cLuxJournal::UIListenerArrowPress(iWidget* apWidget, const cGuiMessageData& aData)
{
	int lType = apWidget->GetUserValue();

	iWidget* pBack = mpImageBackward[lType];
	iWidget* pForward = mpImageForward[lType];

	bool bLeftAllowed = pBack->IsVisible();
	bool bRightAllowed = pForward->IsVisible();

	
	
	switch(aData.mlVal)
	{
	case eUIArrow_Left:
		if(bLeftAllowed) return NoteArrowClick(pBack, cGuiMessageData(eGuiMouseButton_Left));
		break;
	case eUIArrow_Right:
		if(bRightAllowed) return NoteArrowClick(pForward, cGuiMessageData(eGuiMouseButton_Left));
		break;
	case eUIArrow_Down:
#ifdef USE_GAMEPAD
# if USE_SDL2
		if(gpBase->mpInputHandler->GetGamepad() && gpBase->mpInputHandler->GetGamepad()->ButtonIsDown(eGamepadButton_DpadDown) == false) break;
# else
		if(gpBase->mpInputHandler->GetGamepad() && gpBase->mpInputHandler->GetGamepad()->HatIsInState(eGamepadHat_0, eGamepadHatState_Down) == false) break;
# endif
#endif
		if(lType == eLuxJournalState_OpenNote
		|| lType == eLuxJournalState_OpenDiary
		|| lType == eLuxJournalState_OpenNarratedDiary)
		{
			ExitPressed(false);
			return true;
		}
		break;
	}
	
	return false;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, UIListenerArrowPress);

//-----------------------------------------------------------------------

bool cLuxJournal::UIListenerButtonPress(iWidget* apWidget, const cGuiMessageData& aData)
{
	int lType = apWidget->GetUserValue();

	if(!(lType == eLuxJournalState_OpenNote || lType == eLuxJournalState_OpenDiary	|| lType == eLuxJournalState_OpenNote || lType == eLuxJournalState_OpenNarratedDiary)) return false;

	iWidget* pBack = mpImageBackward[lType];
	iWidget* pForward = mpImageForward[lType];

	bool bLeftAllowed = pBack->IsVisible();
	bool bRightAllowed = pForward->IsVisible();

	switch(aData.mlVal)
	{
	case eUIButton_Secondary:
		if(bLeftAllowed) return NoteArrowClick(pBack, cGuiMessageData(eGuiMouseButton_Left));
		else
		{
			ExitPressed(false);
			return true;
		}
	case eUIButton_Primary:
		if(bRightAllowed) return NoteArrowClick(pForward, cGuiMessageData(eGuiMouseButton_Left));
		break;
	}

	return false;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, UIListenerButtonPress);

//-----------------------------------------------------------------------

bool cLuxJournal::NoteBackUIButtonPress(iWidget* apWidget, const cGuiMessageData& aData)
{
	/*
	if(aData.mlVal==eUIButton_Secondary)
	{
		return NoteBackClick(apWidget, aData);
	}
	*/
	return false;
}
kGuiCallbackDeclaredFuncEnd(cLuxJournal, NoteBackUIButtonPress);

//-----------------------------------------------------------------------

