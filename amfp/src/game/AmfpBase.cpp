
#include "AmfpBase.h"

#include "system/HeadlessControl.h"
#include "../../../amnesia/src/game/RustLuxBase.h"

cAmfpBase *gpAmfpBase = NULL;

static void cAmfpBase_RunHeadless(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp, uint32_t alCmd)
{
	cAmfpBase *pBase = (cAmfpBase*)apUserData;

	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r)
	{
		cCamera *pCam = pBase->GetDebugCamera();
		switch(op)
		{
		case eOhplGa9Op_ReqHas: r->i = aReq.HasKey(OhplGa9GetS(a[0])); break;
		case eOhplGa9Op_ReqFloat: r->f = aReq.GetFloat(OhplGa9GetS(a[0]), a[1].f); break;
		case eOhplGa9Op_RespError: aResp.SetError(OhplGa9GetS(a[0])); break;
		case eOhplGa9Op_RespFloat: aResp.Set(OhplGa9GetS(a[0]), a[1].f); break;
		case eOhplGa9Op_Camera: r->i = pCam != NULL; break;
		case eOhplGa9Op_CameraValue:
			switch(a[0].i)
			{
			case 0: r->f = pCam->GetPosition().x; break;
			case 1: r->f = pCam->GetPosition().y; break;
			case 2: r->f = pCam->GetPosition().z; break;
			case 3: r->f = pCam->GetPitch(); break;
			case 4: r->f = pCam->GetYaw(); break;
			}
			break;
		case eOhplGa9Op_Fps: r->f = pBase->mpEngine->GetFPS(); break;
		case eOhplGa9Op_SetCameraPos: pCam->SetPosition(cVector3f(a[0].f, a[1].f, a[2].f)); break;
		case eOhplGa9Op_SetPitch: pCam->SetPitch(a[0].f); break;
		case eOhplGa9Op_SetYaw: pCam->SetYaw(a[0].f); break;
		}
	};
	ohpl_ga9_headless(alCmd, &call, OhplGa9CallOf<decltype(call)>::Fn());
}

static void cAmfpBase_HeadlessCmd_CameraState(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cAmfpBase_RunHeadless(apUserData, aReq, aResp, eOhplGa9Cmd_CameraState);
}

static void cAmfpBase_HeadlessCmd_SetCamera(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cAmfpBase_RunHeadless(apUserData, aReq, aResp, eOhplGa9Cmd_SetCamera);
}

cAmfpBase::cAmfpBase()
{
	mpEngine = NULL;

	mpTestWorld = NULL;
	mpDebugCamera = NULL;
	mpDebugViewport = NULL;
	mpDebugCameraController = NULL;
}

cAmfpBase::~cAmfpBase()
{
}

bool cAmfpBase::Init(const tString &asCommandline)
{
	if (ParseCommandLine(asCommandline) == false)
		return false;

	if (InitMainConfig() == false)
		return false;

	{
		tString sBanner;
		auto emit = [&](uint32_t tag, const void *p, size_t n) { sBanner = OhplGa9EmitS(p, n); };
		ohpl_ga9_amfp_text(1, (const unsigned char*)msGameName.data(), msGameName.size(), OhplGa9EmitOf<decltype(emit)>::Fn(), &emit);
		Log("%s", sBanner.c_str());
	}

	if (InitEngine() == false)
		return false;

	if (mpEngine->GetHeadlessControl())
	{
		cHeadlessControlServer *pCtrl = mpEngine->GetHeadlessControl();
		pCtrl->RegisterHandler("camera_state", cAmfpBase_HeadlessCmd_CameraState, this);
		pCtrl->RegisterHandler("set_camera", cAmfpBase_HeadlessCmd_SetCamera, this);
	}

	if (InitTestMap() == false)
		return false;

	return true;
}

void cAmfpBase::Exit()
{
	ExitTestMap();
	ExitEngine();
}

void cAmfpBase::Run()
{
	mpEngine->Run();
}

