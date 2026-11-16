#include "LuxInputHost.h"

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxPlayerHelpers.h"
#include "LuxPreMenu.h"
#include "LuxMainMenu.h"
#include "LuxCredits.h"
#include "LuxDemoEnd.h"
#include "LuxInventory.h"
#include "LuxJournal.h"
#include "LuxMessageHandler.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxSaveHandler.h"
#include "LuxEffectHandler.h"
#include "LuxConfigHandler.h"
#include "LuxLoadScreenHandler.h"
#include "LuxDebugHandler.h"
#include "LuxInputHandler.h"
#include "LuxA12bDump.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static iGamepad *gpPad = NULL;

// Scripted mode (OHPL_A12C_DUMP): every answer is a hash of the step; effects are logged and skipped.
static bool gbScript = false;
static uint32_t glStep = 0;
static int glKeys = 0;

static FILE *gpDump = NULL;
static bool gbInit = false;

static FILE *Dump()
{
	if(gbInit == false)
	{
		gbInit = true;
		const char *sPath = getenv("OHPL_A12C_DUMP");
		if(sPath && *sPath) gpDump = fopen(sPath, "w");
	}
	return gpDump;
}

bool OhplInDumpEnabled() { return Dump() != NULL; }
bool OhplInScripted() { return gbScript; }

void OhplInLog(const char *fmt, ...)
{
	FILE *f = Dump();
	if(f == NULL) return;
	va_list ap;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fflush(f);
}

static uint32_t Mix(uint32_t a, uint32_t b, uint32_t c)
{
	uint32_t h = a * 2654435761u ^ (b + 0x9e3779b9u) * 2246822519u ^ (c + 0x85ebca6bu) * 3266489917u;
	h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
	return h;
}

iGamepad *OhplInPad() { return gpPad; }

//-----------------------------------------------------------------------

bool OhplInTrig(int alKind, int alAction)
{
	if(gbScript)
	{
		static const uint32_t vMod[4] = { 5, 5, 3, 8 };
		bool bRes = Mix(glStep, alKind + 1, alAction) % vMod[alKind & 3] == 0;
		OhplInLog("c t %d %d %d", alKind, alAction, bRes ? 1 : 0);
		return bRes;
	}
	cInput *pInput = gpBase->mpEngine->GetInput();
	switch(alKind)
	{
	case 0: return pInput->BecameTriggerd(alAction);
	case 1: return pInput->WasTriggerd(alAction);
	case 2: return pInput->IsTriggerd(alAction);
	}
	return pInput->DoubleTriggerd(alAction, 0.3f);
}

//-----------------------------------------------------------------------

double OhplInVal(int alWhat, int alIdx)
{
	if(gbScript)
	{
		double fRes = 0;
		uint32_t h = Mix(glStep, 100 + alWhat, alIdx);
		switch(alWhat)
		{
		case eInVal_MouseAnyButton: fRes = h % 4 == 0; break;
		case eInVal_MouseAbsX: fRes = Mix(glStep / 3, 7, 1) % 64; break;
		case eInVal_MouseAbsY: fRes = Mix(glStep / 3, 7, 2) % 64; break;
		case eInVal_MouseRelX: case eInVal_MouseRelY: fRes = (int)(h % 21) - 10; break;
		case eInVal_ScreenW: fRes = 1280; break;
		case eInVal_ScreenH: fRes = 720; break;
		case eInVal_GameTime: fRes = glStep * 0.75; break;
		case eInVal_PadPresent: fRes = (glStep / 40) % 2 == 1; break;
		case eInVal_PadButton: fRes = h % 3 == 0; break;
		case eInVal_PadAxis: fRes = (float)((int)(h % 2001) - 1000) / 1000.0f; break;
		case eInVal_PadDead: fRes = 0.15f; break;
		case eInVal_FrameTime: fRes = 1.0f / 60.0f; break;
		case eInVal_StepSize: fRes = 0.016f; break;
		}
		OhplInLog("c v %d %d %.9g", alWhat, alIdx, fRes);
		return fRes;
	}
	cInput *pInput = gpBase->mpEngine->GetInput();
	switch(alWhat)
	{
	case eInVal_MouseAnyButton:
		for(int i = 0; i < eMouseButton_LastEnum; ++i)
			if(pInput->GetMouse()->ButtonIsDown(eMouseButton(i))) return 1;
		return 0;
	case eInVal_MouseAbsX: return pInput->GetMouse()->GetAbsPosition().x;
	case eInVal_MouseAbsY: return pInput->GetMouse()->GetAbsPosition().y;
	case eInVal_MouseRelX: return pInput->GetMouse()->GetRelPosition().x;
	case eInVal_MouseRelY: return pInput->GetMouse()->GetRelPosition().y;
	case eInVal_ScreenW: return gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeInt().x;
	case eInVal_ScreenH: return gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat().y;
	case eInVal_GameTime: return gpBase->mpEngine->GetGameTime();
	case eInVal_PadPresent: return gpPad != NULL;
	case eInVal_PadButton: return gpPad->ButtonIsDown((eGamepadButton)alIdx);
	case eInVal_PadAxis: return gpPad->GetAxisValue((eGamepadAxis)alIdx);
	case eInVal_PadDead: return gpPad->GetAxisDeadZoneRadiusValue();
	case eInVal_FrameTime: return gpBase->mpEngine->GetFrameTime();
	case eInVal_StepSize: return gpBase->mpEngine->GetStepSize();
	}
	return 0;
}

