#include "SomaLux.h"
#include <type_traits>
#include <cstring>
#include <functional>
#include "SomaBase.h"
#include "SomaLuxGame.h"
#include "SomaLuxEntity.h"
#include "SomaScriptBind.h"
#include "SomaScriptNatives.h"
#include "SomaScriptRuntime.h"

cSomaLuxMap *cSomaLuxMap::mpCurrent = NULL;

//---------------------------------------

cSomaLuxMap::cSomaLuxMap(cWorld *apWorld, const tString &asFileName)
	: mpWorld(apWorld), msFileName(asFileName), mpRuntime(NULL), mpScript(NULL)
{
	msName = cString::SetFileExt(cString::GetFileName(asFileName), "");
	mvEntities.swap(cSomaLuxEntity::Pending());
	// cLuxMap::LoadFromFile adds the player and camera proxy entities
	cSomaLuxEntity *pPlayer = new cSomaLuxEntity();
	pPlayer->msName = "Player";
	pPlayer->meType = eSomaLuxEntityType_Player;
	mvEntities.push_back(pPlayer);
	cSomaLuxEntity *pCamera = new cSomaLuxEntity();
	pCamera->msName = "Camera";
	mvEntities.push_back(pCamera);
	int lNextId = 1;
	for (cSomaLuxEntity *pEnt : mvEntities)
	{
		if (pEnt->mID == cSomaID())
		{
			pEnt->mID.mA = 0xfd;
			pEnt->mID.mB = lNextId++;
		}
		pEnt->mpMap = this;
		pEnt->msScriptName = pEnt->msName;
		mmapEntities[pEnt->msName] = pEnt;
	}
}

cSomaLuxMap::~cSomaLuxMap()
{
	for (cSomaLuxEntity *pEnt : mvEntities)
		delete pEnt;
	if (mpCurrent == this)
		mpCurrent = NULL;
	if (mpScript)
		mpScript->Release();
}

bool cSomaLuxMap::CreateScript(cSomaScriptRuntime *apRuntime, const tString &asScriptFile)
{
	mpRuntime = apRuntime;
	asIScriptModule *pModule = apRuntime->GetModule(asScriptFile);
	mpScript = apRuntime->CreateObject(pModule, "cScrMap");
	if (mpScript == NULL)
		return false;

	cSomaLuxMap *pThis = this;
	apRuntime->Call(mpScript, "void SetupBaseInterface(cLuxMap @aObj)",
					[&](asIScriptContext *apCtx) { apCtx->SetArgAddress(0, pThis); });

	// Entity script classes (cLuxMap::LoadFromFile -> iLuxEntity::AfterWorldLoad)
	int lScripted = 0;
	for (cSomaLuxEntity *pEnt : mvEntities)
	{
		static const char *vGroups[] = {"PropTypes", "AreaTypes", "LiquidAreaTypes", "LiquidAreaTypes", "CritterTypes", "AgentTypes"};
		if (pEnt->meType >= (int)(sizeof(vGroups) / sizeof(vGroups[0])))
			continue;
		const cSomaLuxGame::cEntityScript *pScript = cSomaLuxGame::Get() ? cSomaLuxGame::Get()->GetEntityScript(vGroups[pEnt->meType], pEnt->msClassName) : NULL;
		if (pScript == NULL || pEnt->LoadScript(apRuntime, pScript->msFile, pScript->msClass, pEnt->GetBaseTypeName()) == false)
			continue;
		++lScripted;
		cWorld *pWorld = mpWorld;
		if (pEnt->meType == eSomaLuxEntityType_Area || pEnt->meType == eSomaLuxEntityType_LiquidArea)
			pEnt->Call("void SetupAfterLoad(cWorld @apWorld, cResourceVarsObject @apVars)", [&](asIScriptContext *c) {
				c->SetArgAddress(0, pWorld);
				c->SetArgAddress(1, &pEnt->mInstanceVars);
			});
		else
			pEnt->Call("void SetupAfterLoad(cWorld @apWorld, cResourceVarsObject@ apVars, cResourceVarsObject@ apInstanceVars)", [&](asIScriptContext *c) {
				c->SetArgAddress(0, pWorld);
				c->SetArgAddress(1, &pEnt->mVars);
				c->SetArgAddress(2, &pEnt->mInstanceVars);
			});
	}
	Log("SOMA script: %d map entities, %d with a script class\n", (int)mvEntities.size(), lScripted);
	for (cSomaLuxEntity *pEnt : mvEntities)
		if (pEnt->meType == eSomaLuxEntityType_Area && pEnt->GetScript())
			pEnt->CreateAreaBody(mpWorld->GetPhysicsWorld());

	apRuntime->Call(mpScript, "void PreloadData()");
	return true;
}

