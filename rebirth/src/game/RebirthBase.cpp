/*
 * Phase 0 scaffolding for an Amnesia: Rebirth game module running on the
 * HPL2 engine. See RebirthBase.h for scope notes.
 */

#include "RebirthBase.h"

//---------------------------------------

cRebirthBase *gpRebirthBase = NULL;

//---------------------------------------

cRebirthBase::cRebirthBase()
{
	mpEngine = NULL;

	mpTestWorld = NULL;
	mpDebugCamera = NULL;
	mpDebugViewport = NULL;
	mpDebugCameraController = NULL;
}

//-----------------------------------------------------------------------

cRebirthBase::~cRebirthBase()
{
}

//-----------------------------------------------------------------------

bool cRebirthBase::Init(const tString &asCommandline)
{
	if (ParseCommandLine(asCommandline) == false)
		return false;

	if (InitMainConfig() == false)
		return false;

	Log("Amnesia: Rebirth game module - Phase 0 scaffolding (%s)\n", msGameName.c_str());

	if (InitEngine() == false)
		return false;

	if (InitTestMap() == false)
		return false;

	return true;
}

//-----------------------------------------------------------------------

void cRebirthBase::Exit()
{
	ExitTestMap();
	ExitEngine();
}

//-----------------------------------------------------------------------

void cRebirthBase::Run()
{
	// Main loop - a map is loaded and a debug free-fly camera is active
	// (see InitTestMap()), but there is still no player controller and no
	// scripts running.
	mpEngine->Run();
}

//-----------------------------------------------------------------------

bool cRebirthBase::ParseCommandLine(const tString &asCommandline)
{
	msInitConfigFile = cString::To16Char(asCommandline);
	if (msInitConfigFile == _W(""))
		msInitConfigFile = _W("config/main_init.cfg");

	return true;
}

//-----------------------------------------------------------------------

bool cRebirthBase::InitMainConfig()
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
	msGameName = pInitCfg->GetString("Variables", "GameName", "Amnesia: Rebirth");

	msStartMapFile = pInitCfg->GetString("StartMap", "File", "");
	msStartMapPos = pInitCfg->GetString("StartMap", "Pos", "");

	hplDelete(pInitCfg);

	return true;
}

//-----------------------------------------------------------------------

bool cRebirthBase::InitEngine()
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
	sStateDir += _W("rebirth/");
	if(cPlatform::FolderExists(sStateDir) == false) cPlatform::CreateFolder(sStateDir);
	SetLogFile(sStateDir + _W("hpl.log"));
#endif

	mpEngine = CreateHPLEngine(eHplAPI_OpenGL, eHplSetup_All, &vars);
	if (mpEngine == NULL)
	{
		msErrorMessage = _W("Could not create HPL engine!");
		return false;
	}

	// Both parsers are fully generic (no Amnesia: The Dark Descent-specific
	// assumptions), so Rebirth's own resources.cfg/materials.cfg load
	// unmodified.
	mpEngine->GetResources()->LoadResourceDirsFile(msResourceConfigPath);
	mpEngine->GetPhysics()->LoadSurfaceData(msMaterialConfigPath);

	return true;
}

//-----------------------------------------------------------------------

void cRebirthBase::ExitEngine()
{
	if (mpEngine)
		DestroyHPLEngine(mpEngine);
	mpEngine = NULL;
}

//-----------------------------------------------------------------------

bool cRebirthBase::InitTestMap()
{
	if (msStartMapFile == "")
	{
		msErrorMessage = _W("main_init.cfg has no <StartMap File=.../> entry");
		return false;
	}

	////////////////////////////////////
	// Found by basename via the resource dir search - GameMapFolder="maps/"
	// from main_init.cfg's <Directories> is already registered with
	// AddSubDirs in Rebirth's real resources.cfg.
	cWorld *pWorld = mpEngine->GetScene()->LoadWorld(msStartMapFile, 0);
	if (pWorld == NULL)
	{
		msErrorMessage = _W("Could not load start map '") + cString::To16Char(msStartMapFile) + _W("'");
		return false;
	}
	mpTestWorld = pWorld;

	////////////////////////////////////
	// Camera position from the map's own declared start position (see
	// RebirthBase.h for why this is resolved generically rather than
	// hardcoded per-map, unlike SOMA's Phase 0). Nudged up half a metre from
	// whatever the entity's own transform gives - PlayerStart areas in this
	// engine generation are typically placed at floor/foot level, not eye
	// level.
	cVector3f vPos(0, 1.7f, 0);
	if (msStartMapPos != "")
	{
		cStartPosEntity *pStartPos = pWorld->GetStartPosEntity(msStartMapPos);
		if (pStartPos)
			vPos = pStartPos->GetWorldMatrix().GetTranslation() + cVector3f(0, 0.5f, 0);
		else
			Log("Rebirth: start map has no StartPosEntity named '%s', using world origin\n", msStartMapPos.c_str());
	}

	cCamera *pCamera = mpEngine->GetScene()->CreateCamera(eCameraMoveMode_Fly);
	pCamera->SetPosition(vPos);
	pCamera->SetFarClipPlane(200.0f);
	mpDebugCamera = pCamera;

	mpDebugViewport = mpEngine->GetScene()->CreateViewport(pCamera, pWorld, true);

	mpDebugCameraController = hplNew(cRebirthDebugFreeCamera, (pCamera, mpEngine->GetInput()));
	mpEngine->GetUpdater()->AddGlobalUpdate(mpDebugCameraController);

	return true;
}

//-----------------------------------------------------------------------

void cRebirthBase::ExitTestMap()
{
	// mpDebugCamera / mpDebugViewport / mpTestWorld are owned by cScene and
	// torn down together with the rest of the engine in ExitEngine().
	// mpDebugCameraController was registered with cUpdater::AddGlobalUpdate,
	// which has no matching "remove" API (see cSomaBase::ExitTestMap()'s
	// comment for the same constraint) - left for cUpdater's own teardown.
	mpDebugCameraController = NULL;

	mpDebugViewport = NULL;
	mpDebugCamera = NULL;
	mpTestWorld = NULL;
}

//-----------------------------------------------------------------------
