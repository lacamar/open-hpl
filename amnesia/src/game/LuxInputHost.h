#ifndef LUX_INPUT_HOST_H
#define LUX_INPUT_HOST_H

#include "LuxBase.h"
#include "RustLuxBase.h"

// Engine boundary of the input handler. Codes mirror rust/src/luxbase/input_h.rs.
enum
{
	eInQ_DebugMenu, eInQ_PTest, eInQ_PlayerDead, eInQ_PlayerActive, eInQ_AllowMenus, eInQ_InvDisabled,
	eInQ_MsgPause, eInQ_EffectPaused, eInQ_InvMsg, eInQ_DemoEnd, eInQ_QuickSave, eInQ_FastForward,
	eInQ_Crosshair, eInQ_PreContinue, eInQ_KeyPop, eInQ_PadSetup
};

enum
{
	eInVal_MouseAnyButton, eInVal_MouseAbsX, eInVal_MouseAbsY, eInVal_MouseRelX, eInVal_MouseRelY,
	eInVal_ScreenW, eInVal_ScreenH, eInVal_GameTime, eInVal_PadPresent, eInVal_PadButton, eInVal_PadAxis,
	eInVal_PadDead, eInVal_FrameTime, eInVal_StepSize
};

enum
{
	eInFx_RenderLog, eInFx_GuiMouse, eInFx_GuiClick, eInFx_PreMenuButton, eInFx_PadDrain, eInFx_UiSend,
	eInFx_DebugWindow, eInFx_DebugReload, eInFx_FastForward, eInFx_AutoSave, eInFx_AutoLoad, eInFx_Container,
	eInFx_Crosshair, eInFx_DeathPress, eInFx_JournalForce, eInFx_JournalQuestLog, eInFx_JournalRecent,
	eInFx_JournalExit, eInFx_PlMove, eInFx_PlLeanSet, eInFx_PlLeanAdd, eInFx_PlScroll, eInFx_PlAction,
	eInFx_PlRun, eInFx_PlJump, eInFx_PlCrouch, eInFx_PlHolster, eInFx_PlYaw, eInFx_PlPitch, eInFx_PlStop,
	eInFx_MsgAction, eInFx_EffectAction, eInFx_MenuMouse, eInFx_HideMouseUis, eInFx_InvExit, eInFx_InvJournal,
	eInFx_MainMenuExit, eInFx_CreditsExit, eInFx_DemoEndExit, eInFx_LoadScreenExit, eInFx_ResetActions
};

bool OhplInTrig(int alKind, int alAction);
double OhplInVal(int alWhat, int alIdx);
bool OhplInQ(int alQ);
void OhplInFx(int alOp, int alA = 0, int alB = 0, float afF = 0);

tWString OhplInShotDir();
tString OhplInShotExt();
bool OhplInFileExists(const tWString &asPath);
void OhplInShotSave(const tWString &asPath);
bool OhplInInputName(const tString &asAction, tWString &asOut);

iGamepad *OhplInPad();

// Host call of the Rust input handler; op numbers are `hop` in rust/src/luxbase/input_h.rs.
void OhplInHostCall(void *apCtx, uint32_t alOp, const OhplGa9Arg *apArgs, size_t alNum, OhplGa9Ret *apRet);

bool OhplInScripted();
void OhplInDumpTick();
bool OhplInDumpEnabled();
void OhplInLog(const char *fmt, ...);

#endif