cSomaLuxEntity *cSomaLuxMap::GetEntity(const tString &asName)
{
	std::map<tString, cSomaLuxEntity *>::iterator it = mmapEntities.find(asName);
	if (it != mmapEntities.end())
		return it->second;
	if (asName.find('*') != tString::npos)
		for (cSomaLuxEntity *pEnt : mvEntities)
			if (SomaWildcardMatch(asName, pEnt->msName))
				return pEnt;
	return NULL;
}

cSomaLuxEntity *cSomaLuxMap::GetEntity(const cSomaID &aID)
{
	for (cSomaLuxEntity *pEnt : mvEntities)
		if (pEnt->mID == aID)
			return pEnt;
	return NULL;
}

void cSomaLuxMap::OnEnter(bool abFirstTime)
{
	if (mpScript == NULL)
		return;
	mpRuntime->Call(mpScript, "void Setup()");
	if (abFirstTime)
		mpRuntime->Call(mpScript, "void OnStart()");
	mpRuntime->Call(mpScript, "void OnEnter()");
}

void cSomaLuxMap::OnLeave()
{
	if (mpScript)
		mpRuntime->Call(mpScript, "void OnLeave()");
}

void cSomaLuxMap::Update(float afTimeStep)
{
	if (mpScript == NULL)
		return;

	// Collect due timers first: callbacks may add or remove timers
	std::vector<cSomaLuxTimer> vDue;
	for (size_t i = 0; i < mvTimers.size();)
	{
		if (mvTimers[i].mbPaused == false)
			mvTimers[i].mfTime -= afTimeStep;
		if (mvTimers[i].mfTime <= 0 && mvTimers[i].mbPaused == false)
		{
			vDue.push_back(mvTimers[i]);
			mvTimers.erase(mvTimers.begin() + i);
		}
		else
			++i;
	}
	for (size_t i = 0; i < vDue.size(); ++i)
		mpRuntime->CallByName(mpScript, vDue[i].msFunction, vDue[i].msName);

	float fStep = afTimeStep;
	mpRuntime->Call(mpScript, "void Update(float afTimeStep)", [&](asIScriptContext *apCtx) { apCtx->SetArgFloat(0, fStep); });

	for (cSomaLuxEntity *pEnt : mvEntities)
	{
		if (pEnt->GetScript() == NULL || pEnt->mbActive == false)
			continue;
		pEnt->UpdateTimers(afTimeStep);
		pEnt->CallWithFloat("void Update(float afTimeStep)", afTimeStep);
	}

	UpdateCollideCallbacks();
}

void cSomaLuxMap::OnAction(int alAction, bool abPressed)
{
	if (mpScript)
		mpRuntime->Call(mpScript, "void OnAction(int alAction, bool abPressed)", [=](asIScriptContext *c) {
			c->SetArgDWord(0, alAction);
			c->SetArgByte(1, abPressed);
		});
}

