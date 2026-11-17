// Host of the Rust inventory (rust/src/luxui/invhost.rs): one op entry reaching the engine objects.

#include "LuxInventory.h"
#include "LuxJournal.h"
#include "LuxA11cDump.h"

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
	kA11a_Op_SET_OP = 6, kA11a_Op_SET_QUERY = 7, kA11a_Op_GFX_NEW = 8, kA11a_Op_GFX_OP = 9, kA11a_Op_DRAW_GFX = 10,
	kA11a_Op_DRAW_FONT = 11, kA11a_Op_FONT_LOAD = 12, kA11a_Op_FONT_WRAP = 13, kA11a_Op_FONT_ROW = 14, kA11a_Op_WIDGET_NEW = 15,
	kA11a_Op_WIDGET_CB = 16, kA11a_Op_WIDGET_GET = 17, kA11a_Op_WIDGET_SET = 18, kA11a_Op_LL = 19, kA11a_Op_SCRIPT = 20,
	kA11a_Op_GAME = 21, kA11a_Op_ITEM_EVENT = 22, kA11a_Op_LOG = 24, kA11a_Op_WIDGET_NEW2 = 25, kA11a_Op_WIDGET_X = 26, kA11a_Op_JGAME = 27
};

enum
{
	kA11a_Cfg_FLOAT = 0, kA11a_Cfg_STRING = 1, kA11a_Cfg_V3 = 2, kA11a_Cfg_V2 = 3, kA11a_Cfg_COLOR = 4, kA11a_Cfg_INT = 5, kA11a_Cfg_BOOL = 6
};

enum
{
	kA11a_Set_ACTIVE = 0, kA11a_Set_DRAW_MOUSE = 1, kA11a_Set_MOUSE_MOVEMENT = 2, kA11a_Set_DRAW_FOCUS = 3, kA11a_Set_RESET_MOUSE_OVER = 4,
	kA11a_Set_GUI_FOCUS = 5, kA11a_Set_FOCUSED_WIDGET = 6, kA11a_Set_DEFAULT_NAV = 7, kA11a_Set_VIEWPORT = 8, kA11a_Set_GAMEPAD_PRESENT = 9,
	kA11a_Set_MOUSE_POS = 10, kA11a_Set_MOUSE_Z = 11, kA11a_Set_GET_FOCUSED = 12, kA11a_Set_POINTER_GFX = 13, kA11a_Set_CLIP_CURRENT = 14,
	kA11a_Set_CLIP_SET = 15, kA11a_Set_CLIP_CHILD = 16
};

enum
{
	kA11a_Gfx_IMAGE = 0, kA11a_Gfx_RECT = 1, kA11a_Gfx_TEXTURE = 2, kA11a_Gfx_FILE = 3, kA11a_Gfx_DESTROY = 0, kA11a_Gfx_SIZES = 1, kA11a_Gfx_COLOR = 2,
	kA11a_Gfx_ACTIVE_SIZE = 3
};

enum
{
	kA11a_Wd_IMAGE = 0, kA11a_Wd_DUMMY = 1, kA11a_Wd_GET_SIZE = 0, kA11a_Wd_GET_GLOBAL = 1, kA11a_Wd_GET_USER_VALUE = 2,
	kA11a_Wd_GET_COLOR_MUL = 3, kA11a_Wd_GET_IMAGE = 4, kA11a_Wd_SET_POSITION = 0, kA11a_Wd_SET_SIZE = 1, kA11a_Wd_SET_IMAGE = 2,
	kA11a_Wd_SET_USER_VALUE = 3, kA11a_Wd_SET_COLOR_MUL = 4, kA11a_Wd_SET_GLOBAL_UI = 5, kA11a_Wd_SET_NAV = 6, kA11a_Wd_NAV_FOCUS = 7
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
	kA11a_Wn_LABEL = 0, kA11a_Wn_IMAGE = 1, kA11a_Wn_DUMMY = 2, kA11a_Wn_FRAME = 3
};

