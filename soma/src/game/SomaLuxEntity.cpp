#include "SomaLuxEntity.h"
#include "SomaScriptApi.h"
#include "scene/GuiSetEntity.h"
#include "graphics/MaterialType_BasicTranslucent.h"
#include "SomaLuxPlayer.h"
#include "SomaImGui.h"
#include "SomaBase.h"
#include <algorithm>
#include "SomaLuxGame.h"
#include "SomaAgent.h"
#include "SomaCritter.h"

#include <cmath>
#include <random>
#include <set>
#include "SomaLux.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"

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

std::vector<iPhysicsJoint *> &cSomaLuxEntity::Joints()
{
	// broken joints are deleted by the physics world, which only unlinks them from their bodies
	std::erase_if(mvJoints, [&](iPhysicsJoint *j) {
		for (iPhysicsBody *b : mvBodies)
			for (int i = 0; i < b->GetJointNum(); ++i)
				if (b->GetJoint(i) == j)
					return j->IsBroken();
		return true;
	});
	return mvJoints;
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
	if (abX == false && mbCameraInLiquid)
	{
		mbCameraInLiquid = false;
		if (--glSomaUnderwaterUsers <= 0)
			gbSomaUnderwaterEffects = false;
	}
	if (mpMesh)
	{
		mpMesh->SetActive(abX);
		mpMesh->SetVisible(abX && mbShowMesh);
	}
	if (meType == eSomaLuxEntityType_LiquidArea)
	{
		m_mtxLiquidPlaced = cMatrixf::Zero;
		PlaceLiquidGraphics();
	}
	for (iPhysicsBody *pBody : mvBodies)
		pBody->SetActive(abX);
	float fAlpha = mfEffectsAlpha;
	SetEffectsActive(abX && mbEffectsActive);
	if (mbEffectsActive)
	{
		mfEffectsAlpha = fAlpha > 0 ? fAlpha : 1.0f;
		mfEffectsFadeSpeed = 0;
		ApplyEffectsAlpha();
	}
	Call("void OnSetActive(bool abX)", [abX](asIScriptContext *c) { c->SetArgByte(0, abX); });
	SomaAgentSetActive(this, abX);
}

// cLuxPropLightConnection: one per light, combining every prop connected to it
struct cSomaLightConnection
{
	struct cProp
	{
		cSomaLuxEntity *mpEnt;
		float mfAmount;
		bool mbUseOnColor, mbUseSpec, mbMul;
	};
	iLight *mpLight;
	cColor mBaseColor;
	std::vector<cProp> mvProps;
};
static std::vector<cSomaLightConnection> gvLightConnections;

void cSomaLuxEntity::ResolveConnectedLights()
{
	if (mpMap == NULL) return;
	for (const char *pPrefix : {"", "Extra"})
	{
		tString sPrefix = pPrefix;
		tString *pNames = mInstanceVars.GetUserVariable(sPrefix + "ConnectedLight");
		if (pNames == NULL || pNames->empty()) continue;
		tStringVec vPatterns;
		cString::GetStringVec(*pNames, vPatterns, NULL);
		cSomaLightConnection::cProp prop{this, mInstanceVars.GetVarFloat(sPrefix + "ConnectionLightAmount", 1),
										 mInstanceVars.GetVarBool(sPrefix + "ConnectionLightUseOnColor", false),
										 mInstanceVars.GetVarBool(sPrefix + "ConnectionLightUseSpec", false),
										 mInstanceVars.GetVarString(sPrefix + "ConnectionLightType", "Add") != "Add"};
		cLightListIterator it = mpMap->GetWorld()->GetLightIterator();
		while (it.HasNext())
		{
			iLight *pLight = it.Next();
			for (const tString &sPattern : vPatterns)
				if (cString::MatchesWildcard(sPattern, pLight->GetName()))
				{
					auto conn = std::find_if(gvLightConnections.begin(), gvLightConnections.end(),
											 [pLight](const cSomaLightConnection &c) { return c.mpLight == pLight; });
					if (conn == gvLightConnections.end())
						conn = gvLightConnections.insert(gvLightConnections.end(), cSomaLightConnection{pLight, pLight->GetDiffuseColor(), {}});
					conn->mvProps.push_back(prop);
					break;
				}
		}
	}
}

void SomaUpdateLightConnections()
{
	for (cSomaLightConnection &conn : gvLightConnections)
	{
		cColor add(0, 0);
		float fMulSum = 0, fMulAcc = 0;
		bool bMul = false;
		for (const cSomaLightConnection::cProp &p : conn.mvProps)
		{
			cSomaLuxEntity *pEnt = p.mpEnt;
			if (p.mbMul)
			{
				fMulSum += p.mfAmount;
				fMulAcc += p.mfAmount * pEnt->mfEffectsAlpha;
				bMul = true;
				continue;
			}
			cColor col;
			if (pEnt->mvLights.empty() == false)
				col = p.mbUseOnColor ? pEnt->mvEffectDefaults[0] * pEnt->mEffectBaseColor * pEnt->mfEffectsAlpha : pEnt->mvLights[0]->GetDiffuseColor();
			else if (pEnt->mvBillboards.empty() == false)
				col = pEnt->mvBillboards[0]->GetColor();
			else
				col = pEnt->mInstanceVars.GetVarColor("IllumColor", cColor(1, 1)) * pEnt->mfEffectsAlpha;
			if (p.mbUseSpec == false)
				col.a = 0;
			add = add + col * p.mfAmount;
		}
		cColor col = (bMul ? conn.mBaseColor * (1 - fMulSum + fMulAcc) : conn.mBaseColor) + add;
		conn.mpLight->SetDiffuseColor(col);
	}
}

static void SetLookAtCallback(cSomaLuxEntity *p, const tString &f, bool r, bool c, bool ray, float d, float t)
{
	p->msLookAtCallback = f;
	p->mbLookAtCallbackAutoRemove = r;
	p->mbLookAtCheckCenter = c;
	p->mbLookAtCheckRay = ray;
	p->mfLookAtMaxDistance = d;
	p->mfLookAtDelay = t;
	p->mfLookAtTime = 0;
}

static void ForgetLightConnections(cSomaLuxEntity *apEnt)
{
	for (cSomaLightConnection &conn : gvLightConnections)
		std::erase_if(conn.mvProps, [apEnt](const cSomaLightConnection::cProp &p) { return p.mpEnt == apEnt; });
	std::erase_if(gvLightConnections, [](const cSomaLightConnection &c) { return c.mvProps.empty(); });
}

void cSomaLuxEntity::SetEffectsActive(bool abX, bool abFade)
{
	CaptureEffectDefaults();
	float fTime = abFade ? mVars.GetVarFloat(abX ? "EffectsOnTime" : "EffectsOffTime", 1) : 0;
	mfEffectsFadeSpeed = fTime > 0 ? (abX ? 1 : -1) / fTime : 0;
	if (mfEffectsFadeSpeed == 0)
		mfEffectsAlpha = abX ? 1.0f : 0.0f;
	for (cParticleSystem *pPS : mvParticleSystems)
		if (pPS)
		{
			pPS->SetVisible(abX);
			pPS->SetActive(abX);
		}
	for (cSoundEntity *pSound : mvSoundEntities)
	{
		if (abX)
			abFade ? pSound->FadeIn(mfEffectsFadeSpeed) : pSound->Play(false);
		else
			abFade && mfEffectsFadeSpeed != 0 ? pSound->FadeOut(-mfEffectsFadeSpeed) : pSound->Stop(false);
	}
	ApplyEffectsAlpha();
}

void cSomaLuxEntity::CaptureEffectDefaults()
{
	if (mvEffectDefaults.empty() == false)
		return;
	for (iLight *pLight : mvLights)
		mvEffectDefaults.push_back(pLight->GetDiffuseColor());
	for (cBillboard *pBB : mvBillboards)
		mvEffectDefaults.push_back(pBB->GetColor());
}

void cSomaLuxEntity::ApplyEffectsAlpha()
{
	CaptureEffectDefaults();
	bool bOn = mfEffectsAlpha > 0 && mbActive;
	size_t i = 0;
	for (iLight *pLight : mvLights)
	{
		cColor col = mvEffectDefaults[i++] * mEffectBaseColor * mfEffectsAlpha;
		bool bLit = bOn && col.r + col.g + col.b > 0;
		pLight->SetDiffuseColor(col);
		pLight->SetVisible(bLit);
		pLight->SetActive(bLit);
	}
	for (cBillboard *pBB : mvBillboards)
	{
		pBB->SetColor(mvEffectDefaults[i++] * mEffectBaseColor * mfEffectsAlpha);
		pBB->SetVisible(bOn);
		pBB->SetActive(bOn);
	}
	if (mpMesh)
		mpMesh->SetIlluminationColor(mInstanceVars.GetVarColor("IllumColor", cColor(1, 1)) * mEffectBaseColor * mfEffectsAlpha);
}

cMatrixf cSomaLuxEntity::GetMatrix()
{
	if (meType == eSomaLuxEntityType_Player)
	{
		iCharacterBody *pBody = cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetCharacterBody() : NULL;
		return pBody ? cMath::MatrixTranslate(pBody->GetFeetPosition()) : m_mtxOnLoad;
	}
	cMatrixf mtxAgent;
	if (meType == eSomaLuxEntityType_Agent && SomaAgentGetMatrix(this, mtxAgent))
		return mtxAgent;
	if (mbCameraProxy && cSomaLuxPlayer::Get() && cSomaLuxPlayer::Get()->GetCamera())
		return cMath::MatrixInverse(cSomaLuxPlayer::Get()->GetCamera()->GetViewMatrix());
	if (iPhysicsBody *pBody = GetMainBody())
		return pBody->GetLocalMatrix();
	if (mpMesh)
		return mpMesh->GetWorldMatrix();
	return m_mtxOnLoad;
}

cVector3f cSomaLuxEntity::GetPosition() { return GetMatrix().GetTranslation(); }

void cSomaLuxEntity::MakeDynamic()
{
	cWorld *pWorld = mpMesh->GetWorld();
	pWorld->MakeMeshEntityDynamic(mpMesh);
	for (iLight *pLight : mvLights)
		pWorld->MakeRenderableDynamic(pLight);
	for (cBillboard *pBillboard : mvBillboards)
		pWorld->MakeRenderableDynamic(pBillboard);
}