// cLuxCollideCallbackContainer::CheckCollisionCallback: 1 on enter, -1 on leave, a false return removes the callback
void cSomaLuxMap::UpdateCollideCallbacks()
{
	std::set<std::tuple<cSomaLuxEntity *, cSomaLuxEntity *, tString>> setNow;
	for (size_t e = 0; e < mvEntities.size(); ++e)
	{
		cSomaLuxEntity *pParent = mvEntities[e];
		if (pParent->mvCollideCallbacks.empty() || pParent->mbActive == false)
			continue;
		for (size_t i = 0; i < pParent->mvCollideCallbacks.size(); ++i)
		{
			cSomaLuxEntity::cCollideCallback cb = pParent->mvCollideCallbacks[i];
			bool bRemove = false;
			for (size_t c = 0; c < mvEntities.size() && bRemove == false; ++c)
			{
				cSomaLuxEntity *pChild = mvEntities[c];
				if (pChild == pParent || pChild->mbActive == false || SomaWildcardMatch(cb.msChild, pChild->msName) == false)
					continue;
				auto key = std::make_tuple(pParent, pChild, cb.msFunc);
				bool bWas = msetColliding.count(key) > 0;
				bool bNow = SomaEntitiesCollide(pParent, pChild);
				if (bNow)
					setNow.insert(key);
				if (bNow == bWas)
					continue;
				int lState = bNow ? 1 : -1;
				tString sDecl = "bool " + cb.msFunc + "(const tString &in, const tString &in, int)";
				auto args = [&](asIScriptContext *ctx) {
					ctx->SetArgObject(0, &pParent->msName);
					ctx->SetArgObject(1, &pChild->msName);
					ctx->SetArgDWord(2, lState);
				};
				bool bKeep = true;
				auto result = [&](asIScriptContext *ctx) { bKeep = ctx->GetReturnByte() != 0; };
				if (mpScript && mpScript->GetObjectType()->GetMethodByDecl(sDecl.c_str()))
					mpRuntime->Call(mpScript, sDecl, args, result);
				else if (pParent->HasMethod(sDecl))
					bKeep = pParent->CallBool(sDecl, args, true);
				else
					Warning("SOMA script: collide callback '%s' not found\n", cb.msFunc.c_str());
				bRemove = bKeep == false;
			}
			if (bRemove)
			{
				for (size_t k = 0; k < pParent->mvCollideCallbacks.size(); ++k)
					if (pParent->mvCollideCallbacks[k].msChild == cb.msChild && pParent->mvCollideCallbacks[k].msFunc == cb.msFunc)
					{
						pParent->mvCollideCallbacks.erase(pParent->mvCollideCallbacks.begin() + k);
						break;
					}
				--i;
			}
		}
	}
	msetColliding.swap(setNow);
}

void cSomaLuxMap::AddTimer(const tString &asName, float afTime, const tString &asFunction)
{
	cSomaLuxTimer timer;
	timer.msName = asName;
	timer.msFunction = asFunction;
	timer.mfTime = afTime;
	timer.mbPaused = false;
	timer.mfUserFloat = 0;
	timer.mlUserInt = 0;
	mvTimers.push_back(timer);
}

void cSomaLuxMap::RemoveTimer(const tString &asName)
{
	for (size_t i = 0; i < mvTimers.size();)
	{
		if (mvTimers[i].msName == asName)
			mvTimers.erase(mvTimers.begin() + i);
		else
			++i;
	}
}

cSomaLuxTimer *cSomaLuxMap::GetTimer(const tString &asName)
{
	for (size_t i = 0; i < mvTimers.size(); ++i)
		if (mvTimers[i].msName == asName)
			return &mvTimers[i];
	return NULL;
}

//---------------------------------------

void cSomaLuxUpdater::Update(float afTimeStep)
{
	bool bMap = cSomaLuxMap::GetCurrent() != NULL;
	if (bMap && gpSomaBase->UsesScriptPlayer())
	{
		bool bEscape = gpSomaBase->mpEngine->GetInput()->GetKeyboard()->KeyIsDown(eKey_Escape);
		if (bEscape && mbEscapeDown == false)
			gpSomaBase->SetGameplayPaused(gpSomaBase->IsGameplayPaused() == false);
		mbEscapeDown = bEscape;
	}
	if (gpSomaBase->IsGameplayPaused())
		return;
	if (cSomaLuxGame::Get())
	{
		cSomaLuxGame::Get()->mbGameInput = bMap && gpSomaBase->UsesScriptPlayer() && gpSomaBase->UsesRealPlayer();
		cSomaLuxGame::Get()->Update(afTimeStep);
	}
	if (cSomaLuxMap::GetCurrent())
		cSomaLuxMap::GetCurrent()->Update(afTimeStep);
}

//---------------------------------------
// Natives

static cSomaLuxMap *CurrentMap() { return cSomaLuxMap::GetCurrent(); }

// Class filters name the script class (cScrAreaCameraAnimationNode) or the entity type
static bool SomaEntityIsClass(cSomaLuxEntity *apEnt, const tString &asClass)
{
	if (asClass.empty() || asClass == apEnt->msClassName)
		return true;
	return apEnt->GetScript() && asClass == apEnt->GetScript()->GetObjectType()->GetName();
}

