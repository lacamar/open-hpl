/*
 * Phase 0 scaffolding for an AMFP game module running on the HPL2 engine.
 * See AmfpBase.h for scope notes.
 */

#include "AmfpBase.h"

//---------------------------------------

cAmfpBase *gpAmfpBase = NULL;

//---------------------------------------

cAmfpBase::cAmfpBase()
{
	mpEngine = NULL;

	mpTestWorld = NULL;
	mpDebugCamera = NULL;
	mpDebugViewport = NULL;
	mpDebugCameraController = NULL;
}

//-----------------------------------------------------------------------

cAmfpBase::~cAmfpBase()
{
}

//-----------------------------------------------------------------------

bool cAmfpBase::Init(const tString &asCommandline)
{
	/////////////////////////////
	// Parse the command line (an alternate init config file path, same
	// convention as cLuxBase::ParseCommandLine)
	if (ParseCommandLine(asCommandline) == false)
		return false;

	/////////////////////////////
	// Load AMFP's real main_init.cfg to get resource/material config paths
	// and the game name - like Amnesia's main_init.cfg (not SOMA's), AMFP's
	// has the full set of Menu/PreMenu/Demo/etc config file entries, but
	// Phase 0 only pulls out what it actually needs.
	if (InitMainConfig() == false)
		return false;

	Log("AMFP game module - Phase 0 scaffolding (%s)\n", msGameName.c_str());

	/////////////////////////////
	// Init the engine: create the window, load resources.cfg/materials.cfg,
	// and get to a state where an empty scene can be rendered.
	if (InitEngine() == false)
		return false;

	/////////////////////////////
	// Phase 1: load a hardcoded test map through the existing HPL2 ".map"
	// world loader (unchanged from Dark Descent - AMFP maps are the same
	// format) and set up a debug free-fly camera to look at it with.
	if (InitTestMap() == false)
		return false;

	return true;
}

//-----------------------------------------------------------------------

void cAmfpBase::Exit()
{
	ExitTestMap();
	ExitEngine();
}

//-----------------------------------------------------------------------

void cAmfpBase::Run()
{
	// Main loop - a map is loaded and a debug free-fly camera is active
	// (see InitTestMap()), but there is still no player controller and no
	// scripts running.
	mpEngine->Run();
}

//-----------------------------------------------------------------------

bool cAmfpBase::ParseCommandLine(const tString &asCommandline)
{
	msInitConfigFile = cString::To16Char(asCommandline);
	if (msInitConfigFile == _W(""))
		msInitConfigFile = _W("config/main_init.cfg");

	return true;
}

//-----------------------------------------------------------------------

bool cAmfpBase::InitMainConfig()
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
	msGameName = pInitCfg->GetString("Variables", "GameName", "Amnesia - A Machine For Pigs");

	hplDelete(pInitCfg);

	return true;
}

//-----------------------------------------------------------------------

bool cAmfpBase::InitEngine()
{
	cEngineInitVars vars;
	vars.mGraphics.mvScreenSize = cVector2l(1280, 720);
	vars.mGraphics.mbFullscreen = false;
	vars.mGraphics.msWindowCaption = msGameName + " (Phase 0)";

	/////////////////////////
	// Create the engine
	mpEngine = CreateHPLEngine(eHplAPI_OpenGL, eHplSetup_All, &vars);
	if (mpEngine == NULL)
	{
		msErrorMessage = _W("Could not create HPL engine!");
		return false;
	}

	/////////////////////////
	// Load AMFP's real resource directory listing and physics surface data.
	mpEngine->GetResources()->LoadResourceDirsFile(msResourceConfigPath);
	mpEngine->GetPhysics()->LoadSurfaceData(msMaterialConfigPath);

	return true;
}

//-----------------------------------------------------------------------

void cAmfpBase::ExitEngine()
{
	if (mpEngine)
		DestroyHPLEngine(mpEngine);
	mpEngine = NULL;
}

//-----------------------------------------------------------------------

bool cAmfpBase::InitTestMap()
{
	////////////////////////////////////
	// Hardcoded Phase 1 test map: main_init.cfg's own <StartMap File=
	// "01_mansion_01.map" Folder="maps/main/" Pos="InitStart" /> - the
	// game's actual first map. Found by basename via the resource dir
	// search ("/maps" is registered with AddSubDirs in AMFP's real
	// resources.cfg), same convention meshes/entities use - so this is not
	// an absolute filesystem path (and the map file itself in fact lives
	// directly under maps/, not maps/main/ - that Folder value is unused by
	// this basename lookup, same situation as SOMA's InitTestMap).
	cWorld *pWorld = mpEngine->GetScene()->LoadWorld("01_mansion_01.map", 0);
	if (pWorld == NULL)
	{
		msErrorMessage = _W("Could not load test map '01_mansion_01.map'!");
		return false;
	}
	mpTestWorld = pWorld;

	////////////////////////////////////
	// Debug free-fly camera (see DebugFreeCamera.h) - no player controller.
	// Start position/facing taken directly from the map's own "InitStart"
	// Area (WorldPos="-58 5.75 -42.5" Rotation="-3.12847 1.5708 -3.12847" in
	// 01_mansion_01.map), nudged up to a more eye-like height. Hardcoded
	// rather than resolved through the engine's Area system, since Phase 1
	// has no game-side PlayerStart area loader registered to query.
	cCamera *pCamera = mpEngine->GetScene()->CreateCamera(eCameraMoveMode_Fly);
	pCamera->SetPosition(cVector3f(-58.0f, 6.45f, -42.5f));
	pCamera->SetYaw(1.5708f);
	pCamera->SetFarClipPlane(200.0f);
	mpDebugCamera = pCamera;

	mpDebugViewport = mpEngine->GetScene()->CreateViewport(pCamera, pWorld, true);

	mpDebugCameraController = hplNew(cAmfpDebugFreeCamera, (pCamera, mpEngine->GetInput()));
	mpEngine->GetUpdater()->AddGlobalUpdate(mpDebugCameraController);

	return true;
}

//-----------------------------------------------------------------------

void cAmfpBase::ExitTestMap()
{
	// mpDebugCamera / mpDebugViewport / mpTestWorld are owned by cScene and
	// torn down together with the rest of the engine in ExitEngine().
	//
	// mpDebugCameraController was registered with cUpdater::AddGlobalUpdate,
	// which has no matching "remove" API; cUpdater itself is destroyed as
	// part of DestroyHPLEngine() right after this call, with no further
	// Update() in between, so it's left for that teardown rather than
	// explicitly deleted here against a dangling reference in the updater's
	// list (same reasoning as SOMA's ExitTestMap).
	mpDebugCameraController = NULL;

	mpDebugViewport = NULL;
	mpDebugCamera = NULL;
	mpTestWorld = NULL;
}

//-----------------------------------------------------------------------
