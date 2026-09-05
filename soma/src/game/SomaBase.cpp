/*
 * Phase 0 scaffolding for a SOMA game module running on the HPL2 engine.
 * See SomaBase.h for scope notes.
 */

#include "SomaBase.h"
#include "HpslTranspilerSelfTest.h"
#include "HpslTranspiler.h"
#include "SomaLoaders.h"
#include "SomaSplash.h"

#include "system/HeadlessControl.h"
#include "resources/GpuShaderManager.h"

#if defined(__linux__)
#include <unistd.h>
#endif

//---------------------------------------

cSomaBase *gpSomaBase = NULL;

//---------------------------------------

//////////////////////////////////////////////////////////////////////////
// HEADLESS CONTROL COMMANDS (see HPL2/core/include/system/HeadlessControl.h)
//
// Still just the shared debug camera's own transform, whether it's actually
// being driven by cSomaDebugFreeCamera or (see SomaPlayer.h) a real
// character body - both write straight into the same cCamera, so this needs
// no changes to support the real player controller. mpDebugCamera is
// checked at call time, not registration time: it doesn't exist until
// InitMainMenuScene()/InitTestMap() run, which happens later (after the
// splash sequence, via OnSplashFinished()) than where these are registered
// below. There is still no script layer at all - see SomaPlayer.h/PORTING_NOTES.md.
//////////////////////////////////////////////////////////////////////////

static void cSomaBase_HeadlessCmd_CameraState(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	if(pBase->GetDebugCamera() == NULL)
	{
		aResp.SetError("no camera yet");
		return;
	}

	const cVector3f &vPos = pBase->GetDebugCamera()->GetPosition();
	aResp.Set("pos_x", vPos.x);
	aResp.Set("pos_y", vPos.y);
	aResp.Set("pos_z", vPos.z);
	aResp.Set("pitch", pBase->GetDebugCamera()->GetPitch());
	aResp.Set("yaw", pBase->GetDebugCamera()->GetYaw());
	aResp.Set("fps", pBase->mpEngine->GetFPS());
}

static void cSomaBase_HeadlessCmd_SetCamera(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	if(pBase->GetDebugCamera() == NULL)
	{
		aResp.SetError("no camera yet");
		return;
	}

	if(aReq.HasKey("x") || aReq.HasKey("y") || aReq.HasKey("z"))
	{
		const cVector3f &vCur = pBase->GetDebugCamera()->GetPosition();
		cVector3f vPos(aReq.GetFloat("x", vCur.x), aReq.GetFloat("y", vCur.y), aReq.GetFloat("z", vCur.z));
		pBase->GetDebugCamera()->SetPosition(vPos);
	}
	if(aReq.HasKey("pitch")) pBase->GetDebugCamera()->SetPitch(aReq.GetFloat("pitch", 0));
	if(aReq.HasKey("yaw")) pBase->GetDebugCamera()->SetYaw(aReq.GetFloat("yaw", 0));
}

// Lets a headless caller load any real map by basename (found via the same
// resource-dir search InitTestMap()/InitMainMenuScene() already use) instead
// of being stuck with whatever InitMainMenuScene()/InitTestMap()'s
// boot-time fallback logic decided - added specifically so this scaffold's
// real content (e.g. 00_01_apartment.hpm) can be inspected headlessly now
// that InitMainMenuScene() succeeds (loading a real but legitimately empty
// main_menu.hpm - see PORTING_NOTES.md) and no longer falls back to it.
static void cSomaBase_HeadlessCmd_StartMap(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	tString sMap = aReq.GetString("map", "");
	if(sMap == "")
	{
		aResp.SetError("missing 'map' field");
		return;
	}

	cVector3f vPos(aReq.GetFloat("x", 0), aReq.GetFloat("y", 1.7f), aReq.GetFloat("z", 0));
	tString sStartPosName = aReq.GetString("pos", "");

	tString sError;
	if(pBase->LoadMap(sMap, vPos, sError, sStartPosName) == false)
	{
		aResp.SetError(sError);
		return;
	}
}

//---------------------------------------

