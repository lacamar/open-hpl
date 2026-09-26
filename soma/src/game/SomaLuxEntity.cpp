#include "SomaLuxEntity.h"
#include "SomaLuxPlayer.h"
#include <algorithm>
#include "SomaLuxGame.h"

#include <cmath>
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
	if (meType == eSomaLuxEntityType_Player)
	{
		iCharacterBody *pBody = cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetCharacterBody() : NULL;
		return pBody ? cMath::MatrixTranslate(pBody->GetFeetPosition()) : m_mtxOnLoad;
	}
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

int cSomaLuxEntity::PlayAnimation(const tString &asName, float afFadeTime, bool abLoop, const tString &asCallback)
{
	if (mpMesh == NULL)
		return -1;
	int lIdx = mpMesh->GetAnimationStateIndex(asName);
	if (lIdx < 0)
	{
		Warning("SOMA script: entity '%s' has no animation '%s'\n", msName.c_str(), asName.c_str());
		return -1;
	}
	mvAnimQueue.clear();
	if (afFadeTime > 0)
		mpMesh->PlayFadeTo(lIdx, abLoop, afFadeTime);
	else
		mpMesh->Play(lIdx, abLoop, true);
	mpMesh->GetAnimationState(lIdx)->SetTimePosition(0);
	mlCurrentAnim = lIdx;
	msAnimCallback = asCallback;
	return lIdx;
}

bool cSomaLuxEntity::GetAnimationIsPlaying()
{
	if (mpMesh == NULL || mlCurrentAnim < 0)
		return false;
	cAnimationState *pState = mpMesh->GetAnimationState(mlCurrentAnim);
	return pState->IsActive() && (pState->IsLooping() || pState->IsOver() == false);
}

void cSomaLuxEntity::StopAnimations(float afFadeTime)
{
	mvAnimQueue.clear();
	mlCurrentAnim = -1;
	msAnimCallback = "";
	if (mpMesh == NULL)
		return;
	for (int i = 0; i < mpMesh->GetAnimationStateNum(); ++i)
		if (mpMesh->GetAnimationState(i)->IsActive())
		{
			if (afFadeTime > 0)
				mpMesh->GetAnimationState(i)->FadeOut(afFadeTime);
			else
				mpMesh->GetAnimationState(i)->SetActive(false);
		}
}

void cSomaLuxEntity::UpdateAnimation(float afTimeStep)
{
	if (mpMesh == NULL || mlCurrentAnim < 0 || GetAnimationIsPlaying())
		return;
	tString sAnim = mpMesh->GetAnimationState(mlCurrentAnim)->GetName();
	if (mvAnimQueue.empty() == false)
	{
		std::pair<tString, bool> next = mvAnimQueue.front();
		mvAnimQueue.erase(mvAnimQueue.begin());
		std::vector<std::pair<tString, bool>> vRest = mvAnimQueue;
		tString sCallback = msAnimCallback;
		PlayAnimation(next.first, 0.2f, next.second, sCallback);
		mvAnimQueue = vRest;
		return;
	}
	tString sCallback = msAnimCallback;
	msAnimCallback = "";
	mlCurrentAnim = -1;
	if (sCallback != "")
	{
		cSomaLuxMap *pMap = mpMap ? mpMap : cSomaLuxMap::GetCurrent();
		if (pMap && pMap->GetScript())
			cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void " + sCallback + "(const tString &in, const tString &in)", [&](asIScriptContext *c) {
				c->SetArgObject(0, &msName);
				c->SetArgObject(1, &sAnim);
			});
	}
}

float cSomaLuxEntity::GetMaxInteractDistance()
{
	if (mfMaxInteractDistance > 0)
		return mfMaxInteractDistance;
	return cSomaLuxGame::Get() ? cSomaLuxGame::Get()->GetDefaultInteractDistance(meType) : 2.0f;
}

bool cSomaLuxEntity::CanInteract(int alType, iPhysicsBody *apBody)
{
	return CallBool("bool CanInteract(int alType, iPhysicsBody@ apBody)", [&](asIScriptContext *c) {
		c->SetArgDWord(0, alType);
		c->SetArgAddress(1, apBody);
	}, false);
}

