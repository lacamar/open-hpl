// Host of the Rust inventory (rust/src/luxui/invhost.rs): one op entry reaching the engine objects.

#include "LuxInventory.h"
#include "LuxJournal.h"

#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxPlayerState.h"
#include "LuxInputHandler.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxMessageHandler.h"
#include "LuxHintHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxEffectHandler.h"
#include "LuxJournal.h"
#include "LuxGlobalDataHandler.h"
#include "LuxAchievementHandler.h"
#include "LuxCompletionCountHandler.h"
#include "LuxSavedGameTypes.h"

namespace
{

enum
{
	kA11a_Op_CFG = 1, kA11a_Op_TRANSLATE = 2, kA11a_Op_PARSE_STRING = 3, kA11a_Op_STRING_DURATION = 4, kA11a_Op_GUI_INIT = 5,
	kA11a_Op_SET_OP = 6, kA11a_Op_FONT_LOAD = 12, kA11a_Op_WIDGET_CB = 16, kA11a_Op_LL = 19, kA11a_Op_SCRIPT = 20,
	kA11a_Op_GAME = 21, kA11a_Op_ITEM_EVENT = 22, kA11a_Op_LOG = 24, kA11a_Op_JGAME = 27
};

enum
{
	kA11a_Cfg_FLOAT = 0, kA11a_Cfg_STRING = 1, kA11a_Cfg_V3 = 2, kA11a_Cfg_V2 = 3, kA11a_Cfg_COLOR = 4, kA11a_Cfg_INT = 5, kA11a_Cfg_BOOL = 6
};

enum
{
	kA11a_Set_VIEWPORT = 8, kA11a_Set_GAMEPAD_PRESENT = 9
};

enum
{
	kA11a_Ll_SCREEN_SIZE_INT = 0, kA11a_Ll_TEXTURE_NEW = 1, kA11a_Ll_TEXTURE_DESTROY = 2, kA11a_Ll_FB_NEW = 3, kA11a_Ll_FB_DESTROY = 4,
	kA11a_Ll_BG_SCREEN = 5, kA11a_Ll_BLEND = 6, kA11a_Ll_DEPTH_TEST = 7, kA11a_Ll_DEPTH_WRITE = 8, kA11a_Ll_ORTHO = 9, kA11a_Ll_IDENTITY = 10,
	kA11a_Ll_COPY_SCREEN = 11, kA11a_Ll_PROGRAM = 12, kA11a_Ll_SET_FB = 13, kA11a_Ll_SET_TEXTURE = 14, kA11a_Ll_DRAW_QUAD = 15,
	kA11a_Ll_FLUSH = 16, kA11a_Ll_PROGRAM_NEW = 17
};

enum
{
	kA11a_Game_SANITY = 0, kA11a_Game_HEALTH = 1, kA11a_Game_LAMP_OIL = 2, kA11a_Game_TINDERBOXES = 3, kA11a_Game_COINS = 4,
	kA11a_Game_STATE_USE_ITEM = 5, kA11a_Game_LANTERN_ACTIVE = 6, kA11a_Game_TRIGGERED = 7, kA11a_Game_GAME_TIME = 8,
	kA11a_Game_SET_CONTAINER = 9, kA11a_Game_INPUT_STATE = 10, kA11a_Game_HINT_UPDATE = 12,
	kA11a_Game_HINT_DRAW = 13, kA11a_Game_FLASH_UPDATE = 14, kA11a_Game_FLASH_DRAW = 15, kA11a_Game_LEAVE_USE_ITEM = 16,
	kA11a_Game_RESUME_SOUNDS = 17, kA11a_Game_FADE_LOOPS = 18, kA11a_Game_STOP_TERROR = 19, kA11a_Game_VOICES = 20,
	kA11a_Game_TINDER_GET = 21, kA11a_Game_TINDER_SET = 22, kA11a_Game_ACHIEVEMENT = 23, kA11a_Game_SET_MESSAGE = 24,
	kA11a_Game_MAP_FOLDER = 25
};

enum
{
	kA11a_Ev_PROG_LOG = 1, kA11a_Ev_SET_COINS = 2, kA11a_Ev_HINT = 3, kA11a_Ev_SET_CONTAINER_JOURNAL = 4, kA11a_Ev_RUN_MAP_SCRIPT = 7, kA11a_Ev_LANTERN_TOGGLE = 10,
	kA11a_Ev_SET_HEALTH = 11, kA11a_Ev_ADD_SANITY = 12, kA11a_Ev_SET_LAMP_OIL = 13, kA11a_Ev_SOUND = 14, kA11a_Ev_USE_ITEM_STATE_RESET = 15,
	kA11a_Ev_USE_ITEM_ENTER = 16, kA11a_Ev_ADD_TINDERBOX = 17, kA11a_Ev_COMPLETION = 18
};

enum
{
	kA11a_Jg_PROG_LOW = 3, kA11a_Jg_HINT_RECENT = 4,
	kA11a_Jg_MAIN_BOOL_GET = 5, kA11a_Jg_MAIN_BOOL_SAVE = 6, kA11a_Jg_ACH_ARCHIVIST = 7, kA11a_Jg_GAME_NAME = 8, kA11a_Jg_DPAD_DOWN_OK = 9
};

// Ops numbered 100 + n run op n against the journal's GUI set, effect program, config section and font loader.
const int kJournalBase = 100;

struct HostCtx
{
	cGuiSet *pSet;
	cViewport *pViewport;
	iGpuProgram *pProgram;
};

cLuxInventory *gpInv = NULL;
cLuxJournal *gpJrn = NULL;
HostCtx gInvCtx = { NULL, NULL, NULL };
HostCtx gJrnCtx = { NULL, NULL, NULL };
HostCtx *gpCtx = &gInvCtx;
#define gpSet (gpCtx->pSet)
#define gpViewport (gpCtx->pViewport)
#define gpEffectProgram (gpCtx->pProgram)
std::string gsRet;
tWString gsRetW;

tString S(OhplA11aArgs *a, int i){ return tString((const char*)a->s[i].p, a->s[i].n); }
tWString W(OhplA11aArgs *a, int i){ return tWString((const wchar_t*)a->w[i].p, a->w[i].n); }
cVector3f V3(OhplA11aArgs *a, int i = 0){ return cVector3f(a->f[i], a->f[i+1], a->f[i+2]); }
cVector2f V2(OhplA11aArgs *a, int i = 0){ return cVector2f(a->f[i], a->f[i+1]); }
cColor Col(OhplA11aArgs *a, int i){ return cColor(a->f[i], a->f[i+1], a->f[i+2], a->f[i+3]); }
void RetS(OhplA11aArgs *a, const tString &asX){ gsRet = asX; a->rs = OhplA11aS(gsRet); }
void RetW(OhplA11aArgs *a, const tWString &asX){ gsRetW = asX; a->rw.p = (const unsigned int*)gsRetW.data(); a->rw.n = gsRetW.size(); }

eGuiMessage Msg(int alMsg)
{
	static const eGuiMessage vMsg[] = { eGuiMessage_MouseDown, eGuiMessage_MouseUp, eGuiMessage_MouseDoubleClick, eGuiMessage_MouseEnter,
		eGuiMessage_MouseLeave, eGuiMessage_OnUpdate, eGuiMessage_OnDraw, eGuiMessage_GetUINavFocus, eGuiMessage_LoseUINavFocus,
		eGuiMessage_UIButtonPress, eGuiMessage_UIButtonDoublePress, eGuiMessage_UIButtonRelease, eGuiMessage_UIArrowPress };
	return vMsg[alMsg];
}

bool Tramp(void *apToken, iWidget *apWidget, const cGuiMessageData &aData)
{
	return ohpl_inv_cb((uintptr_t)apToken, (uintptr_t)apWidget, aData.mlVal, aData.mfVal) != 0;
}

bool TrampJ(void *apToken, iWidget *apWidget, const cGuiMessageData &aData)
{
	return ohpl_jrn_cb((uintptr_t)apToken, (uintptr_t)apWidget, aData.mlVal, aData.mfVal) != 0;
}

bool InJournal(){ return gpCtx == &gJrnCtx; }
const char *CfgSection(){ return InJournal() ? "Journal" : "Inventory"; }

void OpCfg(OhplA11aArgs *a)
{
	cConfigFile *pCfg = gpBase->mpMenuCfg;
	tString sKey = S(a, 0);
	switch(a->i[0])
	{
	case kA11a_Cfg_FLOAT: a->f[0] = pCfg->GetFloat(CfgSection(), sKey, a->f[0]); break;
	case kA11a_Cfg_STRING: RetS(a, pCfg->GetString(CfgSection(), sKey, S(a, 1))); break;
	case kA11a_Cfg_V3: { cVector3f v = pCfg->GetVector3f(CfgSection(), sKey, V3(a)); a->f[0] = v.x; a->f[1] = v.y; a->f[2] = v.z; } break;
	case kA11a_Cfg_V2: { cVector2f v = pCfg->GetVector2f(CfgSection(), sKey, V2(a)); a->f[0] = v.x; a->f[1] = v.y; } break;
	case kA11a_Cfg_COLOR: { cColor c = pCfg->GetColor(CfgSection(), sKey, Col(a, 0)); a->f[0] = c.r; a->f[1] = c.g; a->f[2] = c.b; a->f[3] = c.a; } break;
	case kA11a_Cfg_INT: a->i[1] = pCfg->GetInt(CfgSection(), sKey, a->i[1]); break;
	case kA11a_Cfg_BOOL: a->i[1] = pCfg->GetBool(CfgSection(), sKey, a->i[1] != 0); break;
	}
}

void OpGuiInit(OhplA11aArgs *a)
{
	cGui *pGui = gpBase->mpEngine->GetGui();
	cGuiSkin *pSkin = pGui->CreateSkin("gui_main_menu.skin");
	gpSet = pGui->CreateSet("Inventory", pSkin);
	if(InJournal() == false)
	{
		gpSet->SetFocusDrawCallback((void*)a->p[0], &Tramp);
#ifdef USE_GAMEPAD
		gpSet->SetDrawFocus(gpBase->mpInputHandler->IsGamepadPresent());
#endif
	}
	cVector2f vCenterSize(800, 600), vSize, vOffset;
	LuxCalcGuiSetScreenOffset(vCenterSize, vSize, vOffset);
	gpSet->SetVirtualSize(vSize, -1000, 1000, vOffset, true);
	gpSet->SetActive(false);
	gpSet->SetDrawMouse(false);
	cVector2f vScreen = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
	gpViewport = gpBase->mpEngine->GetScene()->CreateViewport();
	gpViewport->SetActive(false);
	gpViewport->SetVisible(false);
	gpViewport->AddGuiSet(gpSet);
	a->r = (uintptr_t)gpSet;
	a->f[0] = vSize.x; a->f[1] = vSize.y; a->f[2] = vOffset.x; a->f[3] = vOffset.y; a->f[4] = vScreen.x; a->f[5] = vScreen.y;
}

void OpSet(OhplA11aArgs *a)
{
	switch(a->i[0])
	{
	case kA11a_Set_VIEWPORT: gpViewport->SetActive(a->i[1] != 0); gpViewport->SetVisible(a->i[1] != 0); break;
	case kA11a_Set_GAMEPAD_PRESENT: a->r = gpBase->mpInputHandler->IsGamepadPresent(); break;
	}
}

void OpLowLevel(OhplA11aArgs *a)
{
	cGraphics *pGraphics = gpBase->mpEngine->GetGraphics();
	iLowLevelGraphics *pLow = pGraphics->GetLowLevel();
	switch(a->i[0])
	{
	case kA11a_Ll_SCREEN_SIZE_INT: { cVector3l v = pLow->GetScreenSizeInt(); a->i[1] = v.x; a->i[2] = v.y; } break;
	case kA11a_Ll_TEXTURE_NEW:
		{
			iTexture *pTex = pGraphics->CreateTexture(S(a, 0), eTextureType_Rect, eTextureUsage_RenderTarget);
			pTex->CreateFromRawData(cVector3l(a->i[2], a->i[3], 0), ePixelFormat_RGBA, NULL);
			if(a->i[1]) pTex->SetWrapSTR(eTextureWrap_ClampToEdge);
			a->r = (uintptr_t)pTex;
		}
		break;
	case kA11a_Ll_TEXTURE_DESTROY: pGraphics->DestroyTexture((iTexture*)a->p[0]); break;
	case kA11a_Ll_FB_NEW:
		{
			iFrameBuffer *pBuf = pGraphics->CreateFrameBuffer("InventoryEffectbuffer");
			pBuf->SetTexture2D(0, (iTexture*)a->p[0]);
			pBuf->CompileAndValidate();
			a->r = (uintptr_t)pBuf;
		}
		break;
	case kA11a_Ll_FB_DESTROY: pGraphics->DestroyFrameBuffer((iFrameBuffer*)a->p[0]); break;
	case kA11a_Ll_BG_SCREEN: gpBase->mpHelpFuncs->RenderBackgroundScreen(a->i[1] != 0); break;
	case kA11a_Ll_BLEND: pLow->SetBlendActive(a->i[1] != 0); break;
	case kA11a_Ll_DEPTH_TEST: pLow->SetDepthTestActive(a->i[1] != 0); break;
	case kA11a_Ll_DEPTH_WRITE: pLow->SetDepthWriteActive(a->i[1] != 0); break;
	case kA11a_Ll_ORTHO: pLow->SetOrthoProjection(V2(a), -1000, 1000); break;
	case kA11a_Ll_IDENTITY: pLow->SetIdentityMatrix(eMatrix_ModelView); break;
	case kA11a_Ll_COPY_SCREEN: pLow->CopyFrameBufferToTexure((iTexture*)a->p[0], 0, pLow->GetScreenSizeInt(), 0); break;
	case kA11a_Ll_PROGRAM: if(a->i[1]) gpEffectProgram->Bind(); else gpEffectProgram->UnBind(); break;
	case kA11a_Ll_SET_FB: pLow->SetCurrentFrameBuffer((iFrameBuffer*)a->p[0]); break;
	case kA11a_Ll_SET_TEXTURE: pLow->SetTexture(0, (iTexture*)a->p[0]); break;
	case kA11a_Ll_DRAW_QUAD: pLow->DrawQuad(0, V2(a), cVector2f(0, a->f[1]), cVector2f(a->f[0], 0), cColor(1,1)); break;
	case kA11a_Ll_FLUSH: pLow->FlushRendering(); pLow->WaitAndFinishRendering(); break;
	case kA11a_Ll_PROGRAM_NEW:
		{
			cParserVarContainer programVars;
			gpEffectProgram = pGraphics->CreateGpuProgramFromShaders("InventoryEffect", "inventory_screen_effect_vtx.glsl", "inventory_screen_effect_frag.glsl", &programVars);
		}
		break;
	}
}

void OpScript(OhplA11aArgs *a)
{
	cScriptManager *pMan = gpBase->mpEngine->GetResources()->GetScriptManager();
	switch(a->i[0])
	{
	case 0: a->r = (uintptr_t)pMan->CreateScript(S(a, 0)); break;
	case 1: pMan->Destroy((iScript*)a->p[0]); break;
	case 2: ((iScript*)a->p[0])->Run(S(a, 0)); break;
	case 3: { tString sOut; a->r = (uintptr_t)pMan->CreateScript(S(a, 0), &sOut); RetS(a, sOut); } break;
	}
}

eLuxInputState InputState(int alState)
{
	return alState == 0 ? eLuxInputState_Game : (alState == 1 ? eLuxInputState_Null : (alState == 3 ? eLuxInputState_Journal : eLuxInputState_Inventory));
}

void OpGame(OhplA11aArgs *a)
{
	cLuxPlayer *pPlayer = gpBase->mpPlayer;
	switch(a->i[0])
	{
	case kA11a_Game_SANITY: a->f[0] = pPlayer->GetSanity(); break;
	case kA11a_Game_HEALTH: a->f[0] = pPlayer->GetHealth(); break;
	case kA11a_Game_LAMP_OIL: a->f[0] = pPlayer->GetLampOil(); break;
	case kA11a_Game_TINDERBOXES: a->r = pPlayer->GetTinderboxes(); break;
	case kA11a_Game_COINS: a->r = pPlayer->GetCoins(); break;
	case kA11a_Game_STATE_USE_ITEM: a->r = pPlayer->GetCurrentState() == eLuxPlayerState_UseItem; break;
	case kA11a_Game_LANTERN_ACTIVE: a->r = pPlayer->GetHelperLantern()->IsActive(); break;
	case kA11a_Game_TRIGGERED: a->r = gpBase->mpEngine->GetInput()->IsTriggerd(a->i[1] == 0 ? eLuxAction_LeftClick : eLuxAction_UIPrimary); break;
	case kA11a_Game_GAME_TIME: a->d = gpBase->mpEngine->GetGameTime(); break;
	case kA11a_Game_SET_CONTAINER: gpBase->mpEngine->GetUpdater()->SetContainer(S(a, 0)); break;
	case kA11a_Game_INPUT_STATE: gpBase->mpInputHandler->ChangeState(InputState(a->i[1])); break;
	case kA11a_Game_HINT_UPDATE: gpBase->mpHintHandler->UpdateHintText(a->f[0]); break;
	case kA11a_Game_HINT_DRAW: gpBase->mpHintHandler->DrawHintText(a->f[0], gpSet); break;
	case kA11a_Game_FLASH_UPDATE: gpBase->mpEffectHandler->GetSanityGainFlash()->Update(a->f[0]); break;
	case kA11a_Game_FLASH_DRAW: gpBase->mpEffectHandler->GetSanityGainFlash()->DrawFlash(gpSet, a->f[0]); break;
	case kA11a_Game_LEAVE_USE_ITEM: pPlayer->ChangeState(eLuxPlayerState_Normal); break;
	case kA11a_Game_RESUME_SOUNDS: gpBase->mpMapHandler->ResumeSoundsAndMusic(); break;
	case kA11a_Game_FADE_LOOPS: gpBase->mpMapHandler->GetCurrentMap()->GetPhysicsWorld()->FadeoutAllLoopSounds(1.0f); break;
	case kA11a_Game_STOP_TERROR: pPlayer->StopTerrorSound(); break;
	case kA11a_Game_VOICES:
		{
			cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
			if(a->i[1])
			{
				gpBase->mpEffectHandler->GetPlayVoice()->PauseCurrentVoices();
				pSoundHandler->FadeGlobalVolume(0.5f, 0.5f, eSoundEntryType_World, eLuxGlobalVolumeType_GameMenu, false);
			}
			else
			{
				gpBase->mpEffectHandler->GetPlayVoice()->UnpauseCurrentVoices();
				pSoundHandler->FadeGlobalVolume(1.0f, 0.5f, eSoundEntryType_World, eLuxGlobalVolumeType_GameMenu, false);
			}
		}
		break;
	case kA11a_Game_TINDER_GET:
		{
			cLuxScriptVar *pVar = gpBase->mpGlobalDataHandler->GetVar("TinderboxesCollected");
			a->i[1] = pVar != NULL;
			if(pVar) RetS(a, pVar->msVal);
		}
		break;
	case kA11a_Game_TINDER_SET:
		{
			cLuxScriptVar *pVar = gpBase->mpGlobalDataHandler->GetVar("TinderboxesCollected");
			if(pVar) pVar->msVal = S(a, 0);
		}
		break;
	case kA11a_Game_ACHIEVEMENT: gpBase->mpAchievementHandler->UnlockAchievement(a->i[1] == 0 ? eLuxAchievement_Illuminatus : eLuxAchievement_StillAlive); break;
	case kA11a_Game_SET_MESSAGE: gpBase->mpMessageHandler->SetMessage(W(a, 0), 0); break;
	case kA11a_Game_MAP_FOLDER: RetS(a, gpBase->mpMapHandler->GetMapFolder()); break;
	}
}

void AddCompletion(int alAmount)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap) pMap->AddCompletionAmount(alAmount);
}

