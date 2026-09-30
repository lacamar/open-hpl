#include "SomaLuxEntity.h"
#include "SomaScriptApi.h"
#include "scene/GuiSetEntity.h"
#include "SomaLuxPlayer.h"
#include "SomaImGui.h"
#include "SomaBase.h"
#include <algorithm>
#include "SomaLuxGame.h"
#include "SomaCritter.h"

#include <cmath>
#include <set>
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

void cSomaLuxEntity::SetStaticPhysics(bool abX)
{
	if (abX == mbStaticPhysics)
		return;
	mbStaticPhysics = abX;
	if (abX)
		mvDynamicMass.clear();
	for (size_t i = 0; i < mvBodies.size(); ++i)
	{
		iPhysicsBody *pBody = mvBodies[i];
		if (abX)
		{
			mvDynamicMass.push_back(pBody->GetMass());
			pBody->SetMass(0);
			pBody->SetLinearVelocity(0);
			pBody->SetAngularVelocity(0);
		}
		else if (i < mvDynamicMass.size())
		{
			pBody->SetMass(mvDynamicMass[i]);
			pBody->Enable();
		}
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

void cSomaLuxEntity::ResolveConnectedLights()
{
	mbConnectedLightsResolved = true;
	if (mpMap == NULL) return;
	for (const char *pPrefix : {"", "Extra"})
	{
		tString sPrefix = pPrefix;
		tString *pNames = mInstanceVars.GetUserVariable(sPrefix + "ConnectedLight");
		if (pNames == NULL || pNames->empty()) continue;
		tStringVec vPatterns;
		cString::GetStringVec(*pNames, vPatterns, NULL);
		float fAmount = mInstanceVars.GetVarFloat(sPrefix + "ConnectionLightAmount", 1);
		tString *pType = mInstanceVars.GetUserVariable(sPrefix + "ConnectionLightType");
		bool bMul = pType == NULL || *pType != "Add";
		cLightListIterator it = mpMap->GetWorld()->GetLightIterator();
		while (it.HasNext())
		{
			iLight *pLight = it.Next();
			for (const tString &sPattern : vPatterns)
				if (SomaWildcardMatch(sPattern, pLight->GetName()))
				{
					mvConnectedLights.push_back(cConnectedLight{pLight, pLight->GetDiffuseColor(), fAmount, bMul});
					break;
				}
		}
	}
}

void cSomaLuxEntity::SetEffectsActive(bool abX)
{
	if (mbConnectedLightsResolved == false) ResolveConnectedLights();
	for (cConnectedLight &cl : mvConnectedLights)
	{
		float fEffect = abX ? 1.0f : 0.0f;
		float fMul = cl.mbMul ? 1.0f - cl.mfAmount + cl.mfAmount * fEffect : cl.mfAmount * fEffect;
		cColor col(cl.mBaseColor.r * fMul, cl.mBaseColor.g * fMul, cl.mBaseColor.b * fMul, cl.mBaseColor.a * fMul);
		cl.mpLight->SetDiffuseColor(col);
		cl.mpLight->SetVisible(fMul > 0);
	}
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
	if (mbCameraProxy && cSomaLuxPlayer::Get() && cSomaLuxPlayer::Get()->GetCamera())
		return cMath::MatrixInverse(cSomaLuxPlayer::Get()->GetCamera()->GetViewMatrix());
	if (iPhysicsBody *pBody = GetMainBody())
		return pBody->GetLocalMatrix();
	if (mpMesh)
		return mpMesh->GetWorldMatrix();
	return m_mtxOnLoad;
}

cVector3f cSomaLuxEntity::GetPosition() { return GetMatrix().GetTranslation(); }

void cSomaLuxEntity::SetMatrix(const cMatrixf &a_mtx)
{
	if (meType == eSomaLuxEntityType_Player)
	{
		if (iCharacterBody *pBody = cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetCharacterBody() : NULL)
			pBody->SetFeetPosition(a_mtx.GetTranslation(), true);
		return;
	}
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

void cSomaLuxEntity::MoveLinearTo(const cVector3f &avGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed, const tString &asCallback)
{
	mbMoving = true;
	mvMoveGoal = avGoal;
	mfMoveAcc = afAcc;
	mfMoveMaxSpeed = afMaxSpeed;
	mfMoveSlowdownDist = afSlowdownDist;
	if (abResetSpeed)
		mfMoveSpeed = 0;
	msMoveCallback = asCallback;
}

void cSomaLuxEntity::UpdateMove(float afTimeStep)
{
	if (mbMoving == false)
		return;
	cMatrixf m = GetMatrix();
	cVector3f vDelta = mvMoveGoal - m.GetTranslation();
	float fDist = vDelta.Length();
	mfMoveSpeed = cMath::Min(mfMoveSpeed + mfMoveAcc * afTimeStep, mfMoveMaxSpeed);
	float fSpeed = mfMoveSpeed;
	if (mfMoveSlowdownDist > 0 && fDist < mfMoveSlowdownDist)
		fSpeed = cMath::Min(fSpeed, cMath::Max(mfMoveMaxSpeed * fDist / mfMoveSlowdownDist, mfMoveMaxSpeed * 0.05f));
	float fStep = fSpeed * afTimeStep;
	bool bDone = fStep >= fDist;
	m.SetTranslation(bDone ? mvMoveGoal : m.GetTranslation() + vDelta * (fStep / fDist));
	SetMatrix(m);
	if (bDone == false)
		return;
	mbMoving = false;
	mfMoveSpeed = 0;
	tString sCallback = msMoveCallback;
	msMoveCallback = "";
	cSomaLuxMap *pMap = mpMap ? mpMap : cSomaLuxMap::GetCurrent();
	if (sCallback != "" && pMap && pMap->GetScript())
		cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void " + sCallback + "(const tString &in)", [&](asIScriptContext *c) { c->SetArgObject(0, &msName); });
}

void cSomaLuxEntity::ApplyInstanceVars()
{
	cResourceVarsObject &v = mInstanceVars;
	if (v.GetVarString("PlayerInteractCallback", "") != "")
	{
		msInteractCallback = v.GetVarString("PlayerInteractCallback", "");
		mbInteractCallbackAutoRemove = v.GetVarBool("PlayerInteractCallbackAutoRemove", false);
	}
	if (v.GetVarString("PlayerLookAtCallback", "") != "")
	{
		msLookAtCallback = v.GetVarString("PlayerLookAtCallback", "");
		mbLookAtCallbackAutoRemove = v.GetVarBool("PlayerLookAtCallbackAutoRemove", false);
		mbLookAtCheckCenter = v.GetVarBool("PlayerLookAtCheckCenterOfScreen", true);
		mbLookAtCheckRay = v.GetVarBool("PlayerLookAtCheckRayIntersection", true);
		mfLookAtMaxDistance = v.GetVarFloat("PlayerLookAtMaxDistance", -1);
		mfLookAtDelay = v.GetVarFloat("PlayerLookAtCallbackDelay", 0);
	}
	tString sSep = ";, ";
	tStringVec vEnts, vFuncs;
	cString::GetStringVec(v.GetVarString("CC_Entities", ""), vEnts, &sSep);
	cString::GetStringVec(v.GetVarString("CC_Funcs", ""), vFuncs, &sSep);
	for (size_t i = 0; i < vEnts.size() && vFuncs.empty() == false; ++i)
		mvCollideCallbacks.push_back(cCollideCallback{cString::ToLowerCase(vEnts[i]) == "player" ? tString("Player") : vEnts[i],
													  vFuncs[std::min(i, vFuncs.size() - 1)]});
	msConnectionCallback = v.GetVarString("ConnectionStateChangeCallback", "");
	if (v.GetVarString("ConnectedEntity", "") != "")
		mvConnections.push_back(cConnection{"", v.GetVarString("ConnectedEntity", ""), v.GetVarBool("ConnectedEntityInvertState", false),
											v.GetVarInt("ConnectedEntityStatesUsed", 0)});
	mfHealth = mVars.GetVarFloat("Health", 100);
	if (mVars.GetVarBool("BreakActive", false) && v.GetVarBool("DisableBreakable", false) == false)
	{
		mBreakCallback.mpEntity = this;
		for (iPhysicsBody *pBody : mvBodies)
			pBody->AddBodyCallback(&mBreakCallback);
	}
	if (v.GetVarFloat("MaxInteractDistance", 0) > 0)
		mfMaxInteractDistance = v.GetVarFloat("MaxInteractDistance", 0);
	if (v.GetVarBool("InteractionDisabled", false))
		mbInteractionDisabled = true;
}

// Fits position = origin + u * right + v * down over the submesh vertices, so GUI (0,0) is the
// UV (0,0) corner of the screen
void cSomaLuxEntity::SetupGuiScreen(const tString &asSubMesh)
{
	if (mpMesh == NULL)
		return;
	cSubMeshEntity *pSub = mpMesh->GetSubMeshEntityName(asSubMesh);
	for (int i = 0; pSub == NULL && i < mpMesh->GetSubMeshEntityNum(); ++i)
		if (cString::ToLowerCase(mpMesh->GetSubMeshEntity(i)->GetName()).find(cString::ToLowerCase(asSubMesh)) != tString::npos)
			pSub = mpMesh->GetSubMeshEntity(i);
	iVertexBuffer *pVtx = pSub ? pSub->GetSubMesh()->GetVertexBuffer() : NULL;
	if (pVtx == NULL)
		return;
	const float *pPos = pVtx->GetFloatArray(eVertexBufferElement_Position);
	const float *pUV = pVtx->GetFloatArray(eVertexBufferElement_Texture0);
	int lPosNum = pVtx->GetElementNum(eVertexBufferElement_Position);
	int lUVNum = pVtx->GetElementNum(eVertexBufferElement_Texture0);
	if (pPos == NULL || pUV == NULL)
		return;
	// Normal equations of [1 u v] x = p
	double A[3][3] = {}, B[3][3] = {};
	for (int i = 0; i < pVtx->GetVertexNum(); ++i)
	{
		double r[3] = {1, pUV[i * lUVNum], pUV[i * lUVNum + 1]};
		for (int a = 0; a < 3; ++a)
			for (int b = 0; b < 3; ++b)
			{
				A[a][b] += r[a] * r[b];
				B[a][b] += r[a] * pPos[i * lPosNum + b];
			}
	}
	double fDet = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) - A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) +
				  A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);
	if (std::abs(fDet) < 1e-12)
		return;
	double Inv[3][3];
	for (int a = 0; a < 3; ++a)
		for (int b = 0; b < 3; ++b)
		{
			int a1 = (b + 1) % 3, a2 = (b + 2) % 3, b1 = (a + 1) % 3, b2 = (a + 2) % 3;
			Inv[a][b] = (A[a1][b1] * A[a2][b2] - A[a1][b2] * A[a2][b1]) / fDet;
		}
	cVector3f vRows[3];
	for (int k = 0; k < 3; ++k)
		for (int c = 0; c < 3; ++c)
		{
			double x = 0;
			for (int j = 0; j < 3; ++j)
				x += Inv[k][j] * B[j][c];
			vRows[k].v[c] = (float)x;
		}
	mpGuiSubMesh = pSub;
	mvGuiOrigin = vRows[0];
	mvGuiRight = vRows[1];
	mvGuiDown = vRows[2];
}