enum
{
	kA11a_Wx_SET_VISIBLE = 0, kA11a_Wx_SET_ENABLED = 1, kA11a_Wx_SETUP_LABEL = 2, kA11a_Wx_IS_VISIBLE = 3, kA11a_Wx_HOVER_OR_FOCUS = 4,
	kA11a_Wx_GET_TEXT = 5, kA11a_Wx_GET_FONT_SIZE = 6, kA11a_Wx_GET_FONT = 7, kA11a_Wx_GET_ALIGN = 8, kA11a_Wx_DESTROY = 9, kA11a_Wx_FONT_LENGTH = 10
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
tWStringVec gvRows;

tString S(OhplA11aArgs *a, int i){ return tString((const char*)a->s[i].p, a->s[i].n); }
tWString W(OhplA11aArgs *a, int i){ return tWString((const wchar_t*)a->w[i].p, a->w[i].n); }
cVector3f V3(OhplA11aArgs *a, int i = 0){ return cVector3f(a->f[i], a->f[i+1], a->f[i+2]); }
cVector2f V2(OhplA11aArgs *a, int i = 0){ return cVector2f(a->f[i], a->f[i+1]); }
cColor Col(OhplA11aArgs *a, int i){ return cColor(a->f[i], a->f[i+1], a->f[i+2], a->f[i+3]); }
void RetS(OhplA11aArgs *a, const tString &asX){ gsRet = asX; a->rs = OhplA11aS(gsRet); }
void RetW(OhplA11aArgs *a, const tWString &asX){ gsRetW = asX; a->rw.p = (const unsigned int*)gsRetW.data(); a->rw.n = gsRetW.size(); }

eGuiMaterial Mat(int alMat)
{
	switch(alMat)
	{
	case 0: return eGuiMaterial_Alpha;
	case 1: return eGuiMaterial_Additive;
	case 2: return eGuiMaterial_Diffuse;
	case 3: return eGuiMaterial_Modulative;
	}
	return eGuiMaterial_LastEnum;
}

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
	cGui *pGui = gpBase->mpEngine->GetGui();
	switch(a->i[0])
	{
	case kA11a_Set_ACTIVE: gpSet->SetActive(a->i[1] != 0); break;
	case kA11a_Set_DRAW_MOUSE: gpSet->SetDrawMouse(a->i[1] != 0); break;
	case kA11a_Set_MOUSE_MOVEMENT: gpSet->SetMouseMovementEnabled(a->i[1] != 0); break;
	case kA11a_Set_DRAW_FOCUS: gpSet->SetDrawFocus(a->i[1] != 0); break;
	case kA11a_Set_RESET_MOUSE_OVER: gpSet->ResetMouseOver(); break;
	case kA11a_Set_GUI_FOCUS: pGui->SetFocus(a->i[1] ? gpSet : NULL); break;
	case kA11a_Set_FOCUSED_WIDGET: gpSet->SetFocusedWidget((iWidget*)a->p[0]); break;
	case kA11a_Set_DEFAULT_NAV: gpSet->SetDefaultFocusNavWidget((iWidget*)a->p[0]); break;
	case kA11a_Set_VIEWPORT: gpViewport->SetActive(a->i[1] != 0); gpViewport->SetVisible(a->i[1] != 0); break;
	case kA11a_Set_GAMEPAD_PRESENT: a->r = gpBase->mpInputHandler->IsGamepadPresent(); break;
	case kA11a_Set_MOUSE_POS: { cVector2f v = gpSet->GetMousePos(); a->f[0] = v.x; a->f[1] = v.y; } break;
	case kA11a_Set_MOUSE_Z: a->f[0] = gpSet->GetMouseZ(); break;
	case kA11a_Set_GET_FOCUSED: a->r = (uintptr_t)gpSet->GetFocusedWidget(); break;
	case kA11a_Set_POINTER_GFX: a->r = (uintptr_t)gpSet->GetCurrentPointer(); break;
	case kA11a_Set_CLIP_CURRENT: a->r = (uintptr_t)gpSet->GetCurrentClipRegion(); break;
	case kA11a_Set_CLIP_SET: gpSet->SetCurrentClipRegion((cGuiClipRegion*)a->p[0]); break;
	case kA11a_Set_CLIP_CHILD: a->r = (uintptr_t)((cGuiClipRegion*)a->p[0])->CreateChild(V3(a), V2(a, 3)); break;
	}
}

void OpGfx(OhplA11aArgs *a)
{
	cGui *pGui = gpBase->mpEngine->GetGui();
	cGuiGfxElement *pGfx = (cGuiGfxElement*)a->p[0];
	switch(a->i[0])
	{
	case kA11a_Gfx_DESTROY: if(pGfx) pGui->DestroyGfx(pGfx); break;
	case kA11a_Gfx_SIZES: { cVector2f v = pGfx->GetActiveSize(), u = pGfx->GetImageSize(); a->f[0] = v.x; a->f[1] = v.y; a->f[2] = u.x; a->f[3] = u.y; } break;
	case kA11a_Gfx_COLOR: pGfx->SetColor(Col(a, 0)); break;
	case kA11a_Gfx_ACTIVE_SIZE: pGfx->SetActiveSize(V2(a)); break;
	}
}