void OpItemEvent(OhplA11aArgs *a)
{
	cLuxPlayer *pPlayer = gpBase->mpPlayer;
	tString sS = S(a, 0);
	switch(a->i[0])
	{
	case kA11a_Ev_PROG_LOG: ProgLog(eLuxProgressLogLevel_Medium, sS); break;
	case kA11a_Ev_COMPLETION:
		if(a->i[1] == 0) AddCompletion(gpBase->mpCompletionCountHandler->mlItemCompletionValue);
		else if(a->i[1] == 1) AddCompletion(gpBase->mpCompletionCountHandler->mlNoteCompletionValue);
		else AddCompletion(gpBase->mpCompletionCountHandler->mlDiaryCompletionValue);
		break;
	case kA11a_Ev_SET_COINS: pPlayer->SetCoins(a->i[1]); break;
	case kA11a_Ev_HINT: gpBase->mpHintHandler->Add(sS, kTranslate("Hints", sS), 0); break;
	case kA11a_Ev_SET_CONTAINER_JOURNAL: gpBase->mpEngine->GetUpdater()->SetContainer("Journal"); break;
	case kA11a_Ev_RUN_MAP_SCRIPT: gpBase->mpMapHandler->GetCurrentMap()->RunScript(sS); break;
	case kA11a_Ev_LANTERN_TOGGLE: pPlayer->GetHelperLantern()->SetActive(a->i[1] != 0, true); break;
	case kA11a_Ev_SET_HEALTH: pPlayer->SetHealth(a->f[0]); break;
	case kA11a_Ev_ADD_SANITY: pPlayer->AddSanity(a->f[0]); break;
	case kA11a_Ev_SET_LAMP_OIL: pPlayer->SetLampOil(a->f[0]); break;
	case kA11a_Ev_SOUND: gpBase->mpHelpFuncs->PlayGuiSoundData(sS, eSoundEntryType_Gui); break;
	case kA11a_Ev_USE_ITEM_STATE_RESET: pPlayer->ChangeState(eLuxPlayerState_Normal); break;
	case kA11a_Ev_USE_ITEM_ENTER:
		cLuxPlayerStateVars::SetupUseItem((cLuxInventory_Item*)a->p[0]);
		pPlayer->ChangeState(eLuxPlayerState_UseItem);
		break;
	case kA11a_Ev_ADD_TINDERBOX: pPlayer->AddTinderboxes(a->i[1]); break;
	}
}

