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

#include "LuxBase.h"
#include "graphics/RendererDeferred.h"
#include "graphics/Graphics.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Texture.h"

#include <vector>
#include <limits>
#include <cmath>

#if defined(__linux__)
#include <unistd.h>
#endif

#include "RustLuxBase.h"
#include "LuxInputHandler.h"

#include "LuxMapHandler.h"
#include "LuxMapHelper.h"
#include "LuxMap.h"

#include "LuxDebugHandler.h"
#include "LuxSaveHandler.h"
#include "LuxScriptHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxEffectRenderer.h"
#include "LuxMusicHandler.h"
#include "LuxMessageHandler.h"
#include "LuxEffectHandler.h"
#include "LuxJournal.h"
#include "LuxCompletionCountHandler.h"
#include "LuxGlobalDataHandler.h"
#include "LuxHintHandler.h"
#include "LuxConfigHandler.h"
#include "LuxPostEffects.h"
#include "LuxInsanityHandler.h"
#include "LuxProgressLogHandler.h"
#include "LuxLoadScreenHandler.h"

#include "LuxInventory.h"

#include "LuxCredits.h"

#include "LuxDemoEnd.h"

#include "LuxPreMenu.h"

#include "LuxMainMenu.h"

#include "LuxPlayer.h"

#include "system/HeadlessControl.h"
#include "system/EngineDiagnostics.h"

#include "LuxStaticProp.h"

#include "LuxProp_Object.h"
#include "LuxProp_SwingDoor.h"
#include "LuxProp_Lever.h"
#include "LuxProp_Wheel.h"
#include "LuxProp_Lamp.h"
#include "LuxProp_Photocell.h"
#include "LuxProp_MoveObject.h"
#include "LuxProp_Item.h"
#include "LuxProp_Chest.h"
#include "LuxProp_LevelDoor.h"
#include "LuxProp_Button.h"
#include "LuxProp_OilBarrel.h"
#include "LuxProp_EmotionStone.h"
#include "LuxProp_NPC.h"
#include "LuxProp_MultiSlider.h"

#include "LuxCritter_Bug.h"
#include "LuxCritter_Spider.h"

#include "LuxAreaNodes.h"
#include "LuxArea_Script.h"
#include "LuxArea_Flashback.h"
#include "LuxArea_Ladder.h"
#include "LuxArea_Liquid.h"
#include "LuxArea_Sticky.h"
#include "LuxArea_Insanity.h"
#include "LuxArea_Examine.h"
#include "LuxArea_Sign.h"
#include "LuxArea_Rope.h"
#include "LuxArea_SlimeDamage.h"

#include "LuxEnemy_Grunt.h"
#include "LuxEnemy_WaterLurker.h"
#include "LuxEnemy_ManPig.h"

#include "LuxCommentaryIcon.h"
#include "LuxAchievementHandler.h"



//////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

#define kCurrentVersion_Main 1
#define kCurrentVersion_Minor 4

//-----------------------------------------------------------------------

extern tString gsSerialKey;

//-----------------------------------------------------------------------

eLuxAxis StringToAxis(const tString &asAxis)
{
	bool bOk = false;
	int lAxis = ohpl_ga9_axis_from_name((const unsigned char*)asAxis.data(), asAxis.size(), &bOk);

	if(bOk == false) Error("Axis '%s' does not exist!\n", asAxis.c_str());

	return (eLuxAxis)lAxis;
}

//-----------------------------------------------------------------------

void LuxCalcGuiSetOffset(const cVector2f &avVirtualSizeIn, const cVector2f& avScreenSize, cVector2f& avOutSize, cVector2f & avOutOffset)
{
	float vOut[4];
	ohpl_ga9_calc_gui_set_offset(avVirtualSizeIn.x, avVirtualSizeIn.y, avScreenSize.x, avScreenSize.y, vOut);

	avOutSize.x = vOut[0];
	avOutSize.y = vOut[1];
	avOutOffset.x = vOut[2];
	avOutOffset.y = vOut[3];
}

void LuxCalcGuiSetScreenOffset(const cVector2f &avVirtualSizeIn, cVector2f& avOutSize, cVector2f & avOutOffset)
{
	LuxCalcGuiSetOffset(avVirtualSizeIn, gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat(), avOutSize, avOutOffset);
}

//-----------------------------------------------------------------------

void ProgLog(eLuxProgressLogLevel aLevel, const tString& asMessage)
{
	gpBase->mpProgressLogHandler->AddLog(aLevel, asMessage);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CUSTOM STORY SETTINGS CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxCustomStorySettings::cLuxCustomStorySettings()
{
}

cLuxCustomStorySettings::cLuxCustomStorySettings(cLuxCustomStorySettings* apStory)
{
	msStoryRootFolder = apStory->msStoryRootFolder;

	msName = apStory->msName;
	msAuthor = apStory->msAuthor;

	msDefaultExtraLanguage = apStory->msDefaultExtraLanguage;
	msExtraLangFilePrefix = apStory->msExtraLangFilePrefix;
	
	msMapsFolder = apStory->msMapsFolder;
	msStartMap = apStory->msStartMap;
	msStartPos = apStory->msStartPos;
}

cLuxCustomStorySettings::~cLuxCustomStorySettings()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CUSTOM STORY SETTINGS PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxCustomStorySettings::CreateFromPath(const tWString& asPath)
{
	/////////////////////////////////////////////////////////////////
	// We must make sure this is a valid custom story:
	//	- first checking if its a valid path,
	//	- then checking the custom_story_settings.cfg file
	//	- then checking if the data in the cfg file at least exists
	if(cPlatform::FolderExists(asPath)==false)
		return false;

	tString sErrorMsg;

	tWString sFile = cString::AddSlashAtEndW(asPath) + _W("custom_story_settings.cfg");
	bool bValid = false;

	cConfigFile* pCustomStoryCfg = hplNew(cConfigFile,(sFile));
	bValid = pCustomStoryCfg->Load();
	if(bValid)
	{
		msStoryRootFolder = cString::AddSlashAtEndW(asPath);

		auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r)
		{
			if(op == eOhplGa9Op_FileExistsW) r->i = cPlatform::FileExists(OhplGa9GetW(a[0]));
			else OhplGa9CfgCall(pCustomStoryCfg, op, a, r);
		};
		auto emit = [&](uint32_t tag, const void *p, size_t n)
		{
			switch(tag)
			{
			case eOhplGa9Tag_StoryMapsFolder: msMapsFolder = OhplGa9EmitS(p, n); break;
			case eOhplGa9Tag_StoryStartMap: msStartMap = OhplGa9EmitS(p, n); break;
			case eOhplGa9Tag_StoryStartPos: msStartPos = OhplGa9EmitS(p, n); break;
			case eOhplGa9Tag_StoryName: msName = OhplGa9EmitW(p, n); break;
			case eOhplGa9Tag_StoryAuthor: msAuthor = OhplGa9EmitW(p, n); break;
			case eOhplGa9Tag_StoryImgFile: msImgFile = OhplGa9EmitS(p, n); break;
			case eOhplGa9Tag_StoryExtraPrefix: msExtraLangFilePrefix = OhplGa9EmitS(p, n); break;
			case eOhplGa9Tag_StoryDefaultExtra: msDefaultExtraLanguage = OhplGa9EmitS(p, n); break;
			case eOhplGa9Tag_StoryError: sErrorMsg = OhplGa9EmitS(p, n); break;
			}
		};
		bValid = ohpl_ga9_custom_story(&call, OhplGa9CallOf<decltype(call)>::Fn(),
										(const uint32_t*)msStoryRootFolder.data(), msStoryRootFolder.size(),
										(const unsigned char*)gpBase->msDefaultGameLanguage.data(), gpBase->msDefaultGameLanguage.size(),
										OhplGa9EmitOf<decltype(emit)>::Fn(), &emit);

		if(msName==_W(""))
			msName = kTranslate("CustomStory", "NoName");
		if(msAuthor==_W(""))
			msAuthor = kTranslate("CustomStory", "NoAuthor");
	}
	else
	{
		sErrorMsg = "could not find custom_story_settings.cfg file or it was invalid";
	}

	if(bValid==false)
		Log("Error creating custom story from path \"%ls\" : %s.\n", asPath.c_str(), sErrorMsg.c_str());

	hplDelete(pCustomStoryCfg);

	return bValid;
}

