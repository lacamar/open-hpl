#include "SomaLuxGame.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxPlayer.h"
#include "SomaLuxVoice.h"
#include "SomaImGui.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"
#include "impl/scriptarray.h"

#include <SDL2/SDL.h>
#include <cstring>

cSomaLuxGame *cSomaLuxGame::mpInstance = NULL;

cSomaLuxGame::cSomaLuxGame(cSomaScriptRuntime *apRuntime) : mpRuntime(apRuntime)
{
	mpInstance = this;
}

cSomaLuxGame::~cSomaLuxGame()
{
	for (cSomaLuxScriptable *p : mvModules)
		delete p;
	for (cSomaLuxScriptable *p : mvEffects)
		delete p;
	for (cSomaLuxScriptable *p : mvHandlers)
		delete p;
	mpInstance = NULL;
}

template <class F> void cSomaLuxGame::ForEach(F aFunc, bool abActiveEffectsOnly)
{
	for (cSomaLuxHandler *p : mvHandlers)
		aFunc(p);
	// cLuxEffectHandler is a built-in global module, registered before user modules
	for (cSomaLuxEffect *p : mvEffects)
		if (p->mbActive || !abActiveEffectsOnly)
			aFunc(p);
	for (cSomaLuxModule *p : mvModules)
		aFunc(p);
}

// Config files: user settings and keys live under $XDG_CONFIG_HOME/open-hpl/soma, seeded from the game's defaults

static cConfigFile *OpenConfig(const tWString &asUserFile, const tWString &asDefaultFile)
{
	if (asUserFile != _W("") && cPlatform::FileExists(asUserFile) == false && cPlatform::FileExists(asDefaultFile))
		cPlatform::CloneFile(asDefaultFile, asUserFile, true);
	cConfigFile *pCfg = hplNew(cConfigFile, (asUserFile != _W("") ? asUserFile : asDefaultFile));
	pCfg->Load();
	return pCfg;
}

static cConfigFile *gpUserConfig = NULL, *gpKeyConfig = NULL, *gpGameConfig = NULL;

cConfigFile *SomaUserConfig() { return gpUserConfig; }
cConfigFile *SomaKeyConfig() { return gpKeyConfig; }
cConfigFile *SomaGameConfig() { return gpGameConfig; }

float SomaStringDuration(const tWString &asText)
{
	cConfigFile *c = gpGameConfig;
	return std::max(c->GetFloat("General", "TextDuration_MinTime", 2.5f),
					c->GetFloat("General", "TextDuration_StartTime", 1.5f) + asText.size() * c->GetFloat("General", "TextDuration_CharTime", 0.07f));
}

static void LoadConfigs()
{
	if (gpUserConfig)
		return;
	tWString sGame = cPlatform::GetWorkingDir();
	tWString sUserDir = cPlatform::GetSystemSpecialPath(eSystemPath_XDGConfigHome) + _W("open-hpl/");
	cPlatform::CreateFolder(sUserDir);
	sUserDir += _W("soma/");
	cPlatform::CreateFolder(sUserDir);
	gpUserConfig = OpenConfig(sUserDir + _W("user_settings.cfg"), sGame + _W("/config/default_user_settings.cfg"));
	gpKeyConfig = OpenConfig(sUserDir + _W("user_keys.cfg"), sGame + _W("/config/default_user_keys.cfg"));
	gpGameConfig = OpenConfig(_W(""), sGame + _W("/config/game.cfg"));
}

static tString gsLanguage = "english";
const tString &SomaCurrentLanguage() { return gsLanguage; }

// cLuxConfigHandler::LoadUserConfig: screen, gamma, subtitles
void SomaReadUserScreenConfig(cSomaConfig *apCfg)
{
	LoadConfigs();
	cConfigFile *c = gpUserConfig;
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
	cLanguageCategory *&pMenu = (*pRes->GetLanguageFile()->GetCategoryMap())["Menu"];
	if (pMenu == NULL)
		pMenu = new cLanguageCategory();
	pMenu->m_mapEntries["OpenHplHud"] = new cLanguageEntry{_W("OPEN-HPL HUD")};
}

