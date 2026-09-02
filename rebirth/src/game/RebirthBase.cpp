/*
 * Phase 0 scaffolding for an Amnesia: Rebirth game module running on the
 * HPL2 engine. See RebirthBase.h for scope notes.
 */

#include "RebirthBase.h"

#include "RebirthLoaders.h"

#include "system/HeadlessControl.h"

#if defined(__linux__)
#include <unistd.h>
#endif

//---------------------------------------

cRebirthBase *gpRebirthBase = NULL;

//---------------------------------------

//////////////////////////////////////////////////////////////////////////
// HEADLESS CONTROL COMMANDS (see HPL2/core/include/system/HeadlessControl.h)
//
// Same shape as cSomaBase's/cAmfpBase's own camera_state/set_camera pair
// (soma/src/game/SomaBase.cpp, amfp/src/game/AmfpBase.cpp) - no player/
// script layer exists in this free-fly scaffold, so this is just the debug
// camera's own transform.
//////////////////////////////////////////////////////////////////////////

static void cRebirthBase_HeadlessCmd_CameraState(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cRebirthBase *pBase = (cRebirthBase*)apUserData;
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

static void cRebirthBase_HeadlessCmd_SetCamera(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cRebirthBase *pBase = (cRebirthBase*)apUserData;
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

	// Headless control: register camera commands if a control server is
	// active (OPENHPL_HEADLESS_SOCKET) - see HeadlessControl.h.
	if (mpEngine->GetHeadlessControl())
	{
		cHeadlessControlServer *pCtrl = mpEngine->GetHeadlessControl();
		pCtrl->RegisterHandler("camera_state", cRebirthBase_HeadlessCmd_CameraState, this);
		pCtrl->RegisterHandler("set_camera", cRebirthBase_HeadlessCmd_SetCamera, this);
	}

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

	// A fixed hpl.log path collides across concurrent headless test runs -
	// cLogWriter::ReopenFile() truncates on open, so a second process
	// launched while a first is still running silently wipes whatever the
	// first had already logged (see TASKS.md). Suffix with the PID under
	// OPENHPL_HEADLESS_SOCKET only, so normal interactive play keeps the
	// stable, predictable filename.
	tWString sLogFile = sStateDir + _W("hpl.log");
	if(getenv("OPENHPL_HEADLESS_SOCKET") != NULL)
	{
		sLogFile = sStateDir + _W("hpl-") + cString::ToStringW((int)getpid()) + _W(".log");
	}
	SetLogFile(sLogFile);
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

	// See RebirthLoaders.h - without these, cWorldLoaderHpm silently drops
	// every <Entity>/<Area> element in a real Rebirth map (confirmed via a
	// real boot log against real game data).
	RegisterRebirthLoaders(mpEngine->GetResources());

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
