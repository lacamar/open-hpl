#include "SomaLuxPlayer.h"
#include "SomaS5Dump.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaAgent.h"
#include "SomaLuxGame.h"
#include "SomaLuxVoice.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"
#include "impl/scriptarray.h"
#include <algorithm>
#include <array>

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
	OhplGs3MoveSpeeds speeds = {mfMaxForwardSpeed, mfMaxBackwardSpeed, mfMaxSidwaySpeed, mfForwardAcc, mfSidewayAcc, mfForwardDeacc,
								mfSidewayDeacc, mfForwardOppositeDirAccMul, mfSidewayOppositeDirAccMul, mfMaxForwardSpeedMul,
								mfMaxBackwardSpeedMul, mfMaxSidwaySpeedMul};
	float v[10];
	ohpl_gs3_move_speed_settings(&speeds, v);
	pBody->SetMaxPositiveMoveSpeed(eCharDir_Forward, v[0]);
	pBody->SetMaxNegativeMoveSpeed(eCharDir_Forward, v[1]);
	pBody->SetMaxPositiveMoveSpeed(eCharDir_Right, v[2]);
	pBody->SetMaxNegativeMoveSpeed(eCharDir_Right, v[3]);
	pBody->SetMoveAcc(eCharDir_Forward, v[4]);
	pBody->SetMoveAcc(eCharDir_Right, v[5]);
	pBody->SetMoveDeacc(eCharDir_Forward, v[6]);
	pBody->SetMoveDeacc(eCharDir_Right, v[7]);
	pBody->SetMoveOppositeDirAccMul(eCharDir_Forward, v[8]);
	pBody->SetMoveOppositeDirAccMul(eCharDir_Right, v[9]);
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
		mCharBodyCallback.mpPlayer = this;
		mpCharBody->SetCallback(&mCharBodyCallback);
	}
}

void cSomaLuxPlayer::cCharBodyCallback::OnHitGround(iCharacterBody *apBody, const cVector3f &avVel)
{
	const char *sDecl = "void CharBody_HitGround(iCharacterBody@ apCharBody, const cVector3f &in avVel)";
	auto args = [&](asIScriptContext *c) { c->SetArgAddress(0, apBody); c->SetArgAddress(1, (void *)&avVel); };
	mpPlayer->Call(sDecl, args);
	if (mpPlayer->mpState)
		mpPlayer->mpState->Call(sDecl, args);
	if (mpPlayer->mpMoveState)
		mpPlayer->mpMoveState->Call(sDecl, args);
}

void cSomaLuxPlayer::AddState(const tString &asName, int alId, const tString &asFile, const tString &asClass)
{
	// SOMA registers its states from Player.hps and ships PlayerStates.cfg as well
	if (mmapStates.count(alId))
		return;
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
	if (mmapMoveStates.count(alId))
		return;
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
	int lPlan = ohpl_gs3_state_change(it != mmapStates.end(), it != mmapStates.end() && it->second == mpState);
	if (lPlan == 0)
	{
		Error("Could not change to state id %d. It does not exist!\n", alId);
		return;
	}
	if (lPlan == 1)
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
	int lPlan = ohpl_gs3_state_change(it != mmapMoveStates.end(), it != mmapMoveStates.end() && it->second == mpMoveState);
	if (lPlan == 0)
	{
		Error("Could not change to move state id %d. It does not exist!\n", alId);
		return;
	}
	if (lPlan == 1)
		return;
	cSomaLuxMoveState *pPrev = mpMoveState;
	if (pPrev)
		pPrev->Call("void OnLeaveState(int alNextStateId)", IntArg(alId));
	mpMoveState = it->second;
	mpMoveState->Call("void OnEnterState(int alPrevStateId)", IntArg(pPrev ? pPrev->mlId : -1));
}