//-----------------------------------------------------------------------

void cLuxCustomStorySettings::SetActive()
{
	cLuxCustomStorySettings* pStory = hplNew(cLuxCustomStorySettings,(this));
	
	gpBase->SetCustomStory(pStory);
}

//-----------------------------------------------------------------------

bool cLuxCustomStorySettings::StartGame()
{
	gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_LastEnum);

	gpBase->mpInputHandler->ChangeState(eLuxInputState_Game);

	gpBase->mpEngine->GetUpdater()->SetContainer("Default");
	gpBase->mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset);

	gpBase->mpProgressLogHandler->CreateAndResetLogFile();

	return gpBase->StartGame(msStartMap, msMapsFolder, msStartPos);
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PTEST FUNCTIONS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

static inline unsigned int GetFileCRC(const tString& asFilePath, unsigned int alKey)
{
	cBinaryBuffer buff;
	if(buff.Load(cString::To16Char(asFilePath))==false) return 0;

	return buff.GetCRC(alKey, 0);
}

static inline unsigned int GetFileCRC(const tWString& asFilePath, unsigned int alKey)
{
	cBinaryBuffer buff;
	if(buff.Load(asFilePath)==false) return 0;

	return buff.GetCRC(alKey, 0);
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxBase::cLuxBase()
{
	///////////////////////////////
	// Init pointers
	mpEngine = NULL;

	mpMainConfig = NULL;
	mpUserConfig = NULL;
	mpUserKeyConfig = NULL;

	mpCustomStory = NULL;

	mpMenuCfg = NULL;
	mpGameCfg = NULL;
	mpDemoCfg = NULL;

	mpCurrentMapLoading = NULL;


	///////////////////////////////
	// Init string
	msGameName = "";

	msCurrentLanguage = "";

	///////////////////////////////
	// Init variables
	mbPTestActivated = false;

	///////////////////////////////
	// HARDMODE
	mbHardMode = false;
	mbAllowHardmode = false;
}

//-----------------------------------------------------------------------

cLuxBase::~cLuxBase()
{
	ExitEngine();

	if(mpCustomStory)
		hplDelete(mpCustomStory);
}

//-----------------------------------------------------------------------


static void cLuxBase_RunHeadless(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp, uint32_t alCmd)
{
	cLuxBase *pBase = (cLuxBase*)apUserData;
	iCharacterBody *pBody = NULL;
	cVector3f vPos, vVel;
	std::vector<float> vPixels;
	iTexture *pTex = NULL;

	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r)
	{
		switch(op)
		{
		case eOhplGa9Op_ReqHas: r->i = aReq.HasKey(OhplGa9GetS(a[0])); break;
		case eOhplGa9Op_ReqStr: OhplGa9RetS(r, aReq.GetString(OhplGa9GetS(a[0]), OhplGa9GetS(a[1]))); break;
		case eOhplGa9Op_ReqInt: r->i = aReq.GetInt(OhplGa9GetS(a[0]), (int)a[1].i); break;
		case eOhplGa9Op_ReqFloat: r->f = aReq.GetFloat(OhplGa9GetS(a[0]), a[1].f); break;
		case eOhplGa9Op_ReqBool: r->i = aReq.GetBool(OhplGa9GetS(a[0]), a[1].i != 0); break;
		case eOhplGa9Op_RespError: aResp.SetError(OhplGa9GetS(a[0])); break;
		case eOhplGa9Op_RespStr: aResp.Set(OhplGa9GetS(a[0]), OhplGa9GetS(a[1])); break;
		case eOhplGa9Op_RespInt: aResp.Set(OhplGa9GetS(a[0]), (int)a[1].i); break;
		case eOhplGa9Op_RespFloat: aResp.Set(OhplGa9GetS(a[0]), a[1].f); break;
		case eOhplGa9Op_RespBool: aResp.Set(OhplGa9GetS(a[0]), a[1].i != 0); break;
		case eOhplGa9Op_RespRaw: aResp.SetRaw(OhplGa9GetS(a[0]), OhplGa9GetS(a[1])); break;

		case eOhplGa9Op_MapLoaded: r->i = pBase->mpMapHandler->MapIsLoaded(); break;
		case eOhplGa9Op_RunScript: pBase->mpMapHandler->GetCurrentMap()->RunScript(OhplGa9GetS(a[0])); break;
		case eOhplGa9Op_SetGBuffer: cRendererDeferred::SetDebugRenderFrameBuffers(a[0].i != 0); break;
		case eOhplGa9Op_Deferred:
			{
				iRenderer *pRenderer = pBase->mpEngine->GetGraphics()->GetRenderer(eRenderer_Main);
				r->i = static_cast<cRendererDeferred*>(pRenderer) != NULL;
			}
			break;
		case eOhplGa9Op_GBufferSize:
			{
				iRenderer *pRenderer = pBase->mpEngine->GetGraphics()->GetRenderer(eRenderer_Main);
				pTex = static_cast<cRendererDeferred*>(pRenderer)->GetDebugGBufferTexture((int)a[0].i);
				r->f = pTex != NULL ? 1.0f : 0.0f;
				if(pTex) r->i = (int64_t)(uint32_t)pTex->GetWidth() | ((int64_t)pTex->GetHeight() << 32);
			}
			break;
		case eOhplGa9Op_GBufferPixels:
			r->i = pTex->GetRawPixelsRGBAFloat(vPixels);
			if(r->i) ohpl_ga9_ret_bytes(r, (const unsigned char*)vPixels.data(), vPixels.size() * sizeof(float));
			break;
		case eOhplGa9Op_MapName: OhplGa9RetS(r, pBase->mpMapHandler->GetCurrentMap()->GetName()); break;
		case eOhplGa9Op_MapFile: OhplGa9RetS(r, pBase->mpMapHandler->GetCurrentMap()->GetFileName()); break;
		case eOhplGa9Op_Body:
			// character body exists only while a map is loaded
			pBody = pBase->mpPlayer ? pBase->mpPlayer->GetCharacterBody() : NULL;
			r->i = pBody != NULL;
			if(pBody)
			{
				vPos = pBody->GetFeetPosition();
				vVel = pBody->GetVelocity(pBase->mpEngine->GetStepSize());
			}
			break;
		case eOhplGa9Op_BodyValue:
			switch(a[0].i)
			{
			case 0: r->f = vPos.x; break;
			case 1: r->f = vPos.y; break;
			case 2: r->f = vPos.z; break;
			case 3: r->f = vVel.x; break;
			case 4: r->f = vVel.y; break;
			case 5: r->f = vVel.z; break;
			case 6: r->f = pBase->mpPlayer->GetHealth(); break;
			case 7: r->f = pBase->mpPlayer->GetSanity(); break;
			}
			break;
		case eOhplGa9Op_Fps: r->f = pBase->mpEngine->GetFPS(); break;
		case eOhplGa9Op_Occlusion: r->i = pBase->mpMapHandler->GetViewport()->GetRenderSettings()->mbUseOcclusionCulling; break;
		case eOhplGa9Op_SetOcclusion: pBase->mpMapHandler->GetViewport()->GetRenderSettings()->mbUseOcclusionCulling = a[0].i != 0; break;
		case eOhplGa9Op_GpuTiming: cEngineDiagnostics::mbGpuTiming = a[0].i != 0; break;
		case eOhplGa9Op_RenderJson: OhplGa9RetS(r, cEngineDiagnostics::GetRenderStatsJson(pBase->mpMapHandler->GetViewport(), pBase->mpEngine->GetGraphics())); break;
		case eOhplGa9Op_SaveGame: pBase->mpSaveHandler->SaveGameToFile(cString::To16Char(OhplGa9GetS(a[0])), false); break;
		case eOhplGa9Op_FileExists: r->i = cPlatform::FileExists(cString::To16Char(OhplGa9GetS(a[0]))); break;
		case eOhplGa9Op_LoadGame: pBase->mpSaveHandler->LoadGameFromFile(cString::To16Char(OhplGa9GetS(a[0]))); break;
		case eOhplGa9Op_Player: r->i = pBase->mpPlayer != NULL; break;
		case eOhplGa9Op_Teleport:
			pBody = pBase->mpPlayer->GetCharacterBody();
			if(pBody)
			{
				pBody->SetFeetPosition(cVector3f(a[0].f, a[1].f, a[2].f));
				pBody->StopMovement();
			}
			break;
		case eOhplGa9Op_HaveUserCfg: r->i = pBase->mpUserConfig != NULL; break;
		case eOhplGa9Op_DefaultProfile:
			// headless start skips PreMenu profile setup
			pBase->CreateProfile(pBase->msDefaultProfileName);
			pBase->SetProfile(pBase->msDefaultProfileName);
			break;
		case eOhplGa9Op_InitUserCfg: r->i = pBase->InitUserConfig(); break;
		case eOhplGa9Op_EnterGame:
			// input is dropped unless input/updater state is Game/Default
			pBase->mpInputHandler->ChangeState(eLuxInputState_Game);
			pBase->mpEngine->GetUpdater()->SetContainer("Default");
			break;
		case eOhplGa9Op_StartGame: r->i = pBase->StartGame(OhplGa9GetS(a[0]), "", OhplGa9GetS(a[1])); break;
		}
	};
	ohpl_ga9_headless(alCmd, &call, OhplGa9CallOf<decltype(call)>::Fn());
}

