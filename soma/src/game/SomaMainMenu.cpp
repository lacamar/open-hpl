/*
 * See SomaMainMenu.h for scope notes and where these real values came from
 * (script/modules/MenuHandler.hps + helper_imgui_options.hps, read directly
 * out of SOMA's own shipped game data).
 */

#include "SomaMainMenu.h"
#include "SomaBase.h"
#include "SomaConfig.h"

#include <cmath>

//---------------------------------------
// Real design-space constants, verbatim from MenuHandler.hps/
// helper_imgui_options.hps ("Data for menu, values based on 1280x720
// resolution"). Used directly against a 1280x720 cGuiSet virtual size -
// see the class comment for why that's the real scaling mechanism.

static const cVector2f kVirtualCanvas(1280, 720);

static const cVector2f kMainMenuButtonPos(136, 275);
static const cVector2f kTitlePos(100, 67);
static const cVector2f kTitleSize(4 * 173, 173);
static const cColor kMainMenuButtonBgColor(69.0f / 255.0f, 139.0f / 255.0f, 139.0f / 255.0f, 1.0f);

static const cVector2f kOptionMenuButtonBgSize(16 * 42, 42);
static const float kOptionMenuButtonSpacing = 38;

static const cColor kDisabledColor(58.0f / 255.0f, 1.0f);
static const cColor kSelectedTextColor(0, 1);
static const cColor kDeselectedTextColor(1, 1);

static const float kFrontFaceScale = 0.7f;
static const float kBackFaceScale = 0.3f;

//---------------------------------------
// Real Options screen constants, verbatim from helper_imgui_options.hps
// (kOptionMenu_* there). Reusing kMainMenuButtonPos/kOptionMenuButtonSpacing
// above for row position/spacing - the real script does too (every
// OptionMenu_ButtonOptions*() call is passed kMainMenuButtonPos as avPos).

static const cVector2f kOptionsBgPos(100, 260);

static const cVector2f kOptionsCheckboxOffset(405, 2);
static const cVector2f kOptionsCheckboxSize(100, 46);

static const cVector2f kOptionsSliderOffset(305, 2);
static const cVector2f kOptionsSliderSize(368, 46);
static const float kOptionsSliderTrackLocalMinX = 325; // relative to row pos.x
static const float kOptionsSliderTrackLocalMaxX = 570;
static const cVector2f kOptionsSliderBarOffset(325, 16);
static const cVector2f kOptionsSliderBarSize(245, 5);
static const cVector2f kOptionsSliderArrowOffsetL(570, -1);
static const cVector2f kOptionsSliderArrowOffsetR(305, -2);
static const cVector2f kOptionsSliderArrowSize(20, 40);

// Real mGfxFrame.mGfxBackground.mColor (MenuHandler.hps's Init()).
static const cColor kOptionsFrameFillColor(5.0f / 255.0f, 60.0f / 255.0f, 72.0f / 255.0f, 0.25f);

//---------------------------------------
// Small helpers to build a fully-populated cSomaOptionsRow without leaving
// any field at a stale value from a previous push_back() - see
// cSomaMainMenu::BuildOptionsRows().

static cSomaOptionsRow MakeCategoryRow(const tWString &asLabel, eSomaMenuScreen aTarget)
{
	cSomaOptionsRow row;
	row.mKind = cSomaOptionsRow::eKind_Category;
	row.msLabel = asLabel;
	row.mTarget = aTarget;
	row.mpBoolValue = NULL;
	row.mpFloatValue = NULL;
	row.mfMin = row.mfMax = row.mfStep = 0;
	return row;
}

static cSomaOptionsRow MakeBackRow(eSomaMenuScreen aTarget)
{
	cSomaOptionsRow row = MakeCategoryRow(_W("BACK"), aTarget);
	row.mKind = cSomaOptionsRow::eKind_Back;
	return row;
}

static cSomaOptionsRow MakeToggleRow(const tWString &asLabel, bool *apValue)
{
	cSomaOptionsRow row;
	row.mKind = cSomaOptionsRow::eKind_Toggle;
	row.msLabel = asLabel;
	row.mTarget = eSomaMenuScreen_Main;
	row.mpBoolValue = apValue;
	row.mpFloatValue = NULL;
	row.mfMin = row.mfMax = row.mfStep = 0;
	return row;
}

static cSomaOptionsRow MakeSliderRow(const tWString &asLabel, float *apValue, float afMin, float afMax, float afStep)
{
	cSomaOptionsRow row;
	row.mKind = cSomaOptionsRow::eKind_Slider;
	row.msLabel = asLabel;
	row.mTarget = eSomaMenuScreen_Main;
	row.mpBoolValue = NULL;
	row.mpFloatValue = apValue;
	row.mfMin = afMin;
	row.mfMax = afMax;
	row.mfStep = afStep;
	return row;
}

//---------------------------------------

