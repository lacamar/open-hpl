#include "SomaAgent.h"
#include "SomaCritter.h"
#include "SomaLux.h"
#include "SomaScriptApi.h"
#include <type_traits>
#include <cstring>
#include <functional>
#include <map>
#include "SomaBase.h"
#include "SomaLuxGame.h"
#include "impl/scriptarray.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include "SomaLuxPlayer.h"
#include "SomaLuxVoice.h"
#include "SomaLuxEntity.h"
#include "SomaScriptBind.h"
#include "SomaScriptNatives.h"
#include "SomaScriptRuntime.h"
#include "SomaSave.h"
#include "SomaSoundscape.h"

static tString gsPendingMap, gsPendingStart, gsPendingTransfer, gsPreloadMap;
static bool gbMapChangeIsTransfer = false;

static bool gbPendingNewGame = false;

cSomaLuxMap *cSomaLuxMap::mpCurrent = NULL;

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
	for (cSomaLuxEntity *pEnt : mvEntities)
	{
		cResourceVarsObject &v = pEnt->mInstanceVars;
		if (cSomaLuxEntity *pParent = GetEntity(v.GetVarString("ParentAttachEntity", "")))
			pEnt->AttachTo(pParent, pParent->GetBodyFromName(v.GetVarString("ParentAttachBody", "")), v.GetVarString("ParentAttachSocket", ""),
						   v.GetVarBool("ParentAttachUseRotation", true), v.GetVarBool("ParentAttachSnap", false), v.GetVarBool("ParentAttachLocked", false));
	}
	for (cSomaLuxEntity *pEnt : std::vector<cSomaLuxEntity *>(mvEntities))
		pEnt->Call("void OnAfterWorldLoad()");
	for (cSomaLuxEntity *pEnt : mvEntities)
	{
		pEnt->CaptureEffectDefaults();
		if (pEnt->mbEffectsActive == false)
			pEnt->mfEffectsAlpha = 0;
		pEnt->ApplyEffectsAlpha();
		pEnt->ResolveConnectedLights();
	}

	apRuntime->Call(mpScript, "void PreloadData()");
	return true;
}

