#include "SomaLuxGame.h"
#include "RustSomaLuxS9.h"
#include "SomaS5Dump.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxPlayer.h"
#include "SomaLuxVoice.h"
#include "SomaImGui.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"
#include "impl/RustEasArray.h"

#include <SDL2/SDL.h>
#include <cstring>

cSomaLuxGame *cSomaLuxGame::mpInstance = NULL;

// Config files: user settings and keys live under $XDG_CONFIG_HOME/open-hpl/soma, seeded from the game's defaults

static cConfigFile *OpenConfig(const tWString &asUserFile, const tWString &asDefaultFile)
{
	if (asUserFile != _W("") && cPlatform::FileExists(asUserFile) == false && cPlatform::FileExists(asDefaultFile))
		cPlatform::CloneFile(asDefaultFile, asUserFile, true);
	cConfigFile *pCfg = hplNew(cConfigFile, (asUserFile != _W("") ? asUserFile : asDefaultFile));
	pCfg->Load();
	return pCfg;
}

static cConfigFile *gpUserConfig = NULL, *gpKeyConfig = NULL, *gpGameConfig = NULL, *gpStatsConfig = NULL;

cConfigFile *SomaUserConfig() { return gpUserConfig; }
cConfigFile *SomaKeyConfig() { return gpKeyConfig; }
cConfigFile *SomaGameConfig() { return gpGameConfig; }

float SomaStringDuration(const tWString &asText)
{
	cConfigFile *c = gpGameConfig;
	float fMin = c->GetFloat("General", "TextDuration_MinTime", 2.5f);
	float fStart = c->GetFloat("General", "TextDuration_StartTime", 1.5f);
	float fChar = c->GetFloat("General", "TextDuration_CharTime", 0.07f);
	return ohpl_gs3_string_duration(fMin, fStart, fChar, asText.size());
}

static void LoadConfigs()
{
	if (gpUserConfig)
		return;
	tWString sGame = cPlatform::GetWorkingDir();
	tWString sUserDir = cPlatform::GetSystemSpecialPath(eSystemPath_XDGConfigHome) + _W("open-hpl/");
	cPlatform::CreateFolder(sUserDir);
	sUserDir += SomaGameIdW();
	cPlatform::CreateFolder(sUserDir);
	gpUserConfig = OpenConfig(sUserDir + _W("user_settings.cfg"), sGame + _W("/config/default_user_settings.cfg"));
	gpKeyConfig = OpenConfig(sUserDir + _W("user_keys.cfg"), sGame + _W("/config/default_user_keys.cfg"));
	gpGameConfig = OpenConfig(_W(""), sGame + _W("/config/game.cfg"));
	gpStatsConfig = OpenConfig(sUserDir + _W("user_stats.cfg"), _W(""));
}

static tString gsLanguage = "english";
const tString &SomaCurrentLanguage() { return gsLanguage; }