cSomaMainMenu::cSomaMainMenu(cEngine *apEngine, cSomaBase *apBase, cViewport *apViewport) : iUpdateable("SomaMainMenu")
{
	mpEngine = apEngine;
	mpBase = apBase;
	mpViewport = apViewport;

	mpBackgroundGfx = NULL;
	mpCornerUL = mpCornerUR = mpCornerBL = mpCornerBR = NULL;
	mpCathLeft = mpCathRight = mpCathJaw = NULL;
	mpTitleGfx = NULL;
	for (int i = 0; i < 4; ++i)
		mpTitleFlickerGfx[i] = NULL;
	mpButtonBarGfx = NULL;
	for (int i = 0; i < 3; ++i)
		mpButtonBarJitterGfx[i] = NULL;
	mpButtonFont = NULL;

	mbVisible = true;
	mbMouseWasDown = false;

	mfTitleAlpha = 0;
	mfFaceAlpha = 0;
	mfBGAnimTime = 0;

	mfTitleGlitchWaitTimer = cMath::RandRectf(4.0f, 7.0f);
	mlTitleGlitchTimes = 0;
	mfTitleGlitchTimer = 0;
	mlTitleGlitchPic = 0;

	mfTitlePulseTimer = cMath::RandRectf(1.0f, 4.0f);
	mTitleColorStart = cColor(1, 1);
	mTitleColorGoal = cColor(1, 1);
	mfTitleColorFadeT = 1.0f;
	mfTitleColorFadeLen = 1.0f;

	mlHoveredItem = -1;
	mlClickedItem = -1;
	mfButtonClickedTimer = 0;
	mPendingAction = eSomaMainMenuAction_None;

	mScreen = eSomaMenuScreen_Main;
	mlOptionsHoveredRow = -1;
	mlDraggingSliderRow = -1;

	mpFrameCornerTL = mpFrameCornerTR = mpFrameCornerBL = mpFrameCornerBR = NULL;
	mpFrameBorderTop = mpFrameBorderBottom = mpFrameBorderLeft = mpFrameBorderRight = NULL;
	mpFrameFillGfx = NULL;
	mpOptionsHighlightGfx = mpOptionsMeterGfx = mpOptionsArrowGfx = NULL;
	mpOptionsCheckOnGfx = mpOptionsCheckOffGfx = mpOptionsBarGfx = NULL;

	mpGui = mpEngine->GetGui();

	// Same real skin file cSomaSplash already uses (SOMA ships no separate
	// "main menu" skin) - only needed here for cGuiSet's mouse cursor
	// gfx/DrawGfx machinery, not for any skin-drawn widgets (this menu's
	// real text buttons are drawn directly, see DrawMenuItems()).
	mpGuiSkin = mpGui->CreateSkin("gui_default.skin");
	mpGuiSet = mpGui->CreateSet("SomaMainMenu", mpGuiSkin);

	// Real menu values are all in an 1280x720 design canvas ("Data for
	// menu, values based on 1280x720 resolution" - MenuHandler.hps) scaled
	// to the real screen; cGuiSet::SetVirtualSize() is this engine's own
	// existing equivalent of that (Dark Descent's menus use it too), so
	// use it here instead of hand-rolling the real script's aspect-ratio
	// math. cGui::SendMousePos() already converts incoming mouse
	// coordinates into this same virtual space, so mpGuiSet->GetMousePos()
	// below needs no extra conversion.
	mpGuiSet->SetVirtualSize(kVirtualCanvas, -1000, 1000);

	mpGuiSet->SetDrawMouse(true);
	mpViewport->AddGuiSet(mpGuiSet);
	mpGuiSet->SetActive(true);

	// cGui routes all mouse/keyboard input to a single global "focused" set
	// (cGui::mpSetInFocus, see SendMousePos()/SendMouseClickDown() etc. in
	// Gui.cpp) - nothing sets this automatically just from being active on
	// a viewport. Without this, mpGuiSet->GetMousePos() below never
	// updates. Same call cLuxMainMenu/cLuxPreMenu make in the real Dark
	// Descent menu code (LuxMainMenu.cpp/LuxPreMenu.cpp).
	mpGui->SetFocus(mpGuiSet);

	CreateGui();
	CreateOptionsGui();

	// Real menu music - "Menu_Music.ogg" ships as a plain OGG file (not
	// FMOD-banked like most of SOMA's other audio), directly playable
	// through this engine's existing OpenAL music backend with no extra
	// wiring - music/ is already a registered resources.cfg search dir.
	mpEngine->GetSound()->GetMusicHandler()->Play("Menu_Music.ogg", 1.0f, 0.5f, true, false);
}

//-----------------------------------------------------------------------

cSomaMainMenu::~cSomaMainMenu()
{
}

//-----------------------------------------------------------------------

cGuiGfxElement *cSomaMainMenu::CreateGfx(const tString &asFile, eGuiMaterial aMaterial)
{
	return mpGui->CreateGfxTexture(asFile, aMaterial, eTextureType_2D);
}

//-----------------------------------------------------------------------

void cSomaMainMenu::CreateGui()
{
	////////////////////////////////////
	// Real background/title/phase-1 art (graphics/startmenu/) - drawn
	// directly via DrawGfx() rather than cWidgetImage: cWidgetImage's
	// CreateGfxImage()->cImageManager path failed to find these exact
	// files (real cause not chased down further given a known-working
	// alternative). Real background file is 1280x720 native (matches the
	// virtual canvas 1:1).
	mpBackgroundGfx = CreateGfx("menu_background.tga", eGuiMaterial_Diffuse);

	// Phase-1 dirt corners (real mCurrentBGPhase default is
	// eMainMenuPhase_Initial_To_1_3 = 1, i.e. "p1_" - this scaffold has no
	// save/progress system to drive later phases).
	mpCornerUL = CreateGfx("p1_ul.tga", eGuiMaterial_Alpha);
	mpCornerUR = CreateGfx("p1_ur.tga", eGuiMaterial_Alpha);
	mpCornerBL = CreateGfx("p1_bl.tga", eGuiMaterial_Alpha);
	mpCornerBR = CreateGfx("p1_br.tga", eGuiMaterial_Alpha);

	// Real "cathedral" ghost-trail pieces (GuiBackground_DrawCathFacePart).
	mpCathLeft = CreateGfx("p1_left.tga", eGuiMaterial_Alpha);
	mpCathRight = CreateGfx("p1_right.tga", eGuiMaterial_Alpha);
	mpCathJaw = CreateGfx("p1_jaw.tga", eGuiMaterial_Alpha);

	// Real title graphic + its 4 glitch-flicker variants.
	mpTitleGfx = CreateGfx("startmenu_title.tga", eGuiMaterial_Alpha);
	mpTitleFlickerGfx[0] = CreateGfx("startmenu_title_flicker1.tga", eGuiMaterial_Alpha);
	mpTitleFlickerGfx[1] = CreateGfx("startmenu_title_flicker2.tga", eGuiMaterial_Alpha);
	mpTitleFlickerGfx[2] = CreateGfx("startmenu_title_flicker3.tga", eGuiMaterial_Alpha);
	mpTitleFlickerGfx[3] = CreateGfx("startmenu_title_flicker4.tga", eGuiMaterial_Alpha);

	// Real highlight bar behind the focused menu item + its click-flash
	// jitter frames (OptionMenu_ButtonBackground).
	mpButtonBarGfx = CreateGfx("startmenu_button_long.tga", eGuiMaterial_Alpha);
	mpButtonBarJitterGfx[0] = CreateGfx("startmenu_button_long_jitter2.tga", eGuiMaterial_Alpha);
	mpButtonBarJitterGfx[1] = CreateGfx("startmenu_button_long_jitter3.tga", eGuiMaterial_Alpha);
	mpButtonBarJitterGfx[2] = CreateGfx("startmenu_button_long_jitter4.tga", eGuiMaterial_Alpha);

	// Real main-menu font (Sansation Large Bold, 36px - see
	// OptionMenu_ButtonMainMenu()), not the generic skin's "Default" font.
	mpButtonFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("sansation_large_bold.fnt");

	////////////////////////////////////
	// Real menu item list/order/captions (MainMenu.Continue/NewGame/
	// LoadGame/Options/Exit in config/base_english.lang) and real
	// enable rule: with no save system in this scaffold, mbCanContinue is
	// always false, same as a real fresh install - Continue/LoadGame show
	// as disabled labels rather than buttons (GuiMainMenuSelection()).
	mItems[0].msLabel = _W("CONTINUE");
	mItems[0].mbEnabled = false;
	mItems[0].mAction = eSomaMainMenuAction_None;

	mItems[1].msLabel = _W("NEW GAME");
	mItems[1].mbEnabled = true;
	mItems[1].mAction = eSomaMainMenuAction_NewGame;

	mItems[2].msLabel = _W("LOAD GAME");
	mItems[2].mbEnabled = false;
	mItems[2].mAction = eSomaMainMenuAction_None;

	mItems[3].msLabel = _W("OPTIONS");
	mItems[3].mbEnabled = true;
	mItems[3].mAction = eSomaMainMenuAction_Options;

	mItems[4].msLabel = _W("EXIT");
	mItems[4].mbEnabled = true;
	mItems[4].mAction = eSomaMainMenuAction_Exit;

	for (int i = 0; i < 5; ++i)
		mItems[i].mfRowY = kMainMenuButtonPos.y + kOptionMenuButtonSpacing * i;
}