//---------------------------------------
// iScriptUserClassInterface: ScriptPrepare(decl), SetArg*, ScriptExecute, GetReturn* on a script-backed object

struct cSomaPreparedCall
{
	asIScriptObject *mpObj = NULL;
	asIScriptFunction *mpFunc = NULL;
	std::vector<std::function<void(asIScriptContext *)>> mvArgs;
	std::vector<tString> mvStrings;
	std::vector<cVector3f> mvVecs;
	asQWORD mlRet = 0;
	float mfRet = 0;
	tString msRet;
};
static std::map<void *, cSomaPreparedCall> gmapPrepared;

static asIScriptObject *ScriptOf(asIScriptGeneric *g)
{
	void *pObj = g->GetObject();
	if (strcmp(g->GetFunction()->GetObjectType()->GetName(), "cLuxMap") == 0)
		return ((cSomaLuxMap *)pObj)->GetScript();
	return SomaScriptIsDummy(pObj) ? NULL : ((cSomaLuxScriptable *)pObj)->GetScript();
}

static void ScriptPrepare(asIScriptGeneric *g)
{
	cSomaPreparedCall &call = gmapPrepared[g->GetObject()];
	call = cSomaPreparedCall();
	call.mvStrings.reserve(16);
	call.mvVecs.reserve(16);
	call.mpObj = ScriptOf(g);
	const tString &sDecl = *(tString *)g->GetArgObject(0);
	if (call.mpObj)
	{
		call.mpFunc = call.mpObj->GetObjectType()->GetMethodByDecl(sDecl.c_str());
		if (call.mpFunc == NULL)
			call.mpFunc = call.mpObj->GetObjectType()->GetMethodByName(sDecl.c_str());
	}
	*(bool *)g->GetAddressOfReturnLocation() = call.mpFunc != NULL;
}

static void ScriptExecute(asIScriptGeneric *g)
{
	cSomaPreparedCall &call = gmapPrepared[g->GetObject()];
	bool bOk = false;
	if (call.mpFunc)
	{
		asIScriptEngine *pEngine = g->GetEngine();
		asIScriptContext *pCtx = pEngine->RequestContext();
		pCtx->Prepare(call.mpFunc);
		pCtx->SetObject(call.mpObj);
		for (auto &f : call.mvArgs)
			f(pCtx);
		bOk = pCtx->Execute() == asEXECUTION_FINISHED;
		if (bOk == false && pCtx->GetState() == asEXECUTION_EXCEPTION)
		{
			const char *pSection = NULL;
			int lLine = pCtx->GetExceptionLineNumber(NULL, &pSection);
			Error("SOMA script exception in %s: '%s' at %s:%d\n", call.mpFunc->GetDeclaration(), pCtx->GetExceptionString(), pSection ? pSection : "?", lLine);
		}
		if (bOk)
		{
			int lType = call.mpFunc->GetReturnTypeId();
			if (lType == asTYPEID_FLOAT)
				call.mfRet = pCtx->GetReturnFloat();
			else if (lType == asTYPEID_BOOL || lType == asTYPEID_INT8 || lType == asTYPEID_UINT8)
				call.mlRet = pCtx->GetReturnByte();
			else if (lType == asTYPEID_INT32 || lType == asTYPEID_UINT32)
				call.mlRet = pCtx->GetReturnDWord();
			else if (lType > asTYPEID_DOUBLE && pCtx->GetReturnObject())
				call.msRet = *(tString *)pCtx->GetReturnObject();
		}
		pEngine->ReturnContext(pCtx);
	}
	*(bool *)g->GetAddressOfReturnLocation() = bOk;
}

template <class T> static void SetArgValue(asIScriptGeneric *g)
{
	cSomaPreparedCall &call = gmapPrepared[g->GetObject()];
	int lArg = *(int *)g->GetAddressOfArg(0);
	T x = *(T *)g->GetAddressOfArg(1);
	call.mvArgs.push_back([lArg, x](asIScriptContext *c) {
		if constexpr (std::is_same<T, float>::value)
			c->SetArgFloat(lArg, x);
		else if constexpr (std::is_same<T, bool>::value)
			c->SetArgByte(lArg, x);
		else
			c->SetArgDWord(lArg, x);
	});
}

