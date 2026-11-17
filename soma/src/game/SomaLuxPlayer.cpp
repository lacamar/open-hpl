#include "SomaLuxPlayer.h"
#include "RustSomaLuxS9.h"
#include "SomaS5Dump.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaAgent.h"
#include "SomaLuxGame.h"
#include "SomaLuxVoice.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"
#include "impl/RustEasArray.h"

#include <cmath>
#include <cstring>

cSomaLuxPlayer *cSomaLuxPlayer::mpInstance = NULL;
cSomaLuxInputHandler *cSomaLuxInputHandler::mpInstance = NULL;


namespace
{
std::vector<tString> Parts(const OhplS5Io *io)
{
	std::vector<tString> v(1);
	for (size_t i = 0; io->s && i < io->n; ++i)
		if (io->s[i] == 0)
			v.emplace_back();
		else
			v.back() += (char)io->s[i];
	return v;
}

tString Str(const OhplS5Io *io) { return io->s ? tString((const char *)io->s, io->n) : tString(); }

cSomaLuxEntity *PlayerEntity() { return cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(tString("Player")) : NULL; }

std::vector<cSomaLuxEntity *> UsedLiquidAreas()
{
	std::vector<cSomaLuxEntity *> v;
	if (cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent())
		for (cSomaLuxEntity *pEnt : pMap->GetEntities())
			if (pEnt->meType == eSomaLuxEntityType_LiquidArea && pEnt->mbPlayerInLiquid)
				v.push_back(pEnt);
	return v;
}

void Vec(OhplS5Io *io, const cVector3f &avX)
{
	io->f[0] = avX.x;
	io->f[1] = avX.y;
	io->f[2] = avX.z;
}
}