// FullScreen "true" is exclusive at Width x Height (emulated by SDL on Wayland), "borderless" the desktop
void SomaApplyWindowMode(const cSomaConfig *apCfg)
{
	SDL_Window *pWindow = SDL_GL_GetCurrentWindow();
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

void cSomaLuxGame::Load()
{
	LoadConfigs();

	LoadLanguage();
	gpSomaBase->mpEngine->GetUpdater()->AddGlobalUpdate(new cSomaLuxVoiceHandler(gpSomaBase->mpEngine));

	cResources *pRes = gpSomaBase->mpEngine->GetResources();

	// game.cfg: <ScriptFiles Player=... InputHandler=...>
	struct cHandlerDef { const char *mpAttr; const char *mpClass; const char *mpBase; };
	static const cHandlerDef vHandlers[] = {
		{"InputHandler", "cScrInputHandler", "cLuxInputHandler"},
		{"EventDatabaseHandler", "cScrEventDatabaseHandler", "cLuxEventDatabaseHandler"},
		{"GuiHandler", "cScrGuiHandler", "cLuxGuiHandler"},
		{"AchievementHandler", "cScrAchievementHandler", "iLuxAchievementHandler"},
		{"HeroStatsHandler", "cScrHeroStatsHandler", "iLuxHeroStatsHandler"},
		{"RichPresenceHandler", "cScrRichPresenceHandler", "iLuxRichPresenceHandler"},
		{"Player", "cScrPlayer", "cLuxPlayer"},
	};
	cConfigFile *pGame = gpGameConfig;
	for (const cHandlerDef &def : vHandlers)
	{
		tString sFile = pGame->GetString("ScriptFiles", def.mpAttr, "");
		if (sFile.empty())
			continue;
		cSomaLuxHandler *pHandler = strcmp(def.mpAttr, "Player") == 0		   ? new cSomaLuxPlayer()
									: strcmp(def.mpAttr, "InputHandler") == 0 ? new cSomaLuxInputHandler()
																			   : new cSomaLuxHandler();
		pHandler->msName = def.mpAttr;
		pHandler->msScriptName = strcmp(def.mpAttr, "Player") == 0 ? "LuxPlayer" : def.mpAttr;
		if (pHandler->LoadScript(mpRuntime, sFile, def.mpClass, def.mpBase))
			mvHandlers.push_back(pHandler);
		else
			delete pHandler;
	}
	mfPropInteractDistance = pGame->GetFloat("Prop", "DefaultMaxInteractDistance", 2);
	mfCritterInteractDistance = pGame->GetFloat("Critter", "DefaultMaxInteractDistance", 2);

	iXmlDocument *pEffects = pRes->LoadXmlDocument("config/Effects.cfg");
	std::vector<cXmlElement *> vEffectElems = ChildElements(pEffects);
	for (cXmlElement *pElem : vEffectElems)
	{
		cSomaLuxEffect *pEffect = new cSomaLuxEffect();
		pEffect->msName = pElem->GetAttributeString("Name", "");
		pEffect->msScriptName = pEffect->msName;
		pEffect->mlId = pElem->GetAttributeInt("ID", -1);
		if (pEffect->LoadScript(mpRuntime, pElem->GetAttributeString("ScriptFile", ""), pElem->GetAttributeString("ScriptClass", ""), "cLuxEffect"))
			mvEffects.push_back(pEffect);
		else
			delete pEffect;
	}
	if (pEffects)
		pRes->DestroyXmlDocument(pEffects);

	iXmlDocument *pModules = pRes->LoadXmlDocument("config/Modules.cfg");
	std::vector<cXmlElement *> vModuleElems = ChildElements(pModules);
	for (cXmlElement *pElem : vModuleElems)
	{
		if (pElem->GetValue() != "Module")
			continue;
		cSomaLuxModule *pModule = new cSomaLuxModule();
		pModule->msName = pElem->GetAttributeString("Name", "");
		pModule->msScriptName = pModule->msName;
		pModule->mlId = pElem->GetAttributeInt("ID", -1);
		if (pModule->LoadScript(mpRuntime, pElem->GetAttributeString("ScriptFile", ""), pElem->GetAttributeString("ScriptClass", ""), "cLuxUserModule"))
			mvModules.push_back(pModule);
		else
			delete pModule;
	}
	if (pModules)
		pRes->DestroyXmlDocument(pModules);

	iXmlDocument *pTypes = pRes->LoadXmlDocument("config/EntityTypes.cfg");
	for (cXmlElement *pGroup : ChildElements(pTypes))
		for (cXmlElement *pType : ChildElements(pGroup))
		{
			cEntityScript es;
			es.msFile = pType->GetAttributeString("ScriptFile", "");
			es.msClass = pType->GetAttributeString("ScriptClass", "");
			mmapEntityScripts[pGroup->GetValue() + ":" + pType->GetAttributeString("Name", "")] = es;
		}
	if (pTypes)
		pRes->DestroyXmlDocument(pTypes);

	Log("SOMA script: %d handlers, %d user modules, %d effects\n", (int)mvHandlers.size(), (int)mvModules.size(), (int)mvEffects.size());

	if (cSomaLuxInputHandler::Get())
	{
		cSomaLuxInputHandler::Get()->LoadUserConfig();
		cSomaLuxInputHandler::Get()->LoadScript();
	}
	ReloadUserConfig();
	ForEach([](cSomaLuxScriptable *p) { p->OnMessage("void OnStart()"); });
	// cLuxBase::Reset before a new game
	ForEach([](cSomaLuxScriptable *p) { p->OnMessage("void Reset()"); });
}

void cSomaLuxGame::ReloadUserConfig()
{
	ForEach([](cSomaLuxScriptable *p) { p->OnMessage("void LoadUserConfig()"); });
}

void cSomaLuxGame::ResetScriptables()
{
	ForEach([](cSomaLuxScriptable *p) { p->OnMessage("void Reset()"); });
}

void SomaUpdateCameraTextures();
void SomaDestroyCameraTextures();

void cSomaLuxGame::Update(float afTimeStep, bool abPaused)
{
	// Read once: iMouse::GetRelPosition resets the motion
	mvMouseRel = gpSomaBase->mpEngine->GetInput()->GetMouse()->GetRelPosition();
	if (cSomaLuxInputHandler::Get())
		cSomaLuxInputHandler::Get()->LatchActions();
	if (cSomaLuxInputHandler::Get())
		cSomaLuxInputHandler::Get()->UpdateInput(afTimeStep, mbGameInput);
	if (abPaused)
	{
		UpdateGui(afTimeStep);
		return;
	}
	ForEach([afTimeStep](cSomaLuxScriptable *p) { p->OnUpdate(afTimeStep); }, true);
	if (cSomaLuxVoiceHandler::Get())
		cSomaLuxVoiceHandler::Get()->UpdateVoices(afTimeStep);
	cSomaLuxDialogHandler::Get()->Update(afTimeStep);
	UpdateGui(afTimeStep);
}

void cSomaLuxGame::PostUpdate(float afTimeStep)
{
	ForEach([afTimeStep](cSomaLuxScriptable *p) { p->OnPostUpdate(afTimeStep); }, true);
	ForEach([afTimeStep](cSomaLuxScriptable *p) { p->OnVariableUpdate(afTimeStep); }, true);
	SomaUpdateCameraTextures();
}

cSomaImGui *SomaHudImGui();

void cSomaLuxGame::Draw(float afFrameTime)
{
	ForEach([afFrameTime](cSomaLuxScriptable *p) { p->CallWithFloat("void OnDraw(float afFrameTime)", afFrameTime); }, true);
}

// cLuxGuiHandler::Update: default input to the focused ImGui, then the HUD's OnGui pass
void cSomaLuxGame::UpdateGui(float afTimeStep)
{
	cSomaImGui::UpdateFocusHistory();
	// cLuxGuiHandler::SetImGuiInputFocus: absolute pointer only while a 2D ImGui has input; in-world screens move their own cursor
	static int lRelative = -1;
	cSomaImGui *pInputFocus = cSomaImGui::GetInputFocus();
	int lWantRelative = pInputFocus == NULL || pInputFocus->GetSet()->Is3D();
	if (lWantRelative != lRelative)
	{
		lRelative = lWantRelative;
		gpSomaBase->mpEngine->GetInput()->GetLowLevel()->RelativeMouse(lRelative);
		gpSomaBase->mpEngine->GetInput()->GetLowLevel()->LockInput(true);
	}
	if (cSomaImGui *pFocus = cSomaImGui::GetInputFocus())
	{
		if (cSomaLuxHandler *pGui = GetHandler("GuiHandler"))
			pGui->CallWithObject("void UpdateDefaultInput(cImGui @apImGui)", pFocus);
		iMouse *pMouse = gpSomaBase->mpEngine->GetInput()->GetMouse();
		// Pointer starts centred until the OS reports motion, as in the official game
		static bool bMouseMoved = false;
		bMouseMoved |= mvMouseRel.x != 0 || mvMouseRel.y != 0;
		cVector2l vPos = bMouseMoved ? pMouse->GetAbsPosition() : gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeInt() / 2;
		pFocus->SendMousePosition(vPos, mvMouseRel);
	}
	cSomaImGui *pHud = SomaHudImGui();
	cSomaImGui::SetCurrent(pHud);
	pHud->Begin(afTimeStep);
	ForEach([afTimeStep](cSomaLuxScriptable *p) { p->OnGui(afTimeStep); }, true);
	if (cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent())
		if (pMap->GetScript())
			cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void OnGui(float afTimeStep)", [=](asIScriptContext *c) { c->SetArgFloat(0, afTimeStep); });
	pHud->End();
	cSomaImGui::SetCurrent(NULL);
}

void cSomaLuxGame::BroadcastAction(int alAction, bool abPressed)
{
	// Paused: only modules (the menu) hear input
	if (gpSomaBase->mbScriptGamePaused)
	{
		for (cSomaLuxModule *p : mvModules)
			p->OnAction(alAction, abPressed);
		return;
	}
	ForEach([=](cSomaLuxScriptable *p) { p->OnAction(alAction, abPressed); }, true);
	if (cSomaLuxMap::GetCurrent())
		cSomaLuxMap::GetCurrent()->OnAction(alAction, abPressed);
}

void cSomaLuxGame::BroadcastAnalog(int alAnalogId, const cVector3f &avAmount)
{
	if (gpSomaBase->mbScriptGamePaused)
		return;
	ForEach([&](cSomaLuxScriptable *p) { p->OnAnalogInput(alAnalogId, avAmount); }, true);
}

void cSomaLuxGame::PreloadData(cSomaLuxMap *apMap)
{
	ForEach([apMap](cSomaLuxScriptable *p) { p->OnMapMessage("void PreloadData(cLuxMap @apMap)", apMap); });
}

void cSomaLuxGame::EnterMap(cSomaLuxMap *apMap)
{
	ForEach([apMap](cSomaLuxScriptable *p) { p->OnMapMessage("void CreateWorldEntities(cLuxMap @apMap)", apMap); });
	ForEach([apMap](cSomaLuxScriptable *p) { p->OnMapMessage("void OnMapEnter(cLuxMap @apMap)", apMap); });
}

// Saved module state (e.g. an open pause menu) must not outlive the load
void cSomaLuxGame::ModulesMapEnter(cSomaLuxMap *apMap)
{
	for (cSomaLuxModule *p : mvModules)
		p->OnMapMessage("void OnMapEnter(cLuxMap @apMap)", apMap);
}

void cSomaLuxGame::LeaveMap(cSomaLuxMap *apMap)
{
	cSomaLuxDialogHandler::Get()->StopAll();
	SomaDestroyCameraTextures();
	// its Reset() keeps the entity id, so every later datamine fails
	SomaRunGlobalFunc("DatamineHandler", "", "_Global_StopDatamining");
	ForEach([apMap](cSomaLuxScriptable *p) { p->OnMapMessage("void OnMapLeave(cLuxMap @apMap)", apMap); });
	ForEach([apMap](cSomaLuxScriptable *p) { p->OnMapMessage("void DestroyWorldEntities(cLuxMap @apMap)", apMap); });
}

const cSomaLuxGame::cEntityScript *cSomaLuxGame::GetEntityScript(const tString &asGroup, const tString &asType)
{
	std::map<tString, cEntityScript>::iterator it = mmapEntityScripts.find(asGroup + ":" + asType);
	return it == mmapEntityScripts.end() ? NULL : &it->second;
}

cSomaLuxModule *cSomaLuxGame::GetModule(int alId)
{
	for (cSomaLuxModule *p : mvModules)
		if (p->mlId == alId)
			return p;
	return NULL;
}

cSomaLuxModule *cSomaLuxGame::GetModule(const tString &asName)
{
	for (cSomaLuxModule *p : mvModules)
		if (p->msName == asName)
			return p;
	return NULL;
}

cSomaLuxEffect *cSomaLuxGame::GetEffect(int alId)
{
	for (cSomaLuxEffect *p : mvEffects)
		if (p->mlId == alId)
			return p;
	return NULL;
}

cSomaLuxHandler *cSomaLuxGame::GetHandler(const tString &asName)
{
	for (cSomaLuxHandler *p : mvHandlers)
		if (p->msName == asName)
			return p;
	return NULL;
}

// Script objects returned as interface handles need a reference for the caller
static void ReturnScript(asIScriptGeneric *g, cSomaLuxScriptable *apObj)
{
	g->SetReturnObject(apObj ? apObj->GetScript() : NULL);
}

static void GetUserModuleFromID(asIScriptGeneric *g)
{
	ReturnScript(g, cSomaLuxGame::Get() ? cSomaLuxGame::Get()->GetModule(*(int *)g->GetAddressOfArg(0)) : NULL);
}

static void GetUserModuleFromName(asIScriptGeneric *g)
{
	ReturnScript(g, cSomaLuxGame::Get() ? cSomaLuxGame::Get()->GetModule(*(tString *)g->GetArgObject(0)) : NULL);
}

static void EffectHandlerGetEffect(asIScriptGeneric *g)
{
	ReturnScript(g, cSomaLuxGame::Get() ? cSomaLuxGame::Get()->GetEffect(*(int *)g->GetAddressOfArg(0)) : NULL);
}

// cLuxEffectHandler has no state of its own yet; any non-null pointer identifies it
static int gEffectHandlerTag;

template <int N> static cSomaLuxScriptable *HandlerByName()
{
	static const char *vNames[] = {"InputHandler", "EventDatabaseHandler", "GuiHandler", "AchievementHandler",
								   "HeroStatsHandler", "RichPresenceHandler", "Player"};
	return cSomaLuxGame::Get() ? cSomaLuxGame::Get()->GetHandler(vNames[N]) : NULL;
}

void cSomaLuxGame::RegisterNatives(asIScriptEngine *e)
{
	int r;
	r = e->RegisterGlobalFunction("iScrUserModule_Interface@ cLux_GetUserModuleFromID(int alID)", asFUNCTION(GetUserModuleFromID), asCALL_GENERIC); assert(r >= 0);
	r = e->RegisterGlobalFunction("iScrUserModule_Interface@ cLux_GetUserModuleFromName(const tString&in asName)", asFUNCTION(GetUserModuleFromName), asCALL_GENERIC); assert(r >= 0);
	r = e->RegisterObjectMethod("cLuxEffectHandler", "iScrEffect_Interface@ GetEffect(int alId)", asFUNCTION(EffectHandlerGetEffect), asCALL_GENERIC); assert(r >= 0);
	(void)r;
	SOMA_FUNC(e, "void cLux_GetTextCatAndEntryFromString(const tString&in asData, tString &out asOutCat, tString &out asOutEntry)",
			  +[](const tString &d, tString &c, tString &n) {
				  tStringVec v;
				  cString::GetStringVec(d, v);
				  c = v.size() > 0 ? v[0] : "";
				  n = v.size() > 1 ? v[1] : "";
			  });
	SOMA_FUNC(e, "cLuxEffectHandler@ cLux_GetEffectHandler()", +[]() { return (void *)&gEffectHandlerTag; });
	SOMA_FUNC(e, "cLuxInputHandler@ cLux_GetInputHandler()", +[]() { return (void *)HandlerByName<0>(); });
	SOMA_FUNC(e, "cLuxEventDatabaseHandler@ cLux_GetEventDatabaseHandler()", +[]() { return (void *)HandlerByName<1>(); });
	SOMA_FUNC(e, "cLuxGuiHandler@ cLux_GetGuiHandler()", +[]() { return (void *)HandlerByName<2>(); });
	SOMA_METHOD(e, "cLuxGuiHandler", "void SetGameHudInputFocus(bool abX)", +[](void *, bool b) {
		cSomaImGui::mbGameHudFocus = b;
		SomaHudImGui()->mbShowMouse = b;
	});
	SOMA_METHOD(e, "cLuxGuiHandler", "bool GetGameHudInputFocus()", +[](void *) { return cSomaImGui::mbGameHudFocus; });
	SOMA_FUNC(e, "iLuxAchievementHandler@ cLux_GetAchievementHandler()", +[]() { return (void *)HandlerByName<3>(); });
	SOMA_FUNC(e, "iLuxHeroStatsHandler@ cLux_GetHeroStatsHandler()", +[]() { return (void *)HandlerByName<4>(); });
	SOMA_FUNC(e, "iLuxRichPresenceHandler@ cLux_GetRichPresenceHandler()", +[]() { return (void *)HandlerByName<5>(); });
	SOMA_FUNC(e, "cLuxPlayer@ cLux_GetPlayer()", +[]() { return (void *)HandlerByName<6>(); });
	cSomaLuxPlayer::RegisterNatives(e);
	cSomaLuxVoiceHandler::RegisterNatives(e);
	cSomaLuxDialogHandler::RegisterNatives(e);
	cSomaLuxInputHandler::RegisterNatives(e);

	const char *vTimerTypes[] = {"cLuxUserModule", "cLuxEffect", "cLuxPlayer", "cLuxInputHandler", "cLuxGuiHandler",
								 "cLuxEventDatabaseHandler"};
	for (const char *pType : vTimerTypes)
	{
		asITypeInfo *pInfo = e->GetTypeInfoByName(pType);
		if (pInfo && pInfo->GetMethodByName("Timer_Add"))
			continue;
		if (pInfo)
			cSomaLuxScriptable::RegisterTimerNatives(e, pType);
	}

	cSomaLuxModule module;
	e->RegisterObjectProperty("cLuxUserModule", "int mlId", (int)((char *)&module.mlId - (char *)(cSomaLuxScriptable *)&module));
	cSomaLuxEffect effect;
	e->RegisterObjectProperty("cLuxEffect", "int mlId", (int)((char *)&effect.mlId - (char *)(cSomaLuxScriptable *)&effect));

	SOMA_FUNC(e, "cConfigFile@ cLux_GetUserConfig()", +[]() { return gpUserConfig; });
	SOMA_FUNC(e, "float cLux_GetStringDuration(const tWString&in asStr)", SomaStringDuration);
	SOMA_METHOD(e, "iLowLevelGraphics", "void SetBrightness(float afX)", +[](iLowLevelGraphics *g, float x) { g->SetGammaCorrection(x); });
	SOMA_FUNC(e, "iFontData@ cResources_CreateFontData(const tString&in asName)", +[](const tString &s) { return gpSomaBase->mpEngine->GetResources()->GetFontManager()->CreateFontData(s); });
	SOMA_METHOD(e, "cGuiSet", "void DrawFontEx(const tWString &in asText, iFontData @apFont, const cVector3f &in avPos,const cVector2f &in avSize, const cColor&in aColor, eFontAlign aAlign, eGuiMaterial aMaterial)",
				+[](cGuiSet *g, const tWString &t, iFontData *f, const cVector3f &p, const cVector2f &v, const cColor &c, eFontAlign al, eGuiMaterial m) { g->DrawFont(t, f, p, v, c, al, m); });
	SOMA_METHOD(e, "iFontData", "void GetWordWrapRows(float afLength,const cVector2f&in avSize,const tWString&in asString, array<tWString> &inout avRows)",
				+[](iFontData *f, float l, const cVector2f &v, const tWString &t, CScriptArray &rows) {
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

	SOMA_METHOD(e, "cLuxEffect", "void SetActive(bool abX)", +[](cSomaLuxEffect *p, bool b) { p->mbActive = b; });
	SOMA_METHOD(e, "cLuxEffect", "bool IsActive()", +[](cSomaLuxEffect *p) { return p->mbActive; });
}
