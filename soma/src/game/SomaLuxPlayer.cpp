#include "SomaLuxPlayer.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaLuxGame.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"
#include "impl/scriptarray.h"

#include <cmath>
#include <cstring>

cSomaLuxPlayer *cSomaLuxPlayer::mpInstance = NULL;
cSomaLuxInputHandler *cSomaLuxInputHandler::mpInstance = NULL;

static const char *kOnAction = "bool OnAction(int alAction, bool abPressed)";
static const char *kOnAnalog = "bool OnAnalogInput(int alAnalogId, const cVector3f &in avAmount)";

static auto IntArg(int alX)
{
	return [alX](asIScriptContext *c) { c->SetArgDWord(0, alX); };
}

void cSomaLuxMoveState::OnUpdate(float afTimeStep)
{
	cSomaLuxScriptable::OnUpdate(afTimeStep);
	iCharacterBody *pBody = mpPlayer ? mpPlayer->GetCharacterBody() : NULL;
	if (mbAutoUpdateSpeedSettings == false || pBody == NULL)
		return;
	pBody->SetMaxPositiveMoveSpeed(eCharDir_Forward, mfMaxForwardSpeed * mfMaxForwardSpeedMul);
	pBody->SetMaxNegativeMoveSpeed(eCharDir_Forward, -(mfMaxBackwardSpeed * mfMaxBackwardSpeedMul));
	pBody->SetMaxPositiveMoveSpeed(eCharDir_Right, mfMaxSidwaySpeed * mfMaxSidwaySpeedMul);
	pBody->SetMaxNegativeMoveSpeed(eCharDir_Right, -(mfMaxSidwaySpeed * mfMaxSidwaySpeedMul));
	pBody->SetMoveAcc(eCharDir_Forward, mfForwardAcc);
	pBody->SetMoveAcc(eCharDir_Right, mfSidewayAcc);
	pBody->SetMoveDeacc(eCharDir_Forward, mfForwardDeacc);
	pBody->SetMoveDeacc(eCharDir_Right, mfSidewayDeacc);
	pBody->SetMoveOppositeDirAccMul(eCharDir_Forward, mfForwardOppositeDirAccMul);
	pBody->SetMoveOppositeDirAccMul(eCharDir_Right, mfSidewayOppositeDirAccMul);
}

cSomaLuxPlayer::cSomaLuxPlayer()
{
	mpInstance = this;
	mFOVMul.mfValue = mFOVMul.mfGoal = 1;
	mAspectMul.mfValue = mAspectMul.mfGoal = 1;
}

cSomaLuxPlayer::~cSomaLuxPlayer()
{
	for (auto &it : mmapStates)
		delete it.second;
	for (auto &it : mmapMoveStates)
		delete it.second;
	if (mpInstance == this)
		mpInstance = NULL;
}

void cSomaLuxPlayer::SetCamera(cCamera *apCamera)
{
	mpCamera = apCamera;
	cConfigFile *pGame = SomaGameConfig();
	mfDefaultFOV = pGame->GetFloat("Player", "FOV", 70);
	float fMin = pGame->GetFloat("Player", "CameraPitchLimit_Min", -70);
	float fMax = pGame->GetFloat("Player", "CameraPitchLimit_Max", 70);
	mpCamera->SetNearClipPlane(pGame->GetFloat("Player", "NearClipPlane", 0.03f));
	mpCamera->SetFarClipPlane(pGame->GetFloat("Player", "FarClipPlane", 1000));
	mFOV.mfValue = mFOV.mfGoal = mfDefaultFOV;
	mpCamera->SetPitchLimits(cMath::ToRad(fMin), cMath::ToRad(fMax));
}

void cSomaLuxPlayer::SetCharacterBody(iCharacterBody *apBody)
{
	mpCharBody = apBody;
	if (mpCharBody)
	{
		mpCharBody->SetCamera(mpCamera);
		mpCharBody->SetCollideFlags(SomaCollideFlag("+player"));
	}
}

void cSomaLuxPlayer::AddState(const tString &asName, int alId, const tString &asFile, const tString &asClass)
{
	cSomaLuxPlayerState *pState = new cSomaLuxPlayerState();
	pState->msName = asName;
	pState->msScriptName = asName;
	pState->mlId = alId;
	pState->mpPlayer = this;
	if (pState->LoadScript(mpRuntime, asFile, asClass, "cLuxPlayerState") == false)
	{
		Error("SOMA script: could not load player state %s\n", asName.c_str());
		delete pState;
		return;
	}
	mmapStates[alId] = pState;
	if (mpState == NULL)
	{
		mpState = pState;
		mpState->Call("void OnEnterState(int alPrevStateId)", IntArg(-1));
	}
}

void cSomaLuxPlayer::AddMoveState(const tString &asName, int alId, const tString &asFile, const tString &asClass)
{
	cSomaLuxMoveState *pState = new cSomaLuxMoveState();
	pState->msName = asName;
	pState->msScriptName = asName;
	pState->mlId = alId;
	pState->mpPlayer = this;
	if (pState->LoadScript(mpRuntime, asFile, asClass, "cLuxMoveState") == false)
	{
		Error("SOMA script: could not load move state %s\n", asName.c_str());
		delete pState;
		return;
	}
	mmapMoveStates[alId] = pState;
	if (mpMoveState == NULL)
	{
		mpMoveState = pState;
		mpMoveState->Call("void OnEnterState(int alPrevStateId)", IntArg(-1));
	}
}

void cSomaLuxPlayer::ChangeState(int alId)
{
	auto it = mmapStates.find(alId);
	if (it == mmapStates.end())
	{
		Error("Could not change to state id %d. It does not exist!\n", alId);
		return;
	}
	if (it->second == mpState)
		return;
	cSomaLuxPlayerState *pPrev = mpState;
	if (pPrev)
		pPrev->Call("void OnLeaveState(int alNextStateId)", IntArg(alId));
	mpState = it->second;
	mpState->Call("void OnEnterState(int alPrevStateId)", IntArg(pPrev ? pPrev->mlId : -1));
}