cSomaBase::cSomaBase()
{
	mpEngine = NULL;

	mpSplash = NULL;
	mpGammaScreen = NULL;
	mpMainMenu = NULL;

	mpTestWorld = NULL;
	mpDebugCamera = NULL;
	mpDebugViewport = NULL;
	mpDebugCameraController = NULL;
	mpPlayer = NULL;
	mbUseRealPlayer = true;

	mpIntroSequence = NULL;
}

//-----------------------------------------------------------------------

cSomaBase::~cSomaBase()
{
}

//-----------------------------------------------------------------------

bool cSomaBase::Init(const tString &asCommandline)
{
	/////////////////////////////
	// Parse the command line (an alternate init config file path, same
	// convention as cLuxBase::ParseCommandLine)
	if (ParseCommandLine(asCommandline) == false)
		return false;

	/////////////////////////////
	// Load SOMA's real main_init.cfg to get resource/material config paths
	// and the game name - unlike Amnesia's main_init.cfg, SOMA's has no
	// separate "main settings"/Menu/PreMenu/Demo config file entries, so
	// InitMainConfig() here only pulls out what Phase 0 actually needs.
	if (InitMainConfig() == false)
		return false;

	/////////////////////////////
	// Wire the HPSL->GLSL transpiler (soma/src/game/HpslTranspiler.cpp) into
	// cGpuShaderManager as its .glsl-not-found fallback, so SOMA's real
	// .hpsl material shaders get transpiled-and-compiled instead of just
	// erroring (see PORTING_NOTES.md "SOMA" section). HPL2/core can't call
	// TranspileHpslToGlsl() directly (that's game-module code), so this is
	// the only place in this codebase that calls SetHpslTranspileCallback -
	// Dark Descent/AMFP never do, so cGpuShaderManager's fallback path stays
	// dead code for them.
	//
	// MUST run before InitEngine(): CreateHPLEngine() (called from inside
	// InitEngine() below) constructs cGraphics, which in turn constructs
	// cRendererDeferred/cRendererSimple/the post-effect types/some material
	// types - several of which build GPU programs (deferred_base_vtx.glsl
	// and friends) directly in their own constructors, not lazily on first
	// use. When this call came after InitEngine(), every one of those
	// engine-init-time shader lookups saw mpHpslTranspileCallback still
	// NULL, so cGpuShaderManager::CreateShader()'s HPSL-fallback branch
	// never even triggered - straight to "Couldn't find file
	// 'deferred_base_vtx.glsl' in resources", permanently (those program
	// pointers are cached NULL for the object's lifetime, never retried).
	// Found live: a real headless boot of 00_01_apartment.hpm showed 14
	// "Couldn't find file 'deferred_base_vtx.glsl'" / 6 "Could not load
	// material ... shader 'deferred_base_vtx.glsl'" lines in hpl.log even
	// though most materials (loaded later, well after Init() returns and
	// the callback is registered) resolved through the same fallback fine.
	cGpuShaderManager::SetHpslTranspileCallback(TranspileHpslToGlsl);

	/////////////////////////////
	// Init the engine: create the window, load resources.cfg/materials.cfg,
	// and get to a state where an empty scene can be rendered.
	if (InitEngine() == false)
		return false;

	// Deliberately after InitEngine() (which calls SetLogFile() early on,
	// before doing anything else that might Log()) rather than right after
	// InitMainConfig() above where this used to sit - a bare relative
	// "hpl.log" (this engine's default log destination before SetLogFile()
	// runs, see InitEngine()'s own comment) resolves inside whatever the
	// process's cwd happens to be, which for a real deployed build is the
	// Steam install directory itself. Confirmed live: running a build with
	// this Log() call still in its old spot from a scratch test directory
	// that (per this project's own established headless-testing pattern)
	// symlinks "hpl.log" back to the real install for tailing wrote this
	// exact line into the real Steam SOMA install's hpl.log - exactly what
	// this project has a zero-tolerance policy against.
	Log("SOMA game module - Phase 0 scaffolding (%s)\n", msGameName.c_str());

	/////////////////////////////
	// Headless control: register camera commands if a control server is
	// active (OPENHPL_HEADLESS_SOCKET) - see HeadlessControl.h.
	if (mpEngine->GetHeadlessControl())
	{
		cHeadlessControlServer *pCtrl = mpEngine->GetHeadlessControl();
		pCtrl->RegisterHandler("camera_state", cSomaBase_HeadlessCmd_CameraState, this);
		pCtrl->RegisterHandler("set_camera", cSomaBase_HeadlessCmd_SetCamera, this);
		pCtrl->RegisterHandler("start_map", cSomaBase_HeadlessCmd_StartMap, this);
	}

	/////////////////////////////
	// One-shot HPSL->GLSL transpiler proof-of-concept - see
	// HpslTranspilerSelfTest.h. Not part of real rendering yet; just
	// proves whether the transpiled clear_vtx/clear_frag pair compiles as
	// real GLSL against the live GL context. Safe to run every boot: it
	// only reads shader files and compiles throwaway GL shader objects.
	RunHpslTranspilerSelfTest(mpEngine);

	/////////////////////////////
	// Real boot sequence: show the splash logos, then (via
	// OnSplashFinished(), called back from cSomaSplash once its sequence
	// ends) load SOMA's own declared main menu scene. No map is loaded
	// synchronously here anymore - see SomaSplash.h/cpp.
	mpSplash = hplNew(cSomaSplash, (mpEngine, this));
	mpEngine->GetUpdater()->AddGlobalUpdate(mpSplash);

	return true;
}

