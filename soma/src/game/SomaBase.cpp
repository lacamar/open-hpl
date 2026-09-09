/*
 * Phase 0 scaffolding for a SOMA game module running on the HPL2 engine.
 * See SomaBase.h for scope notes.
 */

#include "SomaBase.h"
#include "HpslTranspilerSelfTest.h"
#include "HpslTranspiler.h"
#include "SomaLoaders.h"
#include "SomaAmbientSfx.h"
#include "SomaSplash.h"

#include "system/HeadlessControl.h"
#include "resources/GpuShaderManager.h"
#include "graphics/RendererDeferred.h"

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
	// Degrees, not cCamera::GetFOV()'s native radians - added to verify the
	// real Options screen's Horizontal FOV slider (see SomaConfig.h's
	// mfFOV/SomaPlayer.cpp) actually reaches the real camera live.
	aResp.Set("fov_deg", cMath::ToDeg(pBase->GetDebugCamera()->GetFOV()));
}

// Headless debug hook onto cRendererDeferred's own existing debug quad-view
// of the raw G-buffer contents (color/diffuse top-left, normal+depth top-
// right, specular bottom-left, a 4th target bottom-right if present) - lets
// a headless screenshot inspect each render target directly instead of only
// the final composited frame. Used to root-cause the SOMA "real lights get
// routed for rendering but contribute ~0 visible brightness" investigation
// (see PORTING_NOTES.md) - confirmed live that the color/diffuse target is
// correctly populated (real, plausible lit texture data) while the normal+
// depth target renders solid black regardless of cRendererDeferred's own
// 32-bit vs 64-bit G-buffer texture format, meaning the deferred G-buffer
// solid pass's fragment shader isn't reaching that attachment at all - real,
// still-open investigation, not yet root-caused further than that (needs
// real GPU frame-capture tooling this headless workflow doesn't have).
static void cSomaBase_HeadlessCmd_SetDebugGbuffer(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cRendererDeferred::SetDebugRenderFrameBuffers(aReq.GetBool("enabled", false));
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

// "forward"/"backward"/"left"/"right"/"jump" -> eSomaPlayerAction, shared by
// the two headless commands below. Returns false (aResp gets an error set by
// the caller) for anything else.
static bool ParsePlayerActionName(const tString &asName, cSomaBase::eSomaPlayerAction &aActionOut)
{
	tString sLower = cString::ToLowerCase(asName);
	if(sLower == "forward") { aActionOut = cSomaBase::eSomaPlayerAction_Forward; return true; }
	if(sLower == "backward") { aActionOut = cSomaBase::eSomaPlayerAction_Backward; return true; }
	if(sLower == "left") { aActionOut = cSomaBase::eSomaPlayerAction_Left; return true; }
	if(sLower == "right") { aActionOut = cSomaBase::eSomaPlayerAction_Right; return true; }
	if(sLower == "jump") { aActionOut = cSomaBase::eSomaPlayerAction_Jump; return true; }
	return false;
}

// Headless-only verification hooks for SomaMainMenu.cpp's real KEYBINDINGS
// screen (see cSomaBase::RebindPlayerAction()/GetPlayerActionKeyName()) -
// same idea as camera_state/set_camera above, letting this be tested without
// clicking through the actual menu UI pixel-by-pixel. "keybind_get" reads
// the current binding; "keybind_set" rebinds it exactly like clicking a row
// and pressing a key would (used together with the generic "input" command's
// type=key events and "action_triggered" below to prove a rebind actually
// changes which real key the player controller responds to).
static void cSomaBase_HeadlessCmd_KeybindGet(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	cSomaBase::eSomaPlayerAction action;
	if(ParsePlayerActionName(aReq.GetString("action", ""), action) == false)
	{
		aResp.SetError("unknown 'action' - expected forward/backward/left/right/jump");
		return;
	}

	aResp.Set("key", pBase->GetPlayerActionKeyName(action));
}

static void cSomaBase_HeadlessCmd_KeybindSet(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	cSomaBase::eSomaPlayerAction action;
	if(ParsePlayerActionName(aReq.GetString("action", ""), action) == false)
	{
		aResp.SetError("unknown 'action' - expected forward/backward/left/right/jump");
		return;
	}

	tString sKeyName = aReq.GetString("key", "");
	eKey key = pBase->mpEngine->GetInput()->GetKeyboard()->StringToKey(sKeyName);
	if(key == eKey_LastEnum)
	{
		aResp.SetError("unknown 'key' name: '" + sKeyName + "'");
		return;
	}

	pBase->RebindPlayerAction(action, key);
	aResp.Set("key", pBase->GetPlayerActionKeyName(action));
}

// Real cAction::IsTriggerd() readback for one of cSomaBase's 5 player
// actions - lets a headless test confirm which real key an action responds
// to after a keybind_set rebind, by injecting a raw "input" type=key event
// for a specific key and checking whether the ACTION (not the key itself)
// reports triggered.
static void cSomaBase_HeadlessCmd_ActionTriggered(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	cSomaBase::eSomaPlayerAction action;
	if(ParsePlayerActionName(aReq.GetString("action", ""), action) == false)
	{
		aResp.SetError("unknown 'action' - expected forward/backward/left/right/jump");
		return;
	}

	aResp.Set("triggered", pBase->mpEngine->GetInput()->IsTriggerd(cSomaBase::GetPlayerActionName(action)));
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

	mpPreloadedMainMenuWorld = NULL;
	mbMainMenuWorldPreloadAttempted = false;

	mpIntroSequence = NULL;
	mpApartmentIntroCall = NULL;
}

//-----------------------------------------------------------------------

cSomaBase::~cSomaBase()
{
}

//-----------------------------------------------------------------------

bool cSomaBase::Init(const tString &asCommandline)
{
	/////////////////////////////
	// Set the real log destination FIRST, before anything below that could
	// Log()/Error() - see SetupLogFile()'s own comment for why this can't
	// wait until InitEngine() (that used to be where this happened, and
	// InitMainConfig() below - which can genuinely fail and Error() on a
	// missing/malformed main_init.cfg - ran before it).
	SetupLogFile();

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
	// Real SOMA's own .hpsl corpus (deferred_gbuffer_solid_frag.hpsl,
	// deferred_light_frag.hpsl, ...) always writes/reads the deferred
	// normal G-buffer raw and unconditionally stores linear (unpacked)
	// depth in its alpha channel - there is no HPL2-style "@ifdef
	// Deferred_32bit ... *0.5+0.5 ... @elseif Deferred_64bit ... raw ...
	// @endif" branch anywhere in the real shipped source (confirmed via a
	// full grep of the real corpus: every gl_FragData[1]/out_vNormal write
	// is unconditional). That's exactly what HPL2's existing
	// eDeferredGBuffer_64Bit renderer mode already means (see
	// RendererDeferred.cpp's own matching @ifdef Deferred_32bit/@elseif
	// Deferred_64bit branches in the real Dark Descent .glsl corpus) - but
	// cRendererDeferred::mGBufferType defaults to (and, with no call
	// anywhere in this codebase to ever change it, always stays)
	// eDeferredGBuffer_32Bit, a standard 8-bit-per-channel UNSIGNED G-buffer
	// texture. Writing SOMA's real raw signed (-1..1) normal components
	// into that unsigned target silently clamps every negative component
	// to 0 on write - corrupting the surface normal on nearly every pixel
	// in the game and starving real lights of any dot(N,L) contribution.
	// Root-caused live: a real New-Game boot into 00_01_apartment.hpm
	// rendered almost entirely black despite 38 real lights being
	// correctly culled into view near the camera - a LIGHTDIAG-SHADER
	// dump of the real transpiled deferred_gbuffer_solid_frag.glsl (with
	// the real Deferred_32bit=1 combo var actually set) showed the
	// unconditional, un-biased `gl_FragData[1].xyz = vScreenNormal;` this
	// comment describes. Must run before InitEngine() (which constructs
	// cRendererDeferred and allocates the G-buffer textures based on this
	// value) - same ordering constraint as SetHpslTranspileCallback()
	// above.
	cRendererDeferred::SetGBufferType(eDeferredGBuffer_64Bit);

	// cRendererDeferred::InitLightRendering() (RendererDeferred.cpp) attaches
	// a real GPU occlusion query (GetOcclusionQuery()) to any light whose
	// projected screen area exceeds mlMinLargeLightArea, then skips
	// re-rendering that light entirely on a later frame once the query's
	// sample count comes back at or below mlSampleVisiblilityLimit - a real
	// performance optimization (never re-shade a light that's fully hidden
	// behind other geometry). Root-caused live: a real New Game boot into
	// 00_01_apartment.hpm rendered near pure black (mean pixel value ~10/255)
	// despite 38 real lights correctly culled into view - a temporary
	// counter on cRenderSettings::mlNumberOfLightsRendered showed a real,
	// nonzero count (16, then 19) in the first couple of rendered frames,
	// then permanently 0 from then on. Every light in this small apartment
	// bedroom sits very close to the camera, so each has a large projected
	// screen area and takes this occlusion-query path - consistent with
	// queries reporting these lights as occluded/invisible once their
	// results are actually available (a 1-frame-later readback), which
	// would never happen for a real light this close to the camera. This
	// GPU/driver stack (Mesa on Apple Silicon, not what HPL2's occlusion
	// query path was ever verified against) is the plausible source, not
	// verified further than that. Disabling this test for SOMA (verified
	// live: mean pixel value rose from ~10/255 to ~26/255 and real
	// previously-invisible room geometry/detail became visible in a
	// screenshot at the identical camera pose) trades a real-but-broken
	// perf optimization for correct light visibility - Dark Descent/AMFP
	// are untouched by this (SOMA-only call, and mbOcclusionTestLargeLights
	// is a process-wide static, but each game module is its own process).
	cRendererDeferred::SetOcclusionTestLargeLights(false);

	// CRITICAL, must run before CreateHPLEngine() below, not merely before
	// map/resource loading: cMeshLoaderCollada's real, original behavior is
	// to treat a shipped `.msh` as a rebuildable cache of the `.dae` source,
	// recompiling and unconditionally SaveMesh()-ing over it. On this Linux
	// port that install directory is the real Steam depot, and this
	// (correctly, for everything loaded later) used to be disabled by a
	// call placed inside InitEngine() itself, right after CreateHPLEngine().
	// That was too late for one specific case: CreateHPLEngine() (below)
	// constructs cGraphics, which constructs cRendererDeferred, whose own
	// constructor immediately calls LoadVertexBufferFromMesh("core_box.dae",
	// ...) (and core_pyramid/core_*_sphere) to build its debug/light-volume
	// shapes - before InitEngine() ever returns, let alone reaches its own
	// old call site. Found live: a real (non-headless) SOMA launch just
	// rewrote exactly these 5 files' real `.msh` caches in the real Steam
	// install directory (confirmed via mtimes matching the launch, and
	// Steam's own "5 files failed to validate" integrity check reacting to
	// it) - the exact zero-tolerance class of bug already fixed once for
	// every other, later-loaded mesh, just never for these five, since
	// nothing reached this code path at all before this session's earlier
	// fix made core_box.dae loadable in the first place (previously the
	// engine just crashed here instead - see the FatalError fix earlier
	// this session). Hardcoded true, not config-driven, same reasoning as
	// this call's own original site.
	cResources::SetForceCacheLoadingAndSkipSaving(true);

	/////////////////////////////
	// Init the engine: create the window, load resources.cfg/materials.cfg,
	// and get to a state where an empty scene can be rendered.
	if (InitEngine() == false)
		return false;

	// Safe wherever this sits now - SetupLogFile() at the very top of Init()
	// already set a real, XDG-routed log destination before anything else
	// in this function could Log()/Error(). This used to need to sit after
	// InitEngine() specifically (which used to be the only place
	// SetLogFile() was called) - confirmed live, once: running a build with
	// this Log() call in its old spot (right after InitMainConfig(), before
	// SetLogFile() existed anywhere) from a scratch test directory that (per
	// this project's own established headless-testing pattern) symlinks
	// "hpl.log" back to the real install for tailing wrote this exact line
	// into the real Steam SOMA install's hpl.log - exactly what this
	// project has a zero-tolerance policy against.
	Log("SOMA game module - Phase 0 scaffolding (%s)\n", msGameName.c_str());

	/////////////////////////////
	// Headless control: register camera commands if a control server is
	// active (OPENHPL_HEADLESS_SOCKET) - see HeadlessControl.h.
	if (mpEngine->GetHeadlessControl())
	{
		cHeadlessControlServer *pCtrl = mpEngine->GetHeadlessControl();
		pCtrl->RegisterHandler("camera_state", cSomaBase_HeadlessCmd_CameraState, this);
		pCtrl->RegisterHandler("set_debug_gbuffer", cSomaBase_HeadlessCmd_SetDebugGbuffer, this);
		pCtrl->RegisterHandler("set_camera", cSomaBase_HeadlessCmd_SetCamera, this);
		pCtrl->RegisterHandler("start_map", cSomaBase_HeadlessCmd_StartMap, this);
		pCtrl->RegisterHandler("keybind_get", cSomaBase_HeadlessCmd_KeybindGet, this);
		pCtrl->RegisterHandler("keybind_set", cSomaBase_HeadlessCmd_KeybindSet, this);
		pCtrl->RegisterHandler("action_triggered", cSomaBase_HeadlessCmd_ActionTriggered, this);
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
			// A custom test map won instead of the real main menu - destroy
			// any real main-menu world cSomaSplash's boot-init phase already
			// loaded via PreloadMainMenuWorld() (see that method's own
			// comment), else it leaks: InitMainMenuScene() (the only other
			// consumer) never runs on this path, so nothing else will ever
			// free the cWorld cScene::LoadWorld() heap-allocated for it.
			if (mpPreloadedMainMenuWorld)
			{
				mpEngine->GetScene()->DestroyWorld(mpPreloadedMainMenuWorld);
				mpPreloadedMainMenuWorld = NULL;
			}
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

// Split out of InitEngine() and called first thing from Init(), before
// InitMainConfig() - InitMainConfig()'s cConfigFile::Load() calls Error()
// on a missing/malformed main_init.cfg, which (like cSomaConfig::Load()'s
// own Log() call, see the comment below) needs a real log destination
// already set up to avoid falling back to the engine's pre-SetLogFile()
// default: a bare relative "hpl.log" resolved against cwd, which for a
// real Steam launch (or a headless-check.sh run against a real install -
// see that script's own guard, added for exactly this reason) is the real
// Steam install directory. This was a real, confirmed-live gap: this one
// call site was missed when the equivalent pre-SetLogFile() ordering bug
// was fixed for cSomaConfig::Load()/the old "Phase 0 scaffolding" Log()
// call (see PORTING_NOTES.md).
void cSomaBase::SetupLogFile()
{
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
	vars.mGraphics.msWindowCaption = msGameName + " (Phase 0)";

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

	// Real Resolution row's own restart-required contract - see
	// SomaConfig.h's mlScreenWidth/mlScreenHeight comment. Was hardcoded
	// 1280x720 here before those fields existed.
	vars.mGraphics.mvScreenSize = cVector2l(mConfig.mlScreenWidth, mConfig.mlScreenHeight);

	// TEMP DEBUG ONLY - not for commit: lets a large windowed boot size be
	// tested headlessly (a hidden window never picks up real monitor
	// dimensions for SDL_WINDOW_FULLSCREEN_DESKTOP, so this is the only way
	// to reproduce "booted directly at a large real resolution" headlessly).
	// Deliberately AFTER the config-driven default above so this always wins
	// over a persisted Resolution setting for debugging.
	if (getenv("OPENHPL_SOMA_DEBUG_SCREENSIZE"))
	{
		int lW = 1280, lH = 720;
		sscanf(getenv("OPENHPL_SOMA_DEBUG_SCREENSIZE"), "%dx%d", &lW, &lH);
		vars.mGraphics.mvScreenSize = cVector2l(lW, lH);
	}

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

	// See SomaAmbientSfx.h/.cpp - real map-authored ambient sound entities
	// (car honks, distant dogs, seagulls, a fridge hum, ...) reference real
	// FMOD-event names with no matching .snt resource anywhere in the
	// install, so cWorldLoaderHpm::LoadSoundsTrack() (which already runs,
	// unmodified) silently fails to create every one. Must run before any
	// map load, same as RegisterSomaLoaders() above.
	cSomaAmbientSfx::EnsureCached(mpEngine->GetResources());

	/////////////////////////
	// Apply the persisted settings that DO have a live/runtime API (unlike
	// Fullscreen above, which only applies at the next InitEngine()) - same
	// APIs amnesia/src/game/LuxMainMenu_Options.cpp's own Options menu uses
	// for these.
	mpEngine->GetSound()->GetLowLevel()->SetVolume(mConfig.mfMasterVolume);
	mpEngine->GetGraphics()->GetLowLevel()->SetGammaCorrection(mConfig.mfGamma);
	mpEngine->GetGraphics()->GetLowLevel()->SetVsyncActive(mConfig.mbVSync, false);

	CreateInputActions();

	return true;
}

//-----------------------------------------------------------------------

const char* cSomaBase::GetPlayerActionName(eSomaPlayerAction aAction)
{
	switch (aAction)
	{
	case eSomaPlayerAction_Forward: return "SomaMoveForward";
	case eSomaPlayerAction_Backward: return "SomaMoveBackward";
	case eSomaPlayerAction_Left: return "SomaMoveLeft";
	case eSomaPlayerAction_Right: return "SomaMoveRight";
	case eSomaPlayerAction_Jump: return "SomaJump";
	default: return "";
	}
}

//-----------------------------------------------------------------------

const wchar_t* cSomaBase::GetPlayerActionLabel(eSomaPlayerAction aAction)
{
	switch (aAction)
	{
	case eSomaPlayerAction_Forward: return L"MOVE FORWARD";
	case eSomaPlayerAction_Backward: return L"MOVE BACKWARD";
	case eSomaPlayerAction_Left: return L"MOVE LEFT";
	case eSomaPlayerAction_Right: return L"MOVE RIGHT";
	case eSomaPlayerAction_Jump: return L"JUMP";
	default: return L"";
	}
}

//-----------------------------------------------------------------------

void cSomaBase::CreateInputActions()
{
	// Real defaults matching cSomaPlayer's own previous hardcoded
	// eKey_W/S/A/D/Space checks (see SomaPlayer.cpp) - used whenever the
	// persisted config has no value yet (fresh install) or an
	// unparseable one (hand-edited typo).
	struct cDefaultBinding { eSomaPlayerAction mAction; tString *mpConfigField; eKey mDefaultKey; };
	cDefaultBinding vDefaults[] = {
		{ eSomaPlayerAction_Forward,  &mConfig.msKeyForward,  eKey_W },
		{ eSomaPlayerAction_Backward, &mConfig.msKeyBackward, eKey_S },
		{ eSomaPlayerAction_Left,     &mConfig.msKeyLeft,     eKey_A },
		{ eSomaPlayerAction_Right,    &mConfig.msKeyRight,    eKey_D },
		{ eSomaPlayerAction_Jump,     &mConfig.msKeyJump,     eKey_Space },
	};

	iKeyboard *pKeyboard = mpEngine->GetInput()->GetKeyboard();

	for (size_t i = 0; i < sizeof(vDefaults) / sizeof(vDefaults[0]); ++i)
	{
		const cDefaultBinding &def = vDefaults[i];

		cAction *pAction = mpEngine->GetInput()->CreateAction(GetPlayerActionName(def.mAction));

		eKey key = pKeyboard->StringToKey(*def.mpConfigField);
		if (key == eKey_LastEnum)
		{
			// Unparseable (or empty, on a fresh install where the config
			// field's constructor default is already the right string, but
			// this also self-heals a hand-edited bad value) - fall back to
			// the real hardcoded default and persist the corrected value so
			// it reads back clean next time.
			key = def.mDefaultKey;
			*def.mpConfigField = pKeyboard->KeyToString(key);
		}

		pAction->AddKey(key);
	}

	mConfig.Save();
}

//-----------------------------------------------------------------------

void cSomaBase::RebindPlayerAction(eSomaPlayerAction aAction, eKey aKey)
{
	cAction *pAction = mpEngine->GetInput()->GetAction(GetPlayerActionName(aAction));
	if (pAction == NULL)
		return;

	pAction->ClearSubActions();
	pAction->AddKey(aKey);

	tString sKeyName = mpEngine->GetInput()->GetKeyboard()->KeyToString(aKey);
	switch (aAction)
	{
	case eSomaPlayerAction_Forward:  mConfig.msKeyForward  = sKeyName; break;
	case eSomaPlayerAction_Backward: mConfig.msKeyBackward = sKeyName; break;
	case eSomaPlayerAction_Left:     mConfig.msKeyLeft     = sKeyName; break;
	case eSomaPlayerAction_Right:    mConfig.msKeyRight    = sKeyName; break;
	case eSomaPlayerAction_Jump:     mConfig.msKeyJump     = sKeyName; break;
	default: break;
	}

	mConfig.Save();
}

//-----------------------------------------------------------------------

tString cSomaBase::GetPlayerActionKeyName(eSomaPlayerAction aAction)
{
	cAction *pAction = mpEngine->GetInput()->GetAction(GetPlayerActionName(aAction));
	if (pAction == NULL || pAction->GetSubActionNum() == 0)
		return "-";

	return pAction->GetSubAction(0)->GetInputName();
}

//-----------------------------------------------------------------------

void cSomaBase::SetGameplayPaused(bool abPaused)
{
	if (mpPlayer)
		mpPlayer->SetActive(abPaused == false);

	if (mpMainMenu)
	{
		if (abPaused)
			mpMainMenu->ShowPaused();
		else
			mpMainMenu->HidePaused();
	}
}

//-----------------------------------------------------------------------

bool cSomaBase::IsGameplayPaused()
{
	return mpMainMenu && mpMainMenu->IsPaused();
}

//-----------------------------------------------------------------------

void cSomaBase::ExitEngine()
{
	if (mpEngine)
		DestroyHPLEngine(mpEngine);
	mpEngine = NULL;
}

//-----------------------------------------------------------------------

// See this method's own declaration comment in SomaBase.h. Reads the same
// main_init.cfg <MainMenu File=.../> entry InitMainMenuScene() below reads -
// duplicated rather than cached earlier for the same reason InitMainMenuScene()
// already re-reads it itself (Phase 0 only kept the two fields InitMainConfig()
// needed at the time).
cWorld* cSomaBase::PreloadMainMenuWorld()
{
	if (mbMainMenuWorldPreloadAttempted)
		return mpPreloadedMainMenuWorld;
	mbMainMenuWorldPreloadAttempted = true;

	cConfigFile *pInitCfg = hplNew(cConfigFile, (msInitConfigFile));
	if (pInitCfg->Load() == false)
	{
		hplDelete(pInitCfg);
		return NULL;
	}
	tString sMainMenuFile = pInitCfg->GetString("MainMenu", "File", "");
	hplDelete(pInitCfg);

	if (sMainMenuFile == "")
		return NULL;

	mpPreloadedMainMenuWorld = mpEngine->GetScene()->LoadWorld(sMainMenuFile, 0);
	return mpPreloadedMainMenuWorld;
}

//-----------------------------------------------------------------------

bool cSomaBase::InitMainMenuScene()
{
	// Consume whatever cSomaSplash's real boot-work step already loaded
	// (see PreloadMainMenuWorld()'s own comment) - grabbed unconditionally
	// up front so every return path below (including the early error
	// returns) leaves mpPreloadedMainMenuWorld NULL again, never orphaned.
	cWorld *pPreloadedWorld = mpPreloadedMainMenuWorld;
	mpPreloadedMainMenuWorld = NULL;

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
		if (pPreloadedWorld) mpEngine->GetScene()->DestroyWorld(pPreloadedWorld);
		return false;
	}
	tString sMainMenuFile = pInitCfg->GetString("MainMenu", "File", "");
	hplDelete(pInitCfg);

	if (sMainMenuFile == "")
	{
		msErrorMessage = _W("main_init.cfg has no <MainMenu File=.../> entry");
		if (pPreloadedWorld) mpEngine->GetScene()->DestroyWorld(pPreloadedWorld);
		return false;
	}

	////////////////////////////////////
	// Found by basename via the resource dir search, same convention as
	// InitTestMap()'s apartment map load below - Folder="maps/" from the
	// config is not needed, "/maps" is already registered with AddSubDirs
	// in SOMA's real resources.cfg. Reuses cSomaSplash's real preload
	// (see PreloadMainMenuWorld()) instead of loading a second time when
	// one is already available.
	cWorld *pWorld = pPreloadedWorld ? pPreloadedWorld : mpEngine->GetScene()->LoadWorld(sMainMenuFile, 0);
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

	// Real Anti-Aliasing row's live backend - see SomaConfig.h's
	// mbAntiAliasing comment. cRenderSettings defaults mbUseEdgeSmooth to
	// false (Renderer.cpp), so this needs applying explicitly on every fresh
	// cRenderSettings a new cViewport creates.
	mpDebugViewport->GetRenderSettings()->mbUseEdgeSmooth = mConfig.mbAntiAliasing;

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
	mpDebugViewport->GetRenderSettings()->mbUseEdgeSmooth = mConfig.mbAntiAliasing; // see InitMainMenuScene()'s copy of this line

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

	// Real Anti-Aliasing row's live backend (see SomaConfig.h's
	// mbAntiAliasing comment) - reapplied unconditionally on every map load,
	// covering both the "brand new cRenderSettings" branch above (which
	// defaults mbUseEdgeSmooth to false) and the "reused viewport" branch
	// (already correct, but cheap to just re-set).
	mpDebugViewport->GetRenderSettings()->mbUseEdgeSmooth = mConfig.mbAntiAliasing;

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

	// Real 00_01_apartment.hpm Munshi phone-call hand-port (see
	// SomaApartmentIntroCall.h) - as narrowly scoped/one-map-gated as the
	// intro sequence's own construction just above, and same "constructed
	// once, never destroyed" pattern.
	if (asMapFile == "00_01_apartment.hpm")
	{
		if (mpApartmentIntroCall == NULL)
		{
			mpApartmentIntroCall = hplNew(cSomaApartmentIntroCall, (mpEngine, this));
			mpEngine->GetUpdater()->AddGlobalUpdate(mpApartmentIntroCall);
		}
		else
		{
			Log("SOMA: apartment intro call object already exists, not starting a second one\n");
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
