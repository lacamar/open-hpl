#include "SomaLux.h"
#include "SomaScriptApi.h"
#include <type_traits>
#include <cstring>
#include <functional>
#include <map>
#include "SomaBase.h"
#include "SomaLuxGame.h"
#include "impl/scriptarray.h"
#include "SomaLuxPlayer.h"
#include "SomaLuxVoice.h"
#include "SomaLuxEntity.h"
#include "SomaScriptBind.h"
#include "SomaScriptNatives.h"
#include "SomaScriptRuntime.h"
#include "SomaSave.h"

static bool gbPendingNewGame = false;

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
	pCamera->mbCameraProxy = true;
	mvEntities.push_back(pCamera);
	std::vector<cSomaLuxEntity *> vLoaded;
	vLoaded.swap(mvEntities);
	for (cSomaLuxEntity *pEnt : vLoaded)
		AddEntity(pEnt);
}

cSomaLuxMap::~cSomaLuxMap()
{
	SomaClearObjectIDs();
	for (cSomaLuxEntity *pEnt : mvDestroyed)
		delete pEnt;
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
		lScripted += SetupEntityScript(pEnt) ? 1 : 0;
	Log("SOMA script: %d map entities, %d with a script class\n", (int)mvEntities.size(), lScripted);
	for (cSomaLuxEntity *pEnt : std::vector<cSomaLuxEntity *>(mvEntities))
		pEnt->Call("void OnAfterWorldLoad()");

	apRuntime->Call(mpScript, "void PreloadData()");
	return true;
}

bool cSomaLuxMap::SetupEntityScript(cSomaLuxEntity *apEnt)
{
	apEnt->ApplyInstanceVars();
	static const char *vGroups[] = {"PropTypes", "AreaTypes", "LiquidAreaTypes", "LiquidAreaTypes", "CritterTypes", "AgentTypes"};
	if (apEnt->meType >= (int)(sizeof(vGroups) / sizeof(vGroups[0])))
		return false;
	if (apEnt->meType == eSomaLuxEntityType_Critter && apEnt->mpCritterProps == NULL)
		apEnt->mpCritterProps = SomaNewPropBlock("cLuxCritter");
	const cSomaLuxGame::cEntityScript *pScript = cSomaLuxGame::Get() ? cSomaLuxGame::Get()->GetEntityScript(vGroups[apEnt->meType], apEnt->msClassName) : NULL;
	if (pScript == NULL || apEnt->LoadScript(mpRuntime, pScript->msFile, pScript->msClass, apEnt->GetBaseTypeName()) == false)
		return false;
	cWorld *pWorld = mpWorld;
	if (apEnt->meType == eSomaLuxEntityType_Area || apEnt->meType == eSomaLuxEntityType_LiquidArea)
		apEnt->Call("void SetupAfterLoad(cWorld @apWorld, cResourceVarsObject @apVars)", [&](asIScriptContext *c) {
			c->SetArgAddress(0, pWorld);
			c->SetArgAddress(1, &apEnt->mInstanceVars);
		});
	else
		apEnt->Call("void SetupAfterLoad(cWorld @apWorld, cResourceVarsObject@ apVars, cResourceVarsObject@ apInstanceVars)", [&](asIScriptContext *c) {
			c->SetArgAddress(0, pWorld);
			c->SetArgAddress(1, &apEnt->mVars);
			c->SetArgAddress(2, &apEnt->mInstanceVars);
		});
	if (apEnt->meType == eSomaLuxEntityType_Area)
		apEnt->CreateAreaBody(mpWorld->GetPhysicsWorld());
	return true;
}

void cSomaLuxMap::AddEntity(cSomaLuxEntity *apEnt)
{
	if (apEnt->mID == cSomaID())
	{
		apEnt->mID.mA = 0xfd;
		apEnt->mID.mB = mlNextId++;
	}
	apEnt->mpMap = this;
	apEnt->msScriptName = apEnt->msName;
	mvEntities.push_back(apEnt);
	mmapEntities[apEnt->msName] = apEnt;
}