void cSomaLuxEntity::UpdateGuiScreen()
{
	if (mpGuiSubMesh == NULL || mpImGui == NULL)
		return;
	cGuiSet *pSet = mpImGui->GetSet();
	cMatrixf mtx = mpGuiSubMesh->GetWorldMatrix();
	cVector3f vRight = cMath::MatrixMul3x3(mtx, mvGuiRight);
	cVector3f vDown = cMath::MatrixMul3x3(mtx, mvGuiDown);
	float fW = vRight.Length(), fH = vDown.Length();
	if (fW <= 0 || fH <= 0)
		return;
	cVector3f vX = vRight / fW, vY = vDown * (-1.0f / fH);
	cVector3f vZ = cMath::Vector3Normalize(cMath::Vector3Cross(vX, vY));
	cVector3f vOrigin = cMath::MatrixMul(mtx, mvGuiOrigin) + vZ * 0.002f;
	cMatrixf mtxScreen = cMath::MatrixUnitVectors(vX, vY, vZ, vOrigin);
	pSet->Set3DTransform(mtxScreen);
	pSet->Set3DSize(cVector3f(fW, fH, 0.001f));
	static cSomaGuiScreenRenderer gRenderer;
	gRenderer.Register();
}

// HPL2 render lists skip gui set renderables, so prop screens are drawn after the translucent pass
void cSomaGuiScreenRenderer::Register()
{
	cViewport *pViewport = gpSomaBase->GetCurrentViewport();
	if (pViewport == NULL || pViewport == mpViewport)
		return;
	pViewport->AddRendererCallback(this);
	mpViewport = pViewport;
}

void cSomaGuiScreenRenderer::OnPostTranslucentDraw(cRendererCallbackFunctions *apFunctions)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap == NULL)
		return;
	iLowLevelGraphics *pLowLevel = gpSomaBase->mpEngine->GetGraphics()->GetLowLevel();
	cFrustum *pFrustum = apFunctions->GetFrustum();
	apFunctions->SetProgram(NULL);
	apFunctions->SetTextureRange(NULL, 0);
	apFunctions->SetVertexBuffer(NULL);
	pLowLevel->SetMatrix(eMatrix_Projection, pFrustum->GetProjectionMatrix());
	for (cSomaLuxEntity *p : pMap->GetEntities())
	{
		if (p->mpGuiSubMesh == NULL || p->mpImGui == NULL || p->mbGuiActive == false || p->mbActive == false)
			continue;
		cGuiSet *pSet = p->mpImGui->GetSet();
		cVector3f vCorner = pSet->Get3DTransform().GetTranslation(), vReach = pSet->Get3DSize().Length() + 0.1f;
		cBoundingVolume bv;
		bv.SetPosition(vCorner);
		bv.SetSize(vReach * 2);
		if (pFrustum->CollideBoundingVolume(&bv) == eCollision_Outside)
			continue;
		pSet->Render(pFrustum);
	}
}

