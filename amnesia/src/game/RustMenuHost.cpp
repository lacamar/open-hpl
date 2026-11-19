#include "RustMenuHost.h"

#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxInputHandler.h"
#include "LuxHintHandler.h"
#include "LuxConfigHandler.h"
#include "LuxDebugHandler.h"
#include "LuxPostEffects.h"
#include "LuxMessageHandler.h"
#include "LuxPlayerHelpers.h"
#include "LuxPlayer.h"
#include "LuxMainMenu.h"
#include "LuxSaveHandler.h"
#include "LuxLoadScreenHandler.h"
#include "LuxAchievementHandler.h"
#include "LuxDemoEnd.h"

#include <cstdio>
#include <cstdlib>
#include <map>

static_assert(sizeof(wchar_t) == 4, "tWString holds UTF-32");
static_assert(eKey_Escape == 5 && eGamepadButton_B == 1 && eGuiMouseButton_Left == 1, "menu host constants");
static_assert(eUIButton_Primary == 1 && eUIButton_Secondary == 2 && eUIButton_PrevPage == 4 && eUIButton_NextPage == 8, "menu host constants");
static_assert(eInputDeviceType_LastEnum == 3 && eGamepadInputType_LastEnum == 4, "menu host constants");
static_assert(eLuxActionCategory_Movement == 1 && eLuxActionCategory_LastEnum == 4, "menu host constants");
static_assert(eLuxMainMenuWindow_LastEnum == 7 && eLuxMainMenuWindow_Options == 1 && eLuxMainMenuWindow_KeyConfig == 2, "menu host constants");

enum eOhplA10aOp
{
	eWindow = 1, eButton, eLabel, eCheckBox, eCombo, eSlider, eTabFrame, eImage, eGroup, eDummy, eFrame,
	eSize = 20, eLocalPos, eSetSize, eSetPos, eSetText, eGetText, eSetVisible, eSetEnabled, eIsVisible, eSetUserData,
	eGetUserData, eSetUserValue, eGetUserValue, eSetNav, eSetGlobalUi, eSetStatic, eFontColor, eSetFontColor,
	eFontSize, eSetFontSize, eTextLength, eSetWordWrap, eSetClip, eSetScrollSpeed, eSetDrawBg, eSetBgColor,
	eSetAlign, eSetAutosize, eSetScrollOffset, eParent, eGlobalPos, eAddCallback, eSetChecked, eIsChecked,
	eComboAdd, eComboClear, eComboSelect, eComboSelected, eComboNum, eSliderMax, eSliderBar, eSliderSet,
	eSliderGet, eSliderGetMax, eTabAdd, eTabNum, eTabGet, eTabTop, eTabLabel, eTabFrameOf, eFrameOf,
	eFrameScrollTo, eFrameScroll, eFrameDrawBg, eMouseOver,
	ePopupActive = 80, eSetDefaultNav, eSetFocused, ePushAttention, ePopAttention, ePushFocused, ePopFocused,
	ePushDefaultNav, eSetAttention, eSetMouseMove, eSetDrawMouse, eMsgBox,
	eSetWindowActive = 100, eTranslate, eOptRead, eOptApply, eSaveConfig, eNeedsRestart, eRestartWarning,
	eSetGamma, eSetMouseSens, eSetPadSens, eSetVolume, ePlaySanity, eGamepadPresent,
	eShowCommentaryCfg = 114, eLangFiles, eListStr, eVideoModes, eVideoMode, eDisplayName, eSndDevices, eSndDevice,
	eActions = 140, eAction, eDefaultInputs, eDefaultInput, eInitialInputs, eActionClear, eActionAdd, eActionName,
	eGuiMouse, eAxisRange,
	eSkinNew = 200, eSetNew, eIsChinese, eSkinFontMul, eSetActive, eVpNew, eVpActive, eVpVisible, eVpAddSet, eVpWorld,
	eVpCamera, eCfg, eGfxRect, eGfxImage, eGfxSize, eGfxTex, eGfxDestroy, eDrawGfx, eSetVSize, eWidgetDestroy,
	eSetDestroyAll, eWidgetName, eHasFocus, eIsEnabled, eNavGet, eFontType, eFontLen, eFontLoad, eSetDrawFocus,
	eGetDrawFocus, eGetFocused, ePopupOpts, eProcessMouseDown, eWorldDestroy, eCameraDestroy, eWorldLoad, eCameraNew,
	eCameraFov, eCameraAspect, eCameraPlace, eScreenSize, eTexNew, eTexDestroy, eFbNew, eFbDestroy, eProgNew,
	eProgBind, eGfxState, eGfxOrtho, eGfxCopyScreen, eGfxSetFb, eGfxSetTex, eGfxQuad, eGfxFlush, eLabelNamed,
	eSetGlobalPos = 256, eGuiFocus, eCenter,
	eMapLoaded = 260, eMusic, ePauseMap, eProgLog, eInputState, eLockInput, eRelativeMouse, eFlag, eSetHard, eTinder,
	eSave, eHasMap, eDestroyMap, eUpdater, eCustomClear, eProfile, eStartGame, eDrawMenuScreen, eZoomSound, eMapName,
	eUnlockNope, eLogError,
};

static const cGuiMessageData* gpCbData;