extern tString gsSomaSpawnName;

cSomaLuxEntity *cSomaLuxMap::CreateEntity(const tString &asName, const tString &asFile, const cMatrixf &a_mtx, const cVector3f &avScale)
{
	gsSomaSpawnName = asName;
	mpWorld->CreateEntity(asName, a_mtx, asFile, -1, true, avScale);
	gsSomaSpawnName = "";
	std::vector<cSomaLuxEntity *> vNew;
	vNew.swap(cSomaLuxEntity::Pending());
	for (cSomaLuxEntity *pEnt : vNew)
	{
		AddEntity(pEnt);
		if (mpRuntime)
		{
			SetupEntityScript(pEnt);
			pEnt->Call("void OnAfterWorldLoad()");
		}
		mpLatestEntity = pEnt;
	}
	return vNew.empty() ? NULL : vNew.back();
}

void cSomaLuxMap::DestroyEntity(cSomaLuxEntity *apEnt)
{
	for (size_t i = 0; i < mvEntities.size(); ++i)
		if (mvEntities[i] == apEnt)
		{
			apEnt->SetActive(false);
			mvEntities.erase(mvEntities.begin() + i);
			if (mmapEntities[apEnt->msName] == apEnt)
				mmapEntities.erase(apEnt->msName);
			if (mpLatestEntity == apEnt)
				mpLatestEntity = NULL;
			// Scripts may still hold the handle this frame
			mvDestroyed.push_back(apEnt);
			return;
		}
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
	if (cSomaLuxVoiceHandler::Get())
		cSomaLuxVoiceHandler::Get()->LoadMapFile(msFileName, msName);
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
	mfTime += afTimeStep;
	for (size_t i = 0; i < vDue.size(); ++i)
	{
		mpFiringTimer = &vDue[i];
		mpRuntime->CallByName(mpScript, vDue[i].msFunction, vDue[i].msName);
		mpFiringTimer = NULL;
	}

	float fStep = afTimeStep;
	mpRuntime->Call(mpScript, "void Update(float afTimeStep)", [&](asIScriptContext *apCtx) { apCtx->SetArgFloat(0, fStep); });

	for (cSomaLuxEntity *pEnt : mvEntities)
	{
		if (pEnt->GetScript() == NULL || pEnt->mbActive == false)
			continue;
		pEnt->UpdateTimers(afTimeStep);
		pEnt->CallWithFloat("void Update(float afTimeStep)", afTimeStep);
	}

	for (size_t i = 0; i < mvEntities.size(); ++i)
	{
		mvEntities[i]->UpdateAnimation(afTimeStep);
		mvEntities[i]->UpdateGui(afTimeStep);
	}
	UpdateLookAtCallbacks(afTimeStep);
	UpdateCollideCallbacks();
}

// iLuxEntity look-at callbacks: 1 when the player starts looking at the entity, -1 when looking away
void cSomaLuxMap::UpdateLookAtCallbacks(float afTimeStep)
{
	cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
	cCamera *pCam = pPlayer ? pPlayer->GetCamera() : NULL;
	if (pCam == NULL)
		return;
	cVector3f vStart = pCam->GetPosition(), vDir = pCam->GetForward();
	for (size_t i = 0; i < mvEntities.size(); ++i)
	{
		cSomaLuxEntity *pEnt = mvEntities[i];
		if (pEnt->msLookAtCallback == "" || pEnt->mbActive == false)
			continue;
		float fMax = pEnt->mfLookAtMaxDistance > 0 ? pEnt->mfLookAtMaxDistance : 1000.0f;
		float fDist = 0;
		bool bLooking = SomaRayHitsEntity(pEnt, vStart, vDir, fMax, fDist);
		if (bLooking && pEnt->mbLookAtCheckRay)
			bLooking = SomaLineOfSight(vStart, vStart + vDir * fDist, pEnt);
		if (bLooking)
			pEnt->mfLookAtTime += afTimeStep;
		else
			pEnt->mfLookAtTime = 0;
		bool bNow = bLooking && pEnt->mfLookAtTime >= pEnt->mfLookAtDelay;
		if (bNow == pEnt->mbLookedAt)
			continue;
		pEnt->mbLookedAt = bNow;
		tString sFunc = pEnt->msLookAtCallback;
		if (bNow && pEnt->mbLookAtCallbackAutoRemove)
			pEnt->msLookAtCallback = "";
		int lState = bNow ? 1 : -1;
		mpRuntime->Call(mpScript, "void " + sFunc + "(const tString &in, int)", [&](asIScriptContext *c) {
			c->SetArgObject(0, &pEnt->msName);
			c->SetArgDWord(1, lState);
		});
	}
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
	timer.mfLength = afTime;
	mvTimers.push_back(timer);
}

