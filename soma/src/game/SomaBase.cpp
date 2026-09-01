/*
 * Phase 0 scaffolding for a SOMA game module running on the HPL2 engine.
 * See SomaBase.h for scope notes.
 */

#include "SomaBase.h"
#include "HpslTranspilerSelfTest.h"

//---------------------------------------

cSomaBase *gpSomaBase = NULL;

//---------------------------------------

cSomaBase::cSomaBase()
{
	mpEngine = NULL;

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
	// One-shot HPSL->GLSL transpiler proof-of-concept - see
	// HpslTranspilerSelfTest.h. Not part of real rendering yet; just
	// proves whether the transpiled clear_vtx/clear_frag pair compiles as
	// real GLSL against the live GL context. Safe to run every boot: it
	// only reads shader files and compiles throwaway GL shader objects.
	RunHpslTranspilerSelfTest(mpEngine);

	/////////////////////////////
	// Phase 1: load a hardcoded test map through the new SOMA ".hpm" world
	// loader and set up a debug free-fly camera to look at it with.
	if (InitTestMap() == false)
		return false;

	return true;
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
