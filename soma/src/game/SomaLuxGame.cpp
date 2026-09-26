#include "SomaLuxGame.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"

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

static void LoadConfigs()
{
	tWString sGame = cPlatform::GetWorkingDir();
	tWString sUserDir = cPlatform::GetSystemSpecialPath(eSystemPath_XDGConfigHome) + _W("open-hpl/");
	cPlatform::CreateFolder(sUserDir);
	sUserDir += _W("soma/");
	cPlatform::CreateFolder(sUserDir);
	gpUserConfig = OpenConfig(sUserDir + _W("user_settings.cfg"), sGame + _W("/config/default_user_settings.cfg"));
	gpKeyConfig = OpenConfig(sUserDir + _W("user_keys.cfg"), sGame + _W("/config/default_user_keys.cfg"));
	gpGameConfig = OpenConfig(_W(""), sGame + _W("/config/game.cfg"));
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
		cSomaLuxHandler *pHandler = new cSomaLuxHandler();
		pHandler->msName = def.mpAttr;
		pHandler->msScriptName = def.mpAttr;
		pHandler->msBaseType = def.mpBase;
		if (pHandler->LoadScript(mpRuntime, sFile, def.mpClass, def.mpBase))
			mvHandlers.push_back(pHandler);
		else
			delete pHandler;
	}
	if (pGame)
		pRes->DestroyXmlDocument(pGame);

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

	ForEach([](cSomaLuxScriptable *p) { p->Call("void LoadUserConfig()"); });
	ForEach([](cSomaLuxScriptable *p) { p->Call("void OnStart()"); });
}

void cSomaLuxGame::Update(float afTimeStep)
{
	ForEach([afTimeStep](cSomaLuxScriptable *p) {
		p->UpdateTimers(afTimeStep);
		p->CallWithFloat("void Update(float afTimeStep)", afTimeStep);
	});
	ForEach([afTimeStep](cSomaLuxScriptable *p) { p->CallWithFloat("void PostUpdate(float afTimeStep)", afTimeStep); });
	ForEach([afTimeStep](cSomaLuxScriptable *p) { p->CallWithFloat("void VariableUpdate(float afDeltaTime)", afTimeStep); });
}

void cSomaLuxGame::PreloadData(cSomaLuxMap *apMap)
{
	ForEach([apMap](cSomaLuxScriptable *p) { p->CallWithObject("void PreloadData(cLuxMap @apMap)", apMap); });
}

void cSomaLuxGame::EnterMap(cSomaLuxMap *apMap)
{
	ForEach([apMap](cSomaLuxScriptable *p) { p->CallWithObject("void CreateWorldEntities(cLuxMap @apMap)", apMap); });
	ForEach([apMap](cSomaLuxScriptable *p) { p->CallWithObject("void OnMapEnter(cLuxMap @apMap)", apMap); });
}

void cSomaLuxGame::LeaveMap(cSomaLuxMap *apMap)
{
	ForEach([apMap](cSomaLuxScriptable *p) { p->CallWithObject("void OnMapLeave(cLuxMap @apMap)", apMap); });
	ForEach([apMap](cSomaLuxScriptable *p) { p->CallWithObject("void DestroyWorldEntities(cLuxMap @apMap)", apMap); });
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
	SOMA_FUNC(e, "iLuxAchievementHandler@ cLux_GetAchievementHandler()", +[]() { return (void *)HandlerByName<3>(); });
	SOMA_FUNC(e, "iLuxHeroStatsHandler@ cLux_GetHeroStatsHandler()", +[]() { return (void *)HandlerByName<4>(); });
	SOMA_FUNC(e, "iLuxRichPresenceHandler@ cLux_GetRichPresenceHandler()", +[]() { return (void *)HandlerByName<5>(); });
	SOMA_FUNC(e, "cLuxPlayer@ cLux_GetPlayer()", +[]() { return (void *)HandlerByName<6>(); });

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