static void cLuxBase_HeadlessCmd_RunScript(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cLuxBase_RunHeadless(apUserData, aReq, aResp, eOhplGa9Cmd_RunScript);
}

static void cLuxBase_HeadlessCmd_SetDebugGbuffer(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cLuxBase_RunHeadless(apUserData, aReq, aResp, eOhplGa9Cmd_SetDebugGBuffer);
}

static void cLuxBase_HeadlessCmd_ReadGbufferStats(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cLuxBase_RunHeadless(apUserData, aReq, aResp, eOhplGa9Cmd_ReadGBufferStats);
}

static void cLuxBase_HeadlessCmd_State(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cLuxBase_RunHeadless(apUserData, aReq, aResp, eOhplGa9Cmd_State);
}

static void cLuxBase_HeadlessCmd_RenderStats(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cLuxBase_RunHeadless(apUserData, aReq, aResp, eOhplGa9Cmd_RenderStats);
}

static void cLuxBase_HeadlessCmd_SaveGame(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cLuxBase_RunHeadless(apUserData, aReq, aResp, eOhplGa9Cmd_SaveGame);
}

static void cLuxBase_HeadlessCmd_LoadGame(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cLuxBase_RunHeadless(apUserData, aReq, aResp, eOhplGa9Cmd_LoadGame);
}

static void cLuxBase_HeadlessCmd_Teleport(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cLuxBase_RunHeadless(apUserData, aReq, aResp, eOhplGa9Cmd_Teleport);
}

static void cLuxBase_HeadlessCmd_StartMap(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cLuxBase_RunHeadless(apUserData, aReq, aResp, eOhplGa9Cmd_StartMap);
}


//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxBase::Init(const tString &asCommandline)
{
	/////////////////////////////
	// Parse the command line
	if(ParseCommandLine(asCommandline)==false) return false;

	/////////////////////////////
	// Init basic app and engine stuff
	if(InitApp()==false) return false;

	/////////////////////////////
	// Load the config files
	if(InitMainConfig()==false) return false;

	Log("Version %d.%d \n",kCurrentVersion_Main, kCurrentVersion_Minor);

	#ifdef COPY_PROTECTION_ENABLED
	/////////////////////////////
	// Copy Protection check
	eSerialInputReturnState serialState = AskForSerial();
	while(serialState!=eSerialInputReturnState_Validated)
	{
		if(serialState==eSerialInputReturnState_WrongKey)
		{
			cPlatform::CreateMessageBox(_W("Error!"),_W("Invalid Serial!\n"));
		}
		else
		{
			msErrorMessage = _W("Serial key needed to run game!\n");
			return false;
		}

		serialState = AskForSerial();
	}
	
	//calculate the CRC for the serial key.
	int lCRCKey = 1337;
	cCRC serialCRC(lCRCKey);
	for(size_t i=0; i<gsSerialKey.size(); ++i)
		serialCRC.PutByte(gsSerialKey[i]);	
	Log(" - s%X-%X", (int)serialCRC.Done(), lCRCKey);

	#endif
	Log("\n");
	
	/////////////////////////////
	// Init the engine
	if(InitEngine()==false) return false;

	/////////////////////////////
	// Check so all needed features are supported
	if(CheckFeatureSupport()==false) return false;

	/////////////////////////////
	// Init the game data and structures
	if(InitGame()==false) return false;

	if(mpEngine->GetHeadlessControl())
	{
		cHeadlessControlServer *pCtrl = mpEngine->GetHeadlessControl();
		pCtrl->RegisterHandler("run_script", cLuxBase_HeadlessCmd_RunScript, this);
		pCtrl->RegisterHandler("set_debug_gbuffer", cLuxBase_HeadlessCmd_SetDebugGbuffer, this);
		pCtrl->RegisterHandler("read_gbuffer_stats", cLuxBase_HeadlessCmd_ReadGbufferStats, this);
		pCtrl->RegisterHandler("state", cLuxBase_HeadlessCmd_State, this);
		pCtrl->RegisterHandler("render_stats", cLuxBase_HeadlessCmd_RenderStats, this);
		pCtrl->RegisterHandler("teleport", cLuxBase_HeadlessCmd_Teleport, this);
		pCtrl->RegisterHandler("save_game", cLuxBase_HeadlessCmd_SaveGame, this);
		pCtrl->RegisterHandler("load_game", cLuxBase_HeadlessCmd_LoadGame, this);
		pCtrl->RegisterHandler("start_map", cLuxBase_HeadlessCmd_StartMap, this);
	}


	int lStartMode = ohpl_ga9_start_mode(mbShowPreMenu, mbShowMenu);

	//////////////////////////
	// Start premenu
	if(lStartMode == 0)
	{
		mpEngine->GetUpdater()->SetContainer("PreMenu");
	}
	//////////////////////////
	// Start menu
	else if(lStartMode == 1)
	{
		///////////////////////////////////////
		// Draw loading screen
		mpLoadScreenHandler->DrawMenuScreen();

		mpEngine->GetUpdater()->SetContainer("MainMenu");
	}
	//////////////////////////
	// Start game directly and create default profile
	else
	{
		//Create (if not existing) and set profile.
		CreateProfile(msDefaultProfileName);
		SetProfile(msDefaultProfileName);

		//Load user config
		if(InitUserConfig()==false) return false;
		
		//Unlock input if not in window
		int lLockInput = ohpl_ga9_direct_start_input(mpDebugHandler->GetDebugWindowActive(), mpConfigHandler->mbFullscreen);
		if (lLockInput >= 0)
		{
			if (lLockInput == 1)
			{
				mpEngine->GetInput()->GetLowLevel()->LockInput(true);
			}
			mpEngine->GetInput()->GetLowLevel()->RelativeMouse(true);
		}

		//Load map and start game.
		//By using "" user config values are used.
		StartGame("","", "");
	}

	
	//////////////////////////
	//The init is over
	InitOver();
		
	return true;
}

//-----------------------------------------------------------------------

void cLuxBase::Exit()
{
	if(mbSaveConfigAtExit) SaveConfig();
	ExitGame();
	ExitConfig();
}

//-----------------------------------------------------------------------

void cLuxBase::Run()
{
	mpEngine->Run();
}

void cLuxBase::Reset()
{
	mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset);
}

//-----------------------------------------------------------------------

void cLuxBase::RunModuleMessage(eLuxUpdateableMessage aMessage, void * apData)
{
	for(size_t i=0; i<mvModules.size(); ++i)
	{
		iLuxUpdateable *pModule = mvModules[i];

		pModule->LuxRunMessage(aMessage, apData);
	}
}