void cSomaLuxPlayer::ChangeMoveState(int alId)
{
	auto it = mmapMoveStates.find(alId);
	if (it == mmapMoveStates.end())
	{
		Error("Could not change to move state id %d. It does not exist!\n", alId);
		return;
	}
	if (it->second == mpMoveState)
		return;
	cSomaLuxMoveState *pPrev = mpMoveState;
	if (pPrev)
		pPrev->Call("void OnLeaveState(int alNextStateId)", IntArg(alId));
	mpMoveState = it->second;
	mpMoveState->Call("void OnEnterState(int alPrevStateId)", IntArg(pPrev ? pPrev->mlId : -1));
}

void cSomaLuxPlayer::PlaceAtStart(const cVector3f &avFeetPos, float afYaw, bool abCrouching)
{
	if (mpCharBody == NULL)
		return;
	cVector3f vPos = avFeetPos;
	bool bScript = Call("void SetupStartPos(const cVector3f&in avPos, float afAngle, bool abCrouching)", [&](asIScriptContext *c) {
		c->SetArgObject(0, &vPos);
		c->SetArgFloat(1, afYaw);
		c->SetArgByte(2, abCrouching);
	});
	if (bScript == false)
	{
		mpCharBody->SetFeetPosition(avFeetPos);
		mpCharBody->SetYaw(afYaw);
	}
	mpCharBody->SetForceVelocity(0);
	mpCharBody->Update(0.001f);
	if (mpCamera)
	{
		mpCamera->SetYaw(afYaw);
		mpCamera->SetPitch(0);
	}
}

void cSomaLuxPlayer::OnUpdate(float afTimeStep)
{
	cSomaLuxScriptable::OnUpdate(afTimeStep);
	if (mpState)
		mpState->OnUpdate(afTimeStep);
	if (mpMoveState)
		mpMoveState->OnUpdate(afTimeStep);

	mfTimeSincePhysicsInteraction += afTimeStep;
	if (mpCharBody)
	{
		cVector3f vVel = mpCharBody->GetVelocity(afTimeStep);
		vVel.y = 0;
		float fSpeed = vVel.Length();
		mfAverageMoveSpeed = mfAverageMoveSpeed * 0.9f + fSpeed * 0.1f;
		if (fSpeed > 0.001f)
			mvAverageMoveDirection = mvAverageMoveDirection * 0.9f + (vVel / fSpeed) * 0.1f;
	}
	UpdateCamera(afTimeStep);
}

void cSomaLuxPlayer::OnPostUpdate(float afTimeStep)
{
	cSomaLuxScriptable::OnPostUpdate(afTimeStep);
	if (mpState)
		mpState->OnPostUpdate(afTimeStep);
	if (mpMoveState)
		mpMoveState->OnPostUpdate(afTimeStep);
}

void cSomaLuxPlayer::OnMessage(const char *apDecl)
{
	cSomaLuxScriptable::OnMessage(apDecl);
	for (auto &it : mmapStates)
		it.second->OnMessage(apDecl);
	for (auto &it : mmapMoveStates)
		it.second->OnMessage(apDecl);
}

void cSomaLuxPlayer::OnMapMessage(const char *apDecl, void *apMap)
{
	bool bDestroy = strcmp(apDecl, "void DestroyWorldEntities(cLuxMap @apMap)") == 0;
	cSomaLuxScriptable::OnMapMessage(apDecl, apMap);
	for (auto &it : mmapStates)
		it.second->OnMapMessage(apDecl, apMap);
	for (auto &it : mmapMoveStates)
		it.second->OnMapMessage(apDecl, apMap);
	// The character body belongs to the map's physics world
	if (bDestroy)
		mpCharBody = NULL;
}

void cSomaLuxPlayer::OnAction(int alAction, bool abPressed)
{
	if (mbActive == false)
		return;
	auto args = [=](asIScriptContext *c) { c->SetArgDWord(0, alAction); c->SetArgByte(1, abPressed); };
	if (mpState && mpState->CallBool(kOnAction, args, true) == false)
		return;
	if (mpMoveState && mpMoveState->CallBool(kOnAction, args, true) == false)
		return;
	cSomaLuxScriptable::OnAction(alAction, abPressed);
}

void cSomaLuxPlayer::OnAnalogInput(int alAnalogId, const cVector3f &avAmount)
{
	if (mbActive == false)
		return;
	auto args = [&](asIScriptContext *c) { c->SetArgDWord(0, alAnalogId); c->SetArgAddress(1, (void *)&avAmount); };
	if (mpState && mpState->CallBool(kOnAnalog, args, true) == false)
		return;
	if (mpMoveState && mpMoveState->CallBool(kOnAnalog, args, true) == false)
		return;
	cSomaLuxScriptable::OnAnalogInput(alAnalogId, avAmount);
}

static float FadeTowards(cSomaLuxPlayer::cFadeValue &aX, float afTimeStep)
{
	float fDiff = aX.mfGoal - aX.mfValue;
	float fSpeed = aX.mfSpeedMul > 0 ? std::fabs(fDiff) * aX.mfSpeedMul : aX.mfSpeed;
	if (aX.mfMaxSpeed > 0)
		fSpeed = std::min(fSpeed, aX.mfMaxSpeed);
	if (fSpeed <= 0 || std::fabs(fDiff) <= std::max(fSpeed * afTimeStep, 0.0001f))
		aX.mfValue = aX.mfGoal;
	else
		aX.mfValue += fDiff > 0 ? fSpeed * afTimeStep : -fSpeed * afTimeStep;
	return aX.mfValue;
}

static float AngleDelta(float afFrom, float afTo)
{
	float d = std::fmod(afTo - afFrom + kPif, k2Pif);
	if (d < 0)
		d += k2Pif;
	return d - kPif;
}