void cSomaLuxEntity::SetMatrix(const cMatrixf &a_mtx)
{
	if (meType == eSomaLuxEntityType_Player)
	{
		if (iCharacterBody *pBody = cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetCharacterBody() : NULL)
			pBody->SetFeetPosition(a_mtx.GetTranslation(), true);
		return;
	}
	if (meType == eSomaLuxEntityType_Agent && SomaAgentSetMatrix(this, a_mtx))
		return;
	if (mpMesh && mpMesh->IsStatic() && mpMesh->GetWorld() && GetMatrix() != a_mtx)
		MakeDynamic();
	if (iPhysicsBody *pBody = GetMainBody())
	{
		cMatrixf mtxInvMain = cMath::MatrixInverse(pBody->GetLocalMatrix());
		for (iPhysicsBody *b : mvBodies)
			if (b != pBody)
				b->SetMatrix(cMath::MatrixMul(a_mtx, cMath::MatrixMul(mtxInvMain, b->GetLocalMatrix())));
		pBody->SetMatrix(a_mtx);
	}
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
		if (asName.empty() == false)
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

void cSomaLuxEntity::MoveAngularTo(const cMatrixf &a_mtxGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed, const cVector3f &avPivotLocal,
								   const tString &asCallback)
{
	mlRotateMode = 1;
	m_mtxRotateGoal = a_mtxGoal.GetRotation();
	mfRotateAcc = afAcc;
	mfRotateMaxSpeed = afMaxSpeed;
	mfRotateSlowdown = afSlowdownDist;
	if (abResetSpeed)
		mfRotateSpeed = 0;
	mvPivotLocal = avPivotLocal;
	msRotateCallback = asCallback;
}

void cSomaLuxEntity::RotateAtSpeed(float afAcc, float afGoalSpeed, const cVector3f &avAxis, bool abResetSpeed, const cVector3f &avPivotLocal)
{
	mlRotateMode = 2;
	mfRotateAcc = afAcc;
	mfRotateMaxSpeed = afGoalSpeed;
	mvRotateAxis = avAxis;
	if (abResetSpeed)
		mfRotateSpeed = 0;
	mvPivotLocal = avPivotLocal;
}

void cSomaLuxEntity::StopMove()
{
	mbMoving = false;
	mfMoveSpeed = 0;
	mlRotateMode = 0;
	mfRotateSpeed = 0;
}

static void RunMoveCallback(cSomaLuxEntity *apEnt, tString &asCallback)
{
	tString sCallback = asCallback;
	asCallback = "";
	cSomaLuxMap *pMap = apEnt->mpMap ? apEnt->mpMap : cSomaLuxMap::GetCurrent();
	if (sCallback != "" && pMap && pMap->GetScript())
		cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void " + sCallback + "(const tString &in)", [&](asIScriptContext *c) { c->SetArgObject(0, &apEnt->msName); });
}

void cSomaLuxEntity::UpdateRotate(float afTimeStep)
{
	cMatrixf m = GetMatrix();
	cMatrixf mtxRot = m.GetRotation();
	cVector3f vPivot = m.GetTranslation() + cMath::MatrixMul3x3(mtxRot, mvPivotLocal);
	cMatrixf mtxStep;
	bool bDone = false;
	if (mlRotateMode == 2)
	{
		float fDiff = mfRotateMaxSpeed - mfRotateSpeed;
		mfRotateSpeed += cMath::Clamp(fDiff, -mfRotateAcc * afTimeStep, mfRotateAcc * afTimeStep);
		cVector3f vAxis = cMath::MatrixMul3x3(mtxRot, mvRotateAxis);
		if (vAxis.SqrLength() < 1e-8f || mfRotateSpeed == 0)
			return;
		cQuaternion q;
		q.FromAngleAxis(mfRotateSpeed * afTimeStep, cMath::Vector3Normalize(vAxis));
		mtxStep = cMath::MatrixQuaternion(q);
	}
	else
	{
		cQuaternion qCur, qGoal;
		qCur.FromRotationMatrix(mtxRot);
		qGoal.FromRotationMatrix(m_mtxRotateGoal);
		float fAngle = 2 * std::acos(cMath::Min(std::fabs(cMath::QuaternionDot(qCur, qGoal)), 1.0f));
		mfRotateSpeed = cMath::Min(mfRotateSpeed + mfRotateAcc * afTimeStep, mfRotateMaxSpeed);
		float fSpeed = mfRotateSpeed;
		if (mfRotateSlowdown > 0 && fAngle < mfRotateSlowdown)
			fSpeed = cMath::Min(fSpeed, cMath::Max(mfRotateMaxSpeed * fAngle / mfRotateSlowdown, mfRotateMaxSpeed * 0.05f));
		float fStep = fSpeed * afTimeStep;
		bDone = fStep >= fAngle;
		cQuaternion qNew = bDone ? qGoal : cMath::QuaternionSlerp(fStep / fAngle, qCur, qGoal, true);
		mtxStep = cMath::MatrixMul(cMath::MatrixQuaternion(qNew), cMath::MatrixInverse(mtxRot));
	}
	cMatrixf mtxNew = cMath::MatrixMul(mtxStep, mtxRot);
	mtxNew.SetTranslation(vPivot + cMath::MatrixMul3x3(mtxStep, m.GetTranslation() - vPivot));
	SetMatrix(mtxNew);
	if (bDone)
	{
		mlRotateMode = 0;
		mfRotateSpeed = 0;
		RunMoveCallback(this, msRotateCallback);
	}
}

void cSomaLuxEntity::UpdateMove(float afTimeStep)
{
	if (mlRotateMode)
		UpdateRotate(afTimeStep);
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
	RunMoveCallback(this, msMoveCallback);
}

void cSomaLuxEntity::ApplyInstanceVars(cSomaLuxEntity *apPlayer)
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
	// iLuxEntity::SetupCollideCallbacks: "player" goes on the player's container, this entity as child
	for (size_t i = 0; i < vEnts.size() && vFuncs.empty() == false; ++i)
	{
		const tString &sFunc = vFuncs[std::min(i, vFuncs.size() - 1)];
		if (cString::ToLowerCase(vEnts[i]) != "player")
			mvCollideCallbacks.push_back(cCollideCallback{vEnts[i], sFunc});
		else if (apPlayer && apPlayer != this)
			apPlayer->mvCollideCallbacks.push_back(cCollideCallback{msName, sFunc});
	}
	if (v.GetVarString("UserVar", "") != "")
		mmapScriptVars[""] = v.GetVarString("UserVar", "");
	msConnectionCallback = v.GetVarString("ConnectionStateChangeCallback", "");
	tStringVec vConnected;
	cString::GetStringVec(v.GetVarString("ConnectedEntity", ""), vConnected, &sSep);
	for (const tString &sEnt : vConnected)
		mvConnections.push_back(cConnection{"", sEnt, v.GetVarBool("ConnectedEntityInvertState", false), v.GetVarInt("ConnectedEntityStatesUsed", 0)});
	mfHealth = mVars.GetVarFloat("Health", 100);
	if (mVars.GetVarBool("BreakActive", false) && v.GetVarBool("DisableBreakable", false) == false)
	{
		mBreakCallback.mpEntity = this;
		mBreakCallback.mfStartTime = mpMap ? mpMap->GetTime() : 0;
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

void cSomaLuxEntity::SetGuiActive(bool abX, float afFadeTime)
{
	mbGuiDirty = true;
	if (afFadeTime > 0)
	{
		mfGuiFadeSpeed = (abX ? 1 : -1) / afFadeTime;
		mbGuiActive |= abX;
		return;
	}
	mfGuiFadeSpeed = 0;
	mfGuiFade = abX;
	mbGuiActive = abX;
}

void cSomaLuxEntity::UpdateGuiScreen()
{
	if (mpGuiSubMesh == NULL || mpImGui == NULL)
		return;
	// The original hides the submesh and draws its own copy while the gui set entity is active
	bool bVisible = mbGuiActive && mpMesh->IsVisible();
	if (mpGuiSubMesh->GetVisibleVar() != bVisible)
		mpGuiSubMesh->SetVisible(bVisible);
	if (cMaterial *pMat = mpGuiSubMesh->GetCustomMaterial(); pMat && dynamic_cast<cMaterialType_Translucent *>(pMat->GetType()))
		((cMaterialType_Translucent_Vars *)pMat->GetVars())->mFadeColor.a = 1 - mfGuiFade * mfGuiFade;
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
	cSomaGuiScreenRenderer::Get()->Register();
}

cSomaGuiScreenRenderer *cSomaGuiScreenRenderer::Get()
{
	static cSomaGuiScreenRenderer gRenderer;
	return &gRenderer;
}

void cSomaGuiScreenRenderer::Register()
{
	cViewport *pViewport = gpSomaBase->GetCurrentViewport();
	if (pViewport == NULL || pViewport == mpViewport)
		return;
	pViewport->AddRendererCallback(this);
	mpViewport = pViewport;
}

void cSomaGuiScreenRenderer::Forget(cSomaLuxEntity *apEnt)
{
	auto it = mmapTargets.find(apEnt);
	if (it == mmapTargets.end())
		return;
	cGraphics *pGraphics = gpSomaBase->mpEngine->GetGraphics();
	pGraphics->DestroyFrameBuffer(it->second.mpBuffer);
	pGraphics->DestroyTexture(it->second.mpTexture);
	mmapTargets.erase(it);
}

// Like the original, the screen glass gets a material of its own whose diffuse is the gui render target
static void SetScreenMaterial(cSubMeshEntity *apSub, iTexture *apTexture, const tString &asName)
{
	cMaterial *pMat = apSub->GetCustomMaterial();
	if (pMat == NULL || pMat->GetName() != asName)
	{
		cMaterial *pOrig = apSub->GetMaterial();
		if (pOrig == NULL)
			return;
		pMat = hplNew(cMaterial, (asName, cString::To16Char(asName), gpSomaBase->mpEngine->GetGraphics(), gpSomaBase->mpEngine->GetResources(), pOrig->GetType()));
		pMat->SetAutoDestroyTextures(false);
		pMat->SetDepthTest(pOrig->GetDepthTest());
		pMat->SetBlendMode(pOrig->GetBlendMode());
		pMat->SetAlphaMode(pOrig->GetAlphaMode());
		pMat->SetPhysicsMaterial(pOrig->GetPhysicsMaterial());
		for (int i = 0; i < eMaterialTexture_LastEnum; ++i)
			pMat->SetTexture((eMaterialTexture)i, pOrig->GetTexture((eMaterialTexture)i));
		cResourceVarsObject *pVars = pOrig->GetVarsObject();
		pMat->LoadVariablesFromVarsObject(pVars);
		hplDelete(pVars);
		// cGuiSetEntity renders with UseFadeColor
		if (dynamic_cast<cMaterialType_Translucent *>(pMat->GetType()))
			((cMaterialType_Translucent_Vars *)pMat->GetVars())->mbFadeColor = true;
		pMat->IncUserCount();
		// a .ent override stays alive: the copy shares its textures
		apSub->SetCustomMaterial(pMat, false);
	}
	pMat->SetTexture(eMaterialTexture_Diffuse, apTexture);
	pMat->Compile();
}

cSomaGuiScreenRenderer::cTarget &cSomaGuiScreenRenderer::GetTarget(cSomaLuxEntity *apEnt, const cVector2l &avSize)
{
	cTarget &t = mmapTargets[apEnt];
	if (t.mpBuffer && t.mvSize == avSize)
		return t;
	cGraphics *pGraphics = gpSomaBase->mpEngine->GetGraphics();
	if (t.mpBuffer)
	{
		pGraphics->DestroyFrameBuffer(t.mpBuffer);
		pGraphics->DestroyTexture(t.mpTexture);
	}
	tString sName = "SomaScreen_" + apEnt->msName;
	t.mpTexture = pGraphics->CreateTexture(sName, eTextureType_2D, eTextureUsage_RenderTarget);
	t.mpTexture->SetUseMipMaps(true);
	t.mpTexture->SetsRGB(true);
	t.mpTexture->CreateFromRawData(cVector3l(avSize.x, avSize.y, 0), ePixelFormat_RGBA, NULL);
	t.mpTexture->SetWrapSTR(eTextureWrap_ClampToEdge);
	t.mpTexture->SetFilter(eTextureFilter_Trilinear);
	t.mpTexture->SetAnisotropyDegree(8);
	t.mpBuffer = pGraphics->CreateFrameBuffer(sName);
	t.mpBuffer->SetTexture2D(0, t.mpTexture);
	t.mpBuffer->CompileAndValidate();
	t.mvSize = avSize;
	t.mlGuiCalls = -1;
	SetScreenMaterial(apEnt->mpGuiSubMesh, t.mpTexture, sName);
	return t;
}

void cSomaGuiScreenRenderer::OnPostSolidDraw(cRendererCallbackFunctions *apFunctions)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap == NULL)
		return;
	cFrustum *pFrustum = apFunctions->GetFrustum();
	// Reflection passes share callbacks
	if (mpViewport == NULL || mpViewport->GetCamera() == NULL || pFrustum != mpViewport->GetCamera()->GetFrustum())
		return;
	iLowLevelGraphics *pLowLevel = gpSomaBase->mpEngine->GetGraphics()->GetLowLevel();
	std::vector<std::pair<cSomaLuxEntity *, cTarget *>> vScreens;
	for (cSomaLuxEntity *p : pMap->GetEntities())
	{
		if (p->mpGuiSubMesh == NULL || p->mpImGui == NULL || p->mbActive == false)
			continue;
		cGuiSet *pSet = p->mpImGui->GetSet();
		cBoundingVolume bv;
		bv.SetPosition(pSet->Get3DTransform().GetTranslation());
		bv.SetSize((pSet->Get3DSize().Length() + 0.1f) * 2);
		if (pFrustum->CollideBoundingVolume(&bv) == eCollision_Outside)
			continue;
		cVector2f vVirtual = pSet->GetVirtualSize();
		cTarget &t = GetTarget(p, cVector2l((int)vVirtual.x, (int)vVirtual.y));
		if (t.mlGuiCalls == p->mlGuiCalls && t.mbGuiActive == p->mbGuiActive)
			continue;
		t.mlGuiCalls = p->mlGuiCalls;
		t.mbGuiActive = p->mbGuiActive;
		vScreens.push_back(std::make_pair(p, &t));
	}
	if (vScreens.empty())
		return;

	// official gui: base shaders, sRGB target without encode (ForceLinearSpace)
	static iGpuProgram *vPrograms[2] = {};
	if (static bool bTried = false; bTried == false)
	{
		bTried = true;
		cGraphics *pGraphics = gpSomaBase->mpEngine->GetGraphics();
		cParserVarContainer vars;
		vars.Add("UseUv");
		vars.Add("UseColor");
		vPrograms[1] = pGraphics->CreateGpuProgramFromShaders("SomaGuiFlat", "gui_vtx.glsl", "gui_frag.glsl", &vars);
		vars.Add("UseDiffuse");
		vPrograms[0] = pGraphics->CreateGpuProgramFromShaders("SomaGuiDiffuse", "gui_vtx.glsl", "gui_frag.glsl", &vars);
	}
	iFrameBuffer *pPrevBuffer = pLowLevel->GetCurrentFrameBuffer();
	apFunctions->SetProgram(NULL);
	apFunctions->SetTextureRange(NULL, 0);
	apFunctions->SetVertexBuffer(NULL);
	for (auto &it : vScreens)
	{
		cSomaLuxEntity *p = it.first;
		cGuiSet *pSet = p->mpImGui->GetSet();
		apFunctions->SetFrameBuffer(it.second->mpBuffer, false);
		pLowLevel->SetClearColor(p->mbGuiActive ? p->mpImGui->mScreenClear : p->mpImGui->mScreenOfflineClear);
		pLowLevel->ClearFrameBuffer(eClearFrameBufferFlag_Color);
		if (p->mbGuiActive == false)
			continue;
		pSet->SetIs3D(false);
		pSet->SetFlipScreenY(true);
		pSet->ClearRenderObjects();
		p->mpImGui->DrawAll();
		pLowLevel->SetCullActive(false);
		pSet->SetPrograms(vPrograms[0], vPrograms[1]);
		pSet->Render(pFrustum);
		pSet->SetPrograms(NULL, NULL);
		pSet->SetFlipScreenY(false);
		pSet->SetIs3D(true);
		++p->mlGuiDraws;
	}
	apFunctions->SetFrameBuffer(pPrevBuffer, true);
	pLowLevel->SetClearColor(cColor(0, 0));
	for (auto &it : vScreens)
		it.second->mpTexture->AutoGenerateMipmaps();

	// The gui set changed GL state behind the renderer's cache
	apFunctions->SetDepthTest(false);
	apFunctions->SetDepthTest(true);
	apFunctions->SetDepthWrite(false);
	apFunctions->SetDepthWrite(true);
	apFunctions->SetBlendMode(eMaterialBlendMode_Add);
	apFunctions->SetBlendMode(eMaterialBlendMode_None);
	apFunctions->SetAlphaMode(eMaterialAlphaMode_Trans);
	apFunctions->SetAlphaMode(eMaterialAlphaMode_Solid);
	apFunctions->SetChannelMode(eMaterialChannelMode_None);
	apFunctions->SetChannelMode(eMaterialChannelMode_RGBA);
	apFunctions->SetCullActive(false);
	apFunctions->SetCullActive(true);
	apFunctions->SetTexture(0, NULL);
	static cMatrixf mtxDummy = cMatrixf::Identity;
	apFunctions->SetMatrix(&mtxDummy);
	apFunctions->SetMatrix(NULL);
	apFunctions->SetFlatProjection();
	apFunctions->SetNormalFrustumProjection();
}