// iLuxEntity::OnInteract: the map's player interact callback first, then the entity script
bool cSomaLuxEntity::OnInteract(int alType, iPhysicsBody *apBody, const cVector3f &avFocusPos, const tString &asData)
{
	if (msInteractCallback != "")
	{
		tString sFunc = msInteractCallback;
		if (mbInteractCallbackAutoRemove)
			msInteractCallback = "";
		cSomaLuxMap *pMap = mpMap ? mpMap : cSomaLuxMap::GetCurrent();
		if (pMap && pMap->GetScript())
			cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void " + sFunc + "(const tString &in)",
											[&](asIScriptContext *c) { c->SetArgObject(0, &msName); });
	}
	return CallBool("bool OnInteract(int, iPhysicsBody@, const cVector3f &in, const tString&in)", [&](asIScriptContext *c) {
		c->SetArgDWord(0, alType);
		c->SetArgAddress(1, apBody);
		c->SetArgObject(2, (void *)&avFocusPos);
		c->SetArgObject(3, (void *)&asData);
	}, false);
}

void cSomaLuxEntity::CreateAreaBody(iPhysicsWorld *apWorld)
{
	if (apWorld == NULL || mvBodies.empty() == false)
		return;
	cMatrixf m = m_mtxOnLoad;
	cVector3f vCols[3] = {m.GetRight(), m.GetUp(), m.GetForward()};
	cVector3f vSize = mvSize;
	for (int i = 0; i < 3; ++i)
	{
		float fLen = vCols[i].Length();
		if (fLen > 0)
		{
			vCols[i] = vCols[i] / fLen;
			vSize.v[i] *= fLen;
		}
	}
	cMatrixf mtxBody = cMath::MatrixUnitVectors(vCols[0], vCols[1], vCols[2], m.GetTranslation());
	iPhysicsBody *pBody = apWorld->CreateBody(msName, apWorld->CreateBoxShape(vSize, NULL));
	pBody->SetMass(0);
	pBody->SetCollide(false);
	pBody->SetCollideCharacter(false);
	pBody->SetBlocksSound(false);
	pBody->SetBlocksLight(false);
	pBody->SetMatrix(mtxBody);
	pBody->SetActive(mbActive);
	mvBodies.push_back(pBody);
}

//---------------------------------------

static std::map<void *, cSomaID> gmapObjectToID;
static std::map<int32_t, void *> gmapIDToObject;

cSomaID SomaObjectID(void *apObj)
{
	if (apObj == NULL)
		return cSomaID();
	auto it = gmapObjectToID.find(apObj);
	if (it != gmapObjectToID.end())
		return it->second;
	cSomaID id;
	id.mA = 0xfe;
	id.mB = (int32_t)gmapIDToObject.size() + 1;
	gmapObjectToID[apObj] = id;
	gmapIDToObject[id.mB] = apObj;
	return id;
}

void *SomaObjectFromID(const cSomaID &aID)
{
	if (aID.mA != 0xfe)
		return NULL;
	auto it = gmapIDToObject.find(aID.mB);
	return it == gmapIDToObject.end() ? NULL : it->second;
}

//---------------------------------------
// cLuxMapHelper::GetClosestEntity: nearest entity along the ray, blocked by colliding world geometry

class cSomaClosestRay : public iPhysicsRayCallback
{
public:
	std::vector<std::pair<float, iPhysicsBody *>> mvHits;
	bool OnIntersect(iPhysicsBody *apBody, cPhysicsRayParams *apParams) override
	{
		if (apBody->IsCharacter() == false)
			mvHits.push_back(std::make_pair(apParams->mfDist, apBody));
		return true;
	}
};