void cSomaLuxEntity::UpdateGui(float afTimeStep)
{
	UpdateGuiScreen();
	if (mpImGui == NULL || mbGuiActive == false || msOnGuiFunc == "" || mbActive == false)
		return;
	cSomaImGui *pPrev = cSomaImGui::GetCurrent();
	cSomaImGui::SetCurrent(mpImGui);
	mpImGui->Begin(afTimeStep);
	cSomaLuxMap *pMap = mpMap ? mpMap : cSomaLuxMap::GetCurrent();
	if (pMap && pMap->GetScript())
		cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void " + msOnGuiFunc + "(const tString&in, float)", [&](asIScriptContext *c) {
			c->SetArgObject(0, &msName);
			c->SetArgFloat(1, afTimeStep);
		});
	mpImGui->End();
	// Drawn into the set now: 3D sets render with the scene, before OnDraw
	if (mpGuiSubMesh)
		mpImGui->DrawAll();
	cSomaImGui::SetCurrent(pPrev);
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

void cSomaLuxEntity::SetEffectBaseColor(const cColor &aCol)
{
	if (mvEffectDefaults.empty())
	{
		for (iLight *pLight : mvLights)
			mvEffectDefaults.push_back(pLight->GetDiffuseColor());
		for (cBillboard *pBB : mvBillboards)
			mvEffectDefaults.push_back(pBB->GetColor());
	}
	mEffectBaseColor = aCol;
	size_t i = 0;
	for (iLight *pLight : mvLights)
		pLight->SetDiffuseColor(mvEffectDefaults[i++] * aCol);
	for (cBillboard *pBB : mvBillboards)
		pBB->SetColor(mvEffectDefaults[i++] * aCol);
}

void cSomaLuxEntity::FadeEffectBaseColor(const cColor &aCol, float afTime)
{
	if (afTime <= 0)
	{
		mfEffectColorTime = 0;
		SetEffectBaseColor(aCol);
		return;
	}
	mEffectColorFrom = mEffectBaseColor;
	mEffectColorTo = aCol;
	mfEffectColorTime = afTime;
	mfEffectColorT = 0;
}

void cSomaLuxEntity::UpdateEffectColor(float afTimeStep)
{
	if (mfEffectColorTime <= 0)
		return;
	mfEffectColorT = std::min(mfEffectColorT + afTimeStep / mfEffectColorTime, 1.0f);
	SetEffectBaseColor(mEffectColorFrom * (1 - mfEffectColorT) + mEffectColorTo * mfEffectColorT);
	if (mfEffectColorT >= 1)
		mfEffectColorTime = 0;
}

bool cSomaLuxEntity::CollidesWithPlayer()
{
	cSomaLuxMap *pMap = mpMap ? mpMap : cSomaLuxMap::GetCurrent();
	cSomaLuxEntity *pPlayer = pMap ? pMap->GetEntity("Player") : NULL;
	return pPlayer && mbActive && SomaEntitiesCollide(this, pPlayer);
}

void cSomaLuxEntity::SetHealth(float afX)
{
	mfHealth = afX;
	Call("void OnHealthChange()");
	if (mfHealth <= 0 && mInstanceVars.GetVarBool("DisableBreakable", false) == false)
		Break();
}

void cSomaLuxEntity::GiveDamage(float afAmount, int alStrength, const tString &asType, const tString &asSource)
{
	int lToughness = mVars.GetVarInt("Toughness", 0);
	if (alStrength < lToughness - 1)
		afAmount = 0;
	else if (alStrength == lToughness - 1)
		afAmount *= 0.5f;
	SetHealth(mfHealth - afAmount);
	Call("void GiveDamage(float, int, const tString&in, const tString&in)", [&](asIScriptContext *c) {
		c->SetArgFloat(0, afAmount);
		c->SetArgDWord(1, alStrength);
		c->SetArgObject(2, (void *)&asType);
		c->SetArgObject(3, (void *)&asSource);
	});
}

// Deferred like cLuxMap::DestroyEntity: runs after the entity update loop
void cSomaLuxEntity::Break()
{
	cSomaLuxMap *pMap = mpMap ? mpMap : cSomaLuxMap::GetCurrent();
	if (mbBroken || pMap == NULL)
		return;
	mbBroken = true;
	pMap->mvPendingBreaks.push_back(this);
}

// cLuxProp_Object::BeforePropDestruction
void cSomaLuxEntity::DoBreak()
{
	cSomaLuxMap *pMap = mpMap ? mpMap : cSomaLuxMap::GetCurrent();
	for (iPhysicsBody *pBody : mvBodies)
		pBody->RemoveBodyCallback(&mBreakCallback);
	if (mvBodies.empty() == false)
	{
		cWorld *pWorld = pMap->GetWorld();
		iPhysicsBody *pBase = mvBodies[0];
		tString sAlign = mVars.GetVarString("BreakEntityAlignBody", "");
		for (iPhysicsBody *pBody : mvBodies)
			if (sAlign != "" && pBody->GetName() == msName + "_" + sAlign)
				pBase = pBody;
		cMatrixf mtxCenter = pBase->GetLocalMatrix();
		float fImpulse = mVars.GetVarFloat("BreakImpulse", 3);
		if (mVars.GetVarBool("BreakDestroyJoints", false))
		{
			for (iPhysicsJoint *pJoint : mvJoints)
				if (pJoint) pJoint->Break();
			mvJoints.clear();
		}
		else if (mVars.GetVarString("BreakEntity", "") != "")
		{
			cVector3f vVel = pBase->GetLinearVelocity();
			cSomaLuxEntity *pNew = pMap->CreateEntity(msName + "_broken", mVars.GetVarString("BreakEntity", ""), mtxCenter, mvScale);
			if (pNew)
				for (iPhysicsBody *pBody : pNew->mvBodies)
				{
					cVector3f vCenter = cMath::MatrixMul(pBody->GetLocalMatrix(), pBody->GetMassCentre());
					pBody->AddImpulse(cMath::Vector3Normalize(vCenter - mtxCenter.GetTranslation()) * fImpulse + vVel);
				}
		}
		tString sSound = mVars.GetVarString("BreakSound", "");
		if (sSound != "")
			if (cSoundEntity *pSound = pWorld->CreateSoundEntity(msName + "_BreakSound", sSound, true))
				pSound->SetPosition(mtxCenter.GetTranslation());
		tString sPS = mVars.GetVarString("BreakParticleSystem", "");
		if (sPS != "")
			if (cParticleSystem *pPS = pWorld->CreateParticleSystem(msName + "_BreakPS", sPS, 1))
				pPS->SetMatrix(mtxCenter);
	}
	tString sCallback = mInstanceVars.GetVarString("OnBreakCallbackFunc", "");
	if (sCallback != "" && pMap->GetScript())
		cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void " + sCallback + "(const tString &in)", [&](asIScriptContext *c) { c->SetArgObject(0, &msName); });
	pMap->DestroyEntity(this);
}