static cGui* Gui() { return gpBase->mpEngine->GetGui(); }
static iLowLevelGraphics* LowGfx() { return gpBase->mpEngine->GetGraphics()->GetLowLevel(); }

static tWString gOut;
static std::vector<tWString> gLists[6];
static tVideoModeVec gModes;
static tSoundDeviceVec gDevices;
static iSoundDeviceIdentifier* gCurDevice;
static tLuxActionVec gActions;
static tLuxInputVec gDefaultInputs;

static tWString Wide(const OhplA10aArgs& a, int i) { return tWString((const wchar_t*)a.s[i], a.n[i]); }
static tString Narrow(const OhplA10aArgs& a, int i)
{
	tString s;
	for(size_t k = 0; k < a.n[i]; ++k) s += (char)a.s[i][k];
	return s;
}
static tWString Widen(const tString& s) { tWString w; for(size_t k = 0; k < s.size(); ++k) w += (wchar_t)(unsigned char)s[k]; return w; }
static void Out(OhplA10aArgs& a, const tWString& s) { gOut = s; a.s[0] = (const uint32_t*)gOut.data(); a.n[0] = gOut.size(); }
static cVector3f Pos(const OhplA10aArgs& a) { return cVector3f(a.f[0], a.f[1], a.f[2]); }
static cVector2f Size(const OhplA10aArgs& a) { return cVector2f(a.f[3], a.f[4]); }
static void SetStr(OhplA10aStr& d, const tString& s) { d.n = (uint32_t)std::min(s.size(), sizeof(d.b)); memcpy(d.b, s.data(), d.n); }
static tString GetStr(const OhplA10aStr& d) { return tString(d.b, std::min<size_t>(d.n, sizeof(d.b))); }

static const eGuiMessage kMsgs[] = {
	eGuiMessage_OnUpdate, eGuiMessage_ButtonPressed, eGuiMessage_UIButtonPress, eGuiMessage_SelectionChange,
	eGuiMessage_CheckChange, eGuiMessage_SliderMove, eGuiMessage_MouseMove, eGuiMessage_GetUINavFocus,
	eGuiMessage_LoseUINavFocus, eGuiMessage_MouseEnter, eGuiMessage_MouseLeave, eGuiMessage_MouseDown,
	eGuiMessage_TextChange, eGuiMessage_KeyPress, eGuiMessage_UIArrowPress, eGuiMessage_GamepadInput, eGuiMessage_OnDraw,
};
static const eUIArrow kArrows[] = { eUIArrow_Up, eUIArrow_Right, eUIArrow_Down, eUIArrow_Left };

static bool Trampoline(void* apToken, iWidget* apWidget, const cGuiMessageData& aData)
{
	OhplA10aCb cb = {};
	for(int i = 0; i < (int)(sizeof(kMsgs) / sizeof(kMsgs[0])); ++i)
		if(kMsgs[i] == aData.mMessage) cb.msg = i;
	switch(aData.mMessage)
	{
	case eGuiMessage_OnUpdate: cb.fval = aData.mfVal; break;
	case eGuiMessage_KeyPress: cb.key = aData.mKeyPress.mKey; break;
	case eGuiMessage_GamepadInput:
		cb.padValue = aData.mGamepadInputData.mfInputValue;
		cb.padType = (int)aData.mGamepadInputData.mInputType;
		cb.padId = aData.mGamepadInputData.mlInputId;
		break;
	default: cb.lval = aData.mlVal; break;
	}
	const cGuiMessageData* pPrev = gpCbData;
	gpCbData = &aData;
	bool bHandled = ohpl_a10a_cb((size_t)apToken, (size_t)apWidget, &cb) != 0;
	gpCbData = pPrev;
	return bHandled;
}

