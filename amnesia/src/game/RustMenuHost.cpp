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
};

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
	eGuiMessage_TextChange, eGuiMessage_KeyPress, eGuiMessage_UIArrowPress, eGuiMessage_GamepadInput,
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
	return ohpl_a10a_cb((size_t)apToken, (size_t)apWidget, &cb) != 0;
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
	SetStr(o.lang, pCfg->msLangFile);
	o.resX = pCfg->mvScreenSize.x;
	o.resY = pCfg->mvScreenSize.y;
	o.display = pCfg->mlDisplay;
	o.fullscreen = pCfg->mbFullscreen;
	o.vsync = pCfg->mbVSync;
	o.showFps = gpBase->mpDebugHandler->GetShowFPS();
	SetStr(o.renderer, pCfg->msRenderer);
	SetStr(o.color, pCfg->msColorOutput);
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
	pCfg->msLangFile = GetStr(o.lang);
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
	pCfg->msRenderer = GetStr(o.renderer);
	pCfg->msColorOutput = GetStr(o.color);
	pCfg->mfPaperWhite = o.paperWhite;
	pCfg->mfPeakNits = o.peakNits;
	pGfx->GetLowLevel()->SetColorOutput(pCfg->msColorOutput, pCfg->mfPaperWhite, pCfg->mfPeakNits);
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
	cColor col(a.f[0], a.f[1], a.f[2], a.f[3]);
	switch(alOp)
	{
	case eWindow: return (size_t)pSet->CreateWidgetWindow(eWidgetWindowButtonFlag_None, Pos(a), Size(a), Wide(a, 0));
	case eButton: return (size_t)pSet->CreateWidgetButton(Pos(a), Size(a), Wide(a, 0), pParent);
	case eLabel: return (size_t)pSet->CreateWidgetLabel(Pos(a), Size(a), Wide(a, 0), pParent);
	case eCheckBox: return (size_t)pSet->CreateWidgetCheckBox(Pos(a), Size(a), Wide(a, 0), pParent);
	case eCombo: return (size_t)pSet->CreateWidgetComboBox(Pos(a), Size(a), _W(""), pParent);
	case eSlider: return (size_t)pSet->CreateWidgetSlider(eWidgetSliderOrientation_Horizontal, Pos(a), Size(a), 0, pParent);
	case eTabFrame: return (size_t)pSet->CreateWidgetTabFrame(Pos(a), Size(a), _W(""), pParent, false, true);
	case eImage:
		if(a.n[1] == 0)
			return (size_t)pSet->CreateWidgetImage(Narrow(a, 0), Pos(a), Size(a), eGuiMaterial_Alpha, false, pParent);
		return (size_t)pSet->CreateWidgetImage(Narrow(a, 0), Pos(a), Size(a), eGuiMaterial_Alpha, false, pParent, Narrow(a, 1));
	case eGroup: return (size_t)pSet->CreateWidgetGroup(Pos(a), Size(a), Wide(a, 0), pParent);
	case eDummy: return (size_t)pSet->CreateWidgetDummy(Pos(a), pParent);
	case eFrame: return (size_t)pSet->CreateWidgetFrame(Pos(a), Size(a), false, pParent, false, true);

	case eSize: a.f[0] = pW->GetSize().x; a.f[1] = pW->GetSize().y; return 0;
	case eLocalPos: { const cVector3f& v = pW->GetLocalPosition(); a.f[0] = v.x; a.f[1] = v.y; a.f[2] = v.z; return 0; }
	case eGlobalPos: { const cVector3f& v = pW->GetGlobalPosition(); a.f[0] = v.x; a.f[1] = v.y; a.f[2] = v.z; return 0; }
	case eSetSize: pW->SetSize(cVector2f(a.f[0], a.f[1])); return 0;
	case eSetPos: pW->SetPosition(Pos(a)); return 0;
	case eSetText: pW->SetText(Wide(a, 0)); return 0;
	case eGetText: Out(a, pW->GetText()); return 0;
	case eSetVisible: pW->SetVisible(a.i[0] != 0); return 0;
	case eSetEnabled: pW->SetEnabled(a.i[0] != 0); return 0;
	case eIsVisible: return pW->IsVisible();
	case eSetUserData: pW->SetUserData((void*)a.p[1]); return 0;
	case eGetUserData: return (size_t)pW->GetUserData();
	case eSetUserValue: pW->SetUserValue(a.i[0]); return 0;
	case eGetUserValue: return (size_t)(unsigned)pW->GetUserValue();
	case eSetNav: pW->SetFocusNavigation(kArrows[a.i[0] & 3], pParent); return 0;
	case eSetGlobalUi: pW->SetGlobalUIInputListener(a.i[0] != 0); return 0;
	case eSetStatic: static_cast<cWidgetWindow*>(pW)->SetStatic(a.i[0] != 0); return 0;
	case eFontColor: { const cColor& c = pW->GetDefaultFontColor(); a.f[0] = c.r; a.f[1] = c.g; a.f[2] = c.b; a.f[3] = c.a; return 0; }
	case eSetFontColor: pW->SetDefaultFontColor(col); return 0;
	case eFontSize: a.f[0] = pW->GetDefaultFontSize().x; a.f[1] = pW->GetDefaultFontSize().y; return 0;
	case eSetFontSize: pW->SetDefaultFontSize(cVector2f(a.f[0], a.f[1])); return 0;
	case eTextLength: a.f[0] = pW->GetDefaultFontType()->GetLength(pW->GetDefaultFontSize(), pW->GetText().c_str()); return 0;
	case eSetWordWrap: static_cast<cWidgetLabel*>(pW)->SetWordWrap(a.i[0] != 0); return 0;
	case eSetClip: pW->SetClipActive(a.i[0] != 0); return 0;
	case eSetScrollSpeed: static_cast<cWidgetLabel*>(pW)->SetScrollSpeedMul(a.f[0]); return 0;
	case eSetDrawBg: static_cast<cWidgetLabel*>(pW)->SetDrawBackGround(a.i[0] != 0); return 0;
	case eSetBgColor: static_cast<cWidgetLabel*>(pW)->SetBackGroundColor(col); return 0;
	case eSetAlign: static_cast<cWidgetLabel*>(pW)->SetTextAlign(eFontAlign_Center); return 0;
	case eSetAutosize: static_cast<cWidgetLabel*>(pW)->SetAutogenerateSize(a.i[0] != 0); return 0;
	case eSetScrollOffset: static_cast<cWidgetLabel*>(pW)->SetScrollOffset(a.f[0]); return 0;
	case eParent: return (size_t)pW->GetParent();
	case eAddCallback: pW->AddCallback(kMsgs[a.i[0]], (void*)a.p[1], &Trampoline); return 0;
	case eSetChecked: static_cast<cWidgetCheckBox*>(pW)->SetChecked(a.i[0] != 0, a.i[1] != 0); return 0;
	case eIsChecked: return static_cast<cWidgetCheckBox*>(pW)->IsChecked();
	case eComboAdd: static_cast<cWidgetComboBox*>(pW)->AddItem(Wide(a, 0)); return 0;
	case eComboClear: static_cast<cWidgetComboBox*>(pW)->ClearItems(); return 0;
	case eComboSelect: static_cast<cWidgetComboBox*>(pW)->SetSelectedItem(a.i[0], a.i[1] != 0, a.i[2] != 0); return 0;
	case eComboSelected: return (size_t)(unsigned)static_cast<cWidgetComboBox*>(pW)->GetSelectedItem();
	case eComboNum: return (size_t)(unsigned)static_cast<cWidgetComboBox*>(pW)->GetItemNum();
	case eSliderMax: static_cast<cWidgetSlider*>(pW)->SetMaxValue(a.i[0]); return 0;
	case eSliderBar: static_cast<cWidgetSlider*>(pW)->SetBarValueSize(a.i[0]); return 0;
	case eSliderSet: static_cast<cWidgetSlider*>(pW)->SetValue(a.i[0], a.i[1] != 0); return 0;
	case eSliderGet: return (size_t)(unsigned)static_cast<cWidgetSlider*>(pW)->GetValue();
	case eSliderGetMax: return (size_t)(unsigned)static_cast<cWidgetSlider*>(pW)->GetMaxValue();
	case eTabAdd: return (size_t)static_cast<cWidgetTabFrame*>(pW)->AddTab(Wide(a, 0));
	case eTabNum: return (size_t)(unsigned)static_cast<cWidgetTabFrame*>(pW)->GetTabNum();
	case eTabGet: return (size_t)static_cast<cWidgetTabFrame*>(pW)->GetTab(a.i[0]);
	case eTabTop: return (size_t)static_cast<cWidgetTabFrame*>(pW)->GetTabOnTop();
	case eTabLabel: return (size_t)static_cast<cWidgetTab*>(pW)->GetTabLabel();
	case eTabFrameOf: return (size_t)static_cast<cWidgetTab*>(pW)->GetParentTabFrame();
	case eFrameOf: return (size_t)dynamic_cast<cWidgetFrame*>(pW);
	case eFrameScrollTo: static_cast<cWidgetFrame*>(pW)->ScrollToPosition(cVector2f(a.f[0], a.f[1])); return 0;
	case eFrameScroll: { const cVector3f& v = pW->GetScrollAmount(); a.f[0] = v.x; a.f[1] = v.y; a.f[2] = v.z; return 0; }
	case eFrameDrawBg: static_cast<cWidgetFrame*>(pW)->SetDrawBackground(a.i[0] != 0); return 0;
	case eMouseOver: return pW->GetMouseIsOver();

	case ePopupActive: return pSet->PopUpIsActive();
	case eSetDefaultNav: pSet->SetDefaultFocusNavWidget(pParent); return 0;
	case eSetFocused: pSet->SetFocusedWidget(pParent); return 0;
	case ePushAttention: pSet->PushAttentionWidget(); return 0;
	case ePopAttention: pSet->PopAttentionWidget(); return 0;
	case ePushFocused: pSet->PushFocusedWidget(); return 0;
	case ePopFocused: pSet->PopFocusedWidget(); return 0;
	case ePushDefaultNav: pSet->PushDefaultFocusNavWidget(); return 0;
	case eSetAttention: pSet->SetAttentionWidget(pParent); return 0;
	case eSetMouseMove: pSet->SetMouseMovementEnabled(a.i[0] != 0); return 0;
	case eSetDrawMouse: pSet->SetDrawMouse(a.i[0] != 0); return 0;
	case eMsgBox: pSet->CreatePopUpMessageBox(Wide(a, 0), Wide(a, 1), Wide(a, 2), Wide(a, 3), (void*)a.p[1], &Trampoline); return 0;

	case eSetWindowActive: gpBase->mpMainMenu->SetWindowActive((eLuxMainMenuWindow)a.i[0]); return 0;
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

	tWidgetList& lstChildren = apWidget->GetChildren();
	for(tWidgetListIt it = lstChildren.begin(); it != lstChildren.end(); ++it)
		DumpWidget(apOut, *it, alDepth+1, amIds);
}

static void NumberWidgets(iWidget* apWidget, std::map<iWidget*,int>& amIds)
{
	int lId = (int)amIds.size();
	amIds[apWidget] = lId;
	tWidgetList& lstChildren = apWidget->GetChildren();
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
