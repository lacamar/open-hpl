/*
 * Phase 0 scaffolding for an Amnesia: The Bunker game module running on the
 * HPL2 engine. See BunkerBase.h for scope notes.
 */

#include "BunkerBase.h"

#include "system/HeadlessControl.h"

#if defined(__linux__)
#include <unistd.h>
#endif

//---------------------------------------

cBunkerBase *gpBunkerBase = NULL;

//---------------------------------------

//////////////////////////////////////////////////////////////////////////
// HEADLESS CONTROL COMMANDS (see HPL2/core/include/system/HeadlessControl.h)
//
// Same shape as soma/src/game/SomaBase.cpp's - no player/script layer
// exists in this free-fly scaffold, so this is just the debug camera's own
// transform. Needed so this task's verification pass could confirm the
// PlayerStart-Area spawn fix (see BunkerAreaLoader.h) actually landed the
// camera at the map's real Start_Begin coordinates, not just "didn't
// crash" - the core control server only offers "screenshot"/"quit" until a
// game module registers commands of its own, and Bunker's Phase 0 had none
// before this.
//////////////////////////////////////////////////////////////////////////

static void cBunkerBase_HeadlessCmd_CameraState(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cBunkerBase *pBase = (cBunkerBase*)apUserData;
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

static void cBunkerBase_HeadlessCmd_SetCamera(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cBunkerBase *pBase = (cBunkerBase*)apUserData;
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

cBunkerBase::cBunkerBase()
{
	mpEngine = NULL;

	mpTestWorld = NULL;
	mpDebugCamera = NULL;
	mpDebugViewport = NULL;
	mpDebugCameraController = NULL;
}

//-----------------------------------------------------------------------

cBunkerBase::~cBunkerBase()
{
}

//-----------------------------------------------------------------------

bool cBunkerBase::Init(const tString &asCommandline)
{
	if (ParseCommandLine(asCommandline) == false)
		return false;

	if (InitMainConfig() == false)
		return false;

	Log("Amnesia: The Bunker game module - Phase 0 scaffolding (%s)\n", msGameName.c_str());

	if (InitEngine() == false)
		return false;

	// Headless control: register camera commands if a control server is
	// active (OPENHPL_HEADLESS_SOCKET) - see HeadlessControl.h and the
	// handler comment above.
	if (mpEngine->GetHeadlessControl())
	{
		cHeadlessControlServer *pCtrl = mpEngine->GetHeadlessControl();
		pCtrl->RegisterHandler("camera_state", cBunkerBase_HeadlessCmd_CameraState, this);
		pCtrl->RegisterHandler("set_camera", cBunkerBase_HeadlessCmd_SetCamera, this);
	}

	if (InitTestMap() == false)
		return false;

	return true;
}

//-----------------------------------------------------------------------

void cBunkerBase::Exit()
{
	ExitTestMap();
	ExitEngine();
}

//-----------------------------------------------------------------------

void cBunkerBase::Run()
{
	// Main loop - a map is loaded and a debug free-fly camera is active
	// (see InitTestMap()), but there is still no player controller and no
	// scripts running.
	mpEngine->Run();
}

//-----------------------------------------------------------------------

bool cBunkerBase::ParseCommandLine(const tString &asCommandline)
{
	msInitConfigFile = cString::To16Char(asCommandline);
	if (msInitConfigFile == _W(""))
		msInitConfigFile = _W("config/main_init.cfg");

	return true;
}

//-----------------------------------------------------------------------

// "Main:trenches.hpm, PostIntro:officer_hub.hpm" -> "trenches.hpm" (see
// BunkerBase.h for why only the first entry is taken).
static tString FirstStartMapFile(const tString &asRaw)
{
	tString sFirst = asRaw;
	size_t lComma = sFirst.find(',');
	if (lComma != tString::npos)
		sFirst = sFirst.substr(0, lComma);

	size_t lColon = sFirst.find(':');
	if (lColon != tString::npos)
		sFirst = sFirst.substr(lColon + 1);

	// Trim surrounding whitespace left over from the comma/colon split.
	size_t lStart = sFirst.find_first_not_of(" \t");
	size_t lEnd = sFirst.find_last_not_of(" \t");
	if (lStart == tString::npos)
		return "";
	return sFirst.substr(lStart, lEnd - lStart + 1);
}

//-----------------------------------------------------------------------

bool cBunkerBase::InitMainConfig()
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
	msGameName = pInitCfg->GetString("Variables", "GameName", "Amnesia: The Bunker");

	msStartMapFile = FirstStartMapFile(pInitCfg->GetString("StartMap", "File", ""));
	msStartMapPos = pInitCfg->GetString("StartMap", "Pos", "");

	hplDelete(pInitCfg);

	return true;
}

//-----------------------------------------------------------------------

bool cBunkerBase::InitEngine()
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
	sStateDir += _W("bunker/");
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
	// assumptions), so the Bunker's own resources.cfg/materials.cfg load
	// unmodified.
	mpEngine->GetResources()->LoadResourceDirsFile(msResourceConfigPath);
	mpEngine->GetPhysics()->LoadSurfaceData(msMaterialConfigPath);

	// See BunkerAreaLoader.h - without this, cWorldLoaderHpm::CreateMapArea
	// drops every PlayerStart Area on the floor (logged as "no area loader
	// registered for AreaType 'PlayerStart'") and InitTestMap() has nothing
	// to resolve <StartMap Pos="..."/> against.
	mpEngine->GetResources()->AddAreaLoader(hplNew(cBunkerAreaLoader_PlayerStart, ("PlayerStart")));

	return true;
}