//-----------------------------------------------------------------------

bool OhplInQ(int alQ)
{
	if(gbScript)
	{
		bool bRes;
		if(alQ == eInQ_KeyPop)
		{
			bRes = glKeys > 0;
			if(bRes) --glKeys;
		}
		else if(alQ == eInQ_PadSetup) bRes = (glStep / 40) % 2 == 1;
		else bRes = Mix(glStep, 200, alQ) % 2 == 0;
		OhplInLog("c q %d %d", alQ, bRes ? 1 : 0);
		return bRes;
	}
	switch(alQ)
	{
	case eInQ_DebugMenu: return gpBase->mpConfigHandler->mbLoadDebugMenu;
	case eInQ_PTest: return gpBase->mbPTestActivated;
	case eInQ_PlayerDead: return gpBase->mpPlayer->IsDead();
	case eInQ_PlayerActive: return gpBase->mpPlayer->IsActive();
	case eInQ_AllowMenus: return gpBase->mpPlayer->GetCurrentStateData()->AllowPlayerMenus();
	case eInQ_InvDisabled: return gpBase->mpInventory->GetDisabled();
	case eInQ_MsgPause: return gpBase->mpMessageHandler->IsPauseMessageActive();
	case eInQ_EffectPaused: return gpBase->mpEffectHandler->GetPlayerIsPaused();
	case eInQ_InvMsg: return gpBase->mpInventory->GetMessageActive();
	case eInQ_DemoEnd: return gpBase->mpDemoEnd != NULL;
	case eInQ_QuickSave: return gpBase->mpDebugHandler->GetAllowQuickSave();
	case eInQ_FastForward: return gpBase->mpDebugHandler->GetFastForward();
	case eInQ_Crosshair: return gpBase->mpPlayer->GetShowCrosshair();
	case eInQ_PreContinue: return gpBase->mpPreMenu->IsContinueButtonVisible();
	case eInQ_KeyPop:
		{
			cInput *pInput = gpBase->mpEngine->GetInput();
			if(pInput->GetKeyboard()->KeyIsPressed() == false) return false;
			gpBase->mpEngine->GetGui()->SendKeyPress(pInput->GetKeyboard()->GetKey());
			return true;
		}
	case eInQ_PadSetup:
		gpPad = gpBase->mpEngine->GetInput()->GetGamepad(0);
		if(gpPad) gpPad->SetAxisDeadZoneRadiusValue(0.15f);
		return gpPad != NULL;
	}
	return false;
}

//-----------------------------------------------------------------------

static cGuiSet *MenuSet(int alSet)
{
	switch(alSet)
	{
	case 0: return gpBase->mpInventory->GetSet();
	case 1: return gpBase->mpPreMenu->GetSet();
	case 2: return gpBase->mpMainMenu->GetSet();
	}
	return gpBase->mpJournal->GetSet();
}