void cSomaLuxMap::RestartCurrentTimer(float afTime)
{
	if (mpFiringTimer == NULL)
		return;
	cSomaLuxTimer timer = *mpFiringTimer;
	timer.mfTime = afTime < 0 ? timer.mfLength : afTime;
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

bool SomaTakePendingMapChange(tString &asMap, tString &asStart);
void SomaDrawImGuis();

void cSomaLuxUpdater::OnDraw(float afFrameTime)
{
	SomaDrawImGuis();
}

void cSomaLuxUpdater::Update(float afTimeStep)
{
	if (gpSomaBase->ScriptsHeld())
		return;
	bool bMap = cSomaLuxMap::GetCurrent() != NULL;
	if (bMap && gpSomaBase->UsesScriptPlayer() && gpSomaBase->UsesScriptMenu() == false)
	{
		bool bEscape = gpSomaBase->mpEngine->GetInput()->GetKeyboard()->KeyIsDown(eKey_Escape);
		if (bEscape && mbEscapeDown == false)
			gpSomaBase->SetGameplayPaused(gpSomaBase->IsGameplayPaused() == false);
		mbEscapeDown = bEscape;
	}
	if (gpSomaBase->IsGameplayPaused() && gpSomaBase->mbScriptGamePaused == false)
		return;
	tString sMap, sStart, sError;
	if (SomaTakePendingMapChange(sMap, sStart))
	{
		if (gbPendingNewGame)
		{
			gbPendingNewGame = false;
			gpSomaBase->mbScriptGamePaused = false;
			gpSomaBase->GetVisitedMaps().clear();
			SomaDeserializeGlobalVars("");
			if (cSomaLuxGame::Get())
				cSomaLuxGame::Get()->ResetScriptables();
		}
		if (gpSomaBase->LoadMap(sMap, cVector3f(0), sError, sStart.empty() ? "*" : sStart) == false)
			Error("SOMA script: %s\n", sError.c_str());
		return;
	}
	if (cSomaLuxGame::Get())
	{
		cSomaLuxGame::Get()->mbGameInput = bMap && gpSomaBase->UsesScriptPlayer() && gpSomaBase->UsesRealPlayer();
		cSomaLuxGame::Get()->Update(afTimeStep, gpSomaBase->mbScriptGamePaused);
	}
	if (cSomaLuxMap::GetCurrent() && gpSomaBase->mbScriptGamePaused == false)
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

static tString gsPendingMap, gsPendingStart;

void SomaRequestNewGame(const tString &asMap, const tString &asStart)
{
	SomaRequestMapChange(asMap, asStart);
	gbPendingNewGame = true;
}

//---------------------------------------
// Collide groups ("+a -b"): membership bits low, collide-with mask high

static bool CollideFlagsMatch(tFlag alA, tFlag alB)
{
	return (alA & (alB >> 16) & 0xFFFF) && (alB & (alA >> 16) & 0xFFFF);
}

unsigned int SomaCollideFlag(const tString &asGroups)
{
	static std::map<tString, int> mapGroups;
	iPhysicsBody::mpCollideFlagsMatch = CollideFlagsMatch;
	unsigned int lMember = 0, lExclude = 0;
	tStringVec vTokens;
	cString::GetStringVec(asGroups, vTokens, NULL);
	for (const tString &sTok : vTokens)
	{
		if (sTok.size() < 2 || (sTok[0] != '+' && sTok[0] != '-'))
			continue;
		auto it = mapGroups.find(sTok.substr(1));
		if (it == mapGroups.end())
		{
			if (mapGroups.size() >= 16)
				continue;
			it = mapGroups.insert(std::make_pair(sTok.substr(1), (int)mapGroups.size())).first;
		}
		(sTok[0] == '+' ? lMember : lExclude) |= 1u << it->second;
	}
	if (lMember == 0 && lExclude == 0)
		return 0;
	return (lMember ? lMember : 0xFFFF) | ((0xFFFF & ~lExclude) << 16);
}

void SomaRequestMapChange(const tString &asMap, const tString &asStart)
{
	gsPendingMap = cString::SetFileExt(cString::GetFileName(asMap), "hpm");
	gsPendingStart = asStart;
}

bool SomaTakePendingMapChange(tString &asMap, tString &asStart)
{
	if (gsPendingMap.empty())
		return false;
	asMap = gsPendingMap;
	asStart = gsPendingStart;
	gsPendingMap.clear();
	return true;
}

void RegisterSomaScriptLuxNatives(asIScriptEngine *e)
{
	typedef const tString &S;
	SOMA_FUNC(e, "void cLux_ChangeMap(const tString&in asMapName, const tString&in asStartPos, const tString&in asTransferArea, const tString&in asStartSound, const tString&in asEndSound)",
			  +[](S map, S start, S, S, S) {
				  SomaRequestMapChange(map, start);
				  Log("SOMA script: change map to %s (%s)\n", gsPendingMap.c_str(), start.c_str());
			  });
	SOMA_FUNC(e, "void cLux_StartNewGame()", +[]() {
		SomaRequestNewGame(gpSomaBase->GetInitConfigString("StartMap", "File"), gpSomaBase->GetInitConfigString("StartMap", "Pos"));
	});
	SOMA_FUNC(e, "void cLux_StartMap(const tString&in asMapName)", +[](S map) { SomaRequestNewGame(map, ""); });
	SOMA_FUNC(e, "const tString &cLux_GetMainMenuFile()", +[]() -> const tString & {
		static tString sFile;
		sFile = gpSomaBase->GetInitConfigString("MainMenu", "File");
		return sFile;
	});
	SOMA_FUNC(e, "void cLux_Exit()", +[]() { gpSomaBase->mpEngine->Exit(); });
	SOMA_FUNC(e, "void cLux_SetGamePaused(bool abX)", +[](bool b) { gpSomaBase->mbScriptGamePaused = b; });
	SOMA_FUNC(e, "bool cLux_GetGamePaused()", +[]() { return gpSomaBase->mbScriptGamePaused; });
	SOMA_FUNC(e, "bool cLux_IsChangingMap()", +[]() { return gsPendingMap.empty() == false; });
	SOMA_FUNC(e, "bool cLux_IsReadyToChangeMap()", +[]() { return true; });
	SOMA_FUNC(e, "bool cLux_IsStreamingMap()", +[]() { return false; });
	SOMA_FUNC(e, "void cLux_PreloadMap(const tString&in asMapName, eWorldStreamPriority aPrio = eWorldStreamPriority_Normal)", +[](S, int) {});
	SOMA_FUNC(e, "void cLux_DeloadMap(const tString&in asTransferArea)", +[](S) {});
	SOMA_FUNC(e, "void cLux_SetMapPreloadPriority(eWorldStreamPriority aPrio)", +[](int) {});
	SOMA_FUNC(e, "cLuxMap@ cLux_GetPreloadMap()", +[]() { return (cSomaLuxMap *)NULL; });

	for (const char *pType : {"cLuxMap", "iLuxEntity", "cLuxProp", "cLuxArea", "cLuxAgent", "cLuxCritter", "cLuxLiquidArea"})
		RegisterSomaScriptCallNatives(e, pType);

	SOMA_FUNC(e, "cLuxMap@ cLux_GetCurrentMap()", +[]() { return CurrentMap(); });

	const char *M = "cLuxMap";
	SOMA_METHOD(e, M, "uint GetCollideFlag(const tString&in asGroupName)", +[](cSomaLuxMap &, const tString &s) { return SomaCollideFlag(s); });
	SOMA_METHOD(e, M, "cWorld@ GetWorld()", +[](cSomaLuxMap &m) { return m.GetWorld(); });
	SOMA_METHOD(e, M, "bool IsActive()", +[](cSomaLuxMap &m) { return &m == cSomaLuxMap::GetCurrent(); });
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
	SOMA_METHOD(e, M, "void RestartCurrentTimer(float afTime = -1)", +[](cSomaLuxMap &m, float t) { m.RestartCurrentTimer(t); });
	SOMA_METHOD(e, M, "const tString& GetTimerUserVarString(const tString&in asName)", +[](cSomaLuxMap &m, S n) -> const tString & {
		static tString sEmpty;
		cSomaLuxTimer *t = m.GetTimer(n);
		return t ? t->msUserString : sEmpty;
	});
	SOMA_METHOD(e, M, "int IncTimerUserVarInt(const tString&in asName, int alX)",
				+[](cSomaLuxMap &m, S n, int x) { cSomaLuxTimer *t = m.GetTimer(n); return t ? (t->mlUserInt += x) : 0; });
	SOMA_METHOD(e, M, "float IncTimerUserVarFloat(const tString&in asName, float afX)",
				+[](cSomaLuxMap &m, S n, float x) { cSomaLuxTimer *t = m.GetTimer(n); return t ? (t->mfUserFloat += x) : 0.0f; });
	SOMA_METHOD(e, M, "bool GetTimersNamed(const tString&in asName, array<tString>&inout avNames)", +[](cSomaLuxMap &m, S n, CScriptArray &a) {
		bool bAny = false;
		for (cSomaLuxTimer &t : m.GetTimers())
			if (SomaWildcardMatch(n, t.msName))
			{
				a.InsertLast(&t.msName);
				bAny = true;
			}
		return bAny;
	});
	SOMA_METHOD(e, M, "int GetTimeStamp()", +[](cSomaLuxMap &m) { return (int)(m.GetTime() * 1000.0); });
	SOMA_METHOD(e, M, "float GetElapsedTime(int alTimeStamp)", +[](cSomaLuxMap &m, int st) { return (float)(m.GetTime() - st / 1000.0); });
	SOMA_METHOD(e, M, "bool GetEntityArray(const tString&in asName, eLuxEntityType aType, const tString&in asClassName, array<iLuxEntity@>&inout avEntities)",
				+[](cSomaLuxMap &m, S n, int t, S c, CScriptArray &a) {
					bool bAny = false;
					for (cSomaLuxEntity *p : m.GetEntities())
						if ((t == 7 || p->meType == t) && SomaWildcardMatch(n, p->msName) && SomaEntityIsClass(p, c))
						{
							a.InsertLast(&p);
							bAny = true;
						}
					return bAny;
				});
	SOMA_METHOD(e, M, "void CreateEntity(const tString&in asName, const tString&in asFile, const cMatrixf&in a_mtxTransform, const cVector3f&in avScale)",
				+[](cSomaLuxMap &m, S n, S f, const cMatrixf &mtx, const cVector3f &scale) { m.CreateEntity(n, f, mtx, scale); });
	SOMA_METHOD(e, M, "bool DestroyEntity(iLuxEntity @apEntity)", +[](cSomaLuxMap &m, cSomaLuxEntity *p) { if (p) m.DestroyEntity(p); return p != NULL; });
	SOMA_METHOD(e, M, "iLuxEntity @GetLatestEntity()", +[](cSomaLuxMap &m) { return m.mpLatestEntity; });
	SOMA_METHOD(e, M, "void ResetLatestEntity()", +[](cSomaLuxMap &m) { m.mpLatestEntity = NULL; });
	SOMA_METHOD(e, M, "iLuxEntity@ GetPlayerEntity()", +[](cSomaLuxMap &m) { return m.GetEntity(tString("Player")); });
	SOMA_METHOD(e, M, "bool GetEntityArrayID(const tString&in asName, eLuxEntityType aType, const tString&in asClassName, array<tID> &inout avOutEntities)",
				+[](cSomaLuxMap &m, S n, int t, S c, CScriptArray &a) {
					bool bAny = false;
					for (cSomaLuxEntity *p : m.GetEntities())
						if ((t == 7 || p->meType == t) && SomaWildcardMatch(n, p->msName) && SomaEntityIsClass(p, c))
						{
							a.InsertLast(&p->mID);
							bAny = true;
						}
					return bAny;
				});
	SOMA_FUNC(e, "bool Map_GetLightArray(const tString &in asName, array<iLight@> &inout avOutLights)", +[](S n, CScriptArray &a) {
		a.Resize(0);
		if (cSomaLuxMap::GetCurrent() == NULL) return false;
		cLightListIterator it = cSomaLuxMap::GetCurrent()->GetWorld()->GetLightIterator();
		while (it.HasNext())
		{
			iLight *pLight = it.Next();
			if (SomaWildcardMatch(n, pLight->GetName())) a.InsertLast(&pLight);
		}
		return a.GetSize() > 0;
	});
	SOMA_FUNC(e, "float cLux_GetLightLevelAtPos(const cVector3f&in avPos, iLight @apSkipLight, float afRadiusAdd)",
			  +[](const cVector3f &p, iLight *pSkip, float fAdd) -> float {
				  if (cSomaLuxMap::GetCurrent() == NULL) return 0;
				  float fLevel = 0;
				  cLightListIterator it = cSomaLuxMap::GetCurrent()->GetWorld()->GetLightIterator();
				  while (it.HasNext())
				  {
					  iLight *pLight = it.Next();
					  if (pLight == pSkip || pLight->IsVisible() == false) continue;
					  const cColor &c = pLight->GetDiffuseColor();
					  float fAmount = cMath::Max(c.r, cMath::Max(c.g, c.b)) * pLight->GetBrightness();
					  if (pLight->GetLightType() == eLightType_Box)
					  {
						  if (cMath::CheckPointInAABBIntersection(p, pLight->GetBoundingVolume()->GetMin(), pLight->GetBoundingVolume()->GetMax()))
						  {
							  cLightBox *pBox = static_cast<cLightBox *>(pLight);
							  const cVector3f &vDC = pBox->GetIrradianceBands()[0];
							  // Ref's SH term fits max(DC) within ~15%
							  fLevel += pBox->GetUseSphericalHarmonics() ? cMath::Max(vDC.x, cMath::Max(vDC.y, vDC.z)) * fAmount : fAmount;
						  }
						  continue;
					  }
					  if (pLight->GetLightType() == eLightType_Spot)
					  {
						  cLightSpot *pSpot = static_cast<cLightSpot *>(pLight);
						  cVector3f vLocal = cMath::MatrixMul(pSpot->GetViewMatrix(), p);
						  float fTan = tanf(pSpot->GetFOV() * 0.5f);
						  if (vLocal.z >= 0 || std::fabs(vLocal.y) > -vLocal.z * fTan || std::fabs(vLocal.x) > -vLocal.z * fTan * pSpot->GetAspect())
							  continue;
					  }
					  float fT = 1 - cMath::Vector3Dist(pLight->GetWorldPosition(), p) / (pLight->GetRadius() + fAdd);
					  if (fT > 0 && (pLight->GetCastShadows() == false || SomaLineOfSight(pLight->GetWorldPosition(), p, NULL)))
						  fLevel += fAmount * fT;
				  }
				  return fLevel;
			  });
	SOMA_FUNC(e, "double cLux_GetGameTime()", +[]() -> double {
		return gpSomaBase->mfGameStartTime < 0 ? 0.0 : gpSomaBase->mpEngine->GetGameTime() - gpSomaBase->mfGameStartTime;
	});
	struct cCameraExtra
	{
		cMatrixf mtxWorld;
		cVector3f vVelocity = 0;
		float fExtYaw = 0, fExtPitch = 0, fExtRoll = 0;
	};
	static std::map<cCamera *, cCameraExtra> mapCameraExtra;
	SOMA_METHOD(e, "cCamera", "const cMatrixf& GetMatrix()", +[](cCamera *c) -> const cMatrixf & {
		cMatrixf &m = mapCameraExtra[c].mtxWorld;
		m = cMath::MatrixInverse(c->GetViewMatrix());
		return m;
	});
	SOMA_METHOD(e, "cCamera", "void SetVelocity(const cVector3f&in avVel)", +[](cCamera *c, const cVector3f &v) { mapCameraExtra[c].vVelocity = v; });
	SOMA_METHOD(e, "cCamera", "const cVector3f& GetVelocity()const", +[](cCamera *c) -> const cVector3f & { return mapCameraExtra[c].vVelocity; });
	// Eye-tracker view offsets; no tracker, so stored only
	SOMA_METHOD(e, "cCamera", "void SetExtendedYaw(float afAngle)", +[](cCamera *c, float f) { mapCameraExtra[c].fExtYaw = f; });
	SOMA_METHOD(e, "cCamera", "void SetExtendedPitch(float afAngle)", +[](cCamera *c, float f) { mapCameraExtra[c].fExtPitch = f; });
	SOMA_METHOD(e, "cCamera", "void SetExtendedRoll(float afAngle)", +[](cCamera *c, float f) { mapCameraExtra[c].fExtRoll = f; });
	SOMA_METHOD(e, "cCamera", "float GetExtendedYaw() const", +[](cCamera *c) { return mapCameraExtra[c].fExtYaw; });
	SOMA_METHOD(e, "cCamera", "float GetExtendedPitch() const", +[](cCamera *c) { return mapCameraExtra[c].fExtPitch; });
	SOMA_METHOD(e, "cCamera", "float GetExtenededRoll() const", +[](cCamera *c) { return mapCameraExtra[c].fExtRoll; });
	SOMA_METHOD(e, "iPhysicsBody", "cBoundingVolume@ GetBoundingVolume()", +[](iPhysicsBody *b) { return b->GetBoundingVolume(); });
	SOMA_METHOD(e, "iPhysicsWorld", "void GetBodiesInAABB(const cVector3f&in avMin, const cVector3f&in avMax, array<iPhysicsBody@> &inout apBodyVec)",
				+[](iPhysicsWorld *w, const cVector3f &vMin, const cVector3f &vMax, CScriptArray &a) {
					cBoundingVolume bv;
					bv.SetLocalMinMax(vMin, vMax);
					std::vector<iPhysicsBody *> vBodies;
					w->GetBodiesInBV(&bv, &vBodies);
					for (iPhysicsBody *pBody : vBodies) a.InsertLast(&pBody);
				});
	SOMA_FUNC(e, "void Light_FadeTo(const tString &in asLightName, const cColor &in acColor, float afRadius, float afTime)",
			  +[](S n, const cColor &c, float r, float t) {
				  if (cSomaLuxMap::GetCurrent() == NULL) return;
				  cLightListIterator it = cSomaLuxMap::GetCurrent()->GetWorld()->GetLightIterator();
				  while (it.HasNext())
				  {
					  iLight *pLight = it.Next();
					  if (SomaWildcardMatch(n, pLight->GetName())) pLight->FadeTo(c, r < 0 ? pLight->GetRadius() : r, t);
				  }
			  });
	SOMA_METHOD(e, M, "void PlacePlayerAtStartPos(const tString&in asName)", +[](cSomaLuxMap &m, S n) {
		cStartPosEntity *pStart = n == "" ? m.GetWorld()->GetFirstStartPosEntity() : m.GetWorld()->GetStartPosEntity(n);
		if (pStart && cSomaLuxPlayer::Get())
			cSomaLuxPlayer::Get()->PlaceAtStart(pStart->GetWorldMatrix().GetTranslation(),
												 cMath::MatrixToEulerAngles(pStart->GetWorldMatrix().GetRotation(), eEulerRotationOrder_XYZ).y);
	});
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