bool cSomaLuxMap::SetupEntityScript(cSomaLuxEntity *apEnt)
{
	apEnt->ApplyInstanceVars(GetEntity(tString("Player")));
	static const char *vGroups[] = {"PropTypes", "AreaTypes", "LiquidAreaTypes", "LiquidAreaTypes", "CritterTypes", "AgentTypes"};
	if (apEnt->meType >= (int)(sizeof(vGroups) / sizeof(vGroups[0])))
		return false;
	if (apEnt->meType == eSomaLuxEntityType_Critter && apEnt->mpCritterProps == NULL)
	{
		apEnt->mpCritterProps = SomaNewPropBlock("cLuxCritter");
		SomaInitCritterProps(apEnt);
	}
	if (apEnt->meType == eSomaLuxEntityType_Agent)
		SomaCreateAgent(apEnt);
	const cSomaLuxGame::cEntityScript *pScript = cSomaLuxGame::Get() ? cSomaLuxGame::Get()->GetEntityScript(vGroups[apEnt->meType], apEnt->msClassName) : NULL;
	if (pScript == NULL)
		return false;
	tString sCustomFile = apEnt->mVars.GetVarString("CustomScriptFile", "");
	tString sCustomClass = apEnt->mVars.GetVarString("CustomScriptClass", "");
	bool bLoaded = sCustomFile != "" && sCustomClass != "" &&
				   apEnt->LoadScript(mpRuntime, sCustomFile, sCustomClass, apEnt->GetBaseTypeName());
	if (bLoaded == false && apEnt->LoadScript(mpRuntime, pScript->msFile, pScript->msClass, apEnt->GetBaseTypeName()) == false)
		return false;
	cWorld *pWorld = mpWorld;
	if (apEnt->meType == eSomaLuxEntityType_Agent)
		apEnt->Call("void SetupCharBody()");
	if (apEnt->meType == eSomaLuxEntityType_Area ||apEnt->meType == eSomaLuxEntityType_LiquidArea)
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
	if (vNew.empty())
		return NULL;
	vNew.back()->mbSpawned = true;
	return vNew.back();
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

static bool EntityNameMatch(const tString &asPattern, cSomaLuxEntity *apEnt)
{
	if (apEnt->meType == eSomaLuxEntityType_Player)
		return cString::ToLowerCase(asPattern) == "player" || SomaWildcardMatch(asPattern, apEnt->msName);
	return SomaWildcardMatch(asPattern, apEnt->msName);
}

cSomaLuxEntity *cSomaLuxMap::GetEntity(const tString &asName)
{
	std::map<tString, cSomaLuxEntity *>::iterator it = mmapEntities.find(cString::ToLowerCase(asName) == "player" ? tString("Player") : asName);
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
	std::vector<cSomaLuxTimer> &vDue = mvDueTimers;
	vDue.clear();
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

	for (cSomaLuxEntity *pEnt : std::vector<cSomaLuxEntity *>(mvEntities))
	{
		if (pEnt->GetScript() == NULL || pEnt->mbActive == false)
			continue;
		pEnt->UpdateTimers(afTimeStep);
		pEnt->CallWithFloat("void OnUpdate(float afTimeStep)", afTimeStep);
		pEnt->CallWithFloat("void OnVariableUpdate(float afTimeStep)", afTimeStep);
		if (pEnt->meType == eSomaLuxEntityType_Critter)
			SomaUpdateCritter(pEnt, afTimeStep);
		else if (pEnt->meType == eSomaLuxEntityType_Agent)
			SomaUpdateAgent(pEnt, afTimeStep);
		else if (pEnt->meType == eSomaLuxEntityType_Area)
			pEnt->UpdateCheckCollision(afTimeStep);
	}

	for (size_t i = 0; i < mvEntities.size(); ++i)
	{
		mvEntities[i]->UpdateAnimation(afTimeStep);
		mvEntities[i]->UpdateMove(afTimeStep);
		mvEntities[i]->UpdateEffectColor(afTimeStep);
		mvEntities[i]->UpdateGui(afTimeStep);
	}
	for (cSomaLuxEntity *pEnt : std::vector<cSomaLuxEntity *>(mvEntities))
		pEnt->UpdateAttachment();
	SomaUpdateLightConnections();
	cSomaSoundscape::Get()->Update(this, afTimeStep);
	UpdateLookAtCallbacks(afTimeStep);
	UpdateCollideCallbacks();
	for (cSomaLuxEntity *pEnt : mvEntities)
		if (pEnt->GetScript() && pEnt->mbActive)
			pEnt->CallWithFloat("void OnPostUpdate(float afTimeStep)", afTimeStep);
	std::vector<cSomaLuxEntity *> vBreaks;
	vBreaks.swap(mvPendingBreaks);
	for (cSomaLuxEntity *pEnt : vBreaks)
		pEnt->DoBreak();
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
	std::erase_if(mvTimers, [&](const cSomaLuxTimer &t) { return t.msName == asName; });
	for (cSomaLuxTimer &t : mvDueTimers)
		if (&t != mpFiringTimer && t.msName == asName)
			t.msFunction.clear();
}

cSomaLuxTimer *cSomaLuxMap::GetTimer(const tString &asName)
{
	for (size_t i = 0; i < mvTimers.size(); ++i)
		if (mvTimers[i].msName == asName)
			return &mvTimers[i];
	return NULL;
}

bool SomaTakePendingMapChange(tString &asMap, tString &asStart);
void SomaDrawImGuis();

void cSomaLuxUpdater::OnDraw(float afFrameTime)
{
	if (cSomaLuxGame::Get() && gpSomaBase->ScriptsHeld() == false)
		cSomaLuxGame::Get()->Draw(afFrameTime);
	SomaDrawImGuis();
}

void SomaSetGamePaused(bool abX)
{
	if (gpSomaBase->mbScriptGamePaused == abX)
		return;
	gpSomaBase->mbScriptGamePaused = abX;
	cSound *pSound = gpSomaBase->mpEngine->GetSound();
	const tFlag lWorld = 11;
	if (abX)
	{
		pSound->GetSoundHandler()->PauseAll(lWorld);
		pSound->GetMusicHandler()->Pause();
	}
	else
	{
		pSound->GetSoundHandler()->ResumeAll(lWorld);
		pSound->GetMusicHandler()->Resume();
	}
}

void cSomaLuxUpdater::AppLostInputFocus()
{
	cSomaLuxModule *pMenu = cSomaLuxGame::Get() ? cSomaLuxGame::Get()->GetModule("MenuHandler") : NULL;
	if (pMenu == NULL || gpSomaBase->ScriptsHeld() || cSomaLuxMap::GetCurrent() == NULL || gpSomaBase->mbScriptGamePaused ||
		pMenu->CallBool("bool GetMenuActive()", nullptr, true))
		return;
	pMenu->OnAction(11, true);
}

void cSomaLuxUpdater::Update(float afTimeStep)
{
	if (gpSomaBase->ScriptsHeld())
		return;
	bool bMap = cSomaLuxMap::GetCurrent() != NULL;
	tString sMap, sStart, sError;
	if (SomaTakePendingMapChange(sMap, sStart))
	{
		if (gbPendingNewGame)
		{
			gbPendingNewGame = false;
			SomaSetGamePaused(false);
			gpSomaBase->GetVisitedMaps().clear();
			SomaDeserializeGlobalVars("");
			if (cSomaLuxGame::Get())
				cSomaLuxGame::Get()->ResetScriptables();
		}
		cMatrixf mtxRel;
		float fYawRel = 0;
		tString sTransfer = sStart.empty() ? gsPendingTransfer : "";
		gbMapChangeIsTransfer = sTransfer != "";
		cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
		cSomaLuxEntity *pArea = bMap && sTransfer != "" ? cSomaLuxMap::GetCurrent()->GetEntity(sTransfer) : NULL;
		iCharacterBody *pBody = pPlayer ? pPlayer->GetCharacterBody() : NULL;
		if (pArea && pBody)
		{
			cMatrixf mtxInv = cMath::MatrixInverse(pArea->GetMatrix());
			mtxRel = cMath::MatrixTranslate(cMath::MatrixMul(mtxInv, pBody->GetFeetPosition()));
			fYawRel = pBody->GetYaw() - SomaStartYaw(pArea->GetMatrix());
		}
		gsPreloadMap.clear();
		if (gpSomaBase->GetSplash())
			gpSomaBase->GetSplash()->DrawLoadingScreen();
		if (gpSomaBase->LoadMap(sMap, cVector3f(0), sError, sStart.empty() ? "*" : sStart) == false)
			Error("SOMA script: %s\n", sError.c_str());
		else if (pArea && pBody && cSomaLuxMap::GetCurrent())
		{
			if (cSomaLuxEntity *pNew = cSomaLuxMap::GetCurrent()->GetEntity(sTransfer))
				pPlayer->PlaceAtStart(cMath::MatrixMul(pNew->GetMatrix(), mtxRel).GetTranslation(),
									  SomaStartYaw(pNew->GetMatrix()) + fYawRel);
			else
				Warning("SOMA script: transfer area '%s' not found in %s\n", sTransfer.c_str(), sMap.c_str());
		}
		return;
	}
	if (cSomaLuxGame::Get())
	{
		cSomaLuxGame::Get()->mbGameInput = bMap && gpSomaBase->UsesRealPlayer();
		cSomaLuxGame::Get()->Update(afTimeStep, gpSomaBase->mbScriptGamePaused);
	}
	if (cSomaLuxMap::GetCurrent() && cSomaLuxMap::GetCurrent()->mbActive && gpSomaBase->mbScriptGamePaused == false)
		cSomaLuxMap::GetCurrent()->Update(afTimeStep);
}

static cSomaLuxMap *CurrentMap() { return cSomaLuxMap::GetCurrent(); }

// Class filters name the script class (cScrAreaCameraAnimationNode) or the entity type
static bool SomaEntityIsClass(cSomaLuxEntity *apEnt, const tString &asClass)
{
	if (asClass.empty() || asClass == apEnt->msClassName)
		return true;
	return apEnt->GetScript() && asClass == apEnt->GetScript()->GetObjectType()->GetName();
}

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


void SomaRequestNewGame(const tString &asMap, const tString &asStart)
{
	SomaRequestMapChange(asMap, asStart);
	gbPendingNewGame = true;
}

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

// Unwrapped, in (-2pi, 0]: yaw limits set by map scripts clamp against it
float SomaStartYaw(const cMatrixf &a_mtxArea)
{
	return -cMath::GetAngleFromPoints2D(0, cVector2f(a_mtxArea.m[0][2], a_mtxArea.m[2][2]));
}

bool SomaStartPosCrouching(const tString &asName)
{
	cSomaLuxEntity *pEnt = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(asName) : NULL;
	return pEnt && pEnt->mInstanceVars.GetVarBool("Crouching", false);
}

void SomaRequestMapChange(const tString &asMap, const tString &asStart)
{
	gsPendingMap = cString::SetFileExt(cString::GetFileName(asMap), "hpm");
	gsPendingStart = asStart;
	gsPendingTransfer.clear();
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

// Script iterators are NOCOUNT handles; a recycled pool bounds the leak
template <class It>
static It *SomaPooledIterator(const It &aIt)
{
	static std::vector<It *> vPool;
	static size_t lNext = 0;
	if (vPool.size() < 256)
	{
		vPool.push_back(new It(aIt));
		return vPool.back();
	}
	It *pIt = vPool[lNext++ % vPool.size()];
	*pIt = aIt;
	return pIt;
}

template <class It, class T>
static void SomaRegisterIterator(asIScriptEngine *e, const char *apType, const char *apElem)
{
	SOMA_METHOD(e, apType, "bool HasNext()", +[](It *p) { return p->HasNext(); });
	SOMA_METHOD(e, apType, (tString(apElem) + "@ Next()").c_str(), +[](It *p) -> T { return p->Next(); });
	SOMA_METHOD(e, apType, (tString(apElem) + "@ PeekNext()").c_str(), +[](It *p) -> T { return p->PeekNext(); });
}

template <class T>
static void SomaRegisterChildIterator(asIScriptEngine *e, const char *apType)
{
	SOMA_METHOD(e, apType, "cEntity3DIterator@ GetChildIterator()", +[](T *p) { return SomaPooledIterator(static_cast<iEntity3D *>(p)->GetChildIterator()); });
}

static void RegisterSomaScriptIterators(asIScriptEngine *e)
{
	SomaRegisterIterator<cEntity3DIterator, iEntity3D *>(e, "cEntity3DIterator", "iEntity3D");
	SomaRegisterIterator<cLightListIterator, iLight *>(e, "cLightListIterator", "iLight");
	SomaRegisterIterator<cMeshEntityIterator, cMeshEntity *>(e, "cMeshEntityIterator", "cMeshEntity");
	SomaRegisterIterator<cParticleSystemIterator, cParticleSystem *>(e, "cParticleSystemIterator", "cParticleSystem");
	SomaRegisterIterator<cSoundEntityIterator, cSoundEntity *>(e, "cSoundEntityIterator", "cSoundEntity");
	SomaRegisterIterator<cBillboardIterator, cBillboard *>(e, "cBillboardIterator", "cBillboard");
	SomaRegisterIterator<cBeamIterator, cBeam *>(e, "cBeamIterator", "cBeam");
	SomaRegisterIterator<cFogAreaIterator, cFogArea *>(e, "cFogAreaIterator", "cFogArea");
	SomaRegisterIterator<cGuiSetEntityIterator, cGuiSetEntity *>(e, "cGuiSetEntityIterator", "cGuiSetEntity");

	SOMA_METHOD(e, "cWorld", "cLightListIterator@ GetLightIterator()", +[](cWorld *w) { return SomaPooledIterator(w->GetLightIterator()); });
	SOMA_METHOD(e, "cWorld", "cMeshEntityIterator@ GetStaticMeshEntityIterator()", +[](cWorld *w) { return SomaPooledIterator(w->GetStaticMeshEntityIterator()); });
	SOMA_METHOD(e, "cWorld", "cMeshEntityIterator@ GetDynamicMeshEntityIterator()", +[](cWorld *w) { return SomaPooledIterator(w->GetDynamicMeshEntityIterator()); });
	SOMA_METHOD(e, "cWorld", "cParticleSystemIterator@ GetParticleSystemIterator()", +[](cWorld *w) { return SomaPooledIterator(w->GetParticleSystemIterator()); });
	SOMA_METHOD(e, "cWorld", "cSoundEntityIterator@ GetSoundEntityIterator()", +[](cWorld *w) { return SomaPooledIterator(w->GetSoundEntityIterator()); });
	SOMA_METHOD(e, "cWorld", "cBillboardIterator@ GetBillboardIterator()", +[](cWorld *w) { return SomaPooledIterator(w->GetBillboardIterator()); });
	SOMA_METHOD(e, "cWorld", "cBeamIterator@ GetBeamIterator()", +[](cWorld *w) { return SomaPooledIterator(w->GetBeamIterator()); });
	SOMA_METHOD(e, "cWorld", "cFogAreaIterator@ GetFogAreaIterator()", +[](cWorld *w) { return SomaPooledIterator(w->GetFogAreaIterator()); });
	SOMA_METHOD(e, "cWorld", "cGuiSetEntityIterator@ GetGuiSetEntityIterator()", +[](cWorld *w) { return SomaPooledIterator(w->GetGuiSetEntityIterator()); });

	SomaRegisterChildIterator<iEntity3D>(e, "iEntity3D");
	SomaRegisterChildIterator<iPhysicsBody>(e, "iPhysicsBody");
	SomaRegisterChildIterator<cMeshEntity>(e, "cMeshEntity");
	SomaRegisterChildIterator<cSubMeshEntity>(e, "cSubMeshEntity");
	SomaRegisterChildIterator<iLight>(e, "iLight");
	SomaRegisterChildIterator<cLightPoint>(e, "cLightPoint");
	SomaRegisterChildIterator<cLightSpot>(e, "cLightSpot");
	SomaRegisterChildIterator<cLightBox>(e, "cLightBox");
	SomaRegisterChildIterator<cParticleSystem>(e, "cParticleSystem");
	SomaRegisterChildIterator<cSoundEntity>(e, "cSoundEntity");
	SomaRegisterChildIterator<cBillboard>(e, "cBillboard");
	SomaRegisterChildIterator<cBeam>(e, "cBeam");
	SomaRegisterChildIterator<cFogArea>(e, "cFogArea");
}

static float SomaLightLevelAtPos(const cVector3f &p, iLight *pSkip, float fAdd)
{
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
}

void RegisterSomaScriptLuxNatives(asIScriptEngine *e)
{
	RegisterSomaScriptIterators(e);
	typedef const tString &S;
	SOMA_FUNC(e, "void cLux_ChangeMap(const tString&in asMapName, const tString&in asStartPos, const tString&in asTransferArea, const tString&in asStartSound, const tString&in asEndSound)",
			  +[](S map, S start, S transfer, S, S) {
				  SomaRequestMapChange(map, start);
				  gsPendingTransfer = transfer;
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
	SOMA_FUNC(e, "void cLux_SetGamePaused(bool abX)", +[](bool b) { SomaSetGamePaused(b); });
	SOMA_FUNC(e, "bool cLux_GetGamePaused()", +[]() { return gpSomaBase->mbScriptGamePaused; });
	SOMA_FUNC(e, "bool cLux_IsChangingMap()", +[]() { return gsPendingMap.empty() == false; });
	SOMA_FUNC(e, "bool cLux_MapChangeIsTransfer()", +[]() { return gbMapChangeIsTransfer; });
	SOMA_FUNC(e, "bool cLux_IsReadyToChangeMap()", +[]() { return true; });
	SOMA_FUNC(e, "bool cLux_IsPlayGoReady(int&out alETA)", +[](int &l) { l = 0; return true; });
	SOMA_FUNC(e, "bool cLux_IsStreamingMap()", +[]() { return gsPreloadMap.empty() == false; });
	SOMA_FUNC(e, "void cLux_PreloadMap(const tString&in asMapName, eWorldStreamPriority aPrio = eWorldStreamPriority_Normal)", +[](S map, int) { gsPreloadMap = map; });
	SOMA_FUNC(e, "void cLux_DeloadMap(const tString&in asTransferArea)", +[](S) {});
	SOMA_FUNC(e, "void cLux_SetMapPreloadPriority(eWorldStreamPriority aPrio)", +[](int) {});
	// No streaming: the next map's settings aren't loaded yet, so the copy fades to the current ones
	SOMA_FUNC(e, "cLuxMap@ cLux_GetPreloadMap()", +[]() { return gsPreloadMap.empty() ? NULL : cSomaLuxMap::GetCurrent(); });

	for (const char *pType : {"cLuxMap", "iLuxEntity", "cLuxProp", "cLuxArea", "cLuxAgent", "cLuxCritter", "cLuxLiquidArea"})
		RegisterSomaScriptCallNatives(e, pType);

	SOMA_FUNC(e, "cLuxMap@ cLux_GetCurrentMap()", +[]() { return CurrentMap(); });

	const char *M = "cLuxMap";
	SOMA_METHOD(e, M, "uint GetCollideFlag(const tString&in asGroupName)", +[](cSomaLuxMap &, const tString &s) { return SomaCollideFlag(s); });
	SOMA_METHOD(e, M, "void BroadcastSoundHeardEvent(const tString& in asName, const cVector3f&in avPosition, float afRadius, int alPrio, bool abPhysicsObject=false)",
				+[](cSomaLuxMap &, const tString &, const cVector3f &p, float r, int prio, bool) { SomaBroadcastSoundHeard(p, r, prio); });
	SOMA_METHOD(e, M, "cWorld@ GetWorld()", +[](cSomaLuxMap &m) { return m.GetWorld(); });
	SOMA_METHOD(e, M, "bool IsActive()", +[](cSomaLuxMap &m) { return m.mbActive; });
	SOMA_METHOD(e, M, "void SetActive(bool abX)", +[](cSomaLuxMap &m, bool b) {
		m.mbActive = b;
		m.GetWorld()->SetActive(b);
	});
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
	// ponytail: gates an EntityExists check; always true instead of a per-frame destroyed flag
	SOMA_METHOD(e, M, "bool EntityWasDestroyed()", +[](cSomaLuxMap &) { return true; });
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
						if ((t == 7 || p->meType == t) && EntityNameMatch(n, p) && SomaEntityIsClass(p, c))
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
						if ((t == 7 || p->meType == t) && EntityNameMatch(n, p) && SomaEntityIsClass(p, c))
						{
							a.InsertLast(&p->mID);
							bAny = true;
						}
					return bAny;
				});
	{
		static auto World = []() { return cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetWorld() : (cWorld *)NULL; };
		static auto ForPS = [](S n, const std::function<void(cParticleSystem *)> &f) {
			if (World() == NULL) return;
			std::vector<cParticleSystem *> v;
			cParticleSystemIterator it = World()->GetParticleSystemIterator();
			while (it.HasNext())
			{
				cParticleSystem *p = it.Next();
				if (SomaWildcardMatch(n, p->GetName())) v.push_back(p);
			}
			for (cParticleSystem *p : v) f(p);
		};
		static auto AttachTarget = [](cSomaLuxEntity *p) -> iEntity3D * {
			if (p->mpMesh) return p->mpMesh;
			if (p->GetMainBody()) return p->GetMainBody();
			iCharacterBody *pBody = p->meType == eSomaLuxEntityType_Player && cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetCharacterBody() : NULL;
			return pBody ? pBody->GetCurrentBody() : NULL;
		};
		static auto CreateAt = [](S n, S f, S ent, bool attach) -> cParticleSystem * {
			cSomaLuxEntity *pEnt = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(cString::ToLowerCase(ent) == "player" ? tString("Player") : ent) : NULL;
			if (pEnt == NULL || World() == NULL)
			{
				Error("Could not find entity '%s' to create particle system '%s' at\n", ent.c_str(), n.c_str());
				return NULL;
			}
			cParticleSystem *pPS = World()->CreateParticleSystem(n, f, 1);
			if (pPS == NULL)
				return NULL;
			iEntity3D *pTarget = attach ? AttachTarget(pEnt) : NULL;
			if (pTarget)
			{
				pPS->SetMatrix(cMath::MatrixMul(cMath::MatrixInverse(pTarget->GetWorldMatrix()), pEnt->GetMatrix()));
				pTarget->AddChild(pPS);
			}
			else
				pPS->SetMatrix(pEnt->GetMatrix());
			return pPS;
		};
		SOMA_FUNC(e, "cParticleSystem@ ParticleSystem_CreateAtEntity(const tString &in asPSName, const tString &in asPSFile, const tString &in asEntity, bool abAttach)",
				  +[](S n, S f, S ent, bool attach) { return CreateAt(n, f, ent, attach); });
		SOMA_FUNC(e, "cParticleSystem@ ParticleSystem_CreateAtEntityExt(const tString &in asPSName, const tString &in asPSFile, const tString &in asEntity, bool abAttach, const cColor &in acColor, float afBrightness = 1.0f, bool abFadeAtDistance = false, float afFadeMinEnd = 1.0f, float afFadeMinStart = 2.0f, float afFadeMaxStart = 100.0f, float afFadeMaxEnd = 110.0f)",
				  +[](S n, S f, S ent, bool attach, const cColor &c, float b, bool fade, float minEnd, float minStart, float maxStart, float maxEnd) {
					  cParticleSystem *pPS = CreateAt(n, f, ent, attach);
					  if (pPS == NULL) return pPS;
					  pPS->SetColor(c);
					  pPS->SetBrightness(b);
					  pPS->SetFadeAtDistance(fade);
					  pPS->SetMinFadeDistanceEnd(minEnd);
					  pPS->SetMinFadeDistanceStart(minStart);
					  pPS->SetMaxFadeDistanceStart(maxStart);
					  pPS->SetMaxFadeDistanceEnd(maxEnd);
					  return pPS;
				  });
		SOMA_METHOD_NEW(e, "iLight", "void SetScriptableIsSaved(bool abX)", +[](iLight *o, bool b) { o->SetIsSaved(b); });
		SOMA_METHOD_NEW(e, "cLightPoint", "void SetScriptableIsSaved(bool abX)", +[](cLightPoint *o, bool b) { o->SetIsSaved(b); });
		SOMA_METHOD_NEW(e, "cLightSpot", "void SetScriptableIsSaved(bool abX)", +[](cLightSpot *o, bool b) { o->SetIsSaved(b); });
		SOMA_METHOD_NEW(e, "cLightBox", "void SetScriptableIsSaved(bool abX)", +[](cLightBox *o, bool b) { o->SetIsSaved(b); });
		SOMA_METHOD_NEW(e, "cParticleSystem", "void SetScriptableIsSaved(bool abX)", +[](cParticleSystem *o, bool b) { o->SetIsSaved(b); });
		SOMA_METHOD_NEW(e, "cSoundEntity", "void SetScriptableIsSaved(bool abX)", +[](cSoundEntity *o, bool b) { o->SetIsSaved(b); });
		SOMA_METHOD_NEW(e, "cBillboard", "void SetScriptableIsSaved(bool abX)", +[](cBillboard *o, bool b) { o->SetIsSaved(b); });
		SOMA_FUNC(e, "void ParticleSystem_Destroy(const tString &in asPSName)", +[](S n) { ForPS(n, [](cParticleSystem *p) { p->Kill(); }); });
		SOMA_FUNC(e, "void ParticleSystem_SetVisible(const tString &in asPSName, bool abVisible)", +[](S n, bool b) { ForPS(n, [b](cParticleSystem *p) { p->SetVisible(b); }); });
		SOMA_FUNC(e, "void ParticleSystem_SetActive(const tString &in asPSName, bool abActive)", +[](S n, bool b) { ForPS(n, [b](cParticleSystem *p) { p->SetActive(b); }); });
		SOMA_FUNC(e, "void ParticleSystem_SetColor(const tString &in asPSName, const cColor &in acColor)", +[](S n, const cColor &c) {
			ForPS(n, [&c](cParticleSystem *p) { p->SetColor(c); });
		});
		SOMA_FUNC(e, "void ParticleSystem_SetBrightness(const tString &in asPSName, float afBrightness)", +[](S n, float b) { ForPS(n, [b](cParticleSystem *p) { p->SetBrightness(b); }); });
		SOMA_FUNC(e, "void ParticleSystem_AttachToEntity(const tString &in asPSName, const tString &in asEntityName)", +[](S n, S ent) {
			cSomaLuxEntity *pEnt = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(ent) : NULL;
			iEntity3D *pTarget = pEnt ? AttachTarget(pEnt) : NULL;
			if (pTarget == NULL) return;
			ForPS(n, [pTarget](cParticleSystem *p) {
				p->SetMatrix(cMath::MatrixMul(cMath::MatrixInverse(pTarget->GetWorldMatrix()), p->GetWorldMatrix()));
				pTarget->AddChild(p);
			});
		});
		SOMA_FUNC(e, "bool ParticleSystem_Exists(const tString &in asPSName)", +[](S n) { bool b = false; ForPS(n, [&b](cParticleSystem *) { b = true; }); return b; });
		SOMA_FUNC(e, "bool Map_GetParticleSystemArray(const tString &in asName, array<cParticleSystem@> &inout avOutParticles)", +[](S n, CScriptArray &a) {
			a.Resize(0);
			ForPS(n, [&a](cParticleSystem *p) { a.InsertLast(&p); });
			return a.GetSize() > 0;
		});
		SOMA_METHOD(e, "cWorld", "cParticleSystem@ CreateParticleSystem( const tString&in asName,const tString&in asType, const cVector3f&in avSize, bool abRemoveWhenDead, bool abStatic)",
					+[](cWorld *w, S n, S t, const cVector3f &size, bool remove, bool) { return w->CreateParticleSystem(n, t, size, remove); });
		for (const char *pDecl : {"void ParticleSystem_Preload(const tString &in asFile)", "void cLux_PreloadParticleSystem(const tString &in asFile)",
								  "void cResources_PreloadParticleSystem(const tString&in asDataName)"})
			SOMA_FUNC(e, pDecl, +[](S) {});
		SOMA_METHOD(e, "cLuxMap", "void PreloadParticleSystem(const tString&in asFile)", +[](cSomaLuxMap *, S) {});
	}
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
	SOMA_FUNC(e, "float cLux_GetLightLevelAtPos(const cVector3f&in avPos, iLight @apSkipLight, float afRadiusAdd)", SomaLightLevelAtPos);
	cEnvironmentParticles::mpLightLevelFunc = +[](const cVector3f &p) { return SomaLightLevelAtPos(p, NULL, 0); };
	SOMA_FUNC(e, "const tString& cSystem_GetPlatformName()", +[]() -> const tString & { static tString s = "Linux"; return s; });
	SOMA_FUNC(e, "void cSystem_GetAvailableVideoModes(array<cVector2l> &inout avScreenSizes, array<int> &inout avBpps, array<int> &inout avMinRefreshRates, int alMinBpp, int alMinRefreshRate, bool abRemoveDuplicates)",
			  +[](CScriptArray &sizes, CScriptArray &bpps, CScriptArray &rates, int minBpp, int minRate, bool unique) {
				  std::vector<std::pair<cVector2l, int>> vModes;
				  std::vector<int> vRates;
				  for (int d = 0; d < SDL_GetNumVideoDisplays(); ++d)
					  for (int m = 0; m < SDL_GetNumDisplayModes(d); ++m)
					  {
						  SDL_DisplayMode mode;
						  if (SDL_GetDisplayMode(d, m, &mode) != 0)
							  continue;
						  int bpp = SDL_BYTESPERPIXEL(mode.format) * 8;
						  if (bpp < minBpp || (mode.refresh_rate && mode.refresh_rate < minRate))
							  continue;
						  vModes.push_back({cVector2l(mode.w, mode.h), bpp});
						  if (mode.refresh_rate && std::find(vRates.begin(), vRates.end(), mode.refresh_rate) == vRates.end())
							  vRates.push_back(mode.refresh_rate);
					  }
				  std::sort(vModes.begin(), vModes.end(), [](const std::pair<cVector2l, int> &a, const std::pair<cVector2l, int> &b) {
					  return a.first.x != b.first.x ? a.first.x < b.first.x : a.first.y < b.first.y;
				  });
				  if (unique)
					  vModes.erase(std::unique(vModes.begin(), vModes.end(), [](const std::pair<cVector2l, int> &a, const std::pair<cVector2l, int> &b) { return a.first == b.first; }),
								   vModes.end());
				  std::sort(vRates.begin(), vRates.end());
				  if (vRates.empty())
					  vRates.push_back(60);
				  sizes.Resize((asUINT)vModes.size());
				  bpps.Resize((asUINT)vModes.size());
				  for (size_t i = 0; i < vModes.size(); ++i)
				  {
					  sizes.SetValue((asUINT)i, &vModes[i].first);
					  bpps.SetValue((asUINT)i, &vModes[i].second);
				  }
				  rates.Resize((asUINT)vRates.size());
				  for (size_t i = 0; i < vRates.size(); ++i)
					  rates.SetValue((asUINT)i, &vRates[i]);
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
	SOMA_METHOD(e, "iPhysicsBody", "cVector3f GetMassCenter() const", +[](iPhysicsBody *b) { return b->GetMassCentre(); });
	SOMA_FUNC(e, "bool cMath_CheckPointInBVIntersection(const cVector3f&in avPoint, cBoundingVolume@ aBV)",
			  +[](const cVector3f &v, cBoundingVolume *b) { return b && cMath::CheckPointInBVIntersection(v, *b); });
	SOMA_FUNC(e, "bool cMath_CheckBVIntersection(cBoundingVolume@ aBV1,cBoundingVolume@ aBV2)",
			  +[](cBoundingVolume *a, cBoundingVolume *b) { return a && b && cMath::CheckBVIntersection(*a, *b); });
	SOMA_METHOD(e, "cBoundingVolume", "void SetTransform(const cMatrixf&in a_mtxTransform, bool abUpdateSize = true)", +[](cBoundingVolume *b, const cMatrixf &m, bool) { b->SetTransform(m); });
	SOMA_METHOD(e, "iPhysicsWorld", "iCollideShape@ CreateCylinderShape(float afRadius, float afHeight, cMatrixf&in a_mtxOffsetMtx)",
				+[](iPhysicsWorld *w, float r, float h, cMatrixf &m) { return w->CreateCylinderShape(r, h, &m); });
	SOMA_METHOD(e, "iPhysicsWorld", "bool CheckShapeWorldCollision(cVector3f&out avPushVector, iCollideShape@ apShape, const cMatrixf&in a_mtxTransform, iPhysicsBody@ apSkipBody, bool abSkipStatic, bool abIsCharacter, bool abCollideCharacter)",
				+[](iPhysicsWorld *w, cVector3f &push, iCollideShape *pShape, const cMatrixf &m, iPhysicsBody *pSkip, bool bSkipStatic, bool bChar, bool bCollideChar) {
					push = 0;
					return pShape && w->CheckShapeWorldCollision(&push, pShape, m, pSkip, bSkipStatic, bChar, NULL, bCollideChar);
				});
	SOMA_METHOD(e, "iPhysicsWorld", "bool CheckShapeWorldCollision(cVector3f&out avPushVector, iCollideShape@ apShape, const cMatrixf&in a_mtxTransform, iPhysicsBody@ apSkipBody, bool abSkipStatic)",
				+[](iPhysicsWorld *w, cVector3f &push, iCollideShape *pShape, const cMatrixf &m, iPhysicsBody *pSkip, bool bSkipStatic) {
					push = 0;
					return pShape && w->CheckShapeWorldCollision(&push, pShape, m, pSkip, bSkipStatic);
				});
	SOMA_METHOD(e, "iCharacterBody", "bool CheckCharacterFits(const cVector3f &in avPosition, bool abFeetPosition, int alSizeIdx, cVector3f &out avOutPushBackVec)",
				+[](iCharacterBody *c, const cVector3f &p, bool bFeet, int lSize, cVector3f &push) {
					push = 0;
					return c->CheckCharacterFits(p, bFeet, lSize < c->GetShapeNum() ? lSize : -1, &push);
				});
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
	static auto ForLights = [](const tString &n, std::function<void(iLight *)> f) {
		if (cSomaLuxMap::GetCurrent() == NULL) return;
		cLightListIterator it = cSomaLuxMap::GetCurrent()->GetWorld()->GetLightIterator();
		while (it.HasNext())
		{
			iLight *pLight = it.Next();
			if (SomaWildcardMatch(n, pLight->GetName())) f(pLight);
		}
	};
	SOMA_FUNC(e, "void Light_SetVisible(const tString &in asLightName, bool abVisible)", +[](S n, bool b) { ForLights(n, [b](iLight *l) { l->SetVisible(b); }); });
	SOMA_FUNC(e, "void Light_SetBrightness(const tString &in asLightName, float afBrightness)", +[](S n, float f) { ForLights(n, [f](iLight *l) { l->SetBrightness(f); }); });
	SOMA_FUNC(e, "float Light_GetBrightness(const tString &in asLightName)", +[](S n) {
		float f = 0;
		ForLights(n, [&f](iLight *l) { f = l->GetBrightness(); });
		return f;
	});
	SOMA_FUNC(e, "void Light_SetFlickerActive(const tString &in asLightName, bool abX)", +[](S n, bool b) { ForLights(n, [b](iLight *l) { l->SetFlickerActive(b); }); });
	SOMA_FUNC(e, "void Light_SetCastShadows(const tString &in asLightName, bool abX)", +[](S n, bool b) { ForLights(n, [b](iLight *l) { l->SetCastShadows(b); }); });
	SOMA_FUNC(e, "void Billboard_SetVisible(const tString &in asBillboardName, bool abVisible)", +[](S n, bool b) {
		if (cSomaLuxMap::GetCurrent() == NULL) return;
		cBillboardIterator it = cSomaLuxMap::GetCurrent()->GetWorld()->GetBillboardIterator();
		while (it.HasNext())
		{
			cBillboard *p = it.Next();
			if (SomaWildcardMatch(n, p->GetName())) p->SetVisible(b);
		}
	});
	SOMA_METHOD(e, M, "void PlacePlayerAtStartPos(const tString&in asName)", +[](cSomaLuxMap &m, S n) {
		cStartPosEntity *pStart = n == "" ? m.GetWorld()->GetFirstStartPosEntity() : m.GetWorld()->GetStartPosEntity(n);
		if (pStart && cSomaLuxPlayer::Get())
			cSomaLuxPlayer::Get()->PlaceAtStart(pStart->GetWorldMatrix().GetTranslation(),
												 SomaStartYaw(pStart->GetWorldMatrix()),
												 SomaStartPosCrouching(pStart->GetName()));
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