bool cAmfpBase::ParseCommandLine(const tString &asCommandline)
{
	auto emit = [&](uint32_t tag, const void *p, size_t n) { msInitConfigFile = OhplGa9EmitW(p, n); };
	ohpl_ga9_amfp_init_config_file((const unsigned char*)asCommandline.data(), asCommandline.size(), OhplGa9EmitOf<decltype(emit)>::Fn(), &emit);

	return true;
}

bool cAmfpBase::InitMainConfig()
{
	cConfigFile *pInitCfg = hplNew(cConfigFile, (msInitConfigFile));
	bool bLoaded = pInitCfg->Load();

	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r) { OhplGa9CfgCall(pInitCfg, op, a, r); };
	auto emit = [&](uint32_t tag, const void *p, size_t n)
	{
		switch(tag)
		{
		case eOhplGa9Tag_AmfpResources: msResourceConfigPath = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_AmfpMaterials: msMaterialConfigPath = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_AmfpGameName: msGameName = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_AmfpError: msErrorMessage = OhplGa9EmitW(p, n); break;
		}
	};
	bool bOk = ohpl_ga9_amfp_load_vars(&call, OhplGa9CallOf<decltype(call)>::Fn(), bLoaded,
										(const uint32_t*)msInitConfigFile.data(), msInitConfigFile.size(),
										OhplGa9EmitOf<decltype(emit)>::Fn(), &emit);

	hplDelete(pInitCfg);

	return bOk;
}

bool cAmfpBase::InitEngine()
{
	cEngineInitVars vars;
	vars.mGraphics.mvScreenSize = cVector2l(1280, 720);
	vars.mGraphics.mbFullscreen = false;
	{
		auto emit = [&](uint32_t tag, const void *p, size_t n) { vars.mGraphics.msWindowCaption = OhplGa9EmitS(p, n); };
		ohpl_ga9_amfp_text(0, (const unsigned char*)msGameName.data(), msGameName.size(), OhplGa9EmitOf<decltype(emit)>::Fn(), &emit);
	}

	mpEngine = CreateHPLEngine(eHplAPI_OpenGL, eHplSetup_All, &vars);
	if (mpEngine == NULL)
	{
		msErrorMessage = _W("Could not create HPL engine!");
		return false;
	}

	mpEngine->GetResources()->LoadResourceDirsFile(msResourceConfigPath);
	mpEngine->GetPhysics()->LoadSurfaceData(msMaterialConfigPath);

	return true;
}

void cAmfpBase::ExitEngine()
{
	if (mpEngine)
		DestroyHPLEngine(mpEngine);
	mpEngine = NULL;
}

bool cAmfpBase::InitTestMap()
{
	cWorld *pWorld = mpEngine->GetScene()->LoadWorld("01_mansion_01.map", 0);
	if (pWorld == NULL)
	{
		msErrorMessage = _W("Could not load test map '01_mansion_01.map'!");
		return false;
	}
	mpTestWorld = pWorld;

	// from the map's InitStart area, raised to eye height
	cCamera *pCamera = mpEngine->GetScene()->CreateCamera(eCameraMoveMode_Fly);
	pCamera->SetPosition(cVector3f(-58.0f, 6.45f, -42.5f));
	pCamera->SetYaw(1.5708f);
	pCamera->SetFarClipPlane(200.0f);
	mpDebugCamera = pCamera;

	mpDebugViewport = mpEngine->GetScene()->CreateViewport(pCamera, pWorld, true);

	mpDebugCameraController = hplNew(cDebugFreeCamera, (pCamera, mpEngine->GetInput()));
	mpEngine->GetUpdater()->AddGlobalUpdate(mpDebugCameraController);

	return true;
}

void cAmfpBase::ExitTestMap()
{
	// no cUpdater remove API; freed with the updater
	mpDebugCameraController = NULL;

	mpDebugViewport = NULL;
	mpDebugCamera = NULL;
	mpTestWorld = NULL;
}