void OhplInFx(int alOp, int alA, int alB, float afF)
{
	if(gbScript)
	{
		OhplInLog("c fx %d %d %d %.9g", alOp, alA, alB, afF);
		return;
	}
	cEngine *pEngine = gpBase->mpEngine;
	cGui *pGui = pEngine->GetGui();
	cInput *pInput = pEngine->GetInput();
	cLuxPlayer *pPlayer = gpBase->mpPlayer;
	switch(alOp)
	{
	case eInFx_RenderLog: gpBase->mpMapHandler->GetViewport()->GetRenderSettings()->mbLog = alA != 0; break;
	case eInFx_GuiMouse: pGui->SendMousePos(pInput->GetMouse()->GetAbsPosition(), pInput->GetMouse()->GetRelPosition()); break;
	case eInFx_GuiClick:
		if(alA == 0) pGui->SendMouseClickDown((eGuiMouseButton)alB);
		else if(alA == 1) pGui->SendMouseClickUp((eGuiMouseButton)alB);
		else pGui->SendMouseDoubleClick((eGuiMouseButton)alB);
		break;
	case eInFx_PreMenuButton: gpBase->mpPreMenu->ButtonPressed(); break;
	case eInFx_PadDrain:
		while(gpPad->HasInputUpdates()) pGui->SendGamepadInput(gpPad->GetInputUpdate());
		break;
	case eInFx_UiSend:
		switch(alA)
		{
		case 0: pGui->SendUIArrowPress((eUIArrow)alB); break;
		case 1: pGui->SendUIArrowRelease((eUIArrow)alB); break;
		case 4: pGui->SendUIButtonPress((eUIButton)alB); break;
		case 5: pGui->SendUIButtonRelease((eUIButton)alB); break;
		case 6: pGui->SendUIButtonDoublePress((eUIButton)alB); break;
		}
		break;
	case eInFx_DebugWindow: gpBase->mpDebugHandler->SetDebugWindowActive(alA != 0); break;
	case eInFx_DebugReload: gpBase->mpDebugHandler->QuickReloadMap(); break;
	case eInFx_FastForward: gpBase->mpDebugHandler->SetFastForward(alA != 0); break;
	case eInFx_AutoSave: gpBase->mpSaveHandler->AutoSave(); break;
	case eInFx_AutoLoad: gpBase->mpSaveHandler->AutoLoad(false); break;
	case eInFx_Container: pEngine->GetUpdater()->SetContainer(alA == 0 ? "MainMenu" : (alA == 1 ? "Inventory" : "Journal")); break;
	case eInFx_Crosshair: pPlayer->SetShowCrosshair(alA != 0); break;
	case eInFx_DeathPress: pPlayer->GetHelperDeath()->OnPressButton(); break;
	case eInFx_JournalForce: gpBase->mpJournal->SetForceInstantExit(true); break;
	case eInFx_JournalQuestLog: gpBase->mpJournal->ChangeState(eLuxJournalState_QuestLog); break;
	case eInFx_JournalRecent: gpBase->mpJournal->OpenLastReadText(); break;
	case eInFx_JournalExit: gpBase->mpJournal->ExitPressed(alA != 0); break;
	case eInFx_PlMove: pPlayer->Move((eCharDir)alA, (float)alB); break;
	case eInFx_PlLeanSet: pPlayer->SetLean(afF); break;
	case eInFx_PlLeanAdd: pPlayer->AddLean(afF); break;
	case eInFx_PlScroll: pPlayer->Scroll(afF); break;
	case eInFx_PlAction: pPlayer->DoAction((eLuxPlayerAction)alA, alB != 0); break;
	case eInFx_PlRun: pPlayer->Run(alA != 0); break;
	case eInFx_PlJump: pPlayer->Jump(alA != 0); break;
	case eInFx_PlCrouch: pPlayer->Crouch(alA != 0); break;
	case eInFx_PlHolster:
		pPlayer->GiveSanityDamage(10);
		pPlayer->LowerSanity(3, true);
		pPlayer->GiveDamage(10, 10, eLuxDamageType_BloodSplat, true, true);
		break;
	case eInFx_PlYaw: pPlayer->AddYaw(afF); break;
	case eInFx_PlPitch: pPlayer->AddPitch(afF); break;
	case eInFx_PlStop: pPlayer->GetCharacterBody()->StopMovement(); break;
	case eInFx_MsgAction: gpBase->mpMessageHandler->DoAction((eLuxPlayerAction)alA, alB != 0); break;
	case eInFx_EffectAction: gpBase->mpEffectHandler->DoAction((eLuxPlayerAction)alA, alB != 0); break;
	case eInFx_MenuMouse:
		MenuSet(alA)->SetMouseMovementEnabled(true);
		MenuSet(alA)->SetDrawMouse(true);
		if(alA == 0) MenuSet(0)->SetDrawFocus(false);
		break;
	case eInFx_HideMouseUis:
		gpBase->mpInventory->GetSet()->SetDrawMouse(false);
		gpBase->mpInventory->GetSet()->SetMouseMovementEnabled(false);
		gpBase->mpInventory->GetSet()->SetDrawFocus(true);
		for(int i : { 2, 3, 1 })
		{
			MenuSet(i)->SetMouseMovementEnabled(false);
			MenuSet(i)->SetDrawMouse(false);
		}
		break;
	case eInFx_InvExit: gpBase->mpInventory->ExitPressed(); break;
	case eInFx_InvJournal: gpBase->mpInventory->OpenJournal(); break;
	case eInFx_MainMenuExit: gpBase->mpMainMenu->ExitPressed(); break;
	case eInFx_CreditsExit: gpBase->mpCredits->ExitPressed(); break;
	case eInFx_DemoEndExit: gpBase->mpDemoEnd->Exit(false); break;
	case eInFx_LoadScreenExit: gpBase->mpLoadScreenHandler->ExitPressed(); break;
	case eInFx_ResetActions: pInput->ResetActionsToCurrentState(); break;
	}
}