static void OptRead(OhplA10aOpts& o)
{
	cLuxConfigHandler* pCfg = gpBase->mpConfigHandler;
	o.showHints = gpBase->mpHintHandler->IsActive();
	o.showDeathHints = gpBase->mpPlayer->GetHelperDeath()->ShowHint();
	o.showSubtitles = gpBase->mpMessageHandler->ShowSubtitles();
	o.showEffectSubtitles = gpBase->mpMessageHandler->ShowEffectSubtitles();
	o.showCrosshair = gpBase->mpPlayer->GetShowCrosshair();
	o.focusIconStyle = gpBase->mpPlayer->GetFocusIconStyle();
	o.showCommentary = gpBase->mpMapHandler->GetShowCommentary();
	SetStr(o.lang, pCfg->LangFile());
	o.resX = pCfg->mvScreenSize.x;
	o.resY = pCfg->mvScreenSize.y;
	o.display = pCfg->mlDisplay;
	o.fullscreen = pCfg->mbFullscreen;
	o.vsync = pCfg->mbVSync;
	o.showFps = gpBase->mpDebugHandler->GetShowFPS();
	SetStr(o.renderer, pCfg->Renderer());
	SetStr(o.color, pCfg->ColorOutput());
	o.paperWhite = pCfg->mfPaperWhite;
	o.peakNits = pCfg->mfPeakNits;
	o.guiScale = pCfg->mlGuiScale;
	o.texQuality = pCfg->mlTextureQuality;
	o.texFilter = pCfg->mlTextureFilter;
	o.aniso = pCfg->mfTextureAnisotropy;
	o.edgeSmooth = pCfg->mbEdgeSmooth;
	o.shadows = pCfg->mbShadowsActive;
	o.shadowQuality = pCfg->mlShadowQuality;
	o.shadowRes = pCfg->mlShadowRes;
	o.parallax = pCfg->mbParallaxEnabled;
	o.worldReflection = pCfg->mbWorldReflection;
	o.refraction = pCfg->mbRefraction;
	o.ssao = pCfg->mbSSAOActive;
	o.ssaoSamples = pCfg->mlSSAOSamples;
	o.ssaoRes = pCfg->mlSSAOResolution;
	cLuxMapHandler* pMapHdlr = gpBase->mpMapHandler;
	o.bloom = pMapHdlr->GetPostEffect_Bloom()->IsDisabled() == false;
	o.imageTrail = pMapHdlr->GetPostEffect_ImageTrail()->IsDisabled() == false;
	o.sepia = pMapHdlr->GetPostEffect_Sepia()->IsDisabled() == false;
	o.radialBlur = pMapHdlr->GetPostEffect_RadialBlur()->IsDisabled() == false;
	o.insanity = gpBase->mpPostEffectHandler->GetInsanity()->IsDisabled() == false;
	o.gamma = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection();
	o.invertMouse = gpBase->mpInputHandler->GetInvertMouse();
	o.smoothMouse = gpBase->mpInputHandler->GetSmoothMouse();
	o.mouseSens = gpBase->mpInputHandler->GetMouseSensitivity();
	o.padInvert = gpBase->mpInputHandler->GetInvertGamepadLook();
	o.padSens = gpBase->mpInputHandler->GetGamepadLookSensitivity();
	o.volume = gpBase->mpEngine->GetSound()->GetLowLevel()->GetVolume();
	o.soundDev = pCfg->mlSoundDevID;
}

static void OptApply(const OhplA10aOpts& o, bool abCommentary)
{
	cLuxConfigHandler* pCfg = gpBase->mpConfigHandler;
	pCfg->SetLangFile(GetStr(o.lang));
	gpBase->mpMessageHandler->SetShowSubtitles(o.showSubtitles);
	gpBase->mpMessageHandler->SetShowEffectSubtitles(o.showEffectSubtitles);
	gpBase->mpHintHandler->SetActive(o.showHints);
	gpBase->mpPlayer->GetHelperDeath()->SetShowHint(o.showDeathHints);
	if(abCommentary) gpBase->mpMapHandler->SetShowCommentary(o.showCommentary);
	gpBase->mpPlayer->SetShowCrosshair(o.showCrosshair);
	gpBase->mpPlayer->SetFocusIconStyle((eLuxFocusIconStyle)o.focusIconStyle);

	cGraphics* pGfx = gpBase->mpEngine->GetGraphics();
	cMaterialManager* pMatMgr = gpBase->mpEngine->GetResources()->GetMaterialManager();
	pCfg->mvScreenSize = cVector2l(o.resX, o.resY);
	pCfg->mlDisplay = o.display;
	pCfg->mbFullscreen = o.fullscreen;
	pCfg->mbVSync = o.vsync;
	pGfx->GetLowLevel()->SetVsyncActive(pCfg->mbVSync, pCfg->mbAdaptiveVSync);
	pGfx->GetLowLevel()->SetGammaCorrection(o.gamma);
	gpBase->mpDebugHandler->SetShowFPS(o.showFps);
	pCfg->SetRenderer(GetStr(o.renderer));
	pCfg->SetColorOutput(GetStr(o.color));
	pCfg->mfPaperWhite = o.paperWhite;
	pCfg->mfPeakNits = o.peakNits;
	pGfx->GetLowLevel()->SetColorOutput(pCfg->ColorOutput(), pCfg->mfPaperWhite, pCfg->mfPeakNits);
	pCfg->mlGuiScale = o.guiScale;
	pCfg->mbParallaxEnabled = o.parallax;
	pCfg->mlTextureQuality = o.texQuality;
	pCfg->mlTextureFilter = o.texFilter;
	pCfg->mfTextureAnisotropy = o.aniso;
	pMatMgr->SetTextureAnisotropy(pCfg->mfTextureAnisotropy);
	pMatMgr->SetTextureFilter((eTextureFilter)pCfg->mlTextureFilter);
	pCfg->mbShadowsActive = o.shadows;
	pCfg->mlShadowQuality = o.shadowQuality;
	pCfg->mlShadowRes = o.shadowRes;
	pCfg->mbWorldReflection = o.worldReflection;
	pCfg->mbRefraction = o.refraction;
	gpBase->mpMapHandler->UpdateViewportRenderProperties();
	pCfg->mbEdgeSmooth = o.edgeSmooth;

	cLuxMapHandler* pMapHdlr = gpBase->mpMapHandler;
	pMapHdlr->GetPostEffect_Bloom()->SetDisabled(o.bloom == 0);
	pMapHdlr->GetPostEffect_ImageTrail()->SetDisabled(o.imageTrail == 0);
	pMapHdlr->GetPostEffect_Sepia()->SetDisabled(o.sepia == 0);
	pMapHdlr->GetPostEffect_RadialBlur()->SetDisabled(o.radialBlur == 0);
	gpBase->mpPostEffectHandler->GetInsanity()->SetDisabled(o.insanity == 0);

	pCfg->mbSSAOActive = o.ssao;
	pCfg->mlSSAOSamples = o.ssaoSamples;
	pCfg->mlSSAOResolution = o.ssaoRes;

	gpBase->mpInputHandler->SetInvertMouse(o.invertMouse);
	gpBase->mpInputHandler->SetSmoothMouse(o.smoothMouse);
	gpBase->mpInputHandler->SetMouseSensitivity(o.mouseSens);
	gpBase->mpInputHandler->SetGamepadLookSensitivity(o.padSens);
	gpBase->mpInputHandler->SetInvertGamepadLook(o.padInvert);
}