static void SetArgString(asIScriptGeneric *g)
{
	cSomaPreparedCall &call = gmapPrepared[g->GetObject()];
	int lArg = *(int *)g->GetAddressOfArg(0);
	call.mvStrings.push_back(*(tString *)g->GetArgObject(1));
	tString *pStr = &call.mvStrings.back();
	call.mvArgs.push_back([lArg, pStr](asIScriptContext *c) { c->SetArgObject(lArg, pStr); });
}

static void SetArgVector3f(asIScriptGeneric *g)
{
	cSomaPreparedCall &call = gmapPrepared[g->GetObject()];
	int lArg = *(int *)g->GetAddressOfArg(0);
	call.mvVecs.push_back(*(cVector3f *)g->GetArgObject(1));
	cVector3f *pVec = &call.mvVecs.back();
	call.mvArgs.push_back([lArg, pVec](asIScriptContext *c) { c->SetArgObject(lArg, pVec); });
}

void RegisterSomaScriptCallNatives(asIScriptEngine *e, const char *apType)
{
	auto reg = [&](const char *apDecl, asGENFUNC_t apFunc) {
		asITypeInfo *pType = e->GetTypeInfoByName(apType);
		if (pType && pType->GetMethodByDecl(apDecl) == NULL)
			e->RegisterObjectMethod(apType, apDecl, asFUNCTION(apFunc), asCALL_GENERIC);
	};
	reg("bool ScriptPrepare(const tString&in asMethod)", ScriptPrepare);
	reg("bool ScriptPrepareFast(const tString&in asMethod, int alId)", ScriptPrepare);
	reg("bool ScriptExecute()", ScriptExecute);
	reg("void SetArgBool(int alArgNum, bool abVal)", SetArgValue<bool>);
	reg("void SetArgInt(int alArg, int alX)", SetArgValue<int>);
	reg("void SetArgFloat(int alArg, float afX)", SetArgValue<float>);
	reg("void SetArgString(int alArg, const tString &in asStr)", SetArgString);
	reg("void SetArgVector3f(int alArg, const cVector3f &in avX)", SetArgVector3f);
	reg("bool GetReturnBool()", +[](asIScriptGeneric *g) { *(bool *)g->GetAddressOfReturnLocation() = gmapPrepared[g->GetObject()].mlRet != 0; });
	reg("int GetReturnInt()", +[](asIScriptGeneric *g) { *(int *)g->GetAddressOfReturnLocation() = (int)gmapPrepared[g->GetObject()].mlRet; });
	reg("float GetReturnFloat()", +[](asIScriptGeneric *g) { *(float *)g->GetAddressOfReturnLocation() = gmapPrepared[g->GetObject()].mfRet; });
	reg("tString GetReturnString()", +[](asIScriptGeneric *g) { g->SetReturnObject(&gmapPrepared[g->GetObject()].msRet); });
}