// cLuxProp_Object_BodyCallback::OnBodyCollide
void cSomaLuxEntity::cBreakBodyCallback::OnBodyCollide(iPhysicsBody *apBody, iPhysicsBody *apCollideBody, cPhysicsContactData *apContactData)
{
	if (mpEntity->mbBroken || mpEntity->mbActive == false)
		return;
	float fEnergy = 0;
	for (iPhysicsBody *pBody : {apBody, apCollideBody})
	{
		if (pBody->GetMass() == 0)
			continue;
		cVector3f vVel = pBody->GetVelocityAtPosition(apContactData->mvContactPosition);
		fEnergy += std::fabs(cMath::Vector3Dot(apContactData->mvContactNormal, vVel)) * pBody->GetMass();
	}
	if (fEnergy > mpEntity->mVars.GetVarFloat("BreakMinEnergy", 1000))
		mpEntity->Break();
}

// iLuxEntity::ChangeConnectionState: the map callback, then every connected entity's script
void cSomaLuxEntity::ChangeConnectionState(int alState)
{
	cSomaLuxMap *pMap = mpMap ? mpMap : cSomaLuxMap::GetCurrent();
	if (pMap == NULL)
		return;
	if (msConnectionCallback != "" && pMap->GetScript())
		cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void " + msConnectionCallback + "(const tString &in, int)", [&](asIScriptContext *c) {
			c->SetArgObject(0, &msName);
			c->SetArgDWord(1, alState);
		});
	std::vector<cConnection> vConnections = mvConnections;
	for (const cConnection &conn : vConnections)
	{
		if (conn.mlStatesUsed != 0 && alState != conn.mlStatesUsed)
			continue;
		int lState = conn.mbInvert ? -alState : alState;
		for (cSomaLuxEntity *pEnt : pMap->GetEntities())
			if (pEnt != this && SomaWildcardMatch(conn.msEntity, pEnt->msName))
				pEnt->Call("void OnConnectionStateChange(iLuxEntity@ apEntity, int alState)", [&](asIScriptContext *c) {
					c->SetArgAddress(0, this);
					c->SetArgDWord(1, lState);
				});
	}
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
	cVector3f vCols[3];
	for (int i = 0; i < 3; ++i)
		vCols[i] = cVector3f(m.m[0][i], m.m[1][i], m.m[2][i]);
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

namespace
{
	struct cObjectEntry
	{
		void *mpObj;
		tString msType;
		int mlCreationID;
	};
	std::map<void *, cSomaID> gmapObjectToID;
	std::map<int32_t, cObjectEntry> gmapIDToObject;
	int32_t glNextObjectID = 1;

	bool IsEntity3DType(const tString &asType) { return asType != "iPhysicsJoint" && asType.compare(0, 13, "iPhysicsJoint") != 0 && asType != "iCharacterBody"; }

	// Script type -> the object as that HPL2 class, NULL when it is not one
	void *CastEntity3D(iEntity3D *e, const tString &asType)
	{
		if (asType == "iEntity3D") return e;
		if (asType == "cMeshEntity") return dynamic_cast<cMeshEntity *>(e);
		if (asType == "cSubMeshEntity") return dynamic_cast<cSubMeshEntity *>(e);
		if (asType == "iLight") return dynamic_cast<iLight *>(e);
		if (asType == "cLightPoint") return dynamic_cast<cLightPoint *>(e);
		if (asType == "cLightSpot") return dynamic_cast<cLightSpot *>(e);
		if (asType == "cLightBox") return dynamic_cast<cLightBox *>(e);
		if (asType == "cBillboard") return dynamic_cast<cBillboard *>(e);
		if (asType == "cBeam") return dynamic_cast<cBeam *>(e);
		if (asType == "cParticleSystem") return dynamic_cast<cParticleSystem *>(e);
		if (asType == "cGuiSetEntity") return dynamic_cast<cGuiSetEntity *>(e);
		if (asType == "cFogArea") return dynamic_cast<cFogArea *>(e);
		if (asType == "cSoundEntity") return dynamic_cast<cSoundEntity *>(e);
		if (asType == "iPhysicsBody") return dynamic_cast<iPhysicsBody *>(e);
		return NULL;
	}

	bool IsAlive(const cObjectEntry &aEntry)
	{
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		cWorld *pWorld = pMap ? pMap->GetWorld() : NULL;
		if (aEntry.msType == "cSoundEntity")
			return pWorld && pWorld->SoundEntityExists((cSoundEntity *)aEntry.mpObj, aEntry.mlCreationID);
		if (aEntry.msType == "cParticleSystem")
			return pWorld && pWorld->ParticleSystemExists((cParticleSystem *)aEntry.mpObj);
		return true;
	}
}

void SomaClearObjectIDs()
{
	gmapObjectToID.clear();
	gmapIDToObject.clear();
}

cSomaID SomaObjectID(void *apObj, const tString &asType)
{
	if (apObj == NULL)
		return cSomaID();
	auto it = gmapObjectToID.find(apObj);
	if (it != gmapObjectToID.end())
	{
		const cObjectEntry &entry = gmapIDToObject[it->second.mB];
		// Same object when alive and of the same kind; freed memory reused by a new object gets a new id
		if (IsAlive(entry) && IsEntity3DType(entry.msType) == IsEntity3DType(asType) &&
			(entry.msType == asType || IsEntity3DType(asType) == false || CastEntity3D((iEntity3D *)apObj, entry.msType) != NULL))
			return it->second;
		gmapIDToObject.erase(it->second.mB);
	}
	cSomaID id;
	id.mA = 0xfe;
	id.mB = glNextObjectID++;
	int lCreationID = asType == "cSoundEntity" ? ((cSoundEntity *)apObj)->GetCreationID() : 0;
	gmapObjectToID[apObj] = id;
	gmapIDToObject[id.mB] = cObjectEntry{apObj, asType, lCreationID};
	return id;
}

