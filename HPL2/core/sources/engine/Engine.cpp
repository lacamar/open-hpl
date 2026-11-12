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

		mbApplicationHasInputFocus = false;
		mbApplicationHasMouseFocus = false;
		mbApplicationIsVisible = false;

		mvEngineTypeStrings.resize(eVariableType_LastEnum);
		for(int i=0; i<(int)eVariableType_LastEnum; ++i)
			ohpl_en_engine_type_name(i, OhplEnAssign, &mvEngineTypeStrings[i]);
	}


	//-----------------------------------------------------------------------

	void cEngine::GameInit(iLowLevelEngineSetup *apGameSetup,tFlag alHplSetupFlags, cEngineInitVars *apVars)
	{
		mpGameSetup = apGameSetup;

		Log("Creating Engine Modules\n");
		Log("--------------------------------------------------------\n");

		//Create the modules that game connects to and init them!
		Log(" Creating graphics module\n");
		mpGraphics = mpGameSetup->CreateGraphics();
		
		Log(" Creating system module\n");
		mpSystem = mpGameSetup->CreateSystem();

		Log(" Creating resource module\n");
		mpResources = mpGameSetup->CreateResources(mpGraphics);

		Log(" Creating input module\n");
		mpInput = mpGameSetup->CreateInput(mpGraphics);

		Log(" Creating sound module\n");
		mpSound = mpGameSetup->CreateSound();

		Log(" Creating physics module\n");
		mpPhysics = mpGameSetup->CreatePhysics();

		Log(" Creating ai module\n");
		mpAI = mpGameSetup->CreateAI();

		Log(" Creating gui module\n");
		mpGui = hplNew(cGui,());


		Log(" Creating scene module\n");
		mpScene = mpGameSetup->CreateScene(mpGraphics, mpResources, mpSound,mpPhysics,mpSystem,mpAI,mpGui);

		Log("--------------------------------------------------------\n\n");


		//Init the resources
		mpResources->Init(mpGraphics,mpSystem, mpSound,mpScene,mpGui, mpPhysics);

		//Init the graphics
		mpGraphics->Init(	apVars->mGraphics.mvScreenSize.x,
							apVars->mGraphics.mvScreenSize.y,
							apVars->mGraphics.mlDisplay,
							apVars->mGraphics.mlScreenBpp,
							apVars->mGraphics.mbFullscreen,
							apVars->mGraphics.mlMultisampling, 
							apVars->mGraphics.mGpuProgramFormat,
							apVars->mGraphics.msWindowCaption,
							apVars->mGraphics.mvWindowPosition,
							mpResources,alHplSetupFlags);
		
		//Init Sound
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

		//Init physics
		mpPhysics->Init(mpResources);

		//Init AI
		mpAI->Init();

		//Init Gui
		mpGui->Init(mpResources,mpGraphics,mpSound,mpScene, mpInput);
		

		Log("Initializing Game Module\n");
		Log("--------------------------------------------------------\n");
		//Create the updatehandler
		Log(" Adding engine updates\n");
		mpUpdater = hplNew( cUpdater,(mpSystem->GetLowLevel()));

		//Add some loaded modules to the updater
		mpUpdater->AddGlobalUpdate(mpInput);
		mpUpdater->AddGlobalUpdate(mpPhysics);
		mpUpdater->AddGlobalUpdate(mpScene);
		mpUpdater->AddGlobalUpdate(mpGraphics);
		mpUpdater->AddGlobalUpdate(mpSound);
		mpUpdater->AddGlobalUpdate(mpAI);
		mpUpdater->AddGlobalUpdate(mpGui);
		mpUpdater->AddGlobalUpdate(mpResources);

		//Setup the "default" updater container
		mpUpdater->AddContainer("Default");
		mpUpdater->SetContainer("Default");

		//Create the logic timer.
		mpLogicTimer = mpSystem->CreateLogicTimer(apVars->mGame.mlUpdateRate);

		//Init some standard script funcs
		Log(" Initializing script functions\n");
		cScriptFuncs::Init(mpGraphics,mpResources,mpSystem,mpInput,mpScene,mpSound,this);
		
		mpMutex = cPlatform::CreateMutEx();

		//Since game is not done:
		mbGameIsDone=false;

		mbPaused = false;

		mbRenderOnce = false;

		ohpl_en_loop_init(&mLoop);

		mbLimitFPS = true;

		mpFPSCounter = hplNew( cFPSCounter,(mpSystem->GetLowLevel()) );
		mpFrameTimer = cPlatform::CreateTimer();
		Log("--------------------------------------------------------\n\n");

		Log("User Initialization\n");
		Log("--------------------------------------------------------\n");
	}

	//-----------------------------------------------------------------------

	cEngine::~cEngine()
	{
		Log("--------------------------------------------------------\n\n");

		if(mpHeadlessControl) hplDelete(mpHeadlessControl);
		mpHeadlessControl = NULL;
		hplDelete(mpDevHud);

		hplDelete(mpLogicTimer);
		hplDelete(mpFPSCounter);
		hplDelete(mpFrameTimer);
		hplDelete(mpMutex);
		
		hplDelete(mpUpdater);
		
		hplDelete(mpGui);
		hplDelete(mpScene);
		hplDelete(mpInput);
		hplDelete(mpSound);
		hplDelete(mpGraphics);
		hplDelete(mpResources);
		hplDelete(mpPhysics);
		hplDelete(mpAI);
		hplDelete(mpSystem);
		
		Log(" Deleting game setup provided by user\n");
		hplDelete(mpGameSetup);
		
		Log("HPL Exit was successful!\n");
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHOD
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cEngine::Run()
	{
		//Log line that ends user init.
		Log("--------------------------------------------------------\n\n");

		mpUpdater->BroadcastMessageToAll(eUpdateableMessage_OnStart);

		mpLogicTimer->Reset();

		//Loop the game... fix the var...
		unsigned long lTempTime = cPlatform::GetApplicationTime();

		Log("Game Running\n");
		Log("--------------------------------------------------------\n");

		mfFrameTime = 0;
		mpFrameTimer->Start();

		ohpl_en_loop_begin_run(&mLoop);
		typedef std::chrono::steady_clock tClock;
		auto MsSince = [](tClock::time_point t) { return std::chrono::duration<double, std::milli>(tClock::now() - t).count(); };

		while(!GetGameIsDone())
		{
			//////////////////////////
			//Check if application is in focus.
			if(mbWaitIfAppOutOfFocus) CheckIfAppInFocusElseWait();

			//////////////////////////
			//Check if paused
			if(GetPaused())
			{
				cPlatform::Sleep(10);
				mpInput->Update(1.0f/100.0f);
				if(mpHeadlessControl) mpHeadlessControl->LogicStep();
				ohpl_en_loop_pause_tick(&mLoop);

				mpUpdater->RunMessage(eUpdateableMessage_OnPauseUpdate, 1.0f/100.0f);
			}
			else
			{
				//////////////////////////
				//Update logic.
				tClock::time_point tLogic = tClock::now();
				while(mpLogicTimer->WantUpdate() && !GetGameIsDone())
				{
					ohpl_en_loop_step_begin(&mLoop);
					/////////////////////////////////////////////
					// Run Update callback in updater
					mpUpdater->RunMessage(eUpdateableMessage_PreUpdate, GetStepSize());
					mpUpdater->RunMessage(eUpdateableMessage_Update, GetStepSize());
					mpUpdater->RunMessage(eUpdateableMessage_PostUpdate, GetStepSize());
					if(mpHeadlessControl) mpHeadlessControl->LogicStep();
					ohpl_en_loop_step_end(&mLoop);

                    if (mpInput->isQuitMessagePosted()) {

                        mpUpdater->RunMessage(eUpdateableMessage_OnQuit);

                        mpInput->resetQuitMessagePosted();
                    }

					/////////////////////////////////////////////
					// Run Check if any application focus changed
					CheckAndBroadcastFocusChange();

					/////////////////////////////////////////////
					// Run Check if devices were plugged/unplugged
					CheckAndBroadcastDeviceChange();

					/////////////////////////////////////////////
					// If log update is active, clear it regularly.
					if(GetUpdateLogActive())
					{
						if(ohpl_en_loop_update_log_tick(&mLoop))
						{
							if(mpUpdater->GetCurrentContainerName() == "Default") ClearUpdateLogFile();
						}
					}

					//Increase game time.
					ohpl_en_loop_advance_game_time(&mLoop, GetStepSize());
				}
				mpLogicTimer->EndUpdateLoop();
				ohpl_en_loop_add_logic_ms(&mLoop, MsSince(tLogic));
			}

			if(mpHeadlessControl) mpHeadlessControl->Update();

            ////////////////////////////////////
			// If rendering once, make a check and if already drawn screen, just continue
			if(ohpl_en_loop_skip_render_once(&mLoop, mbRenderOnce))
			{
				continue;
			}

			////////////////////////////////////////////////
			//Swap buffers and call callback, do this after update, so hardware can work during update
			if(ohpl_en_loop_take_swap(&mLoop))
			{
				START_TIMING(WaitAndFinishRendering)
				STOP_TIMING(WaitAndFinishRendering)

				START_TIMING(SwapBuffers)
				tClock::time_point tSwap = tClock::now();
				mpGraphics->GetLowLevel()->SwapBuffers();
				ohpl_en_loop_add_swap_ms(&mLoop, MsSince(tSwap));
				STOP_TIMING(SwapBuffers)

				mpUpdater->RunMessage(eUpdateableMessage_OnPostBufferSwap);
				if(ohpl_en_loop_swap_done(&mLoop, mbRenderOnce)) continue;
			}

			////////////////////////////////////
			// Render frame
			if(ohpl_en_loop_want_render(&mLoop, mbLimitFPS))
			{
				///////////////////////////////////////
           		//Get the the from the last frame.
				UpdateFrameTimer();
				tClock::time_point tRender = tClock::now();

				//On draw callback sending that to gui, etc
				START_TIMING(OnDraw)
				mpUpdater->RunMessage(eUpdateableMessage_OnDraw, mfFrameTime);
				STOP_TIMING(OnDraw)

				//Render this frame
				START_TIMING(RenderAll)
				mpScene->Render(mfFrameTime, tSceneRenderFlag_All);
				STOP_TIMING(RenderAll)

				START_TIMING(PostRender)
				mpUpdater->RunMessage(eUpdateableMessage_OnPostRender, mfFrameTime);
				STOP_TIMING(PostRender)

				mpDevHud->Render(mbDevHudActive);

				START_TIMING(FlushRender)
				mpGraphics->GetLowLevel()->FlushRendering();
				STOP_TIMING(FlushRender)

				cEngineDiagnostics::EndFrame();
				OhplEnFrameTiming Timing;
				ohpl_en_loop_frame_rendered(&mLoop, &Timing);
				cEngineDiagnostics::AddFrameTiming(Timing.logicMs, Timing.steps, MsSince(tRender), Timing.swapMs);

				//Update fps counter.
				mpFPSCounter->AddFrame();
			}
			else
			{
				cPlatform::Sleep(1);
			}
		}
		Log("--------------------------------------------------------\n\n");

		Log("Statistics\n");
		Log("--------------------------------------------------------\n");

		unsigned long lTime = cPlatform::GetApplicationTime() - lTempTime;
		double fMediumTime = ohpl_en_loop_mean_framerate(&mLoop, lTime);

		Log(" Medium framerate: %f\n", fMediumTime);
		Log("--------------------------------------------------------\n\n");

		Log("User Exit\n");
		Log("--------------------------------------------------------\n");

		mpUpdater->BroadcastMessageToAll(eUpdateableMessage_OnExit);
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

	void cEngine::UpdateFrameTimer()
	{
		mpFrameTimer->Stop();
		mfFrameTime = ohpl_en_frame_time(mpFrameTimer->GetTimeInSec());

		/*mlstFrameTimes.push_back(mpFrameTimer->GetTimeInMilliSec());
		if((int)mlstFrameTimes.size() >= mlMaxFrameTimes)
		{
			double fFrameSum=0.0;
			for(tDoubleListIt it = mlstFrameTimes.begin(); it != mlstFrameTimes.end(); it++)
			{
				fFrameSum += *it;
			}
			mfAvgFrameTimeInMS = (float)(fFrameSum / (double)mlstFrameTimes.size());
			mlstFrameTimes.clear();
		}*/
				
		mpFrameTimer->Start();
	}

	//-----------------------------------------------------------------------

	void cEngine::CheckAndBroadcastFocusChange()
	{
		// Hidden headless window: focus comes from the control socket only
		if(mpHeadlessControl) return;
		uint32_t lHad = (mbApplicationHasInputFocus ? 1 : 0) | (mbApplicationHasMouseFocus ? 2 : 0) | (mbApplicationIsVisible ? 4 : 0);

        iLowLevelGraphics *pllGfx = mpGraphics->GetLowLevel();
		mbApplicationHasInputFocus = pllGfx->GetWindowInputFocus();
		mbApplicationHasMouseFocus = pllGfx->GetWindowMouseFocus();
		mbApplicationIsVisible = pllGfx->GetWindowIsVisible();
		uint32_t lNow = (mbApplicationHasInputFocus ? 1 : 0) | (mbApplicationHasMouseFocus ? 2 : 0) | (mbApplicationIsVisible ? 4 : 0);

		int vMessages[8];
		size_t lCount = ohpl_en_focus_messages(lHad, lNow, vMessages);
		for(size_t i = 0; i < lCount; ++i) mpUpdater->RunMessage((eUpdateableMessage)vMessages[i]);
	}

	//-----------------------------------------------------------------------

	void cEngine::CheckIfAppInFocusElseWait()
	{
		iLowLevelGraphics *pllGfx = mpGraphics->GetLowLevel();
		while(	pllGfx->GetWindowInputFocus()==false)
		{
			cPlatform::Sleep(100);
			mpInput->Update(1.0f/10.0f);
		}
	}

	//-----------------------------------------------------------------------

	void cEngine::CheckAndBroadcastDeviceChange()
	{
		if(mbDevicePlugged)
		{
			mbDevicePlugged = false;
			mpUpdater->RunMessage(eUpdateableMessage_AppDeviceWasPlugged);
		}

		if(mbDeviceRemoved)
		{
			mbDeviceRemoved = false;
			mpUpdater->RunMessage(eUpdateableMessage_AppDeviceWasRemoved);
		}
	}

	//-----------------------------------------------------------------------
}