// cLuxConfigHandler::LoadUserConfig: screen, gamma, subtitles
void SomaReadUserScreenConfig(cSomaConfig *apCfg)
{
	LoadConfigs();
	cConfigFile *c = gpUserConfig;
	cRendererDeferred::SetSSAOBufferSizeDiv(c->GetInt("Graphics", "SSAOQuality", 0) == 1 ? 1 : 2);
	cRendererDeferred::SetDepthOfFieldSampleNum(4 << std::min(std::max(c->GetInt("Graphics", "DepthOfFieldQuality", 2), 0), 2));
	apCfg->mlScreenWidth = c->GetInt("Screen", "Width", apCfg->mlScreenWidth);
	apCfg->mlScreenHeight = c->GetInt("Screen", "Height", apCfg->mlScreenHeight);
	tString sFull = cString::ToLowerCase(c->GetString("Screen", "FullScreen", apCfg->mbFullscreen ? "true" : "false"));
	apCfg->mbFullscreen = sFull != "false";
	tString sVsync = cString::ToLowerCase(c->GetString("Screen", "Vsync", apCfg->mbVSync ? "true" : "false"));
	apCfg->mbVSync = sVsync == "true" || sVsync == "adaptive";
	apCfg->mfGamma = c->GetFloat("Graphics", "Brightness", apCfg->mfGamma);
	apCfg->mbShowSubtitles = c->GetBool("Sound", "ShowSubtitles", false);
	apCfg->mbDevHud = c->GetBool("Gameplay", "OpenHplHud", false);
	apCfg->mbAntiAliasing = c->GetBool("Graphics", "AntiAliasing", apCfg->mbAntiAliasing);
	apCfg->msRenderer = c->GetString("Graphics", "Renderer", apCfg->msRenderer);
	apCfg->msColorOutput = c->GetString("Graphics", "ColorOutput", apCfg->msColorOutput);
	apCfg->mfHdrPaperWhite = c->GetFloat("Graphics", "HdrPaperWhite", apCfg->mfHdrPaperWhite);
	apCfg->mfHdrPeak = c->GetFloat("Graphics", "HdrPeak", apCfg->mfHdrPeak);
	c->SetInt("Screen", "Width", apCfg->mlScreenWidth);
	c->SetInt("Screen", "Height", apCfg->mlScreenHeight);
	if (sFull != "borderless")
		c->SetString("Screen", "FullScreen", apCfg->mbFullscreen ? "true" : "false");
	if (sVsync != "adaptive")
		c->SetString("Screen", "Vsync", apCfg->mbVSync ? "true" : "false");
}

static void LoadLanguage()
{
	gsLanguage = cString::SetFileExt(gpUserConfig->GetString("Main", "StartLanguage", "english.lang"), "");
	cResources *pRes = gpSomaBase->mpEngine->GetResources();
	pRes->ClearTranslations();
	pRes->AddLanguageFile("config/base_" + gsLanguage + ".lang", false);
	pRes->AddLanguageFile("config/lang_main/" + gsLanguage + ".lang", false);
	cLanguageFile *pLang = pRes->GetLanguageFile();
	pLang->SetEntry("Menu", "OpenHplHud", _W("OPEN-HPL HUD"));
	static const wchar_t *vEntries[][2] = {
		{L"Renderer", L"RENDERER (RESTART)"}, {L"ColorOutput", L"COLOR OUTPUT"},
		{L"HdrPaperWhite", L"HDR PAPER WHITE"}, {L"HdrPeak", L"HDR PEAK BRIGHTNESS"},
		{L"OpenGL", L"OpenGL"}, {L"Vulkan", L"Vulkan"}, {L"SDR", L"SDR"}, {L"HDR10", L"HDR10"},
		{L"scRGB", L"scRGB HDR"}, {L"P3", L"Wide color (P3)"},
		{L"Renderer_Info", L"Graphics API. OpenGL is the default; Vulkan is experimental. Takes effect after a restart."},
		{L"ColorOutput_Info", L"SDR is standard. HDR10 and scRGB need an HDR display, P3 a wide-gamut one. Vulkan only."},
		{L"HdrPaperWhite_Info", L"Brightness of white UI and paper in nits when an HDR output is active."},
		{L"HdrPeak_Info", L"Peak brightness of your HDR display in nits."},
	};
	for (auto &e : vEntries)
		if (!pLang->HasEntry("Menu", cString::To8Char(e[0])))
			pLang->SetEntry("Menu", cString::To8Char(e[0]), e[1]);
}