void *SomaObjectFromID(const cSomaID &aID, const tString &asType)
{
	if (aID.mA != 0xfe)
		return NULL;
	auto it = gmapIDToObject.find(aID.mB);
	if (it == gmapIDToObject.end() || IsAlive(it->second) == false)
		return NULL;
	const cObjectEntry &entry = it->second;
	if (entry.msType == asType)
		return entry.mpObj;
	if (IsEntity3DType(entry.msType) && IsEntity3DType(asType))
		return CastEntity3D((iEntity3D *)entry.mpObj, asType);
	if (asType == "iPhysicsJoint" && entry.msType.compare(0, 13, "iPhysicsJoint") == 0)
		return entry.mpObj;
	if (asType.compare(0, 13, "iPhysicsJoint") == 0 && entry.msType == "iPhysicsJoint")
	{
		iPhysicsJoint *j = (iPhysicsJoint *)entry.mpObj;
		if (asType == "iPhysicsJointHinge") return dynamic_cast<iPhysicsJointHinge *>(j);
		if (asType == "iPhysicsJointSlider") return dynamic_cast<iPhysicsJointSlider *>(j);
		if (asType == "iPhysicsJointBall") return dynamic_cast<iPhysicsJointBall *>(j);
	}
	return NULL;
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
	std::map<iPhysicsBody *, cSomaLuxEntity *> mapOwner;
	for (cSomaLuxEntity *pEnt : pMap->GetEntities())
		for (iPhysicsBody *pBody : pEnt->mvBodies)
		{
			mapOwner[pBody] = pEnt;
			// Rays starting inside an area hit it at once (whole-room tool use areas)
			if (pEnt->meType == eSomaLuxEntityType_Area && pEnt->mbActive && pBody->GetShape())
			{
				cVector3f vLocal = cMath::MatrixMul(cMath::MatrixInverse(pBody->GetWorldMatrix()), avStart);
				cVector3f vHalf = pBody->GetShape()->GetSize() * 0.5f;
				if (std::abs(vLocal.x) <= vHalf.x && std::abs(vLocal.y) <= vHalf.y && std::abs(vLocal.z) <= vHalf.z)
					ray.mvHits.push_back(std::make_pair(0.0f, pBody));
			}
		}
	std::sort(ray.mvHits.begin(), ray.mvHits.end(), [](auto &a, auto &b) { return a.first < b.first; });
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
		cVector3f vCols[3];
	for (int i = 0; i < 3; ++i)
		vCols[i] = cVector3f(m.m[0][i], m.m[1][i], m.m[2][i]);
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

bool SomaRayHitsEntity(cSomaLuxEntity *apEnt, const cVector3f &avStart, const cVector3f &avDir, float afMaxDist, float &afDistOut)
{
	std::vector<cSomaOBB> vBoxes;
	EntityBoxes(apEnt, vBoxes);
	bool bHit = false;
	afDistOut = afMaxDist;
	for (const cSomaOBB &b : vBoxes)
	{
		cVector3f vRel = avStart - b.mvCenter;
		float tMin = 0, tMax = afMaxDist;
		bool bOk = true;
		for (int i = 0; i < 3 && bOk; ++i)
		{
			float o = cMath::Vector3Dot(vRel, b.mvAxis[i]);
			float d = cMath::Vector3Dot(avDir, b.mvAxis[i]);
			if (std::fabs(d) < 1e-6f)
			{
				if (std::fabs(o) > b.mvHalf.v[i])
					bOk = false;
				continue;
			}
			float t1 = (-b.mvHalf.v[i] - o) / d, t2 = (b.mvHalf.v[i] - o) / d;
			if (t1 > t2)
				std::swap(t1, t2);
			tMin = std::max(tMin, t1);
			tMax = std::min(tMax, t2);
			if (tMin > tMax)
				bOk = false;
		}
		if (bOk && tMin < afDistOut)
		{
			afDistOut = tMin;
			bHit = true;
		}
	}
	return bHit;
}

class cSomaLosRay : public iPhysicsRayCallback
{
public:
	std::set<iPhysicsBody *> msetIgnore;
	bool mbBlocked = false;
	bool OnIntersect(iPhysicsBody *apBody, cPhysicsRayParams *apParams) override
	{
		if (apBody->IsCharacter() || apBody->GetCollide() == false || apBody->GetBlocksLight() == false || msetIgnore.count(apBody))
			return true;
		mbBlocked = true;
		return false;
	}
};

bool SomaLineOfSight(const cVector3f &avStart, const cVector3f &avEnd, cSomaLuxEntity *apIgnore)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap == NULL || pMap->GetWorld()->GetPhysicsWorld() == NULL || (avEnd - avStart).SqrLength() < 0.0001f)
		return true;
	cSomaLosRay ray;
	if (apIgnore)
		ray.msetIgnore.insert(apIgnore->mvBodies.begin(), apIgnore->mvBodies.end());
	// Stop short of the target surface
	cVector3f vEnd = avEnd - (avEnd - avStart) * (0.05f / std::max((avEnd - avStart).Length(), 0.05f));
	pMap->GetWorld()->GetPhysicsWorld()->CastRay(&ray, avStart, vEnd, false, false, false);
	return ray.mbBlocked == false;
}