int32_t cSomaLuxPlayer::HostCall(void *, uint32_t alOp, OhplS5Io *io)
{
	cSomaLuxScriptable *pSelf = (cSomaLuxScriptable *)io->p[0];
	cCamera *pCam = (cCamera *)io->p[0];
	iCharacterBody *pBody = (iCharacterBody *)io->p[0];
	switch (alOp)
	{
	case kOhplS5d_DUMP_ENABLED: return OhplS5DumpEnabled();
	case kOhplS5d_DUMP_LOG: OhplS5Log("%s", Str(io).c_str()); return 0;
	case kOhplS5d_ERROR_LOG: Error("%s", Str(io).c_str()); return 0;
	case kOhplS5d_CFG_FLOAT:
	{
		std::vector<tString> v = Parts(io);
		io->f[0] = SomaGameConfig()->GetFloat(v[0], v[1], io->f[0]);
		return 0;
	}
	case kOhplS5d_NEW_STATE:
	{
		cSomaLuxScriptable *pNew;
		if (io->i[0])
		{
			cSomaLuxMoveState *pState = new cSomaLuxMoveState();
			pState->msName = Str(io);
			pState->mB.mlId = io->i[1];
			pState->mB.mpPlayer = mpInstance;
			io->p[1] = &pState->mB;
			pNew = pState;
		}
		else
		{
			cSomaLuxPlayerState *pState = new cSomaLuxPlayerState();
			pState->msName = Str(io);
			pState->mB.mlId = io->i[1];
			pState->mB.mpPlayer = mpInstance;
			io->p[1] = &pState->mB;
			pNew = pState;
		}
		pNew->msScriptName = Str(io);
		io->p[0] = pNew;
		return 0;
	}
	case kOhplS5d_LOAD_STATE:
	{
		std::vector<tString> v = Parts(io);
		if (pSelf->LoadScript(mpInstance->mpRuntime, v[0], v[1], v[2]))
			return 1;
		Error("SOMA script: could not load %s %s\n", io->i[0] ? "move state" : "player state", pSelf->msScriptName.c_str());
		delete pSelf;
		return 0;
	}
	case kOhplS5d_DELETE: delete pSelf; return 0;
	case kOhplS5d_CALL:
	{
		tString sDecl = Str(io);
		const OhplS5dArg *pArgs = (const OhplS5dArg *)io->p[1];
		int lCount = io->i[0];
		std::vector<tString> vStrings;
		for (int i = 0; i < lCount; ++i)
			if (pArgs[i].kind == kOhplS5dArg_STRING)
				vStrings.emplace_back((const char *)pArgs[i].p, pArgs[i].i);
		auto args = [&](asIScriptContext *c) {
			size_t lString = 0;
			for (int i = 0; i < lCount; ++i)
			{
				const OhplS5dArg &a = pArgs[i];
				switch (a.kind)
				{
				case kOhplS5dArg_DWORD: c->SetArgDWord(i, a.i); break;
				case kOhplS5dArg_FLOAT: c->SetArgFloat(i, a.f); break;
				case kOhplS5dArg_BYTE: c->SetArgByte(i, a.i); break;
				case kOhplS5dArg_ADDRESS: c->SetArgAddress(i, a.p); break;
				case kOhplS5dArg_OBJECT: c->SetArgObject(i, a.p); break;
				case kOhplS5dArg_STRING: c->SetArgObject(i, &vStrings[lString++]); break;
				}
			}
		};
		if (pSelf == NULL)
		{
			SomaMapScriptCall(sDecl, args);
			return 0;
		}
		return io->i[1] ? pSelf->CallBool(sDecl, args, io->i[2] != 0) : pSelf->Call(sDecl, args);
	}
	case kOhplS5d_BASE:
	{
		tString sDecl = Str(io);
		switch (io->i[0])
		{
		case kOhplS5dBase_UPDATE: pSelf->cSomaLuxScriptable::OnUpdate(io->f[0]); break;
		case kOhplS5dBase_POST_UPDATE: pSelf->cSomaLuxScriptable::OnPostUpdate(io->f[0]); break;
		case kOhplS5dBase_VARIABLE_UPDATE: pSelf->cSomaLuxScriptable::OnVariableUpdate(io->f[0]); break;
		case kOhplS5dBase_GUI: pSelf->cSomaLuxScriptable::OnGui(io->f[0]); break;
		case kOhplS5dBase_MESSAGE: pSelf->cSomaLuxScriptable::OnMessage(sDecl.c_str()); break;
		case kOhplS5dBase_MAP_MESSAGE: pSelf->cSomaLuxScriptable::OnMapMessage(sDecl.c_str(), io->p[1]); break;
		case kOhplS5dBase_ACTION: pSelf->cSomaLuxScriptable::OnAction(io->i[1], io->i[2] != 0); break;
		case kOhplS5dBase_ANALOG: pSelf->cSomaLuxScriptable::OnAnalogInput(io->i[1], cVector3f(io->f[0], io->f[1], io->f[2])); break;
		}
		return 0;
	}
	// cLuxPlayer::Update/PostUpdate/VariableUpdate skip without an active map (game over)
	case kOhplS5d_MAP_ACTIVE: return cSomaLuxMap::GetCurrent() && cSomaLuxMap::GetCurrent()->mbActive;
	case kOhplS5d_TRANSFER: return ohpl_s9_is_transfer();
	case kOhplS5d_CAM_GET:
		switch (io->i[0])
		{
		case 0: Vec(io, pCam->GetPosition()); break;
		case 1: io->f[0] = pCam->GetPitch(); break;
		case 2: io->f[0] = pCam->GetYaw(); break;
		case 3: io->f[0] = pCam->GetFOV(); break;
		}
		return 0;
	case kOhplS5d_CAM_SET:
		switch (io->i[0])
		{
		case 0: pCam->SetRoll(io->f[0]); break;
		case 1: pCam->SetFOV(io->f[0]); break;
		case 2: pCam->SetYaw(io->f[0]); break;
		case 3: pCam->SetPitch(io->f[0]); break;
		case 4: pCam->AddPitch(io->f[0]); break;
		case 5: pCam->SetNearClipPlane(io->f[0]); break;
		case 6: pCam->SetFarClipPlane(io->f[0]); break;
		case 7: pCam->SetPitchLimits(io->f[0], io->f[1]); break;
		case 8:
			pCam->SetRotateMode(eCameraRotateMode_EulerAngles);
			pCam->SetRoll(0);
			pCam->SetPitch(0);
			pCam->SetYaw(0);
			break;
		}
		return 0;
	case kOhplS5d_BODY_GET:
		switch (io->i[0])
		{
		case 0: Vec(io, pBody->GetVelocity(io->f[0])); break;
		case 1: Vec(io, pBody->GetFeetPosition()); break;
		case 2: Vec(io, pBody->GetForceVelocity()); break;
		case 3: Vec(io, pBody->GetForward()); break;
		case 4: Vec(io, pBody->GetRight()); break;
		case 5: io->f[0] = pBody->GetYaw(); break;
		case 6: io->p[1] = pBody->GetCamera(); break;
		}
		return 0;
	case kOhplS5d_BODY_SET:
		switch (io->i[0])
		{
		case 0: pBody->SetCameraPosAdd(cVector3f(io->f[0], io->f[1], io->f[2])); break;
		case 1: pBody->AddYaw(io->f[0]); break;
		case 2: pBody->Move(eCharDir_Forward, io->f[0]); break;
		case 3: pBody->Move(eCharDir_Right, io->f[0]); break;
		case 4: pBody->SetYaw(io->f[0]); break;
		case 5: pBody->SetFeetPosition(cVector3f(io->f[0], io->f[1], io->f[2])); break;
		case 6: pBody->SetForceVelocity(0); break;
		case 7: pBody->Update(io->f[0]); break;
		case 8:
			pBody->SetCamera((cCamera *)io->p[1]);
			pBody->SetCollideFlags(SomaCollideFlag("+player"));
			pBody->SetCallback(&mpInstance->mCharBodyCallback);
			break;
		case 9:
		{
			const float *v = (const float *)io->p[1];
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
			break;
		}
		}
		return 0;
	case kOhplS5d_LIQUID_SURFACE: io->f[0] = SomaPlayerLiquidSurface(); return 0;
	case kOhplS5d_LIQUID_COUNT: return (int32_t)UsedLiquidAreas().size();
	case kOhplS5d_LIQUID_ID:
	{
		std::vector<cSomaLuxEntity *> v = UsedLiquidAreas();
		int lIndex = io->i[0];
		if (lIndex < 0 || lIndex >= (int)v.size())
			return 0;
		io->i[0] = v[lIndex]->mID.mA;
		io->i[1] = v[lIndex]->mID.mB;
		io->i[2] = v[lIndex]->mID.mC;
		return 1;
	}
	case kOhplS5d_LIQUID_SET: ((cSomaLuxEntity *)io->p[1])->mbPlayerInLiquid = io->i[0] != 0; return 0;
	case kOhplS5d_LIQUID_GET: return ((cSomaLuxEntity *)io->p[1])->mbPlayerInLiquid;
	case kOhplS5d_HAS_COLLIDE:
	{
		cSomaLuxEntity *pEnt = PlayerEntity();
		return pEnt && pEnt->mvCollideCallbacks.empty() == false;
	}
	case kOhplS5d_ADD_COLLIDE:
	{
		cSomaLuxEntity *pEnt = PlayerEntity(), *pOther = (cSomaLuxEntity *)io->p[1];
		if (pEnt && pOther)
			pEnt->mvCollideCallbacks.push_back(cSomaLuxEntity::cCollideCallback{pOther->msName, Str(io)});
		return 0;
	}
	case kOhplS5d_REMOVE_COLLIDE:
		if (cSomaLuxEntity *pEnt = PlayerEntity())
			pEnt->RemoveCollideCallbacks(Str(io));
		return 0;
	case kOhplS5d_CHECK_COLLIDE:
	{
		cSomaLuxEntity *pEnt = PlayerEntity();
		return pEnt && SomaEntitiesCollide(pEnt, (cSomaLuxEntity *)io->p[1]);
	}
	case kOhplS5d_DEAD_BROADCAST:
		if (cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent())
			for (cSomaLuxEntity *pEnt : std::vector<cSomaLuxEntity *>(pMap->GetEntities()))
				SomaAgentSendMessage(pEnt, 10);
		return 0;
	}
	return 0;
}