bool cLuxBase::StartGame(const tString& asFile, const tString& asFolder, const tString& asStartPos)
{
	///////////////////////////////////////
	// Draw loading screen
	mpLoadScreenHandler->DrawMenuScreen();

	///////////////////
	//Reset everything
	mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset);
	mpProgressLogHandler->CreateAndResetLogFile();

	///////////////////
	//Get map file, folder and start pos
	OhplGa9StartArgs startArgs = {};
	startArgs.file = OhplGa9StrOf(asFile);
	startArgs.folder = OhplGa9StrOf(asFolder);
	startArgs.pos = OhplGa9StrOf(asStartPos);
	startArgs.has_story = mpCustomStory != NULL;
	if(mpCustomStory)
	{
		startArgs.story_map = OhplGa9StrOf(mpCustomStory->msStartMap);
		startArgs.story_folder = OhplGa9StrOf(mpCustomStory->msMapsFolder);
		startArgs.story_pos = OhplGa9StrOf(mpCustomStory->msStartPos);
	}
	startArgs.def_file = OhplGa9StrOf(msStartMapFile);
	startArgs.def_folder = OhplGa9StrOf(msStartMapFolder);
	startArgs.def_pos = OhplGa9StrOf(msStartMapPos);
	startArgs.ptest = gpBase->mbPTestActivated;

	tString sMapFile, sMapFolder, sStartPos;
	auto userCall = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r)
	{
		if(op == eOhplGa9Op_MapFolder) mpMapHandler->SetMapFolder(OhplGa9GetS(a[0]));
		else OhplGa9CfgCall(mpUserConfig, op, a, r);
	};
	auto startEmit = [&](uint32_t tag, const void *p, size_t n)
	{
		switch(tag)
		{
		case eOhplGa9Tag_StartFile: sMapFile = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_StartFolder: sMapFolder = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_StartPos: sStartPos = OhplGa9EmitS(p, n); break;
		}
	};
	ohpl_ga9_resolve_start(&userCall, OhplGa9CallOf<decltype(userCall)>::Fn(), &startArgs, OhplGa9EmitOf<decltype(startEmit)>::Fn(), &startEmit);

	//////////////////
	//Global script
	mpGlobalDataHandler->LoadAndInitGlobalScript();
	
	//////////////////
	//Load map
	cLuxMap *pMap = mpMapHandler->LoadMap(sMapFile, true);
	mpMapHandler->SetCurrentMap(pMap, true, true, sStartPos);
	
	///////////////////
	//Send message that game has been started.
	RunModuleMessage(eLuxUpdateableMessage_OnGameStart, NULL);

	return true;
}

//-----------------------------------------------------------------------

bool cLuxBase::StartCustomStory()
{
	if(mpCustomStory)
		return mpCustomStory->StartGame();

	return false;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//#define LOG_CRC 0

//-----------------------------------------------------------------------

bool cLuxBase::ParseCommandLine(const tString &asCommandline)
{
	bool vFlags[2] = {false, false};
	auto emit = [&](uint32_t tag, const void *p, size_t n)
	{
		if(tag == eOhplGa9Tag_InitFile) msInitConfigFile = OhplGa9EmitW(p, n);
		else if(tag == eOhplGa9Tag_DefaultInit) msDefaultInitConfigFile = OhplGa9EmitW(p, n);
	};
	ohpl_ga9_parse_cmdline((const unsigned char*)asCommandline.data(), asCommandline.size(), vFlags, OhplGa9EmitOf<decltype(emit)>::Fn(), &emit);

	if(vFlags[0]) mbPTestActivated = true;
	if(vFlags[1]) mbHardMode = true;

	return true;
}

//-----------------------------------------------------------------------

#include "LuxBasePersonal.h"

//-----------------------------------------------------------------------

bool cLuxBase::InitApp()
{
	// We load this HERE so the "userdir" magic can work
	tWString sPersonalDir = cPlatform::GetSystemSpecialPath(PERSONAL_SYSTEMPATH_TYPE);
#ifdef USERDIR_RESOURCES
	//Set the user resource directory (used for mac and linux builds to not require saving files in game dir)
	msUserResourceDir = sPersonalDir+PERSONAL_RELATIVEROOT PERSONAL_RELATIVEGAME_PARENT PERSONAL_RESOURCES;
#endif

	/////////////////////////
	// Load the Init file and read the config paths
	cConfigFile *pInitCfg = NULL;
	cConfigFile *pInitCfgDefaults = NULL;
	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r)
	{
		switch(op)
		{
		case eOhplGa9Op_LoadInit:
#ifdef USERDIR_RESOURCES
			pInitCfg = hplNew(cConfigFile, (msInitConfigFile, msUserResourceDir));
#else
			pInitCfg = hplNew(cConfigFile, (msInitConfigFile ));
#endif
			r->i = pInitCfg->Load();
			break;
		case eOhplGa9Op_LoadDefaults:
			pInitCfgDefaults = hplNew(cConfigFile, (msDefaultInitConfigFile));
			r->i = pInitCfgDefaults->Load();
			break;
		case eOhplGa9Op_Warn:
			cPlatform::CreateMessageBox(eMsgBoxType_Warning, _W("Warning"), OhplGa9GetW(a[0]).c_str());
			break;
		default:
			if(op >= eOhplGa9Op_Cfg2) OhplGa9CfgCall(pInitCfgDefaults, op - eOhplGa9Op_Cfg2, a, r);
			else OhplGa9CfgCall(pInitCfg, op, a, r);
			break;
		}
	};
	bool bAllowHardmode = false;
	auto emit = [&](uint32_t tag, const void *p, size_t n)
	{
		switch(tag)
		{
		case eOhplGa9Tag_Error: msErrorMessage = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_AppSaveFolder: msMainSaveFolder = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_AppUserCfg: msDefaultUserConfigPath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_AppUserKeys: msDefaultUserKeyConfigPath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_AppMainCfg: msDefaultMainConfigPath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_AppGameCfg: msGameConfigPath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_AppMenuCfg: msMenuConfigPath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_AppPreMenuCfg: msPreMenuConfigPath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_AppDemoCfg: msDemoConfigPath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_AppResources: msResourceConfigPath = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_AppMaterials: msMaterialConfigPath = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_AppBaseLang: msDefaultBaseLanguage = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_AppGameLang: msDefaultGameLanguage = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_AppBaseLangFolder: msBaseLanguageFolder = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_AppGameLangFolder: msGameLanguageFolder = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_AppCustomStoryPath: msCustomStoryPath = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_AppGameName: msGameName = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_AppAllowHardmode: bAllowHardmode = n != 0; break;
		case eOhplGa9Tag_AppStartFile: msStartMapFile = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_AppStartFolder: msStartMapFolder = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_AppStartPos: msStartMapPos = OhplGa9EmitS(p, n); break;
		}
	};
	bool bOk = ohpl_ga9_init_app(&call, OhplGa9CallOf<decltype(call)>::Fn(),
								(const uint32_t*)msInitConfigFile.data(), msInitConfigFile.size(),
								(const uint32_t*)msDefaultInitConfigFile.data(), msDefaultInitConfigFile.size(),
								OhplGa9EmitOf<decltype(emit)>::Fn(), &emit);
	if(pInitCfg) hplDelete(pInitCfg);
	if(pInitCfgDefaults) hplDelete(pInitCfgDefaults);
	if(bOk == false) return false;
	mbAllowHardmode = bAllowHardmode;

	//////////////////////////////
	// Set up the directories, the base paths and the log files
#ifndef HPL_MINIMAL
	iFileBrowser::msGameDir = cPlatform::GetWorkingDir();
	iFileBrowser::msPersonalDir = PERSONAL_RELATIVEROOT PERSONAL_RELATIVEGAME_PARENT;
#endif

	tWString sConfigHome = cPlatform::GetSystemSpecialPath(eSystemPath_XDGConfigHome);
	tWString sStateHome = cPlatform::GetSystemSpecialPath(eSystemPath_XDGStateHome);
	tWString sMoveFrom;
	auto pathEmit = [&](uint32_t tag, const void *p, size_t n)
	{
		switch(tag)
		{
		case eOhplGa9Tag_PathMigrate: hpl::MigrateLegacyPersonalDir(OhplGa9EmitW(p, n)); break;
		case eOhplGa9Tag_PathCreateIfMissing:
			if(cPlatform::FolderExists(OhplGa9EmitW(p, n)) == false) cPlatform::CreateFolder(OhplGa9EmitW(p, n));
			break;
		case eOhplGa9Tag_PathMkdir: cPlatform::CreateFolder(OhplGa9EmitW(p, n)); break;
		case eOhplGa9Tag_PathMoveFrom: sMoveFrom = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_PathMoveTo: hpl::MoveIfMissing(sMoveFrom, OhplGa9EmitW(p, n)); break;
		case eOhplGa9Tag_PathBaseSave: msBaseSavePath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_PathBaseConfig: msBaseConfigPath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_PathBaseState: msBaseStatePath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_PathCrashFlag: msCrashFlagPath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_PathFirstStartFlag: msFirstStartFlagPath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_PathLogFile: SetLogFile(OhplGa9EmitW(p, n)); break;
		case eOhplGa9Tag_PathUpdateLogFile: SetUpdateLogFile(OhplGa9EmitW(p, n)); break;
		}
	};
	// PID suffix: concurrent runs truncate a shared log
	ohpl_ga9_plan_paths(OhplGa9StrOfW(sPersonalDir), OhplGa9StrOfW(sConfigHome), OhplGa9StrOfW(sStateHome),
						OhplGa9StrOfW(msMainSaveFolder), OhplGa9StrOfW(cString::To16Char(msCustomStoryPath)),
						getenv("OPENHPL_HEADLESS_SOCKET") != NULL, (int)getpid(),
						OhplGa9EmitOf<decltype(pathEmit)>::Fn(), &pathEmit);

	return true;
}