void OpJGame(OhplA11aArgs *a)
{
	switch(a->i[0])
	{
	case kA11a_Jg_PROG_LOW: ProgLog(eLuxProgressLogLevel_Low, S(a, 0)); break;
	case kA11a_Jg_HINT_RECENT: gpBase->mpHintHandler->Add("RecentlyReadText", kTranslate("Hints", "RecentlyReadText"), -1); break;
	case kA11a_Jg_MAIN_BOOL_GET: a->r = gpBase->mpMainConfig->GetBool("Main", S(a, 0), false); break;
	case kA11a_Jg_MAIN_BOOL_SAVE:
		gpBase->mpMainConfig->SetBool("Main", S(a, 0), true);
		gpBase->mpMainConfig->Save();
		break;
	case kA11a_Jg_ACH_ARCHIVIST: gpBase->mpAchievementHandler->UnlockAchievement(eLuxAchievement_MasterArchivist); break;
	case kA11a_Jg_GAME_NAME: RetS(a, gpBase->msGameName); break;
	case kA11a_Jg_DPAD_DOWN_OK:
		a->r = 1;
#ifdef USE_GAMEPAD
# if USE_SDL2
		if(gpBase->mpInputHandler->GetGamepad() && gpBase->mpInputHandler->GetGamepad()->ButtonIsDown(eGamepadButton_DpadDown) == false) a->r = 0;
# else
		if(gpBase->mpInputHandler->GetGamepad() && gpBase->mpInputHandler->GetGamepad()->HatIsInState(eGamepadHat_0, eGamepadHatState_Down) == false) a->r = 0;
# endif
#endif
		break;
	}
}