//-----------------------------------------------------------------------

tWString OhplInShotDir()
{
	if(gbScript)
	{
		OhplInLog("c shotdir");
		return _W("/shots");
	}
	tWString sDir = cString::AddSlashAtEndW(cPlatform::GetSystemSpecialPath(eSystemPath_XDGPictures)) + _W("OpenHPL");
	if(cPlatform::FolderExists(sDir) == false) cPlatform::CreateFolder(sDir);
	return sDir;
}

tString OhplInShotExt()
{
	return gpBase->mpConfigHandler->ScreenShotExt();
}

bool OhplInFileExists(const tWString &asPath)
{
	if(gbScript)
	{
		bool bRes = asPath.size() % 3 != 0 && asPath.size() < 40;
		OhplInLog("c exists %d", bRes ? 1 : 0);
		return bRes;
	}
	return cPlatform::FileExists(asPath);
}

void OhplInShotSave(const tWString &asPath)
{
	if(gbScript)
	{
		OhplInLog("c shotsave %d", (int)asPath.size());
		return;
	}
	cEngine *pEngine = gpBase->mpEngine;
	cBitmap *pBmp = pEngine->GetGraphics()->GetLowLevel()->CopyFrameBufferToBitmap();
	pEngine->GetResources()->GetBitmapLoaderHandler()->SaveBitmap(pBmp, asPath, 0);
	hplDelete(pBmp);
}

bool OhplInInputName(const tString &asAction, tWString &asOut)
{
	cAction *pAction = gpBase->mpEngine->GetInput()->GetAction(asAction);
	if(pAction == NULL) return false;
	asOut = cString::To16Char(pAction->GetSubAction(0)->GetInputName());
	return true;
}

//-----------------------------------------------------------------------

enum { eInOp_Trig = 160, eInOp_Val, eInOp_Q, eInOp_Fx, eInOp_ShotDir, eInOp_ShotExt, eInOp_FileExists, eInOp_ShotSave,
	eInOp_GameInt, eInOp_GameFloat, eInOp_LogCfg };

static bool gbLogCfg = false;

static void ActionCall(uint32_t alOp, const OhplGa9Arg *a, OhplGa9Ret *r)
{
	cInput *pInput = gpBase->mpEngine->GetInput();
	switch(alOp)
	{
	case eOhplGa9Op_ActionCreate: pInput->CreateAction(OhplGa9GetS(a[0]), (int)a[1].i); break;
	case eOhplGa9Op_ActionClear: pInput->GetAction((int)a[0].i)->ClearSubActions(); break;
	case eOhplGa9Op_ActionAdd:
		{
			cAction *pAction = pInput->GetAction((int)a[0].i);
			int lA = (int)a[2].i, lB = (int)a[3].i;
			switch(a[1].i)
			{
			case 0: pAction->AddKey((eKey)lA); break;
			case 1: pAction->AddMouseButton((eMouseButton)lA); break;
			case 2: pAction->AddGamepadButton(0, (eGamepadButton)lA); break;
			case 3: pAction->AddGamepadHat(0, (eGamepadHat)lA, (eGamepadHatState)lB); break;
			case 4: pAction->AddGamepadAxis(0, (eGamepadAxis)lA, (eGamepadAxisRange)lB, 0.25f); break;
			}
		}
		break;
	case eOhplGa9Op_ActionCount: r->i = pInput->GetAction((int)a[0].i)->GetSubActionNum(); break;
	case eOhplGa9Op_ActionType: OhplGa9RetS(r, pInput->GetAction((int)a[0].i)->GetSubAction((size_t)a[1].i)->GetInputType()); break;
	case eOhplGa9Op_ActionInput: OhplGa9RetS(r, pInput->GetAction((int)a[0].i)->GetSubAction((size_t)a[1].i)->GetInputName()); break;
	}
}

