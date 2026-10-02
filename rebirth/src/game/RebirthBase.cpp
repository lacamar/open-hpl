
#include "RebirthBase.h"

#include "RebirthLoaders.h"

#include "system/HeadlessControl.h"

#if defined(__linux__)
#include <unistd.h>
#endif

cRebirthBase *gpRebirthBase = NULL;

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

cRebirthBase::cRebirthBase()
{
	mpEngine = NULL;

	mpTestWorld = NULL;
	mpDebugCamera = NULL;
	mpDebugViewport = NULL;
	mpDebugCameraController = NULL;
}

cRebirthBase::~cRebirthBase()
{
}

bool cRebirthBase::Init(const tString &asCommandline)
{
	if (ParseCommandLine(asCommandline) == false)
		return false;

	if (InitMainConfig() == false)
		return false;

	Log("Amnesia: Rebirth game module - Phase 0 scaffolding (%s)\n", msGameName.c_str());

	if (InitEngine() == false)
		return false;

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

void cRebirthBase::Exit()
{
	ExitTestMap();
	ExitEngine();
}

void cRebirthBase::Run()
{
	mpEngine->Run();
}

bool cRebirthBase::ParseCommandLine(const tString &asCommandline)
{
	msInitConfigFile = cString::To16Char(asCommandline);
	if (msInitConfigFile == _W(""))
		msInitConfigFile = _W("config/main_init.cfg");

	return true;
}

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

bool cRebirthBase::InitEngine()
{
	cEngineInitVars vars;
	vars.mGraphics.mvScreenSize = cVector2l(1280, 720);
	vars.mGraphics.mbFullscreen = false;
	vars.mGraphics.msWindowCaption = msGameName + " (Phase 0)";

#if defined(__linux__)
	tWString sStateRoot = cPlatform::GetSystemSpecialPath(eSystemPath_XDGStateHome);
	tWString sStateDir = sStateRoot + _W("open-hpl/");
	if(cPlatform::FolderExists(sStateDir) == false) cPlatform::CreateFolder(sStateDir);
	sStateDir += _W("rebirth/");
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

	RegisterRebirthLoaders(mpEngine->GetResources());

	return true;
}

void cRebirthBase::ExitEngine()
{
	if (mpEngine)
		DestroyHPLEngine(mpEngine);
	mpEngine = NULL;
}

bool cRebirthBase::InitTestMap()
{
	if (msStartMapFile == "")
	{
		msErrorMessage = _W("main_init.cfg has no <StartMap File=.../> entry");
		return false;
	}

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

	mpDebugCameraController = hplNew(cDebugFreeCamera, (pCamera, mpEngine->GetInput()));
	mpEngine->GetUpdater()->AddGlobalUpdate(mpDebugCameraController);

	return true;
}

void cRebirthBase::ExitTestMap()
{
	// no cUpdater remove API; freed with the updater
	mpDebugCameraController = NULL;

	mpDebugViewport = NULL;
	mpDebugCamera = NULL;
	mpTestWorld = NULL;
}