void cSomaLuxPlayer::PlaceAtStart(const cVector3f &avFeetPos, float afYaw, bool abCrouching)
{
	if (ohpl_gs3_place_plan(mpCharBody != NULL, false) == 0)
		return;
	cVector3f vPos = avFeetPos;
	bool bScript = Call("void SetupStartPos(const cVector3f&in avPos, float afAngle, bool abCrouching)", [&](asIScriptContext *c) {
		c->SetArgObject(0, &vPos);
		c->SetArgFloat(1, afYaw);
		c->SetArgByte(2, abCrouching);
	});
	if (ohpl_gs3_place_plan(true, bScript) == 2)
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

// cLuxPlayer::Update/PostUpdate/VariableUpdate skip without an active map (game over)
static bool MapActive() { return cSomaLuxMap::GetCurrent() && cSomaLuxMap::GetCurrent()->mbActive; }

void cSomaLuxPlayer::OnUpdate(float afTimeStep)
{
	if (MapActive() == false)
		return;
	cSomaLuxScriptable::OnUpdate(afTimeStep);
	if (mpState)
		mpState->OnUpdate(afTimeStep);
	if (mpMoveState)
		mpMoveState->OnUpdate(afTimeStep);

	mfTimeSincePhysicsInteraction += afTimeStep;
	if (mpCharBody)
	{
		cVector3f vVel = mpCharBody->GetVelocity(afTimeStep);
		ohpl_gs3_update_average_move(&mfAverageMoveSpeed, &mvAverageMoveDirection, &vVel);
	}
	UpdateCamera(afTimeStep);
	if (OhplS5DumpEnabled() && mpCharBody)
	{
		static unsigned slTick = 0;
		cVector3f f = mpCharBody->GetFeetPosition(), v = mpCharBody->GetForceVelocity();
		OhplS5Log("pl %u %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %d %d %08x", slTick++, OhplS5Bits(afTimeStep), OhplS5Bits(f.x),
				  OhplS5Bits(f.y), OhplS5Bits(f.z), OhplS5Bits(v.x), OhplS5Bits(v.y), OhplS5Bits(v.z), OhplS5Bits(mpCharBody->GetYaw()),
				  OhplS5Bits(mpCamera ? mpCamera->GetPitch() : 0), OhplS5Bits(mpCamera ? mpCamera->GetFOV() : 0), OhplS5Bits(mfAverageMoveSpeed),
				  mpState ? mpState->mlId : -1, mpMoveState ? mpMoveState->mlId : -1, OhplS5Bits(mfHealth));
	}
}

void cSomaLuxPlayer::OnPostUpdate(float afTimeStep)
{
	if (MapActive() == false)
		return;
	cSomaLuxScriptable::OnPostUpdate(afTimeStep);
	if (mpState)
		mpState->OnPostUpdate(afTimeStep);
	if (mpMoveState)
		mpMoveState->OnPostUpdate(afTimeStep);
}

void cSomaLuxPlayer::OnVariableUpdate(float afTimeStep)
{
	if (MapActive())
		cSomaLuxScriptable::OnVariableUpdate(afTimeStep);
}

void cSomaLuxPlayer::OnGui(float afTimeStep)
{
	cSomaLuxScriptable::OnGui(afTimeStep);
	if (mpState)
		mpState->OnGui(afTimeStep);
	if (mpMoveState)
		mpMoveState->OnGui(afTimeStep);
}

void cSomaLuxPlayer::OnMessage(const char *apDecl)
{
	// the map is gone, so states that cleared their data on map leave must not run OnLeaveState
	if (ohpl_gs3_drops_state_on_reset(ohpl_gs3_message_kind((const unsigned char *)apDecl, strlen(apDecl)), mpState != NULL, mpState ? mpState->mlId : 0))
		mpState = NULL;
	cSomaLuxScriptable::OnMessage(apDecl);
	for (auto &it : mmapStates)
		it.second->OnMessage(apDecl);
	for (auto &it : mmapMoveStates)
		it.second->OnMessage(apDecl);
}

extern bool gbMapChangeIsTransfer;

void cSomaLuxPlayer::OnMapMessage(const char *apDecl, void *apMap)
{
	int lKind = ohpl_gs3_message_kind((const unsigned char *)apDecl, strlen(apDecl));
	bool bDestroy = lKind == 3;
	// Real engine (ResetMapSpecifics): states back to id 0, no OnLeaveState
	if (lKind == 2)
	{
		mpState = mmapStates.count(0) ? mmapStates[0] : NULL;
		if (mpState)
			mpState->Call("void OnEnterState(int alPrevStateId)", IntArg(-1));
		mpMoveState = mmapMoveStates.count(0) ? mmapMoveStates[0] : NULL;
		if (mpMoveState)
			mpMoveState->Call("void OnEnterState(int alPrevStateId)", IntArg(-1));
		if (ohpl_gs3_clears_camera_adds(gbMapChangeIsTransfer))
			mmapCameraPosAdd.clear();
		if (mpCamera)
		{
			SetCamera(mpCamera);
			mpCamera->SetRotateMode(eCameraRotateMode_EulerAngles);
			mpCamera->SetRoll(0);
			mpCamera->SetPitch(0);
			mpCamera->SetYaw(0);
		}
	}
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

static float FadeTowards(cSomaLuxPlayer::cFadeValue &aX, float afTimeStep) { return ohpl_gs3_fade_towards(&aX, afTimeStep); }

void cSomaLuxPlayer::UpdateCamera(float afTimeStep)
{
	if (mpCamera == NULL)
		return;

	cVector3f vAdd = mvBaseCameraPosAdd;
	for (auto &it : mmapCameraPosAdd)
	{
		cMoveVector &m = it.second;
		ohpl_gs3_step_move_vec(&m, afTimeStep);
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
	for (auto &it : mmapFOVMul)
		FadeTowards(it.second, afTimeStep);
	mpCamera->SetFOV(cMath::ToRad(mFOV.mfValue) * GetFOVMul());
	FadeTowards(mAspectMul, afTimeStep);

	if (mpCharBody == NULL)
		return;

	if (mbCameraRotateActive)
	{
		OhplGs3RotateIn in = {mfCameraRotateSpeed, mfCameraRotateAcc, mfCameraRotateSpeedMul, mfCameraRotateMaxSpeed, mbCameraRotateLocal,
							  mvCameraRotateTarget};
		OhplGs3RotateOut out;
		cVector3f vCamPos = mpCamera->GetPosition();
		ohpl_gs3_step_camera_rotate(&in, &vCamPos, mpCharBody->GetYaw(), mpCamera->GetPitch(), afTimeStep, &out);
		mfCameraRotateSpeed = out.mfSpeed;
		if (out.mbApply)
		{
			mpCharBody->AddYaw(out.mfAddYaw);
			mpCamera->AddPitch(out.mfAddPitch);
		}
	}

	if (mbAutomoveActive)
	{
		cVector3f vFeet = mpCharBody->GetFeetPosition(), vFwd = mpCharBody->GetForward(), vRight = mpCharBody->GetRight();
		float fForward, fRight;
		if (ohpl_gs3_step_automove(&mvAutomoveTarget, &vFeet, &vFwd, &vRight, mfAutomoveSpeedMul, &fForward, &fRight))
		{
			mpCharBody->Move(eCharDir_Forward, fForward);
			mpCharBody->Move(eCharDir_Right, fRight);
		}
		else
			mbAutomoveActive = false;
	}

	if (mpCharBody->GetCamera() == mpCamera)
	{
		mpCamera->SetYaw(mpCharBody->GetYaw());
		// Body yaw past the camera limit made the limit edge sticky
		if (mpCamera->GetYaw() != mpCharBody->GetYaw())
			mpCharBody->SetYaw(mpCamera->GetYaw());
	}
}

namespace
{
int32_t InputHostCall(void *, uint32_t alOp, OhplS5Io *io)
{
	cInput *pInput = gpSomaBase->mpEngine->GetInput();
	cSomaLuxInputHandler *pSelf = cSomaLuxInputHandler::Get();
	static tString sOut;
	tString sIn(io->s ? (const char *)io->s : "", io->s ? io->n : 0);
	switch (alOp)
	{
	case kOhplS5In_EXISTS_ID: return pInput->GetAction(io->i[0]) != NULL;
	case kOhplS5In_EXISTS_NAME: return pInput->GetAction(sIn) != NULL;
	case kOhplS5In_DESTROY_ID:
		if (cAction *pAction = pInput->GetAction(io->i[0]))
			pInput->DestroyAction(pAction);
		return 0;
	case kOhplS5In_DESTROY_NAME:
		if (cAction *pAction = pInput->GetAction(sIn))
			pInput->DestroyAction(pAction);
		return 0;
	case kOhplS5In_CREATE: pInput->CreateAction(sIn, io->i[0]); return 0;
	case kOhplS5In_CREATE_MOUSE: pInput->CreateAction(sIn, io->i[0])->AddMouseButton((eMouseButton)io->i[1]); return 0;
	case kOhplS5In_ID_OF:
	{
		cAction *pAction = pInput->GetAction(sIn);
		if (pAction)
			io->i[0] = pAction->GetId();
		return pAction != NULL;
	}
	case kOhplS5In_TRIGGERED: return pInput->IsTriggerd(io->i[0]);
	case kOhplS5In_ADD_KEY:
	{
		eKey key = pInput->GetKeyboard()->StringToKey(sIn);
		if (key != eKey_LastEnum)
			pInput->GetAction(io->i[0])->AddKey(key);
		return 0;
	}
	case kOhplS5In_ADD_BUTTON:
	{
		eMouseButton button = pInput->GetMouse()->StringToButton(sIn);
		if (button != eMouseButton_LastEnum)
			pInput->GetAction(io->i[0])->AddMouseButton(button);
		return 0;
	}
	case kOhplS5In_KEY_DOWN: return pInput->GetKeyboard()->KeyIsDown((eKey)io->i[0]);
	case kOhplS5In_KEY_NAME:
		sOut = pInput->GetKeyboard()->KeyToString((eKey)io->i[0]);
		io->s = (const unsigned char *)sOut.data();
		io->n = sOut.size();
		return 0;
	case kOhplS5In_BUTTON_DOWN: return pInput->GetMouse()->ButtonIsDown((eMouseButton)io->i[0]);
	case kOhplS5In_BUTTON_NAME:
		sOut = pInput->GetMouse()->ButtonToString((eMouseButton)io->i[0]);
		io->s = (const unsigned char *)sOut.data();
		io->n = sOut.size();
		return 0;
	case kOhplS5In_WARN: Warning("%s", sIn.c_str()); return 0;
	case kOhplS5In_LOG_ACTIONS: Log("SOMA script: %d input actions\n", io->i[0]); return 0;
	case kOhplS5In_CALL: pSelf->Call(sIn); return 0;
	case kOhplS5In_CALL_KEY_CONFIG: pSelf->CallWithObject("void LoadKeyConfig(cConfigFile@ apKeyConfig)", SomaKeyConfig()); return 0;
	case kOhplS5In_BROADCAST_ACTION: cSomaLuxGame::Get()->BroadcastAction(io->i[0], io->i[1] != 0); return 0;
	case kOhplS5In_BROADCAST_ANALOG: cSomaLuxGame::Get()->BroadcastAnalog(io->i[0], cVector3f(io->f[0], io->f[1], io->f[2])); return 0;
	case kOhplS5In_MOUSE_REL:
	{
		cVector2l vRel = cSomaLuxGame::Get()->MouseRel();
		io->i[0] = vRel.x;
		io->i[1] = vRel.y;
		return 0;
	}
	case kOhplS5In_SCREEN_HEIGHT: io->f[0] = gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat().y; return 0;
	case kOhplS5In_USER_CONFIG:
	{
		cConfigFile *pCfg = SomaUserConfig();
		if (pCfg == NULL)
			return 0;
		io->i[0] = pCfg->GetBool("Input", "InvertMouse", false);
		io->i[1] = pCfg->GetBool("Input", "SmoothMouse", true);
		io->f[0] = pCfg->GetFloat("Input", "MouseSensitivity", 1.0f);
		io->f[1] = pCfg->GetFloat("Input", "GamepadSensitivity", 2.0f);
		return 1;
	}
	case kOhplS5In_ARR_SIZE: return (int32_t)((CScriptArray *)io->p[0])->GetSize();
	case kOhplS5In_ARR_AT: io->p[1] = ((CScriptArray *)io->p[0])->At(io->i[0]); return 0;
	case kOhplS5In_STR_REF:
	{
		static tString sRef;
		sRef = sIn;
		io->p[1] = &sRef;
		return 0;
	}
	case kOhplS5In_ENGINE_ACTION_NAME:
	{
		cAction *pAction = pInput->GetAction(io->i[0]);
		if (pAction == NULL)
			return 0;
		sOut = pAction->GetName();
		io->s = (const unsigned char *)sOut.data();
		io->n = sOut.size();
		return 1;
	}
	case kOhplS5In_ENGINE_ACTION_BINDS:
	{
		cAction *pAction = pInput->GetAction(io->i[0]);
		sOut.clear();
		for (int k = 0; pAction && k < pAction->GetSubActionNum(); ++k)
			sOut += pAction->GetSubAction(k)->GetInputType() + "." + pAction->GetSubAction(k)->GetInputName() + ",";
		io->s = (const unsigned char *)sOut.data();
		io->n = sOut.size();
		return 0;
	}
	case kOhplS5In_DUMP: OhplS5Log("%s", sIn.c_str()); return 0;
	}
	return 0;
}
const OhplS5Host gInputHost = {NULL, InputHostCall};
}

cSomaLuxInputHandler::cSomaLuxInputHandler()
{
	mpInstance = this;
	ohpl_s5_in_install_host(&gInputHost);
	ohpl_s5_in_new(SomaGameConfig()->GetInt("Input", "MaxSmoothMousePos", 7), SomaGameConfig()->GetFloat("Input", "PrevSmoothMousePosMul", 0.7f), eKey_LastEnum,
				   eMouseButton_LastEnum);
}

static std::vector<cSomaLuxEntity *> UsedLiquidAreas()
{
	std::vector<cSomaLuxEntity *> v;
	if (cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent())
		for (cSomaLuxEntity *pEnt : pMap->GetEntities())
			if (pEnt->meType == eSomaLuxEntityType_LiquidArea && pEnt->mbPlayerInLiquid)
				v.push_back(pEnt);
	return v;
}

// cLuxPlayer::SetHealth 0xca7940 / IsDead 0xcbb000: health <= 0 is only death with CanDie
void cSomaLuxPlayer::SetHealth(float afX)
{
	if (ohpl_gs3_set_health(mfHealth, mfMaxHealth, mbCanDie, afX, &mfHealth) == false)
		return;
	tString sSource;
	auto args = [&](asIScriptContext *c) {
		c->SetArgDWord(0, (asDWORD)-1);
		c->SetArgObject(1, &sSource);
	};
	Call("void OnPlayerDead(int, const tString&in)", args);
	SomaMapScriptCall("void OnPlayerDead(int, const tString&in)", args);
	if (cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent())
		for (cSomaLuxEntity *pEnt : std::vector<cSomaLuxEntity *>(pMap->GetEntities()))
			SomaAgentSendMessage(pEnt, 10);
}

// cLuxPlayer::GiveDamage 0xca6090: an attack instance hurts once
bool cSomaLuxPlayer::GiveDamage(float afAmount, int alType, float afMinHealth, const cSomaID &aSource, int alDamageID)
{
	if (IsDead())
		return false;
	if (alDamageID != -1)
	{
		int lPlan = ohpl_gs3_damage_id(mvDamageIDs.data(), mvDamageIDs.size(), alDamageID);
		if (lPlan == 1)
			return false;
		if (lPlan == 3)
			mvDamageIDs.erase(mvDamageIDs.begin());
		if (lPlan >= 2)
			mvDamageIDs.push_back(alDamageID);
	}
	cSomaID src = aSource;
	Call("void OnDamageTaken(float, int, tID)", [&](asIScriptContext *c) {
		c->SetArgFloat(0, afAmount);
		c->SetArgDWord(1, alType);
		c->SetArgObject(2, &src);
	});
	SetHealth(ohpl_gs3_health_after_damage(mfHealth, afAmount, afMinHealth));
	return true;
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
	SOMA_METHOD(e, T, "bool IsInLiquid()", +[](P *p) { return SomaPlayerLiquidSurface() > -100000.0f; });
	SOMA_METHOD(e, T, "float GetLiquidHeight()", +[](P *p) {
		return p->GetCharacterBody() ? SomaPlayerLiquidSurface() - p->GetCharacterBody()->GetFeetPosition().y : 0.0f;
	});
	SOMA_METHOD(e, T, "void AddUsedLiquidArea(cLuxLiquidArea@ apArea)", +[](P *p, cSomaLuxEntity *a) { if (a) a->mbPlayerInLiquid = true; });
	SOMA_METHOD(e, T, "void RemoveUsedLiquidArea(cLuxLiquidArea@ apArea)", +[](P *p, cSomaLuxEntity *a) { if (a) a->mbPlayerInLiquid = false; });
	SOMA_METHOD(e, T, "bool GetIsLiquidAreaUsed(cLuxLiquidArea@ apArea)", +[](P *p, cSomaLuxEntity *a) { return a && a->mbPlayerInLiquid; });
	SOMA_METHOD(e, T, "int GetUsedLiquidAreaNum()", +[](P *p) { return (int)UsedLiquidAreas().size(); });
	SOMA_METHOD(e, T, "tID GetUsedLiquidAreaID(int alId)", +[](P *p, int i) {
		std::vector<cSomaLuxEntity *> v = UsedLiquidAreas();
		return i >= 0 && i < (int)v.size() ? v[i]->mID : cSomaID();
	});
	SOMA_METHOD(e, T, "float GetSmoothingRotAmount()", +[](P *p) { return p->mfSmoothingRotAmount; });
	SOMA_METHOD(e, T, "void SetSmoothingRotAmount(float afX)", +[](P *p, float f) { p->mfSmoothingRotAmount = f; });
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
				+[](P *p, float a, int, int, float fMin, S) { p->SetHealth(ohpl_gs3_health_after_script_damage(p->mfHealth, a, fMin)); });
	SOMA_METHOD(e, T, "bool GiveDamage(float afAmount, int alStrength, int aType, float afMinHealth, tID a_idSource, int alDamageID)",
				+[](P *p, float a, int, int t, float fMin, cSomaID src, int id) { return p->GiveDamage(a, t, fMin, src, id); });
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
		vSum = 0;
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
	SOMA_METHOD(e, T, "void FadeCameraFOVMulTo(int alId, float afX, float afSpeed)", +[](P *p, int id, float x, float s) {
		cSomaLuxPlayer::cFadeValue &f = p->FOVMul(id);
		f.mfGoal = x;
		f.mfSpeed = s;
	});
	SOMA_METHOD(e, T, "void SetCameraFOVMul(int alId, float afFOVMul)", +[](P *p, int id, float x) {
		cSomaLuxPlayer::cFadeValue &f = p->FOVMul(id);
		f.mfValue = f.mfGoal = x;
	});
	SOMA_METHOD(e, T, "float GetCameraFOVMul(int alId)", +[](P *p, int id) { return p->FOVMul(id).mfValue; });
	SOMA_METHOD(e, T, "float GetCameraFOVMul()", +[](P *p) { return p->GetFOVMul(); });
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
	SOMA_METHOD(e, T, "void SetHealth(float afX)", +[](P *p, float x) { p->SetHealth(x); });
	SOMA_METHOD(e, T, "float GetHealth()", +[](P *p) { return p->mfHealth; });
	SOMA_METHOD_NEW(e, T, "bool SetCanDie(bool abX)", +[](P *p, bool b) { p->mbCanDie = b; return b; });
	SOMA_METHOD_NEW(e, T, "bool GetCanDie()", +[](P *p) { return p->mbCanDie; });
	SOMA_METHOD(e, T, "void SetMaxHealth(float afX)", +[](P *p, float x) { p->mfMaxHealth = x; });
	SOMA_METHOD(e, T, "float GetMaxHealth()", +[](P *p) { return p->mfMaxHealth; });
	SOMA_METHOD(e, T, "void AddHealth(float afX, float afMinHealth)", +[](P *p, float x, float fMin) {
		p->SetHealth(ohpl_gs3_add_health(p->mfHealth, x, fMin));
	});
	SOMA_METHOD(e, T, "bool IsDead()", +[](P *p) { return p->IsDead(); });
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
	SOMA_METHOD(e, T, "float GetVisibilityRangeMul()", +[](P *p) { return p->GetVisibilityRangeMul(); });
	SOMA_METHOD(e, T, "void SetVisibilityRangeMul(int alId, float afX)", +[](P *p, int id, float x) { p->mmapVisibilityRangeMul[id] = x; });
	SOMA_METHOD(e, T, "void SetVisibilityMaxRange(int alId, float afX)", +[](P *p, int id, float x) { p->mmapVisibilityMaxRange[id] = x; });
	SOMA_METHOD(e, T, "float GetVisibilityMaxRange()", +[](P *p) { return p->GetVisibilityMaxRange(); });
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