static bool SomaGetClosestEntity(const cVector3f &avStart, const cVector3f &avDir, float afLength, int alType, cSomaLuxEntity *&apEntOut,
								 iPhysicsBody *&apBodyOut, float &afDistOut)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap == NULL || pMap->GetWorld()->GetPhysicsWorld() == NULL)
		return false;
	cSomaClosestRay ray;
	pMap->GetWorld()->GetPhysicsWorld()->CastRay(&ray, avStart, avStart + avDir * afLength, true, false, false);
	std::sort(ray.mvHits.begin(), ray.mvHits.end(), [](auto &a, auto &b) { return a.first < b.first; });
	std::map<iPhysicsBody *, cSomaLuxEntity *> mapOwner;
	for (cSomaLuxEntity *pEnt : pMap->GetEntities())
		for (iPhysicsBody *pBody : pEnt->mvBodies)
			mapOwner[pBody] = pEnt;
	for (auto &hit : ray.mvHits)
	{
		auto it = mapOwner.find(hit.second);
		if (it == mapOwner.end())
		{
			if (hit.second->GetCollide())
				return false;
			continue;
		}
		cSomaLuxEntity *pEnt = it->second;
		if (pEnt->mbActive == false)
			continue;
		bool bArea = pEnt->meType == eSomaLuxEntityType_Area;
		if (bArea && (pEnt->mbInteractionDisabled || pEnt->CanInteract(alType, hit.second) == false))
			continue;
		apEntOut = pEnt;
		apBodyOut = hit.second;
		afDistOut = hit.first;
		return true;
	}
	return false;
}

void cSomaLuxEntity::RemoveCollideCallbacks(const tString &asChild)
{
	for (size_t i = 0; i < mvCollideCallbacks.size();)
		if (SomaWildcardMatch(asChild, mvCollideCallbacks[i].msChild) || SomaWildcardMatch(mvCollideCallbacks[i].msChild, asChild))
			mvCollideCallbacks.erase(mvCollideCallbacks.begin() + i);
		else
			++i;
}

//---------------------------------------

struct cSomaOBB
{
	cVector3f mvCenter;
	cVector3f mvAxis[3];
	cVector3f mvHalf;
};

static cSomaOBB AABBToOBB(const cVector3f &avMin, const cVector3f &avMax)
{
	cSomaOBB box;
	box.mvCenter = (avMin + avMax) * 0.5f;
	box.mvHalf = (avMax - avMin) * 0.5f;
	box.mvAxis[0] = cVector3f(1, 0, 0);
	box.mvAxis[1] = cVector3f(0, 1, 0);
	box.mvAxis[2] = cVector3f(0, 0, 1);
	return box;
}

static void EntityBoxes(cSomaLuxEntity *apEnt, std::vector<cSomaOBB> &avOut)
{
	if (apEnt->meType == eSomaLuxEntityType_Player)
	{
		iCharacterBody *pBody = cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetCharacterBody() : NULL;
		if (pBody)
			avOut.push_back(AABBToOBB(pBody->GetPosition() - pBody->GetSize() * 0.5f, pBody->GetPosition() + pBody->GetSize() * 0.5f));
		return;
	}
	if (apEnt->meType == eSomaLuxEntityType_Area || apEnt->meType == eSomaLuxEntityType_LiquidArea)
	{
		cMatrixf m = apEnt->GetMatrix();
		cSomaOBB box;
		box.mvCenter = m.GetTranslation();
		cVector3f vCols[3] = {m.GetRight(), m.GetUp(), m.GetForward()};
		for (int i = 0; i < 3; ++i)
		{
			float fLen = vCols[i].Length();
			box.mvAxis[i] = fLen > 0 ? vCols[i] / fLen : cVector3f(i == 0, i == 1, i == 2);
			box.mvHalf.v[i] = apEnt->mvSize.v[i] * 0.5f * (fLen > 0 ? fLen : 1);
		}
		avOut.push_back(box);
		return;
	}
	for (iPhysicsBody *pBody : apEnt->mvBodies)
		if (pBody->IsActive())
			avOut.push_back(AABBToOBB(pBody->GetBoundingVolume()->GetMin(), pBody->GetBoundingVolume()->GetMax()));
	if (avOut.empty() && apEnt->mpMesh)
		avOut.push_back(AABBToOBB(apEnt->mpMesh->GetBoundingVolume()->GetMin(), apEnt->mpMesh->GetBoundingVolume()->GetMax()));
}

