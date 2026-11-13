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

#include "engine/Engine.h"

#include "system/EngineDiagnostics.h"
#include "system/System.h"
#include "sound/Sound.h"
#include "physics/Physics.h"
#include "ai/AI.h"
#include "resources/Resources.h"
#include "graphics/Graphics.h"
#include "gui/Gui.h"
#include "scene/Scene.h"

#include "system/LogicTimer.h"
#include "system/String.h"
#include "system/Platform.h"
#include "system/Timer.h"
#include "system/Mutex.h"

#include "input/Input.h"
#include "input/Mouse.h"

#include "graphics/LowLevelGraphics.h"
#include "graphics/Renderer.h"

#include "engine/Updater.h"
#include "engine/ScriptFuncs.h"
#include "engine/EngineInitVars.h"

#include "system/LowLevelSystem.h"
#include "engine/LowLevelEngineSetup.h"
#include "system/HeadlessControl.h"
#include "engine/DevHud.h"

#include "impl/SDLEngineSetup.h"

#include <chrono>
#include <cstdlib>

static_assert(hpl::eVariableType_LastEnum == 8, "ohpl_en variable types");
static_assert(hpl::eUpdateableMessage_AppGotInputFocus == 11 && hpl::eUpdateableMessage_AppGotMouseFocus == 12 &&
			  hpl::eUpdateableMessage_AppGotVisibility == 13 && hpl::eUpdateableMessage_AppLostInputFocus == 14 &&
			  hpl::eUpdateableMessage_AppLostMouseFocus == 15 && hpl::eUpdateableMessage_AppLostVisibility == 16,
			  "ohpl_en focus messages");

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// FPS COUNTER
	//////////////////////////////////////////////////////////////////////////

	cFPSCounter::cFPSCounter(iLowLevelSystem* apLowLevelSystem)
	{
		mpLowLevelSystem = apLowLevelSystem;
		ohpl_en_fps_init(&mFps, cPlatform::GetApplicationTime());
	}

	void cFPSCounter::AddFrame()
	{
		ohpl_en_fps_add_frame(&mFps, cPlatform::GetApplicationTime());
	}

	//////////////////////////////////////////////////////////////////////////
	// SETUP VAR CONTAINER
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSetupVarContainer::cSetupVarContainer()
	{
		mpVars = ohpl_en_vars_new();
	}

	cSetupVarContainer::~cSetupVarContainer()
	{
		ohpl_en_vars_free(mpVars);
	}

	//-----------------------------------------------------------------------

	void cSetupVarContainer::AddString(const tString& asName, const tString& asValue)
	{
		ohpl_en_vars_add_string(mpVars, OhplEnP(asName), asName.size(), OhplEnP(asValue), asValue.size());
	}

	void cSetupVarContainer::AddInt(const tString& asName, int alValue)
	{
		ohpl_en_vars_add_int(mpVars, OhplEnP(asName), asName.size(), alValue);
	}
	void cSetupVarContainer::AddFloat(const tString& asName, float afValue)
	{
		ohpl_en_vars_add_float(mpVars, OhplEnP(asName), asName.size(), afValue);
	}
	void cSetupVarContainer::AddBool(const tString& asName, bool abValue)
	{
		ohpl_en_vars_add_bool(mpVars, OhplEnP(asName), asName.size(), abValue);
	}

	//-----------------------------------------------------------------------


	tString cSetupVarContainer::GetString(const tString& asName)
	{
		tString sVal;
		ohpl_en_vars_get_string(mpVars, OhplEnP(asName), asName.size(), OhplEnAssign, &sVal);
		return sVal;
	}

	float cSetupVarContainer::GetFloat(const tString& asName, float afDefault)
	{
		return ohpl_en_vars_get_float(mpVars, OhplEnP(asName), asName.size(), afDefault);
	}
	int cSetupVarContainer::GetInt(const tString& asName, int alDefault)
	{
		return ohpl_en_vars_get_int(mpVars, OhplEnP(asName), asName.size(), alDefault);
	}
	bool cSetupVarContainer::GetBool(const tString& asName, bool abDefault)
	{
		return ohpl_en_vars_get_bool(mpVars, OhplEnP(asName), asName.size(), abDefault);
	}

	//////////////////////////////////////////////////////////////////////////
	// GLOBAL FUCNTIONS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cEngine* CreateHPLEngine(eHplAPI aApi, tFlag alHplModuleFlags, cEngineInitVars *apVars)
	{
		iLowLevelEngineSetup *pGameSetup = NULL;

		switch(aApi)
		{
			case eHplAPI_OpenGL: pGameSetup = hplNew(cSDLEngineSetup, (alHplModuleFlags, apVars) ); break;
		}

		return hplNew( cEngine,  (pGameSetup,alHplModuleFlags, apVars) ); 
	}

	//-----------------------------------------------------------------------

	void DestroyHPLEngine(cEngine* apGame)
	{
		hplDelete( apGame );
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool cEngine::mbDevicePlugged = false;
	bool cEngine::mbDeviceRemoved = false;

	//-----------------------------------------------------------------------

	void cEngine::SetDevHudActive(bool abX)
	{
		const char *pEnv = getenv("OPENHPL_DEV_HUD");
		mbDevHudActive = ohpl_en_dev_hud_active(abX, pEnv && pEnv[0] ? (unsigned char)pEnv[0] : -1);
	}

	//-----------------------------------------------------------------------

	cEngine::cEngine(iLowLevelEngineSetup *apGameSetup,tFlag alHplSetupFlags, cEngineInitVars *apVars)
	{
		mpHeadlessControl = NULL;
		mlRustFocusBits = 0;

		GameInit(apGameSetup,alHplSetupFlags, apVars);

		//Set up variables
		mbWaitIfAppOutOfFocus = false;

		const char *pHeadlessSocketPath = getenv("OPENHPL_HEADLESS_SOCKET");
		if(pHeadlessSocketPath != NULL && pHeadlessSocketPath[0] != '\0')
		{
			mpHeadlessControl = hplNew(cHeadlessControlServer, (this, tString(pHeadlessSocketPath)));
		}

		mpDevHud = hplNew(cDevHud, (this));
		SetDevHudActive(false);


		mvEngineTypeStrings.resize(eVariableType_LastEnum);
		for(int i=0; i<(int)eVariableType_LastEnum; ++i)
			ohpl_en_engine_type_name(i, OhplEnAssign, &mvEngineTypeStrings[i]);
	}


	//-----------------------------------------------------------------------

	void cEngine::GameInit(iLowLevelEngineSetup *apGameSetup,tFlag alHplSetupFlags, cEngineInitVars *apVars)
	{
		mpGameSetup = apGameSetup;

		mRustInit.mlSetupFlags = alHplSetupFlags;
		mRustInit.mpVars = apVars;
		ohpl_h22_engine_init(RustCall, this);
	}

	//-----------------------------------------------------------------------

	cEngine::~cEngine()
	{
		ohpl_h22_engine_exit(RustCall, this);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHOD
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cEngine::Run()
	{
		RunRust();
	}
	//-----------------------------------------------------------------------

	void cEngine::RunRust()
	{
		ohpl_h22_engine_run(RustCall, this, &mLoop, &mfFrameTime, &mlRustFocusBits);
	}

	double cEngine::RustCall(void *apCtx, int alOp, double a0, double a1, double a2, double a3)
	{
		if(alOp == OHPL_H22_OP_STEP)
		{
			static_cast<cEngine*>(apCtx)->RustStep((int)a0, (int)a1);
			return 0;
		}
		return static_cast<cEngine*>(apCtx)->RustHost(alOp, a0, a1, a2, a3);
	}

	double cEngine::RustHost(int alOp, double a0, double a1, double a2, double a3)
	{
		static const std::chrono::steady_clock::time_point tBase = std::chrono::steady_clock::now();
		switch(alOp)
		{
		case OHPL_H22_OP_LOG:
			switch((int)a0)
			{
			case 0: Log("--------------------------------------------------------\n\n"); break;
			case 1: Log("Game Running\n"); break;
			case 2: Log("--------------------------------------------------------\n"); break;
			case 3: Log("Statistics\n"); break;
			case 4: Log("User Exit\n"); break;
			}
			return 0;
		case OHPL_H22_OP_LOG_MEDIUM: Log(" Medium framerate: %f\n", a0); return 0;
		case OHPL_H22_OP_BROADCAST: mpUpdater->BroadcastMessageToAll((eUpdateableMessage)(int)a0); return 0;
		case OHPL_H22_OP_RUN_MESSAGE: mpUpdater->RunMessage((eUpdateableMessage)(int)a0, (float)a1); return 0;
		case OHPL_H22_OP_CONTAINER_IS_DEFAULT: return mpUpdater->GetCurrentContainerName() == "Default";
		case OHPL_H22_OP_LT_RESET: mpLogicTimer->Reset(); return 0;
		case OHPL_H22_OP_WANT_UPDATE: return mpLogicTimer->WantUpdate();
		case OHPL_H22_OP_END_UPDATE_LOOP: mpLogicTimer->EndUpdateLoop(); return 0;
		case OHPL_H22_OP_STEP_SIZE: return GetStepSize();
		case OHPL_H22_OP_GAME_IS_DONE: return GetGameIsDone();
		case OHPL_H22_OP_WAIT_OUT_OF_FOCUS: return mbWaitIfAppOutOfFocus;
		case OHPL_H22_OP_WINDOW_INPUT_FOCUS: return mpGraphics->GetLowLevel()->GetWindowInputFocus();
		case OHPL_H22_OP_SLEEP: cPlatform::Sleep((unsigned int)a0); return 0;
		case OHPL_H22_OP_INPUT_UPDATE: mpInput->Update((float)a0); return 0;
		case OHPL_H22_OP_PAUSED: return GetPaused();
		case OHPL_H22_OP_HAS_HEADLESS: return mpHeadlessControl != NULL;
		case OHPL_H22_OP_HC_LOGIC_STEP: mpHeadlessControl->LogicStep(); return 0;
		case OHPL_H22_OP_HC_UPDATE: mpHeadlessControl->Update(); return 0;
		case OHPL_H22_OP_QUIT_POSTED: return mpInput->isQuitMessagePosted();
		case OHPL_H22_OP_RESET_QUIT: mpInput->resetQuitMessagePosted(); return 0;
		case OHPL_H22_OP_WINDOW_FOCUS_BITS:
		{
			iLowLevelGraphics *pllGfx = mpGraphics->GetLowLevel();
			bool bInput = pllGfx->GetWindowInputFocus();
			bool bMouse = pllGfx->GetWindowMouseFocus();
			bool bVisible = pllGfx->GetWindowIsVisible();
			return (bInput ? 1 : 0) | (bMouse ? 2 : 0) | (bVisible ? 4 : 0);
		}
		case OHPL_H22_OP_TAKE_PLUGGED: { bool b = mbDevicePlugged; mbDevicePlugged = false; return b; }
		case OHPL_H22_OP_TAKE_REMOVED: { bool b = mbDeviceRemoved; mbDeviceRemoved = false; return b; }
		case OHPL_H22_OP_UPDATE_LOG_ACTIVE: return GetUpdateLogActive();
		case OHPL_H22_OP_CLEAR_UPDATE_LOG: ClearUpdateLogFile(); return 0;
		case OHPL_H22_OP_RENDER_ONCE: return mbRenderOnce;
		case OHPL_H22_OP_LIMIT_FPS: return mbLimitFPS;
		case OHPL_H22_OP_SWAP: mpGraphics->GetLowLevel()->SwapBuffers(); return 0;
		case OHPL_H22_OP_TIMING:
		{
			static const char *vNames[6] = { "WaitAndFinishRendering", "SwapBuffers", "OnDraw", "RenderAll", "PostRender", "FlushRender" };
			int lTag = (int)a0;
			if(a1 != 0)
			{
				LogUpdate("Updating %s in file %s at line %d\n", vNames[lTag], __FILE__, __LINE__);
				mlRustTimingStart[lTag] = cPlatform::GetApplicationTime();
			}
			else LogUpdate(" Time spent: %d ms\n", cPlatform::GetApplicationTime() - mlRustTimingStart[lTag]);
			return 0;
		}
		case OHPL_H22_OP_FT_START: mpFrameTimer->Start(); return 0;
		case OHPL_H22_OP_FT_STOP: mpFrameTimer->Stop(); return 0;
		case OHPL_H22_OP_FT_SEC: return mpFrameTimer->GetTimeInSec();
		case OHPL_H22_OP_SCENE_RENDER: mpScene->Render((float)a0, tSceneRenderFlag_All); return 0;
		case OHPL_H22_OP_DEVHUD_RENDER: mpDevHud->Render(mbDevHudActive); return 0;
		case OHPL_H22_OP_FLUSH: mpGraphics->GetLowLevel()->FlushRendering(); return 0;
		case OHPL_H22_OP_DIAG_END_FRAME: cEngineDiagnostics::EndFrame(); return 0;
		case OHPL_H22_OP_DIAG_ADD_TIMING: cEngineDiagnostics::AddFrameTiming(a0, (int)a1, a2, a3); return 0;
		case OHPL_H22_OP_FPS_ADD: mpFPSCounter->AddFrame(); return 0;
		case OHPL_H22_OP_APP_TIME: return (double)cPlatform::GetApplicationTime();
		case OHPL_H22_OP_STEADY_NS: return (double)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - tBase).count();
		}
		return 0;
	}

	void cEngine::RustStep(int alStep, int alModule)
	{
		cEngineInitVars *apVars = mRustInit.mpVars;
		static const char vRule[] = "--------------------------------------------------------\n";
		switch(alStep)
		{
		case 0: Log("Creating Engine Modules\n"); break;
		case 1: Log("%s", vRule); break;
		case 2: Log("%s\n", vRule); break;
		case 3: Log(" Creating graphics module\n"); mpGraphics = mpGameSetup->CreateGraphics(); break;
		case 4: Log(" Creating system module\n"); mpSystem = mpGameSetup->CreateSystem(); break;
		case 5: Log(" Creating resource module\n"); mpResources = mpGameSetup->CreateResources(mpGraphics); break;
		case 6: Log(" Creating input module\n"); mpInput = mpGameSetup->CreateInput(mpGraphics); break;
		case 7: Log(" Creating sound module\n"); mpSound = mpGameSetup->CreateSound(); break;
		case 8: Log(" Creating physics module\n"); mpPhysics = mpGameSetup->CreatePhysics(); break;
		case 9: Log(" Creating ai module\n"); mpAI = mpGameSetup->CreateAI(); break;
		case 10: Log(" Creating gui module\n"); mpGui = hplNew(cGui,()); break;
		case 11: Log(" Creating scene module\n"); mpScene = mpGameSetup->CreateScene(mpGraphics, mpResources, mpSound,mpPhysics,mpSystem,mpAI,mpGui); break;
		case 12: mpResources->Init(mpGraphics,mpSystem, mpSound,mpScene,mpGui, mpPhysics); break;
		case 13:
			mpGraphics->Init(	apVars->mGraphics.mvScreenSize.x,
								apVars->mGraphics.mvScreenSize.y,
								apVars->mGraphics.mlDisplay,
								apVars->mGraphics.mlScreenBpp,
								apVars->mGraphics.mbFullscreen,
								apVars->mGraphics.mlMultisampling,
								apVars->mGraphics.mGpuProgramFormat,
								apVars->mGraphics.msWindowCaption,
								apVars->mGraphics.mvWindowPosition,
								mpResources,mRustInit.mlSetupFlags);
			break;
		case 14:
			mpSound->Init(mpResources, apVars->mSound.mlSoundDeviceID,
							apVars->mSound.mbUseEnvironmentalAudio,
							apVars->mSound.mlMaxChannels,
							apVars->mSound.mlStreamUpdateFreq,
							apVars->mSound.mbUseThreading,
							apVars->mSound.mbUseVoiceManagement,
							apVars->mSound.mlMaxMonoChannelsHint,
							apVars->mSound.mlMaxStereoChannelsHint,
							apVars->mSound.mlStreamBufferSize,
							apVars->mSound.mlStreamBufferCount,
							apVars->mSound.mbLowLevelLogging);
			break;
		case 15: mpPhysics->Init(mpResources); break;
		case 16: mpAI->Init(); break;
		case 17: mpGui->Init(mpResources,mpGraphics,mpSound,mpScene, mpInput); break;
		case 18: Log("Initializing Game Module\n"); break;
		case 19: Log(" Adding engine updates\n"); break;
		case 20: mpUpdater = hplNew( cUpdater,(mpSystem->GetLowLevel())); break;
		case 21:
			switch(alModule)
			{
			case 0: mpUpdater->AddGlobalUpdate(mpInput); break;
			case 1: mpUpdater->AddGlobalUpdate(mpPhysics); break;
			case 2: mpUpdater->AddGlobalUpdate(mpScene); break;
			case 3: mpUpdater->AddGlobalUpdate(mpGraphics); break;
			case 4: mpUpdater->AddGlobalUpdate(mpSound); break;
			case 5: mpUpdater->AddGlobalUpdate(mpAI); break;
			case 6: mpUpdater->AddGlobalUpdate(mpGui); break;
			case 7: mpUpdater->AddGlobalUpdate(mpResources); break;
			}
			break;
		case 22: mpUpdater->AddContainer("Default"); mpUpdater->SetContainer("Default"); break;
		case 23: mpLogicTimer = mpSystem->CreateLogicTimer(apVars->mGame.mlUpdateRate); break;
		case 24: Log(" Initializing script functions\n"); break;
		case 25: cScriptFuncs::Init(mpGraphics,mpResources,mpSystem,mpInput,mpScene,mpSound,this); break;
		case 26: mpMutex = cPlatform::CreateMutEx(); break;
		case 27:
			mbGameIsDone=false;
			mbPaused = false;
			mbRenderOnce = false;
			ohpl_en_loop_init(&mLoop);
			mbLimitFPS = true;
			break;
		case 28: mpFPSCounter = hplNew( cFPSCounter,(mpSystem->GetLowLevel()) ); break;
		case 29: mpFrameTimer = cPlatform::CreateTimer(); break;
		case 30: Log("User Initialization\n"); break;
		case 32: if(mpHeadlessControl) hplDelete(mpHeadlessControl); mpHeadlessControl = NULL; break;
		case 33: hplDelete(mpDevHud); break;
		case 34: hplDelete(mpLogicTimer); break;
		case 35: hplDelete(mpFPSCounter); break;
		case 36: hplDelete(mpFrameTimer); break;
		case 37: hplDelete(mpMutex); break;
		case 38: hplDelete(mpUpdater); break;
		case 39:
			switch(alModule)
			{
			case 0: hplDelete(mpInput); break;
			case 1: hplDelete(mpPhysics); break;
			case 2: hplDelete(mpScene); break;
			case 3: hplDelete(mpGraphics); break;
			case 4: hplDelete(mpSound); break;
			case 5: hplDelete(mpAI); break;
			case 6: hplDelete(mpGui); break;
			case 7: hplDelete(mpResources); break;
			case 8: hplDelete(mpSystem); break;
			}
			break;
		case 40: Log(" Deleting game setup provided by user\n"); break;
		case 41: hplDelete(mpGameSetup); break;
		case 42: Log("HPL Exit was successful!\n"); break;
		}
	}

	//-----------------------------------------------------------------------

	void cEngine::Exit()
	{
		mpMutex->Lock();

		mbGameIsDone = true;
		//Log("Exit!\n");

		mpMutex->Unlock();
	}

	bool cEngine::GetGameIsDone()
	{
		mpMutex->Lock();

		bool bDone = mbGameIsDone;

		mpMutex->Unlock();

		return bDone;
	}
	//-----------------------------------------------------------------------

	void cEngine::ResetLogicTimer()
	{
		mpLogicTimer->Reset();
	}

	void cEngine::SetUpdatesPerSec(int alUpdatesPerSec)
	{
		mpLogicTimer->SetUpdatesPerSec(alUpdatesPerSec);
	}

	int cEngine::GetUpdatesPerSec()
	{
		return mpLogicTimer->GetUpdatesPerSec();
	}

	float cEngine::GetStepSize()
	{
		return mpLogicTimer->GetStepSize();
	}

	//-----------------------------------------------------------------------
	
	float cEngine::GetFPS()
	{
		return mpFPSCounter->mFps.fps;
	}

	//-----------------------------------------------------------------------

	void cEngine::SetSpeedMul(float afX)
	{
		mpLogicTimer->SetSpeedMul(afX);
	}

	//-----------------------------------------------------------------------

	float cEngine::GetAvgFrameTimeInMS()
	{
		return ohpl_en_fps_avg_frame_ms(&mpFPSCounter->mFps);
	}
	
	//-----------------------------------------------------------------------

	void cEngine::SetFPSUpdateRate(float afSec)
	{
		mpFPSCounter->mFps.updateRate = afSec;
	}
	float cEngine::GetFPSUpdateRate()
	{
		return mpFPSCounter->mFps.updateRate;
	}

	//-----------------------------------------------------------------------

	cScriptVar* cEngine::CreateLocalVar(const tString& asName)
	{
		cScriptVar* pVar;
		pVar= GetLocalVar(asName);
		if(pVar==NULL)
		{
			cScriptVar Var;
			Var.msName = asName;
			m_mapLocalVars.insert(tScriptVarMap::value_type(cString::ToLowerCase(asName),Var));
			pVar= GetLocalVar(asName);
			if(pVar==NULL)FatalError("Very strange error when creating script var!\n");
		}
		return pVar;
	}

	//-----------------------------------------------------------------------

	cScriptVar* cEngine::GetLocalVar(const tString& asName)
	{
		tScriptVarMapIt it = m_mapLocalVars.find(cString::ToLowerCase(asName));
		if(it==m_mapLocalVars.end()) return NULL;

		return &it->second;
	}

	//-----------------------------------------------------------------------

	tScriptVarMap* cEngine::GetLocalVarMap()
	{
		return &m_mapLocalVars;
	}

	//-----------------------------------------------------------------------

	cScriptVar* cEngine::CreateGlobalVar(const tString& asName)
	{
		cScriptVar* pVar;
		pVar= GetGlobalVar(asName);
		if(pVar==NULL)
		{
			cScriptVar Var;
			Var.msName = asName;
			m_mapGlobalVars.insert(tScriptVarMap::value_type(cString::ToLowerCase(asName),Var));
			pVar= GetGlobalVar(asName);
			if(pVar==NULL)FatalError("Very strange error when creating script var!\n");
		}
		return pVar;
	}

	//-----------------------------------------------------------------------

	cScriptVar* cEngine::GetGlobalVar(const tString& asName)
	{
		tScriptVarMapIt it = m_mapGlobalVars.find(cString::ToLowerCase(asName));
		if(it==m_mapGlobalVars.end()) return NULL;

		return &it->second;
	}

	//-----------------------------------------------------------------------

	tScriptVarMap* cEngine::GetGlobalVarMap()
	{
		return &m_mapGlobalVars;
	}

	//-----------------------------------------------------------------------

	void cEngine::ClearAllVariables()
	{
		m_mapGlobalVars.clear();
		m_mapLocalVars.clear();
	}

	//-----------------------------------------------------------------------

	eVariableType cEngine::GetEngineTypeFromString(const tString& asType)
	{
		return (eVariableType)ohpl_en_engine_type_from_string(OhplEnP(asType), asType.size());
	}

	eVariableType cEngine::GetEngineTypeFromStringW(const tWString& asType)
	{
		return GetEngineTypeFromString(cString::To8Char(asType));
	}

	//-----------------------------------------------------------------------

	void cEngine::SetPaused(bool abPaused)
	{
		mpMutex->Lock();

		if(mbPaused != abPaused)
		{
			mbPaused = abPaused;
			if(mbPaused==false) mpLogicTimer->Reset();
		}

		mpMutex->Unlock();
	}

	//-----------------------------------------------------------------------
	
	bool cEngine::GetPaused()
	{
		mpMutex->Lock();

		bool bRet = mbPaused;

		mpMutex->Unlock();

		return bRet;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHOD
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

}
