#include "SomaLuxGame.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxPlayer.h"
#include "SomaLuxVoice.h"
#include "SomaImGui.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"

#include <SDL2/SDL.h>
#include <cstring>

cSomaLuxGame *cSomaLuxGame::mpInstance = NULL;

//---------------------------------------

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

template <class F> void cSomaLuxGame::ForEach(F aFunc)
{
	for (cSomaLuxHandler *p : mvHandlers)
		aFunc(p);
	for (cSomaLuxModule *p : mvModules)
		aFunc(p);
	for (cSomaLuxEffect *p : mvEffects)
		aFunc(p);
}

//---------------------------------------
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

// cLuxConfigHandler::LoadUserConfig, [Screen] part
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
}

static void LoadLanguage()
{
	gsLanguage = cString::SetFileExt(gpUserConfig->GetString("Main", "StartLanguage", "english.lang"), "");
	cResources *pRes = gpSomaBase->mpEngine->GetResources();
	pRes->ClearTranslations();
	pRes->AddLanguageFile("config/base_" + gsLanguage + ".lang", false);
	pRes->AddLanguageFile("config/lang_main/" + gsLanguage + ".lang", false);
}

// cGlobalScriptFuncs::ApplyUserConfig: UpdateGraphicSettings, UpdateSoundSettings, LoadLanguage; never asks for a restart
static bool ApplyUserConfig()
{
	cSomaConfig *pCfg = gpSomaBase->GetConfig();
	SomaReadUserScreenConfig(pCfg);
	SDL_Window *pWindow = SDL_GL_GetCurrentWindow();
	if (pWindow && getenv("OPENHPL_HEADLESS_SOCKET") == NULL)
		SDL_SetWindowFullscreen(pWindow, pCfg->mbFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	if (pWindow && pCfg->mbFullscreen == false && pCfg->mlScreenWidth > 0 && pCfg->mlScreenHeight > 0)
		SDL_SetWindowSize(pWindow, pCfg->mlScreenWidth, pCfg->mlScreenHeight);
	tString sVsync = cString::ToLowerCase(gpUserConfig->GetString("Screen", "Vsync", "true"));
	gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->SetVsyncActive(pCfg->mbVSync, sVsync == "adaptive");
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
	iXmlDocument *pGame = pRes->LoadXmlDocument("config/game.cfg");
	// A cfg with several top-level elements loads as its first one
	cXmlElement *pFiles = pGame ? (pGame->GetValue() == "ScriptFiles" ? pGame : pGame->GetFirstElement("ScriptFiles")) : NULL;
	for (const cHandlerDef &def : vHandlers)
	{
		tString sFile = pFiles ? pFiles->GetAttributeString(def.mpAttr, "") : "";
		if (sFile.empty())
			continue;
		cSomaLuxHandler *pHandler = strcmp(def.mpAttr, "Player") == 0		   ? new cSomaLuxPlayer()
									: strcmp(def.mpAttr, "InputHandler") == 0 ? new cSomaLuxInputHandler()
																			   : new cSomaLuxHandler();
		pHandler->msName = def.mpAttr;
		pHandler->msScriptName = strcmp(def.mpAttr, "Player") == 0 ? "LuxPlayer" : def.mpAttr;
		pHandler->msBaseType = def.mpBase;
		if (pHandler->LoadScript(mpRuntime, sFile, def.mpClass, def.mpBase))
			mvHandlers.push_back(pHandler);
		else
			delete pHandler;
	}
	if (pGame)
	{
		if (cXmlElement *pProp = pGame->GetFirstElement("Prop"))
			mfPropInteractDistance = pProp->GetAttributeFloat("DefaultMaxInteractDistance", 2);
		if (cXmlElement *pCritter = pGame->GetFirstElement("Critter"))
			mfCritterInteractDistance = pCritter->GetAttributeFloat("DefaultMaxInteractDistance", 2);
		pRes->DestroyXmlDocument(pGame);
	}

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
		pModule->msContainer = pElem->GetAttributeString("Container", "Default");
		pModule->mlId = pElem->GetAttributeInt("ID", -1);
		pModule->mbGlobal = pElem->GetAttributeBool("IsGlobal", false);
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
		cSomaLuxInputHandler::Get()->LoadKeyConfig();
	}
	ForEach([](cSomaLuxScriptable *p) { p->OnMessage("void LoadUserConfig()"); });
	ForEach([](cSomaLuxScriptable *p) { p->OnMessage("void OnStart()"); });
	// cLuxBase::Reset before a new game
	ForEach([](cSomaLuxScriptable *p) { p->OnMessage("void Reset()"); });
}

