#include "SomaLuxEntity.h"
#include "SomaLux.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"

//---------------------------------------

bool SomaWildcardMatch(const tString &asPattern, const tString &asName)
{
	if (asPattern.find('*') == tString::npos)
		return asPattern == asName;
	size_t p = 0, n = 0, lStar = tString::npos, lMatch = 0;
	while (n < asName.size())
	{
		if (p < asPattern.size() && asPattern[p] == asName[n])
		{
			++p;
			++n;
		}
		else if (p < asPattern.size() && asPattern[p] == '*')
		{
			lStar = p++;
			lMatch = n;
		}
		else if (lStar != tString::npos)
		{
			p = lStar + 1;
			n = ++lMatch;
		}
		else
			return false;
	}
	while (p < asPattern.size() && asPattern[p] == '*')
		++p;
	return p == asPattern.size();
}

std::vector<cSomaLuxEntity *> &cSomaLuxEntity::Pending()
{
	static std::vector<cSomaLuxEntity *> v;
	return v;
}

const char *cSomaLuxEntity::GetBaseTypeName() const
{
	switch (meType)
	{
	case eSomaLuxEntityType_Area: return "cLuxArea";
	case eSomaLuxEntityType_LiquidArea: return "cLuxLiquidArea";
	case eSomaLuxEntityType_Critter: return "cLuxCritter";
	case eSomaLuxEntityType_Agent: return "cLuxAgent";
	default: return "cLuxProp";
	}
}

void cSomaLuxEntity::SetActive(bool abX)
{
	if (mbActive == abX)
		return;
	mbActive = abX;
	if (mpMesh)
	{
		mpMesh->SetActive(abX);
		mpMesh->SetVisible(abX);
	}
	for (iPhysicsBody *pBody : mvBodies)
		pBody->SetActive(abX);
	SetEffectsActive(abX && mbEffectsActive);
	Call("void OnSetActive(bool abX)", [abX](asIScriptContext *c) { c->SetArgByte(0, abX); });
}

void cSomaLuxEntity::SetEffectsActive(bool abX)
{
	for (iLight *pLight : mvLights)
	{
		pLight->SetVisible(abX);
		pLight->SetActive(abX);
	}
	for (cParticleSystem *pPS : mvParticleSystems)
		if (pPS)
		{
			pPS->SetVisible(abX);
			pPS->SetActive(abX);
		}
	for (cBillboard *pBB : mvBillboards)
	{
		pBB->SetVisible(abX);
		pBB->SetActive(abX);
	}
	for (cSoundEntity *pSound : mvSoundEntities)
	{
		if (abX)
			pSound->Play(false);
		else
			pSound->Stop(false);
	}
}

cMatrixf cSomaLuxEntity::GetMatrix()
{
	if (iPhysicsBody *pBody = GetMainBody())
		return pBody->GetLocalMatrix();
	if (mpMesh)
		return mpMesh->GetWorldMatrix();
	return m_mtxOnLoad;
}

cVector3f cSomaLuxEntity::GetPosition() { return GetMatrix().GetTranslation(); }

void cSomaLuxEntity::SetMatrix(const cMatrixf &a_mtx)
{
	if (iPhysicsBody *pBody = GetMainBody())
		pBody->SetMatrix(a_mtx);
	else if (mpMesh)
		mpMesh->SetMatrix(a_mtx);
	m_mtxOnLoad = a_mtx;
}

//---------------------------------------

static cSomaLuxEntity *Find(const tString &asName)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	return pMap ? pMap->GetEntity(asName) : NULL;
}

template <class F> static void ForMatching(const tString &asName, F aFunc)
{
	if (cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent())
		for (cSomaLuxEntity *pEnt : pMap->GetEntities())
			if (SomaWildcardMatch(asName, pEnt->msName))
				aFunc(pEnt);
}

static tString VarGet(cSomaLuxEntity *e, const tString &n)
{
	std::map<tString, tString>::iterator it = e->mmapScriptVars.find(n);
	return it == e->mmapScriptVars.end() ? tString() : it->second;
}