void cSomaLuxPlayer::UpdateCamera(float afTimeStep)
{
	if (mpCamera == NULL)
		return;

	cVector3f vAdd = mvBaseCameraPosAdd;
	for (auto &it : mmapCameraPosAdd)
	{
		cMoveVector &m = it.second;
		if (m.mbMoving)
		{
			cVector3f vDelta = m.mvGoal - m.mvValue;
			float fDist = vDelta.Length();
			m.mfSpeed = std::min(m.mfSpeed + m.mfAcc * afTimeStep, m.mfMaxSpeed);
			float fSpeed = m.mfSpeed;
			if (m.mfSlowdownDist > 0 && fDist < m.mfSlowdownDist)
				fSpeed *= std::max(fDist / m.mfSlowdownDist, 0.1f);
			if (fDist <= fSpeed * afTimeStep || fDist < 0.0001f)
			{
				m.mvValue = m.mvGoal;
				m.mbMoving = false;
				m.mfSpeed = 0;
			}
			else
				m.mvValue += vDelta * (fSpeed * afTimeStep / fDist);
		}
		vAdd += m.mvValue;
	}
	if (mpCharBody)
		mpCharBody->SetCameraPosAdd(vAdd);

	float fRoll = 0;
	for (auto &it : mmapCameraRoll)
		fRoll += FadeTowards(it.second, afTimeStep);
	mpCamera->SetRoll(fRoll);

	FadeTowards(mFOV, afTimeStep);
	FadeTowards(mFOVMul, afTimeStep);
	mpCamera->SetFOV(cMath::ToRad(mFOV.mfValue) * mFOVMul.mfValue);
	FadeTowards(mAspectMul, afTimeStep);

	if (mpCharBody == NULL)
		return;

	if (mbCameraRotateActive)
	{
		cVector3f vTarget = mbCameraRotateLocal ? mpCamera->GetPosition() + mvCameraRotateTarget : mvCameraRotateTarget;
		cVector3f vDir = vTarget - mpCamera->GetPosition();
		float fYaw = std::atan2(-vDir.x, -vDir.z);
		float fPitch = std::atan2(vDir.y, std::sqrt(vDir.x * vDir.x + vDir.z * vDir.z));
		float fDYaw = AngleDelta(mpCharBody->GetYaw(), fYaw);
		float fDPitch = fPitch - mpCamera->GetPitch();
		mfCameraRotateSpeed = std::min(mfCameraRotateSpeed + mfCameraRotateAcc * afTimeStep, mfCameraRotateMaxSpeed);
		float fMax = std::min(mfCameraRotateSpeed, std::sqrt(fDYaw * fDYaw + fDPitch * fDPitch) * mfCameraRotateSpeedMul) * afTimeStep;
		float fLen = std::sqrt(fDYaw * fDYaw + fDPitch * fDPitch);
		if (fLen > 0.0001f)
		{
			float f = std::min(fMax / fLen, 1.0f);
			mpCharBody->AddYaw(fDYaw * f);
			mpCamera->AddPitch(fDPitch * f);
		}
	}

	if (mbAutomoveActive)
	{
		cVector3f vDelta = mvAutomoveTarget - mpCharBody->GetFeetPosition();
		vDelta.y = 0;
		float fDist = vDelta.Length();
		if (fDist < 0.05f)
			mbAutomoveActive = false;
		else
		{
			cVector3f vFwd = mpCharBody->GetForward(), vRight = mpCharBody->GetRight();
			vFwd.y = vRight.y = 0;
			vFwd.Normalize();
			vRight.Normalize();
			float fMul = std::min(1.0f, fDist * mfAutomoveSpeedMul);
			mpCharBody->Move(eCharDir_Forward, cMath::Vector3Dot(vDelta, vFwd) / fDist * fMul);
			mpCharBody->Move(eCharDir_Right, cMath::Vector3Dot(vDelta, vRight) / fDist * fMul);
		}
	}

	if (mpCharBody->GetCamera() == mpCamera)
		mpCamera->SetYaw(mpCharBody->GetYaw());
}

cSomaLuxInputHandler::cSomaLuxInputHandler()
{
	mpInstance = this;
	mlMaxSmoothMousePos = SomaGameConfig()->GetInt("Input", "MaxSmoothMousePos", 7);
	mfPrevSmoothMousePosMul = SomaGameConfig()->GetFloat("Input", "PrevSmoothMousePosMul", 0.7f);
}

void cSomaLuxInputHandler::CreateAction(const cLuxAction &aAction)
{
	cInput *pInput = gpSomaBase->mpEngine->GetInput();
	if (cAction *pOld = pInput->GetAction(aAction.mlId))
		pInput->DestroyAction(pOld);
	if (cAction *pOld = pInput->GetAction(aAction.msName))
		pInput->DestroyAction(pOld);
	for (size_t i = 0; i < mvActions.size(); ++i)
		if (mvActions[i].mlId == aAction.mlId)
		{
			mvActions.erase(mvActions.begin() + i);
			break;
		}
	pInput->CreateAction(aAction.msName, aAction.mlId);
	mvActions.push_back(aAction);
}

void cSomaLuxInputHandler::CreateActionInput(const tString &asInput, int alActionId)
{
	cInput *pInput = gpSomaBase->mpEngine->GetInput();
	cAction *pAction = pInput->GetAction(alActionId);
	if (pAction == NULL)
	{
		Warning("Could not add input %s for action %d. Action does not exist!\n", asInput.c_str(), alActionId);
		return;
	}
	tString sSep = ".";
	tStringVec vParts;
	cString::GetStringVec(asInput, vParts, &sSep);
	if (vParts.size() < 2)
		return;
	if (vParts[0] == "Keyboard")
	{
		eKey key = pInput->GetKeyboard()->StringToKey(vParts[1]);
		if (key != eKey_LastEnum)
			pAction->AddKey(key);
	}
	else if (vParts[0] == "MouseButton")
	{
		eMouseButton button = pInput->GetMouse()->StringToButton(vParts[1]);
		if (button != eMouseButton_LastEnum)
			pAction->AddMouseButton(button);
	}
}

tString cSomaLuxInputHandler::GetActionName(int alId, bool abGamepad)
{
	const cLuxAction *pAny = NULL;
	for (const cLuxAction &a : mvActions)
		if (a.mlId == alId)
		{
			if (a.mbGamepad == abGamepad)
				return a.msName;
			pAny = &a;
		}
	return pAny ? pAny->msName : "";
}

cSomaLuxInputHandler::cGamepadProfile *cSomaLuxInputHandler::GetGamepadProfile(const tString &asName)
{
	for (cGamepadProfile &p : mvGamepadProfiles)
		if (p.msName == asName)
			return &p;
	return NULL;
}

cSomaLuxInputHandler::cGamepadPreset *cSomaLuxInputHandler::GetGamepadPreset()
{
	cGamepadProfile *pProfile = GetGamepadProfile(msGamepadProfile);
	if (pProfile == NULL)
		return NULL;
	cGamepadPreset *pLast = NULL;
	for (cGamepadPreset &p : pProfile->mvPresets)
		if (p.msName == msGamepadPreset)
			pLast = &p;
	return pLast;
}