//-----------------------------------------------------------------------

void cSomaBase::OnSplashFinished()
{
	// Real SOMA only shows its gamma-calibration screen once, on a
	// completely fresh install (MenuHandler.hps's mbPremenuActive flag) -
	// see cSomaGammaScreen::ShouldShowAndMarkSeen() for how that's tracked
	// here. On every later boot this goes straight to ProceedPastBoot().
	if (cSomaGammaScreen::ShouldShowAndMarkSeen())
	{
		mpGammaScreen = hplNew(cSomaGammaScreen, (mpEngine, this));
		mpEngine->GetUpdater()->AddGlobalUpdate(mpGammaScreen);
		return;
	}

	ProceedPastBoot();
}

//-----------------------------------------------------------------------

void cSomaBase::OnGammaScreenFinished()
{
	ProceedPastBoot();
}

//-----------------------------------------------------------------------

void cSomaBase::ProceedPastBoot()
{
	// Opt-in escape hatch for interactively looking at real map content -
	// InitMainMenuScene() (the normal path) loads a real but legitimately
	// empty main_menu.hpm, since there's no menu/script layer yet to make
	// anything else of it. Set OPENHPL_SOMA_MAP to a real map filename
	// (e.g. "00_01_apartment.hpm") to load that instead, so a real desktop
	// launch can show real geometry/lighting without needing the headless
	// control-socket workflow. No effect when unset.
	const char *pTestMap = getenv("OPENHPL_SOMA_MAP");
	if (pTestMap != NULL && pTestMap[0] != '\0')
	{
		tString sError;
		tString sStartPos = getenv("OPENHPL_SOMA_MAP_STARTPOS") ? getenv("OPENHPL_SOMA_MAP_STARTPOS") : "";
		if (LoadMap(pTestMap, cVector3f(0, 1.7f, 0), sError, sStartPos) == false)
		{
			Log("SOMA: OPENHPL_SOMA_MAP='%s' failed to load (%s), falling back to the main menu scene\n",
				pTestMap, sError.c_str());
		}
		else
		{
			return;
		}
	}

	if (InitMainMenuScene() == false)
	{
		Log("SOMA: could not load main menu scene ('%s'), falling back to the "
			"apartment test map\n", cString::To8Char(msErrorMessage).c_str());
		InitTestMap();
	}
}

//-----------------------------------------------------------------------

void cSomaBase::Exit()
{
	ExitTestMap();
	ExitEngine();
}

//-----------------------------------------------------------------------

void cSomaBase::Run()
{
	// Main loop - a map is loaded and either the debug free-fly camera or
	// (the default for real game maps - see LoadMap()) a real physics-based
	// player controller (see SomaPlayer.h) is active, but there is still no
	// script layer running at all (no OnStart()/quest/door/intro logic).
	mpEngine->Run();
}

//-----------------------------------------------------------------------

