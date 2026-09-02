/*
 * Phase 0 scaffolding for a SOMA game module running on the HPL2 engine.
 * See SomaBase.h for scope notes.
 */

#include "SomaBase.h"
#include "HpslTranspilerSelfTest.h"
#include "SomaSplash.h"

#include "system/HeadlessControl.h"

//---------------------------------------

cSomaBase *gpSomaBase = NULL;

//---------------------------------------

//////////////////////////////////////////////////////////////////////////
// HEADLESS CONTROL COMMANDS (see HPL2/core/include/system/HeadlessControl.h)
//
// No player/script layer exists in this free-fly scaffold, so this is just
// the debug camera's own transform. mpDebugCamera is checked at call time,
// not registration time: it doesn't exist until InitMainMenuScene()/
// InitTestMap() run, which happens later (after the splash sequence, via
// OnSplashFinished()) than where these are registered below.
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

	tString sError;
	if(pBase->LoadMap(sMap, vPos, sError) == false)
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

	mpTestWorld = NULL;
	mpDebugCamera = NULL;
	mpDebugViewport = NULL;
	mpDebugCameraController = NULL;
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

	Log("SOMA game module - Phase 0 scaffolding (%s)\n", msGameName.c_str());

	/////////////////////////////
	// Init the engine: create the window, load resources.cfg/materials.cfg,
	// and get to a state where an empty scene can be rendered.
	if (InitEngine() == false)
		return false;

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
	// Main loop - a map is loaded and a debug free-fly camera is active
	// (see InitTestMap()), but there is still no player controller and no
	// scripts running.
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
	cEngineInitVars vars;
	vars.mGraphics.mvScreenSize = cVector2l(1280, 720);
	vars.mGraphics.mbFullscreen = false;
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
	SetLogFile(sStateDir + _W("hpl.log"));
#endif

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

	return true;
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

bool cSomaBase::LoadMap(const tString &asMapFile, const cVector3f &avStartPos, tString &asErrorOut)
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

	if (mpTestWorld) mpEngine->GetScene()->DestroyWorld(mpTestWorld);
	mpTestWorld = pNewWorld;

	// Reuse the existing camera/viewport/controller if this isn't the first
	// load rather than destroying and recreating them - cUpdater has no
	// "remove" counterpart to AddGlobalUpdate() (see ExitTestMap()'s own
	// comment on this), so a fresh cSomaDebugFreeCamera on every call would
	// leak one dangling iUpdateable per call once its camera is destroyed
	// below. cViewport::SetWorld() is the real engine API for exactly this
	// "same camera, new world" case.
	if (mpDebugCamera == NULL)
	{
		mpDebugCamera = mpEngine->GetScene()->CreateCamera(eCameraMoveMode_Fly);
		mpDebugCamera->SetFarClipPlane(200.0f);
		mpDebugViewport = mpEngine->GetScene()->CreateViewport(mpDebugCamera, mpTestWorld, true);
		mpDebugCameraController = hplNew(cSomaDebugFreeCamera, (mpDebugCamera, mpEngine->GetInput()));
		mpEngine->GetUpdater()->AddGlobalUpdate(mpDebugCameraController);
	}
	else
	{
		mpDebugViewport->SetWorld(mpTestWorld);
	}

	mpDebugCamera->SetPosition(avStartPos);
	mpDebugCamera->SetPitch(0);
	mpDebugCamera->SetYaw(0);

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

	mpDebugViewport = NULL;
	mpDebugCamera = NULL;
	mpTestWorld = NULL;
}

//-----------------------------------------------------------------------