//-----------------------------------------------------------------------

void cBunkerBase::ExitEngine()
{
	if (mpEngine)
		DestroyHPLEngine(mpEngine);
	mpEngine = NULL;
}

//-----------------------------------------------------------------------

bool cBunkerBase::InitTestMap()
{
	if (msStartMapFile == "")
	{
		msErrorMessage = _W("main_init.cfg has no usable <StartMap File=.../> entry");
		return false;
	}

	////////////////////////////////////
	// Found by basename via the resource dir search - GameMapFolder="maps/"
	// from main_init.cfg's <Directories> is already registered with
	// AddSubDirs in the Bunker's real resources.cfg.
	cBunkerAreaLoader_PlayerStart::Clear();
	cWorld *pWorld = mpEngine->GetScene()->LoadWorld(msStartMapFile, 0);
	if (pWorld == NULL)
	{
		msErrorMessage = _W("Could not load start map '") + cString::To16Char(msStartMapFile) + _W("'");
		return false;
	}
	mpTestWorld = pWorld;

	////////////////////////////////////
	// Camera position from the map's own declared start position. The
	// Bunker's maps carry this as a PlayerStart-type Area (see
	// BunkerAreaLoader.h), not a cStartPosEntity - GetStartPosEntity() is
	// kept as a fallback in case a future map ever has one instead (e.g. if
	// a later phase reuses this scaffold against an older-format map).
	// Nudged up half a metre from the Area's own transform - PlayerStart
	// Areas are placed at floor/foot level, not eye level (confirmed
	// against a real install's trenches.hpm_Area: Start_Begin's WorldPos.y
	// is 0.978, consistent with a foot position, not a ~1.7m eye height).
	cVector3f vPos(0, 1.7f, 0);
	if (msStartMapPos != "")
	{
		cMatrixf mtxStart;
		if (cBunkerAreaLoader_PlayerStart::GetStartTransform(msStartMapPos, mtxStart))
		{
			vPos = mtxStart.GetTranslation() + cVector3f(0, 0.5f, 0);
		}
		else if (cStartPosEntity *pStartPos = pWorld->GetStartPosEntity(msStartMapPos))
		{
			vPos = pStartPos->GetWorldMatrix().GetTranslation() + cVector3f(0, 0.5f, 0);
		}
		else
		{
			Log("Bunker: start map has no PlayerStart Area or StartPosEntity named '%s', using world origin\n", msStartMapPos.c_str());
		}
	}

	cCamera *pCamera = mpEngine->GetScene()->CreateCamera(eCameraMoveMode_Fly);
	pCamera->SetPosition(vPos);
	pCamera->SetFarClipPlane(200.0f);
	mpDebugCamera = pCamera;

	mpDebugViewport = mpEngine->GetScene()->CreateViewport(pCamera, pWorld, true);

	mpDebugCameraController = hplNew(cBunkerDebugFreeCamera, (pCamera, mpEngine->GetInput()));
	mpEngine->GetUpdater()->AddGlobalUpdate(mpDebugCameraController);

	return true;
}

//-----------------------------------------------------------------------

void cBunkerBase::ExitTestMap()
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