void OpGfxNew(OhplA11aArgs *a)
{
	cGui *pGui = gpBase->mpEngine->GetGui();
	switch(a->i[0])
	{
	case kA11a_Gfx_IMAGE: a->r = (uintptr_t)pGui->CreateGfxImage(S(a, 0), Mat(a->i[1])); break;
	case kA11a_Gfx_RECT: a->r = (uintptr_t)pGui->CreateGfxFilledRect(Col(a, 0), Mat(a->i[1])); break;
	case kA11a_Gfx_TEXTURE: a->r = (uintptr_t)pGui->CreateGfxTexture((iTexture*)a->p[0], false, Mat(a->i[1])); break;
	case kA11a_Gfx_FILE: a->r = (uintptr_t)pGui->CreateGfxTexture(S(a, 0), eGuiMaterial_Alpha, eTextureType_Rect); break;
	}
}

void OpWidgetNew(OhplA11aArgs *a)
{
	iWidget *pParent = (iWidget*)a->p[0];
	if(a->i[0] == kA11a_Wd_IMAGE)
		a->r = (uintptr_t)(iWidget*)gpSet->CreateWidgetImage(S(a, 0), V3(a), cVector2f(-1), eGuiMaterial_Alpha, false, pParent);
	else
		a->r = (uintptr_t)(iWidget*)gpSet->CreateWidgetDummy(V3(a));
}

eUIArrow Arrow(int alArrow)
{
	switch(alArrow)
	{
	case 0: return eUIArrow_Up;
	case 1: return eUIArrow_Right;
	case 2: return eUIArrow_Down;
	}
	return eUIArrow_Left;
}

void OpWidgetGet(OhplA11aArgs *a)
{
	iWidget *pW = (iWidget*)a->p[0];
	switch(a->i[0])
	{
	case kA11a_Wd_GET_SIZE: { cVector2f v = pW->GetSize(); a->f[0] = v.x; a->f[1] = v.y; } break;
	case kA11a_Wd_GET_GLOBAL: { cVector3f v = pW->GetGlobalPosition(); a->f[0] = v.x; a->f[1] = v.y; a->f[2] = v.z; } break;
	case kA11a_Wd_GET_USER_VALUE: a->r = (uintptr_t)(intptr_t)pW->GetUserValue(); break;
	case kA11a_Wd_GET_COLOR_MUL: { cColor c = pW->GetColorMul(); a->f[0] = c.r; a->f[1] = c.g; a->f[2] = c.b; a->f[3] = c.a; } break;
	case kA11a_Wd_GET_IMAGE: a->r = (uintptr_t)((cWidgetImage*)pW)->GetImage(); break;
	}
}