namespace
{
const OhplS5Host gPlayerHost = {NULL, cSomaLuxPlayer::HostCall};
}

cSomaLuxPlayer::cSomaLuxPlayer()
{
	mpInstance = this;
	ohpl_s5d_install_host(&gPlayerHost);
	ohpl_s5d_new(this);
}

cSomaLuxPlayer::~cSomaLuxPlayer()
{
	ohpl_s5d_free();
	if (mpInstance == this)
		mpInstance = NULL;
}

void cSomaLuxPlayer::RegisterNatives(asIScriptEngine *e)
{
	cSomaLuxPlayerState state;
	cSomaLuxMoveState move;
	auto off = [](void *apBase, void *apField) { return (int)((char *)apField - (char *)apBase); };
	ohpl_s5d_register(e, off((cSomaLuxScriptable *)&state, &state.mB), off((cSomaLuxScriptable *)&move, &move.mB),
					  off((cSomaLuxScriptable *)&state, &state.msName), off((cSomaLuxScriptable *)&move, &move.msName));
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
	case kOhplS5In_ARR_SIZE: return (int32_t)((OhplEasArray *)io->p[0])->GetSize();
	case kOhplS5In_ARR_AT: io->p[1] = ((OhplEasArray *)io->p[0])->At(io->i[0]); return 0;
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