void cSomaLuxGame::ResetScriptables()
{
	ForEach([](cSomaLuxScriptable *p) { p->OnMessage("void Reset()"); });
}

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
	ForEach([afTimeStep](cSomaLuxScriptable *p) { p->OnUpdate(afTimeStep); });
	if (cSomaLuxVoiceHandler::Get())
		cSomaLuxVoiceHandler::Get()->UpdateVoices(afTimeStep);
	cSomaLuxDialogHandler::Get()->Update(afTimeStep);
	UpdateGui(afTimeStep);
	ForEach([afTimeStep](cSomaLuxScriptable *p) { p->OnPostUpdate(afTimeStep); });
	ForEach([afTimeStep](cSomaLuxScriptable *p) { p->OnVariableUpdate(afTimeStep); });
}

cSomaImGui *SomaHudImGui();

void cSomaLuxGame::Draw(float afFrameTime)
{
	ForEach([afFrameTime](cSomaLuxScriptable *p) { p->CallWithFloat("void OnDraw(float afFrameTime)", afFrameTime); });
}

// cLuxGuiHandler::Update: default input to the focused ImGui, then the HUD's OnGui pass
void cSomaLuxGame::UpdateGui(float afTimeStep)
{
	cSomaImGui::UpdateFocusHistory();
	// cLuxGuiHandler::SetImGuiInputFocus: absolute pointer only while an ImGui has input
	static int lRelative = -1;
	int lWantRelative = cSomaImGui::GetInputFocus() == NULL;
	if (lWantRelative != lRelative)
	{
		lRelative = lWantRelative;
		gpSomaBase->mpEngine->GetInput()->GetLowLevel()->RelativeMouse(lRelative);
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
	ForEach([afTimeStep](cSomaLuxScriptable *p) { p->CallWithFloat("void OnGui(float afTimeStep)", afTimeStep); });
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
	ForEach([=](cSomaLuxScriptable *p) { p->OnAction(alAction, abPressed); });
	if (cSomaLuxMap::GetCurrent())
		cSomaLuxMap::GetCurrent()->OnAction(alAction, abPressed);
}

void cSomaLuxGame::BroadcastAnalog(int alAnalogId, const cVector3f &avAmount)
{
	if (gpSomaBase->mbScriptGamePaused)
		return;
	ForEach([&](cSomaLuxScriptable *p) { p->OnAnalogInput(alAnalogId, avAmount); });
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

void cSomaLuxGame::LeaveMap(cSomaLuxMap *apMap)
{
	cSomaLuxDialogHandler::Get()->StopAll();
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

//---------------------------------------
// Natives

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
	SOMA_FUNC(e, "cLuxEffectHandler@ cLux_GetEffectHandler()", +[]() { return (void *)&gEffectHandlerTag; });
	SOMA_FUNC(e, "cLuxInputHandler@ cLux_GetInputHandler()", +[]() { return (void *)HandlerByName<0>(); });
	SOMA_FUNC(e, "cLuxEventDatabaseHandler@ cLux_GetEventDatabaseHandler()", +[]() { return (void *)HandlerByName<1>(); });
	SOMA_FUNC(e, "cLuxGuiHandler@ cLux_GetGuiHandler()", +[]() { return (void *)HandlerByName<2>(); });
	SOMA_METHOD(e, "cLuxGuiHandler", "void SetGameHudInputFocus(bool abX)", +[](void *, bool b) {
		if (b)
			cSomaImGui::SetInputFocus(SomaHudImGui(), true);
		else if (cSomaImGui::GetInputFocus() == SomaHudImGui())
			cSomaImGui::SetInputFocus(NULL, false);
	});
	SOMA_METHOD(e, "cLuxGuiHandler", "bool GetGameHudInputFocus()", +[](void *) { return cSomaImGui::GetInputFocus() == SomaHudImGui(); });
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
	SOMA_FUNC(e, "bool cLux_ApplyUserConfig()", +[]() { return ApplyUserConfig(); });
	SOMA_FUNC(e, "cConfigFile@ cLux_GetKeyConfig()", +[]() { return gpKeyConfig; });
	SOMA_FUNC(e, "cConfigFile@ cLux_GetGameConfig()", +[]() { return gpGameConfig; });
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