static size_t Host(int alOp, OhplA10aArgs* apA)
{
	OhplA10aArgs& a = *apA;
	iWidget* pW = (iWidget*)a.p[0];
	cGuiSet* pSet = (cGuiSet*)a.p[0];
	iWidget* pParent = (iWidget*)a.p[1];
	switch(alOp)
	{
	case eLabelNamed: return (size_t)pSet->CreateWidgetLabel(Pos(a), 0, Wide(a, 0), NULL, Narrow(a, 1));
	case eCombo: return (size_t)pSet->CreateWidgetComboBox(Pos(a), Size(a), _W(""), pParent);
	case eTabFrame: return (size_t)pSet->CreateWidgetTabFrame(Pos(a), Size(a), _W(""), pParent, false, true);
	case eImage:
		return (size_t)pSet->CreateWidgetImage(Narrow(a, 0), Pos(a), Size(a), eGuiMaterial_Alpha, false, pParent, Narrow(a, 1));

	case eSetFontSize: pW->SetDefaultFontSize(cVector2f(a.f[0], a.f[1])); return 0;
	case eTextLength: a.f[0] = pW->GetDefaultFontType()->GetLength(pW->GetDefaultFontSize(), pW->GetText().c_str()); return 0;
	case eAddCallback: pW->AddCallback(kMsgs[a.i[0]], (void*)a.p[1], &Trampoline); return 0;
	case eComboAdd: static_cast<cWidgetComboBox*>(pW)->AddItem(Wide(a, 0)); return 0;
	case eComboClear: static_cast<cWidgetComboBox*>(pW)->ClearItems(); return 0;
	case eComboSelect: static_cast<cWidgetComboBox*>(pW)->SetSelectedItem(a.i[0], a.i[1] != 0, a.i[2] != 0); return 0;
	case eComboSelected: return (size_t)(unsigned)static_cast<cWidgetComboBox*>(pW)->GetSelectedItem();
	case eComboNum: return (size_t)(unsigned)static_cast<cWidgetComboBox*>(pW)->GetItemNum();
	case eTabAdd: return (size_t)static_cast<cWidgetTabFrame*>(pW)->AddTab(Wide(a, 0));
	case eTabNum: return (size_t)(unsigned)static_cast<cWidgetTabFrame*>(pW)->GetTabNum();
	case eTabGet: return (size_t)static_cast<cWidgetTabFrame*>(pW)->GetTab(a.i[0]);
	case eTabTop: return (size_t)static_cast<cWidgetTabFrame*>(pW)->GetTabOnTop();
	case eTabLabel: return (size_t)static_cast<cWidgetTab*>(pW)->GetTabLabel();
	case eTabFrameOf: return (size_t)static_cast<cWidgetTab*>(pW)->GetParentTabFrame();
	case eFrameOf: return (size_t)dynamic_cast<cWidgetFrame*>(pW);
	case eFrameScrollTo: static_cast<cWidgetFrame*>(pW)->ScrollToPosition(cVector2f(a.f[0], a.f[1])); return 0;

	case eMsgBox: return (size_t)pSet->CreatePopUpMessageBox(Wide(a, 0), Wide(a, 1), Wide(a, 2), Wide(a, 3), (void*)a.p[1], &Trampoline);

	case eTranslate: Out(a, kTranslate(Narrow(a, 0), Narrow(a, 1))); return 0;
	case eOptRead: OptRead(*(OhplA10aOpts*)a.p[0]); return 0;
	case eOptApply: OptApply(*(const OhplA10aOpts*)a.p[0], a.i[0] != 0); return 0;
	case eSaveConfig: gpBase->SaveConfig(); return 0;
	case eNeedsRestart: gpBase->mpConfigHandler->SetGameNeedsRestart(); return 0;
	case eRestartWarning: return gpBase->mpConfigHandler->ShowRestartWarning(pSet, (void*)a.p[1], &Trampoline);
	case eSetGamma: gpBase->mpEngine->GetGraphics()->GetLowLevel()->SetGammaCorrection(a.f[0]); return 0;
	case eSetMouseSens: gpBase->mpInputHandler->SetMouseSensitivity(a.f[0]); return 0;
	case eSetPadSens: gpBase->mpInputHandler->SetGamepadLookSensitivity(a.f[0]); return 0;
	case eSetVolume: gpBase->mpEngine->GetSound()->GetLowLevel()->SetVolume(a.f[0]); return 0;
	case ePlaySanity: gpBase->mpEngine->GetSound()->GetSoundHandler()->PlayGui("ui_use_sanity.ogg", false, 1); return 0;
	case eGamepadPresent: return gpBase->mpInputHandler->IsGamepadPresent();
	case eShowCommentaryCfg: return gpBase->mpMenuCfg->GetBool("Options", "ShowCommentary", false);
	case eLangFiles:
	{
		tWStringList lstLangs;
		cPlatform::FindFilesInDir(lstLangs, cString::To16Char(gpBase->msGameLanguageFolder), _W("*.lang"));
		gLists[0].assign(lstLangs.begin(), lstLangs.end());
		return gLists[0].size();
	}
	case eListStr: Out(a, gLists[a.i[0]][a.i[1]]); return 0;
	case eVideoModes: cPlatform::GetAvailableVideoModes(gModes, 32); return gModes.size();
	case eVideoMode:
	{
		const cVideoMode& mode = gModes[a.i[0]];
		a.i[0] = mode.mlDisplay; a.i[1] = mode.mvScreenSize.x; a.i[2] = mode.mvScreenSize.y; a.i[3] = mode.mlRefreshRate;
		return 0;
	}
	case eDisplayName: Out(a, cPlatform::GetDisplayName(a.i[0])); return 0;
	case eSndDevices:
	{
		iLowLevelSound* pLow = gpBase->mpEngine->GetSound()->GetLowLevel();
		gCurDevice = pLow->GetCurrentSoundDevice();
		gDevices = pLow->GetFilteredSoundDevices();
		gLists[1].clear();
		for(size_t i = 0; i < gDevices.size(); ++i) gLists[1].push_back(cString::To16Char(gDevices[i]->GetName()));
		gLists[5].assign(1, cString::To16Char(gCurDevice->GetName()));
		return gDevices.size();
	}
	case eSndDevice: { iSoundDeviceIdentifier* pDev = gDevices[a.i[0]]; a.i[0] = pDev->GetID(); a.i[1] = pDev == gCurDevice; return 0; }

	case eActions:
	{
		gActions = gpBase->mpInputHandler->GetActionsByCategory((eLuxActionCategory)a.i[0]);
		gLists[2].clear();
		for(size_t i = 0; i < gActions.size(); ++i) gLists[2].push_back(Widen(gActions[i]->msName));
		return gActions.size();
	}
	case eAction: { cLuxAction* pAct = gActions[a.i[0]]; a.i[0] = pAct->mlId; a.i[1] = pAct->mbConfigurable; return 0; }
	case eDefaultInputs:
	{
		gDefaultInputs = gpBase->mpInputHandler->GetDefaultInputsByActionId(a.i[0]);
		gLists[3].clear();
		for(size_t i = 0; i < gDefaultInputs.size(); ++i) gLists[3].push_back(Widen(gDefaultInputs[i]->msInputType));
		return gDefaultInputs.size();
	}
	case eDefaultInput: return (size_t)(unsigned)gDefaultInputs[a.i[0]]->mlValue;
	case eInitialInputs:
	{
		cAction* pAction = gpBase->mpEngine->GetInput()->GetAction(a.i[0]);
		gLists[4].clear();
		for(int i = 0; i < pAction->GetSubActionNum() && i < 2; ++i)
		{
			iSubAction* pSub = pAction->GetSubAction(i);
			gLists[4].push_back(Widen(pSub->GetInputType() + "." + pSub->GetInputName()));
		}
		return gLists[4].size();
	}
	case eActionClear: gpBase->mpEngine->GetInput()->GetAction(a.i[0])->ClearSubActions(); return 0;
	case eActionAdd:
	{
		cAction* pAction = gpBase->mpEngine->GetInput()->GetAction(a.i[0]);
		switch(a.i[1])
		{
		case 1: pAction->AddKey((eKey)a.i[2]); break;
		case 2: pAction->AddMouseButton((eMouseButton)a.i[2]); break;
		case 3: pAction->AddGamepadButton(0, (eGamepadButton)a.i[2]); break;
		case 4: pAction->AddGamepadHat(0, (eGamepadHat)a.i[2], (eGamepadHatState)a.i[3]); break;
		case 5: pAction->AddGamepadAxis(0, (eGamepadAxis)a.i[2], (eGamepadAxisRange)a.i[3], 0.25f); break;
		}
		return 0;
	}
	case eActionName: Out(a, Widen(gpBase->mpEngine->GetInput()->GetAction(a.i[0])->GetName())); return 0;

	case eIsChinese: return gpBase->msCurrentLanguage == "chinese.lang";
	case eSkinFontMul:
		{
			cGuiSkin* pSkin = (cGuiSkin*)a.p[0];
			pSkin->GetFont(eGuiSkinFont_Default)->mvSize *= a.f[0];
			pSkin->GetFont(eGuiSkinFont_Disabled)->mvSize *= a.f[0];
			return 0;
		}
	case eVpNew: return (size_t)gpBase->mpEngine->GetScene()->CreateViewport();
	case eVpActive: ((cViewport*)a.p[0])->SetActive(a.i[0] != 0); return 0;
	case eVpVisible: ((cViewport*)a.p[0])->SetVisible(a.i[0] != 0); return 0;
	case eVpAddSet: ((cViewport*)a.p[0])->AddGuiSet((cGuiSet*)a.p[1]); return 0;
	case eVpWorld: ((cViewport*)a.p[0])->SetWorld((cWorld*)a.p[1]); return 0;
	case eVpCamera: ((cViewport*)a.p[0])->SetCamera((cCamera*)a.p[1]); return 0;
	case eCfg:
		{
			cConfigFile* pCfg = gpBase->mpMenuCfg;
			switch(a.i[0])
			{
			case 0: a.f[0] = pCfg->GetFloat(Narrow(a, 0), Narrow(a, 1), a.f[0]); break;
			case 1: { cVector2f v = pCfg->GetVector2f(Narrow(a, 0), Narrow(a, 1), 0); a.f[0] = v.x; a.f[1] = v.y; break; }
			default: Out(a, Widen(pCfg->GetString(Narrow(a, 0), Narrow(a, 1), Narrow(a, 2)))); break;
			}
			return 0;
		}
	case eNavGet: return (size_t)pW->GetFocusNavigation(kArrows[a.i[0] & 3]);
	case eFontType: pW->SetDefaultFontType((iFontData*)a.p[1]); return 0;
	case eFontLen:
		{
			float fLength = pW->GetDefaultFontType()->GetLength(cVector2f(a.f[0], a.f[1]), Wide(a, 0).c_str());
			a.f[0] = fLength;
			return 0;
		}
	case eFontLoad: return (size_t)gpBase->mpMainMenu->LoadFont(Narrow(a, 0));
	case ePopupOpts:
		{
			cGuiPopUpMessageBox* pPopUp = (cGuiPopUpMessageBox*)a.p[0];
			pPopUp->GetGuiSet()->SetDrawFocus(((cGuiSet*)a.p[1])->GetDrawFocus());
			pPopUp->SetKillOnEscapeKey(false);
			return 0;
		}
	case eProcessMouseDown: return pW->ProcessMessage(eGuiMessage_MouseDown, gpCbData ? *gpCbData : cGuiMessageData());
	case eWorldDestroy: gpBase->mpEngine->GetScene()->DestroyWorld((cWorld*)a.p[0]); return 0;
	case eCameraDestroy: gpBase->mpEngine->GetScene()->DestroyCamera((cCamera*)a.p[0]); return 0;
	case eWorldLoad: return (size_t)gpBase->mpEngine->GetScene()->LoadWorld(Narrow(a, 0), 0);
	case eCameraNew:
		{
			cCamera* pCamera = gpBase->mpEngine->GetScene()->CreateCamera(eCameraMoveMode_Fly);
			pCamera->SetRotateMode(eCameraRotateMode_Matrix);
			return (size_t)pCamera;
		}
	case eCameraFov: ((cCamera*)a.p[0])->SetFOV(a.f[0]); return 0;
	case eCameraAspect: ((cCamera*)a.p[0])->SetAspect(a.f[0]); return 0;
	case eCameraPlace:
		{
			cCamera* pCamera = (cCamera*)a.p[0];
			cWorld* pWorld = (cWorld*)a.p[1];
			cMatrixf mtxCam = cMatrixf::Identity;
			bool bFound = true;
			if(pWorld)
			{
				cStartPosEntity* pStartPos = pWorld->GetStartPosEntity(Narrow(a, 0));
				if(pStartPos) mtxCam = pStartPos->GetWorldMatrix();
				else bFound = false;
			}
			pCamera->SetRotationMatrix(cMath::MatrixInverse(mtxCam.GetRotation()));
			pCamera->SetPosition(mtxCam.GetTranslation());
			return bFound;
		}
	case eScreenSize:
		{
			iLowLevelGraphics* pLow = LowGfx();
			cVector2f vF = pLow->GetScreenSizeFloat();
			cVector3l vI = pLow->GetScreenSizeInt();
			a.f[0] = vF.x; a.f[1] = vF.y; a.i[0] = vI.x; a.i[1] = vI.y;
			return 0;
		}
	case eTexNew:
		{
			iTexture* pTex = gpBase->mpEngine->GetGraphics()->CreateTexture(Narrow(a, 0), eTextureType_Rect, eTextureUsage_RenderTarget);
			pTex->CreateFromRawData(cVector3l(a.i[0], a.i[1], 0), ePixelFormat_RGBA, NULL);
			if(a.i[2]) pTex->SetWrapSTR(eTextureWrap_ClampToEdge);
			return (size_t)pTex;
		}
	case eTexDestroy: gpBase->mpEngine->GetGraphics()->DestroyTexture((iTexture*)a.p[0]); return 0;
	case eFbNew:
		{
			iFrameBuffer* pBuffer = gpBase->mpEngine->GetGraphics()->CreateFrameBuffer(Narrow(a, 0));
			pBuffer->SetTexture2D(0, (iTexture*)a.p[0]);
			pBuffer->CompileAndValidate();
			return (size_t)pBuffer;
		}
	case eFbDestroy: gpBase->mpEngine->GetGraphics()->DestroyFrameBuffer((iFrameBuffer*)a.p[0]); return 0;
	case eProgNew:
		{
			cParserVarContainer programVars;
			if(a.i[0]) programVars.Add("BlurHorisontal");
			return (size_t)gpBase->mpEngine->GetGraphics()->CreateGpuProgramFromShaders(Narrow(a, 0), Narrow(a, 1), Narrow(a, 2), &programVars);
		}
	case eProgBind:
		if(a.i[0]) ((iGpuProgram*)a.p[0])->Bind();
		else ((iGpuProgram*)a.p[0])->UnBind();
		return 0;
	case eGfxState:
		switch(a.i[0])
		{
		case 0: LowGfx()->SetBlendActive(a.i[1] != 0); break;
		case 1: LowGfx()->SetDepthTestActive(a.i[1] != 0); break;
		case 2: LowGfx()->SetDepthWriteActive(a.i[1] != 0); break;
		}
		return 0;
	case eGfxOrtho:
		LowGfx()->SetOrthoProjection(cVector2f(a.f[0], a.f[1]), -1000, 1000);
		LowGfx()->SetIdentityMatrix(eMatrix_ModelView);
		return 0;
	case eGfxCopyScreen: LowGfx()->CopyFrameBufferToTexure((iTexture*)a.p[0], 0, LowGfx()->GetScreenSizeInt(), 0); return 0;
	case eGfxSetFb: LowGfx()->SetCurrentFrameBuffer((iFrameBuffer*)a.p[0]); return 0;
	case eGfxSetTex: LowGfx()->SetTexture(0, (iTexture*)a.p[0]); return 0;
	case eGfxQuad:
		{
			cVector2f vSize(a.f[0], a.f[1]);
			LowGfx()->DrawQuad(0, vSize, cVector2f(0, vSize.y), cVector2f(vSize.x, 0), cColor(1, 1));
			return 0;
		}
	case eGfxFlush: LowGfx()->FlushRendering(); LowGfx()->WaitAndFinishRendering(); return 0;

	case eMapLoaded: return gpBase->mpMapHandler->MapIsLoaded();
	case eMusic:
		{
			cMusicHandler* pMusic = gpBase->mpEngine->GetSound()->GetMusicHandler();
			switch(a.i[0])
			{
			case 0: pMusic->Play(Narrow(a, 0), 1.0f, 0.35f, true, false); break;
			case 1: pMusic->Stop(0.33f); break;
			case 2: pMusic->Pause(); break;
			case 3: pMusic->Resume(); break;
			}
			return 0;
		}
	case ePauseMap: gpBase->mpMapHandler->PauseSoundsAndMusic(); return 0;
	case eProgLog: ProgLog(eLuxProgressLogLevel_Low, Narrow(a, 0)); return 0;
	case eInputState:
		{
			static const eLuxInputState vStates[3] = { eLuxInputState_Null, eLuxInputState_Game, eLuxInputState_MainMenu };
			gpBase->mpInputHandler->ChangeState(vStates[a.i[0]]);
			return 0;
		}
	case eLockInput: gpBase->mpEngine->GetInput()->GetLowLevel()->LockInput(a.i[0] != 0); return 0;
	case eRelativeMouse: gpBase->mpEngine->GetInput()->GetLowLevel()->RelativeMouse(a.i[0] != 0); return 0;
	case eFlag:
		switch(a.i[0])
		{
		case 0: return gpBase->mpConfigHandler->mbFullscreen;
		case 1: return gpBase->mpDebugHandler->GetDebugWindowActive();
		case 2: return gpBase->mpInputHandler->IsGamepadPresent();
		case 3: return gpBase->mpUserConfig == NULL;
		case 4: return gpBase->mbAllowHardmode;
		case 5: return gpBase->mpCustomStory == 0;
		case 6: return gpBase->mbPTestActivated;
		case 7: return gpBase->mpSaveHandler->SaveFileExists();
		case 8: return gpBase->mbExitMenuDirectly;
		case 9: return gpBase->mpPlayer->IsDead();
		case 10: return gpBase->mbHardMode;
		case 11: return gpBase->mpDemoEnd && gpBase->mpDemoEnd->ShowOnAllExit();
#ifdef USE_GAMEPAD
		case 12: return 1;
#endif
#ifdef LUX_DEMO_VERSION
		case 13: return 1;
#endif
		}
		return 0;
	case eSetHard: gpBase->mbHardMode = a.i[0] != 0; return 0;
	case eTinder:
		if(a.i[0] == 0) return (size_t)(unsigned)gpBase->mpPlayer->GetTinderboxes();
		gpBase->mpPlayer->SetTinderboxes(a.i[1]);
		return 0;
	case eSave:
		switch(a.i[0])
		{
		case 0: gpBase->mpSaveHandler->AutoSave(); break;
		case 1: gpBase->mpSaveHandler->AutoLoad(true); break;
		case 2: gpBase->mpSaveHandler->HardModeSave(); break;
		}
		return 0;
	case eHasMap: return gpBase->mpMapHandler->GetCurrentMap() != NULL;
	case eDestroyMap: gpBase->mpMapHandler->DestroyMap(gpBase->mpMapHandler->GetCurrentMap(), false); return 0;
	case eUpdater:
		switch(a.i[0])
		{
		case 0: gpBase->mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset); break;
		case 1: gpBase->mpEngine->GetUpdater()->SetContainer("Default"); break;
		case 2: gpBase->mpEngine->GetUpdater()->SetContainer("DemoEnd"); break;
		case 3: gpBase->mpEngine->Exit(); break;
		}
		return 0;
	case eCustomClear: gpBase->SetCustomStory(NULL); return 0;
	case eProfile:
		switch(a.i[0])
		{
		case 0: gpBase->CreateProfile(gpBase->msDefaultProfileName); break;
		case 1: gpBase->SetProfile(gpBase->msDefaultProfileName); break;
		case 2: gpBase->InitUserConfig(); break;
		}
		return 0;
	case eStartGame: gpBase->StartGame("", "", ""); return 0;
	case eDrawMenuScreen: gpBase->mpLoadScreenHandler->DrawMenuScreen(); return 0;
	case eZoomSound: gpBase->mpEngine->GetSound()->GetSoundHandler()->PlayGui(Narrow(a, 0), false, 1.0f); return 0;
	case eMapName: Out(a, gpBase->mpMapHandler->GetCurrentMap() ? Widen(gpBase->mpMapHandler->GetCurrentMap()->GetName()) : tWString()); return 0;
	case eUnlockNope: gpBase->mpAchievementHandler->UnlockAchievement(eLuxAchievement_NOPE); return 0;
	case eLogError: Error("%s", Narrow(a, 0).c_str()); return 0;
	case eGuiMouse: return (size_t)(unsigned)gpBase->mpEngine->GetGui()->TranslateGuiMouseToMouse((eGuiMouseButton)a.i[0]);
	case eAxisRange: return (size_t)(unsigned)iGamepad::GetAxisRangeFromValue(a.f[0]);
	}
	return 0;
}