bool cSomaBase::ParseCommandLine(const tString &asCommandline)
{
	msInitConfigFile = cString::To16Char(asCommandline);
	if (msInitConfigFile == _W(""))
		msInitConfigFile = _W("config/main_init.cfg");

	return true;
}

//-----------------------------------------------------------------------

bool cSomaBase::InitMainConfig()
{
	cConfigFile *pInitCfg = hplNew(cConfigFile, (msInitConfigFile));
	if (pInitCfg->Load() == false)
	{
		msErrorMessage = _W("Could not load main init file: ") + msInitConfigFile;
		hplDelete(pInitCfg);
		return false;
	}

	msResourceConfigPath = pInitCfg->GetString("ConfigFiles", "Resources", "resources.cfg");
	msMaterialConfigPath = pInitCfg->GetString("ConfigFiles", "Materials", "materials.cfg");
	msGameName = pInitCfg->GetString("Variables", "GameName", "SOMA");

	hplDelete(pInitCfg);

	return true;
}

//-----------------------------------------------------------------------

bool cSomaBase::InitEngine()
{
	// Real physics-based player controller (see SomaPlayer.h/.cpp) for real
	// game maps loaded via LoadMap() - the debug free-fly camera stays
	// available as an opt-out escape hatch (e.g. to no-clip through a level
	// for inspection) via OPENHPL_SOMA_FREECAM=1. Main menu scenes
	// (InitMainMenuScene()) and the old InitTestMap() fallback always keep
	// using the free-fly camera regardless of this flag - no player body
	// makes sense there.
	mbUseRealPlayer = (getenv("OPENHPL_SOMA_FREECAM") == NULL);

	cEngineInitVars vars;
	vars.mGraphics.mvScreenSize = cVector2l(1280, 720);
	vars.mGraphics.msWindowCaption = msGameName + " (Phase 0)";

#if defined(__linux__)
	// hpl.log otherwise defaults to a bare relative "hpl.log" (see
	// LowLevelSystemSDL.cpp), landing wherever cwd happens to be at first
	// Log() - the real game's Steam install directory, since that's where
	// this binary gets deployed and run from. XDG_STATE_HOME is the
	// correct home for transient log/state data (see amnesia/src/game/
	// LuxBasePersonal.h's equivalent for the real Amnesia game module).
	tWString sStateRoot = cPlatform::GetSystemSpecialPath(eSystemPath_XDGStateHome);
	tWString sStateDir = sStateRoot + _W("open-hpl/");
	if(cPlatform::FolderExists(sStateDir) == false) cPlatform::CreateFolder(sStateDir);
	sStateDir += _W("soma/");
	if(cPlatform::FolderExists(sStateDir) == false) cPlatform::CreateFolder(sStateDir);

	// A fixed hpl.log path collides across concurrent headless test runs
	// (now a normal occurrence with multiple agents each testing their own
	// Soma.<branch>.aarch64 build) - cLogWriter::ReopenFile() truncates on
	// open, so a second process launched while a first is still running
	// silently wipes whatever the first had already logged. Suffix with the
	// PID under OPENHPL_HEADLESS_SOCKET only, so normal interactive play
	// keeps the stable, predictable filename.
	tWString sLogFile = sStateDir + _W("hpl.log");
	if(getenv("OPENHPL_HEADLESS_SOCKET") != NULL)
	{
		sLogFile = sStateDir + _W("hpl-") + cString::ToStringW((int)getpid()) + _W(".log");
	}
	SetLogFile(sLogFile);
#endif

	// Load persisted settings (see SomaConfig.h) - deliberately AFTER
	// SetLogFile() above: cConfigFile::Load()/cSomaConfig::Load() both Log()
	// on a missing/fresh-install config file (the common case), and doing
	// this any earlier sends that Log() to the engine's pre-SetLogFile()
	// default destination - a bare relative "hpl.log" in whatever the
	// process's cwd happens to be (see the comment above). A real headless
	// test run from a scratch directory containing an "hpl.log" symlink
	// (this project's own established pattern, e.g. for tailing it via the
	// headless control socket) turned that into a real, confirmed write
	// into the actual Steam install directory the very first time this bug
	// existed - exactly what this project has a zero-tolerance policy
	// against. mbFullscreen only takes effect at window-creation time
	// (cLowLevelGraphics::Init()'s abFullscreen param), so it has to be
	// read back and applied to vars here, before CreateHPLEngine() below -
	// unlike Gamma/Volume/VSync, applied live further down once cGraphics/
	// cSound exist.
	mConfig.Load();
	vars.mGraphics.mbFullscreen = mConfig.mbFullscreen;

	/////////////////////////
	// Create the engine
	mpEngine = CreateHPLEngine(eHplAPI_OpenGL, eHplSetup_All, &vars);
	if (mpEngine == NULL)
	{
		msErrorMessage = _W("Could not create HPL engine!");
		return false;
	}

	/////////////////////////
	// Load SOMA's real resource directory listing and physics surface data.
	// Both parsers are fully generic (no Amnesia-specific assumptions), so
	// SOMA's own files load unmodified.
	mpEngine->GetResources()->LoadResourceDirsFile(msResourceConfigPath);
	mpEngine->GetPhysics()->LoadSurfaceData(msMaterialConfigPath);

	// See SomaLoaders.h - without these, cWorldLoaderHpm silently drops
	// every <Entity>/<Area> element in a real SOMA map (confirmed via a real
	// boot log against real game data).
	RegisterSomaLoaders(mpEngine->GetResources());

	/////////////////////////
	// Apply the persisted settings that DO have a live/runtime API (unlike
	// Fullscreen above, which only applies at the next InitEngine()) - same
	// APIs amnesia/src/game/LuxMainMenu_Options.cpp's own Options menu uses
	// for these.
	mpEngine->GetSound()->GetLowLevel()->SetVolume(mConfig.mfMasterVolume);
	mpEngine->GetGraphics()->GetLowLevel()->SetGammaCorrection(mConfig.mfGamma);
	mpEngine->GetGraphics()->GetLowLevel()->SetVsyncActive(mConfig.mbVSync, false);

	return true;
}