void OpWidgetSet(OhplA11aArgs *a)
{
	iWidget *pW = (iWidget*)a->p[0];
	switch(a->i[0])
	{
	case kA11a_Wd_SET_POSITION: pW->SetPosition(V3(a)); break;
	case kA11a_Wd_SET_SIZE: pW->SetSize(V2(a)); break;
	case kA11a_Wd_SET_IMAGE: ((cWidgetImage*)pW)->SetImage((cGuiGfxElement*)a->p[1]); break;
	case kA11a_Wd_SET_USER_VALUE: pW->SetUserValue(a->i[1]); break;
	case kA11a_Wd_SET_COLOR_MUL: pW->SetColorMul(Col(a, 0)); break;
	case kA11a_Wd_SET_GLOBAL_UI: pW->SetGlobalUIInputListener(a->i[1] != 0); break;
	case kA11a_Wd_SET_NAV: pW->SetFocusNavigation(Arrow(a->i[1]), (iWidget*)a->p[1]); break;
	case kA11a_Wd_NAV_FOCUS: pW->ProcessMessage(eGuiMessage_GetUINavFocus, cGuiMessageData()); break;
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

void OpWidgetNew2(OhplA11aArgs *a)
{
	iWidget *pParent = (iWidget*)a->p[0];
	switch(a->i[0])
	{
	case kA11a_Wn_LABEL: a->r = (uintptr_t)(iWidget*)gpSet->CreateWidgetLabel(V3(a), V2(a, 3), W(a, 0), pParent); break;
	case kA11a_Wn_IMAGE: a->r = (uintptr_t)(iWidget*)gpSet->CreateWidgetImage(S(a, 0), V3(a), V2(a, 3), eGuiMaterial_Alpha, false, pParent); break;
	case kA11a_Wn_DUMMY: a->r = (uintptr_t)(iWidget*)gpSet->CreateWidgetDummy(V3(a), pParent); break;
	case kA11a_Wn_FRAME: a->r = (uintptr_t)(iWidget*)gpSet->CreateWidgetFrame(V3(a), V2(a, 3), false, pParent); break;
	}
}

void OpWidgetX(OhplA11aArgs *a)
{
	iWidget *pW = (iWidget*)a->p[0];
	switch(a->i[0])
	{
	case kA11a_Wx_SET_VISIBLE: pW->SetVisible(a->i[1] != 0); break;
	case kA11a_Wx_SET_ENABLED: pW->SetEnabled(a->i[1] != 0); break;
	case kA11a_Wx_SETUP_LABEL:
		{
			cWidgetLabel *pLabel = static_cast<cWidgetLabel*>(pW);
			pLabel->SetDefaultFontColor(Col(a, 0));
			pLabel->SetDefaultFontSize(V2(a, 4));
			pLabel->SetTextAlign((eFontAlign)a->i[1]);
			pLabel->SetDefaultFontType((iFontData*)a->p[1]);
		}
		break;
	case kA11a_Wx_IS_VISIBLE: a->r = pW->IsVisible(); break;
	case kA11a_Wx_HOVER_OR_FOCUS: a->r = pW->GetMouseIsOver() || pW->HasFocus(); break;
	case kA11a_Wx_GET_TEXT: RetW(a, pW->GetText()); break;
	case kA11a_Wx_GET_FONT_SIZE: { cVector2f v = pW->GetDefaultFontSize(); a->f[0] = v.x; a->f[1] = v.y; } break;
	case kA11a_Wx_GET_FONT: a->r = (uintptr_t)pW->GetDefaultFontType(); break;
	case kA11a_Wx_GET_ALIGN: a->r = (uintptr_t)static_cast<cWidgetLabel*>(pW)->GetTextAlign(); break;
	case kA11a_Wx_DESTROY: gpSet->DestroyWidget(pW); break;
	case kA11a_Wx_FONT_LENGTH: a->f[0] = ((iFontData*)a->p[0])->GetLength(V2(a), W(a, 0).c_str()); break;
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
	case kA11a_Op_GFX_NEW: OpGfxNew(a); break;
	case kA11a_Op_GFX_OP: OpGfx(a); break;
	case kA11a_Op_DRAW_GFX:
		gpSet->DrawGfx((cGuiGfxElement*)a->p[0], V3(a), V2(a, 3), Col(a, 5), Mat(a->i[0]));
		break;
	case kA11a_Op_DRAW_FONT:
		gpSet->DrawFont(W(a, 0), (iFontData*)a->p[0], V3(a), V2(a, 3), Col(a, 5), (eFontAlign)a->i[0]);
		break;
	case kA11a_Op_FONT_LOAD:
		if(InJournal()) a->r = (uintptr_t)gpJrn->LoadFont(gpBase->mpMenuCfg->GetString("Journal", S(a, 0), ""));
		else a->r = (uintptr_t)gpInv->LoadFont(gpBase->mpMenuCfg->GetString("Inventory", S(a, 0), ""));
		break;
	case kA11a_Op_FONT_WRAP:
		gvRows.clear();
		((iFontData*)a->p[0])->GetWordWrapRows(a->f[0], a->f[1], cVector2f(a->f[2]), W(a, 0), &gvRows);
		a->i[0] = (int)gvRows.size();
		break;
	case kA11a_Op_FONT_ROW: RetW(a, gvRows[a->i[0]]); break;
	case kA11a_Op_WIDGET_NEW: OpWidgetNew(a); break;
	case kA11a_Op_WIDGET_CB: ((iWidget*)a->p[0])->AddCallback(Msg(a->i[0]), (void*)a->p[1], InJournal() ? &TrampJ : &Tramp); break;
	case kA11a_Op_WIDGET_GET: OpWidgetGet(a); break;
	case kA11a_Op_WIDGET_SET: OpWidgetSet(a); break;
	case kA11a_Op_LL: OpLowLevel(a); break;
	case kA11a_Op_SCRIPT: OpScript(a); break;
	case kA11a_Op_GAME: OpGame(a); break;
	case kA11a_Op_ITEM_EVENT: OpItemEvent(a); break;
	case kA11a_Op_WIDGET_NEW2: OpWidgetNew2(a); break;
	case kA11a_Op_WIDGET_X: OpWidgetX(a); break;
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

// OHPL_A11A_INVDUMP=<file>: item/type/combine/save-data sequence through the public inventory API
static void OhplA11aDumpItems(FILE *f, const char *asTag, cLuxInventory *apInv, bool abEquip=true)
{
	fprintf(f, "%s n=%d eq=%d dis=%d msg=%d picked=%d\n", asTag, apInv->GetItemNum(), abEquip ? apInv->GetItemIndex(apInv->GetEquippedHandItem()) : -2,
			apInv->GetDisabled(), apInv->GetMessageActive(), apInv->GetPickedItem()!=NULL);
	for(int i=0; i<apInv->GetItemNum(); ++i)
	{
		cLuxInventory_Item *p = apInv->GetItem(i);
		fprintf(f, "  [%d] name=%s type=%d sub=%s val=%s extra=%s img=%s cnt=%d amt=%g ne=%s de=%s gfx=%d\n", i, OhplInvName(p).c_str(), (int)OhplInvType(p),
				OhplInvSubType(p).c_str(), OhplInvStringVal(p).c_str(), OhplInvExtraVal(p).c_str(), OhplInvImageName(p).c_str(), OhplInvCount(p),
				OhplInvAmount(p), OhplInvNameEntry(p).c_str(), OhplInvDescEntry(p).c_str(), OhplInvImage(p)!=NULL);
	}
}

void OhplA11aInvDump(cLuxInventory *apInv)
{
	static bool bDone = false;
	const char *sPath = getenv("OHPL_A11A_INVDUMP");
	if(sPath==NULL || bDone) return;
	bDone = true;
	FILE *f = fopen(sPath, "w");
	if(f==NULL) return;

	static const char *vTypes[] = { "Puzzle","coins","NOTE","Diary","Lantern","Health","Sanity","LampOil","Tinderbox","HandObject" };
	for(size_t i=0; i<sizeof(vTypes)/sizeof(vTypes[0]); ++i)
	{
		eLuxItemType t = apInv->GetItemTypeFromString(vTypes[i]);
		tString sName;
		ohpl_inv_type_name(t, OhplA11aStrSink, &sName);
		fprintf(f, "type %s -> %d name=%s count=%d max=%d pickup=%d hasmax=%d\n", vTypes[i], (int)t, sName.c_str(), ohpl_inv_type_has_count(t), ohpl_inv_type_max_count(t),
				ohpl_inv_type_show_pickup(t), ohpl_inv_type_has_max(t));
	}

	apInv->Reset();
	bool bRemove;
	struct { const char *name; eLuxItemType type; const char *sub; float amount; const char *val; const char *extra; } vAdd[] = {
		{ "puz_a", eLuxItemType_Puzzle, "agrippa_head", 0, "", "" },
		{ "puz_b", eLuxItemType_Puzzle, "bone_saw", 1, "L02_Shipment", "x" },
		{ "hp_a", eLuxItemType_Health, "health", 25, "", "" },
		{ "hp_b", eLuxItemType_Health, "health", 25, "", "" },
		{ "san_a", eLuxItemType_Sanity, "sanity", 30, "", "" },
		{ "oil_a", eLuxItemType_LampOil, "oil", 20, "", "" },
		{ "oil_b", eLuxItemType_LampOil, "oil", 20, "", "" },
		{ "lantern", eLuxItemType_Lantern, "lantern", 0, "", "" },
		{ "hand_a", eLuxItemType_HandObject, "bucket_of_tar", 0, "", "" },
		{ "hand_b", eLuxItemType_HandObject, "bone_saw", 0, "", "" },
		{ "tb_a", eLuxItemType_Tinderbox, "tinderbox", 0, "", "" },
		{ "coins", eLuxItemType_Coins, "coins_large", 10, "", "" },
	};
	for(size_t i=0; i<sizeof(vAdd)/sizeof(vAdd[0]); ++i)
	{
		bRemove = false;
		cLuxInventory_Item *pItem = apInv->AddItem(vAdd[i].name, vAdd[i].type, vAdd[i].sub, "agrippa_head.tga", vAdd[i].amount, vAdd[i].val, vAdd[i].extra, &bRemove);
		fprintf(f, "add %s -> %d remove=%d idx=%d\n", vAdd[i].name, pItem!=NULL, bRemove, apInv->GetItemIndex(pItem));
		OhplA11aDumpItems(f, " state", apInv);
	}
	for(int i=0; i<100; ++i) apInv->AddItem("hp_x", eLuxItemType_Health, "health", "agrippa_head.tga", 25, "", "", &bRemove);
	OhplA11aDumpItems(f, "capped", apInv);
	fprintf(f, "has lantern=%d oil=%d tinder=%d note=%d\n", apInv->HasItemOfType(eLuxItemType_Lantern), apInv->HasItemOfType(eLuxItemType_LampOil),
			apInv->HasItemOfType(eLuxItemType_Tinderbox), apInv->HasItemOfType(eLuxItemType_Note));
	fprintf(f, "get puz_b=%d sub bone_saw=%d nothing=%d\n", apInv->GetItemIndex(apInv->GetItem("puz_b")),
			apInv->GetItemIndex(apInv->GetItemFromSubType("bone_saw")), apInv->GetItem("nothing")!=NULL);

	apInv->SetEquippedHandItem(apInv->GetItem("hand_b"));
	apInv->AddCombineCallback("cb1", "puz_a", "puz_b", "CombineFn", true);
	apInv->AddCombineCallback("cb2", "hp_a", "oil_a", "OtherFn", false);
	apInv->RemoveCombineCallback("cb2");
	apInv->SetDisabled(true);
	const char *vPairs[][2] = { {"puz_a","puz_b"}, {"puz_b","puz_a"}, {"hp_a","oil_a"}, {"puz_a","puz_a"} };
	for(size_t i=0; i<4; ++i)
	{
		int lComb = ohpl_inv_find_combine(OhplA11aS(vPairs[i][0]), OhplA11aS(vPairs[i][1]));
		tString sName, sFunc;
		if(lComb >= 0) { ohpl_inv_combine_str(lComb, 0, OhplA11aStrSink, &sName); ohpl_inv_combine_str(lComb, 3, OhplA11aStrSink, &sFunc); }
		fprintf(f, "comb %s,%s -> %s\n", vPairs[i][0], vPairs[i][1], lComb >= 0 ? (sName+","+sFunc).c_str() : "none");
	}

	cLuxInventory_SaveData save;
	save.FromInventory(apInv);
	fprintf(f, "save dis=%d eq=%d items=%d cbs=%d\n", save.mbDisabled, save.mlEquippedHandItem, (int)save.mvItems.Size(), (int)save.mvCombineCallbacks.Size());
	for(size_t i=0; i<save.mvItems.Size(); ++i)
	{
		cLuxInventory_Item_SaveData &s = save.mvItems[i];
		fprintf(f, "  s[%d] %s %d %s %s %s %s %g %d %s %s\n", (int)i, s.msName.c_str(), s.mlType, s.msSubType.c_str(), s.msVal.c_str(), s.msExtraVal.c_str(),
				s.msImageFile.c_str(), s.mfAmount, s.mlCount, s.msGameNameEntry.c_str(), s.msGameDescEntry.c_str());
	}
	for(size_t i=0; i<save.mvCombineCallbacks.Size(); ++i)
	{
		cLuxCombineItemsCallback &c = save.mvCombineCallbacks[i];
		fprintf(f, "  c[%d] %s %s %s %s %d\n", (int)i, c.msName.c_str(), c.msItemA.c_str(), c.msItemB.c_str(), c.msFunction.c_str(), c.mbAutoDestroy);
	}

	apInv->Reset();
	OhplA11aDumpItems(f, "reset", apInv);
	save.ToInventory(NULL, apInv);
	OhplA11aDumpItems(f, "restored", apInv, false);
	fprintf(f, "restored comb=%d\n", ohpl_inv_find_combine(OhplA11aS("puz_b"), OhplA11aS("puz_a"))>=0);
	apInv->RemoveItem(apInv->GetItem("hp_a"));
	apInv->RemoveItem("puz_a");
	apInv->RemoveItem("oil_a");
	apInv->RemoveItem("missing");
	apInv->RemoveItemOfType(eLuxItemType_Sanity);
	OhplA11aDumpItems(f, "removed", apInv, false);

	apInv->Reset();
	apInv->LoadScript();
	fclose(f);
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
