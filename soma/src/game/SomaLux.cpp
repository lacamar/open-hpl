#include "SomaLux.h"
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
	for (cSomaLuxEntity *pEnt : mvEntities)
	{
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
	if (cSomaLuxGame::Get())
		cSomaLuxGame::Get()->Update(afTimeStep);
	if (cSomaLuxMap::GetCurrent())
		cSomaLuxMap::GetCurrent()->Update(afTimeStep);
}

//---------------------------------------
// Natives

static cSomaLuxMap *CurrentMap() { return cSomaLuxMap::GetCurrent(); }

void RegisterSomaScriptLuxNatives(asIScriptEngine *e)
{
	SOMA_FUNC(e, "cLuxMap@ cLux_GetCurrentMap()", +[]() { return CurrentMap(); });

	const char *M = "cLuxMap";
	SOMA_METHOD(e, M, "cWorld@ GetWorld()", +[](cSomaLuxMap &m) { return m.GetWorld(); });
	SOMA_METHOD(e, M, "iPhysicsWorld@ GetPhysicsWorld()", +[](cSomaLuxMap &m) { return m.GetWorld()->GetPhysicsWorld(); });
	SOMA_METHOD(e, M, "iLuxEntity @GetEntityByName(const tString&in asName, eLuxEntityType aType=eLuxEntityType_LastEnum, const tString&in asClassName=\"\")",
				+[](cSomaLuxMap &m, const tString &n, int t, const tString &c) {
					cSomaLuxEntity *p = m.GetEntity(n);
					return p && (t == 7 || p->meType == t) && (c.empty() || c == p->msClassName) ? p : (cSomaLuxEntity *)NULL;
				});
	SOMA_METHOD(e, M, "iLuxEntity @GetEntityByID(tID alID, eLuxEntityType aType=eLuxEntityType_LastEnum, const tString&in asClassName=\"\")",
				+[](cSomaLuxMap &m, cSomaID id, int t, const tString &c) {
					cSomaLuxEntity *p = m.GetEntity(id);
					return p && (t == 7 || p->meType == t) && (c.empty() || c == p->msClassName) ? p : (cSomaLuxEntity *)NULL;
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