//-----------------------------------------------------------------------

bool cLuxBase::CheckFeatureSupport()
{
	iLowLevelGraphics *pLowLevelGfx = mpEngine->GetGraphics()->GetLowLevel();

	///////////////////////////////
	// Features
	tString sError;
	auto emit = [&](uint32_t tag, const void *p, size_t n) { sError = OhplGa9EmitS(p, n); };
	if(ohpl_ga9_feature_error(pLowLevelGfx->GetCaps(eGraphicCaps_ShaderModel_2),
								pLowLevelGfx->GetCaps(eGraphicCaps_MaxDrawBuffers),
								pLowLevelGfx->GetCaps(eGraphicCaps_PackedDepthStencil),
								OhplGa9EmitOf<decltype(emit)>::Fn(), &emit))
	{
		msErrorMessage = cString::To16Char(sError);
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------

cConfigFile* cLuxBase::LoadConfigFile(const tWString& asDefaultPath, const tWString& asWantedPath, bool abForceLoadDefault, bool *abDidLoadDefault)
{
	cConfigFile *pConfig;
	bool bLoadedWantedPath = false;
	
	//////////////////////
	//Check if wanted exist and created config using existing file
	if(ohpl_ga9_use_wanted_config(abForceLoadDefault, abForceLoadDefault==false && cPlatform::FileExists(asWantedPath)))
	{
		pConfig = hplNew( cConfigFile, (asWantedPath) );
		bLoadedWantedPath = true;
	}
	else
	{
#ifdef USERDIR_RESOURCES
		pConfig = hplNew( cConfigFile, (asDefaultPath, msUserResourceDir) );
#else
		pConfig = hplNew( cConfigFile, (asDefaultPath) );
#endif
	}

	////////////////////////////////////
	// Load the settings config file
	if(pConfig->Load()==false)
	{
		msErrorMessage = _W("Failed to load config file!");
		return NULL;
	}

	//Set correct path for saving.
	if(bLoadedWantedPath==false)
	{
		pConfig->SetFileLocation(asWantedPath);
	}
	if (abDidLoadDefault) {
		*abDidLoadDefault = !bLoadedWantedPath;
	}

	return pConfig;
}

//-----------------------------------------------------------------------

bool cLuxBase::InitMainConfig()
{
	///////////////////////////////////////////////////////////////
	//Check if last init crashed, load default main config if so
	// Skip this for now since there is a launcher
	//bool bLastInitCrashed = false; //CheckCrashFlag(); 
	
	/////////////////////////////////////////////////
	// Load the main settings
	mpMainConfig = LoadConfigFile(msDefaultMainConfigPath, msBaseConfigPath + _W("main_settings.cfg"),false);
	if(mpMainConfig==NULL) return false;

	//Load some basic variables
	auto mainCall = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r) { OhplGa9CfgCall(mpMainConfig, op, a, r); };
	auto mainEmit = [&](uint32_t tag, const void *p, size_t n) { msDefaultProfileName = OhplGa9EmitW(p, n); };
	bool vMain[5];
	ohpl_ga9_main_vars(&mainCall, OhplGa9CallOf<decltype(mainCall)>::Fn(), vMain, OhplGa9EmitOf<decltype(mainEmit)>::Fn(), &mainEmit);
	mbSaveConfigAtExit = vMain[0];
	mbExitMenuDirectly = vMain[1];
	mbShowPreMenu = vMain[2];
	mbShowMenu = vMain[3];

	SetUpdateLogActive(vMain[4]);
	
	////////////////////////////////////
	// Load the game config file
#ifdef USERDIR_RESOURCES
	mpGameCfg = hplNew( cConfigFile, (msGameConfigPath, msUserResourceDir) );
#else
	mpGameCfg = hplNew( cConfigFile, (msGameConfigPath) );
#endif
	if(mpGameCfg->Load()==false)
	{
		msErrorMessage = _W("Failed to load game config file!");
		return false;
	}

	////////////////////////////////////
	// Load the menu config file
#ifdef USERDIR_RESOURCES
	mpMenuCfg = hplNew( cConfigFile, (msMenuConfigPath, msUserResourceDir) );
#else
	mpMenuCfg = hplNew( cConfigFile, (msMenuConfigPath) );
#endif

	if(mpMenuCfg->Load()==false)
	{
		msErrorMessage = _W("Failed to load menu config file!");
		return false;
	}

	////////////////////////////////////
	// Load the demo config file
#ifdef LUX_DEMO_VERSION
	mpDemoCfg = hplNew( cConfigFile, (msDemoConfigPath) );
	if(mpDemoCfg->Load()==false)
	{
		msErrorMessage = _W("Failed to load demo config file!");
		return false;
	}
#endif

	///////////////////////////////////
	// Create and init config handler
	mpConfigHandler = hplNew(cLuxConfigHandler,());
	mpConfigHandler->LoadMainConfig();

	///////////////////////////////////
	// Ptest extra fixes!!
	if(mbPTestActivated)
	{
		#ifndef SKIP_PTEST_TESTS
			mbShowMenu = true;
			mbShowPreMenu = true;
			mpConfigHandler->mbLoadDebugMenu = false;
			mpConfigHandler->msLangFile = "english.lang";
		#endif
	}

	return true;
}

bool cLuxBase::InitUserConfig()
{
	////////////////////////////////////////////////
	// Clear previous config
	if(mpUserConfig) hplDelete(mpUserConfig);
	if(mpUserKeyConfig) hplDelete(mpUserKeyConfig);
	mpUserConfig = NULL;
	mpUserKeyConfig = NULL;

	////////////////////////////////////////////////
	// Check if a valid profile is set
	if(msProfileName.empty())
		return false;
	
	/////////////////////////////////////////////////
	// Load the user settings
	mpUserConfig = LoadConfigFile(msDefaultUserConfigPath, msMainProfileConfigPath +_W("user_settings.cfg") );
	if(mpUserConfig==NULL) return false;

	/////////////////////////
	//Load user key config
	bool bDidLoadDefault;
	mpUserKeyConfig = LoadConfigFile(msDefaultUserKeyConfigPath, msMainProfileConfigPath +_W("user_keys.cfg") , false, &bDidLoadDefault);
	if(mpUserKeyConfig==NULL) return false;

#ifdef __APPLE__
	// Heinous kludge to get a default Mac keyboard shortcut without relying on different config files.
	if (bDidLoadDefault) {
		mpUserKeyConfig->SetString("Attack", "Secondary", "Keyboard::LeftMeta");
	}
#endif

	/////////////////////////
	//Load the profile language --- REMOVED - Language is now loaded only at startup
	//LoadLanguage(mpUserConfig->GetString("Game", "Language", msDefaultGameLanguage));

	/////////////////////////
	//Let all modules load user config.
	RunModuleMessage(eLuxUpdateableMessage_LoadUserConfig);

	return true;
}	

void cLuxBase::ExitConfig()
{
	hplDelete(mpConfigHandler);
	Log(" Deleting config files.\n");
	hplDelete(mpMainConfig);
	if(mpUserConfig) hplDelete(mpUserConfig);
	if(mpUserKeyConfig) hplDelete(mpUserKeyConfig);
	hplDelete(mpGameCfg);
	hplDelete(mpMenuCfg);
	if(mpDemoCfg) hplDelete(mpDemoCfg);
}

//-----------------------------------------------------------------------

void cLuxBase::SaveConfig()
{
	/////////////////////////////////////
	//Save all stuff
	mpConfigHandler->SaveMainConfig();

	//////////////////////////////////
	//Make other modules save too!
	RunModuleMessage(eLuxUpdateableMessage_SaveMainConfig);
	if(mpUserConfig) RunModuleMessage(eLuxUpdateableMessage_SaveUserConfig);

	/////////////////////
	// Main variables
	mpMainConfig->SetBool("Main", "ShowMenu", mbShowMenu);
	mpMainConfig->SetBool("Main", "ShowPreMenu", mbShowPreMenu);

	mpMainConfig->SetBool("Main","UpdateLogActive",GetUpdateLogActive());

	/////////////////////
	// User variables
	//if(mpUserConfig) 
	//{
		//mpUserConfig->SetString("Game", "Language",msCurrentLanguage);
	//}
	
	
	///////////////////////
	//Save the config files
	Log(" Saving main config.\n");
	mpMainConfig->Save();
	if(mpUserConfig)
	{
		Log(" Saving user config.\n");
		mpUserConfig->Save();
		if(mpUserKeyConfig)
			mpUserKeyConfig->Save();
	}
}	

//-----------------------------------------------------------------------

bool cLuxBase::InitEngine()
{
	//////////////////////////////////////////////////////////////////
	//Prepare to init, create crash_flag just in case it actually crashes :)
	// Skip this now that we have a launcher.
	//RaiseCrashFlag();

	cEngineInitVars vars;
	vars.mGraphics.mvScreenSize =  mpConfigHandler->mvScreenSize;
	vars.mGraphics.mlDisplay = mpConfigHandler->mlDisplay;
	vars.mGraphics.mbFullscreen =  mpConfigHandler->mbFullscreen;
	vars.mGraphics.msWindowCaption = msGameName + " Loading...";
	vars.mGraphics.msRenderer = mpConfigHandler->msRenderer;
	vars.mGraphics.msColorOutput = mpConfigHandler->msColorOutput;
	vars.mGraphics.mfPaperWhite = mpConfigHandler->mfPaperWhite;
	vars.mGraphics.mfPeakNits = mpConfigHandler->mfPeakNits;

	vars.mSound.mlSoundDeviceID = mpConfigHandler->mlSoundDevID;
	vars.mSound.mlMaxChannels = mpConfigHandler->mlMaxSoundChannels;
	vars.mSound.mlStreamBufferCount = mpConfigHandler->mlSoundStreamBuffers;
	vars.mSound.mlStreamBufferSize = mpConfigHandler->mlSoundStreamBufferSize;

	// Sound device filter set here (if needed)
#if defined(WIN32)
	iLowLevelSound::SetSoundDeviceNameFilter("software");
#endif
	
	//renderer variables
	iRenderer::SetShadowMapQuality((eShadowMapQuality)mpConfigHandler->mlShadowQuality);
	iRenderer::SetShadowMapResolution((eShadowMapResolution)mpConfigHandler->mlShadowRes);
	
	iRenderer::SetParallaxQuality((eParallaxQuality)mpConfigHandler->mlParallaxQuality);
	iRenderer::SetParallaxEnabled(mpConfigHandler->mbParallaxEnabled);

	// before any cGuiSet exists; sets bake the scale in
	cGuiSet::SetGlobalGuiScale((float)mpConfigHandler->mlGuiScale);

	iRenderer::SetRefractionEnabled(mpConfigHandler->mbRefraction);

	cRendererDeferred::SetSSAOBufferSizeDiv(mpConfigHandler->mlSSAOResolution==0? 2 : 1);
	cRendererDeferred::SetSSAONumOfSamples(mpConfigHandler->mlSSAOSamples);
	cRendererDeferred::SetSSAOLoaded(mpConfigHandler->mbSSAOActive);
	cRendererDeferred::SetGBufferType((eDeferredGBuffer)mpMainConfig->GetInt("Graphics","GBufferType", eDeferredGBuffer_32Bit));
	cRendererDeferred::SetNumOfGBufferTextures(mpMainConfig->GetInt("Graphics","NumOfGBufferTextures", 3));
	cRendererDeferred::SetEdgeSmoothLoaded(mpConfigHandler->mbEdgeSmooth);

	cRendererDeferred::SetOcclusionTestLargeLights(mpConfigHandler->mbOcclusionTestLights);

	iLowLevelGraphics::SetForceShaderModel3And4Off(mpConfigHandler->mbForceShaderModel3And4Off);

	//Other vars
	cResources::SetForceCacheLoadingAndSkipSaving(mpConfigHandler->mbForceCacheLoadingAndSkipSaving);
	cResources::SetCreateAndLoadCompressedMaps(false);
	//cResources::SetCreateAndLoadCompressedMaps(mbPTestActivated || mpConfigHandler->mbCreateAndLoadCompressedMaps);
    
	/////////////////////////
	// Create the engine
	mpEngine = CreateHPLEngine(eHplAPI_OpenGL, eHplSetup_All, &vars);
	
	/////////////////////////
	// Set up more properties
	mpEngine->GetGraphics()->GetLowLevel()->SetVsyncActive(mpConfigHandler->mbVSync, mpConfigHandler->mbAdaptiveVSync);
	
	float fGamma = mpMainConfig->GetFloat("Graphics","Gamma", 1.0f);
	mpEngine->GetGraphics()->GetLowLevel()->SetGammaCorrection(fGamma);
	
	mpEngine->SetLimitFPS(mpMainConfig->GetBool("Engine","LimitFPS", false));
	mpEngine->SetWaitIfAppOutOfFocus(mpMainConfig->GetBool("Engine","SleepWhenOutOfFocus", true));

	cMaterialManager* pMatMgr = mpEngine->GetResources()->GetMaterialManager();
	pMatMgr->SetTextureSizeDownScaleLevel(mpConfigHandler->mlTextureQuality);
	pMatMgr->SetTextureFilter((eTextureFilter)mpConfigHandler->mlTextureFilter);
	pMatMgr->SetTextureAnisotropy(mpConfigHandler->mfTextureAnisotropy);
	
	cSound *pSound = mpEngine->GetSound();
	pSound->GetLowLevel()->SetVolume(mpMainConfig->GetFloat("Sound","Volume",1.0f));

	/////////////////////////
	//Load configurations
#ifdef USERDIR_RESOURCES
	mpEngine->GetResources()->LoadResourceDirsFile(msResourceConfigPath, msUserResourceDir);
#else
	mpEngine->GetResources()->LoadResourceDirsFile(msResourceConfigPath);
#endif

	mpEngine->GetPhysics()->LoadSurfaceData(msMaterialConfigPath);

	/////////////////////////
	//Load the start language
	//if(mbPTestActivated)
	//	LoadLanguage("english.lang");
	//else
		LoadLanguage(mpMainConfig->GetString("Main", "StartLanguage",msDefaultGameLanguage));

	/////////////////////////
	//Add extra paths
	//TO add...?

	/////////////////////////
	// Misc set up
	mpEngine->GetResources()->GetMeshManager()->SetFastloadMaterial("fastload_material.mat");

	////////////////////////////////////////////////
	// Seems there was no crash, set flag to false
	// Skip crash flag for now, as there is a launcher
	//LowerCrashFlag();

	////////////////////////////////////////////////
	// Check stuff matches after engine creation
	iSoundDeviceIdentifier* pSndDevice = mpEngine->GetSound()->GetLowLevel()->GetCurrentSoundDevice();
	mpConfigHandler->mlSoundDevID = pSndDevice->GetID();
	
	return true;
}

void cLuxBase::ExitEngine()
{
	if(mpEngine) DestroyHPLEngine(mpEngine);
}

//-----------------------------------------------------------------------

//Helper functions to more easily create modules.

#define CreateModule( aClassType, asContainer ) (aClassType*)AddModule(hplNew(aClassType, () ), asContainer);
#define CreateGlobalModule( aClassType) (aClassType*)AddGlobalModule(hplNew(aClassType, () ));

//-----------------------------------------------------------------------

bool cLuxBase::InitGame()
{
	///////////////////////////////////////
	// Demo output
	#ifdef LUX_DEMO_VERSION
		Log("- DEMO VERSION -\n");
	#endif

	///////////////////////////////////////
	// Create game data
	mvHudVirtualCenterSize = cVector2f(800,600);
	LuxCalcGuiSetScreenOffset(mvHudVirtualCenterSize, mvHudVirtualSize, mvHudVirtualOffset);
	mvHudVirtualStartPos = cVector3f(-mvHudVirtualOffset.x,-mvHudVirtualOffset.y,0);

	mpGameHudSet = mpEngine->GetGui()->CreateSet("GameHud",NULL);
	// fixed 800x600 layout: exempt from GuiScale
	mpGameHudSet->SetVirtualSize(mvHudVirtualSize,-1000, 1000, mvHudVirtualOffset, true);
	mpGameDebugSet = mpEngine->GetGui()->CreateSet("GameDebug",NULL);
	mpGameDebugSet->SetDrawPriority(1);
	mpGameHudSet->SetDrawPriority(0);
	
	/////////////////////////////
	// Load another font if game is in chinese, 
	// might want to fix this later since some debug text is tiny now

	if (ohpl_ga9_chinese_font((const unsigned char*)msCurrentLanguage.data(), msCurrentLanguage.size()))
		mpDefaultFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("game_default.fnt");
	else
		mpDefaultFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("font_default.fnt");

	///////////////////////////////////////
	// Create updater containers
	mpEngine->GetUpdater()->AddContainer("PreMenu");
	mpEngine->GetUpdater()->AddContainer("MainMenu");	
	mpEngine->GetUpdater()->AddContainer("Inventory");
	mpEngine->GetUpdater()->AddContainer("Journal");
	mpEngine->GetUpdater()->AddContainer("Credits");
	mpEngine->GetUpdater()->AddContainer("LoadScreen");
#ifdef LUX_DEMO_VERSION
	mpEngine->GetUpdater()->AddContainer("DemoEnd");
#endif

	///////////////////////////////////////
	// Create Modules

	//Global
	mpInputHandler = CreateGlobalModule( cLuxInputHandler);
	mpHelpFuncs = CreateGlobalModule( cLuxHelpFuncs);
	mpSaveHandler = CreateGlobalModule( cLuxSaveHandler);
	mpScriptHandler = CreateGlobalModule( cLuxScriptHandler);
	mpProgressLogHandler = CreateGlobalModule( cLuxProgressLogHandler);
	
	//Default
	mpMapHandler = CreateModule( cLuxMapHandler, "Default");
	mpMapHelper = CreateModule( cLuxMapHelper, "Default");
	mpPlayer = CreateModule( cLuxPlayer, "Default");
	mpInsanityHandler = CreateModule( cLuxInsanityHandler, "Default"); 
	mpDebugHandler = CreateModule( cLuxDebugHandler, "Default");
	mpEffectRenderer = CreateModule( cLuxEffectRenderer, "Default");
	mpMusicHandler = CreateModule( cLuxMusicHandler, "Default");
	mpMessageHandler = CreateModule( cLuxMessageHandler, "Default");
	mpEffectHandler = CreateModule( cLuxEffectHandler, "Default");
	mpCompletionCountHandler = CreateModule( cLuxCompletionCountHandler, "Default"); 
	mpGlobalDataHandler = CreateModule( cLuxGlobalDataHandler, "Default"); 
	mpHintHandler = CreateModule( cLuxHintHandler, "Default"); 
	mpPostEffectHandler = CreateModule( cLuxPostEffectHandler, "Default"); 
	mpAchievementHandler = CreateModule( iLuxAchievementHandler, "Default");

	InitAchievements();
	
	//PreMenu
	mpPreMenu = CreateModule( cLuxPreMenu, "PreMenu");

	//Main Menu
	mpMainMenu = CreateModule( cLuxMainMenu, "MainMenu");

	//Inventory
	mpInventory = CreateModule( cLuxInventory, "Inventory");

	//Journal
	mpJournal = CreateModule( cLuxJournal, "Journal");

	//Credits
#ifndef LUX_DEMO_VERSION
	mpCredits = CreateModule( cLuxCredits, "Credits");
#endif

	//Demo End
	mpDemoEnd = NULL;
#ifdef LUX_DEMO_VERSION
	mpDemoEnd = CreateModule( cLuxDemoEnd, "DemoEnd");
#endif

	//Load screen
	mpLoadScreenHandler = CreateModule( cLuxLoadScreenHandler, "LoadScreen");


	//Make sure all of the modules are reset!
	mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset);

	///////////////////////////////////////
	// Create and Add Loaders
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxStaticPropLoader, ("StaticProp")));

    mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_Object, ("Object")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_SwingDoor, ("SwingDoor")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_Lever, ("Lever")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_Wheel, ("Wheel")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_Lamp, ("Lamp")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_Photocell, ("Photocell")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_MoveObject, ("MoveObject")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_Item, ("Item")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_Chest, ("Chest")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_LevelDoor, ("LevelDoor")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_Button, ("Button")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_OilBarrel, ("OilBarrel")));
	