// FullScreen "true" is exclusive at Width x Height (emulated by SDL on Wayland), "borderless" the desktop
void SomaApplyWindowMode(const cSomaConfig *apCfg)
{
	SDL_Window *pWindow = (SDL_Window*)gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->GetWindowHandle();
	if (pWindow == NULL || getenv("OPENHPL_HEADLESS_SOCKET"))
		return;
	tString sFull = cString::ToLowerCase(gpUserConfig->GetString("Screen", "FullScreen", apCfg->mbFullscreen ? "true" : "false"));
	if (sFull == "true" && apCfg->mlScreenWidth > 0 && apCfg->mlScreenHeight > 0)
	{
		int lDisplay = SDL_GetWindowDisplayIndex(pWindow);
		SDL_DisplayMode want = {}, mode = {};
		SDL_GetDesktopDisplayMode(lDisplay, &want);
		want.w = apCfg->mlScreenWidth;
		want.h = apCfg->mlScreenHeight;
		mode = want;
		SDL_GetClosestDisplayMode(lDisplay, &want, &mode);
		SDL_SetWindowFullscreen(pWindow, 0);
		SDL_SetWindowDisplayMode(pWindow, &mode);
		SDL_SetWindowFullscreen(pWindow, SDL_WINDOW_FULLSCREEN);
	}
	else if (sFull != "false")
		SDL_SetWindowFullscreen(pWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
	else
	{
		SDL_SetWindowFullscreen(pWindow, 0);
		if (apCfg->mlScreenWidth > 0 && apCfg->mlScreenHeight > 0)
			SDL_SetWindowSize(pWindow, apCfg->mlScreenWidth, apCfg->mlScreenHeight);
	}
}

// cLuxBase::UpdateGraphicSettings: texture quality and filtering
void SomaApplyTextureConfig()
{
	LoadConfigs();
	cMaterialManager *pMatMgr = gpSomaBase->mpEngine->GetResources()->GetMaterialManager();
	pMatMgr->SetTextureSizeDownScaleLevel(gpUserConfig->GetInt("Graphics", "TextureQuality", 0));
	pMatMgr->SetTextureFilter((eTextureFilter)gpUserConfig->GetInt("Graphics", "TextureFilter", eTextureFilter_Trilinear));
	pMatMgr->SetTextureAnisotropy(gpUserConfig->GetFloat("Graphics", "TextureAnisotropy", 1));
}

// cGlobalScriptFuncs::ApplyUserConfig: UpdateGraphicSettings, UpdateSoundSettings, LoadLanguage; never asks for a restart
void SomaApplyRenderConfig(cViewport *apViewport)
{
	cRenderSettings *s = apViewport->GetRenderSettings();
	s->mbUseFxaa = gpSomaBase->GetConfig()->mbAntiAliasing;
	s->mbSSAOActive = gpUserConfig->GetBool("Graphics", "SSAOActive", true);
	s->mbRenderShadows = gpUserConfig->GetBool("Graphics", "ShadowsActive", true);
	s->mbRenderWorldReflection = gpUserConfig->GetBool("Graphics", "WorldReflection", true);
}

static bool ApplyUserConfig()
{
	cSomaConfig *pCfg = gpSomaBase->GetConfig();
	SomaReadUserScreenConfig(pCfg);
	SomaApplyWindowMode(pCfg);
	SomaApplyTextureConfig();
	if (gpSomaBase->GetCurrentViewport())
		SomaApplyRenderConfig(gpSomaBase->GetCurrentViewport());
	if (cSomaLuxInputHandler::Get())
		cSomaLuxInputHandler::Get()->LoadUserConfig();
	if (cSomaLuxGame::Get())
		cSomaLuxGame::Get()->ReloadUserConfig();
	tString sVsync = cString::ToLowerCase(gpUserConfig->GetString("Screen", "Vsync", "true"));
	gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->SetVsyncActive(pCfg->mbVSync, sVsync == "adaptive");
	gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->SetGammaCorrection(pCfg->mfGamma);
	gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->SetColorOutput(pCfg->msColorOutput, pCfg->mfHdrPaperWhite, pCfg->mfHdrPeak);
	gpSomaBase->mpEngine->SetDevHudActive(pCfg->mbDevHud);
	pCfg->mfMasterVolume = gpUserConfig->GetFloat("Sound", "Volume", pCfg->mfMasterVolume);
	gpSomaBase->mpEngine->GetSound()->GetLowLevel()->SetVolume(pCfg->mfMasterVolume);
	pCfg->Save();
	LoadLanguage();
	return false;
}

static std::vector<cXmlElement *> ChildElements(iXmlNode *apNode)
{
	std::vector<cXmlElement *> v;
	if (apNode == NULL)
		return v;
	cXmlNodeListIterator it = apNode->GetChildIterator();
	while (it.HasNext())
		if (cXmlElement *pElem = it.Next()->ToElement())
			v.push_back(pElem);
	return v;
}

void SomaUpdateCameraTextures();
void SomaDestroyCameraTextures();
cSomaImGui *SomaHudImGui();

namespace
{
cSomaScriptRuntime *gpRuntime = NULL;
tString gsOut;

std::vector<tString> Parts(const OhplS5Io *io)
{
	std::vector<tString> v(1);
	for (size_t i = 0; io->s && i < io->n; ++i)
		if (io->s[i] == 0)
			v.emplace_back();
		else
			v.back() += (char)io->s[i];
	return v;
}

tString Str(const OhplS5Io *io) { return io->s ? tString((const char *)io->s, io->n) : tString(); }

void Out(OhplS5Io *io, const tString &asText)
{
	gsOut = asText;
	io->s = (const unsigned char *)gsOut.data();
	io->n = gsOut.size();
}

void DumpElement(tString &asOut, cXmlElement *apElem, int alDepth)
{
	asOut += (char)('0' + alDepth);
	asOut += '\x1f';
	asOut += apElem->GetValue();
	for (auto &it : *apElem->GetAttributeMap())
	{
		asOut += '\x1f';
		asOut += it.first;
		asOut += '\x1f';
		asOut += it.second;
	}
	asOut += '\x1e';
}

int32_t GameHostCall(void *, uint32_t alOp, OhplS5Io *io)
{
	cSomaLuxScriptable *pSelf = (cSomaLuxScriptable *)io->p[0];
	switch (alOp)
	{
	case kOhplS5c_DUMP_ENABLED: return OhplS5DumpEnabled();
	case kOhplS5c_DUMP_LOG: OhplS5Log("%s", Str(io).c_str()); return 0;
	case kOhplS5c_SELF_TEST: OhplS5SelfTest(); return 0;
	case kOhplS5c_NEW:
	{
		cSomaLuxScriptable *pNew = NULL;
		switch (io->i[0])
		{
		case 1: pNew = new cSomaLuxPlayer(); break;
		case 2: pNew = new cSomaLuxInputHandler(); break;
		case 3: pNew = new cSomaLuxEffect(); break;
		case 4: pNew = new cSomaLuxModule(); break;
		default: pNew = new cSomaLuxHandler(); break;
		}
		if (io->i[0] == 3)
		{
			static_cast<cSomaLuxEffect *>(pNew)->msName = Str(io);
			static_cast<cSomaLuxEffect *>(pNew)->mlId = io->i[1];
		}
		else if (io->i[0] == 4)
		{
			static_cast<cSomaLuxModule *>(pNew)->msName = Str(io);
			static_cast<cSomaLuxModule *>(pNew)->mlId = io->i[1];
		}
		else
			static_cast<cSomaLuxHandler *>(pNew)->msName = Str(io);
		io->p[0] = pNew;
		return 0;
	}
	case kOhplS5c_SET_SCRIPT_NAME: pSelf->msScriptName = Str(io); return 0;
	case kOhplS5c_SET_SAVED: pSelf->mbSaved = io->i[0] != 0; return 0;
	case kOhplS5c_PLAYER_CAMERA:
		static_cast<cSomaLuxPlayer *>(pSelf)->SetCamera(gpSomaBase->mpEngine->GetScene()->CreateCamera(eCameraMoveMode_Fly));
		return 0;
	case kOhplS5c_LOAD_SCRIPT:
	{
		std::vector<tString> v = Parts(io);
		return pSelf->LoadScript(gpRuntime, v[0], v[1], v[2]);
	}
	case kOhplS5c_DELETE: delete pSelf; return 0;
	case kOhplS5c_SC:
	{
		tString sDecl = Str(io);
		switch (io->i[0])
		{
		case kOhplS5cSc_UPDATE: pSelf->OnUpdate(io->f[0]); break;
		case kOhplS5cSc_POST_UPDATE: pSelf->OnPostUpdate(io->f[0]); break;
		case kOhplS5cSc_VARIABLE_UPDATE: pSelf->OnVariableUpdate(io->f[0]); break;
		case kOhplS5cSc_GUI: pSelf->OnGui(io->f[0]); break;
		case kOhplS5cSc_DRAW: pSelf->CallWithFloat("void OnDraw(float afFrameTime)", io->f[0]); break;
		case kOhplS5cSc_ACTION: pSelf->OnAction(io->i[1], io->i[2] != 0); break;
		case kOhplS5cSc_ANALOG: pSelf->OnAnalogInput(io->i[1], cVector3f(io->f[0], io->f[1], io->f[2])); break;
		case kOhplS5cSc_MAP_MESSAGE: pSelf->OnMapMessage(sDecl.c_str(), io->p[1]); break;
		case kOhplS5cSc_MESSAGE: pSelf->OnMessage(sDecl.c_str()); break;
		case kOhplS5cSc_CALL_OBJECT: pSelf->CallWithObject(sDecl, io->p[1]); break;
		}
		return 0;
	}
	case kOhplS5c_SCRIPT_OBJECT: io->p[1] = pSelf->GetScript(); return 0;
	case kOhplS5c_CFG_STRING:
	{
		std::vector<tString> v = Parts(io);
		Out(io, SomaGameConfig()->GetString(v[0], v[1], v[2]));
		return 0;
	}
	case kOhplS5c_CFG_FLOAT:
	{
		std::vector<tString> v = Parts(io);
		io->f[0] = SomaGameConfig()->GetFloat(v[0], v[1], io->f[0]);
		return 0;
	}
	case kOhplS5c_LOAD_CONFIGS: LoadConfigs(); return 0;
	case kOhplS5c_LOAD_LANGUAGE: LoadLanguage(); return 0;
	case kOhplS5c_ADD_VOICE: gpSomaBase->mpEngine->GetUpdater()->AddGlobalUpdate(new cSomaLuxVoiceHandler(gpSomaBase->mpEngine)); return 0;
	case kOhplS5c_PLAYER_EXISTS: return cSomaLuxPlayer::Get() != NULL;
	case kOhplS5c_INPUT_EXISTS: return cSomaLuxInputHandler::Get() != NULL;
	case kOhplS5c_XML_FLAT:
	{
		tString sPath = Str(io);
		cResources *pRes = gpSomaBase->mpEngine->GetResources();
		if (io->i[0] && pRes->GetFileSearcher()->GetFilePath(sPath) == _W(""))
			return -1;
		iXmlDocument *pDoc = pRes->LoadXmlDocument(sPath);
		if (pDoc == NULL)
			return 0;
		tString sDump;
		for (cXmlElement *pTop : ChildElements(pDoc))
		{
			DumpElement(sDump, pTop, 0);
			for (cXmlElement *pChild : ChildElements(pTop))
				DumpElement(sDump, pChild, 1);
		}
		pRes->DestroyXmlDocument(pDoc);
		Out(io, sDump);
		return 1;
	}
	case kOhplS5c_PLAYER_ADD_STATE:
	{
		std::vector<tString> v = Parts(io);
		if (io->i[1])
			cSomaLuxPlayer::Get()->AddMoveState(v[0], io->i[0], v[1], v[2]);
		else
			cSomaLuxPlayer::Get()->AddState(v[0], io->i[0], v[1], v[2]);
		return 0;
	}
	case kOhplS5c_MOUSE_REL:
	{
		cVector2l vRel = gpSomaBase->mpEngine->GetInput()->GetMouse()->GetRelPosition();
		io->i[0] = vRel.x;
		io->i[1] = vRel.y;
		return 0;
	}
	case kOhplS5c_DIALOG_UPDATE: cSomaLuxDialogHandler::Get()->Update(io->f[0]); return 0;
	case kOhplS5c_VOICE_UPDATE:
		if (cSomaLuxVoiceHandler::Get())
			cSomaLuxVoiceHandler::Get()->UpdateVoices(io->f[0]);
		return 0;
	case kOhplS5c_PAUSED: return gpSomaBase->mbScriptGamePaused;
	case kOhplS5c_MAP_ACTION:
		if (cSomaLuxMap::GetCurrent())
			cSomaLuxMap::GetCurrent()->OnAction(io->i[0], io->i[1] != 0);
		return 0;
	case kOhplS5c_UPDATE_CAMERA_TEXTURES: SomaUpdateCameraTextures(); return 0;
	case kOhplS5c_LEAVE_MAP:
		cSomaLuxDialogHandler::Get()->StopAll();
		SomaDestroyCameraTextures();
		// its Reset() keeps the entity id, so every later datamine fails
		SomaRunGlobalFunc("DatamineHandler", "", "_Global_StopDatamining");
		return 0;
	case kOhplS5c_GUI_FOCUS_HISTORY: cSomaImGui::UpdateFocusHistory(); return 0;
	case kOhplS5c_GUI_FOCUS:
	{
		cSomaImGui *pFocus = cSomaImGui::GetInputFocus();
		io->p[0] = pFocus;
		io->i[0] = pFocus != NULL;
		io->i[1] = pFocus && pFocus->GetSet()->Is3D();
		return 0;
	}
	case kOhplS5c_GUI_POINTER_MODE:
		gpSomaBase->mpEngine->GetInput()->GetLowLevel()->RelativeMouse(io->i[0]);
		gpSomaBase->mpEngine->GetInput()->GetLowLevel()->LockInput(true);
		return 0;
	case kOhplS5c_GUI_SEND_MOUSE:
	{
		iMouse *pMouse = gpSomaBase->mpEngine->GetInput()->GetMouse();
		cVector2l vPos = io->i[0] ? pMouse->GetAbsPosition() : gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeInt() / 2;
		((cSomaImGui *)io->p[0])->SendMousePosition(vPos, cVector2l(io->i[1], io->i[2]));
		return 0;
	}
	case kOhplS5c_GUI_HUD_BEGIN:
	{
		cSomaImGui *pHud = SomaHudImGui();
		cSomaImGui::SetCurrent(pHud);
		pHud->Begin(io->f[0]);
		return 0;
	}
	case kOhplS5c_GUI_MAP_ON_GUI:
		if (cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent())
			if (pMap->GetScript())
			{
				float fTime = io->f[0];
				cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void OnGui(float afTimeStep)", [=](asIScriptContext *c) { c->SetArgFloat(0, fTime); });
			}
		return 0;
	case kOhplS5c_GUI_HUD_END:
		SomaHudImGui()->End();
		cSomaImGui::SetCurrent(NULL);
		return 0;
	case kOhplS5c_HUD_FOCUS_SET:
		cSomaImGui::SetGameHudFocus(io->i[0] != 0);
		SomaHudImGui()->SetShowMouse(io->i[0] != 0);
		return 0;
	case kOhplS5c_HUD_FOCUS_GET: return cSomaImGui::GameHudFocus();
	case kOhplS5c_LOG: Log("%s", Str(io).c_str()); return 0;
	case kOhplS5c_DUMP_ANALOG: OhplS5Log("ana %d %.9g %.9g %.9g", io->i[0], io->f[0], io->f[1], io->f[2]); return 0;
	}
	if (alOp >= kOhplS9_FIRE)
		return OhplS9HostCall(alOp, io);
	return 0;
}
const OhplS5Host gGameHost = {NULL, GameHostCall};
}

cSomaLuxGame::cSomaLuxGame(cSomaScriptRuntime *apRuntime)
{
	mpInstance = this;
	gpRuntime = apRuntime;
	ohpl_s5c_game_install_host(&gGameHost);
	ohpl_s5c_game_new();
}

cSomaLuxGame::~cSomaLuxGame()
{
	ohpl_s5c_game_free();
	mpInstance = NULL;
}

const cSomaLuxGame::cEntityScript *cSomaLuxGame::GetEntityScript(const tString &asGroup, const tString &asType)
{
	static cEntityScript sScript;
	bool bFound = ohpl_s5c_game_entity_script(
		(const unsigned char *)asGroup.data(), asGroup.size(), (const unsigned char *)asType.data(), asType.size(),
		+[](void *apCtx, const unsigned char *apFile, size_t alFile, const unsigned char *apClass, size_t alClass) {
			cEntityScript *pOut = (cEntityScript *)apCtx;
			pOut->msFile.assign((const char *)apFile, alFile);
			pOut->msClass.assign((const char *)apClass, alClass);
		},
		&sScript);
	return bFound ? &sScript : NULL;
}

void cSomaLuxGame::RegisterNatives(asIScriptEngine *e)
{
	ohpl_s5c_game_register1(e);
	cSomaLuxPlayer::RegisterNatives(e);
	cSomaLuxVoiceHandler::RegisterNatives(e);
	cSomaLuxDialogHandler::RegisterNatives(e);
	cSomaLuxInputHandler::RegisterNatives(e);

	cSomaLuxModule module;
	cSomaLuxEffect effect;
	ohpl_s5c_game_register2(e, (int)((char *)&module.mlId - (char *)(cSomaLuxScriptable *)&module), (int)((char *)&effect.mlId - (char *)(cSomaLuxScriptable *)&effect));

	SOMA_FUNC(e, "cConfigFile@ cLux_GetUserConfig()", +[]() { return gpUserConfig; });
	SOMA_FUNC(e, "cConfigFile@ cLux_GetStatsConfig()", +[]() { return gpStatsConfig; });
	SOMA_FUNC(e, "float cLux_GetStringDuration(const tWString&in asStr)", SomaStringDuration);
	SOMA_METHOD(e, "iLowLevelGraphics", "void SetBrightness(float afX)", +[](iLowLevelGraphics *g, float x) { g->SetGammaCorrection(x); });
	SOMA_FUNC(e, "iFontData@ cResources_CreateFontData(const tString&in asName)", +[](const tString &s) { return gpSomaBase->mpEngine->GetResources()->GetFontManager()->CreateFontData(s); });
	// Rebirth display safe area
	SOMA_METHOD(e, "cGuiSet", "float GetDisplayScale()", +[](cGuiSet *) { return 1.0f; });
	SOMA_FUNC(e, "float cLux_GetHudDisplayScale()", +[]() { return 1.0f; });
	SOMA_METHOD(e, "cGuiSet", "void DrawFontEx(const tWString &in asText, iFontData @apFont, const cVector3f &in avPos,const cVector2f &in avSize, const cColor&in aColor, eFontAlign aAlign, eGuiMaterial aMaterial)",
				+[](cGuiSet *g, const tWString &t, iFontData *f, const cVector3f &p, const cVector2f &v, const cColor &c, eFontAlign al, eGuiMaterial m) { g->DrawFont(t, f, p, v, c, al, m); });
	SOMA_METHOD(e, "iFontData", "void GetWordWrapRows(float afLength,const cVector2f&in avSize,const tWString&in asString, array<tWString> &inout avRows)",
				+[](iFontData *f, float l, const cVector2f &v, const tWString &t, OhplEasArray &rows) {
					tWStringVec vRows;
					f->GetWordWrapRows(l, v.y, v, t, &vRows);
					rows.Resize(0);
					for (tWString &r : vRows)
						rows.InsertLast(&r);
				});
	SOMA_METHOD(e, "iFontData", "float GetLength(const cVector2f&in avSize,const tWString&in asString)", +[](iFontData *f, const cVector2f &v, const tWString &t) { return f->GetLength(v, t.c_str()); });
	SOMA_FUNC(e, "bool cLux_ApplyUserConfig()", +[]() { return ApplyUserConfig(); });
	SOMA_FUNC(e, "bool cLux_GetSaveConfigAtExit()", +[]() { return true; });
	SOMA_FUNC(e, "cConfigFile@ cLux_GetKeyConfig()", +[]() { return gpKeyConfig; });
	SOMA_FUNC(e, "cConfigFile@ cLux_GetGameConfig()", +[]() { return gpGameConfig; });
	SOMA_FUNC(e, "bool cLux_GetSupportExplorationMode()", +[]() { return gpGameConfig && gpGameConfig->GetBool("General", "SupportExplorationMode", false); });
	const char *C = "cConfigFile";
	typedef const tString &S;
	SOMA_METHOD(e, C, "bool Load()", +[](cConfigFile *c) { return c->Load(); });
	SOMA_METHOD(e, C, "bool Save()", +[](cConfigFile *c) { return c->Save(); });
	SOMA_METHOD(e, C, "void SetString(const tString&in asLevel, const tString&in asName, const tString&in asVal)", +[](cConfigFile *c, S l, S n, S v) { c->SetString(l, n, v); });
	SOMA_METHOD(e, C, "void SetInt(const tString&in asLevel, const tString&in asName, int alVal)", +[](cConfigFile *c, S l, S n, int v) { c->SetInt(l, n, v); });
	SOMA_METHOD(e, C, "void SetFloat(const tString&in asLevel, const tString&in asName, float afVal)", +[](cConfigFile *c, S l, S n, float v) { c->SetFloat(l, n, v); });
	SOMA_METHOD(e, C, "void SetBool(const tString&in asLevel, const tString&in asName, bool abVal)", +[](cConfigFile *c, S l, S n, bool v) { c->SetBool(l, n, v); });
	SOMA_METHOD(e, C, "void SetVector2f(const tString&in asLevel, const tString&in asName, const cVector2f&in avVal)", +[](cConfigFile *c, S l, S n, const cVector2f &v) { c->SetVector2f(l, n, v); });
	SOMA_METHOD(e, C, "void SetVector3f(const tString&in asLevel, const tString&in asName, const cVector3f&in avVal)", +[](cConfigFile *c, S l, S n, const cVector3f &v) { c->SetVector3f(l, n, v); });
	SOMA_METHOD(e, C, "tString GetString(const tString&in asLevel, const tString&in asName, const tString&in asDefault)", +[](cConfigFile *c, S l, S n, S d) { return c->GetString(l, n, d); });
	SOMA_METHOD(e, C, "tWString GetStringW(const tString&in asLevel, const tString&in asName, const tWString&in asDefault)", +[](cConfigFile *c, S l, S n, const tWString &d) { return c->GetStringW(l, n, d); });
	SOMA_METHOD(e, C, "int GetInt(const tString&in asLevel, const tString&in asName, int alDefault)", +[](cConfigFile *c, S l, S n, int d) { return c->GetInt(l, n, d); });
	SOMA_METHOD(e, C, "float GetFloat(const tString&in asLevel, const tString&in asName, float afDefault)", +[](cConfigFile *c, S l, S n, float d) { return c->GetFloat(l, n, d); });
	SOMA_METHOD(e, C, "bool GetBool(const tString&in asLevel, const tString&in asName, bool abDefault)", +[](cConfigFile *c, S l, S n, bool d) { return c->GetBool(l, n, d); });
	SOMA_METHOD(e, C, "cVector2f GetVector2f(const tString&in asLevel, const tString&in asName, const cVector2f&in avDefault)", +[](cConfigFile *c, S l, S n, const cVector2f &d) { return c->GetVector2f(l, n, d); });
	SOMA_METHOD(e, C, "cVector3f GetVector3f(const tString&in asLevel, const tString&in asName, const cVector3f&in avDefault)", +[](cConfigFile *c, S l, S n, const cVector3f &d) { return c->GetVector3f(l, n, d); });
	SOMA_METHOD(e, C, "cVector2l GetVector2l(const tString&in asLevel, const tString&in asName, const cVector2l&in avDefault)", +[](cConfigFile *c, S l, S n, const cVector2l &d) { return c->GetVector2l(l, n, d); });
	SOMA_METHOD(e, C, "cColor GetColor(const tString&in asLevel, const tString&in asName, const cColor&in aDefault)", +[](cConfigFile *c, S l, S n, const cColor &d) { return c->GetColor(l, n, d); });
	ohpl_s5c_game_register3(e);
}