void cSomaLuxEntity::UpdateGui(float afTimeStep)
{
	if (mfGuiFadeSpeed != 0)
	{
		mfGuiFade += mfGuiFadeSpeed * afTimeStep;
		if (mfGuiFade <= 0 || mfGuiFade >= 1)
		{
			mbGuiActive = mfGuiFade > 0;
			mfGuiFade = mbGuiActive ? 1 : 0;
			mfGuiFadeSpeed = 0;
		}
	}
	UpdateGuiScreen();
	if (mpImGui == NULL || mbGuiActive == false || msOnGuiFunc == "" || mbActive == false)
		return;
	if (cSomaImGui::GetScriptInputFocus() != mpImGui)
	{
		mfGuiTimeAcc += afTimeStep;
		if (mfGuiTimeAcc < 1.0f / cMath::Clamp(mfGuiFPS, 1.0f, 30.0f) && mbGuiDirty == false)
			return;
		afTimeStep = mfGuiTimeAcc;
		mfGuiTimeAcc = 0;
		mbGuiDirty = false;
		if (mbGuiUpdateWhenOutOfView == false && mpGuiSubMesh && mpGuiSubMesh->GetRenderFrameCount() != iRenderer::GetRenderFrameCount())
			return;
	}
	++mlGuiCalls;
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
	CaptureEffectDefaults();
	mEffectBaseColor = aCol;
	ApplyEffectsAlpha();
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
	if (mfEffectsFadeSpeed != 0)
	{
		mfEffectsAlpha = cMath::Clamp(mfEffectsAlpha + mfEffectsFadeSpeed * afTimeStep, 0.0f, 1.0f);
		if (mfEffectsAlpha == 0 || mfEffectsAlpha == 1)
			mfEffectsFadeSpeed = 0;
		ApplyEffectsAlpha();
	}
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
			for (iPhysicsJoint *pJoint : Joints())
				pJoint->Break();
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
	cSomaLuxMap *pMap = mpEntity->mpMap ? mpEntity->mpMap : cSomaLuxMap::GetCurrent();
	float fSpeed = apContactData->mfMaxContactNormalSpeed;
	if (mpEntity->mbBroken || mpEntity->mbActive == false || fSpeed <= 0 || pMap == NULL || pMap->GetTime() - mfStartTime <= 0.5)
		return;
	float fMass2 = apCollideBody->GetMass() > 0 ? apCollideBody->GetMass() : 1e6f;
	float fV1 = apBody->GetVelocityAtPosition(apContactData->mvContactPosition).Length();
	float fV2 = apCollideBody->GetVelocityAtPosition(apContactData->mvContactPosition).Length();
	float fMass = fV1 + fV2 > 0 ? apBody->GetMass() + (fMass2 - apBody->GetMass()) * (fV2 / (fV1 + fV2)) : apBody->GetMass();
	if (0.5f * fSpeed * fSpeed * fMass > mpEntity->mVars.GetVarFloat("BreakMinEnergy", 1000))
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
			if (pEnt != this && cString::MatchesWildcard(conn.msEntity, pEnt->msName))
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
	// Saved tIDs of runtime objects must not alias this session's objects
	int32_t glNextObjectID = (int32_t)(std::random_device{}() & 0x3fffffff) + 1;

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
		if (asType == "cLensFlare") return dynamic_cast<cLensFlare *>(e);
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

// Saved script vars hold body tIDs: same map, same ids
void SomaRegisterBodyIDs(const std::vector<cSomaLuxEntity *> &avEnts)
{
	int32_t l = 0;
	for (cSomaLuxEntity *p : avEnts)
		for (iPhysicsBody *b : p->mvBodies)
		{
			cSomaID id;
			id.mA = 0xfe;
			id.mB = --l;
			gmapObjectToID[b] = id;
			gmapIDToObject[id.mB] = cObjectEntry{b, "iPhysicsBody", 0};
		}
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
	{
		// Lights etc. have no liveness check: never dynamic_cast a concrete entry, it may be freed
		if (entry.msType != "iEntity3D" && entry.msType != "iLight")
			return asType == "iEntity3D" || (asType == "iLight" && entry.msType.compare(0, 6, "cLight") == 0) ? entry.mpObj : NULL;
		return CastEntity3D((iEntity3D *)entry.mpObj, asType);
	}
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

// cLuxMapHelper::GetClosestEntity: nearest entity along the ray, blocked by colliding world geometry

class cSomaClosestRay : public iPhysicsRayCallback
{
public:
	std::vector<std::pair<float, iPhysicsBody *>> mvHits;
	bool OnIntersect(iPhysicsBody *apBody, cPhysicsRayParams *apParams) override
	{
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
	for (cSomaLuxEntity *pEnt : pMap->GetEntities())
		if (pEnt->meType == eSomaLuxEntityType_Area && pEnt->mbActive)
			for (iPhysicsBody *pBody : pEnt->mvBodies)
			{
				// Rays starting inside an area hit it at once (whole-room tool use areas)
				if (pBody->GetShape() == NULL)
					continue;
				cVector3f vLocal = cMath::MatrixMul(cMath::MatrixInverse(pBody->GetWorldMatrix()), avStart);
				cVector3f vHalf = pBody->GetShape()->GetSize() * 0.5f;
				if (std::abs(vLocal.x) <= vHalf.x && std::abs(vLocal.y) <= vHalf.y && std::abs(vLocal.z) <= vHalf.z)
					ray.mvHits.push_back(std::make_pair(0.0f, pBody));
			}
	std::sort(ray.mvHits.begin(), ray.mvHits.end(), [](auto &a, auto &b) { return a.first < b.first; });
	for (auto &hit : ray.mvHits)
	{
		cSomaLuxEntity *pEnt = NULL;
		if (hit.second->IsCharacter())
		{
			iCharacterBody *pChar = hit.second->GetCharacterBody();
			if (pChar == NULL || pChar->GetUserData() == NULL)
				continue;
			pEnt = (cSomaLuxEntity *)pChar->GetUserData();
		}
		else
			for (cSomaLuxEntity *pOwner : pMap->GetEntities())
				if (std::find(pOwner->mvBodies.begin(), pOwner->mvBodies.end(), hit.second) != pOwner->mvBodies.end())
				{
					pEnt = pOwner;
					break;
				}
		// Static props are world geometry in the original, not lux entities
		bool bWorld = pEnt == NULL || pEnt->msClassName == "StaticProp" || pEnt->msClassName == "StaticCollider";
		if (bWorld)
		{
			if (hit.second->GetCollide() == false)
				continue;
			apBodyOut = hit.second;
			afDistOut = hit.first;
			return false;
		}
		if (pEnt->mbActive == false)
			continue;
		if (hit.second->GetCollide() == false && (pEnt->mbInteractionDisabled || pEnt->CanInteract(alType, hit.second) == false))
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
	std::erase_if(mvCollideCallbacks, [&](const cCollideCallback &c) { return cString::MatchesWildcard(asChild, c.msChild) || cString::MatchesWildcard(c.msChild, asChild); });
}

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

static cSomaOBB MatrixOBB(const cMatrixf &am, const cVector3f &avMin, const cVector3f &avMax)
{
	cSomaOBB box;
	box.mvCenter = cMath::MatrixMul(am, (avMin + avMax) * 0.5f);
	for (int i = 0; i < 3; ++i)
	{
		cVector3f vCol(am.m[0][i], am.m[1][i], am.m[2][i]);
		float fLen = vCol.Length();
		box.mvAxis[i] = fLen > 0 ? vCol / fLen : cVector3f(i == 0, i == 1, i == 2);
		box.mvHalf.v[i] = (avMax.v[i] - avMin.v[i]) * 0.5f * (fLen > 0 ? fLen : 1);
	}
	return box;
}

// iLuxCollideCallbackContainer::CheckEntityCollision tests the body shapes, not their AABBs
static void ShapeBoxes(iCollideShape *apShape, const cMatrixf &am, std::vector<cSomaOBB> &avOut)
{
	if (apShape->GetType() == eCollideShapeType_Compound)
	{
		for (int i = 0; i < apShape->GetSubShapeNum(); ++i)
			ShapeBoxes(apShape->GetSubShape(i), am, avOut);
		return;
	}
	if (apShape->GetType() == eCollideShapeType_Null)
		return;
	cBoundingVolume &bv = apShape->GetBoundingVolume();
	avOut.push_back(MatrixOBB(cMath::MatrixMul(am, apShape->GetOffset()), bv.GetLocalMin(), bv.GetLocalMax()));
}

static void EntityBoxes(cSomaLuxEntity *apEnt, std::vector<cSomaOBB> &avOut, bool abShapes = true)
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
		avOut.push_back(MatrixOBB(apEnt->GetMatrix(), apEnt->mvSize * -0.5f, apEnt->mvSize * 0.5f));
		return;
	}
	for (iPhysicsBody *pBody : apEnt->mvBodies)
		if (pBody->IsActive() == false)
			continue;
		else if (abShapes && pBody->GetShape())
			ShapeBoxes(pBody->GetShape(), pBody->GetLocalMatrix(), avOut);
		else
			avOut.push_back(AABBToOBB(pBody->GetBoundingVolume()->GetMin(), pBody->GetBoundingVolume()->GetMax()));
	if (avOut.empty() && apEnt->mpMesh)
		avOut.push_back(AABBToOBB(apEnt->mpMesh->GetBoundingVolume()->GetMin(), apEnt->mpMesh->GetBoundingVolume()->GetMax()));
}

static bool PointInOBB(const cSomaOBB &b, const cVector3f &avPos)
{
	cVector3f d = avPos - b.mvCenter;
	for (int i = 0; i < 3; ++i)
		if (std::fabs(cMath::Vector3Dot(d, b.mvAxis[i])) > b.mvHalf.v[i])
			return false;
	return true;
}

float SomaLiquidHeightAt(const cVector3f &avPos)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap)
		for (cSomaLuxEntity *pEnt : pMap->GetEntities())
		{
			if (pEnt->meType != eSomaLuxEntityType_LiquidArea || pEnt->mbActive == false)
				continue;
			std::vector<cSomaOBB> v;
			EntityBoxes(pEnt, v);
			if (PointInOBB(v[0], avPos))
				return v[0].mvCenter.y + v[0].mvHalf.y;
		}
	return -10000.0f;
}

// cMeshCreator::CreateGridPlane: upper submesh faces +y, lower (reversed winding) faces -y
static cMesh *CreateGridPlane(const tString &asName, const tString &asUpper, const tString &asLower, const cVector2f &avSize, float afGrid, float afTile)
{
	cResources *pRes = gpSomaBase->mpEngine->GetResources();
	int nx = afGrid > 0 ? (int)(avSize.x / afGrid + 0.99f) : 1, nz = afGrid > 0 ? (int)(avSize.y / afGrid + 0.99f) : 1;
	cVector2f vStep = afGrid > 0 ? cVector2f(afGrid) : avSize;
	cMesh *pMesh = hplNew(cMesh, (asName, _W(""), pRes->GetMaterialManager(), pRes->GetAnimationManager()));
	for (int m = 0; m < (asLower != "" ? 2 : 1); ++m)
	{
		iVertexBuffer *pVtx = gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->CreateVertexBuffer(
			eVertexBufferType_Hardware, eVertexBufferDrawType_Tri, eVertexBufferUsageType_Static, (nx + 1) * (nz + 1), nx * nz * 6);
		pVtx->CreateElementArray(eVertexBufferElement_Position, eVertexBufferElementFormat_Float, 4);
		pVtx->CreateElementArray(eVertexBufferElement_Normal, eVertexBufferElementFormat_Float, 3);
		pVtx->CreateElementArray(eVertexBufferElement_Color0, eVertexBufferElementFormat_Float, 4);
		pVtx->CreateElementArray(eVertexBufferElement_Texture0, eVertexBufferElementFormat_Float, 3);
		for (int z = 0; z <= nz; ++z)
			for (int x = 0; x <= nx; ++x)
			{
				cVector2f v(std::min(x * vStep.x, avSize.x), std::min(z * vStep.y, avSize.y));
				pVtx->AddVertexVec3f(eVertexBufferElement_Position, cVector3f(v.x - avSize.x * 0.5f, 0, v.y - avSize.y * 0.5f));
				pVtx->AddVertexVec3f(eVertexBufferElement_Normal, cVector3f(0, m == 0 ? 1.0f : -1.0f, 0));
				pVtx->AddVertexColor(eVertexBufferElement_Color0, cColor(1, 1));
				pVtx->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(v.x / afTile, v.y / afTile, 0));
			}
		for (int z = 0; z < nz; ++z)
			for (int x = 0; x < nx; ++x)
			{
				int a = z * (nx + 1) + x, b = a + nx + 1;
				int vTri[6] = {a, a + 1, b, b, a + 1, b + 1};
				for (int i = 0; i < 6; ++i)
					pVtx->AddIndex(vTri[m == 0 ? i : i - 2 * (i % 3) + 2]);
			}
		pVtx->Compile(eVertexCompileFlag_CreateTangents);
		tString sMat = cString::GetFileName(m == 0 ? asUpper : asLower);
		cSubMesh *pSub = pMesh->CreateSubMesh(m == 0 ? "Upper" : "Lower");
		pSub->SetMaterial(pRes->GetMaterialManager()->CreateMaterial(sMat));
		pSub->SetVertexBuffer(pVtx);
		pSub->SetMaterialName(sMat);
		pSub->Compile();
	}
	return pMesh;
}

// cLuxLiquidArea::GenerateGraphics
void cSomaLuxEntity::CreateLiquidGraphics(cWorld *apWorld)
{
	cResourceVarsObject &v = mInstanceVars;
	// ponytail: always dynamic, the static container is already compiled when areas are set up
	bool bStatic = false;
	tString sUpper = v.GetVarString("UpperMaterial", ""), sLower = v.GetVarString("LowerMaterial", "");
	if (sUpper != "" && sLower != "")
	{
		cMesh *pMesh = CreateGridPlane(msName + "_surface", sUpper, sLower, cVector2f(mvSize.x, mvSize.z), v.GetVarFloat("GridSize", 0), v.GetVarFloat("TextureTileSize", 1));
		mpLiquidMesh = apWorld->CreateMeshEntity(msName, pMesh, bStatic);
		if (mpLiquidMesh->GetSubMeshEntityNum() > 1)
			mpLiquidMesh->GetSubMeshEntity(1)->SetRenderFlagBit(eRenderableFlag_VisibleInReflection, false);
	}
	mpLiquidFog = apWorld->CreateFogArea(msName + "_FogArea", bStatic);
	mpLiquidFog->SetSize(mvSize);
	mpLiquidFog->SetColor(v.GetVarColor("FogColor", cColor(1, 1)));
	mpLiquidFog->SetStart(v.GetVarFloat("FogStart", 0));
	mpLiquidFog->SetEnd(v.GetVarFloat("FogEnd", 2));
	mpLiquidFog->SetFalloffExp(v.GetVarFloat("FogFalloffExp", 1));
	mpLiquidFog->SetBrightness(v.GetVarFloat("FogBrightness", 1));
	mpLiquidFog->SetUnderwater(v.GetVarBool("FogUnderwater", false));
	mpLiquidFog->SetSkybox(v.GetVarBool("FogSkybox", false));
	mpLiquidFog->SetRenderFlagBit(eRenderableFlag_VisibleInReflection, false);
	m_mtxLiquidPlaced = cMatrixf::Zero;
	PlaceLiquidGraphics();
}

void cSomaLuxEntity::PlaceLiquidGraphics()
{
	cMatrixf m = GetMatrix();
	if (m == m_mtxLiquidPlaced)
		return;
	m_mtxLiquidPlaced = m;
	if (mpLiquidMesh)
	{
		mpLiquidMesh->SetMatrix(m.GetRotation());
		mpLiquidMesh->SetPosition(m.GetTranslation() + cVector3f(0, mvSize.y * 0.5f, 0));
		mpLiquidMesh->SetVisible(mbActive);
	}
	if (mpLiquidFog)
	{
		mpLiquidFog->SetMatrix(m);
		mpLiquidFog->SetVisible(mbActive && mInstanceVars.GetVarBool("UseFog", false));
	}
}