#ifndef LUX_DEMO_VERSION
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_EmotionStone, ("EmotionStone")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_NPC, ("NPC")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_MultiSlider, ("MultiSlider")));
#endif
	
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_Critter_Bug, ("CritterBug")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxPropLoader_Critter_Spider, ("CritterSpider")));

	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxEnemyLoader_Grunt, ("Enemy_Grunt")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxEnemyLoader_WaterLurker, ("Enemy_WaterLurker")));
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxEnemyLoader_ManPig, ("Enemy_ManPig")));

#ifndef LUX_DEMO_VERSION
	mpEngine->GetResources()->AddEntityLoader(hplNew(cLuxCommentaryIconLoader, ("CommentaryIcon")));
#endif

	mpEngine->GetResources()->AddAreaLoader(hplNew(cLuxAreaNodeLoader_PlayerStart, ("PlayerStart")));
	mpEngine->GetResources()->AddAreaLoader(hplNew(cLuxAreaNodeLoader_PathNode, ("PathNode")));
	mpEngine->GetResources()->AddAreaLoader(hplNew(cLuxAreaNodeLoader_PosNode, ("PosNode")));

	mpEngine->GetResources()->AddAreaLoader(hplNew(cLuxAreaLoader_Script, ("Script")));
	mpEngine->GetResources()->AddAreaLoader(hplNew(cLuxAreaLoader_Flashback, ("Flashback")));
