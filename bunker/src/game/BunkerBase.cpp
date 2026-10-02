
#include "BunkerBase.h"

#include "system/HeadlessControl.h"

#if defined(__linux__)
#include <unistd.h>
#endif

cBunkerBase *gpBunkerBase = NULL;

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

cBunkerBase::cBunkerBase()
{
	mpEngine = NULL;

	mpTestWorld = NULL;
	mpDebugCamera = NULL;
	mpDebugViewport = NULL;
	mpDebugCameraController = NULL;
}

cBunkerBase::~cBunkerBase()
{
}

bool cBunkerBase::Init(const tString &asCommandline)
{
	if (ParseCommandLine(asCommandline) == false)
		return false;

	if (InitMainConfig() == false)
		return false;

	Log("Amnesia: The Bunker game module - Phase 0 scaffolding (%s)\n", msGameName.c_str());

	if (InitEngine() == false)
		return false;

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

void cBunkerBase::Exit()
{
	ExitTestMap();
	ExitEngine();
}

void cBunkerBase::Run()
{
	mpEngine->Run();
}

bool cBunkerBase::ParseCommandLine(const tString &asCommandline)
{
	msInitConfigFile = cString::To16Char(asCommandline);
	if (msInitConfigFile == _W(""))
		msInitConfigFile = _W("config/main_init.cfg");

	return true;
}

static tString FirstStartMapFile(const tString &asRaw)
{
	tString sFirst = asRaw;
	size_t lComma = sFirst.find(',');
	if (lComma != tString::npos)
		sFirst = sFirst.substr(0, lComma);

	size_t lColon = sFirst.find(':');
	if (lColon != tString::npos)
		sFirst = sFirst.substr(lColon + 1);

	size_t lStart = sFirst.find_first_not_of(" \t");
	size_t lEnd = sFirst.find_last_not_of(" \t");
	if (lStart == tString::npos)
		return "";
	return sFirst.substr(lStart, lEnd - lStart + 1);
}

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

bool cBunkerBase::InitEngine()
{
	cEngineInitVars vars;
	vars.mGraphics.mvScreenSize = cVector2l(1280, 720);
	vars.mGraphics.mbFullscreen = false;
	vars.mGraphics.msWindowCaption = msGameName + " (Phase 0)";

#if defined(__linux__)
	tWString sStateRoot = cPlatform::GetSystemSpecialPath(eSystemPath_XDGStateHome);
	tWString sStateDir = sStateRoot + _W("open-hpl/");
	if(cPlatform::FolderExists(sStateDir) == false) cPlatform::CreateFolder(sStateDir);
	sStateDir += _W("bunker/");
	if(cPlatform::FolderExists(sStateDir) == false) cPlatform::CreateFolder(sStateDir);

	// PID suffix: concurrent runs truncate a shared log
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

	mpEngine->GetResources()->LoadResourceDirsFile(msResourceConfigPath);
	mpEngine->GetPhysics()->LoadSurfaceData(msMaterialConfigPath);

	mpEngine->GetResources()->AddAreaLoader(hplNew(cBunkerAreaLoader_PlayerStart, ("PlayerStart")));

	return true;
}

void cBunkerBase::ExitEngine()
{
	if (mpEngine)
		DestroyHPLEngine(mpEngine);
	mpEngine = NULL;
}

bool cBunkerBase::InitTestMap()
{
	if (msStartMapFile == "")
	{
		msErrorMessage = _W("main_init.cfg has no usable <StartMap File=.../> entry");
		return false;
	}

	cBunkerAreaLoader_PlayerStart::Clear();
	cWorld *pWorld = mpEngine->GetScene()->LoadWorld(msStartMapFile, 0);
	if (pWorld == NULL)
	{
		msErrorMessage = _W("Could not load start map '") + cString::To16Char(msStartMapFile) + _W("'");
		return false;
	}
	mpTestWorld = pWorld;

	// PlayerStart is at foot level
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

	mpDebugCameraController = hplNew(cDebugFreeCamera, (pCamera, mpEngine->GetInput()));
	mpEngine->GetUpdater()->AddGlobalUpdate(mpDebugCameraController);

	return true;
}

void cBunkerBase::ExitTestMap()
{
	// no cUpdater remove API; freed with the updater
	mpDebugCameraController = NULL;

	mpDebugViewport = NULL;
	mpDebugCamera = NULL;
	mpTestWorld = NULL;
}