void cSomaLuxInputHandler::GetActionsAssociatedToGamepadControl(const tString &asProfile, const tString &asPreset, const tString &asControl, tString &asActions)
{
	cGamepadProfile *pProfile = GetGamepadProfile(asProfile);
	if (pProfile == NULL)
		return;
	for (const cGamepadPreset &p : pProfile->mvPresets)
	{
		if (p.msName != asPreset)
			continue;
		for (size_t i = 0; i < p.mvActions.size(); ++i)
			if (i < p.mvBindings.size() && p.mvBindings[i] == asControl)
				asActions += GetActionName(p.mvActions[i], i < p.mvAnalog.size() && p.mvAnalog[i]) + "/";
		if (asActions.empty() == false)
			asActions.pop_back();
	}
}

bool cSomaLuxInputHandler::FetchGamepadInputLayoutString(const tString &asInput, tString &asPrefix, tString &asLayout)
{
	tString sSep = ".";
	tStringVec vParts;
	cString::GetStringVec(cString::ToLowerCase(asInput), vParts, &sSep);
	asPrefix.clear();
	asLayout.clear();
	cGamepadProfile *pProfile = vParts.size() >= 4 && vParts[0] == "gamepad" ? GetGamepadProfile(msGamepadProfile) : NULL;
	if (pProfile == NULL)
		return false;
	asPrefix = pProfile->msPrefix;
	int lIdx = cString::ToInt(vParts[3].c_str(), 0);
	const tStringVec *pVec = vParts[2] == "button" ? &pProfile->mvButtons : vParts[2] == "axis" ? &pProfile->mvAxes : NULL;
	if (pVec && lIdx >= 0 && lIdx < (int)pVec->size())
		asLayout = (*pVec)[lIdx];
	else
		asPrefix.clear();
	return true;
}

void cSomaLuxInputHandler::LoadUserConfig()
{
	cConfigFile *pCfg = SomaUserConfig();
	if (pCfg == NULL)
		return;
	mbInvertMouse = pCfg->GetBool("Input", "InvertMouse", false);
	mbSmoothMouse = pCfg->GetBool("Input", "SmoothMouse", true);
	mfMouseSensitivity = pCfg->GetFloat("Input", "MouseSensitivity", 1.0f);
	mfGamepadSensitivity = pCfg->GetFloat("Input", "GamepadSensitivity", 2.0f);
}

void cSomaLuxInputHandler::LoadScript()
{
	mvGamepadProfiles.clear();
	Call("void CreateGamepadProfiles()");
	Call("void CreateInputLayoutMapping()");
	LoadKeyConfig();
}

void cSomaLuxInputHandler::LoadKeyConfig()
{
	cInput *pInput = gpSomaBase->mpEngine->GetInput();
	for (const cLuxAction &a : mvActions)
		if (cAction *p = pInput->GetAction(a.mlId))
			pInput->DestroyAction(p);
	mvActions.clear();
	Call("void CreateActions()");
	CallWithObject("void LoadKeyConfig(cConfigFile@ apKeyConfig)", SomaKeyConfig());
	Log("SOMA script: %d input actions\n", (int)mvActions.size());
}

void cSomaLuxInputHandler::LatchActions()
{
	cInput *pInput = gpSomaBase->mpEngine->GetInput();
	for (int i = 0; i < kMaxActions; ++i)
	{
		mvPrevDown[i] = mvDown[i];
		mvDown[i] = pInput->GetAction(i) && pInput->IsTriggerd(i);
	}
}

void cSomaLuxInputHandler::UpdateInput(float afTimeStep, bool abGameInput)
{
	if (abGameInput == false)
		return;
	cInput *pInput = gpSomaBase->mpEngine->GetInput();
	cSomaLuxGame *pGame = cSomaLuxGame::Get();
	for (size_t i = 0; i < mvActions.size(); ++i)
	{
		cLuxAction a = mvActions[i];
		if (a.mbGamepad)
			continue;
		if (BecameDown(a.mlId))
			pGame->BroadcastAction(a.mlId, true);
		else if (BecameUp(a.mlId))
			pGame->BroadcastAction(a.mlId, false);
		if (a.mlAnalogId >= 0 && a.mlAxis >= 0 && a.mlAxis < 3 && pInput->IsTriggerd(a.mlId) && a.mfMul != 0)
		{
			cVector3f v(0);
			v.v[a.mlAxis] = a.mfMul;
			pGame->BroadcastAnalog(a.mlAnalogId, v);
		}
	}

	float fHeight = gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat().y;
	cVector2f v = cVector2f((float)pGame->mvMouseRel.x, (float)pGame->mvMouseRel.y) * mfMouseSensitivity / fHeight;
	if (mbSmoothMouse)
	{
		mlstSmoothMousePos.push_front(v);
		if ((int)mlstSmoothMousePos.size() > mlMaxSmoothMousePos)
			mlstSmoothMousePos.pop_back();
		cVector2f vSum(0);
		float fMul = 1, fTotal = 0;
		for (const cVector2f &p : mlstSmoothMousePos)
		{
			vSum += p * fMul;
			fTotal += fMul;
			fMul *= mfPrevSmoothMousePosMul;
		}
		v = vSum / fTotal;
	}
	if (mbInvertMouse)
		v.y = -v.y;
	if (v.x != 0 || v.y != 0)
		pGame->BroadcastAnalog(0, cVector3f(v.x, v.y, 0));
}