#ifndef LUX_DEMO_VERSION
	mpEngine->GetResources()->AddAreaLoader(hplNew(cLuxAreaLoader_Ladder, ("Ladder")));
#endif
	mpEngine->GetResources()->AddAreaLoader(hplNew(cLuxAreaLoader_Liquid, ("Liquid")));
#ifndef LUX_DEMO_VERSION
	mpEngine->GetResources()->AddAreaLoader(hplNew(cLuxAreaLoader_Sticky, ("Sticky")));
#endif
	mpEngine->GetResources()->AddAreaLoader(hplNew(cLuxAreaLoader_Insanity, ("Insanity")));
	mpEngine->GetResources()->AddAreaLoader(hplNew(cLuxAreaLoader_Examine, ("Examine")));
	mpEngine->GetResources()->AddAreaLoader(hplNew(cLuxAreaLoader_Sign, ("Sign")));
	mpEngine->GetResources()->AddAreaLoader(hplNew(cLuxAreaLoader_SlimeDamage, ("SlimeDamage")));
	mpEngine->GetResources()->AddAreaLoader(hplNew(cLuxAreaRopeLoader, ("Rope")));
	

	RunModuleMessage(eLuxUpdateableMessage_LoadFonts);
	///////////////////////////////////////////////////////
	// Run the LoadMainConfig message for game modules, couldn't be run before
	RunModuleMessage(eLuxUpdateableMessage_LoadMainConfig);

	return true;
}


//-----------------------------------------------------------------------

void cLuxBase::ExitGame()
{
	Log(" Deleting game modules.\n");
	for(size_t i=0; i<mvModules.size(); ++i)
	{
		Log("   '%s'\n", mvModules[i]->GetName().c_str());
		hplDelete( mvModules[i]);
	}
}

//-----------------------------------------------------------------------

void cLuxBase::InitOver()
{
	//Set proper caption when loading is done.
    gpBase->mpEngine->GetGraphics()->GetLowLevel()->SetWindowCaption(msGameName);

}

//-----------------------------------------------------------------------