static void RegisterEntityMethods(asIScriptEngine *e, const char *T)
{
	typedef cSomaLuxEntity E;
	typedef const tString &S;
	SOMA_METHOD_NEW(e, T, "const tString& GetName()", +[](E *p) -> const tString & { return p->msName; });
	SOMA_METHOD_NEW(e, T, "const tID& GetID()", +[](E *p) -> const cSomaID & { return p->mID; });
	SOMA_METHOD_NEW(e, T, "eLuxEntityType GetEntityType()", +[](E *p) { return p->meType; });
	SOMA_METHOD_NEW(e, T, "const tString& GetClassName()", +[](E *p) -> const tString & { return p->msClassName; });
	SOMA_METHOD_NEW(e, T, "const tString& GetFileName()", +[](E *p) -> const tString & { return p->msFileName; });
	SOMA_METHOD_NEW(e, T, "void SetActive(bool abX)", +[](E *p, bool b) { p->SetActive(b); });
	SOMA_METHOD_NEW(e, T, "bool IsActive()", +[](E *p) { return p->mbActive; });
	SOMA_METHOD_NEW(e, T, "cLuxMap@ GetMap()", +[](E *p) { return p->mpMap; });
	SOMA_METHOD_NEW(e, T, "void SetMatrix(const cMatrixf&in a_mtxTransform)", +[](E *p, const cMatrixf &m) { p->SetMatrix(m); });
	SOMA_METHOD_NEW(e, T, "void SetPosition(const cVector3f&in avPos)", +[](E *p, const cVector3f &v) { cMatrixf m = p->GetMatrix(); m.SetTranslation(v); p->SetMatrix(m); });
	SOMA_METHOD_NEW(e, T, "cMatrixf GetMatrix()", +[](E *p) { return p->GetMatrix(); });
	SOMA_METHOD_NEW(e, T, "cVector3f GetPosition()", +[](E *p) { return p->GetPosition(); });
	SOMA_METHOD_NEW(e, T, "const cMatrixf& GetOnLoadTransform()", +[](E *p) -> const cMatrixf & { return p->m_mtxOnLoad; });
	SOMA_METHOD_NEW(e, T, "const cVector3f& GetOnLoadScale()", +[](E *p) -> const cVector3f & { return p->mvScale; });
	SOMA_METHOD_NEW(e, T, "int GetBodyNum()", +[](E *p) { return (int)p->mvBodies.size(); });
	SOMA_METHOD_NEW(e, T, "iPhysicsBody@ GetBody(int alIdx)", +[](E *p, int i) { return i >= 0 && i < (int)p->mvBodies.size() ? p->mvBodies[i] : (iPhysicsBody *)NULL; });
	SOMA_METHOD_NEW(e, T, "iPhysicsBody@ GetMainBody()", +[](E *p) { return p->GetMainBody(); });
	SOMA_METHOD_NEW(e, T, "iPhysicsBody@ GetBodyFromName(const tString&in asName)", +[](E *p, S n) {
		for (iPhysicsBody *b : p->mvBodies)
			if (b->GetName() == n || cString::GetFileName(b->GetName()) == n || SomaWildcardMatch("*_" + n, b->GetName()))
				return b;
		return (iPhysicsBody *)NULL;
	});
	SOMA_METHOD_NEW(e, T, "cMeshEntity@ GetMeshEntity()", +[](E *p) { return p->mpMesh; });
	SOMA_METHOD_NEW(e, T, "float GetMaxInteractDistance()", +[](E *p) { return p->mfMaxInteractDistance; });
	SOMA_METHOD_NEW(e, T, "void SetMaxInteractDistance(float afX)", +[](E *p, float f) { p->mfMaxInteractDistance = f; });
	SOMA_METHOD_NEW(e, T, "void SetInteractionDisabled(bool abX)", +[](E *p, bool b) { p->mbInteractionDisabled = b; });
	SOMA_METHOD_NEW(e, T, "bool GetInteractionDisabled()", +[](E *p) { return p->mbInteractionDisabled; });
	SOMA_METHOD_NEW(e, T, "void SetPlayerInteractCallback(const tString &in asCallbackFunc, bool abRemoveWhenInteracted)",
					+[](E *p, S f, bool r) { p->msInteractCallback = f; p->mbInteractCallbackAutoRemove = r; });
	SOMA_METHOD_NEW(e, T, "void SetPlayerLookAtCallback(const tString &in asCallbackFunc, bool abRemoveWhenLookedAt, bool abCheckCenterOfScreen, bool abCheckRayIntersection, float afMaxDistance, float afCallbackDelay)",
					+[](E *p, S f, bool r, bool, bool, float, float) { p->msLookAtCallback = f; p->mbLookAtCallbackAutoRemove = r; });
	SOMA_METHOD_NEW(e, T, "bool HasPlayerInteractCallback()", +[](E *p) { return p->msInteractCallback != ""; });
	SOMA_METHOD_NEW(e, T, "bool HasPlayerLookAtCallback()", +[](E *p) { return p->msLookAtCallback != ""; });
	SOMA_METHOD_NEW(e, T, "void SetEffectsActive(bool abActive, bool abFadeAndPlaySounds)", +[](E *p, bool b, bool) { p->mbEffectsActive = b; p->SetEffectsActive(b && p->mbActive); });
	SOMA_METHOD_NEW(e, T, "bool GetEffectsActive()", +[](E *p) { return p->mbEffectsActive; });
	SOMA_METHOD_NEW(e, T, "bool HasCollideCallbacks()", +[](E *p) { return !p->mvCollideCallbacks.empty(); });
	SOMA_METHOD_NEW(e, T, "void AddCollideCallback(iLuxEntity @apEntity, const tString&in asCallbackFunc)",
					+[](E *p, E *c, S f) { if (c) p->mvCollideCallbacks.push_back(E::cCollideCallback{c->msName, f}); });
	SOMA_METHOD_NEW(e, T, "void RemoveCollideCallback(const tString&in asEntityName)", +[](E *p, S n) {
		for (size_t i = 0; i < p->mvCollideCallbacks.size();)
			if (p->mvCollideCallbacks[i].msChild == n)
				p->mvCollideCallbacks.erase(p->mvCollideCallbacks.begin() + i);
			else
				++i;
	});
	SOMA_METHOD_NEW(e, T, "iLight@ GetLightFromName(const tString&in asName)", +[](E *p, S n) {
		for (iLight *l : p->mvLights)
			if (l->GetName() == n || SomaWildcardMatch("*" + n, l->GetName()))
				return l;
		return (iLight *)NULL;
	});
	SOMA_METHOD_NEW(e, T, "cParticleSystem@ GetParticleSystemFromName(const tString&in asName)", +[](E *p, S n) {
		for (cParticleSystem *l : p->mvParticleSystems)
			if (l && (l->GetName() == n || SomaWildcardMatch("*" + n, l->GetName())))
				return l;
		return (cParticleSystem *)NULL;
	});
	SOMA_METHOD_NEW(e, T, "cBillboard@ GetBillboardFromName(const tString&in asName)", +[](E *p, S n) {
		for (cBillboard *l : p->mvBillboards)
			if (l->GetName() == n || SomaWildcardMatch("*" + n, l->GetName()))
				return l;
		return (cBillboard *)NULL;
	});
	SOMA_METHOD_NEW(e, T, "cSoundEntity@ GetSoundEntityFromName(const tString&in asName)", +[](E *p, S n) {
		for (cSoundEntity *l : p->mvSoundEntities)
			if (l->GetName() == n || SomaWildcardMatch("*" + n, l->GetName()))
				return l;
		return (cSoundEntity *)NULL;
	});

	SOMA_METHOD_NEW(e, T, "void SetVarString(const tString&in asName, const tString&in asX)", +[](E *p, S n, S v) { p->mmapScriptVars[n] = v; });
	SOMA_METHOD_NEW(e, T, "void SetVarBool(const tString&in asName, bool abX)", +[](E *p, S n, bool v) { p->mmapScriptVars[n] = v ? "true" : "false"; });
	SOMA_METHOD_NEW(e, T, "void SetVarInt(const tString&in asName, int alX)", +[](E *p, S n, int v) { p->mmapScriptVars[n] = cString::ToString(v); });
	SOMA_METHOD_NEW(e, T, "void SetVarFloat(const tString&in asName, float afX)", +[](E *p, S n, float v) { p->mmapScriptVars[n] = cString::ToString(v); });
	SOMA_METHOD_NEW(e, T, "void IncVarInt(const tString&in asName, int alX)", +[](E *p, S n, int v) { p->mmapScriptVars[n] = cString::ToString(cString::ToInt(VarGet(p, n).c_str(), 0) + v); });
	SOMA_METHOD_NEW(e, T, "void IncVarFloat(const tString&in asName, float afX)", +[](E *p, S n, float v) { p->mmapScriptVars[n] = cString::ToString(cString::ToFloat(VarGet(p, n).c_str(), 0) + v); });
	SOMA_METHOD_NEW(e, T, "const tString& GetVarString(const tString&in asName)", +[](E *p, S n) -> const tString & { return p->mmapScriptVars[n]; });
	SOMA_METHOD_NEW(e, T, "bool GetVarBool(const tString&in asName)", +[](E *p, S n) { return cString::ToBool(VarGet(p, n).c_str(), false); });
	SOMA_METHOD_NEW(e, T, "int GetVarInt(const tString&in asName)", +[](E *p, S n) { return cString::ToInt(VarGet(p, n).c_str(), 0); });
	SOMA_METHOD_NEW(e, T, "float GetVarFloat(const tString&in asName)", +[](E *p, S n) { return cString::ToFloat(VarGet(p, n).c_str(), 0); });

	cSomaLuxScriptable::RegisterTimerNatives(e, T);
}