void OhplInHostCall(void *apCtx, uint32_t alOp, const OhplGa9Arg *a, size_t alNum, OhplGa9Ret *r)
{
	if(alOp < eInOp_Trig)
	{
		if(gbLogCfg) OhplA12bLogCall("input", alOp, a, alNum);
		if(alOp >= eOhplGa9Op_ActionCreate) ActionCall(alOp, a, r);
		else if(alOp >= eOhplGa9Op_Cfg2) OhplGa9CfgCall(gpBase->mpUserKeyConfig, alOp - eOhplGa9Op_Cfg2, a, r);
		else OhplGa9CfgCall(gpBase->mpUserConfig, alOp, a, r);
		return;
	}
	switch(alOp)
	{
	case eInOp_Trig: r->i = OhplInTrig((int)a[0].i, (int)a[1].i); break;
	case eInOp_Val: r->d = OhplInVal((int)a[0].i, (int)a[1].i); break;
	case eInOp_Q: r->i = OhplInQ((int)a[0].i); break;
	case eInOp_Fx: OhplInFx((int)a[0].i, (int)a[1].i, (int)a[2].i, a[3].f); break;
	case eInOp_ShotDir: OhplGa9RetW(r, OhplInShotDir()); break;
	case eInOp_ShotExt: OhplGa9RetS(r, OhplInShotExt()); break;
	case eInOp_FileExists: r->i = OhplInFileExists(OhplGa9GetW(a[0])); break;
	case eInOp_ShotSave: OhplInShotSave(OhplGa9GetW(a[0])); break;
	case eInOp_GameInt: r->i = gpBase->mpGameCfg->GetInt(OhplGa9GetS(a[0]), OhplGa9GetS(a[1]), (int)a[2].i); break;
	case eInOp_GameFloat: r->f = gpBase->mpGameCfg->GetFloat(OhplGa9GetS(a[0]), OhplGa9GetS(a[1]), a[2].f); break;
	case eInOp_LogCfg: gbLogCfg = a[0].i != 0; break;
	}
}

//-----------------------------------------------------------------------

static const eLuxInputState gvStates[] = { eLuxInputState_Game, eLuxInputState_MainMenu, eLuxInputState_Inventory, eLuxInputState_Journal,
	eLuxInputState_Debug, eLuxInputState_PreMenu, eLuxInputState_Credits, eLuxInputState_DemoEnd, eLuxInputState_LoadScreen };

static bool gbDone = false;

void OhplInDumpTick()
{
	if(OhplInDumpEnabled() == false || gbDone) return;
	static int lFrame = 0;
	if(gpBase->mpMapHandler == NULL || gpBase->mpMapHandler->GetCurrentMap() == NULL) { lFrame = 0; return; }
	if(++lFrame < 10) return;
	gbDone = true;
	cLuxInputHandler *pH = gpBase->mpInputHandler;
	OhplInLog("map %s", gpBase->mpMapHandler->GetCurrentMap()->GetName().c_str());

	gbScript = true;
	uint32_t lSeq = 0;
	for(size_t s = 0; s < sizeof(gvStates) / sizeof(gvStates[0]); ++s)
	{
		glStep = ++lSeq;
		OhplInLog("s begin %d", (int)gvStates[s]);
		pH->ChangeState(gvStates[s]);
		OhplInLog("s state %d", (int)pH->GetState());
		int lSteps = gvStates[s] == eLuxInputState_Game ? 1800 : 200;
		for(int i = 0; i < lSteps; ++i)
		{
			glStep = ++lSeq;
			glKeys = (int)(Mix(glStep, 300, 0) % 3);
			OhplInLog("s step %u keys %d", glStep, glKeys);
			pH->Update(1.0f / 60.0f);
			OhplInLog("s state %d", (int)pH->GetState());
			pH->OnPostRender(0.016f);
		}
		OhplInLog("s smooth %.9g", pH->GetSmoothMousePos(cVector2f(1, 2)).x);
	}
	glStep = ++lSeq;
	pH->ChangeState(eLuxInputState_Game);
	OhplInLog("s end");
	gbScript = false;

	const char *vNames[] = { "Forward", "Jump", "NoSuchAction" };
	for(int i = 0; i < 3; ++i)
	{
		tWString sName = pH->GetInputName(vNames[i]);
		OhplInLog("s name %s %s", vNames[i], cString::To8Char(sName).c_str());
	}
	OhplInLog("done");
}
