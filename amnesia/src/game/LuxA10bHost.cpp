#include "LuxA10bHost.h"

#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxInputHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxConfigHandler.h"
#include "LuxDebugHandler.h"

#include <cstring>
#include <exception>

static_assert(eGuiMaterial_Alpha == 1, "kOhplA10bMat");
static_assert(sizeof(wchar_t) == 4, "wide text crosses the ABI as u32");

static float AF(const OhplA10bArg &a)
{
	uint32_t u = (uint32_t)a.v;
	float f;
	memcpy(&f, &u, sizeof(f));
	return f;
}

static void *AP(const OhplA10bArg &a) { return (void*)(uintptr_t)a.v; }
static int AI(const OhplA10bArg &a) { return (int)(int64_t)a.v; }
static bool AB(const OhplA10bArg &a) { return a.v != 0; }
static tString AS(const OhplA10bArg &a) { return tString((const char*)(uintptr_t)a.v, a.n); }
static tWString AW(const OhplA10bArg &a) { return a.n ? tWString((const wchar_t*)(uintptr_t)a.v, a.n) : tWString(); }

static cVector3f V3(const OhplA10bArg *a, int i) { return cVector3f(AF(a[i]), AF(a[i+1]), AF(a[i+2])); }
static cVector2f V2(const OhplA10bArg *a, int i) { return cVector2f(AF(a[i]), AF(a[i+1])); }
static cColor C4(const OhplA10bArg *a, int i) { return cColor(AF(a[i]), AF(a[i+1]), AF(a[i+2]), AF(a[i+3])); }

static void SetP(OhplA10bCall *c, const void *p) { c->u[0] = (uint64_t)(uintptr_t)p; }

static void SetF(OhplA10bCall *c, int i, float f)
{
	uint32_t u;
	memcpy(&u, &f, sizeof(u));
	c->u[i] = u;
}

static void PutW(OhplA10bCall *c, const tWString &s)
{
	c->sink(c->sinkCtx, (const uint32_t*)s.data(), s.size());
}

static void PutS(OhplA10bCall *c, const tString &s)
{
	std::vector<uint32_t> v(s.size());
	for(size_t i=0; i<s.size(); ++i) v[i] = (unsigned char)s[i];
	c->sink(c->sinkCtx, v.data(), v.size());
}

static cConfigFile *Cfg(int alId)
{
	if(alId == 0) return gpBase->mpMenuCfg;
	if(alId == 1) return gpBase->mpDemoCfg;
	return gpBase->mpUserConfig;
}

static eGuiMessage ToMsg(int alMsg)
{
	switch(alMsg)
	{
	case kOhplA10bMsg_ButtonPressed: return eGuiMessage_ButtonPressed;
	case kOhplA10bMsg_UIArrow: return eGuiMessage_UIArrowPress;
	case kOhplA10bMsg_UIButton: return eGuiMessage_UIButtonPress;
	case kOhplA10bMsg_SliderMove: return eGuiMessage_SliderMove;
	case kOhplA10bMsg_OnUpdate: return eGuiMessage_OnUpdate;
	case kOhplA10bMsg_OnDraw: return eGuiMessage_OnDraw;
	case kOhplA10bMsg_MouseUp: return eGuiMessage_MouseUp;
	case kOhplA10bMsg_MouseEnter: return eGuiMessage_MouseEnter;
	default: return eGuiMessage_MouseLeave;
	}
}

static eLuxInputState ToInput(int alState)
{
	switch(alState)
	{
	case kOhplA10bInput_Game: return eLuxInputState_Game;
	case kOhplA10bInput_PreMenu: return eLuxInputState_PreMenu;
	case kOhplA10bInput_Credits: return eLuxInputState_Credits;
	case kOhplA10bInput_DemoEnd: return eLuxInputState_DemoEnd;
	case kOhplA10bInput_LoadScreen: return eLuxInputState_LoadScreen;
	default: return eLuxInputState_Null;
	}
}

static bool GuiCallback(void *apCtx, iWidget *apWidget, const cGuiMessageData &aData)
{
	cOhplA10bShell::Cb *pCb = static_cast<cOhplA10bShell::Cb*>(apCtx);
	return pCb->mpShell->OnCallback(pCb->mlTag, apWidget, aData);
}