bool cLuxBase::CreateProfile(const tWString& asName)
{
	tWString sPath;
	auto emit = [&](uint32_t tag, const void *p, size_t n) { sPath = OhplGa9EmitW(p, n); };

	if(ohpl_ga9_profile_folder(OhplGa9StrOfW(msBaseSavePath), OhplGa9StrOfW(asName),
								cPlatform::FolderExists(msBaseSavePath + asName + _W("/")),
								OhplGa9EmitOf<decltype(emit)>::Fn(), &emit) == false)
		return false;

	cPlatform::CreateFolder(sPath);

	return true;
}

void cLuxBase::SetProfile(const tWString& asName)
{
	tWString sMoveFrom;
	auto emit = [&](uint32_t tag, const void *p, size_t n)
	{
		switch(tag)
		{
		case eOhplGa9Tag_ProfileSavePrevious:
			//If a profile is already set, save it.
			if(n)
			{
				// Commented out cos there's no language setting in user config now
				//Save the config files
				Log(" Saving user config.\n");
				mpUserConfig->Save();
				if(mpUserKeyConfig)
					mpUserKeyConfig->Save();
			}
			break;
		case eOhplGa9Tag_ProfileMainSave: msMainProfileSavePath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_ProfileMainConfig: msMainProfileConfigPath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_ProfileSave: msProfileSavePath = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_PathMkdir: cPlatform::CreateFolder(OhplGa9EmitW(p, n)); break;
		case eOhplGa9Tag_PathMoveFrom: sMoveFrom = OhplGa9EmitW(p, n); break;
		case eOhplGa9Tag_PathMoveTo: hpl::MoveIfMissing(sMoveFrom, OhplGa9EmitW(p, n)); break;
		}
	};
	ohpl_ga9_set_profile(OhplGa9StrOfW(msProfileName), mpUserConfig != NULL, OhplGa9StrOfW(asName),
							OhplGa9StrOfW(msBaseSavePath), OhplGa9StrOfW(msBaseConfigPath),
							OhplGa9EmitOf<decltype(emit)>::Fn(), &emit);

	msProfileName = asName;
	if(msProfileName!=_W(""))
	{
		Log(" Setting profile: '%s' Path: '%s'\n",cString::To8Char(asName).c_str(), cString::To8Char(msMainProfileSavePath).c_str());
	}
}

//-----------------------------------------------------------------------

void cLuxBase::PreloadSound(const tString &asFile)
{
	if(asFile=="") return;
	gpBase->mpEngine->GetResources()->GetSoundEntityManager()->Preload(asFile);
}

void cLuxBase::PreloadParticleSystem(const tString &asFile)
{
	if(asFile=="") return;
	gpBase->mpEngine->GetResources()->GetParticleManager()->Preload(asFile);	
}

//-----------------------------------------------------------------------

void cLuxBase::SetCustomStory(cLuxCustomStorySettings* apCustomStory)
{
	//Debug
	//Log("Trying to set custom story 0x%x\n", apCustomStory);

	if(mpCustomStory==apCustomStory)
		return;

	//Debug
	//Log(" Not current one, setting up\n", apCustomStory);

	if(mpCustomStory)
	{
		hplDelete(mpCustomStory);
		mpCustomStory = NULL;
	}

	mpCustomStory = apCustomStory;

	if(mpCustomStory)
	{
		auto emit = [&](uint32_t tag, const void *p, size_t n) { msProfileSavePath = OhplGa9EmitW(p, n); };
		ohpl_ga9_custom_story_save_path((const uint32_t*)msMainProfileSavePath.data(), msMainProfileSavePath.size(),
										(const uint32_t*)mpCustomStory->msStoryRootFolder.data(), mpCustomStory->msStoryRootFolder.size(),
										OhplGa9EmitOf<decltype(emit)>::Fn(), &emit);

		mpEngine->GetResources()->AddResourceDir(mpCustomStory->msStoryRootFolder, true);
	}
	else
		msProfileSavePath = msMainProfileSavePath;

	LoadLanguage(msCurrentLanguage, true);
}

//-----------------------------------------------------------------------

bool cLuxBase::LoadLanguage(const tString& asName, bool abForceReload)
{
	cResources *pResources = mpEngine->GetResources();

	tString sStoryRoot, sExtraPrefix, sDefaultExtra;
	OhplGa9LangArgs args = {};
	args.name = OhplGa9StrOf(asName);
	args.force = abForceReload;
	args.current = OhplGa9StrOf(msCurrentLanguage);
	args.has_story = mpCustomStory != NULL;
	if(mpCustomStory)
	{
		sStoryRoot = cString::To8Char(mpCustomStory->msStoryRootFolder);
		args.story_root = OhplGa9StrOf(sStoryRoot);
		args.story_prefix = OhplGa9StrOf(mpCustomStory->msExtraLangFilePrefix);
		args.story_default = OhplGa9StrOf(mpCustomStory->msDefaultExtraLanguage);
	}
	args.game_folder = OhplGa9StrOf(msGameLanguageFolder);
	args.base_folder = OhplGa9StrOf(msBaseLanguageFolder);
	args.default_game = OhplGa9StrOf(msDefaultGameLanguage);
	args.default_base = OhplGa9StrOf(msDefaultBaseLanguage);

	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r)
	{
		switch(op)
		{
		case eOhplGa9Op_LangClear: pResources->ClearTranslations(); break;
		case eOhplGa9Op_LangFound: r->i = gpBase->mpEngine->GetResources()->GetFileSearcher()->GetFilePath(OhplGa9GetS(a[0])) != _W(""); break;
		case eOhplGa9Op_LangAdd:
			if(a[2].i) pResources->AddLanguageFile(OhplGa9GetS(a[0]), a[1].i != 0, msUserResourceDir);
			else pResources->AddLanguageFile(OhplGa9GetS(a[0]), a[1].i != 0);
			break;
		}
	};
	tString sNewLanguage;
	auto emit = [&](uint32_t tag, const void *p, size_t n) { sNewLanguage = OhplGa9EmitS(p, n); };
	if(ohpl_ga9_load_language(&call, OhplGa9CallOf<decltype(call)>::Fn(), &args, OhplGa9EmitOf<decltype(emit)>::Fn(), &emit) == false)
		return false;

	msCurrentLanguage = sNewLanguage;

	return true;
}


//-----------------------------------------------------------------------


iLuxUpdateable* cLuxBase::AddModule(iLuxUpdateable *apModule, const tString& asContainer)
{
	mpEngine->GetUpdater()->AddUpdate(asContainer, apModule);
	mvModules.push_back(apModule);
	return apModule;
}

iLuxUpdateable* cLuxBase::AddGlobalModule(iLuxUpdateable *apModule)
{
	mpEngine->GetUpdater()->AddGlobalUpdate(apModule);
	mvModules.push_back(apModule);
	return apModule;
}
//-----------------------------------------------------------------------

void cLuxBase::RaiseCrashFlag()
{
	FILE* pCrashFlagFile = cPlatform::OpenFile(msCrashFlagPath.c_str(), _W("w"));
	fclose(pCrashFlagFile);
}

void cLuxBase::LowerCrashFlag()
{
	cPlatform::RemoveFile(msCrashFlagPath);
}

bool cLuxBase::CheckCrashFlag()
{
	return cPlatform::FileExists(msCrashFlagPath);
}

//-----------------------------------------------------------------------

void cLuxBase::RaiseFirstStartFlag()
{
	if(CheckFirstStartFlag())
		return;

	FILE* pFirstStartFlagFile = cPlatform::OpenFile(msFirstStartFlagPath.c_str(), _W("w"));
	fclose(pFirstStartFlagFile);
}

void cLuxBase::LowerFirstStartFlag()
{
	cPlatform::RemoveFile(msFirstStartFlagPath);
}

bool cLuxBase::CheckFirstStartFlag()
{
	return cPlatform::FileExists(msFirstStartFlagPath);
}

void cLuxBase::InitAchievements()
{
	if(mpAchievementHandler == NULL) return;

	int lId = 0;
	tString sName;
	auto emit = [&](uint32_t tag, const void *p, size_t n) { sName = OhplGa9EmitS(p, n); };
	for(size_t i=0; ohpl_ga9_achievement(i, &lId, OhplGa9EmitOf<decltype(emit)>::Fn(), &emit); ++i)
	{
		mpAchievementHandler->CreateAchievement((eLuxAchievement)lId, sName);
	}

	mpAchievementHandler->RegisterAchievements();
}