void RegisterSomaScriptLuxNatives(asIScriptEngine *e)
{
	for (const char *pType : {"cLuxMap", "iLuxEntity", "cLuxProp", "cLuxArea", "cLuxAgent", "cLuxCritter", "cLuxLiquidArea"})
		RegisterSomaScriptCallNatives(e, pType);

	SOMA_FUNC(e, "cLuxMap@ cLux_GetCurrentMap()", +[]() { return CurrentMap(); });

	const char *M = "cLuxMap";
	SOMA_METHOD(e, M, "cWorld@ GetWorld()", +[](cSomaLuxMap &m) { return m.GetWorld(); });
	SOMA_METHOD(e, M, "iPhysicsWorld@ GetPhysicsWorld()", +[](cSomaLuxMap &m) { return m.GetWorld()->GetPhysicsWorld(); });
	SOMA_METHOD(e, M, "iLuxEntity @GetEntityByName(const tString&in asName, eLuxEntityType aType=eLuxEntityType_LastEnum, const tString&in asClassName=\"\")",
				+[](cSomaLuxMap &m, const tString &n, int t, const tString &c) {
					cSomaLuxEntity *p = m.GetEntity(n);
					return p && (t == 7 || p->meType == t) && SomaEntityIsClass(p, c) ? p : (cSomaLuxEntity *)NULL;
				});
	SOMA_METHOD(e, M, "iLuxEntity @GetEntityByID(tID alID, eLuxEntityType aType=eLuxEntityType_LastEnum, const tString&in asClassName=\"\")",
				+[](cSomaLuxMap &m, cSomaID id, int t, const tString &c) {
					cSomaLuxEntity *p = m.GetEntity(id);
					return p && (t == 7 || p->meType == t) && SomaEntityIsClass(p, c) ? p : (cSomaLuxEntity *)NULL;
				});
	SOMA_METHOD(e, M, "tID GetEntityIDByName(const tString&in asName, eLuxEntityType aType=eLuxEntityType_LastEnum, const tString&in asClassName=\"\")",
				+[](cSomaLuxMap &m, const tString &n, int, const tString &) { cSomaLuxEntity *p = m.GetEntity(n); return p ? p->mID : cSomaID(); });
	SOMA_METHOD(e, M, "bool EntityExists(iLuxEntity @apEntity)", +[](cSomaLuxMap &m, cSomaLuxEntity *p) {
		for (cSomaLuxEntity *q : m.GetEntities())
			if (q == p)
				return true;
		return false;
	});
	SOMA_METHOD(e, M, "const tString& GetName()", +[](cSomaLuxMap &m) -> const tString & { return m.GetName(); });
	SOMA_METHOD(e, M, "float GetMaxInteractDistance()", +[](cSomaLuxMap &m) { return m.mfMaxInteractDistance; });
	SOMA_METHOD(e, M, "void SetMaxInteractDistance(float afX)", +[](cSomaLuxMap &m, float f) { m.mfMaxInteractDistance = f; });
	SOMA_METHOD(e, M, "const tString& GetFileName()", +[](cSomaLuxMap &m) -> const tString & { return m.GetFileName(); });
	SOMA_METHOD(e, M, "void SetDisplayNameEntry(const tString&in asEntry)", +[](cSomaLuxMap &m, const tString &s) { m.msDisplayNameEntry = s; });
	SOMA_METHOD(e, M, "const tString& GetDisplayNameEntry()", +[](cSomaLuxMap &m) -> const tString & { return m.msDisplayNameEntry; });
	SOMA_METHOD(e, M, "void AddTimer(const tString&in asName, float afTime, const tString&in asFunction)",
				+[](cSomaLuxMap &m, const tString &n, float t, const tString &f) { m.AddTimer(n, t, f); });
	SOMA_METHOD(e, M, "void RemoveTimer(const tString&in asName)", +[](cSomaLuxMap &m, const tString &n) { m.RemoveTimer(n); });
	SOMA_METHOD(e, M, "float GetTimerTime(const tString&in asName)",
				+[](cSomaLuxMap &m, const tString &n) { cSomaLuxTimer *t = m.GetTimer(n); return t ? t->mfTime : 0.0f; });
	SOMA_METHOD(e, M, "void SetTimerPaused(const tString&in asName, bool abX)",
				+[](cSomaLuxMap &m, const tString &n, bool b) { if (cSomaLuxTimer *t = m.GetTimer(n)) t->mbPaused = b; });
	SOMA_METHOD(e, M, "void SetTimerUserVarFloat(const tString&in asName, float afX)",
				+[](cSomaLuxMap &m, const tString &n, float f) { if (cSomaLuxTimer *t = m.GetTimer(n)) t->mfUserFloat = f; });
	SOMA_METHOD(e, M, "void SetTimerUserVarInt(const tString&in asName, int alX)",
				+[](cSomaLuxMap &m, const tString &n, int l) { if (cSomaLuxTimer *t = m.GetTimer(n)) t->mlUserInt = l; });
	SOMA_METHOD(e, M, "void SetTimerUserVarString(const tString&in asName, const tString&in asX)",
				+[](cSomaLuxMap &m, const tString &n, const tString &s) { if (cSomaLuxTimer *t = m.GetTimer(n)) t->msUserString = s; });
	SOMA_METHOD(e, M, "float GetTimerUserVarFloat(const tString&in asName)",
				+[](cSomaLuxMap &m, const tString &n) { cSomaLuxTimer *t = m.GetTimer(n); return t ? t->mfUserFloat : 0.0f; });
	SOMA_METHOD(e, M, "int GetTimerUserVarInt(const tString&in asName)",
				+[](cSomaLuxMap &m, const tString &n) { cSomaLuxTimer *t = m.GetTimer(n); return t ? t->mlUserInt : 0; });
}