static void HostOp(cOhplA10bShell *s, OhplA10bCall *c)
{
	const OhplA10bArg *a = c->a;
	cResources *pRes = gpBase->mpEngine->GetResources();

	switch(c->op)
	{
	case kOhplA10b_CALC_OFFSET:
	{
		cVector2f vSize, vOffset;
		LuxCalcGuiSetScreenOffset(V2(a,0), vSize, vOffset);
		SetF(c,0,vSize.x); SetF(c,1,vSize.y); SetF(c,2,vOffset.x); SetF(c,3,vOffset.y);
		break;
	}
	case kOhplA10b_VIEWPORT_NEW: SetP(c, gpBase->mpEngine->GetScene()->CreateViewport()); break;
	case kOhplA10b_VIEWPORT_ACTIVE: ((cViewport*)AP(a[0]))->SetActive(AB(a[1])); break;
	case kOhplA10b_VIEWPORT_VISIBLE: ((cViewport*)AP(a[0]))->SetVisible(AB(a[1])); break;
	case kOhplA10b_VIEWPORT_ADD_SET: ((cViewport*)AP(a[0]))->AddGuiSet((cGuiSet*)AP(a[1])); break;
	case kOhplA10b_TEX_2D: SetP(c, pRes->GetTextureManager()->Create2D(AS(a[0]), false, eTextureType_Rect)); break;
	case kOhplA10b_DRAW_FONT:
		((cGuiSet*)AP(a[0]))->DrawFont(AW(a[1]), (iFontData*)AP(a[2]), V3(a,3), V2(a,6), C4(a,8), (eFontAlign)AI(a[12]));
		break;
	case kOhplA10b_FONT_LOAD: SetP(c, s->mpOwner->LoadFont(AS(a[0]))); break;
	case kOhplA10b_FONT_WRAP:
	{
		tWStringVec vRows;
		((iFontData*)AP(a[0]))->GetWordWrapRows(AF(a[1]), AF(a[2]), V2(a,3), AW(a[5]), &vRows);
		for(size_t i=0; i<vRows.size(); ++i) PutW(c, vRows[i]);
		break;
	}
	case kOhplA10b_FONT_LEN: SetF(c,0, ((iFontData*)AP(a[0]))->GetLength(V2(a,1), AW(a[3]).c_str())); break;
	case kOhplA10b_FONT_HEIGHT: SetF(c,0, ((iFontData*)AP(a[0]))->GetHeight()); break;
	case kOhplA10b_TRANSLATE: PutW(c, kTranslate(AS(a[0]), AS(a[1]))); break;
	case kOhplA10b_CFG_STRING: PutS(c, Cfg(AI(a[0]))->GetString(AS(a[1]), AS(a[2]), AS(a[3]))); break;
	case kOhplA10b_CFG_FLOAT: SetF(c,0, Cfg(AI(a[0]))->GetFloat(AS(a[1]), AS(a[2]), AF(a[3]))); break;
	case kOhplA10b_CFG_VEC2:
	{
		cVector2f v = Cfg(AI(a[0]))->GetVector2f(AS(a[1]), AS(a[2]), V2(a,3));
		SetF(c,0,v.x); SetF(c,1,v.y);
		break;
	}
	case kOhplA10b_CFG_COLOR:
	{
		cColor v = Cfg(AI(a[0]))->GetColor(AS(a[1]), AS(a[2]), C4(a,3));
		SetF(c,0,v.r); SetF(c,1,v.g); SetF(c,2,v.b); SetF(c,3,v.a);
		break;
	}
	case kOhplA10b_CFG_BOOL: c->u[0] = Cfg(AI(a[0]))->GetBool(AS(a[1]), AS(a[2]), AB(a[3])); break;
	case kOhplA10b_CFG_SET_BOOL: Cfg(AI(a[0]))->SetBool(AS(a[1]), AS(a[2]), AB(a[3])); break;
	case kOhplA10b_WIDGET_CALLBACK:
	{
		s->mCbs.push_back(cOhplA10bShell::Cb());
		cOhplA10bShell::Cb &cb = s->mCbs.back();
		cb.mpShell = s;
		cb.mlTag = AI(a[2]);
		((iWidget*)AP(a[0]))->AddCallback(ToMsg(AI(a[1])), &cb, &GuiCallback);
		break;
	}
	case kOhplA10b_MUSIC_PLAY:
		gpBase->mpEngine->GetSound()->GetMusicHandler()->Play(AS(a[0]), AF(a[1]), AF(a[2]), AB(a[3]), AB(a[4]));
		break;
	case kOhplA10b_MUSIC_STOP: gpBase->mpEngine->GetSound()->GetMusicHandler()->Stop(AF(a[0])); break;
	case kOhplA10b_MUSIC_PAUSE: gpBase->mpEngine->GetSound()->GetMusicHandler()->Pause(); break;
	case kOhplA10b_MUSIC_RESUME: gpBase->mpEngine->GetSound()->GetMusicHandler()->Resume(); break;
	case kOhplA10b_UPDATER_CONTAINER: gpBase->mpEngine->GetUpdater()->SetContainer(AS(a[0])); break;
	case kOhplA10b_UPDATER_RESET_ALL: gpBase->mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset); break;
	case kOhplA10b_ENGINE_EXIT: gpBase->mpEngine->Exit(); break;
	case kOhplA10b_INPUT_STATE: gpBase->mpInputHandler->ChangeState(ToInput(AI(a[0]))); break;
	case kOhplA10b_LOCK_INPUT: gpBase->mpEngine->GetInput()->GetLowLevel()->LockInput(AB(a[0])); break;
	case kOhplA10b_RELATIVE_MOUSE: gpBase->mpEngine->GetInput()->GetLowLevel()->RelativeMouse(AB(a[0])); break;
	case kOhplA10b_GAMMA_GET: SetF(c,0, gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection()); break;
	case kOhplA10b_GAMMA_SET: gpBase->mpEngine->GetGraphics()->GetLowLevel()->SetGammaCorrection(AF(a[0])); break;
	case kOhplA10b_BASE_FLAG:
		switch(AI(a[0]))
		{
		case kOhplA10bFlag_Fullscreen: c->u[0] = gpBase->mpConfigHandler->mbFullscreen; break;
		case kOhplA10bFlag_FirstStart: c->u[0] = gpBase->mpConfigHandler->mbFirstStart; break;
		case kOhplA10bFlag_ShowMenu: c->u[0] = gpBase->mbShowMenu; break;
		case kOhplA10bFlag_PTest: c->u[0] = gpBase->mbPTestActivated; break;
		case kOhplA10bFlag_DebugWindow: c->u[0] = gpBase->mpDebugHandler->GetDebugWindowActive(); break;
		case kOhplA10bFlag_MapLoaded: c->u[0] = gpBase->mpMapHandler->MapIsLoaded(); break;
		}
		break;
	case kOhplA10b_PREMENU_PATH:
	{
		tWString sConfigPath = gpBase->msPreMenuConfigPath;
#ifdef USERDIR_RESOURCES
		if (gpBase->msUserResourceDir.length()
			&& cPlatform::FileExists(gpBase->msUserResourceDir + sConfigPath)) {
			sConfigPath = gpBase->msUserResourceDir + sConfigPath;
		}
#endif
		PutW(c, sConfigPath);
		break;
	}
	case kOhplA10b_XML_OPEN:
	{
		iXmlDocument *pDoc = pRes->GetLowLevel()->CreateXmlDocument();
		if(pDoc->CreateFromFile(AW(a[0])))
			SetP(c, static_cast<cXmlElement*>(pDoc));
		else
			pRes->DestroyXmlDocument(pDoc);
		break;
	}
	case kOhplA10b_XML_CLOSE: pRes->DestroyXmlDocument(static_cast<iXmlDocument*>((cXmlElement*)AP(a[0]))); break;
	case kOhplA10b_XML_CHILD:
	{
		cXmlNodeListIterator it = ((cXmlElement*)AP(a[0]))->GetChildIterator();
		int lIdx = AI(a[1]);
		iXmlNode *pNode = NULL;
		for(int i=0; i<=lIdx && it.HasNext(); ++i) pNode = it.Next();
		if(pNode) SetP(c, pNode->ToElement());
		break;
	}
	case kOhplA10b_XML_CHILD_COUNT:
	{
		cXmlNodeListIterator it = ((cXmlElement*)AP(a[0]))->GetChildIterator();
		int lNum = 0;
		while(it.HasNext()) { it.Next(); ++lNum; }
		c->u[0] = (uint64_t)(int64_t)lNum;
		break;
	}
	case kOhplA10b_XML_STR: PutS(c, ((cXmlElement*)AP(a[0]))->GetAttributeString(AS(a[1]), AS(a[2]))); break;
	case kOhplA10b_XML_FLOAT: SetF(c,0, ((cXmlElement*)AP(a[0]))->GetAttributeFloat(AS(a[1]), AF(a[2]))); break;
	case kOhplA10b_XML_BOOL: c->u[0] = ((cXmlElement*)AP(a[0]))->GetAttributeBool(AS(a[1]), AB(a[2])); break;
	case kOhplA10b_XML_COLOR:
	{
		cColor v = ((cXmlElement*)AP(a[0]))->GetAttributeColor(AS(a[1]), C4(a,2));
		SetF(c,0,v.r); SetF(c,1,v.g); SetF(c,2,v.b); SetF(c,3,v.a);
		break;
	}
	case kOhplA10b_XML_VEC2:
	{
		cVector2f v = ((cXmlElement*)AP(a[0]))->GetAttributeVector2f(AS(a[1]), V2(a,2));
		SetF(c,0,v.x); SetF(c,1,v.y);
		break;
	}
	case kOhplA10b_XML_VEC3:
	{
		cVector3f v = ((cXmlElement*)AP(a[0]))->GetAttributeVector3f(AS(a[1]));
		SetF(c,0,v.x); SetF(c,1,v.y); SetF(c,2,v.z);
		break;
	}
	case kOhplA10b_BASE_CREATE_PROFILE: gpBase->CreateProfile(gpBase->msDefaultProfileName); break;
	case kOhplA10b_BASE_SET_PROFILE: gpBase->SetProfile(gpBase->msDefaultProfileName); break;
	case kOhplA10b_BASE_INIT_USER_CONFIG: c->u[0] = gpBase->InitUserConfig(); break;
	case kOhplA10b_BASE_EXIT: gpBase->Exit(); break;
	case kOhplA10b_BASE_START_GAME: gpBase->StartGame("", "", ""); break;
	case kOhplA10b_HELP_SET: SetP(c, gpBase->mpHelpFuncs->GetSet()); break;
	case kOhplA10b_DRAW_SET_TO_SCREEN: gpBase->mpHelpFuncs->DrawSetToScreen(); break;
	case kOhplA10b_STRING_DURATION: SetF(c,0, gpBase->mpHelpFuncs->GetStringDuration(AW(a[0]))); break;
	case kOhplA10b_PLAY_GUI_SOUND: c->u[0] = gpBase->mpHelpFuncs->PlayGuiSoundData(AS(a[0]), eSoundEntryType_Gui); break;
	case kOhplA10b_OPEN_BROWSER: cPlatform::OpenBrowserWindow(cString::To16Char(AS(a[0]))); break;
	case kOhplA10b_DATE:
	{
		cDate d = cPlatform::GetDate();
		c->u[0] = (uint64_t)(int64_t)d.year;
		c->u[1] = (uint64_t)(int64_t)d.month;
		c->u[2] = (uint64_t)(int64_t)d.month_day;
		c->u[3] = (uint64_t)(int64_t)d.hours;
		c->u[4] = (uint64_t)(int64_t)d.minutes;
		c->u[5] = (uint64_t)(int64_t)d.seconds;
		break;
	}
	case kOhplA10b_PROFILE_NAME: PutW(c, gpBase->msProfileName); break;
	case kOhplA10b_STATE_PATH: PutW(c, gpBase->msBaseStatePath); break;
	case kOhplA10b_LOG_ERROR: Error("%s", AS(a[0]).c_str()); break;
	case kOhplA10b_MAP_NAME:
	{
		cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
		PutS(c, pMap ? pMap->GetName() : "NoMap");
		break;
	}
	case kOhplA10b_PLAYER_VITALS:
	{
		cLuxPlayer *pPlayer = gpBase->mpPlayer;
		SetF(c,0, pPlayer->GetHealth());
		SetF(c,1, pPlayer->GetSanity());
		SetF(c,2, pPlayer->GetLampOil());
		c->u[3] = (uint64_t)(int64_t)pPlayer->GetTinderboxes();
		c->u[4] = (uint64_t)(int64_t)pPlayer->GetCoins();
		break;
	}
	}
}

static void HostCall(void *apCtx, OhplA10bCall *apCall)
{
	try
	{
		HostOp(static_cast<cOhplA10bShell*>(apCtx), apCall);
	}
	catch(...)
	{
		std::terminate();
	}
}

cOhplA10bShell::cOhplA10bShell(iLuxUpdateable *apOwner, int alKind)
{
	mHost.ctx = this;
	mHost.call = &HostCall;
	mH = 0;
	mpOwner = apOwner;
	mlKind = alKind;
}

bool cOhplA10bShell::OnCallback(int alTag, iWidget *apWidget, const cGuiMessageData &aData)
{
	switch(mlKind)
	{
	case eOhplA10bKind_PreMenu:
		return ohpl_a10b_pm_cb(&mHost, mH, alTag, (alTag == 2 || alTag == 3) ? aData.mlVal : 0);
	case eOhplA10bKind_DemoEnd:
		return ohpl_a10b_de_cb(&mHost, mH, alTag, (size_t)apWidget, alTag == 3 ? aData.mfVal : 0.0f);
	}
	return true;
}