static bool OBBOverlap(const cSomaOBB &a, const cSomaOBB &b)
{
	cVector3f vT = b.mvCenter - a.mvCenter;
	auto separated = [&](const cVector3f &axis) {
		float fLen = axis.SqrLength();
		if (fLen < 1e-8f)
			return false;
		float ra = 0, rb = 0;
		for (int i = 0; i < 3; ++i)
		{
			ra += a.mvHalf.v[i] * std::fabs(cMath::Vector3Dot(a.mvAxis[i], axis));
			rb += b.mvHalf.v[i] * std::fabs(cMath::Vector3Dot(b.mvAxis[i], axis));
		}
		return std::fabs(cMath::Vector3Dot(vT, axis)) > ra + rb;
	};
	for (int i = 0; i < 3; ++i)
		if (separated(a.mvAxis[i]) || separated(b.mvAxis[i]))
			return false;
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			if (separated(cMath::Vector3Cross(a.mvAxis[i], b.mvAxis[j])))
				return false;
	return true;
}

bool SomaEntitiesCollide(cSomaLuxEntity *apA, cSomaLuxEntity *apB)
{
	std::vector<cSomaOBB> vA, vB;
	EntityBoxes(apA, vA);
	EntityBoxes(apB, vB);
	for (const cSomaOBB &a : vA)
		for (const cSomaOBB &b : vB)
			if (OBBOverlap(a, b))
				return true;
	return false;
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
	SOMA_METHOD_NEW(e, T, "float GetMaxInteractDistance()", +[](E *p) { return p->GetMaxInteractDistance(); });
	SOMA_METHOD_NEW(e, T, "bool CanInteract(int alType, iPhysicsBody@ apBody)", +[](E *p, int t, iPhysicsBody *b) { return p->CanInteract(t, b); });
	SOMA_METHOD_NEW(e, T, "bool OnInteract(int alType, iPhysicsBody@ apBody, const cVector3f &in avFocusPos, const tString&in asData)",
					+[](E *p, int t, iPhysicsBody *b, const cVector3f &v, S d) { return p->OnInteract(t, b, v, d); });
	SOMA_METHOD_NEW(e, T, "int GetInteractIconId(int alType, iPhysicsBody@ apBody)", +[](E *p, int t, iPhysicsBody *b) {
		int lRet = 0;
		if (p->GetScript())
			cSomaScriptRuntime::Get()->Call(p->GetScript(), "int GetInteractIconId(int, iPhysicsBody@)", [&](asIScriptContext *c) {
				c->SetArgDWord(0, t);
				c->SetArgAddress(1, b);
			}, [&](asIScriptContext *c) { lRet = (int)c->GetReturnDWord(); });
		return lRet;
	});
	SOMA_METHOD_NEW(e, T, "int PlayAnimation(const tString&in asName, float afFadeTime=0.3f, bool abLoop=false, bool abPlayTransition=true, const tString&in asCallback=\"\", bool abGlobalSpace=false)",
					+[](E *p, S n, float f, bool l, bool, S cb, bool) { return p->PlayAnimation(n, f, l, cb); });
	SOMA_METHOD_NEW(e, T, "void AppendAnimation(const tString&in asName, bool abLoop)", +[](E *p, S n, bool l) {
		if (p->GetAnimationIsPlaying())
			p->mvAnimQueue.push_back(std::make_pair(n, l));
		else
			p->PlayAnimation(n, 0, l, "");
	});
	SOMA_METHOD_NEW(e, T, "bool GetAnimationIsPlaying()", +[](E *p) { return p->GetAnimationIsPlaying(); });
	SOMA_METHOD_NEW(e, T, "void StopAllAnimations(float afFadeTime)", +[](E *p, float f) { p->StopAnimations(f); });
	SOMA_METHOD_NEW(e, T, "void StopAnimation(const tString&in asName, float afFadeTime)", +[](E *p, S n, float f) {
		if (p->mpMesh && p->mpMesh->GetAnimationStateIndex(n) == p->mlCurrentAnim)
			p->StopAnimations(f);
	});
	SOMA_METHOD_NEW(e, T, "void StopAnimation(int alIdx, float afFadeTime)", +[](E *p, int i, float f) { if (i == p->mlCurrentAnim) p->StopAnimations(f); });
	SOMA_METHOD_NEW(e, T, "void SetNormalizeAnimationWeights(bool abX)", +[](E *p, bool b) { if (p->mpMesh) p->mpMesh->SetNormalizeAnimationWeights(b); });
	SOMA_METHOD_NEW(e, T, "int GetCurrentAnimationIndex()", +[](E *p) { return p->mlCurrentAnim; });
	SOMA_METHOD_NEW(e, T, "cAnimationState@ GetCurrentAnimationState()",
					+[](E *p) { return p->mpMesh && p->mlCurrentAnim >= 0 ? p->mpMesh->GetAnimationState(p->mlCurrentAnim) : (cAnimationState *)NULL; });
	SOMA_METHOD_NEW(e, T, "void SetCurrentAnimationPaused(bool abX)", +[](E *p, bool b) {
		if (p->mpMesh && p->mlCurrentAnim >= 0)
			p->mpMesh->GetAnimationState(p->mlCurrentAnim)->SetPaused(b);
	});
	SOMA_METHOD_NEW(e, T, "void SetIsInteractedWith(bool abX)", +[](E *p, bool b) { p->mbInteractedWith = b; });
	SOMA_METHOD_NEW(e, T, "bool IsInteractedWith()", +[](E *p) { return p->mbInteractedWith; });
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
	SOMA_FUNC(e, "bool Entity_IsInteractedWith(const tString &in asName)", +[](S n) { cSomaLuxEntity *p = Find(n); return p && p->mbInteractedWith; });
	SOMA_FUNC(e, "bool cLux_GetClosestEntity(const cVector3f&in avStart,const cVector3f&in avDir, float afRayLength, int alIteractType, bool abCheckLineOfSight, cLuxClosestEntityData @apOutput)",
			  +[](const cVector3f &st, const cVector3f &dir, float len, int type, bool, char *out) {
				  cSomaLuxEntity *pEnt = NULL;
				  iPhysicsBody *pBody = NULL;
				  float fDist = 0;
				  if (SomaGetClosestEntity(st, dir, len, type, pEnt, pBody, fDist) == false)
					  return false;
				  if (out)
				  {
					  *(float *)(out + 16) = fDist;
					  *(iPhysicsBody **)(out + 24) = pBody;
					  *(cSomaLuxEntity **)(out + 32) = pEnt;
				  }
				  return true;
			  });
	SOMA_FUNC(e, "iLuxEntity@ cLux_ID_Entity(tID aID)", +[](cSomaID id) { return cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(id) : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "iPhysicsBody@ cLux_ID_Body(tID aID)", +[](cSomaID id) { return (iPhysicsBody *)SomaObjectFromID(id); });
	SOMA_FUNC(e, "iEntity3D@ cLux_ID_Entity3D(tID aID)", +[](cSomaID id) { return (iEntity3D *)SomaObjectFromID(id); });
	// Engine objects: tIDs from a registry, looked up by typed cLux_ID_* functions
	const char *vIdTypes[] = {"iEntity3D", "cMeshEntity", "cSubMeshEntity", "iLight", "cLightPoint", "cLightSpot", "cLightBox",
							  "cLightMaskBox", "cBillboard", "cBillboardGroup", "cLensFlare", "cBeam", "cParticleSystem", "cGuiSetEntity",
							  "iRopeEntity", "cClothEntity", "cFogArea", "cExposureArea", "cForceField", "cSoundEntity", "iPhysicsBody",
							  "iPhysicsJoint", "iCharacterBody"};
	for (const char *pType : vIdTypes)
	{
		asITypeInfo *pInfo = e->GetTypeInfoByName(pType);
		if (pInfo == NULL)
			continue;
		if (pInfo->GetMethodByDecl("tID GetID()") == NULL)
			e->RegisterObjectMethod(pType, "tID GetID()", asFUNCTION(+[](asIScriptGeneric *g) {
				// Placeholder objects get IDs too, so cLux_ID_* hands them back
				new (g->GetAddressOfReturnLocation()) cSomaID(SomaObjectID(g->GetObject()));
			}), asCALL_GENERIC);
	}
	const char *vIdFuncs[] = {"iEntity3D@ cLux_ID_Entity3D(tID aID)", "cMeshEntity@ cLux_ID_MeshEntity(tID aID)", "cSubMeshEntity@ cLux_ID_SubMeshEntity(tID aID)",
							  "iLight@ cLux_ID_Light(tID aID)", "cLightMaskBox@ cLux_ID_LightMaskBox(tID aID)", "cBillboard@ cLux_ID_Billboard(tID aID)",
							  "cBillboardGroup@ cLux_ID_BillboardGroup(tID aID)", "cLensFlare@ cLux_ID_LensFlare(tID aID)", "cBeam@ cLux_ID_Beam(tID aID)",
							  "cParticleSystem@ cLux_ID_ParticleSystem(tID aID)", "cGuiSetEntity@ cLux_ID_GuiSetEntity(tID aID)",
							  "iRopeEntity@ cLux_ID_RopeEntity(tID aID)", "cClothEntity@ cLux_ID_ClothEntity(tID aID)", "cFogArea@ cLux_ID_FogArea(tID aID)",
							  "cExposureArea@ cLux_ID_ExposureArea(tID aID)", "cForceField@ cLux_ID_ForceField(tID aID)",
							  "cSoundEntity@ cLux_ID_SoundEntity(tID aID)", "iPhysicsBody@ cLux_ID_Body(tID aID)", "iPhysicsJoint@ cLux_ID_Joint(tID aID)",
							  "iCharacterBody@ cLux_ID_CharacterBody(tID aID)"};
	for (const char *pDecl : vIdFuncs)
		if (e->GetGlobalFunctionByDecl(pDecl) == NULL)
			e->RegisterGlobalFunction(pDecl, asFUNCTION((SomaBind::GenericFunc<+[](cSomaID id) { return SomaObjectFromID(id); }>)), asCALL_GENERIC);

	SOMA_FUNC(e, "cLuxProp@ cLux_ID_Prop(tID aID)", +[](cSomaID id) { cSomaLuxEntity *p = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(id) : NULL; return p && p->meType == eSomaLuxEntityType_Prop ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "cLuxArea@ cLux_ID_Area(tID aID)", +[](cSomaID id) { cSomaLuxEntity *p = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(id) : NULL; return p && p->meType == eSomaLuxEntityType_Area ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "cLuxLiquidArea@ cLux_ID_LiquidArea(tID aID)", +[](cSomaID id) { cSomaLuxEntity *p = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(id) : NULL; return p && p->meType == eSomaLuxEntityType_LiquidArea ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "cLuxCritter@ cLux_ID_Critter(tID aID)", +[](cSomaID id) { cSomaLuxEntity *p = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(id) : NULL; return p && p->meType == eSomaLuxEntityType_Critter ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "cLuxAgent@ cLux_ID_Agent(tID aID)", +[](cSomaID id) { cSomaLuxEntity *p = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(id) : NULL; return p && p->meType == eSomaLuxEntityType_Agent ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "cLuxProp@ cLux_ToProp(iLuxEntity @apEntity)", +[](cSomaLuxEntity *p) { return p && p->meType == eSomaLuxEntityType_Prop ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "cLuxArea@ cLux_ToArea(iLuxEntity @apEntity)", +[](cSomaLuxEntity *p) { return p && p->meType == eSomaLuxEntityType_Area ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "cLuxLiquidArea@ cLux_ToLiquidArea(iLuxEntity @apEntity)", +[](cSomaLuxEntity *p) { return p && p->meType == eSomaLuxEntityType_LiquidArea ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "cLuxAgent@ cLux_ToAgent(iLuxEntity @apEntity)", +[](cSomaLuxEntity *p) { return p && p->meType == eSomaLuxEntityType_Agent ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "cLuxCritter@ cLux_ToCritter(iLuxEntity @apEntity)", +[](cSomaLuxEntity *p) { return p && p->meType == eSomaLuxEntityType_Critter ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "void Entity_CallEntityInteract(const tString &in asName, const tString &in asBodyName = \"\", const cVector3f &in avFocusBodyOffset = cVector3f_Zero, const tString &in asData = \"\")",
			  +[](S n, S body, const cVector3f &off, S data) {
				  ForMatching(n, [&](cSomaLuxEntity *p) {
					  iPhysicsBody *pBody = p->GetMainBody();
					  for (iPhysicsBody *b : p->mvBodies)
						  if (body != "" && (b->GetName() == body || SomaWildcardMatch("*_" + body, b->GetName())))
							  pBody = b;
					  cVector3f vPos = (pBody ? pBody->GetWorldPosition() : p->GetPosition()) + off;
					  p->OnInteract(0, pBody, vPos, data);
				  });
			  });
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