//---------------------------------------

void cSomaLuxEntity::RegisterNatives(asIScriptEngine *e)
{
	const char *vTypes[] = {"iLuxEntity", "cLuxProp", "cLuxArea", "cLuxAgent", "cLuxCritter", "cLuxLiquidArea"};
	for (const char *pType : vTypes)
		if (e->GetTypeInfoByName(pType))
			RegisterEntityMethods(e, pType);

	// tID
	SOMA_METHOD(e, "tID", "bool opEquals(const tID &in) const", +[](const cSomaID &a, const cSomaID &b) { return a == b; });
	SOMA_METHOD(e, "tID", "tID&opAssign(const tID &in)", +[](cSomaID &a, const cSomaID &b) -> cSomaID & { return a = b; });
	static cSomaID invalidId;
	e->RegisterGlobalProperty("const tID tID_Invalid", &invalidId);

	typedef const tString &S;
	SOMA_FUNC(e, "bool Entity_Exists(const tString &in asName)", +[](S n) { return Find(n) != NULL; });
	SOMA_FUNC(e, "void Entity_SetActive(const tString &in asName, bool abActive)", +[](S n, bool b) { ForMatching(n, [b](cSomaLuxEntity *p) { p->SetActive(b); }); });
	SOMA_FUNC(e, "bool Entity_IsActive(const tString &in asName)", +[](S n) { cSomaLuxEntity *p = Find(n); return p && p->mbActive; });
	SOMA_FUNC(e, "void Entity_SetInteractionDisabled(const tString &in asEntityName, bool abX)",
			  +[](S n, bool b) { ForMatching(n, [b](cSomaLuxEntity *p) { p->mbInteractionDisabled = b; }); });
	SOMA_FUNC(e, "void Entity_SetMaxInteractionDistance(const tString &in asEntityName, float afDistance)",
			  +[](S n, float f) { ForMatching(n, [f](cSomaLuxEntity *p) { p->mfMaxInteractDistance = f; }); });
	SOMA_FUNC(e, "bool Entity_IsInteractedWith(const tString &in asName)", +[](S) { return false; });
	SOMA_FUNC(e, "void Entity_SetEffectsActive(const tString &in asEntityName, bool abActive, bool abFadeAndPlaySounds)",
			  +[](S n, bool b, bool) { ForMatching(n, [b](cSomaLuxEntity *p) { p->mbEffectsActive = b; p->SetEffectsActive(b && p->mbActive); }); });
	SOMA_FUNC(e, "void Entity_SetPlayerInteractCallback(const tString &in asEntityName, const tString &in asCallback, bool abRemoveWhenInteracted)",
			  +[](S n, S f, bool r) { ForMatching(n, [&](cSomaLuxEntity *p) { p->msInteractCallback = f; p->mbInteractCallbackAutoRemove = r; }); });
	SOMA_FUNC(e, "void Entity_SetPlayerLookAtCallback(const tString &in asEntityName, const tString &in asCallback, bool abRemoveWhenLookedAt = true, bool abCheckCenterOfScreen = true, bool abCheckRayIntersection = true, float afMaxDistance = -1, float afCallbackDelay = 0)",
			  +[](S n, S f, bool r, bool, bool, float, float) { ForMatching(n, [&](cSomaLuxEntity *p) { p->msLookAtCallback = f; p->mbLookAtCallbackAutoRemove = r; }); });
	SOMA_FUNC(e, "bool Entity_AddCollideCallback(const tString &in asParentName, const tString &in asChildName, const tString &in asFunction)", +[](S par, S child, S f) {
		bool bAny = false;
		ForMatching(par, [&](cSomaLuxEntity *p) { p->mvCollideCallbacks.push_back(cSomaLuxEntity::cCollideCallback{child, f}); bAny = true; });
		return bAny;
	});
	SOMA_FUNC(e, "bool Entity_RemoveCollideCallback(const tString &in asParentName, const tString &in asChildName)", +[](S par, S child) {
		ForMatching(par, [&](cSomaLuxEntity *p) {
			for (size_t i = 0; i < p->mvCollideCallbacks.size();)
				if (SomaWildcardMatch(child, p->mvCollideCallbacks[i].msChild))
					p->mvCollideCallbacks.erase(p->mvCollideCallbacks.begin() + i);
				else
					++i;
		});
		return true;
	});
	SOMA_FUNC(e, "void Entity_SetVarString(const tString&in asEntityName, const tString&in asVarName, const tString&in asX)",
			  +[](S n, S v, S x) { ForMatching(n, [&](cSomaLuxEntity *p) { p->mmapScriptVars[v] = x; }); });
	SOMA_FUNC(e, "void Entity_SetVarBool(const tString&in asEntityName, const tString&in asVarName, bool abX)",
			  +[](S n, S v, bool x) { ForMatching(n, [&](cSomaLuxEntity *p) { p->mmapScriptVars[v] = x ? "true" : "false"; }); });
	SOMA_FUNC(e, "void Entity_SetVarInt(const tString&in asEntityName, const tString&in asVarName, int alX)",
			  +[](S n, S v, int x) { ForMatching(n, [&](cSomaLuxEntity *p) { p->mmapScriptVars[v] = cString::ToString(x); }); });
	SOMA_FUNC(e, "void Entity_SetVarFloat(const tString&in asEntityName, const tString&in asVarName, float afX)",
			  +[](S n, S v, float x) { ForMatching(n, [&](cSomaLuxEntity *p) { p->mmapScriptVars[v] = cString::ToString(x); }); });
	SOMA_FUNC(e, "tString Entity_GetVarString(const tString&in asEntityName, const tString&in asVarName)", +[](S n, S v) { cSomaLuxEntity *p = Find(n); return p ? VarGet(p, v) : tString(); });
	SOMA_FUNC(e, "bool Entity_GetVarBool(const tString&in asEntityName, const tString&in asVarName)", +[](S n, S v) { cSomaLuxEntity *p = Find(n); return p && cString::ToBool(VarGet(p, v).c_str(), false); });
	SOMA_FUNC(e, "int Entity_GetVarInt(const tString&in asEntityName, const tString&in asVarName)", +[](S n, S v) { cSomaLuxEntity *p = Find(n); return p ? cString::ToInt(VarGet(p, v).c_str(), 0) : 0; });
	SOMA_FUNC(e, "float Entity_GetVarFloat(const tString&in asEntityName, const tString&in asVarName)", +[](S n, S v) { cSomaLuxEntity *p = Find(n); return p ? cString::ToFloat(VarGet(p, v).c_str(), 0) : 0.0f; });
	SOMA_FUNC(e, "void Entity_PlaceAtEntity(const tString &in asEntityName, const tString &in asTargetEntity, const cVector3f &in avOffset = cVector3f_Zero, bool abAlignRotation = false, bool abUseEntFileCenter=false)",
			  +[](S n, S t, const cVector3f &off, bool bAlign, bool) {
				  cSomaLuxEntity *pTarget = Find(t);
				  if (pTarget == NULL)
					  return;
				  cMatrixf mtxTarget = pTarget->GetMatrix();
				  ForMatching(n, [&](cSomaLuxEntity *p) {
					  cMatrixf m = bAlign ? mtxTarget : p->GetMatrix();
					  m.SetTranslation(mtxTarget.GetTranslation() + off);
					  p->SetMatrix(m);
				  });
			  });
	SOMA_FUNC(e, "cVector3f Entity_GetDeltaToEntity(const tString &in asEntityA, const tString &in asEntityB)", +[](S a, S b) {
		cSomaLuxEntity *pA = Find(a), *pB = Find(b);
		return pA && pB ? pB->GetPosition() - pA->GetPosition() : cVector3f(0);
	});
	SOMA_FUNC(e, "void Entity_AddImpulse(const tString &in asEntityName, const cVector3f &in avImpulse, bool abLocalSpace, bool abOnlyMainBody)",
			  +[](S n, const cVector3f &v, bool, bool bMain) {
				  ForMatching(n, [&](cSomaLuxEntity *p) {
					  for (size_t i = 0; i < p->mvBodies.size() && (!bMain || i == 0); ++i)
						  p->mvBodies[i]->AddImpulse(v);
				  });
			  });
	SOMA_FUNC(e, "void Entity_AddForce(const tString &in asEntityName, const cVector3f &in avForce, bool abLocalSpace, bool abOnlyMainBody)",
			  +[](S n, const cVector3f &v, bool, bool bMain) {
				  ForMatching(n, [&](cSomaLuxEntity *p) {
					  for (size_t i = 0; i < p->mvBodies.size() && (!bMain || i == 0); ++i)
						  p->mvBodies[i]->AddForce(v);
				  });
			  });
}