//-----------------------------------------------------------------------

void cSomaMainMenu::CreateOptionsGui()
{
	////////////////////////////////////
	// Real 9-slice frame (graphics/startmenu/gfx/window/menu_*.tga) - same
	// asset set MenuHandler.hps's mGfxFrame uses for the Options
	// background (ImGui_DrawFrame(mGfxFrame, ...) in GuiOptions()/
	// GuiOptionsAudio()/GuiOptionsVideoDisplay()/GuiOptionsVideoGamma()).
	// This engine has no scripted cImGuiFrameGfx equivalent, so
	// DrawOptionsPanel() below composites these corners/borders/fill by
	// hand rather than porting the real (closed, C++) DrawFrame() pixel
	// math exactly - a real, working panel using the real assets/colour,
	// not pixel-identical to the original.
	mpFrameCornerTL = CreateGfx("menu_corner_tl.tga", eGuiMaterial_Alpha);
	mpFrameCornerTR = CreateGfx("menu_corner_tr.tga", eGuiMaterial_Alpha);
	mpFrameCornerBL = CreateGfx("menu_corner_bl.tga", eGuiMaterial_Alpha);
	mpFrameCornerBR = CreateGfx("menu_corner_br.tga", eGuiMaterial_Alpha);
	mpFrameBorderTop = CreateGfx("menu_border_top.tga", eGuiMaterial_Alpha);
	mpFrameBorderBottom = CreateGfx("menu_border_bottom.tga", eGuiMaterial_Alpha);
	mpFrameBorderLeft = CreateGfx("menu_border_left.tga", eGuiMaterial_Alpha);
	mpFrameBorderRight = CreateGfx("menu_border_right.tga", eGuiMaterial_Alpha);

	// Real gfxBar/background fill: cImGui's default "no texture" cImGuiGfx
	// is a plain colour quad (used both for mGfxFrame.mGfxBackground and
	// for the slider track/handle) - cGui::CreateGfxFilledRect() is this
	// engine's own equivalent (same call amnesia/src/game/
	// LuxLoadScreenHandler.cpp etc. use for their own filled rects).
	mpFrameFillGfx = mpGui->CreateGfxFilledRect(cColor(1, 1), eGuiMaterial_Alpha);
	mpOptionsBarGfx = mpGui->CreateGfxFilledRect(cColor(1, 1), eGuiMaterial_Alpha);

	// Real Options-row widget art - confirmed distinct from the main menu's
	// own "startmenu_button_long"/jitter set above.
	mpOptionsHighlightGfx = CreateGfx("startmenu_options_button_long.tga", eGuiMaterial_Alpha);
	mpOptionsMeterGfx = CreateGfx("startmenu_options_button_meter.tga", eGuiMaterial_Alpha);
	mpOptionsArrowGfx = CreateGfx("startmenu_options_arrow.tga", eGuiMaterial_Alpha);
	mpOptionsCheckOnGfx = CreateGfx("startmenu_options_button_on.tga", eGuiMaterial_Alpha);
	mpOptionsCheckOffGfx = CreateGfx("startmenu_options_button_off.tga", eGuiMaterial_Alpha);
}

//-----------------------------------------------------------------------