void OhplA10aHostInit()
{
	static bool bDone = false;
	if(bDone) return;
	bDone = true;
	if(ohpl_a10a_set_host(&Host, sizeof(OhplA10aOpts)) == 0) abort();
}

static void DumpWidget(FILE* apOut, iWidget* apWidget, int alDepth, std::map<iWidget*,int>& amIds)
{
	int lId = amIds[apWidget];
	fprintf(apOut, "%*s#%d type=%d name=%s", alDepth*2, "", lId, (int)apWidget->GetType(), apWidget->GetName().c_str());
	const cVector3f& vPos = apWidget->GetLocalPosition();
	cVector2f vSize = apWidget->GetSize();
	fprintf(apOut, " pos=%.9g,%.9g,%.9g size=%.9g,%.9g", vPos.x, vPos.y, vPos.z, vSize.x, vSize.y);
	fprintf(apOut, " text=\"%s\" vis=%d en=%d uv=%d ud=%d", cString::To8Char(apWidget->GetText()).c_str(), apWidget->IsVisible(), apWidget->IsEnabled(), apWidget->GetUserValue(), apWidget->GetUserData() != NULL);
	const cColor& col = apWidget->GetDefaultFontColor();
	fprintf(apOut, " col=%.9g,%.9g,%.9g,%.9g fs=%.9g,%.9g", col.r, col.g, col.b, col.a, apWidget->GetDefaultFontSize().x, apWidget->GetDefaultFontSize().y);
	fprintf(apOut, " nav=");
	const eUIArrow vArrows[4] = { eUIArrow_Up, eUIArrow_Right, eUIArrow_Down, eUIArrow_Left };
	for(int i=0;i<4;++i)
	{
		iWidget* pNav = apWidget->GetFocusNavigation(vArrows[i]);
		fprintf(apOut, "%d%s", pNav && amIds.count(pNav) ? amIds[pNav] : -1, i<3 ? "," : "");
	}
	if(cWidgetCheckBox* pCheck = dynamic_cast<cWidgetCheckBox*>(apWidget))
		fprintf(apOut, " checked=%d", pCheck->IsChecked());
	if(cWidgetSlider* pSlider = dynamic_cast<cWidgetSlider*>(apWidget))
		fprintf(apOut, " slider=%d/%d", pSlider->GetValue(), pSlider->GetMaxValue());
	if(cWidgetComboBox* pCombo = dynamic_cast<cWidgetComboBox*>(apWidget))
	{
		fprintf(apOut, " combo=%d[", pCombo->GetSelectedItem());
		for(int i=0;i<pCombo->GetItemNum();++i)
			fprintf(apOut, "%s\"%s\"", i ? "," : "", cString::To8Char(pCombo->GetItem(i)->GetText()).c_str());
		fprintf(apOut, "]");
	}
	if(cWidgetLabel* pLabel = dynamic_cast<cWidgetLabel*>(apWidget))
		fprintf(apOut, " align=%d wrap=%d", (int)pLabel->GetTextAlign(), pLabel->GetWordWrap());
	fprintf(apOut, "\n");

	tWidgetList lstChildren = apWidget->GetChildren();
	for(tWidgetListIt it = lstChildren.begin(); it != lstChildren.end(); ++it)
		DumpWidget(apOut, *it, alDepth+1, amIds);
}

static void NumberWidgets(iWidget* apWidget, std::map<iWidget*,int>& amIds)
{
	int lId = (int)amIds.size();
	amIds[apWidget] = lId;
	tWidgetList lstChildren = apWidget->GetChildren();
	for(tWidgetListIt it = lstChildren.begin(); it != lstChildren.end(); ++it)
		NumberWidgets(*it, amIds);
}

void OhplA10aDumpWindow(const char* asName, iWidget* apRoot)
{
	const char* sPath = getenv("OHPL_A10A_DUMP");
	if(sPath == NULL || apRoot == NULL) return;

	FILE* pOut = fopen(sPath, "a");
	if(pOut == NULL) return;

	std::map<iWidget*,int> mIds;
	NumberWidgets(apRoot, mIds);
	fprintf(pOut, "== %s\n", asName);
	DumpWidget(pOut, apRoot, 0, mIds);
	fclose(pOut);
}