void cSomaLuxPlayer::RegisterNatives(asIScriptEngine *e)
{
	typedef cSomaLuxPlayer P;
	typedef const tString &S;
	typedef const cVector3f &V;
	const char *T = "cLuxPlayer";

	SOMA_METHOD(e, T, "void SetActive(bool abX)", +[](P *p, bool b) { p->SetActive(b); });
	SOMA_METHOD(e, T, "bool IsActive()", +[](P *p) { return p->IsActive(); });
	SOMA_METHOD(e, T, "cCamera@ GetCamera()", +[](P *p) { return p->GetCamera(); });
	SOMA_METHOD(e, T, "iCharacterBody@ GetCharacterBody()", +[](P *p) { return p->GetCharacterBody(); });
	SOMA_METHOD(e, T, "void SetCharacterBody(iCharacterBody@ apBody)", +[](P *p, iCharacterBody *b) { p->SetCharacterBody(b); });
	SOMA_METHOD(e, T, "bool IsInLiquid()", +[](P *) { return false; });
	SOMA_METHOD(e, T, "float GetLiquidHeight()", +[](P *) { return -10000.0f; });
	SOMA_METHOD(e, T, "float GetAverageMoveSpeed()", +[](P *p) { return p->mfAverageMoveSpeed; });
	SOMA_METHOD(e, T, "const cVector3f& GetAverageMoveDirection()", +[](P *p) -> const cVector3f & { return p->mvAverageMoveDirection; });
	SOMA_METHOD(e, T, "void ChangeState(int alId)", +[](P *p, int id) { p->ChangeState(id); });
	SOMA_METHOD(e, T, "int GetCurrentStateId()", +[](P *p) { return p->GetState() ? p->GetState()->mlId : -1; });
	static tString sEmpty;
	SOMA_METHOD(e, T, "const tString& GetCurrentStateName()", +[](P *p) -> const tString & { return p->GetState() ? p->GetState()->msName : sEmpty; });
	SOMA_METHOD(e, T, "void ChangeMoveState(int alId)", +[](P *p, int id) { p->ChangeMoveState(id); });
	SOMA_METHOD(e, T, "int GetCurrentMoveStateId()", +[](P *p) { return p->GetMoveState() ? p->GetMoveState()->mlId : -1; });
	SOMA_METHOD(e, T, "const tString& GetCurrentMoveStateName()", +[](P *p) -> const tString & { return p->GetMoveState() ? p->GetMoveState()->msName : sEmpty; });
	SOMA_METHOD(e, T, "cLuxMoveState@ GetCurrentMoveState()", +[](P *p) { return p->GetMoveState(); });
	SOMA_METHOD(e, T, "void AddState(const tString&in asName, int alId, const tString&in asScriptFile, const tString&in asScriptClass)",
				+[](P *p, S n, int id, S f, S c) { p->AddState(n, id, f, c); });
	SOMA_METHOD(e, T, "void AddMoveState(const tString&in asName, int alId, const tString&in asScriptFile, const tString&in asScriptClass)",
				+[](P *p, S n, int id, S f, S c) { p->AddMoveState(n, id, f, c); });
	SOMA_METHOD(e, T, "void GiveDamage(float afAmount, int alStrength, int aType, float afMinHealth, const tString&in asSource)",
				+[](P *p, float a, int, int, float fMin, S) { p->mfHealth = std::max(std::min(p->mfHealth, std::max(p->mfHealth - a, fMin)), 0.0f); });
	SOMA_METHOD(e, T, "void SetBaseCameraPosAdd(const cVector3f&in avVec)", +[](P *p, V v) { p->mvBaseCameraPosAdd = v; });
	SOMA_METHOD(e, T, "const cVector3f& GetBaseCameraPosAdd()", +[](P *p) -> const cVector3f & { return p->mvBaseCameraPosAdd; });
	SOMA_METHOD(e, T, "void SetCameraPosAdd(int alType, const cVector3f&in avVector)", +[](P *p, int t, V v) {
		cSomaLuxPlayer::cMoveVector &m = p->mmapCameraPosAdd[t];
		m.mvValue = m.mvGoal = v;
		m.mbMoving = false;
	});
	SOMA_METHOD(e, T, "const cVector3f& GetCameraPosAdd(int alType)", +[](P *p, int t) -> const cVector3f & { return p->mmapCameraPosAdd[t].mvValue; });
	SOMA_METHOD(e, T, "const cVector3f& GetCameraPosAddGoal(int alType)", +[](P *p, int t) -> const cVector3f & { return p->mmapCameraPosAdd[t].mvGoal; });
	SOMA_METHOD(e, T, "const cVector3f& GetCameraPosAddSum()", +[](P *p) -> const cVector3f & {
		static cVector3f vSum;
		vSum = p->mvBaseCameraPosAdd;
		for (auto &it : p->mmapCameraPosAdd)
			vSum += it.second.mvValue;
		return vSum;
	});
	SOMA_METHOD(e, T, "void MoveCameraPosAdd(int alType, const cVector3f&in avGoal,float afAcc, float afSpeed, float afSlowdownDist)",
				+[](P *p, int t, V g, float acc, float speed, float slow) {
					cSomaLuxPlayer::cMoveVector &m = p->mmapCameraPosAdd[t];
					m.mvGoal = g;
					m.mfAcc = acc;
					m.mfMaxSpeed = speed;
					m.mfSlowdownDist = slow;
					m.mbMoving = true;
				});
	SOMA_METHOD(e, T, "void EnableCameraLock(float afLocalYawMin, float afLocalYawMax, float afLocalPitchMin, float afLocalPitchMax)",
				+[](P *p, float, float, float pmin, float pmax) { if (p->GetCamera()) p->GetCamera()->SetPitchLimits(cMath::ToRad(pmin), cMath::ToRad(pmax)); });
	SOMA_METHOD(e, T, "void DisableCameraLock()", +[](P *p) { if (p->GetCamera()) p->GetCamera()->SetPitchLimits(cMath::ToRad(-70.0f), cMath::ToRad(70.0f)); });
	SOMA_METHOD(e, T, "void RotateCameraTowards(float afAcc, float afSpeedMul, float afMaxSpeed, const cVector3f&in avLookAtPos, bool abLocalCoord)",
				+[](P *p, float acc, float mul, float max, V pos, bool local) {
					p->mbCameraRotateActive = true;
					p->mfCameraRotateAcc = acc;
					p->mfCameraRotateSpeedMul = mul;
					p->mfCameraRotateMaxSpeed = max;
					p->mvCameraRotateTarget = pos;
					p->mbCameraRotateLocal = local;
				});
	SOMA_METHOD(e, T, "void SetRotateCameraTarget(const cVector3f&in avLookAtPos, bool abLocalCoord)",
				+[](P *p, V pos, bool local) { p->mvCameraRotateTarget = pos; p->mbCameraRotateLocal = local; });
	SOMA_METHOD(e, T, "float GetRotateCameraTargetDistance()", +[](P *p) {
		return p->GetCamera() ? (p->mvCameraRotateTarget - p->GetCamera()->GetPosition()).Length() : 0.0f;
	});
	SOMA_METHOD(e, T, "bool IsCameraRotateActive()", +[](P *p) { return p->mbCameraRotateActive; });
	SOMA_METHOD(e, T, "void StopCameraRotate(float afDeacc)", +[](P *p, float) { p->mbCameraRotateActive = false; p->mfCameraRotateSpeed = 0; });
	SOMA_METHOD(e, T, "void FadeCameraFOVMulTo(float afX, float afSpeed)", +[](P *p, float x, float s) { p->mFOVMul.mfGoal = x; p->mFOVMul.mfSpeed = s; p->mFOVMul.mfSpeedMul = 0; });
	SOMA_METHOD(e, T, "void FadeCameraAspectMulTo(float afX, float afSpeed)", +[](P *p, float x, float s) { p->mAspectMul.mfGoal = x; p->mAspectMul.mfSpeed = s; p->mAspectMul.mfSpeedMul = 0; });
	SOMA_METHOD(e, T, "void FadeCameraRollTo(int alId, float afX, float afSpeedMul, float afMaxSpeed)", +[](P *p, int id, float x, float mul, float max) {
		cSomaLuxPlayer::cFadeValue &f = p->mmapCameraRoll[id];
		f.mfGoal = x;
		f.mfSpeedMul = mul;
		f.mfMaxSpeed = max;
	});
	SOMA_METHOD(e, T, "void SetCameraRoll(int alId, float afX)", +[](P *p, int id, float x) {
		cSomaLuxPlayer::cFadeValue &f = p->mmapCameraRoll[id];
		f.mfValue = f.mfGoal = x;
	});
	SOMA_METHOD(e, T, "float GetDefaultFOV()", +[](P *p) { return cMath::ToRad(p->GetDefaultFOV()); });
	SOMA_METHOD(e, T, "void FadeCameraFOVTo(float afTargetFOV, float afSpeed)", +[](P *p, float x, float s) {
		p->mFOV.mfGoal = x < 0 ? p->GetDefaultFOV() : cMath::ToDeg(x);
		p->mFOV.mfSpeed = cMath::ToDeg(s);
		p->mFOV.mfSpeedMul = 0;
	});
	SOMA_METHOD(e, T, "void AutomoveCharBodyTo(float afAcc, float afSpeedMul, float afMaxSpeed, const cVector3f&in avPosition)",
				+[](P *p, float, float mul, float, V pos) {
					p->mbAutomoveActive = true;
					p->mfAutomoveSpeedMul = mul;
					p->mvAutomoveTarget = pos;
				});
	SOMA_METHOD(e, T, "void SetAutomoveCharBodyTarget(const cVector3f&in avPosition)", +[](P *p, V pos) { p->mvAutomoveTarget = pos; });
	SOMA_METHOD(e, T, "float GetAutoMoveTargetDistance()", +[](P *p) {
		return p->GetCharacterBody() ? (p->mvAutomoveTarget - p->GetCharacterBody()->GetFeetPosition()).Length() : 0.0f;
	});
	SOMA_METHOD(e, T, "bool IsAutomoveCharBodyActive()", +[](P *p) { return p->mbAutomoveActive; });
	SOMA_METHOD(e, T, "void StopAutomoveCharBody()", +[](P *p) { p->mbAutomoveActive = false; });
	SOMA_METHOD(e, T, "void SetHealth(float afX)", +[](P *p, float x) { p->mfHealth = std::min(x, p->mfMaxHealth); });
	SOMA_METHOD(e, T, "float GetHealth()", +[](P *p) { return p->mfHealth; });
	SOMA_METHOD(e, T, "void SetMaxHealth(float afX)", +[](P *p, float x) { p->mfMaxHealth = x; });
	SOMA_METHOD(e, T, "float GetMaxHealth()", +[](P *p) { return p->mfMaxHealth; });
	SOMA_METHOD(e, T, "void AddHealth(float afX, float afMinHealth)", +[](P *p, float x, float fMin) {
		p->mfHealth = std::min(std::max(p->mfHealth + x, fMin), p->mfMaxHealth);
	});
	SOMA_METHOD(e, T, "bool IsDead()", +[](P *p) { return p->mfHealth <= 0; });
	SOMA_METHOD(e, T, "bool HasCollideCallbacks()", +[](P *) {
		cSomaLuxEntity *pEnt = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(tString("Player")) : NULL;
		return pEnt && pEnt->mvCollideCallbacks.empty() == false;
	});
	SOMA_METHOD(e, T, "void AddCollideCallback(iLuxEntity @apEntity, const tString&in asCallbackFunc)", +[](P *, cSomaLuxEntity *c, S f) {
		cSomaLuxEntity *pEnt = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(tString("Player")) : NULL;
		if (pEnt && c)
			pEnt->mvCollideCallbacks.push_back(cSomaLuxEntity::cCollideCallback{c->msName, f});
	});
	SOMA_METHOD(e, T, "void RemoveCollideCallback(const tString&in asEntityName)", +[](P *, S n) {
		cSomaLuxEntity *pEnt = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(tString("Player")) : NULL;
		if (pEnt)
			pEnt->RemoveCollideCallbacks(n);
	});
	SOMA_METHOD(e, T, "bool CheckEntityCollision(iLuxEntity@ apEntity)", +[](P *, cSomaLuxEntity *c) {
		cSomaLuxEntity *pEnt = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(tString("Player")) : NULL;
		return pEnt && c && SomaEntitiesCollide(pEnt, c);
	});
	SOMA_METHOD(e, T, "float GetVisibilityRangeMul()", +[](P *p) {
		float f = 1;
		for (auto &it : p->mmapVisibilityRangeMul)
			f *= it.second;
		return f;
	});
	SOMA_METHOD(e, T, "void SetVisibilityRangeMul(int alId, float afX)", +[](P *p, int id, float x) { p->mmapVisibilityRangeMul[id] = x; });
	SOMA_METHOD(e, T, "void SetVisibilityMaxRange(int alId, float afX)", +[](P *p, int id, float x) { p->mmapVisibilityMaxRange[id] = x; });
	SOMA_METHOD(e, T, "float GetVisibilityMaxRange()", +[](P *p) {
		float f = 10000;
		for (auto &it : p->mmapVisibilityMaxRange)
			if (it.second >= 0)
				f = std::min(f, it.second);
		return f;
	});
	SOMA_METHOD(e, T, "float GetTimeSincePhysicsObjectInteraction()", +[](P *p) { return p->mfTimeSincePhysicsInteraction; });
	SOMA_METHOD(e, T, "void ResetTimeSincePhysicsObjectInteraction()", +[](P *p) { p->mfTimeSincePhysicsInteraction = 0; });
	SOMA_METHOD(e, T, "void SetMaxCameraTrackingAmount(int alSize)", +[](P *, int) {});
	SOMA_METHOD(e, T, "const cVector2f& GetCameraTrackingAvgMovement()", +[](P *p) -> const cVector2f & { return p->mvCameraTrackingAvg; });
	SOMA_METHOD(e, T, "void ResetBasicProperties()", +[](P *) {});

	cSomaLuxPlayerState state;
	auto stOff = [&](void *apField) { return (int)((char *)apField - (char *)(cSomaLuxScriptable *)&state); };
	e->RegisterObjectProperty("cLuxPlayerState", "int mlId", stOff(&state.mlId));
	e->RegisterObjectProperty("cLuxPlayerState", "cLuxPlayer @mpPlayer", stOff(&state.mpPlayer));
	SOMA_METHOD(e, "cLuxPlayerState", "const tString& GetName()", +[](cSomaLuxPlayerState *s) -> const tString & { return s->msName; });
	SOMA_METHOD(e, "cLuxPlayerState", "int GetId()", +[](cSomaLuxPlayerState *s) { return s->mlId; });

	cSomaLuxMoveState move;
	auto mvOff = [&](void *apField) { return (int)((char *)apField - (char *)(cSomaLuxScriptable *)&move); };
	const char *M = "cLuxMoveState";
	e->RegisterObjectProperty(M, "int mlId", mvOff(&move.mlId));
	e->RegisterObjectProperty(M, "cLuxPlayer @mpPlayer", mvOff(&move.mpPlayer));
	struct cProp { const char *mpDecl; float *mpField; };
	const cProp vProps[] = {
		{"float mfMaxForwardSpeed", &move.mfMaxForwardSpeed}, {"float mfMaxBackwardSpeed", &move.mfMaxBackwardSpeed},
		{"float mfMaxSidwaySpeed", &move.mfMaxSidwaySpeed}, {"float mfForwardAcc", &move.mfForwardAcc},
		{"float mfSidewayAcc", &move.mfSidewayAcc}, {"float mfForwardDeacc", &move.mfForwardDeacc},
		{"float mfSidewayDeacc", &move.mfSidewayDeacc}, {"float mfForwardOppositeDirAccMul", &move.mfForwardOppositeDirAccMul},
		{"float mfSidewayOppositeDirAccMul", &move.mfSidewayOppositeDirAccMul}, {"float mfMaxForwardSpeedMul", &move.mfMaxForwardSpeedMul},
		{"float mfMaxBackwardSpeedMul", &move.mfMaxBackwardSpeedMul}, {"float mfMaxSidwaySpeedMul", &move.mfMaxSidwaySpeedMul},
	};
	for (const cProp &p : vProps)
		e->RegisterObjectProperty(M, p.mpDecl, mvOff(p.mpField));
	SOMA_METHOD(e, M, "const tString& GetName()", +[](cSomaLuxMoveState *s) -> const tString & { return s->msName; });
	SOMA_METHOD(e, M, "int GetId()", +[](cSomaLuxMoveState *s) { return s->mlId; });
	SOMA_METHOD(e, M, "void SetAutoUpdateSpeedSettings(bool abX)", +[](cSomaLuxMoveState *s, bool b) { s->mbAutoUpdateSpeedSettings = b; });

	cSomaLuxScriptable::RegisterTimerNatives(e, "cLuxPlayerState");
	cSomaLuxScriptable::RegisterTimerNatives(e, "cLuxMoveState");
}