//-----------------------------------------------------------------------

void cSomaBase::ExitEngine()
{
	if (mpEngine)
		DestroyHPLEngine(mpEngine);
	mpEngine = NULL;
}

//-----------------------------------------------------------------------

bool cSomaBase::InitMainMenuScene()
{
	////////////////////////////////////
	// Read the <MainMenu File="..."/> entry back out of main_init.cfg -
	// the same file InitMainConfig() already loaded once, re-loaded here
	// rather than caching it earlier since Phase 0 only kept the two
	// fields it needed at the time.
	cConfigFile *pInitCfg = hplNew(cConfigFile, (msInitConfigFile));
	if (pInitCfg->Load() == false)
	{
		msErrorMessage = _W("Could not reload main init file for <MainMenu> entry: ") + msInitConfigFile;
		hplDelete(pInitCfg);
		return false;
	}
	tString sMainMenuFile = pInitCfg->GetString("MainMenu", "File", "");
	hplDelete(pInitCfg);

	if (sMainMenuFile == "")
	{
		msErrorMessage = _W("main_init.cfg has no <MainMenu File=.../> entry");
		return false;
	}

	////////////////////////////////////
	// Found by basename via the resource dir search, same convention as
	// InitTestMap()'s apartment map load below - Folder="maps/" from the
	// config is not needed, "/maps" is already registered with AddSubDirs
	// in SOMA's real resources.cfg.
	cWorld *pWorld = mpEngine->GetScene()->LoadWorld(sMainMenuFile, 0);
	if (pWorld == NULL)
	{
		msErrorMessage = _W("Could not load main menu scene '") + cString::To16Char(sMainMenuFile) + _W("'");
		return false;
	}
	mpTestWorld = pWorld;

	////////////////////////////////////
	// Debug free-fly camera, same as InitTestMap() below. main_menu.hpm's
	// own PlayerStartArea_1 has WorldPos="0 0 0" - the real menu camera
	// path is driven entirely by scripted logic this port doesn't have
	// (main_menu.hps plus the closed ImGui menu layer), so world origin is
	// the only position the map data itself actually declares.
	cCamera *pCamera = mpEngine->GetScene()->CreateCamera(eCameraMoveMode_Fly);
	pCamera->SetPosition(cVector3f(0, 1.7f, 0));
	pCamera->SetFarClipPlane(200.0f);
	mpDebugCamera = pCamera;

	mpDebugViewport = mpEngine->GetScene()->CreateViewport(pCamera, pWorld, true);

	mpDebugCameraController = hplNew(cSomaDebugFreeCamera, (pCamera, mpEngine->GetInput()));
	mpEngine->GetUpdater()->AddGlobalUpdate(mpDebugCameraController);

	////////////////////////////////////
	// Real interactive menu - see SomaMainMenu.h. Attached to this same
	// camera+world viewport (not a separate GUI-only one like the splash
	// uses), since there's a real scene behind it.
	mpMainMenu = hplNew(cSomaMainMenu, (mpEngine, this, mpDebugViewport));
	mpEngine->GetUpdater()->AddGlobalUpdate(mpMainMenu);

	return true;
}