void cSomaLuxEntity::UpdateLiquid()
{
	PlaceLiquidGraphics();
	cCamera *pCam = cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetCamera() : NULL;
	std::vector<cSomaOBB> v;
	EntityBoxes(this, v);
	// ponytail: camera point, not cLuxPlayer::GetCameraCollideShape vs the area shape
	bool bInside = pCam && PointInOBB(v[0], pCam->GetPosition());
	if (bInside == mbCameraInLiquid)
		return;
	mbCameraInLiquid = bInside;
	if (bInside)
	{
		gbSomaUnderwaterEffects = true;
		++glSomaUnderwaterUsers;
	}
	else if (--glSomaUnderwaterUsers <= 0)
		gbSomaUnderwaterEffects = false;
	tString sCallback = mInstanceVars.GetVarString("CameraCollideCallback", "");
	cSomaLuxMap *pMap = mpMap ? mpMap : cSomaLuxMap::GetCurrent();
	int lState = bInside ? 1 : -1;
	if (sCallback != "" && pMap && pMap->GetScript())
		cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void " + sCallback + "(int)", [&](asIScriptContext *c) { c->SetArgDWord(0, (asDWORD)lState); });
	Call(bInside ? "void OnCameraEnter()" : "void OnCameraExit()");
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

bool SomaEntityIsOnScreen(cSomaLuxEntity *apEnt, bool abRayCast)
{
	cCamera *pCam = cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetCamera() : NULL;
	// the original ignores the active flag: an inactive mesh still counts
	if (pCam == NULL || apEnt == NULL)
		return false;
	cFrustum *pFrustum = pCam->GetFrustum();
	std::vector<cSomaOBB> vBoxes;
	EntityBoxes(apEnt, vBoxes);
	if (vBoxes.empty())
		vBoxes.push_back(AABBToOBB(apEnt->GetPosition(), apEnt->GetPosition()));
	for (const cSomaOBB &box : vBoxes)
	{
		cVector3f vHalf;
		for (int k = 0; k < 3; ++k)
			vHalf.v[k] = std::fabs(box.mvAxis[0].v[k]) * box.mvHalf.x + std::fabs(box.mvAxis[1].v[k]) * box.mvHalf.y + std::fabs(box.mvAxis[2].v[k]) * box.mvHalf.z;
		cBoundingVolume bv;
		bv.SetLocalMinMax(box.mvCenter - vHalf - cVector3f(0.001f), box.mvCenter + vHalf + cVector3f(0.001f));
		if (pFrustum->CollideBoundingVolume(&bv) == eCollision_Outside)
			continue;
		if (abRayCast == false || SomaLineOfSight(pCam->GetPosition(), box.mvCenter, apEnt))
			return true;
	}
	return false;
}

// iLuxAIBase::CheckLineOfSight: two clear rays out of 12 offsets across the entity's box
bool SomaEntityInPlayerLOS(cSomaLuxEntity *apEnt, bool abCheckFOV)
{
	cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
	if (pPlayer == NULL || apEnt == NULL || apEnt->mbActive == false || pPlayer->mfHealth <= 0)
		return false;
	if (abCheckFOV && SomaEntityIsOnScreen(apEnt, false) == false)
		return false;
	cBoundingVolume *pBV = apEnt->mpMesh ? apEnt->mpMesh->GetBoundingVolume() : NULL;
	cVector3f vFrom = pPlayer->GetCamera()->GetPosition();
	cVector3f vTo = pBV ? pBV->GetWorldCenter() : apEnt->GetPosition(), vSize = pBV ? pBV->GetSize() : cVector3f(0.5f);
	cVector3f vRight = cMath::Vector3Cross(cMath::Vector3Normalize(vTo - vFrom), cVector3f(0, 1, 0));
	// ponytail: fixed offsets, the official table is filled at runtime
	static const float vOff[12][2] = {{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-0.7f, -0.7f}, {0.7f, -0.7f}, {-0.7f, 0.7f}, {0.7f, 0.7f}, {-0.4f, 0.3f}, {0.4f, -0.3f}, {0, 0.5f}};
	int lClear = 0;
	for (const float *o : vOff)
		if (SomaLineOfSight(vFrom + (vRight * o[0] + cVector3f(0, o[1], 0)) * 0.05f, vTo + vRight * (o[0] * vSize.x * 0.4f) + cVector3f(0, o[1] * vSize.y * 0.4f, 0), apEnt) &&
			++lClear == 2)
			return true;
	return false;
}

static bool BodyInArea(cSomaLuxEntity *apArea, iPhysicsBody *apBody, bool abCenter)
{
	std::vector<cSomaOBB> vBoxes;
	EntityBoxes(apArea, vBoxes);
	if (vBoxes.empty())
		return false;
	const cSomaOBB &box = vBoxes[0];
	if (abCenter == false)
		return OBBOverlap(box, AABBToOBB(apBody->GetBoundingVolume()->GetMin(), apBody->GetBoundingVolume()->GetMax()));
	cVector3f vDelta = cMath::MatrixMul(apBody->GetLocalMatrix(), apBody->GetMassCentre()) - box.mvCenter;
	for (int i = 0; i < 3; ++i)
		if (std::fabs(cMath::Vector3Dot(vDelta, box.mvAxis[i])) > box.mvHalf.v[i])
			return false;
	return true;
}

void cSomaLuxEntity::UpdateCheckCollision(float afTimeStep)
{
	if (mbCheckCollision == false || mbActive == false || mpMap == NULL)
		return;
	mfTimeSinceCheck += afTimeStep;
	float fSince = mfTimeSinceCheck;
	if (CallBool("bool OnStartCheckCollision(float, float)", [&](asIScriptContext *c) {
			c->SetArgFloat(0, afTimeStep);
			c->SetArgFloat(1, fSince);
		}, false) == false)
		return;
	mfTimeSinceCheck = 0;
	bool bContinue = true;
	for (cSomaLuxEntity *pEnt : std::vector<cSomaLuxEntity *>(mpMap->GetEntities()))
	{
		if (bContinue == false)
			break;
		if (pEnt == this || pEnt->mbActive == false || pEnt->meType == eSomaLuxEntityType_Area || pEnt->meType == eSomaLuxEntityType_LiquidArea)
			continue;
		for (iPhysicsBody *pBody : std::vector<iPhysicsBody *>(pEnt->mvBodies))
		{
			bool bDynamic = pBody->GetMass() > 0;
			if (pBody->IsActive() == false || (bDynamic ? mbCheckDynamic : mbCheckStatic) == false || BodyInArea(this, pBody, mbCheckCenterInArea) == false)
				continue;
			if (CallBool("bool OnCheckCollision(iPhysicsBody@, iLuxEntity@)", [&](asIScriptContext *c) {
					c->SetArgAddress(0, pBody);
					c->SetArgAddress(1, pEnt);
				}, true) == false)
			{
				bContinue = false;
				break;
			}
		}
	}
	CallWithFloat("void OnEndCheckCollision(float)", afTimeStep);
}

static bool RayHitsOBB(const cSomaOBB &b, const cVector3f &avStart, const cVector3f &avDir, float afMaxDist, float &afT)
{
	cVector3f vRel = avStart - b.mvCenter;
	float tMin = 0, tMax = afMaxDist;
	for (int i = 0; i < 3; ++i)
	{
		float o = cMath::Vector3Dot(vRel, b.mvAxis[i]);
		float d = cMath::Vector3Dot(avDir, b.mvAxis[i]);
		if (std::fabs(d) < 1e-6f)
		{
			if (std::fabs(o) > b.mvHalf.v[i])
				return false;
			continue;
		}
		float t1 = (-b.mvHalf.v[i] - o) / d, t2 = (b.mvHalf.v[i] - o) / d;
		if (t1 > t2)
			std::swap(t1, t2);
		tMin = std::max(tMin, t1);
		tMax = std::min(tMax, t2);
		if (tMin > tMax)
			return false;
	}
	afT = tMin;
	return true;
}

bool SomaRayHitsEntity(cSomaLuxEntity *apEnt, const cVector3f &avStart, const cVector3f &avDir, float afMaxDist, float &afDistOut)
{
	std::vector<cSomaOBB> vBoxes;
	EntityBoxes(apEnt, vBoxes);
	bool bHit = false;
	afDistOut = afMaxDist;
	float t;
	for (const cSomaOBB &b : vBoxes)
		if (RayHitsOBB(b, avStart, avDir, afMaxDist, t) && t < afDistOut)
		{
			afDistOut = t;
			bHit = true;
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

// iLuxEntity::UpdatePlayerLookAt: per body, in frustum, within distance, optionally under the crosshair,
// then line of sight to the near side of its bounding sphere and four points around it
bool SomaPlayerLooksAt(cSomaLuxEntity *apEnt, cCamera *apCam)
{
	std::vector<cSomaOBB> vBoxes;
	EntityBoxes(apEnt, vBoxes, false);
	cVector3f vCam = apCam->GetPosition();
	float fMax = apEnt->mfLookAtMaxDistance > 0 ? apEnt->mfLookAtMaxDistance : 1000.0f, t;
	for (const cSomaOBB &b : vBoxes)
	{
		float fR = b.mvHalf.Length(), fDist = cMath::Vector3Dist(b.mvCenter, vCam);
		cBoundingVolume bv;
		bv.SetLocalMinMax(b.mvCenter - fR, b.mvCenter + fR);
		if (apCam->GetFrustum()->CollideBoundingVolume(&bv) == eCollision_Outside || fDist > fMax)
			continue;
		if (apEnt->mbLookAtCheckCenter && RayHitsOBB(b, vCam, apCam->GetForward(), fMax, t) == false)
			continue;
		if (apEnt->mbLookAtCheckRay == false || fR * fR + 0.05f > fDist * fDist)
			return true;
		cVector3f vNear = b.mvCenter - (b.mvCenter - vCam) * (fR / fDist), vUp = apCam->GetUp() * (fR * 0.5f), vRight = apCam->GetRight() * (fR * 0.5f);
		for (const cVector3f &p : {vNear, vNear + vUp, vNear - vUp, vNear + vRight, vNear - vRight})
			if (SomaLineOfSight(vCam, p, apEnt))
				return true;
	}
	return false;
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

static cSomaLuxEntity *Find(const tString &asName)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	return pMap ? pMap->GetEntity(asName) : NULL;
}

template <class F> static void ForMatching(const tString &asName, F aFunc)
{
	if (cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent())
		for (cSomaLuxEntity *pEnt : pMap->GetEntities())
			if (cString::MatchesWildcard(asName, pEnt->msName) || (pEnt->meType == eSomaLuxEntityType_Player && cString::ToLowerCase(asName) == "player"))
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
	SOMA_METHOD_NEW(e, T, "cVector3f GetMeshScaleMul()", +[](E *p) { return p->mvMeshScaleMul; });
	SOMA_METHOD_NEW(e, T, "void SetMeshScaleMul(const cVector3f&in avScale)", +[](E *p, const cVector3f &v) {
		if (v == p->mvMeshScaleMul || p->mpMesh == NULL)
			return;
		cVector3f vNew = v;
		if (v.x == 0 || v.y == 0 || v.z == 0)
			vNew += cVector3f(0.001f);
		cMatrixf mtxRel = cMath::MatrixScale(vNew / p->mvMeshScaleMul);
		p->mvMeshScaleMul = vNew;
		for (int i = 0; i < p->mpMesh->GetSubMeshEntityNum(); ++i)
		{
			cSubMeshEntity *pSub = p->mpMesh->GetSubMeshEntity(i);
			cMatrixf mtx = cMath::MatrixMul(pSub->GetLocalMatrix(), mtxRel);
			mtx.SetTranslation(pSub->GetLocalMatrix().GetTranslation());
			pSub->SetMatrix(mtx);
		}
	});
	SOMA_METHOD_NEW(e, T, "int GetBodyNum()", +[](E *p) { return (int)p->mvBodies.size(); });
	SOMA_METHOD_NEW(e, T, "iPhysicsBody@ GetBody(int alIdx)", +[](E *p, int i) { return i >= 0 && i < (int)p->mvBodies.size() ? p->mvBodies[i] : (iPhysicsBody *)NULL; });
	SOMA_METHOD_NEW(e, T, "iPhysicsBody@ GetMainBody()", +[](E *p) { return p->GetMainBody(); });
	SOMA_METHOD_NEW(e, T, "void AttachToEntity(iLuxEntity@ apEntity, iPhysicsBody@ apTargetBody, bool abUseRotation, bool abSnapToParent, bool abLocked=false)",
					+[](E *p, E *pParent, iPhysicsBody *pBody, bool r, bool snap, bool l) { p->AttachTo(pParent, pBody, "", r, snap, l); });
	SOMA_METHOD_NEW(e, T, "void AttachToSocket(iLuxEntity@ apEntity, const tString&in asSocket, bool abUseRotation, bool abSnapToParent, bool abLocked=false)",
					+[](E *p, E *pParent, S sock, bool r, bool snap, bool l) { p->AttachTo(pParent, NULL, sock, r, snap, l); });
	SOMA_METHOD_NEW(e, T, "void UpdateEntityAttachment()", +[](E *p) { p->UpdateAttachment(); });
	SOMA_METHOD_NEW(e, T, "void RemoveEntityAttachment()", +[](E *p) { p->RemoveAttachment(); });
	SOMA_METHOD_NEW(e, T, "int GetBodyIndexFromName(const tString&in asName)", +[](E *p, S n) {
		for (size_t i = 0; i < p->mvBodies.size(); ++i)
			if (p->mvBodies[i]->GetName() == n || p->mvBodies[i]->GetName().ends_with("_" + n))
				return (int)i;
		return -1;
	});
	SOMA_METHOD_NEW(e, T, "iPhysicsBody@ GetBodyFromID(int alID)", +[](E *p, int id) {
		for (iPhysicsBody *b : p->mvBodies)
			if (b->GetUniqueID() == id)
				return b;
		return (iPhysicsBody *)NULL;
	});

	SOMA_METHOD_NEW(e, T, "iPhysicsBody@ GetBodyFromName(const tString&in asName)", +[](E *p, S n) { return p->GetBodyFromName(n); });
	SOMA_METHOD_NEW(e, T, "cMeshEntity@ GetMeshEntity()", +[](E *p) { return p->mpMesh; });
	SOMA_METHOD_NEW(e, T, "void SetupParent(int alTypeId, tID alId, const tString &in asName)", +[](E *p, int t, cSomaID id, S n) {
		p->mlParentType = t;
		p->mParentID = id;
		p->msParentName = n;
	});
	SOMA_METHOD_NEW(e, T, "int GetParentType()", +[](E *p) { return p->mlParentType; });
	SOMA_METHOD_NEW(e, T, "tID GetParentId()", +[](E *p) { return p->mParentID; });
	SOMA_METHOD_NEW(e, T, "const tString& GetParentName()", +[](E *p) -> const tString & { return p->msParentName; });
	SOMA_METHOD_NEW(e, T, "float GetEffectsOnTime()", +[](E *p) { return p->mVars.GetVarFloat("EffectsOnTime", 1); });
	SOMA_METHOD_NEW(e, T, "float GetEffectsOffTime()", +[](E *p) { return p->mVars.GetVarFloat("EffectsOffTime", 1); });
	SOMA_METHOD_NEW(e, T, "int GetJointNum()", +[](E *p) { return (int)p->Joints().size(); });
	SOMA_METHOD_NEW(e, T, "iPhysicsJoint@ GetJoint(int alIdx)", +[](E *p, int i) {
		return i >= 0 && i < (int)p->Joints().size() ? p->Joints()[i] : (iPhysicsJoint *)NULL; });
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
					+[](E *p, S n, float f, bool l, bool, S cb, bool g) {
						if (g && p->mbGlobalSpaceAnim == false && p->mpMesh)
							p->mpMesh->SetMatrix(cMath::MatrixScale(p->mvScale));
						p->mbGlobalSpaceAnim = g;
						return p->PlayAnimation(n, f, l, cb);
					});
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
					+[](E *p, tString sub, const cColor &mul, const cColor &clear, const cColor &offline, const cVector2f &size) {
						if (p->mpImGui)
							return;
						cGui *pGui = gpSomaBase->mpEngine->GetGui();
						cGuiSet *pSet = pGui->CreateSet(p->msName + "_gui", NULL);
						pSet->SetVirtualSize(size, -1000, 1000);
						p->mpImGui = new cSomaImGui(p->msName, pSet);
						p->mpImGui->mScreenClear = clear;
						p->mpImGui->mScreenOfflineClear = offline;
						if (cSomaLuxHandler *pGui = cSomaLuxGame::Get()->GetHandler("GuiHandler"))
							pGui->CallWithObject("void SetDefaultData(cImGui @apImGui)", p->mpImGui);
						p->SetupGuiScreen(sub);
						if (p->mpGuiSubMesh)
						{
							pSet->SetIs3D(true);
							p->mpGuiSubMesh->SetColorMul(mul);
						}
						else
							Warning("SOMA: GUI submesh '%s' not found on '%s'\n", sub.c_str(), p->msName.c_str());
					});
	SOMA_METHOD_NEW(e, T, "void SetOnGuiFunction(const tString&in asFunction)", +[](E *p, S f) { p->msOnGuiFunc = f; p->mbGuiDirty = true; });
	SOMA_METHOD_NEW(e, T, "void SetGuiActive(bool abX, float afFadeTime=0.0f)", +[](E *p, bool b, float f) { p->SetGuiActive(b, f); });
	SOMA_METHOD_NEW(e, T, "void SetGuiVariableFPS(float afX)", +[](E *p, float f) { p->mfGuiFPS = f; });
	SOMA_METHOD_NEW(e, T, "void SetGuiSetUseInput(bool)", +[](E *p, bool b) { p->mbGuiSetUseInput = b; });
	SOMA_METHOD_NEW(e, T, "bool GetGuiSetUseInput()", +[](E *p) { return p->mbGuiSetUseInput; });
	SOMA_METHOD_NEW(e, T, "void SetGuiUpdateWhenOutOfView(bool abX)", +[](E *p, bool b) { p->mbGuiUpdateWhenOutOfView = b; });
	SOMA_METHOD_NEW(e, T, "void ForceGuiCacheUpdate()", +[](E *p) { p->mbGuiDirty = true; });
	SOMA_METHOD_NEW(e, T, "bool IsGuiActive()", +[](E *p) { return p->mbGuiActive; });
	SOMA_METHOD_NEW(e, T, "bool HasActiveGui()", +[](E *p) { return p->mpImGui != NULL; });
	SOMA_METHOD_NEW(e, T, "bool SetGuiIsFocused(bool abX, bool abShowMouse=true)", +[](E *p, bool b, bool mouse) {
		if (p->mpImGui == NULL)
			return false;
		p->mbGuiDirty = true;
		// cLuxGuiSet::SetIsFocused: the interact click must not also press a widget
		if (cSomaLuxInputHandler::Get())
			cSomaLuxInputHandler::Get()->ClearGuiInput();
		if (b)
			cSomaImGui::SetInputFocus(p->mpImGui, mouse);
		else if (cSomaImGui::GetScriptInputFocus() == p->mpImGui)
			cSomaImGui::SetInputFocus(NULL, false);
		return true;
	});
	SOMA_METHOD_NEW(e, T, "bool IsGuiFocused()", +[](E *p) { return p->mpImGui && cSomaImGui::GetScriptInputFocus() == p->mpImGui; });
	SOMA_METHOD_NEW(e, T, "void SetIsInteractedWith(bool abX)", +[](E *p, bool b) { p->mbInteractedWith = b; });
	SOMA_METHOD_NEW(e, T, "bool IsInteractedWith()", +[](E *p) { return p->mbInteractedWith; });
	SOMA_METHOD_NEW(e, T, "void SetMaxInteractDistance(float afX)", +[](E *p, float f) { p->mfMaxInteractDistance = f; });
	SOMA_METHOD_NEW(e, T, "void SetInteractionDisabled(bool abX)", +[](E *p, bool b) { p->mbInteractionDisabled = b; });
	SOMA_METHOD_NEW(e, T, "bool GetInteractionDisabled()", +[](E *p) { return p->mbInteractionDisabled; });
	SOMA_METHOD_NEW(e, T, "void SetPlayerInteractCallback(const tString &in asCallbackFunc, bool abRemoveWhenInteracted)",
					+[](E *p, S f, bool r) { p->msInteractCallback = f; p->mbInteractCallbackAutoRemove = r; });
	SOMA_METHOD_NEW(e, T, "void SetPlayerLookAtCallback(const tString &in asCallbackFunc, bool abRemoveWhenLookedAt, bool abCheckCenterOfScreen, bool abCheckRayIntersection, float afMaxDistance, float afCallbackDelay)",
					+[](E *p, S f, bool r, bool c, bool ray, float d, float t) { SetLookAtCallback(p, f, r, c, ray, d, t); });
	SOMA_METHOD_NEW(e, T, "bool HasPlayerInteractCallback()", +[](E *p) { return p->msInteractCallback != ""; });
	SOMA_METHOD_NEW(e, T, "void ChangeConnectionState(int alState)", +[](E *p, int l) { p->ChangeConnectionState(l); });
	SOMA_METHOD_NEW(e, T, "void SetConnectionStateChangeCallback(const tString &in asCallbackFunc)", +[](E *p, S f) { p->msConnectionCallback = f; });
	SOMA_METHOD_NEW(e, T, "void AddConnection(const tString&in asName, iLuxEntity @apEntity, bool abInvertStateSent, int alStatesUsed)",
					+[](E *p, S n, E *c, bool i, int l) { if (c) p->mvConnections.push_back(E::cConnection{n, c->msName, i, l}); });
	SOMA_METHOD_NEW(e, T, "void RemoveConnection(const tString&in asName)",
					+[](E *p, S n) { std::erase_if(p->mvConnections, [&](const E::cConnection &c) { return c.msName == n; }); });
	SOMA_METHOD_NEW(e, T, "void RemoveAllConnections()", +[](E *p) { p->mvConnections.clear(); });
	SOMA_METHOD_NEW(e, T, "bool HasPlayerLookAtCallback()", +[](E *p) { return p->msLookAtCallback != ""; });
	SOMA_METHOD_NEW(e, T, "void SetForceLookAtCheck(bool abX)", +[](E *p, bool b) { p->mbForceLookAtCheck = b; });
	SOMA_METHOD_NEW(e, T, "void SetLookAtCheckCenterOfScreen(bool abX)", +[](E *p, bool b) { p->mbLookAtCheckCenter = b; });
	SOMA_METHOD_NEW(e, T, "void SetLookAtCheckRayIntersection(bool abX)", +[](E *p, bool b) { p->mbLookAtCheckRay = b; });
	SOMA_METHOD_NEW(e, T, "void SetLookAtMaxDistance(float afX)", +[](E *p, float f) { p->mfLookAtMaxDistance = f; });
	SOMA_METHOD_NEW(e, T, "bool IsLookedAtByPlayer()", +[](E *p) { return p->mbLookedAt; });
	SOMA_METHOD_NEW(e, T, "cSoundEntity@ PlaySound(const tString&in asName, const tString&in asFile, bool abRemoveWhenDone, bool abAttach)",
					+[](E *p, S n, S file, bool remove, bool attach) -> cSoundEntity * {
						cSomaLuxMap *pMap = p->mpMap ? p->mpMap : cSomaLuxMap::GetCurrent();
						cSoundEntity *pSound = pMap && file != "" ? pMap->GetWorld()->CreateSoundEntity(p->msName + "_" + n, file, remove) : NULL;
						if (pSound == NULL)
							return NULL;
						pSound->SetPosition(p->GetPosition());
						iEntity3D *pParent = p->mpMesh ? (iEntity3D *)p->mpMesh : (iEntity3D *)p->GetMainBody();
						if (attach && pParent)
						{
							pSound->SetPosition(0);
							pParent->AddChild(pSound);
						}
						return pSound;
					});
	SOMA_METHOD_NEW(e, T, "void SetEffectsActive(bool abActive, bool abFadeAndPlaySounds)", +[](E *p, bool b, bool f) { p->mbEffectsActive = b; p->SetEffectsActive(b && p->mbActive, f); });
	SOMA_METHOD_NEW(e, T, "bool GetEffectsActive()", +[](E *p) { return p->mbEffectsActive; });
	SOMA_METHOD_NEW(e, T, "float GetEffectsAlpha()", +[](E *p) { return p->mfEffectsAlpha; });
	SOMA_METHOD_NEW(e, T, "bool HasCollideCallbacks()", +[](E *p) { return !p->mvCollideCallbacks.empty(); });
	SOMA_METHOD_NEW(e, T, "void AddCollideCallback(iLuxEntity @apEntity, const tString&in asCallbackFunc)",
					+[](E *p, E *c, S f) { if (c) p->mvCollideCallbacks.push_back(E::cCollideCallback{c->msName, f}); });
	SOMA_METHOD_NEW(e, T, "void RemoveCollideCallback(const tString&in asEntityName)",
					+[](E *p, S n) { std::erase_if(p->mvCollideCallbacks, [&](const E::cCollideCallback &c) { return c.msChild == n; }); });
	SOMA_METHOD_NEW(e, T, "iLight@ GetLightFromName(const tString&in asName)", +[](E *p, S n) {
		for (iLight *l : p->mvLights)
			if (l->GetName() == n || l->GetName().ends_with(n))
				return l;
		return (iLight *)NULL;
	});
	SOMA_METHOD_NEW(e, T, "cParticleSystem@ GetParticleSystemFromName(const tString&in asName)", +[](E *p, S n) {
		for (cParticleSystem *l : p->mvParticleSystems)
			if (l && (l->GetName() == n || l->GetName().ends_with(n)))
				return l;
		return (cParticleSystem *)NULL;
	});
	SOMA_METHOD_NEW(e, T, "cBillboard@ GetBillboardFromName(const tString&in asName)", +[](E *p, S n) {
		for (cBillboard *l : p->mvBillboards)
			if (l->GetName() == n || l->GetName().ends_with(n))
				return l;
		return (cBillboard *)NULL;
	});
	SOMA_METHOD_NEW(e, T, "cLensFlare@ GetLensFlareFromName(const tString&in asName)", +[](E *p, S n) {
		for (cLensFlare *l : p->mvLensFlares)
			if (l->GetName() == n || l->GetName().ends_with(n))
				return l;
		return (cLensFlare *)NULL;
	});
	SOMA_METHOD_NEW(e, T, "cSoundEntity@ GetSoundEntityFromName(const tString&in asName)", +[](E *p, S n) {
		for (cSoundEntity *l : p->mvSoundEntities)
			if (l->GetName() == n || l->GetName().ends_with(n))
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
	SOMA_METHOD_NEW(e, T, "void SetVarVector2f(const tString&in asName, const cVector2f&in avX)", +[](E *p, S n, const cVector2f &v) { p->mmapScriptVars[n] = v.ToFileString(); });
	SOMA_METHOD_NEW(e, T, "void SetVarVector3f(const tString&in asName, const cVector3f&in avX)", +[](E *p, S n, const cVector3f &v) { p->mmapScriptVars[n] = v.ToFileString(); });
	SOMA_METHOD_NEW(e, T, "void SetVarColor(const tString&in asName, const cColor&in aX)", +[](E *p, S n, const cColor &v) { p->mmapScriptVars[n] = v.ToFileString(); });
	SOMA_METHOD_NEW(e, T, "void IncVarVector2f(const tString&in asName, const cVector2f&in avX)", +[](E *p, S n, const cVector2f &v) { p->mmapScriptVars[n] = (cString::ToVector2f(VarGet(p, n).c_str(), 0) + v).ToFileString(); });
	SOMA_METHOD_NEW(e, T, "void IncVarVector3f(const tString&in asName, const cVector3f&in avX)", +[](E *p, S n, const cVector3f &v) { p->mmapScriptVars[n] = (cString::ToVector3f(VarGet(p, n).c_str(), 0) + v).ToFileString(); });
	SOMA_METHOD_NEW(e, T, "cVector2f GetVarVector2f(const tString&in asName)", +[](E *p, S n) { return cString::ToVector2f(VarGet(p, n).c_str(), 0); });
	SOMA_METHOD_NEW(e, T, "cVector3f GetVarVector3f(const tString&in asName)", +[](E *p, S n) { return cString::ToVector3f(VarGet(p, n).c_str(), 0); });
	SOMA_METHOD_NEW(e, T, "cColor GetVarColor(const tString&in asName)", +[](E *p, S n) { return cString::ToColor(VarGet(p, n).c_str(), cColor(0, 0)); });

	cSomaLuxScriptable::RegisterTimerNatives(e, T);
}

void cSomaLuxEntity::AttachAnimationEventEntity(const tString &asSocket, iEntity3D *apEntity)
{
	for (cSocket &sock : mvSockets)
		if (sock.msName == asSocket)
		{
			apEntity->SetMatrix(sock.m_mtxOffset);
			if (sock.mpBone)
				sock.mpBone->AddEntity(apEntity);
			else
				mpMesh->AddChild(apEntity);
			return;
		}
	mpMesh->AddChild(apEntity);
}

// cMeshEntity::HandleAnimationEvent + iLuxEntity::OnAnimationEvent
bool cSomaLuxEntity::OnAnimationEvent(cMeshEntity *apMesh, cAnimationEvent *apEvent)
{
	cWorld *pWorld = apMesh->GetWorld();
	iEntity3D *pCreated = NULL;
	if (apEvent->mType == eAnimationEventType_PlaySound && apEvent->msValue != "")
	{
		pCreated = pWorld->CreateSoundEntity(apMesh->GetName() + "_AnimEvent", apEvent->msValue, true);
		if (pCreated == NULL)
			Error("Failed to play sound %s during animation event for MeshEntity %s\n", apEvent->msValue.c_str(), apMesh->GetName().c_str());
	}
	else if (apEvent->mType == eAnimationEventType_CreateParticle && apEvent->msValue != "")
		pCreated = pWorld->CreateParticleSystem(apMesh->GetName() + "_AnimEvent", apEvent->msValue, 1);
	else if (apEvent->mType == eAnimationEventType_PlayLoopSound || apEvent->mType == eAnimationEventType_StopLoopSound)
	{
		bool bAlive = mpAnimLoopSound && pWorld->SoundEntityExists(mpAnimLoopSound, mlAnimLoopSoundID);
		tString sSound = apEvent->mType == eAnimationEventType_PlayLoopSound ? apEvent->msValue : "";
		if (bAlive && sSound == msAnimLoopSound)
			return true;
		if (bAlive)
			mpAnimLoopSound->FadeOut(3);
		mpAnimLoopSound = NULL;
		msAnimLoopSound = sSound;
		if (sSound == "" || (mpAnimLoopSound = pWorld->CreateSoundEntity(msName + "_LoopAnimSound", sSound, true)) == NULL)
			return true;
		mpAnimLoopSound->FadeIn(3);
		mlAnimLoopSoundID = mpAnimLoopSound->GetCreationID();
		pCreated = mpAnimLoopSound;
	}
	if (pCreated)
	{
		pCreated->SetIsSaved(false);
		AttachAnimationEventEntity(apEvent->msDestSocket, pCreated);
	}
	return true;
}

bool cSomaLuxEntity::GetSocketMatrix(const tString &asName, cMatrixf &a_mtxOut)
{
	for (cSocket &sock : mvSockets)
	{
		if (sock.msName != asName)
			continue;
		cMatrixf mtxBase = sock.mpBone ? sock.mpBone->GetWorldMatrix() : mpMesh ? mpMesh->GetWorldMatrix() : GetMatrix();
		a_mtxOut = cMath::MatrixMul(mtxBase, sock.m_mtxOffset);
		return true;
	}
	return false;
}

// ponytail: unparented node refreshed once per frame, so a frame behind the bone
cNode3D *cSomaLuxEntity::GetSocketNode(int alIdx)
{
	if (alIdx < 0 || alIdx >= (int)mvSockets.size())
		return NULL;
	cSocket &sock = mvSockets[alIdx];
	if (sock.mpNode == NULL)
	{
		sock.mpNode = new cNode3D(sock.msName, false);
		UpdateSocketNodes();
	}
	return sock.mpNode;
}

void cSomaLuxEntity::UpdateSocketNodes()
{
	for (cSocket &sock : mvSockets)
	{
		cMatrixf mtx;
		if (sock.mpNode && GetSocketMatrix(sock.msName, mtx))
			sock.mpNode->SetMatrix(mtx);
	}
}

bool cSomaLuxEntity::GetAttachmentParentMatrix(cMatrixf &a_mtxOut)
{
	cSomaLuxEntity *pParent = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(mpAttachment->msParent) : NULL;
	if (pParent == NULL)
		return false;
	if (mpAttachment->mpBody)
	{
		if (std::find(pParent->mvBodies.begin(), pParent->mvBodies.end(), mpAttachment->mpBody) == pParent->mvBodies.end())
			return false;
		a_mtxOut = mpAttachment->mpBody->GetLocalMatrix();
		return true;
	}
	if (mpAttachment->msSocket != "" && pParent->GetSocketMatrix(mpAttachment->msSocket, a_mtxOut))
		return true;
	a_mtxOut = pParent->GetMatrix();
	return true;
}

iPhysicsBody *cSomaLuxEntity::GetBodyFromName(const tString &asName)
{
	for (iPhysicsBody *b : mvBodies)
		if (asName != "" && (b->GetName() == asName || cString::GetFileName(b->GetName()) == asName || b->GetName().ends_with("_" + asName)))
			return b;
	return NULL;
}

void cSomaLuxEntity::AttachTo(cSomaLuxEntity *apParent, iPhysicsBody *apBody, const tString &asSocket, bool abUseRotation, bool abSnap, bool abLocked)
{
	RemoveAttachment();
	if (apParent == NULL || apParent == this)
		return;
	mpAttachment = new cAttachment();
	mpAttachment->msParent = apParent->msName;
	mpAttachment->mpBody = asSocket == "" ? (apBody ? apBody : apParent->GetMainBody()) : NULL;
	mpAttachment->msSocket = asSocket;
	if (asSocket != "" && apParent->GetSocketMatrix(asSocket, mpAttachment->m_mtxParentPrev) == false)
		Warning("SOMA: socket '%s' not found on '%s'\n", asSocket.c_str(), apParent->msName.c_str());
	mpAttachment->mbUseRotation = abUseRotation;
	mpAttachment->mbLocked = abLocked;
	cMatrixf mtxParent;
	if (GetAttachmentParentMatrix(mtxParent) == false)
		mtxParent = cMatrixf::Identity;
	if (abSnap)
		SetMatrix(mtxParent);
	mpAttachment->m_mtxParentPrev = mtxParent;
	if (abLocked)
		mpAttachment->m_mtxOffset = cMath::MatrixMul(cMath::MatrixInverse(mtxParent), GetMatrix());
}

void cSomaLuxEntity::RemoveAttachment()
{
	delete mpAttachment;
	mpAttachment = NULL;
}

// iLuxEntity::UpdateEntityAttachment: locked children are posed rigidly, unlocked ones get the parent's motion
void cSomaLuxEntity::UpdateAttachment()
{
	if (mpAttachment == NULL)
		return;
	cMatrixf mtxParent;
	if (GetAttachmentParentMatrix(mtxParent) == false)
	{
		RemoveAttachment();
		return;
	}
	cAttachment *a = mpAttachment;
	cMatrixf mtxOld = GetMatrix();
	if (a->mbUseRotation)
	{
		if (mtxParent == a->m_mtxParentPrev)
			return;
		if (a->mbLocked)
			SetMatrix(cMath::MatrixMul(mtxParent, a->m_mtxOffset));
		else
			SetMatrix(cMath::MatrixMul(cMath::MatrixMul(mtxParent, cMath::MatrixInverse(a->m_mtxParentPrev)), GetMatrix()));
	}
	else
	{
		cVector3f vDelta = mtxParent.GetTranslation() - a->m_mtxParentPrev.GetTranslation();
		if (vDelta == cVector3f(0))
			return;
		cMatrixf mtx = GetMatrix();
		mtx.SetTranslation(a->mbLocked ? cMath::MatrixMul(mtxParent, a->m_mtxOffset).GetTranslation() : mtx.GetTranslation() + vDelta);
		SetMatrix(mtx);
	}
	a->m_mtxParentPrev = mtxParent;
	for (iPhysicsBody *pBody : mvBodies)
		pBody->Enable();
	cMatrixf mtxAdd = cMath::MatrixMul(GetMatrix(), cMath::MatrixInverse(mtxOld));
	Call("void OnAttachmentUpdate(const cMatrixf&in a_mtxTransformAdd)", [&](asIScriptContext *c) { c->SetArgObject(0, &mtxAdd); });
}

cSomaLuxEntity::~cSomaLuxEntity()
{
	ForgetLightConnections(this);
	delete mpAttachment;
	for (cSocket &sock : mvSockets)
		delete sock.mpNode;
	cSomaGuiScreenRenderer::Get()->Forget(this);
	SomaForgetCritter(this);
	SomaDestroyAgent(this);
	SomaFreePropBlock("cLuxCritter", mpCritterProps);
}

void cSomaLuxEntity::RegisterNatives(asIScriptEngine *e)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
	SomaSetIndirectProps("cLuxCritter", (int)offsetof(cSomaLuxEntity, mpCritterProps));
#pragma GCC diagnostic pop
	static auto Owner = [](cMeshEntity *m) -> cSomaLuxEntity * {
		if (cSomaLuxMap::GetCurrent())
			for (cSomaLuxEntity *p : cSomaLuxMap::GetCurrent()->GetEntities())
				if (p->mpMesh == m)
					return p;
		return NULL;
	};
	SOMA_METHOD(e, "cMeshEntity", "int GetSocketNum()", +[](cMeshEntity *m) { cSomaLuxEntity *p = Owner(m); return p ? (int)p->mvSockets.size() : 0; });
	SOMA_METHOD(e, "cMeshEntity", "cNode3D@ GetSocketFromIndex(int alIdx)", +[](cMeshEntity *m, int i) { cSomaLuxEntity *p = Owner(m); return p ? p->GetSocketNode(i) : (cNode3D *)NULL; });
	SOMA_METHOD(e, "cMeshEntity", "cNode3D@ GetSocket(const tString&in asName)", +[](cMeshEntity *m, const tString &s) -> cNode3D * {
		cSomaLuxEntity *p = Owner(m);
		for (int i = 0; p && i < (int)p->mvSockets.size(); ++i)
			if (p->mvSockets[i].msName == s)
				return p->GetSocketNode(i);
		return NULL;
	});

	const char *vTypes[] = {"iLuxEntity", "cLuxProp", "cLuxArea", "cLuxAgent", "cLuxCritter", "cLuxLiquidArea"};
	for (const char *pType : vTypes)
		if (e->GetTypeInfoByName(pType))
			RegisterEntityMethods(e, pType);

	for (const char *pType : {"cLuxProp", "cLuxArea"})
	{
		SOMA_METHOD(e, pType, "void MoveLinearTo(const cVector3f&in avGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed, const tString&in asCallback=\"\")",
					+[](cSomaLuxEntity *p, const cVector3f &g, float a, float m, float d, bool r, const tString &cb) { p->MoveLinearTo(g, a, m, d, r, cb); });
		SOMA_METHOD(e, pType, "void StopMove()", +[](cSomaLuxEntity *p) { p->StopMove(); });
		SOMA_METHOD(e, pType, "void SetCheckCollision(bool abX)", +[](cSomaLuxEntity *p, bool b) { p->mbCheckCollision = b; });
		SOMA_METHOD(e, pType, "bool GetCheckCollision()", +[](cSomaLuxEntity *p) { return p->mbCheckCollision; });
		SOMA_METHOD(e, pType, "void SetupCheckCollision(bool abCheckIfCenterInSide, bool abCheckDynamic, bool abCheckStatic, bool abCheckCharacters)",
					+[](cSomaLuxEntity *p, bool c, bool d, bool st, bool) {
						p->mbCheckCenterInArea = c;
						p->mbCheckDynamic = d;
						p->mbCheckStatic = st;
					});
		SOMA_METHOD(e, pType, "void MoveAngularTo(const cMatrixf&in a_mtxGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed, bool abUseOffset, const cVector3f &in avWorldOffset, const cVector3f &in avLocalOffset, const tString&in asCallback=\"\")",
					+[](cSomaLuxEntity *p, const cMatrixf &m, float a, float s, float d, bool r, bool o, const cVector3f &w, const cVector3f &l, const tString &cb) {
						p->MoveAngularTo(m, a, s, d, r, o ? l : cVector3f(0), cb);
					});
		SOMA_METHOD(e, pType, "void RotateAtSpeed( float afAcc, float afGoalSpeed, const cVector3f&in avAxis, bool abResetSpeed, bool abUseOffset, const cVector3f &in avWorldOffset, const cVector3f &in avLocalOffset)",
					+[](cSomaLuxEntity *p, float a, float s, const cVector3f &ax, bool r, bool o, const cVector3f &w, const cVector3f &l) {
						p->RotateAtSpeed(a, s, ax, r, o ? l : cVector3f(0));
					});
	}
	SOMA_METHOD(e, "cLuxLiquidArea", "void MoveLinearTo(const cVector3f&in avGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed, const tString&in asCallback=\"\")",
				+[](cSomaLuxEntity *p, const cVector3f &g, float a, float m, float d, bool r, const tString &cb) { p->MoveLinearTo(g, a, m, d, r, cb); });
	SOMA_METHOD(e, "cLuxLiquidArea", "void StopMove()", +[](cSomaLuxEntity *p) { p->StopMove(); });
	SOMA_METHOD(e, "cLuxLiquidArea", "iPhysicsBody@ GetAreaBody()", +[](cSomaLuxEntity *p) { return p->GetMainBody(); });
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
			SOMA_METHOD(e, pType, "bool CheckEntityCollision(iLuxEntity@ apEntity)", +[](cSomaLuxEntity *p, cSomaLuxEntity *c) {
				if (c == NULL || p->mbActive == false)
					return false;
				return c->msName == "Player" ? p->CollidesWithPlayer() : SomaEntitiesCollide(p, c);
			});
			SOMA_METHOD(e, pType, "bool CheckShapeCollision(iCollideShape @apShape, const cMatrixf &in a_mtxTransform, cLuxMap @apMap)",
						+[](cSomaLuxEntity *p, iCollideShape *s, const cMatrixf &m, void *) {
							if (s == NULL || p->mbActive == false)
								return false;
							cBoundingVolume &bv = s->GetBoundingVolume();
							cSomaOBB box;
							box.mvCenter = cMath::MatrixMul(m, cMath::MatrixMul(s->GetOffset(), (bv.GetLocalMin() + bv.GetLocalMax()) * 0.5f));
							box.mvHalf = (bv.GetLocalMax() - bv.GetLocalMin()) * 0.5f;
							for (int i = 0; i < 3; ++i)
								box.mvAxis[i] = cVector3f(m.m[0][i], m.m[1][i], m.m[2][i]);
							std::vector<cSomaOBB> v;
							EntityBoxes(p, v);
							for (const cSomaOBB &a : v)
								if (OBBOverlap(a, box))
									return true;
							return false;
						});
		}
	SOMA_FUNC(e, "void Entity_SetEffectBaseColor(const tString &in asEntityName,const cColor&in aColor)",
			  +[](const tString &n, const cColor &c) { ForMatching(n, [&](cSomaLuxEntity *p) { p->mfEffectColorTime = 0; p->SetEffectBaseColor(c); }); });
	SOMA_FUNC(e, "void Entity_FadeEffectBaseColor(const tString &in asEntityName,const cColor&in aColor, float afTime)",
			  +[](const tString &n, const cColor &c, float t) { ForMatching(n, [&](cSomaLuxEntity *p) { p->FadeEffectBaseColor(c, t); }); });
	SOMA_FUNC(e, "void Entity_PlayAnimation(const tString &in asEntityName, const tString &in asAnimation, float afFadeTime=0.1f, bool abLoop=false, bool abPlayTransition=true, const tString &in asCallback = \"\")",
			  +[](const tString &n, const tString &a, float f, bool l, bool, const tString &cb) { ForMatching(n, [&](cSomaLuxEntity *p) { p->PlayAnimation(a, f, l, cb); }); });
	SOMA_FUNC(e, "void Entity_StopAnimation(const tString &in asEntityName)", +[](const tString &n) { ForMatching(n, [](cSomaLuxEntity *p) { p->StopAnimations(0); }); });
	SOMA_FUNC(e, "void Entity_SetAnimationPaused(const tString &in asEntityName, const tString &in asAnimationName, bool abPaused = true)",
			  +[](const tString &n, const tString &a, bool b) {
				  ForMatching(n, [&](cSomaLuxEntity *p) {
					  if (p->mpMesh == NULL) return;
					  int lIdx = a == "" ? p->mlCurrentAnim : p->mpMesh->GetAnimationStateIndex(a);
					  if (lIdx >= 0) p->mpMesh->GetAnimationState(lIdx)->SetPaused(b);
				  });
			  });
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

	SOMA_METHOD(e, "tID", "bool opEquals(const tID &in) const", +[](const cSomaID &a, const cSomaID &b) { return a == b; });
	SOMA_METHOD(e, "tID", "tID&opAssign(const tID &in)", +[](cSomaID &a, const cSomaID &b) -> cSomaID & { return a = b; });
	static cSomaID invalidId;
	e->RegisterGlobalProperty("const tID tID_Invalid", &invalidId);

	typedef const tString &S;
	SOMA_FUNC(e, "bool Entity_Exists(const tString &in asName)", +[](S n) { return Find(n) != NULL; });
	SOMA_FUNC(e, "bool Entity_AttachToEntity(const tString &in asName, const tString &in asParentName, const tString &in asParentBodyName, bool abUseRotation, bool abSnapToParent=false, bool abLocked=false)",
			  +[](S n, S parent, S body, bool r, bool snap, bool l) {
				  cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
				  cSomaLuxEntity *pParent = pMap ? pMap->GetEntity(parent) : NULL;
				  bool bFound = false;
				  if (pParent)
					  ForMatching(n, [&](cSomaLuxEntity *p) { p->AttachTo(pParent, pParent->GetBodyFromName(body), "", r, snap, l); bFound = true; });
				  return bFound;
			  });
	SOMA_FUNC(e, "bool Entity_AttachToSocket(const tString &in asName, const tString &in asParentName, const tString &in asParentSocketName, bool abUseRotation, bool abSnapToParent=true)",
			  +[](S n, S parent, S sock, bool r, bool snap) {
				  cSomaLuxEntity *pParent = Find(parent);
				  bool bFound = false;
				  if (pParent)
					  ForMatching(n, [&](cSomaLuxEntity *p) { p->AttachTo(pParent, NULL, sock, r, snap, false); bFound = true; });
				  return bFound;
			  });
	SOMA_FUNC(e, "bool Entity_RemoveEntityAttachment(const tString &in asName)", +[](S n) {
		bool bFound = false;
		ForMatching(n, [&](cSomaLuxEntity *p) { p->RemoveAttachment(); bFound = true; });
		return bFound;
	});
	SOMA_FUNC(e, "void Entity_SetActive(const tString &in asName, bool abActive)", +[](S n, bool b) { ForMatching(n, [b](cSomaLuxEntity *p) { p->SetActive(b); }); });
	SOMA_FUNC(e, "void Entity_SetCollideCharacter(const tString &in asEntityName, bool abActive)",
			  +[](S n, bool b) { ForMatching(n, [b](cSomaLuxEntity *p) { for (iPhysicsBody *pBody : p->mvBodies) pBody->SetCollideCharacter(b); }); });
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
				  bool bFound = SomaGetClosestEntity(st, dir, len, type, pEnt, pBody, fDist);
				  if (out && (bFound || pBody))
				  {
					  *(float *)(out + 16) = fDist;
					  *(iPhysicsBody **)(out + 24) = pBody;
					  *(cSomaLuxEntity **)(out + 32) = pEnt;
				  }
				  return bFound;
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
	SOMA_FUNC(e, "bool cLux_GetClosestCharCollider(const cVector3f&in avStart,const cVector3f&in avDir, float afRayLength, bool abCheckDynamic, cLuxClosestCharCollider @apOutput)",
			  +[](const cVector3f &st, const cVector3f &dir, float len, bool dyn, char *out) {
				  struct cRay : iPhysicsRayCallback
				  {
					  bool mbDynamic = false;
					  iPhysicsBody *mpBody = NULL;
					  float mfDist = 0;
					  cVector3f mvNormal = 0;
					  bool OnIntersect(iPhysicsBody *apBody, cPhysicsRayParams *apParams) override
					  {
						  if (apBody->GetCollideCharacter() && apBody->IsCharacter() == false && (mbDynamic || apBody->GetMass() <= 0) &&
							  (mpBody == NULL || apParams->mfDist < mfDist))
							  mpBody = apBody, mfDist = apParams->mfDist, mvNormal = apParams->mvNormal;
						  return true;
					  }
				  } ray;
				  ray.mbDynamic = dyn;
				  cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
				  if (pMap == NULL || pMap->GetWorld()->GetPhysicsWorld() == NULL)
					  return false;
				  pMap->GetWorld()->GetPhysicsWorld()->CastRay(&ray, st, st + dir * len, true, true, false);
				  if (out && ray.mpBody)
				  {
					  *(float *)(out + 16) = ray.mfDist;
					  *(cVector3f *)(out + 20) = ray.mvNormal;
					  *(iPhysicsBody **)(out + 32) = ray.mpBody;
				  }
				  return ray.mpBody != NULL;
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
	SOMA_FUNC(e, "cLightDirectional@ cScene_ToLightDirectional(iLight@ apLight)", +[](iLight *p) { return dynamic_cast<cLightDirectional *>(p); });
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
						  if (body != "" && (b->GetName() == body || b->GetName().ends_with("_" + body)))
							  pBody = b;
					  cVector3f vPos = (pBody ? pBody->GetWorldPosition() : p->GetPosition()) + off;
					  p->OnInteract(0, pBody, vPos, data);
				  });
			  });
	static auto FindBody = [](S n) -> iPhysicsBody * {
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		return pMap && pMap->GetWorld()->GetPhysicsWorld() ? pMap->GetWorld()->GetPhysicsWorld()->GetBody(n) : NULL;
	};
	static auto FindJoint = [](S n) -> iPhysicsJoint * {
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		return pMap && pMap->GetWorld()->GetPhysicsWorld() ? pMap->GetWorld()->GetPhysicsWorld()->GetJoint(n) : NULL;
	};
	SOMA_FUNC(e, "void Joint_Break(const tString &in asJointName)", +[](S n) { if (iPhysicsJoint *j = FindJoint(n)) j->Break(); });
	SOMA_FUNC(e, "bool Joint_IsBroken(const tString &in asJointName)", +[](S n) { iPhysicsJoint *j = FindJoint(n); return j == NULL || j->IsBroken(); });
	SOMA_FUNC(e, "void Joint_SetBreakable(const tString &in asJointName, bool abBreakable)", +[](S n, bool b) { if (iPhysicsJoint *j = FindJoint(n)) j->SetBreakable(b); });
	SOMA_FUNC(e, "float Joint_GetForceSize(const tString &in asJointName)", +[](S n) { iPhysicsJoint *j = FindJoint(n); return j ? j->GetForceSize() : 0.0f; });
	static auto BodyVec = [](iPhysicsBody *b, const cVector3f &v, bool bLocal) { return bLocal ? cMath::MatrixMul(b->GetLocalMatrix().GetRotation(), v) : v; };
	SOMA_FUNC(e, "void Body_AddForce(const tString &in asBodyName, const cVector3f &in avForce, bool abLocalSpace)",
			  +[](S n, const cVector3f &v, bool l) { if (iPhysicsBody *b = FindBody(n)) b->AddForce(BodyVec(b, v, l)); });
	SOMA_FUNC(e, "void Body_AddImpulse(const tString &in asBodyName, const cVector3f &in avImpulse, bool abLocalSpace)",
			  +[](S n, const cVector3f &v, bool l) { if (iPhysicsBody *b = FindBody(n)) b->AddImpulse(BodyVec(b, v, l)); });
	SOMA_FUNC(e, "void Body_SetCollides(const tString &in asBodyName, bool abCollides)", +[](S n, bool c) { if (iPhysicsBody *b = FindBody(n)) b->SetCollide(c); });
	SOMA_FUNC(e, "tString Body_GetEntityName(const tString &in asBodyName)", +[](S n) -> tString {
		iPhysicsBody *b = FindBody(n);
		if (cSomaLuxMap *pMap = b ? cSomaLuxMap::GetCurrent() : NULL)
			for (cSomaLuxEntity *p : pMap->GetEntities())
				if (std::find(p->mvBodies.begin(), p->mvBodies.end(), b) != p->mvBodies.end())
					return p->msName;
		return "";
	});
	SOMA_FUNC(e, "void Entity_SetEffectsActive(const tString &in asEntityName, bool abActive, bool abFadeAndPlaySounds)",
			  +[](S n, bool b, bool f) { ForMatching(n, [b, f](cSomaLuxEntity *p) { p->mbEffectsActive = b; p->SetEffectsActive(b && p->mbActive, f); }); });
	SOMA_FUNC(e, "void Entity_Connect(const tString &in asName, const tString &in asMainEntity, const tString &in asConnectEntity, bool abInvertStateSent, int alStatesUsed)",
			  +[](S n, S m, S c, bool i, int l) { ForMatching(m, [&](cSomaLuxEntity *p) { p->mvConnections.push_back(cSomaLuxEntity::cConnection{n, c, i, l}); }); });
	SOMA_FUNC(e, "void Entity_RemoveConnection(const tString &in asName, const tString &in asMainEntity)", +[](S n, S m) {
		ForMatching(m, [&](cSomaLuxEntity *p) { std::erase_if(p->mvConnections, [&](const cSomaLuxEntity::cConnection &c) { return c.msName == n; }); });
	});
	SOMA_FUNC(e, "void Entity_RemoveAllConnections(const tString &in asMainEntity)", +[](S m) { ForMatching(m, [](cSomaLuxEntity *p) { p->mvConnections.clear(); }); });
	SOMA_FUNC(e, "void Entity_SetConnectionStateChangeCallback(const tString &in asEntityName, const tString &in asCallback)",
			  +[](S n, S f) { ForMatching(n, [&](cSomaLuxEntity *p) { p->msConnectionCallback = f; }); });
	SOMA_FUNC(e, "void Entity_SetPlayerInteractCallback(const tString &in asEntityName, const tString &in asCallback, bool abRemoveWhenInteracted)",
			  +[](S n, S f, bool r) { ForMatching(n, [&](cSomaLuxEntity *p) { p->msInteractCallback = f; p->mbInteractCallbackAutoRemove = r; }); });
	SOMA_FUNC(e, "void Entity_SetPlayerLookAtCallback(const tString &in asEntityName, const tString &in asCallback, bool abRemoveWhenLookedAt = true, bool abCheckCenterOfScreen = true, bool abCheckRayIntersection = true, float afMaxDistance = -1, float afCallbackDelay = 0)",
			  +[](S n, S f, bool r, bool c, bool ray, float d, float t) { ForMatching(n, [&](cSomaLuxEntity *p) { SetLookAtCallback(p, f, r, c, ray, d, t); }); });
	SOMA_FUNC(e, "bool Entity_AddCollideCallback(const tString &in asParentName, const tString &in asChildName, const tString &in asFunction)", +[](S par, S child, S f) {
		bool bAny = false;
		ForMatching(par, [&](cSomaLuxEntity *p) { p->mvCollideCallbacks.push_back(cSomaLuxEntity::cCollideCallback{child, f}); bAny = true; });
		return bAny;
	});
	SOMA_FUNC(e, "bool Entity_RemoveCollideCallback(const tString &in asParentName, const tString &in asChildName)", +[](S par, S child) {
		ForMatching(par, [&](cSomaLuxEntity *p) { std::erase_if(p->mvCollideCallbacks, [&](const cSomaLuxEntity::cCollideCallback &c) { return cString::MatchesWildcard(child, c.msChild); }); });
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
	SOMA_FUNC(e, "void Entity_IncVarInt(const tString&in asEntityName, const tString&in asVarName, int alX)",
			  +[](S n, S v, int x) { ForMatching(n, [&](cSomaLuxEntity *p) { p->mmapScriptVars[v] = cString::ToString(cString::ToInt(VarGet(p, v).c_str(), 0) + x); }); });
	SOMA_FUNC(e, "void Entity_IncVarFloat(const tString&in asEntityName, const tString&in asVarName, float afX)",
			  +[](S n, S v, float x) { ForMatching(n, [&](cSomaLuxEntity *p) { p->mmapScriptVars[v] = cString::ToString(cString::ToFloat(VarGet(p, v).c_str(), 0) + x); }); });
	SOMA_FUNC(e, "tString Entity_GetVarString(const tString&in asEntityName, const tString&in asVarName)", +[](S n, S v) { cSomaLuxEntity *p = Find(n); return p ? VarGet(p, v) : tString(); });
	SOMA_FUNC(e, "bool Entity_GetVarBool(const tString&in asEntityName, const tString&in asVarName)", +[](S n, S v) { cSomaLuxEntity *p = Find(n); return p && cString::ToBool(VarGet(p, v).c_str(), false); });
	SOMA_FUNC(e, "int Entity_GetVarInt(const tString&in asEntityName, const tString&in asVarName)", +[](S n, S v) { cSomaLuxEntity *p = Find(n); return p ? cString::ToInt(VarGet(p, v).c_str(), 0) : 0; });
	SOMA_FUNC(e, "float Entity_GetVarFloat(const tString&in asEntityName, const tString&in asVarName)", +[](S n, S v) { cSomaLuxEntity *p = Find(n); return p ? cString::ToFloat(VarGet(p, v).c_str(), 0) : 0.0f; });
	SOMA_FUNC(e, "void Entity_SetVarVector2f(const tString&in asEntityName, const tString&in asVarName, const cVector2f&in avX)",
			  +[](S n, S v, const cVector2f &x) { ForMatching(n, [&](cSomaLuxEntity *p) { p->mmapScriptVars[v] = x.ToFileString(); }); });
	SOMA_FUNC(e, "void Entity_SetVarVector3f(const tString&in asEntityName, const tString&in asVarName, const cVector3f&in avX)",
			  +[](S n, S v, const cVector3f &x) { ForMatching(n, [&](cSomaLuxEntity *p) { p->mmapScriptVars[v] = x.ToFileString(); }); });
	SOMA_FUNC(e, "void Entity_SetVarColor(const tString&in asEntityName, const tString&in asVarName, const cColor&in aX)",
			  +[](S n, S v, const cColor &x) { ForMatching(n, [&](cSomaLuxEntity *p) { p->mmapScriptVars[v] = x.ToFileString(); }); });
	SOMA_FUNC(e, "void Entity_IncVarVector2f(const tString&in asEntityName, const tString&in asVarName, const cVector2f&in avX)",
			  +[](S n, S v, const cVector2f &x) { ForMatching(n, [&](cSomaLuxEntity *p) { p->mmapScriptVars[v] = (cString::ToVector2f(VarGet(p, v).c_str(), 0) + x).ToFileString(); }); });
	SOMA_FUNC(e, "void Entity_IncVarVector3f(const tString&in asEntityName, const tString&in asVarName, const cVector3f&in avX)",
			  +[](S n, S v, const cVector3f &x) { ForMatching(n, [&](cSomaLuxEntity *p) { p->mmapScriptVars[v] = (cString::ToVector3f(VarGet(p, v).c_str(), 0) + x).ToFileString(); }); });
	SOMA_FUNC(e, "cVector2f Entity_GetVarVector2f(const tString&in asEntityName, const tString&in asVarName)", +[](S n, S v) { cSomaLuxEntity *p = Find(n); return p ? cString::ToVector2f(VarGet(p, v).c_str(), 0) : cVector2f(0); });
	SOMA_FUNC(e, "cVector3f Entity_GetVarVector3f(const tString&in asEntityName, const tString&in asVarName)", +[](S n, S v) { cSomaLuxEntity *p = Find(n); return p ? cString::ToVector3f(VarGet(p, v).c_str(), 0) : cVector3f(0); });
	SOMA_FUNC(e, "cColor Entity_GetVarColor(const tString&in asEntityName, const tString&in asVarName)", +[](S n, S v) { cSomaLuxEntity *p = Find(n); return p ? cString::ToColor(VarGet(p, v).c_str(), cColor(0, 0)) : cColor(0, 0); });
	SOMA_FUNC(e, "void Prop_MoveLinearTo(const tString &in asName, const tString &in asTargetEntity, float afAcceleration, float afMaxSpeed, float afSlowDownDist, bool abResetSpeed, const tString&in asCallback=\"\")",
			  +[](S n, S t, float a, float m, float d, bool r, S cb) {
				  cSomaLuxEntity *pTarget = Find(t);
				  if (pTarget == NULL)
					  return;
				  cVector3f vGoal = pTarget->GetPosition();
				  ForMatching(n, [&](cSomaLuxEntity *p) { p->MoveLinearTo(vGoal, a, m, d, r, cb); });
			  });
	SOMA_FUNC(e, "void Terminal_SetGuiActive(const tString& in asName, bool abX, float afFadeTime=0.0f)",
			  +[](S n, bool b, float f) { ForMatching(n, [b, f](cSomaLuxEntity *p) { p->SetGuiActive(b, f); }); });
	SOMA_FUNC(e, "bool Terminal_IsGuiActive(const tString& in asName)", +[](S n) { cSomaLuxEntity *p = Find(n); return p && p->mbGuiActive; });
	SOMA_FUNC(e, "void Terminal_SetShowMouse(const tString& in asPropName, bool abShow)",
			  +[](S n, bool b) { ForMatching(n, [b](cSomaLuxEntity *p) { if (p->mpImGui) p->mpImGui->mbShowMouse = b; }); });
	SOMA_FUNC(e, "void Terminal_SetUpdateWhenOutOfView(const tString& in asName, bool abX)",
			  +[](S n, bool b) { ForMatching(n, [b](cSomaLuxEntity *p) { p->mbGuiUpdateWhenOutOfView = b; }); });
	SOMA_FUNC(e, "void Terminal_ForceCacheUpdate(const tString& in asName)", +[](S n) { ForMatching(n, [](cSomaLuxEntity *p) { p->mbGuiDirty = true; }); });
	SOMA_FUNC(e, "void Terminal_SetFPSWhenIdle(const tString& in asName, float afFPS)", +[](S n, float f) { ForMatching(n, [f](cSomaLuxEntity *p) { p->mfGuiFPS = f; }); });
	{
		static auto Gui = [](S n) -> cSomaImGui * { cSomaLuxEntity *p = Find(n); return p ? p->mpImGui : NULL; };
#define TERM_STATE(TYPE, RET, ARG, INTYPE, FIELD, FLAG, DEF)                                                                                                 \
	SOMA_FUNC(e, RET " Terminal_GetImGuiState" TYPE "(const tString&in asPropName, const tString&in asVarName, " ARG " a" DEF ")",                           \
			  +[](S n, S v, INTYPE d) -> std::decay<INTYPE>::type {                                                                                              \
				  cSomaImGui *g = Gui(n);                                                                                                                    \
				  using F = decltype(cSomaImGui::cState::FIELD);                                                                                             \
				  return g ? g->GetOrSetState<F>(SomaHash64(v), &cSomaImGui::cState::FIELD, &cSomaImGui::cState::FLAG, F(d)) : d;                           \
			  });                                                                                                                                            \
	SOMA_FUNC(e, "void Terminal_SetImGuiState" TYPE "(const tString&in asPropName, const tString&in asVarName, " ARG " aVal)", +[](S n, S v, INTYPE x) {     \
		if (cSomaImGui *g = Gui(n)) { auto &st = g->State(SomaHash64(v)); st.FIELD = x; st.FLAG = true; }                                                \
	});
		TERM_STATE("Int", "int", "int", int, mlInt, mbSetInt, "lDefault=0")
		TERM_STATE("Bool", "bool", "bool", bool, mlInt, mbSetInt, "lDefault=false")
		TERM_STATE("Float", "float", "float", float, mfFloat, mbSetFloat, "fDefault=0.0f")
		TERM_STATE("Vector3f", "cVector3f", "const cVector3f&in", const cVector3f &, mvVec, mbSetVec, "vDefault")
		TERM_STATE("Color", "cColor", "const cColor&in", const cColor &, mCol, mbSetCol, "Default")
#undef TERM_STATE
		SOMA_FUNC(e, "void Terminal_IncImGuiStateInt(const tString&in asPropName, const tString&in asVarName, int alVal)",
				  +[](S n, S v, int x) { if (cSomaImGui *g = Gui(n)) { auto &st = g->State(SomaHash64(v)); st.mlInt += x; st.mbSetInt = true; } });
		SOMA_FUNC(e, "void Terminal_IncImGuiStateFloat(const tString&in asPropName, const tString&in asVarName, float afVal)",
				  +[](S n, S v, float x) { if (cSomaImGui *g = Gui(n)) { auto &st = g->State(SomaHash64(v)); st.mfFloat += x; st.mbSetFloat = true; } });
		SOMA_FUNC(e, "void Terminal_IncImGuiStateVector3f(const tString&in asPropName, const tString&in asVarName, const cVector3f&in avVal)",
				  +[](S n, S v, const cVector3f &x) { if (cSomaImGui *g = Gui(n)) { auto &st = g->State(SomaHash64(v)); st.mvVec += x; st.mbSetVec = true; } });
		SOMA_FUNC(e, "void Terminal_IncImGuiStateColor(const tString&in asPropName, const tString&in asVarName, const cColor&in aVal)",
				  +[](S n, S v, const cColor &x) { if (cSomaImGui *g = Gui(n)) { auto &st = g->State(SomaHash64(v)); st.mCol = st.mCol + x; st.mbSetCol = true; } });
		SOMA_FUNC(e, "void Terminal_FadeImGuiStateFloat(const tString&in asPropName, const tString&in asVarName, float afGoalVal, float afTime, eEasing aType=eEasing_QuadInOut, bool abReplaceIfExist=true)",
				  +[](S n, S v, float x, float t, int ease, bool r) { if (cSomaImGui *g = Gui(n)) { float f[4] = {x, 0, 0, 0}; g->Fade(SomaHash64(v), 0, f, t, ease, r); } });
		SOMA_FUNC(e, "void Terminal_FadeImGuiStateVector3f(const tString&in asPropName, const tString&in asVarName, cVector3f avGoalVal, float afTime, eEasing aType=eEasing_QuadInOut, bool abReplaceIfExist=true)",
				  +[](S n, S v, cVector3f x, float t, int ease, bool r) { if (cSomaImGui *g = Gui(n)) { float f[4] = {x.x, x.y, x.z, 0}; g->Fade(SomaHash64(v), 1, f, t, ease, r); } });
		SOMA_FUNC(e, "void Terminal_FadeImGuiStateColor(const tString&in asPropName, const tString&in asVarName, cColor aGoalVal, float afTime, eEasing aType=eEasing_QuadInOut, bool abReplaceIfExist=true)",
				  +[](S n, S v, cColor x, float t, int ease, bool r) { if (cSomaImGui *g = Gui(n)) { float f[4] = {x.r, x.g, x.b, x.a}; g->Fade(SomaHash64(v), 2, f, t, ease, r); } });
		SOMA_FUNC(e, "void Terminal_StopImGuiFade(const tString&in asPropName, const tString&in asVarName)",
				  +[](S n, S v) { if (cSomaImGui *g = Gui(n)) g->mmapFades.erase(SomaHash64(v)); });
	}
	SOMA_FUNC(e, "void Prop_StopMovement(const tString &in asPropName)", +[](S n) { ForMatching(n, [](cSomaLuxEntity *p) { p->StopMove(); }); });
	SOMA_FUNC(e, "void Prop_AlignRotation(const tString &in asName, const tString &in asTargetEntity, float afAcceleration, float afMaxSpeed, float afSlowDownDist, bool abResetSpeed, const tString&in asCallback=\"\")",
			  +[](S n, S t, float a, float m, float d, bool r, S cb) {
				  cSomaLuxEntity *pTarget = Find(t);
				  if (pTarget == NULL)
					  return;
				  cMatrixf mtxGoal = pTarget->GetMatrix();
				  ForMatching(n, [&](cSomaLuxEntity *p) { p->MoveAngularTo(mtxGoal, a, m, d, r, 0, cb); });
			  });
	SOMA_FUNC(e, "void Prop_RotateToSpeed(const tString &in asPropName, float afAcc, float afGoalSpeed, const cVector3f &in avAxis, bool abResetSpeed, const tString &in asOffsetEntity)",
			  +[](S n, float a, float s, const cVector3f &ax, bool r, S off) {
				  cSomaLuxEntity *pOff = off != "" ? Find(off) : NULL;
				  ForMatching(n, [&](cSomaLuxEntity *p) { p->RotateAtSpeed(a, s, ax, r, pOff ? cMath::MatrixMul(cMath::MatrixInverse(p->GetMatrix()), pOff->GetPosition()) : cVector3f(0)); });
			  });
	SOMA_FUNC(e, "void Prop_RotateToSpeed(const tString &in asPropName, float afAcc, float afGoalSpeed, bool abResetSpeed, const tString &in asOffsetEntity)",
			  +[](S n, float a, float s, bool r, S off) {
				  cSomaLuxEntity *pOff = off != "" ? Find(off) : NULL;
				  ForMatching(n, [&](cSomaLuxEntity *p) { p->RotateAtSpeed(a, s, p->mvRotateAxis.SqrLength() > 0 ? p->mvRotateAxis : cVector3f(0, 1, 0), r, pOff ? cMath::MatrixMul(cMath::MatrixInverse(p->GetMatrix()), pOff->GetPosition()) : cVector3f(0)); });
			  });
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
	static auto InFront = [](const cVector3f &avPos, cSomaLuxEntity *apFwd) {
		cMatrixf m = cMath::MatrixInverse(apFwd->GetMatrix());
		return cMath::Vector3Dot(cMath::Vector3Normalize(avPos - apFwd->GetPosition()), cVector3f(m.m[2][0], m.m[2][1], m.m[2][2])) > 0;
	};
	SOMA_FUNC(e, "bool Entity_EntityIsInFront(const tString &in asTargetEntity, const tString &in asForwardEntity)", +[](S t, S f) {
		cSomaLuxEntity *pT = Find(t), *pF = Find(f);
		return pT && pF && InFront(pT->GetPosition(), pF);
	});
	SOMA_FUNC(e, "bool Entity_PlayerIsInFront(const tString &in asName)", +[](S n) {
		cSomaLuxEntity *p = Find(n);
		iCharacterBody *pBody = cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetCharacterBody() : NULL;
		return p && pBody && InFront(pBody->GetPosition(), p);
	});
	SOMA_FUNC(e, "void Entity_AddImpulse(const tString &in asEntityName, const cVector3f &in avImpulse, bool abLocalSpace, bool abOnlyMainBody)",
			  +[](S n, const cVector3f &v, bool bLocal, bool bMain) {
				  ForMatching(n, [&](cSomaLuxEntity *p) {
					  cVector3f w = bLocal ? cMath::MatrixMul(p->GetMatrix().GetRotation(), v) : v;
					  for (iPhysicsBody *b : p->mvBodies)
						  if (!bMain || b == p->GetMainBody())
							  b->AddImpulse(w);
				  });
			  });
	SOMA_FUNC(e, "void Entity_AddForce(const tString &in asEntityName, const cVector3f &in avForce, bool abLocalSpace, bool abOnlyMainBody)",
			  +[](S n, const cVector3f &v, bool bLocal, bool bMain) {
				  ForMatching(n, [&](cSomaLuxEntity *p) {
					  cVector3f w = bLocal ? cMath::MatrixMul(p->GetMatrix().GetRotation(), v) : v;
					  for (iPhysicsBody *b : p->mvBodies)
						  if (!bMain || b == p->GetMainBody())
							  b->AddForce(w);
				  });
			  });
	SOMA_FUNC(e, "void Entity_AddForceFromEntity(const tString &in asEntityName, const tString &in asForceEntityName, float afForce, bool abOnlyMainBody)",
			  +[](S n, S from, float f, bool bMain) {
				  cSomaLuxEntity *pFrom = Find(from);
				  if (!pFrom) return;
				  ForMatching(n, [&](cSomaLuxEntity *p) {
					  cVector3f v = p->GetPosition() - pFrom->GetPosition();
					  float l = v.Length();
					  if (l > 0.0001f) v = v / l;
					  for (iPhysicsBody *b : p->mvBodies)
						  if (!bMain || b == p->GetMainBody())
							  b->AddForce(v * f);
				  });
			  });
	SOMA_FUNC(e, "void Entity_SetCollide(const tString &in asEntityName, bool abActive)", +[](S n, bool c) {
		ForMatching(n, [c](cSomaLuxEntity *p) {
			for (iPhysicsBody *b : p->mvBodies)
				b->SetCollide(c);
		});
	});
	SOMA_FUNC(e, "void Entity_AddTorque(const tString &in asEntityName, const cVector3f &in avTorque, bool abLocalSpace, bool abOnlyMainBody)",
			  +[](S n, const cVector3f &v, bool bLocal, bool bMain) {
				  ForMatching(n, [&](cSomaLuxEntity *p) {
					  cVector3f w = bLocal ? cMath::MatrixMul(p->GetMatrix().GetRotation(), v) : v;
					  for (iPhysicsBody *b : p->mvBodies)
						  if (!bMain || b == p->GetMainBody())
							  b->AddTorque(w);
				  });
			  });
}