void cSomaMainMenu::SetVisible(bool abVisible)
{
	mbVisible = abVisible;

	mpGuiSet->SetActive(abVisible);
	mpGuiSet->SetDrawMouse(abVisible);

	if (abVisible)
	{
		mpGui->SetFocus(mpGuiSet);
		mpEngine->GetSound()->GetMusicHandler()->Play("Menu_Music.ogg", 1.0f, 0.5f, true, false);
	}
	else
	{
		if (mpGui->GetFocusedSet() == mpGuiSet)
			mpGui->SetFocus(NULL);
		mpEngine->GetSound()->GetMusicHandler()->Stop(0.5f);
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::Update(float afTimeStep)
{
	if (mbVisible == false)
		return;

	// See the class comment in SomaMainMenu.h: cGui does not poll iMouse on
	// its own, so this scaffold has to do what amnesia/src/game/
	// LuxInputHandler.cpp does for the real game every frame - push mouse
	// position and left-click edges into cGui by hand.
	iMouse *pMouse = mpEngine->GetInput()->GetMouse();
	if (pMouse == NULL)
		return;

	mpGui->SendMousePos(pMouse->GetAbsPosition(), pMouse->GetRelPosition());

	bool bDown = pMouse->ButtonIsDown(eMouseButton_Left);
	bool bPressedEdge = bDown && mbMouseWasDown == false;

	if (mScreen == eSomaMenuScreen_Main)
	{
		UpdateMouseHitTest();

		if (bPressedEdge && mlHoveredItem != -1 && mlClickedItem == -1)
		{
			ClickItem(mItems[mlHoveredItem]);
		}

		if (mlClickedItem != -1)
		{
			mfButtonClickedTimer -= afTimeStep;
			if (mfButtonClickedTimer <= 0)
				RunPendingAction();
		}
	}
	else
	{
		// Options screen - see BuildOptionsRows()/UpdateOptionsMouseHitTest()/
		// ClickOptionsRow() below. No 0.15s click-flash delay here: the real
		// script's OptionMenu_ButtonOptions() (unlike OptionMenu_
		// ButtonMainMenu()) acts immediately on click.
		BuildOptionsRows();
		UpdateOptionsMouseHitTest();

		if (bPressedEdge && mlOptionsHoveredRow != -1)
			ClickOptionsRow(mlOptionsHoveredRow);

		if (bDown && mlDraggingSliderRow != -1)
			UpdateOptionsSliderDrag();

		if (bDown == false)
			mlDraggingSliderRow = -1;
	}

	mbMouseWasDown = bDown;
}

//-----------------------------------------------------------------------

void cSomaMainMenu::UpdateMouseHitTest()
{
	mlHoveredItem = -1;

	const cVector2f &vMouse = mpGuiSet->GetMousePos();

	// Real OptionMenu_ButtonMainMenu() hit-tests an 8000-unit-wide row
	// (ImGui_DoButtonExt with kOptionMenu_ButtonSize.x=8000) - effectively
	// "anywhere to the right of the label, to the edge of the screen".
	for (int i = 0; i < 5; ++i)
	{
		if (mItems[i].mbEnabled == false)
			continue;

		float fTop = mItems[i].mfRowY;
		float fBottom = fTop + kOptionMenuButtonSpacing;
		if (vMouse.x >= kMainMenuButtonPos.x && vMouse.x <= kVirtualCanvas.x && vMouse.y >= fTop && vMouse.y <= fBottom)
		{
			mlHoveredItem = i;
			break;
		}
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::ClickItem(cSomaMainMenuItem &aItem)
{
	// Real OptionMenu_ButtonMainMenu()/GuiMainMenuSelection(): a click
	// starts a 0.15s "ButtonClicked" flash (jitter background) and only
	// performs the actual action once that timer elapses.
	mlClickedItem = mlHoveredItem;
	mfButtonClickedTimer = 0.15f;
	mPendingAction = aItem.mAction;
}

//-----------------------------------------------------------------------

void cSomaMainMenu::RunPendingAction()
{
	eSomaMainMenuAction action = mPendingAction;
	mlClickedItem = -1;
	mPendingAction = eSomaMainMenuAction_None;

	switch (action)
	{
	case eSomaMainMenuAction_NewGame:
	{
		// Reads the real <StartMap>/main_init.cfg entry (SOMA's own
		// "00_00_intro.hpm"/PlayerStartArea_1 on a real install) via
		// cSomaBase::StartNewGame(), rather than a hardcoded map file -
		// see its comment in SomaBase.cpp for why 00_01_apartment.hpm was
		// wrong here (a later map in the intro sequence, not the real
		// start).
		tString sError;
		if (mpBase->StartNewGame(sError) == false)
		{
			Log("SOMA main menu: New Game failed to load the start map (%s)\n", sError.c_str());
			return;
		}
		SetVisible(false);
		break;
	}
	case eSomaMainMenuAction_Options:
		// Real menu's full tree is Gameplay/Controls/Video{Display,
		// PostEffect,World,Gamma}/Audio (eMainMenuGroup_Options*) - see
		// SomaMainMenu.h's class comment for which two of those this
		// scaffold actually ports (Audio/Display) and why.
		NavigateTo(eSomaMenuScreen_OptionsRoot);
		break;
	case eSomaMainMenuAction_Exit:
		// Real menu shows an "ARE YOU SURE YOU WANT TO EXIT?" confirm box
		// first (mbShowExit); not reproduced here, exits immediately.
		mpEngine->Exit();
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::OnDraw(float afFrameTime)
{
	if (mbVisible == false)
		return;

	// Background/title never go away behind the Options screen - matches
	// the real game (GuiOptions() etc. are drawn as an overlay on top of
	// GuiBackground(), never a scene replacement).
	DrawBackground(afFrameTime);
	DrawTitle(afFrameTime);

	if (mScreen == eSomaMenuScreen_Main)
	{
		DrawMenuItems();
	}
	else
	{
		BuildOptionsRows();
		DrawOptionsScreen();
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::DrawBackground(float afTimeStep)
{
	// Z ordering below mirrors the real script's relative depth (it adds a
	// constant +20 to every one of these via OptionMenu_GetTopLeftOffset()
	// - only the relative order matters here, not the absolute values):
	// background(-1.0) < face(-0.005) < title(0.0) < title-ghost/buttons
	// (1.0) < corners(22.5), all offset down by 10 here to leave headroom.
	if (mpBackgroundGfx)
		mpGuiSet->DrawGfx(mpBackgroundGfx, cVector3f(0, 0, -10.0f), kVirtualCanvas);

	// Real dirt-corner overlays, alpha 0.3, aligned flush to each corner
	// of the canvas (GuiBackground()) - real fZ=22.5 sits above every
	// other element here (even the buttons), so this scaffold matches
	// that instead of assuming it's a mistake in the real menu.
	const cColor cornerCol(1.0f, 0.3f);
	const float fCornerZ = 12.5f;
	if (mpCornerUL)
		mpGuiSet->DrawGfx(mpCornerUL, cVector3f(0, 0, fCornerZ), mpCornerUL->GetImageSize(), cornerCol);
	if (mpCornerUR)
	{
		cVector2f vSize = mpCornerUR->GetImageSize();
		mpGuiSet->DrawGfx(mpCornerUR, cVector3f(kVirtualCanvas.x - vSize.x, 0, fCornerZ), vSize, cornerCol);
	}
	if (mpCornerBL)
	{
		cVector2f vSize = mpCornerBL->GetImageSize();
		mpGuiSet->DrawGfx(mpCornerBL, cVector3f(0, kVirtualCanvas.y - vSize.y, fCornerZ), vSize, cornerCol);
	}
	if (mpCornerBR)
	{
		cVector2f vSize = mpCornerBR->GetImageSize();
		mpGuiSet->DrawGfx(mpCornerBR, cVector3f(kVirtualCanvas.x - vSize.x, kVirtualCanvas.y - vSize.y, fCornerZ), vSize, cornerCol);
	}

	// Real "cath's face" 3-layer ghost trail, fading in over ~2s
	// (mfBackgroundFaceAlpha += afTimeStep*0.5).
	mfFaceAlpha = cMath::Clamp(mfFaceAlpha + afTimeStep * 0.5f, 0.0f, 1.0f);
	mfBGAnimTime += afTimeStep;

	if (mfFaceAlpha > 0 && mpCathLeft && mpCathRight && mpCathJaw)
	{
		cVector3f vFacePos(960, 400, -1.0f);
		cColor colorMul(mfFaceAlpha, mfFaceAlpha);
		float fT = mfBGAnimTime * 0.05f;

		DrawCathFacePart(mpCathLeft,
						  vFacePos + cVector3f(cosf(fT), -sinf(fT), 0) * 10.0f * cosf(mfBGAnimTime * 0.075f),
						  cVector3f(vFacePos.x, vFacePos.y, vFacePos.z - 0.002f),
						  kFrontFaceScale, kBackFaceScale,
						  cColor(1, 1) * colorMul, cColor(0.3f, 0.8f) * colorMul, 3);

		DrawCathFacePart(mpCathRight,
						  vFacePos + cVector3f(-cosf(fT), -sinf(fT), 0) * 8.0f * sinf(mfBGAnimTime * 0.075f),
						  cVector3f(vFacePos.x + 30, vFacePos.y, vFacePos.z - 0.002f),
						  kFrontFaceScale, kBackFaceScale,
						  cColor(1, 1) * colorMul, cColor(0.3f, 0.8f) * colorMul, 3);

		DrawCathFacePart(mpCathJaw,
						  vFacePos + cVector3f(-cosf(fT), sinf(fT / 2.0f), 0) * 10.0f,
						  cVector3f(vFacePos.x, vFacePos.y + 50, vFacePos.z - 0.002f),
						  kFrontFaceScale, kBackFaceScale,
						  cColor(1, 1) * colorMul, cColor(0.3f, 0.8f) * colorMul, 3);
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::DrawCathFacePart(cGuiGfxElement *apGfx, const cVector3f &avFrontCenterPos, const cVector3f &avBackCenterPos,
									  float afFrontScale, float afBackScale, const cColor &aFrontCol, const cColor &aBackCol, int alLayers)
{
	if (alLayers <= 0)
		alLayers = 1;

	cVector2f vOriginalSize = apGfx->GetImageSize();
	cVector2f vSize = vOriginalSize * afFrontScale;

	mpGuiSet->DrawGfx(apGfx, avFrontCenterPos - cVector3f(vSize.x, vSize.y, 0) * 0.5f, vSize, aFrontCol);

	if (alLayers == 1)
		return;

	int lSteps = alLayers - 1;
	cVector3f vDir = avBackCenterPos - avFrontCenterPos;
	cVector3f vStep = vDir * (1.0f / (float)lSteps);

	float fScaleStep = (afBackScale - afFrontScale) / (float)lSteps;
	cColor colStep((aBackCol.r - aFrontCol.r) / lSteps, (aBackCol.g - aFrontCol.g) / lSteps,
					(aBackCol.b - aFrontCol.b) / lSteps, (aBackCol.a - aFrontCol.a) / lSteps);

	cVector3f vPos = avFrontCenterPos;
	float fScale = afFrontScale;
	cColor col = aFrontCol;

	for (int i = 0; i < lSteps; ++i)
	{
		vPos += vStep;
		fScale += fScaleStep;
		vSize = vOriginalSize * fScale;

		col.r += colStep.r;
		col.g += colStep.g;
		col.b += colStep.b;
		col.a += colStep.a;

		mpGuiSet->DrawGfx(apGfx, vPos - cVector3f(vSize.x, vSize.y, 0) * 0.5f, vSize, col);
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::DrawTitle(float afTimeStep)
{
	mfTitleAlpha = cMath::Clamp(mfTitleAlpha + afTimeStep * 0.5f, 0.0f, 1.0f);
	if (mfTitleAlpha <= 0 || mpTitleGfx == NULL)
		return;

	////////////////////////////////////
	// Real glitch-flicker state machine (GuiBackground(), "TitleGlitchWait"
	// / "TitleGlitchTimes" / "TitleGlitch" / "TitleGlitchPic").
	mfTitleGlitchWaitTimer -= afTimeStep;
	if (mfTitleGlitchWaitTimer <= 0 && mlTitleGlitchTimes <= 0)
	{
		mlTitleGlitchTimes = cMath::RandRectl(3, 5);
		mfTitleGlitchTimer = 0;
	}

	if (mlTitleGlitchTimes > 0)
	{
		mfTitleGlitchTimer -= afTimeStep;
		if (mfTitleGlitchTimer <= 0)
		{
			mlTitleGlitchPic = cMath::RandRectl(1, 4);
			--mlTitleGlitchTimes;
			mfTitleGlitchTimer = cMath::RandRectf(0.01f, 0.1f);

			if (mlTitleGlitchTimes <= 0)
			{
				mlTitleGlitchPic = 0;
				mfTitleGlitchWaitTimer = cMath::RandRectf(4.0f, 7.0f);
			}
		}
	}

	////////////////////////////////////
	// Real slow colour pulse ("TitlePulse" timer + "TitleColor" fade).
	mfTitlePulseTimer -= afTimeStep;
	if (mfTitlePulseTimer <= 0)
	{
		float fComponent = cMath::RandRectf(0.5f, 3.0f);
		mTitleColorStart = mTitleColorGoal;
		mTitleColorGoal = cColor(fComponent, cMath::Clamp(fComponent, 0.0f, 1.0f));
		mfTitleColorFadeLen = cMath::RandRectf(0.05f, 0.25f);
		mfTitleColorFadeT = 0;
		mfTitlePulseTimer = cMath::RandRectf(1.0f, 4.0f);
	}
	if (mfTitleColorFadeT < mfTitleColorFadeLen)
		mfTitleColorFadeT = cMath::Min(mfTitleColorFadeT + afTimeStep, mfTitleColorFadeLen);

	float fFadeAlpha = mfTitleColorFadeLen > 0 ? mfTitleColorFadeT / mfTitleColorFadeLen : 1.0f;
	cColor titleCol;
	titleCol.r = mTitleColorStart.r + (mTitleColorGoal.r - mTitleColorStart.r) * fFadeAlpha;
	titleCol.g = mTitleColorStart.g + (mTitleColorGoal.g - mTitleColorStart.g) * fFadeAlpha;
	titleCol.b = mTitleColorStart.b + (mTitleColorGoal.b - mTitleColorStart.b) * fFadeAlpha;
	titleCol.a = mTitleColorStart.a + (mTitleColorGoal.a - mTitleColorStart.a) * fFadeAlpha;

	cColor colorMul(1, mfTitleAlpha);

	cGuiGfxElement *pTitleGfx = mpTitleGfx;
	bool bGlitching = false;
	if (mlTitleGlitchPic > 0 && mpTitleFlickerGfx[mlTitleGlitchPic - 1])
	{
		pTitleGfx = mpTitleFlickerGfx[mlTitleGlitchPic - 1];
		bGlitching = true;
	}

	mpGuiSet->DrawGfx(pTitleGfx, cVector3f(kTitlePos.x, kTitlePos.y, 0.0f), kTitleSize, titleCol * colorMul);

	if (bGlitching)
	{
		cVector2f vGhostPos = kTitlePos + cVector2f(20.0f, 20.0f);
		mpGuiSet->DrawGfx(mpTitleGfx, cVector3f(vGhostPos.x, vGhostPos.y, 1.0f), kTitleSize, cColor(1.0f, 0.5f) * colorMul);
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::DrawMenuItems()
{
	for (int i = 0; i < 5; ++i)
	{
		cSomaMainMenuItem &item = mItems[i];
		cVector3f vPos(kMainMenuButtonPos.x, item.mfRowY, 1.0f);

		if (item.mbEnabled == false)
		{
			if (mpButtonFont)
				mpGuiSet->DrawFont(item.msLabel, mpButtonFont, vPos, cVector2f(36, 36), kDisabledColor, eFontAlign_Left);
			continue;
		}

		bool bSelected = (i == mlHoveredItem && mlClickedItem == -1) || i == mlClickedItem;

		if (bSelected)
		{
			cVector3f vBarPos(kMainMenuButtonPos.x - 22.0f, item.mfRowY, 0.5f);
			if (i == mlClickedItem)
			{
				// Real click-flash: a random jitter frame at full white,
				// only for the 0.15s "ButtonClicked" window.
				int lFrame = cMath::RandRectl(0, 2);
				if (mpButtonBarJitterGfx[lFrame])
					mpGuiSet->DrawGfx(mpButtonBarJitterGfx[lFrame], vBarPos, kOptionMenuButtonBgSize, cColor(1, 1));
			}
			else if (mpButtonBarGfx)
			{
				mpGuiSet->DrawGfx(mpButtonBarGfx, vBarPos, kOptionMenuButtonBgSize, kMainMenuButtonBgColor);
			}
		}

		if (mpButtonFont)
		{
			const cColor &textCol = bSelected ? kSelectedTextColor : kDeselectedTextColor;
			mpGuiSet->DrawFont(item.msLabel, mpButtonFont, vPos, cVector2f(36, 36), textCol, eFontAlign_Left);
		}
	}
}

//-----------------------------------------------------------------------
//
// Options screen
//
// Real script tree (script/modules/MenuHandler.hps): GuiOptions() lists
// Gameplay/Controls/Video/Audio/Back, each of which opens its own
// sub-screen (GuiOptionsVideo() further splits into Display/PostEffect/
// World/Gamma). This scaffold has a real, live backend for exactly two
// slices of that: master volume (cSound) and three video settings
// (fullscreen/vsync/gamma, via cLowLevelGraphics) - see SomaConfig.h. So
// the tree here is deliberately shallow: OptionsRoot lists only "AUDIO"/
// "DISPLAY"/"BACK" (real captions Menu.Audio/Menu.Display/Menu.Back -
// "Display" is the closest single real caption for a page that collapses
// the real Video tab's Display+Gamma sub-pages into one, since nothing
// here implements PostEffect/World). Everything real but not backed by a
// working setting yet (Controls/Gameplay, TextureQuality/ShadowQuality/
// SSAO/AA/refresh rate/resolution list, PS4/XBO speaker type, subtitles,
// FOV, ...) is left out entirely rather than drawn as a dead control -
// see the class comment in SomaMainMenu.h.
//
//-----------------------------------------------------------------------

void cSomaMainMenu::NavigateTo(eSomaMenuScreen aScreen)
{
	mScreen = aScreen;
	mlOptionsHoveredRow = -1;
	mlDraggingSliderRow = -1;
}

//-----------------------------------------------------------------------

void cSomaMainMenu::BuildOptionsRows()
{
	mOptionsRows.clear();

	cSomaConfig *pCfg = mpBase->GetConfig();

	switch (mScreen)
	{
	case eSomaMenuScreen_OptionsRoot:
		// Real GuiOptions(): OptionMenu_ButtonOptions("Audio"/... , kMainMenuButtonPos, id, ...)
		mOptionsRows.push_back(MakeCategoryRow(_W("AUDIO"), eSomaMenuScreen_OptionsAudio));
		mOptionsRows.push_back(MakeCategoryRow(_W("DISPLAY"), eSomaMenuScreen_OptionsDisplay));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_Main));
		break;

	case eSomaMenuScreen_OptionsAudio:
		// Real GuiOptionsAudio(): mpConfig.GetFloat("Sound","Volume",1.0f) via
		// OptionMenu_ButtonOptionsSlider("Volume", ..., 0.1f step, ...). This
		// scaffold has no SpeakerType/Subtitles/HearingAid backend, so only
		// Volume is ported (see the class comment above).
		mOptionsRows.push_back(MakeSliderRow(_W("VOLUME"), &pCfg->mfMasterVolume, 0.0f, 1.0f, 0.1f));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_OptionsRoot));
		break;

	case eSomaMenuScreen_OptionsDisplay:
		// Real GuiOptionsVideoDisplay()'s DisplayMode/VSync (collapsed to a
		// plain on/off toggle here - this engine has no live "borderless"
		// mode, and VSync's real "Adaptive" third state has no exposed
		// getter, see SomaConfig.h) plus GuiOptionsVideoGamma()'s Gamma
		// slider (same 0.3-2.0 range cSomaGammaScreen's first-boot
		// calibration already uses).
		mOptionsRows.push_back(MakeToggleRow(_W("FULLSCREEN"), &pCfg->mbFullscreen));
		mOptionsRows.push_back(MakeToggleRow(_W("V-SYNC"), &pCfg->mbVSync));
		mOptionsRows.push_back(MakeSliderRow(_W("GAMMA"), &pCfg->mfGamma, 0.3f, 2.0f, 0.05f));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_OptionsRoot));
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::UpdateOptionsMouseHitTest()
{
	mlOptionsHoveredRow = -1;

	// While a slider drag is in progress, keep it "hovered"/selected
	// regardless of where the mouse strays this frame (matches the real
	// script's ImGui_DoRepeatButtonExt(), which keeps a widget pressed
	// until the mouse button is released, not just while directly over it).
	if (mlDraggingSliderRow != -1)
	{
		mlOptionsHoveredRow = mlDraggingSliderRow;
		return;
	}

	const cVector2f &vMouse = mpGuiSet->GetMousePos();

	for (size_t i = 0; i < mOptionsRows.size(); ++i)
	{
		float fTop = kMainMenuButtonPos.y + kOptionMenuButtonSpacing * (float)i;
		float fBottom = fTop + kOptionMenuButtonSpacing;
		if (vMouse.x >= kMainMenuButtonPos.x && vMouse.x <= kVirtualCanvas.x && vMouse.y >= fTop && vMouse.y <= fBottom)
		{
			mlOptionsHoveredRow = (int)i;
			break;
		}
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::ClickOptionsRow(int alIndex)
{
	if (alIndex < 0 || alIndex >= (int)mOptionsRows.size())
		return;

	cSomaOptionsRow &row = mOptionsRows[alIndex];
	cSomaConfig *pCfg = mpBase->GetConfig();

	switch (row.mKind)
	{
	case cSomaOptionsRow::eKind_Category:
	case cSomaOptionsRow::eKind_Back:
		NavigateTo(row.mTarget);
		break;

	case cSomaOptionsRow::eKind_Toggle:
		if (row.mpBoolValue)
		{
			*row.mpBoolValue = !(*row.mpBoolValue);

			// Live-apply the ones that have a runtime API; Fullscreen is
			// persisted only (see SomaConfig.h) - applied at the next
			// InitEngine(), same "takes effect after restart" contract
			// amnesia/src/game/LuxMainMenu_Options.cpp's own Fullscreen
			// checkbox has.
			if (row.mpBoolValue == &pCfg->mbVSync)
				mpEngine->GetGraphics()->GetLowLevel()->SetVsyncActive(pCfg->mbVSync, false);
			else if (row.mpBoolValue == &pCfg->mbFullscreen)
				Log("SOMA options: Fullscreen changed to %s - takes effect on next launch\n",
					pCfg->mbFullscreen ? "true" : "false");

			pCfg->Save();
		}
		break;

	case cSomaOptionsRow::eKind_Slider:
	{
		if (row.mpFloatValue == NULL)
			break;

		// Real OptionMenu_ButtonOptionsSlider(): clicking inside the actual
		// track rect starts a direct-drag ("repeat button"); clicking
		// anywhere else in the row steps by afStepSize based on which half
		// of the track the mouse is nearer to.
		float fLocalX = mpGuiSet->GetMousePos().x - kMainMenuButtonPos.x;

		if (fLocalX >= kOptionsSliderTrackLocalMinX && fLocalX <= kOptionsSliderTrackLocalMaxX)
		{
			mlDraggingSliderRow = alIndex;
			UpdateOptionsSliderDrag();
		}
		else
		{
			float fNorm = (*row.mpFloatValue - row.mfMin) / (row.mfMax - row.mfMin);
			float fMid = (kOptionsSliderTrackLocalMinX + kOptionsSliderTrackLocalMaxX) * 0.5f;
			fNorm += (fLocalX < fMid) ? -row.mfStep : row.mfStep;
			fNorm = cMath::Clamp(fNorm, 0.0f, 1.0f);

			*row.mpFloatValue = row.mfMin + fNorm * (row.mfMax - row.mfMin);

			if (row.mpFloatValue == &pCfg->mfMasterVolume)
				mpEngine->GetSound()->GetLowLevel()->SetVolume(pCfg->mfMasterVolume);
			else if (row.mpFloatValue == &pCfg->mfGamma)
				mpEngine->GetGraphics()->GetLowLevel()->SetGammaCorrection(pCfg->mfGamma);

			pCfg->Save();
		}
		break;
	}
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::UpdateOptionsSliderDrag()
{
	if (mlDraggingSliderRow < 0 || mlDraggingSliderRow >= (int)mOptionsRows.size())
	{
		mlDraggingSliderRow = -1;
		return;
	}

	cSomaOptionsRow &row = mOptionsRows[mlDraggingSliderRow];
	if (row.mKind != cSomaOptionsRow::eKind_Slider || row.mpFloatValue == NULL)
	{
		mlDraggingSliderRow = -1;
		return;
	}

	float fLocalX = mpGuiSet->GetMousePos().x - kMainMenuButtonPos.x;
	float fNorm = (fLocalX - kOptionsSliderTrackLocalMinX) / (kOptionsSliderTrackLocalMaxX - kOptionsSliderTrackLocalMinX);
	fNorm = cMath::Clamp(fNorm, 0.0f, 1.0f);

	float fNewValue = row.mfMin + fNorm * (row.mfMax - row.mfMin);
	if (cMath::Abs(fNewValue - *row.mpFloatValue) < 0.0001f)
		return;

	*row.mpFloatValue = fNewValue;

	cSomaConfig *pCfg = mpBase->GetConfig();
	if (row.mpFloatValue == &pCfg->mfMasterVolume)
		mpEngine->GetSound()->GetLowLevel()->SetVolume(pCfg->mfMasterVolume);
	else if (row.mpFloatValue == &pCfg->mfGamma)
		mpEngine->GetGraphics()->GetLowLevel()->SetGammaCorrection(pCfg->mfGamma);

	pCfg->Save();
}

//-----------------------------------------------------------------------

void cSomaMainMenu::DrawOptionsPanel(const cVector2f &avPos, const cVector2f &avSize)
{
	// Real MenuHandler.hps's mGfxFrame, composited by hand (see
	// CreateOptionsGui()'s comment for why) - corners at native size in
	// each corner, borders stretched between them, a translucent fill
	// (real mGfxFrame.mGfxBackground.mColor) covering the interior.
	cVector2f vTL = mpFrameCornerTL ? mpFrameCornerTL->GetImageSize() : cVector2f(0);
	cVector2f vTR = mpFrameCornerTR ? mpFrameCornerTR->GetImageSize() : cVector2f(0);
	cVector2f vBL = mpFrameCornerBL ? mpFrameCornerBL->GetImageSize() : cVector2f(0);
	cVector2f vBR = mpFrameCornerBR ? mpFrameCornerBR->GetImageSize() : cVector2f(0);

	const float fZ = 0.0f;

	if (mpFrameFillGfx)
	{
		mpGuiSet->DrawGfx(mpFrameFillGfx, cVector3f(avPos.x, avPos.y, fZ), avSize, kOptionsFrameFillColor);
	}

	if (mpFrameCornerTL) mpGuiSet->DrawGfx(mpFrameCornerTL, cVector3f(avPos.x, avPos.y, fZ + 0.1f), vTL, cColor(1, 1));
	if (mpFrameCornerTR) mpGuiSet->DrawGfx(mpFrameCornerTR, cVector3f(avPos.x + avSize.x - vTR.x, avPos.y, fZ + 0.1f), vTR, cColor(1, 1));
	if (mpFrameCornerBL) mpGuiSet->DrawGfx(mpFrameCornerBL, cVector3f(avPos.x, avPos.y + avSize.y - vBL.y, fZ + 0.1f), vBL, cColor(1, 1));
	if (mpFrameCornerBR) mpGuiSet->DrawGfx(mpFrameCornerBR, cVector3f(avPos.x + avSize.x - vBR.x, avPos.y + avSize.y - vBR.y, fZ + 0.1f), vBR, cColor(1, 1));

	if (mpFrameBorderTop)
	{
		float fW = avSize.x - vTL.x - vTR.x;
		if (fW > 0)
			mpGuiSet->DrawGfx(mpFrameBorderTop, cVector3f(avPos.x + vTL.x, avPos.y, fZ + 0.1f), cVector2f(fW, mpFrameBorderTop->GetImageSize().y), cColor(1, 1));
	}
	if (mpFrameBorderBottom)
	{
		cVector2f vBorderSize = mpFrameBorderBottom->GetImageSize();
		float fW = avSize.x - vBL.x - vBR.x;
		if (fW > 0)
			mpGuiSet->DrawGfx(mpFrameBorderBottom, cVector3f(avPos.x + vBL.x, avPos.y + avSize.y - vBorderSize.y, fZ + 0.1f), cVector2f(fW, vBorderSize.y), cColor(1, 1));
	}
	if (mpFrameBorderLeft)
	{
		float fH = avSize.y - vTL.y - vBL.y;
		if (fH > 0)
			mpGuiSet->DrawGfx(mpFrameBorderLeft, cVector3f(avPos.x, avPos.y + vTL.y, fZ + 0.1f), cVector2f(mpFrameBorderLeft->GetImageSize().x, fH), cColor(1, 1));
	}
	if (mpFrameBorderRight)
	{
		cVector2f vBorderSize = mpFrameBorderRight->GetImageSize();
		float fH = avSize.y - vTR.y - vBR.y;
		if (fH > 0)
			mpGuiSet->DrawGfx(mpFrameBorderRight, cVector3f(avPos.x + avSize.x - vBorderSize.x, avPos.y + vTR.y, fZ + 0.1f), cVector2f(vBorderSize.x, fH), cColor(1, 1));
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::DrawOptionsRow(const cSomaOptionsRow &aRow, int alIndex, bool abSelected)
{
	float fRowY = kMainMenuButtonPos.y + kOptionMenuButtonSpacing * (float)alIndex;
	cVector3f vTextPos(kMainMenuButtonPos.x, fRowY, 2.0f);

	// Real OptionMenu_ButtonBackgroundOptions(): "startmenu_options_button_long"
	// tinted teal, behind whichever row is currently selected - used for
	// every row kind (category/toggle/slider/back), unlike the main menu's
	// own click-flash jitter set (not used here at all - the real script
	// doesn't add a "ButtonClicked" timer inside GuiOptions() either).
	if (abSelected && mpOptionsHighlightGfx)
	{
		cVector3f vBarPos(kMainMenuButtonPos.x - 22.0f, fRowY, 0.5f);
		mpGuiSet->DrawGfx(mpOptionsHighlightGfx, vBarPos, kOptionMenuButtonBgSize, kMainMenuButtonBgColor);
	}

	const cColor &textCol = abSelected ? kSelectedTextColor : kDeselectedTextColor;
	if (mpButtonFont)
		mpGuiSet->DrawFont(aRow.msLabel, mpButtonFont, vTextPos, cVector2f(36, 36), textCol, eFontAlign_Left);

	switch (aRow.mKind)
	{
	case cSomaOptionsRow::eKind_Category:
	case cSomaOptionsRow::eKind_Back:
		break;

	case cSomaOptionsRow::eKind_Toggle:
	{
		bool bChecked = aRow.mpBoolValue && *aRow.mpBoolValue;

		cVector3f vOffPos(kMainMenuButtonPos.x + kOptionsCheckboxOffset.x, fRowY + kOptionsCheckboxOffset.y, 1.5f);
		cVector3f vOnPos = vOffPos + cVector3f(kOptionsCheckboxSize.x * 0.5f + 5.0f, -0.5f, 0);
		cVector2f vBoxSize = kOptionsCheckboxSize * 0.5f;

		if (mpOptionsCheckOffGfx)
			mpGuiSet->DrawGfx(mpOptionsCheckOffGfx, vOffPos, vBoxSize, bChecked ? kMainMenuButtonBgColor : cColor(1, 1));
		if (mpOptionsCheckOnGfx)
			mpGuiSet->DrawGfx(mpOptionsCheckOnGfx, vOnPos, vBoxSize, bChecked ? cColor(1, 1) : kMainMenuButtonBgColor);

		if (mpButtonFont)
		{
			mpGuiSet->DrawFont(_W("OFF"), mpButtonFont, cVector3f(vOffPos.x, vOffPos.y, 2.0f), cVector2f(20, 20), cColor(0, 1), eFontAlign_Center);
			mpGuiSet->DrawFont(_W("ON"), mpButtonFont, cVector3f(vOnPos.x, vOnPos.y, 2.0f), cVector2f(20, 20), cColor(0, 1), eFontAlign_Center);
		}
		break;
	}

	case cSomaOptionsRow::eKind_Slider:
	{
		float fValue = aRow.mpFloatValue ? *aRow.mpFloatValue : 0;
		float fNorm = cMath::Clamp((fValue - aRow.mfMin) / (aRow.mfMax - aRow.mfMin), 0.0f, 1.0f);

		if (mpOptionsMeterGfx)
		{
			cVector3f vMeterPos(kMainMenuButtonPos.x + kOptionsSliderOffset.x, fRowY + kOptionsSliderOffset.y, 1.5f);
			mpGuiSet->DrawGfx(mpOptionsMeterGfx, vMeterPos, kOptionsSliderSize, kMainMenuButtonBgColor);
		}

		if (mpOptionsArrowGfx)
		{
			cColor arrowCol = abSelected ? cColor(1, 1) : cColor(0, 1);
			cVector3f vArrowL(kMainMenuButtonPos.x + kOptionsSliderArrowOffsetR.x, fRowY + kOptionsSliderArrowOffsetR.y, 2.0f);
			cVector3f vArrowR(kMainMenuButtonPos.x + kOptionsSliderArrowOffsetL.x, fRowY + kOptionsSliderArrowOffsetL.y, 2.0f);
			mpGuiSet->DrawGfx(mpOptionsArrowGfx, vArrowL, kOptionsSliderArrowSize, arrowCol, eGuiMaterial_LastEnum, 180.0f);
			mpGuiSet->DrawGfx(mpOptionsArrowGfx, vArrowR, kOptionsSliderArrowSize, arrowCol);
		}

		if (mpOptionsBarGfx)
		{
			cVector3f vTrackPos(kMainMenuButtonPos.x + kOptionsSliderBarOffset.x, fRowY + kOptionsSliderBarOffset.y, 2.0f);
			mpGuiSet->DrawGfx(mpOptionsBarGfx, vTrackPos, kOptionsSliderBarSize, cColor(0, 1));

			cVector3f vHandlePos = vTrackPos + cVector3f(kOptionsSliderBarSize.x * fNorm - 3.0f, -6.0f, 0.1f);
			mpGuiSet->DrawGfx(mpOptionsBarGfx, vHandlePos, cVector2f(6, 16), cColor(0, 1));
		}
		break;
	}
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::DrawOptionsScreen()
{
	// Panel sized to content - real per-screen kOptionsXxxBgSize constants
	// (kOptionsBgSize/kOptionsAudioBgSize/kOptionsVideoDisplayBgSize) are
	// all "row count * spacing + fixed padding" in the same way.
	float fPanelHeight = 60.0f + kOptionMenuButtonSpacing * (float)mOptionsRows.size();
	cVector2f vPanelSize(680, fPanelHeight);

	DrawOptionsPanel(kOptionsBgPos, vPanelSize);

	// Real OptionMenu_SectionTitle(asTitle, avPos, avSize) 3-arg overload:
	// right-aligned title text along the panel's own bottom edge.
	tWString sTitle;
	switch (mScreen)
	{
	case eSomaMenuScreen_OptionsAudio: sTitle = _W("AUDIO"); break;
	case eSomaMenuScreen_OptionsDisplay: sTitle = _W("DISPLAY"); break;
	default: sTitle = _W("OPTIONS"); break;
	}
	if (mpButtonFont)
	{
		cVector3f vTitlePos(kOptionsBgPos.x, kOptionsBgPos.y + vPanelSize.y + 5.0f, 2.0f);
		mpGuiSet->DrawFont(sTitle, mpButtonFont, vTitlePos, cVector2f(46, 46), cColor(1, 1), eFontAlign_Left);
	}

	for (size_t i = 0; i < mOptionsRows.size(); ++i)
	{
		bool bSelected = ((int)i == mlOptionsHoveredRow);
		DrawOptionsRow(mOptionsRows[i], (int)i, bSelected);
	}
}