void HostOp(int alOp, OhplA11aArgs *a)
{
	a->rs.p = NULL; a->rs.n = 0; a->rw.p = NULL; a->rw.n = 0;
	switch(alOp)
	{
	case kA11a_Op_CFG: OpCfg(a); break;
	case kA11a_Op_TRANSLATE: RetW(a, kTranslate(S(a, 0), S(a, 1))); break;
	case kA11a_Op_PARSE_STRING: RetW(a, gpBase->mpHelpFuncs->ParseString(W(a, 0))); break;
	case kA11a_Op_STRING_DURATION: a->f[0] = gpBase->mpHelpFuncs->GetStringDuration(W(a, 0)); break;
	case kA11a_Op_GUI_INIT: OpGuiInit(a); break;
	case kA11a_Op_SET_OP: OpSet(a); break;
	case kA11a_Op_FONT_LOAD:
		if(InJournal()) a->r = (uintptr_t)gpJrn->LoadFont(gpBase->mpMenuCfg->GetString("Journal", S(a, 0), ""));
		else a->r = (uintptr_t)gpInv->LoadFont(gpBase->mpMenuCfg->GetString("Inventory", S(a, 0), ""));
		break;
	case kA11a_Op_WIDGET_CB: ((iWidget*)a->p[0])->AddCallback(Msg(a->i[0]), (void*)a->p[1], InJournal() ? &TrampJ : &Tramp); break;
	case kA11a_Op_LL: OpLowLevel(a); break;
	case kA11a_Op_SCRIPT: OpScript(a); break;
	case kA11a_Op_GAME: OpGame(a); break;
	case kA11a_Op_ITEM_EVENT: OpItemEvent(a); break;
	case kA11a_Op_JGAME: OpJGame(a); break;
	case kA11a_Op_LOG:
		if(a->i[0] == 0) Error("%s", S(a, 0).c_str()); else Warning("%s", S(a, 0).c_str());
		break;
	}
}

void Host(int alOp, OhplA11aArgs *a)
{
	HostCtx *pPrev = gpCtx;
	if(alOp >= kJournalBase)
	{
		gpCtx = &gJrnCtx;
		alOp -= kJournalBase;
	}
	else gpCtx = &gInvCtx;
	HostOp(alOp, a);
	gpCtx = pPrev;
}

}

cLuxInventory::cLuxInventory() : iLuxUpdateable("LuxInventory")
{
	gpInv = this;
	ohpl_inv_set_host(&Host);
	ohpl_inv_new();
}

cLuxInventory::~cLuxInventory()
{
	ohpl_inv_free();
	gpInv = NULL;
}

cLuxJournal::cLuxJournal() : iLuxUpdateable("LuxJournal")
{
	gpJrn = this;
	ohpl_jrn_new();
}

cLuxJournal::~cLuxJournal()
{
	ohpl_jrn_free();
	gpJrn = NULL;
}