//-----------------------------------------------------------------------

bool cSomaBase::StartNewGame(tString &asErrorOut)
{
	////////////////////////////////////
	// Read the real <StartMap File="..." Pos="..."/> entry back out of
	// main_init.cfg - same file/pattern InitMainMenuScene() already uses
	// for <MainMenu>. A real install declares "00_00_intro.hpm"/
	// "PlayerStartArea_1" here; the previous New Game handler hardcoded
	// "00_01_apartment.hpm" instead (a real, but wrong, map - apartment is
	// reached later in the intro sequence, not where a new game starts).
	cConfigFile *pInitCfg = hplNew(cConfigFile, (msInitConfigFile));
	if (pInitCfg->Load() == false)
	{
		asErrorOut = "Could not reload main init file for <StartMap> entry";
		hplDelete(pInitCfg);
		return false;
	}
	tString sStartMapFile = pInitCfg->GetString("StartMap", "File", "");
	tString sStartMapPos = pInitCfg->GetString("StartMap", "Pos", "");
	hplDelete(pInitCfg);

	if (sStartMapFile == "")
	{
		asErrorOut = "main_init.cfg has no <StartMap File=.../> entry";
		return false;
	}

	return LoadMap(sStartMapFile, cVector3f(0, 1.7f, 0), asErrorOut, sStartMapPos);
}

//-----------------------------------------------------------------------

void cSomaBase::OnIntroSequenceFinished()
{
	tString sError;
	if (LoadMap("00_01_apartment.hpm", cVector3f(0, 1.7f, 0), sError, "PlayerStartArea_1") == false)
	{
		Log("SOMA: intro sequence finished but failed to load next map (%s)\n", sError.c_str());
		return;
	}

	if (mpPlayer) mpPlayer->SetActive(true);
}

//-----------------------------------------------------------------------

bool cSomaBase::InitTestMap()
{
	////////////////////////////////////
	// Hardcoded Phase 1 test map: chapter00/00_01_apartment - smallest,
	// earliest, indoor map, expected not to need terrain. Found by basename
	// via the resource dir search ("/maps" is registered with AddSubDirs in
	// SOMA's real resources.cfg), same convention meshes/entities use - so
	// this is not an absolute filesystem path.
	cWorld *pWorld = mpEngine->GetScene()->LoadWorld("00_01_apartment.hpm", 0);
	if (pWorld == NULL)
	{
		msErrorMessage = _W("Could not load test map '00_01_apartment.hpm'!");
		return false;
	}
	mpTestWorld = pWorld;

	////////////////////////////////////
	// Debug free-fly camera (see DebugFreeCamera.h) - no player controller.
	// Start position/facing taken directly from the map's own
	// "PlayerStartArea_1" PlayerStart Area (WorldPos="-10.75 1.01415 8.25"
	// Rotation="-0 3.92803 -0" in 00_01_apartment.hpm_Area), nudged up to a
	// more eye-like height. Hardcoded rather than resolved through the
	// engine's Area system, since Phase 1 has no game-side PlayerStart area
	// loader registered to query.
	cCamera *pCamera = mpEngine->GetScene()->CreateCamera(eCameraMoveMode_Fly);
	pCamera->SetPosition(cVector3f(-10.75f, 1.7f, 8.25f));
	pCamera->SetYaw(3.92803f);
	pCamera->SetFarClipPlane(200.0f);
	mpDebugCamera = pCamera;

	mpDebugViewport = mpEngine->GetScene()->CreateViewport(pCamera, pWorld, true);

	mpDebugCameraController = hplNew(cSomaDebugFreeCamera, (pCamera, mpEngine->GetInput()));
	mpEngine->GetUpdater()->AddGlobalUpdate(mpDebugCameraController);

	return true;
}