void cSomaLuxInputHandler::RegisterNatives(asIScriptEngine *e)
{
	SOMA_FUNC(e, "bool cInput_BecameTriggered(int alId)", +[](int id) { return mpInstance && mpInstance->BecameDown(id); });
	SOMA_FUNC(e, "bool cInput_WasTriggered(int alId)", +[](int id) { return mpInstance && mpInstance->BecameUp(id); });
	SOMA_FUNC(e, "bool cInput_BecameTriggered(const tString&in asName)", +[](const tString &n) {
		cAction *pAction = gpSomaBase->mpEngine->GetInput()->GetAction(n);
		return mpInstance && pAction && mpInstance->BecameDown(pAction->GetId());
	});
	SOMA_FUNC(e, "bool cInput_WasTriggered(const tString&in asName)", +[](const tString &n) {
		cAction *pAction = gpSomaBase->mpEngine->GetInput()->GetAction(n);
		return mpInstance && pAction && mpInstance->BecameUp(pAction->GetId());
	});
	typedef cSomaLuxInputHandler I;
	typedef const tString &S;
	const char *T = "cLuxInputHandler";
	SOMA_METHOD(e, T, "void CreateAction(const tString&in asName, int alId, bool abConfigurable, const tString&in asCat)",
				+[](I *p, S n, int id, bool, S) { p->CreateAction(cLuxAction{n, id}); });
	SOMA_METHOD(e, T, "void CreateDebugAction(const tString&in asName, int alId)", +[](I *p, S n, int id) { p->CreateAction(cLuxAction{n, id}); });
	SOMA_METHOD(e, T, "void CreateAnalogAction(const tString&in asName, int alId, bool abConfigurable, const tString&in asCat, int alAxis, float afMul, int alAnalogId)",
				+[](I *p, S n, int id, bool, S, int axis, float mul, int analog) { p->CreateAction(cLuxAction{n, id, axis, mul, analog}); });
	SOMA_METHOD(e, T, "void CreateAnalogGamepadAction(const tString&in asName, int alId, const tString&in asCat, int alAnalogId, float afSmoothness, int alDirectionLimit)",
				+[](I *p, S n, int id, S, int analog, float, int) { p->CreateAction(cLuxAction{n, id, -1, 0, analog, true}); });
	SOMA_METHOD(e, T, "void CreateActionInput(const tString&in asInputType, int alActionId)", +[](I *p, S s, int id) { p->CreateActionInput(s, id); });
	SOMA_METHOD(e, T, "void CreateAnalogGamepadActionInput(const tString&in asInputType, int alActionId)", +[](I *, S, int) {});
	SOMA_METHOD(e, T, "bool IsGamepadConnected()", +[](I *) { return false; });
	SOMA_METHOD(e, T, "void LoadKeyConfig()", +[](I *p) { p->LoadKeyConfig(); });
	SOMA_METHOD(e, T, "float GetTimeSinceGamepadWasUsed(int alID)", +[](I *, int) { return 100000.0f; });
	SOMA_METHOD(e, T, "int GetLastUsedGamepadIndex(float afTimeLimit=-1.0f)", +[](I *, float) { return -1; });
	SOMA_METHOD(e, T, "bool GetGamepadWasLastDeviceUsed()", +[](I *) { return false; });
	SOMA_METHOD(e, T, "bool IsYAxisInverted()", +[](I *p) { return p->mbInvertMouse; });
	SOMA_METHOD(e, T, "bool WasAnalogueInputFromPad()", +[](I *) { return false; });
	SOMA_METHOD(e, T, "bool GetSmoothMouse()", +[](I *p) { return p->mbSmoothMouse; });
	SOMA_METHOD(e, T, "void SetSmoothMouse(bool abX)", +[](I *p, bool b) { p->mbSmoothMouse = b; });
	SOMA_METHOD(e, T, "float GetMouseSensitivity()", +[](I *p) { return p->mfMouseSensitivity; });
	SOMA_METHOD(e, T, "void SetMouseSensitivity(float afX)", +[](I *p, float x) { p->mfMouseSensitivity = x; });
	SOMA_METHOD(e, T, "float GetGamepadSensitivity()", +[](I *p) { return p->mfGamepadSensitivity; });
	SOMA_METHOD(e, T, "void SetGamepadSensitivity(float afX)", +[](I *p, float x) { p->mfGamepadSensitivity = x; });
	SOMA_METHOD(e, T, "void SetRumble(int alDevice, float afStrength, float afDuration)", +[](I *, int, float, float) {});
	SOMA_METHOD(e, T, "void ResetSmoothMousePos()", +[](I *p) { p->mlstSmoothMousePos.clear(); });
	SOMA_METHOD(e, T, "void SetMaxSmoothMousePos(int alX)", +[](I *p, int x) { p->mlMaxSmoothMousePos = x; });
	SOMA_METHOD(e, T, "void SetPrevSmoothMousePosMul(float afX)", +[](I *p, float x) { p->mfPrevSmoothMousePosMul = x; });
	SOMA_METHOD(e, T, "tString GetActionName(int alId, bool abAnalog)", +[](I *p, int id, bool a) { return p->GetActionName(id, a); });
	SOMA_METHOD(e, T, "void CreateGamepadProfile(const tString&in asName, const tString&in asPrefix, array<tString> &in avButtons, array<tString> &in avAxes, array<uint> &in avDPad)",
				+[](I *p, S n, S pre, CScriptArray &b, CScriptArray &a, CScriptArray &d) {
					cGamepadProfile *pProfile = p->GetGamepadProfile(n);
					if (pProfile == NULL)
					{
						p->mvGamepadProfiles.emplace_back();
						pProfile = &p->mvGamepadProfiles.back();
					}
					pProfile->msName = n;
					pProfile->msPrefix = pre;
					pProfile->mvButtons.clear();
					pProfile->mvAxes.clear();
					pProfile->mvDPad.clear();
					for (asUINT i = 0; i < b.GetSize(); ++i)
						pProfile->mvButtons.push_back(*(tString *)b.At(i));
					for (asUINT i = 0; i < a.GetSize(); ++i)
						pProfile->mvAxes.push_back(*(tString *)a.At(i));
					for (asUINT i = 0; i < d.GetSize(); ++i)
						pProfile->mvDPad.push_back(*(unsigned *)d.At(i));
				});
	SOMA_METHOD(e, T, "void AddPresetToProfile(const tString&in asProfile, const tString&in asPreset, array<int> &in avActions, array<tString> &in avBindings, array<bool> &in avAnalog)",
				+[](I *p, S n, S preset, CScriptArray &ac, CScriptArray &b, CScriptArray &an) {
					cGamepadProfile *pProfile = p->GetGamepadProfile(n);
					if (pProfile == NULL)
						return;
					pProfile->mvPresets.emplace_back();
					cGamepadPreset &P = pProfile->mvPresets.back();
					P.msName = preset;
					for (asUINT i = 0; i < ac.GetSize(); ++i)
						P.mvActions.push_back(*(int *)ac.At(i));
					for (asUINT i = 0; i < b.GetSize(); ++i)
						P.mvBindings.push_back(*(tString *)b.At(i));
					for (asUINT i = 0; i < an.GetSize(); ++i)
						P.mvAnalog.push_back(*(bool *)an.At(i));
				});
	SOMA_METHOD(e, T, "void SetGamepadMapping(const tString&in asProfile, const tString&in asPreset)", +[](I *p, S a, S b) {
		p->msGamepadProfile = a;
		p->msGamepadPreset = b;
	});
	SOMA_METHOD(e, T, "int GetGamepadMappingActionNum()", +[](I *p) {
		cGamepadPreset *pPreset = p->GetGamepadPreset();
		return pPreset ? (int)pPreset->mvActions.size() : 0;
	});
	SOMA_METHOD(e, T, "bool GetGamepadMappingAction(int alId, int&out alAction, tString&out asPrimary, bool&out abAnalog)",
				+[](I *p, int id, int &action, tString &prim, bool &analog) {
					cGamepadPreset *pPreset = p->GetGamepadPreset();
					if (pPreset == NULL)
					{
						action = -1;
						return false;
					}
					if (id < 0 || id >= (int)pPreset->mvActions.size() || id >= (int)pPreset->mvBindings.size() || id >= (int)pPreset->mvAnalog.size())
						return false;
					action = pPreset->mvActions[id];
					prim = pPreset->mvBindings[id];
					analog = pPreset->mvAnalog[id];
					return true;
				});
	SOMA_METHOD(e, T, "void GetActionsAssociatedToGamepadControl(const tString &in asProfile, const tString &in asPreset, const tString &in asControl, tString &out asActions)",
				+[](I *p, S a, S b, S c, tString &out) { p->GetActionsAssociatedToGamepadControl(a, b, c, out); });
	SOMA_METHOD(e, T, "void FetchGamepadInputLayoutString(const tString &in asInputName, tString &out asPrefixName, tString &out asLayoutString)",
				+[](I *p, S n, tString &a, tString &b) { p->FetchGamepadInputLayoutString(n, a, b); });
}