bool SomaEntityCollidesAABB(cSomaLuxEntity *apEnt, const cVector3f &avMin, const cVector3f &avMax)
{
	std::vector<cSomaOBB> vA;
	EntityBoxes(apEnt, vA);
	cSomaOBB b = AABBToOBB(avMin, avMax);
	for (const cSomaOBB &a : vA)
		if (OBBOverlap(a, b))
			return true;
	return false;
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
	SOMA_METHOD_NEW(e, T, "const tString& GetClassName()", +[](E *p) -> const tString & {
		if (p->GetScript() == NULL)
			return p->msClassName;
		p->msScriptClassName = p->GetScript()->GetObjectType()->GetName();
		return p->msScriptClassName;
	});
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
	SOMA_METHOD_NEW(e, T, "int GetBodyIndexFromName(const tString&in asName)", +[](E *p, S n) {
		for (size_t i = 0; i < p->mvBodies.size(); ++i)
			if (p->mvBodies[i]->GetName() == n || SomaWildcardMatch("*_" + n, p->mvBodies[i]->GetName()))
				return (int)i;
		return -1;
	});
	SOMA_METHOD_NEW(e, T, "iPhysicsBody@ GetBodyFromID(int alID)", +[](E *p, int id) {
		for (iPhysicsBody *b : p->mvBodies)
			if (b->GetUniqueID() == id)
				return b;
		return (iPhysicsBody *)NULL;
	});

	SOMA_METHOD_NEW(e, T, "iPhysicsBody@ GetBodyFromName(const tString&in asName)", +[](E *p, S n) {
		for (iPhysicsBody *b : p->mvBodies)
			if (b->GetName() == n || cString::GetFileName(b->GetName()) == n || SomaWildcardMatch("*_" + n, b->GetName()))
				return b;
		return (iPhysicsBody *)NULL;
	});
	SOMA_METHOD_NEW(e, T, "cMeshEntity@ GetMeshEntity()", +[](E *p) { return p->mpMesh; });
	SOMA_METHOD_NEW(e, T, "int GetParentType()", +[](E *) { return 0; });
	SOMA_METHOD_NEW(e, T, "int GetJointNum()", +[](E *p) { return (int)p->mvJoints.size(); });
	SOMA_METHOD_NEW(e, T, "iPhysicsJoint@ GetJoint(int alIdx)", +[](E *p, int i) {
		return i >= 0 && i < (int)p->mvJoints.size() ? p->mvJoints[i] : (iPhysicsJoint *)NULL; });
	SOMA_METHOD_NEW(e, T, "void WakeUp()", +[](E *p) { for (iPhysicsBody *b : p->mvBodies) b->Enable(); });
	SOMA_METHOD_NEW(e, T, "void SetAutoSleep(bool abX)", +[](E *p, bool x) { for (iPhysicsBody *b : p->mvBodies) b->SetAutoDisable(x); });
	SOMA_METHOD_NEW(e, T, "void SetSaveDataIsUpdated(bool abX)", +[](E *, bool) {});
	SOMA_METHOD_NEW(e, T, "void EnableBodyCollisionCallback()", +[](E *) {});
	SOMA_METHOD_NEW(e, T, "iEntity3D@ GetAttachEntity()", +[](E *p) -> iEntity3D * {
		if (p->mpMesh)
			return p->mpMesh;
		if (p->GetMainBody())
			return p->GetMainBody();
		iCharacterBody *pBody = p->meType == eSomaLuxEntityType_Player && cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetCharacterBody() : NULL;
		return pBody ? pBody->GetCurrentBody() : NULL;
	});
	SOMA_METHOD_NEW(e, T, "cMaterial@ GetBaseMaterial()", +[](E *p) {
		return p->mpMesh && p->mpMesh->GetSubMeshEntityNum() > 0 ? p->mpMesh->GetSubMeshEntity(0)->GetMaterial() : (cMaterial *)NULL;
	});
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
	SOMA_METHOD_NEW(e, T, "void CreateAndSetupGui(tString asSubmesh, const cColor&in aColorMul, const cColor&in aClearColor, const cColor&in aOfflineClearColor, const cVector2f&in avScreenSize)",
					+[](E *p, tString sub, const cColor &, const cColor &, const cColor &, const cVector2f &size) {
						if (p->mpImGui)
							return;
						cGui *pGui = gpSomaBase->mpEngine->GetGui();
						cGuiSet *pSet = pGui->CreateSet(p->msName + "_gui", NULL);
						pSet->SetVirtualSize(size, -1000, 1000);
						p->mpImGui = new cSomaImGui(p->msName, pSet);
						p->SetupGuiScreen(sub);
						if (p->mpGuiSubMesh)
							pSet->SetIs3D(true);
						else
							Warning("SOMA: GUI submesh '%s' not found on '%s'\n", sub.c_str(), p->msName.c_str());
					});
	SOMA_METHOD_NEW(e, T, "void SetOnGuiFunction(const tString&in asFunction)", +[](E *p, S f) { p->msOnGuiFunc = f; });
	SOMA_METHOD_NEW(e, T, "void SetGuiActive(bool abX, float afFadeTime=0.0f)", +[](E *p, bool b, float) { p->mbGuiActive = b; });
	SOMA_METHOD_NEW(e, T, "bool IsGuiActive()", +[](E *p) { return p->mbGuiActive; });
	SOMA_METHOD_NEW(e, T, "bool HasActiveGui()", +[](E *p) { return p->mpImGui && p->mbGuiActive; });
	SOMA_METHOD_NEW(e, T, "bool SetGuiIsFocused(bool abX, bool abShowMouse=true)", +[](E *p, bool b, bool mouse) {
		if (p->mpImGui == NULL)
			return false;
		if (b)
			cSomaImGui::SetInputFocus(p->mpImGui, mouse);
		else if (cSomaImGui::GetInputFocus() == p->mpImGui)
			cSomaImGui::SetInputFocus(NULL, false);
		return true;
	});
	SOMA_METHOD_NEW(e, T, "bool IsGuiFocused()", +[](E *p) { return p->mpImGui && cSomaImGui::GetInputFocus() == p->mpImGui; });
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
	SOMA_METHOD_NEW(e, T, "void ChangeConnectionState(int alState)", +[](E *p, int l) { p->ChangeConnectionState(l); });
	SOMA_METHOD_NEW(e, T, "void SetConnectionStateChangeCallback(const tString &in asCallbackFunc)", +[](E *p, S f) { p->msConnectionCallback = f; });
	SOMA_METHOD_NEW(e, T, "void AddConnection(const tString&in asName, iLuxEntity @apEntity, bool abInvertStateSent, int alStatesUsed)",
					+[](E *p, S n, E *c, bool i, int l) { if (c) p->mvConnections.push_back(E::cConnection{n, c->msName, i, l}); });
	SOMA_METHOD_NEW(e, T, "void RemoveConnection(const tString&in asName)", +[](E *p, S n) {
		p->mvConnections.erase(std::remove_if(p->mvConnections.begin(), p->mvConnections.end(), [&](const E::cConnection &c) { return c.msName == n; }),
							   p->mvConnections.end());
	});
	SOMA_METHOD_NEW(e, T, "void RemoveAllConnections()", +[](E *p) { p->mvConnections.clear(); });
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

cSomaLuxEntity::~cSomaLuxEntity()
{
	SomaForgetCritter(this);
	SomaFreePropBlock("cLuxCritter", mpCritterProps);
}

void cSomaLuxEntity::RegisterNatives(asIScriptEngine *e)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
	SomaSetIndirectProps("cLuxCritter", (int)offsetof(cSomaLuxEntity, mpCritterProps));
#pragma GCC diagnostic pop
	const char *vTypes[] = {"iLuxEntity", "cLuxProp", "cLuxArea", "cLuxAgent", "cLuxCritter", "cLuxLiquidArea"};
	for (const char *pType : vTypes)
		if (e->GetTypeInfoByName(pType))
			RegisterEntityMethods(e, pType);

	for (const char *pType : {"cLuxProp", "cLuxArea"})
	{
		SOMA_METHOD(e, pType, "void MoveLinearTo(const cVector3f&in avGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed, const tString&in asCallback=\"\")",
					+[](cSomaLuxEntity *p, const cVector3f &g, float a, float m, float d, bool r, const tString &cb) { p->MoveLinearTo(g, a, m, d, r, cb); });
		SOMA_METHOD(e, pType, "void StopMove()", +[](cSomaLuxEntity *p) { p->mbMoving = false; p->mfMoveSpeed = 0; });
	}
	for (const char *pType : vTypes)
		if (e->GetTypeInfoByName(pType))
			SOMA_METHOD(e, pType, "void GiveDamage(float afAmount, int alStrength, const tString&in asType, const tString&in asSource)",
						+[](cSomaLuxEntity *p, float a, int l, const tString &t, const tString &s) { p->GiveDamage(a, l, t, s); });
	for (const char *pType : vTypes)
		if (e->GetTypeInfoByName(pType))
		{
			SOMA_METHOD(e, pType, "void SetEffectBaseColor(const cColor&in aColor)", +[](cSomaLuxEntity *p, const cColor &c) { p->mfEffectColorTime = 0; p->SetEffectBaseColor(c); });
			SOMA_METHOD(e, pType, "void FadeEffectBaseColor(const cColor &in aDestColor, float afTime)", +[](cSomaLuxEntity *p, const cColor &c, float t) { p->FadeEffectBaseColor(c, t); });
			SOMA_METHOD(e, pType, "bool CollidesWithPlayer()", +[](cSomaLuxEntity *p) { return p->CollidesWithPlayer(); });
			SOMA_METHOD(e, pType, "bool CheckCharacterCollision(iCharacterBody @apBody, cLuxMap @apMap)", +[](cSomaLuxEntity *p, iCharacterBody *b, void *) {
				if (b == NULL || p->mbActive == false)
					return false;
				if (cSomaLuxPlayer::Get() && b == cSomaLuxPlayer::Get()->GetCharacterBody())
					return p->CollidesWithPlayer();
				return SomaEntityCollidesAABB(p, b->GetPosition() - b->GetSize() * 0.5f, b->GetPosition() + b->GetSize() * 0.5f);
			});
		}
	SOMA_FUNC(e, "void Entity_SetEffectBaseColor(const tString &in asEntityName,const cColor&in aColor)",
			  +[](const tString &n, const cColor &c) { ForMatching(n, [&](cSomaLuxEntity *p) { p->mfEffectColorTime = 0; p->SetEffectBaseColor(c); }); });
	SOMA_FUNC(e, "void Entity_FadeEffectBaseColor(const tString &in asEntityName,const cColor&in aColor, float afTime)",
			  +[](const tString &n, const cColor &c, float t) { ForMatching(n, [&](cSomaLuxEntity *p) { p->FadeEffectBaseColor(c, t); }); });
	SOMA_FUNC(e, "void Entity_PlayAnimation(const tString &in asEntityName, const tString &in asAnimation, float afFadeTime=0.1f, bool abLoop=false, bool abPlayTransition=true, const tString &in asCallback = \"\")",
			  +[](const tString &n, const tString &a, float f, bool l, bool, const tString &cb) { ForMatching(n, [&](cSomaLuxEntity *p) { p->PlayAnimation(a, f, l, cb); }); });
	SOMA_METHOD(e, "cLuxProp", "void SetHealth(float afX)", +[](cSomaLuxEntity *p, float x) { p->SetHealth(x); });
	SOMA_METHOD(e, "cLuxProp", "float GetHealth()", +[](cSomaLuxEntity *p) { return p->mfHealth; });
	SOMA_METHOD(e, "cLuxProp", "void Break()", +[](cSomaLuxEntity *p) { p->Break(); });
	SOMA_FUNC(e, "void Prop_SetHealth(const tString &in asPropName, float afHealth)", +[](const tString &n, float x) { ForMatching(n, [x](cSomaLuxEntity *p) { p->SetHealth(x); }); });
	SOMA_FUNC(e, "void Prop_AddHealth(const tString &in asPropName, float afHealth)", +[](const tString &n, float x) { ForMatching(n, [x](cSomaLuxEntity *p) { p->SetHealth(p->mfHealth + x); }); });
	SOMA_FUNC(e, "float Prop_GetHealth(const tString &in asPropName)", +[](const tString &n) { cSomaLuxEntity *p = Find(n); return p ? p->mfHealth : 0.0f; });
	if (e->GetTypeInfoByName("cLuxArea"))
	{
		SOMA_METHOD(e, "cLuxArea", "iPhysicsBody@ GetAreaBody()", +[](cSomaLuxEntity *p) { return p->GetMainBody(); });
		SOMA_METHOD(e, "cLuxArea", "const cVector3f& GetSize()", +[](cSomaLuxEntity *p) -> const cVector3f & { return p->mvSize; });
	}
	if (e->GetTypeInfoByName("cLuxProp"))
	{
		SOMA_METHOD(e, "cLuxProp", "void SetStaticPhysics(bool abX)", +[](cSomaLuxEntity *p, bool b) { p->SetStaticPhysics(b); });
		SOMA_METHOD(e, "cLuxProp", "bool GetStaticPhysics()", +[](cSomaLuxEntity *p) { return p->mbStaticPhysics; });
	}
	SOMA_FUNC(e, "void Prop_SetStaticPhysics(const tString &in asPropName, bool abX)",
			  +[](const tString &n, bool b) { ForMatching(n, [b](cSomaLuxEntity *p) { p->SetStaticPhysics(b); }); });

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
	SOMA_FUNC(e, "iPhysicsBody@ cLux_GetClosestBody(const cVector3f&in avStart,const cVector3f&in avDir, float afRayLength, float &out afDistance, cVector3f &out avSurfaceNormal)",
			  +[](const cVector3f &st, const cVector3f &dir, float len, float &fDist, cVector3f &vNormal) -> iPhysicsBody * {
				  struct cRay : iPhysicsRayCallback
				  {
					  iPhysicsBody *mpBody = NULL;
					  float mfDist = 0;
					  cVector3f mvNormal;
					  bool OnIntersect(iPhysicsBody *apBody, cPhysicsRayParams *apParams) override
					  {
						  if (apBody->GetCollide() && apBody->IsCharacter() == false && (mpBody == NULL || apParams->mfDist < mfDist))
						  {
							  mpBody = apBody;
							  mfDist = apParams->mfDist;
							  mvNormal = apParams->mvNormal;
						  }
						  return true;
					  }
				  } ray;
				  fDist = 0;
				  vNormal = 0;
				  cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
				  if (pMap == NULL || pMap->GetWorld()->GetPhysicsWorld() == NULL)
					  return NULL;
				  pMap->GetWorld()->GetPhysicsWorld()->CastRay(&ray, st, st + dir * len, true, true, false);
				  fDist = ray.mfDist;
				  vNormal = ray.mvNormal;
				  return ray.mpBody;
			  });
	SOMA_FUNC(e, "iLuxEntity@ cLux_ID_Entity(tID aID)", +[](cSomaID id) { return cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(id) : (cSomaLuxEntity *)NULL; });
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
				new (g->GetAddressOfReturnLocation()) cSomaID(SomaObjectID(g->GetObject(), g->GetFunction()->GetObjectType()->GetName()));
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
			e->RegisterGlobalFunction(pDecl, asFUNCTION(+[](asIScriptGeneric *g) {
				asITypeInfo *pRet = g->GetEngine()->GetTypeInfoById(g->GetFunction()->GetReturnTypeId());
				*(void **)g->GetAddressOfReturnLocation() = SomaObjectFromID(*(cSomaID *)g->GetArgObject(0), pRet ? pRet->GetName() : "");
			}), asCALL_GENERIC);

	SOMA_FUNC(e, "cLuxProp@ cLux_ID_Prop(tID aID)", +[](cSomaID id) { cSomaLuxEntity *p = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(id) : NULL; return p && p->meType == eSomaLuxEntityType_Prop ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "cLuxArea@ cLux_ID_Area(tID aID)", +[](cSomaID id) { cSomaLuxEntity *p = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(id) : NULL; return p && p->meType == eSomaLuxEntityType_Area ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "cLuxLiquidArea@ cLux_ID_LiquidArea(tID aID)", +[](cSomaID id) { cSomaLuxEntity *p = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(id) : NULL; return p && p->meType == eSomaLuxEntityType_LiquidArea ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "cLuxCritter@ cLux_ID_Critter(tID aID)", +[](cSomaID id) { cSomaLuxEntity *p = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(id) : NULL; return p && p->meType == eSomaLuxEntityType_Critter ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "cLuxAgent@ cLux_ID_Agent(tID aID)", +[](cSomaID id) { cSomaLuxEntity *p = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(id) : NULL; return p && p->meType == eSomaLuxEntityType_Agent ? p : (cSomaLuxEntity *)NULL; });
	SOMA_FUNC(e, "iLuxEntity@ cLux_GetBodyEntity(iPhysicsBody @apBody)", +[](iPhysicsBody *b) {
		if (b && cSomaLuxMap::GetCurrent())
			for (cSomaLuxEntity *p : cSomaLuxMap::GetCurrent()->GetEntities())
				for (iPhysicsBody *pb : p->mvBodies)
					if (pb == b)
						return p;
		return (cSomaLuxEntity *)NULL;
	});
	SOMA_FUNC(e, "bool Entity_GetCollide(const tString &in asEntityA, const tString &in asEntityB)", +[](S a, S b) {
		cSomaLuxEntity *pA = Find(a), *pB = Find(b);
		return pA && pB && SomaEntitiesCollide(pA, pB);
	});
	SOMA_FUNC(e, "cSubMeshEntity@ cScene_ToSubMeshEntity(iEntity3D@ apEntity)", +[](iEntity3D *p) { return dynamic_cast<cSubMeshEntity *>(p); });
	SOMA_FUNC(e, "cMeshEntity@ cScene_ToMeshEntity(iEntity3D@ apEntity)", +[](iEntity3D *p) { return dynamic_cast<cMeshEntity *>(p); });
	SOMA_FUNC(e, "cBillboard@ cScene_ToBillboard(iEntity3D@ apEntity)", +[](iEntity3D *p) { return dynamic_cast<cBillboard *>(p); });
	SOMA_FUNC(e, "cBeam@ cScene_ToBeam(iEntity3D@ apEntity)", +[](iEntity3D *p) { return dynamic_cast<cBeam *>(p); });
	SOMA_FUNC(e, "cSoundEntity@ cScene_ToSoundEntity(iEntity3D@ apEntity)", +[](iEntity3D *p) { return dynamic_cast<cSoundEntity *>(p); });
	SOMA_FUNC(e, "cLightBox@ cScene_ToLightBox(iLight@ apLight)", +[](iLight *p) { return dynamic_cast<cLightBox *>(p); });
	SOMA_FUNC(e, "cLightPoint@ cScene_ToLightPoint(iLight@ apLight)", +[](iLight *p) { return dynamic_cast<cLightPoint *>(p); });
	SOMA_FUNC(e, "cLightSpot@ cScene_ToLightSpot(iLight@ apLight)", +[](iLight *p) { return dynamic_cast<cLightSpot *>(p); });
	SOMA_FUNC(e, "iPhysicsJointHinge@ cPhysics_ToJointHinge(iPhysicsJoint@ apJoint)", +[](iPhysicsJoint *j) { return dynamic_cast<iPhysicsJointHinge *>(j); });
	SOMA_FUNC(e, "iPhysicsJointSlider@ cPhysics_ToJointSlider(iPhysicsJoint@ apJoint)", +[](iPhysicsJoint *j) { return dynamic_cast<iPhysicsJointSlider *>(j); });
	SOMA_FUNC(e, "iPhysicsJointBall@ cPhysics_ToJointBall(iPhysicsJoint@ apJoint)", +[](iPhysicsJoint *j) { return dynamic_cast<iPhysicsJointBall *>(j); });
	for (const char *pJoint : {"iPhysicsJoint", "iPhysicsJointHinge", "iPhysicsJointSlider", "iPhysicsJointBall"})
		SOMA_METHOD_NEW(e, pJoint, "ePhysicsJointType GetType()", +[](iPhysicsJoint *j) { return (int)j->GetType(); });
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
	SOMA_FUNC(e, "void Entity_Connect(const tString &in asName, const tString &in asMainEntity, const tString &in asConnectEntity, bool abInvertStateSent, int alStatesUsed)",
			  +[](S n, S m, S c, bool i, int l) { ForMatching(m, [&](cSomaLuxEntity *p) { p->mvConnections.push_back(cSomaLuxEntity::cConnection{n, c, i, l}); }); });
	SOMA_FUNC(e, "void Entity_RemoveConnection(const tString &in asName, const tString &in asMainEntity)", +[](S n, S m) {
		ForMatching(m, [&](cSomaLuxEntity *p) {
			auto &v = p->mvConnections;
			v.erase(std::remove_if(v.begin(), v.end(), [&](const cSomaLuxEntity::cConnection &c) { return c.msName == n; }), v.end());
		});
	});
	SOMA_FUNC(e, "void Entity_RemoveAllConnections(const tString &in asMainEntity)", +[](S m) { ForMatching(m, [](cSomaLuxEntity *p) { p->mvConnections.clear(); }); });
	SOMA_FUNC(e, "void Entity_SetConnectionStateChangeCallback(const tString &in asEntityName, const tString &in asCallback)",
			  +[](S n, S f) { ForMatching(n, [&](cSomaLuxEntity *p) { p->msConnectionCallback = f; }); });
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
	SOMA_FUNC(e, "void Prop_MoveLinearTo(const tString &in asName, const tString &in asTargetEntity, float afAcceleration, float afMaxSpeed, float afSlowDownDist, bool abResetSpeed, const tString&in asCallback=\"\")",
			  +[](S n, S t, float a, float m, float d, bool r, S cb) {
				  cSomaLuxEntity *pTarget = Find(t);
				  if (pTarget == NULL)
					  return;
				  cVector3f vGoal = pTarget->GetPosition();
				  ForMatching(n, [&](cSomaLuxEntity *p) { p->MoveLinearTo(vGoal, a, m, d, r, cb); });
			  });
	SOMA_FUNC(e, "void Prop_StopMovement(const tString &in asPropName)",
			  +[](S n) { ForMatching(n, [](cSomaLuxEntity *p) { p->mbMoving = false; p->mfMoveSpeed = 0; }); });
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