//-----------------------------------------------------------------------

bool cSomaBase::LoadMap(const tString &asMapFile, const cVector3f &avStartPos, tString &asErrorOut,
						 const tString &asStartPosName)
{
	// Found by basename via the resource dir search, same convention as
	// InitTestMap()/InitMainMenuScene() above ("/maps" is registered with
	// AddSubDirs in SOMA's real resources.cfg).
	cWorld *pNewWorld = mpEngine->GetScene()->LoadWorld(asMapFile, 0);
	if (pNewWorld == NULL)
	{
		asErrorOut = "Could not load map '" + asMapFile + "'";
		return false;
	}

	// mpPlayer's character body (if any) belongs to mpTestWorld's specific
	// physics world - must be destroyed before DestroyWorld() below frees
	// that physics world out from under it, or cSomaPlayer::ResetForNewMap()
	// (called further down) would call iPhysicsWorld::DestroyCharacterBody()
	// on an already-dangling pointer. Found live via a real SIGSEGV: the
	// first LoadMap() call (menu -> New Game) worked fine (no old body to
	// destroy yet), but a second one (e.g. a headless "start_map" reload)
	// crashed immediately in cSomaPlayer::DestroyCharacterBody().
	if (mpPlayer) mpPlayer->DestroyCharacterBody();

	if (mpTestWorld) mpEngine->GetScene()->DestroyWorld(mpTestWorld);
	mpTestWorld = pNewWorld;

	// Reuse the existing camera/viewport if this isn't the first load rather
	// than destroying and recreating them - cUpdater has no "remove"
	// counterpart to AddGlobalUpdate() (see ExitTestMap()'s own comment on
	// this), so a fresh controller on every call would leak one dangling
	// iUpdateable per call once its camera is destroyed below.
	// cViewport::SetWorld() is the real engine API for exactly this "same
	// camera, new world" case. Note this camera/viewport may already exist
	// from InitMainMenuScene() (StartNewGame() calling this after the menu
	// was shown is the normal "New Game" path), not just from an earlier
	// LoadMap() call.
	if (mpDebugCamera == NULL)
	{
		mpDebugCamera = mpEngine->GetScene()->CreateCamera(eCameraMoveMode_Fly);
		mpDebugCamera->SetFarClipPlane(200.0f);
		mpDebugViewport = mpEngine->GetScene()->CreateViewport(mpDebugCamera, mpTestWorld, true);
	}
	else
	{
		mpDebugViewport->SetWorld(mpTestWorld);
	}

	// Controller hand-off: InitMainMenuScene() always creates a free-fly
	// mpDebugCameraController for the menu scene itself (see there), so the
	// *first* real game map to load via LoadMap() (typically "New Game")
	// needs to both disable that (rather than destroy it - same
	// no-remove-from-cUpdater constraint as above; a live but disabled
	// controller just returns immediately, see cSomaDebugFreeCamera::Update())
	// and create the real player controller for the first time. A
	// cSomaPlayer, once created, is reused/reset for every later map (see
	// cSomaPlayer::ResetForNewMap()) rather than recreated - unlike its
	// character body, which really does need destroying and recreating on
	// every call, since it belongs to the old world's specific physics
	// world, just torn down by DestroyWorld() above.
	if (mbUseRealPlayer)
	{
		if (mpDebugCameraController)
			mpDebugCameraController->SetActive(false);

		if (mpPlayer == NULL)
		{
			mpPlayer = hplNew(cSomaPlayer, (mpDebugCamera, mpEngine->GetInput()));
			mpEngine->GetUpdater()->AddGlobalUpdate(mpPlayer);
		}
	}
	else if (mpDebugCameraController == NULL)
	{
		mpDebugCameraController = hplNew(cSomaDebugFreeCamera, (mpDebugCamera, mpEngine->GetInput()));
		mpEngine->GetUpdater()->AddGlobalUpdate(mpDebugCameraController);
	}

	// Resolve a real PlayerStart Area by name if asked for (requires
	// cSomaAreaLoader_PlayerStart - see SomaLoaders.h - to have populated
	// one via CreateStartPos() while pNewWorld loaded above); otherwise fall
	// back to the caller-supplied position, same as before this existed.
	// Also pulls the Area's real yaw rotation now (previously discarded -
	// the free-fly camera always started facing world-forward regardless of
	// which way the PlayerStart actually faced), needed for the real player
	// controller below and applied to the free-fly camera too as a minor
	// side-fix.
	cVector3f vAreaPos = avStartPos;
	float fAreaYaw = 0;
	bool bFoundArea = false;
	if (asStartPosName != "")
	{
		cStartPosEntity *pStartPos = pNewWorld->GetStartPosEntity(asStartPosName);
		if (pStartPos)
		{
			vAreaPos = pStartPos->GetWorldMatrix().GetTranslation();
			fAreaYaw = cMath::MatrixToEulerAngles(pStartPos->GetWorldMatrix().GetRotation(), eEulerRotationOrder_XYZ).y;
			bFoundArea = true;
		}
		else
		{
			Log("SOMA: map '%s' has no PlayerStart Area named '%s', using fallback position\n",
				asMapFile.c_str(), asStartPosName.c_str());
		}
	}

	if (mbUseRealPlayer && mpPlayer)
	{
		// Real feet position: the PlayerStart Area's raw translation (no
		// eye-height fudge - the character body's own size/CameraPosAdd
		// handles that, see SomaPlayer.cpp), or the caller-supplied
		// fallback position when no named Area was found (only exercised by
		// the OPENHPL_SOMA_MAP debug env var / the "start_map" headless
		// command with no 'pos' field).
		mpPlayer->ResetForNewMap(mpTestWorld->GetPhysicsWorld(), vAreaPos, fAreaYaw);
	}
	else
	{
		cVector3f vCamPos = bFoundArea ? (vAreaPos + cVector3f(0, 0.5f, 0)) : avStartPos;
		mpDebugCamera->SetPosition(vCamPos);
		mpDebugCamera->SetPitch(0);
		mpDebugCamera->SetYaw(fAreaYaw);
	}

	// Real 00_00_intro.hpm is a non-interactive 2D slideshow, not walkable 3D
	// content - real OnEnter() calls Player_SetActive(false) for the whole
	// map (see SomaIntroSequence.h for the full reverse-engineering
	// citation). Matched on the real map filename here in LoadMap() itself,
	// not just the "New Game" call site (StartNewGame() just calls this),
	// so it also fires for a direct "start_map" headless reload used to
	// verify it - same as the real engine, which runs this map's OnEnter()
	// regardless of how it was reached.
	if (asMapFile == "00_00_intro.hpm")
	{
		if (mpPlayer) mpPlayer->SetActive(false);

		if (mpIntroSequence == NULL)
		{
			mpIntroSequence = hplNew(cSomaIntroSequence, (mpEngine, this));
			mpEngine->GetUpdater()->AddGlobalUpdate(mpIntroSequence);
		}
		else
		{
			Log("SOMA: intro sequence object already exists, not starting a second one\n");
		}
	}

	return true;
}

//-----------------------------------------------------------------------

void cSomaBase::ExitTestMap()
{
	// mpDebugCamera / mpDebugViewport / mpTestWorld are owned by cScene and
	// torn down together with the rest of the engine in ExitEngine().
	//
	// mpDebugCameraController was registered with cUpdater::AddGlobalUpdate,
	// which (like the rest of this codebase's global systems - input,
	// physics, scene, graphics, sound, AI, gui, resources, all added the
	// same way in cEngine::GameInit) has no matching "remove" API; cUpdater
	// itself is destroyed as part of DestroyHPLEngine() right after this
	// call, with no further Update() in between, so it's left for that
	// teardown rather than explicitly deleted here against a dangling
	// reference in the updater's list.
	mpDebugCameraController = NULL;
	mpPlayer = NULL;

	mpDebugViewport = NULL;
	mpDebugCamera = NULL;
	mpTestWorld = NULL;
}

//-----------------------------------------------------------------------
