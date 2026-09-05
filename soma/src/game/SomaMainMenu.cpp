/*
 * See SomaMainMenu.h for scope notes and where these real values came from
 * (script/modules/MenuHandler.hps + helper_imgui_options.hps, read directly
 * out of SOMA's own shipped game data).
 */

#include "SomaMainMenu.h"
#include "SomaBase.h"
#include "SomaConfig.h"
#include "SomaMenuSfx.h"

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

static cSomaOptionsRow MakeCategoryRow(const tWString &asLabel, eSomaMenuScreen aTarget, bool abEnabled = true)
{
	cSomaOptionsRow row;
	row.mKind = cSomaOptionsRow::eKind_Category;
	row.msLabel = asLabel;
	row.mbEnabled = abEnabled;
	row.mTarget = aTarget;
	row.mpBoolValue = NULL;
	row.mpFloatValue = NULL;
	row.mfMin = row.mfMax = row.mfStep = 0;
	row.mlOptionIndex = 0;
	return row;
}

// Real but not-yet-backed category rows (Keybind/MouseOptions/GamepadOptions/
// AutoDetect - real OptionMenu_ButtonOptions() calls that open a rebinder UI
// or trigger a detect-settings popup, neither of which exists in this
// engine) - same row kind as a working category, just permanently disabled
// so it draws grayed and never navigates (see UpdateOptionsMouseHitTest()).
static cSomaOptionsRow MakeDisabledActionRow(const tWString &asLabel, eSomaMenuScreen aCurrentScreen)
{
	return MakeCategoryRow(asLabel, aCurrentScreen, false);
}

static cSomaOptionsRow MakeBackRow(eSomaMenuScreen aTarget)
{
	cSomaOptionsRow row = MakeCategoryRow(_W("BACK"), aTarget);
	row.mKind = cSomaOptionsRow::eKind_Back;
	return row;
}

static cSomaOptionsRow MakeToggleRow(const tWString &asLabel, bool *apValue, bool abEnabled = true)
{
	cSomaOptionsRow row;
	row.mKind = cSomaOptionsRow::eKind_Toggle;
	row.msLabel = asLabel;
	row.mbEnabled = abEnabled;
	row.mTarget = eSomaMenuScreen_Main;
	row.mpBoolValue = apValue;
	row.mpFloatValue = NULL;
	row.mfMin = row.mfMax = row.mfStep = 0;
	row.mlOptionIndex = 0;
	return row;
}

static cSomaOptionsRow MakeSliderRow(const tWString &asLabel, float *apValue, float afMin, float afMax, float afStep, bool abEnabled = true)
{
	cSomaOptionsRow row;
	row.mKind = cSomaOptionsRow::eKind_Slider;
	row.msLabel = asLabel;
	row.mbEnabled = abEnabled;
	row.mTarget = eSomaMenuScreen_Main;
	row.mpBoolValue = NULL;
	row.mpFloatValue = apValue;
	row.mfMin = afMin;
	row.mfMax = afMax;
	row.mfStep = afStep;
	row.mlOptionIndex = 0;
	return row;
}

// Real OptionMenu_ButtonOptionsMultiSelect() rows with no live backend in
// this engine at all (no texture/shadow/AA/resolution/language system) -
// always disabled, mlOptionIndex fixed at the real script's own default
// index (see BuildOptionsRows() call sites for which real default) so the
// displayed value matches what a fresh real install would actually show.
static cSomaOptionsRow MakeMultiSelectRow(const tWString &asLabel, const std::vector<tWString> &aOptions, int alDefaultIndex)
{
	cSomaOptionsRow row;
	row.mKind = cSomaOptionsRow::eKind_MultiSelect;
	row.msLabel = asLabel;
	row.mbEnabled = false;
	row.mTarget = eSomaMenuScreen_Main;
	row.mpBoolValue = NULL;
	row.mpFloatValue = NULL;
	row.mfMin = row.mfMax = row.mfStep = 0;
	row.mOptions = aOptions;
	row.mlOptionIndex = aOptions.empty() ? 0 : cMath::Clamp(alDefaultIndex, 0, (int)aOptions.size() - 1);
	return row;
}

//---------------------------------------

cSomaMainMenu::cSomaMainMenu(cEngine *apEngine, cSomaBase *apBase, cViewport *apViewport) : iUpdateable("SomaMainMenu")
{
	mpEngine = apEngine;
	mpBase = apBase;
	mpViewport = apViewport;

	mpBackgroundGfx = NULL;
	mpCursorGfx = NULL;
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

	// Real SOMA cursor - graphics/imgui/default/imgui_pointer_normal.tga.
	// Confirmed via a real install: no cursor-shaped asset exists anywhere
	// under graphics/startmenu/ (searched exhaustively) or the generic
	// gui/gui_default.skin this class otherwise reuses (that skin's own
	// "PointerNormal" is gui_def_pointer_normal.tga, a Dark Descent
	// placeholder cursor, not SOMA's) - the real one lives instead under
	// graphics/imgui/ (SOMA's own closed cImGui system's asset directory,
	// covered by the same "/graphics" AddSubDirs resources.cfg entry). A
	// real, plain 27x36 uncompressed-alpha TGA (not the mis-decoded
	// uncompressed-8bpp-alpha format that broke vera.fnt - see the class
	// comment above), so DevIL decodes it the same as every other TGA this
	// class already loads. cGuiSet::SetCurrentPointer() is this engine's
	// own existing API for a per-set custom cursor image (falls back to the
	// skin's PointerNormal otherwise, via SetSkin() - see GuiSet.cpp) - no
	// per-frame manual DrawGfx() hack needed.
	mpCursorGfx = CreateGfx("imgui_pointer_normal.tga", eGuiMaterial_Alpha);
	if (mpCursorGfx)
		mpGuiSet->SetCurrentPointer(mpCursorGfx);

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
	CreateParticleEmitters();

	// Real menu click/hover/glitch/sting sound effects - unlike Menu_Music.ogg
	// below, these are FMOD Studio/Designer-banked in the real install (see
	// SomaMenuSfx.cpp's top comment for the real background and how this
	// converts them into plain files this engine's sound backend can
	// already play). Idempotent/cheap after the first call, safe even if
	// the real install can't be found.
	cSomaMenuSfx::EnsureCached(mpEngine->GetResources());

	// Real menu music - "Menu_Music.ogg" ships as a plain OGG file (not
	// FMOD-banked like most of SOMA's other audio), directly playable
	// through this engine's existing OpenAL music backend with no extra
	// wiring - music/ is already a registered resources.cfg search dir.
	mpEngine->GetSound()->GetMusicHandler()->Play("Menu_Music.ogg", 1.0f, 0.5f, true, false);
}

//-----------------------------------------------------------------------

// Real Sound_PlayGui() call sites throughout script/modules/MenuHandler.hps/
// helper_imgui_options.hps all pass volume 1.0f and no looping - PlayGui()
// itself is a no-op if asFile is empty (a sample that failed to convert),
// so call sites below don't need their own empty checks.
static void PlaySomaMenuSfx(cEngine *apEngine, const tString &asFile)
{
	if (asFile.empty())
		return;
	apEngine->GetSound()->GetSoundHandler()->PlayGui(asFile, false, 1.0f);
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
//
// Real "ocean detritus floating over the menu" effect - see the class
// comment above cSomaMenuParticleEmitter in SomaMainMenu.h. Every constant
// below is copied straight out of the real DrawParticles() (script/modules/
// MenuHandler.hps) - spawn rect, velocity, size, life, colour - the only
// translation is real "OptionMenu_GetBotRightOffset(cVector2f(0,0), Z)"
// (screen-right-edge-at-depth-Z in the real script's own scaled coordinate
// space) becoming a plain x=kVirtualCanvas.x constant here, since this
// port's 1280x720 virtual canvas already *is* that same coordinate space
// (see the class comment at the top of this file) - and real
// "ImGui_NrmSize(0, t).y" (t as a fraction of screen height) becoming
// kVirtualCanvas.y * t.
//
//-----------------------------------------------------------------------

void cSomaMainMenu::CreateParticleEmitters()
{
	// Real "dust_light_tiny.dds"/"dust_cloud.dds" - reused from the actual
	// in-game world particle system (particles/dust/materials/), not
	// dedicated startmenu-only art; particles/ is already a registered
	// resources.cfg search dir.
	cGuiGfxElement *pDustLightTiny = CreateGfx("dust_light_tiny.dds", eGuiMaterial_Alpha);

	mEmitterLowerHalf.mvGfx.push_back(pDustLightTiny);
	mEmitterLowerHalf.mMaterial = eGuiMaterial_Alpha;
	mEmitterLowerHalf.mvMin = cVector3f(kVirtualCanvas.x, kVirtualCanvas.y * 0.5f, 13.0f);
	mEmitterLowerHalf.mvMax = cVector3f(kVirtualCanvas.x, kVirtualCanvas.y * 1.0f, 13.0f);
	mEmitterLowerHalf.mvVelocityMin = cVector3f(-10.0f, 0.0f, 0.0f);
	mEmitterLowerHalf.mvVelocityMax = cVector3f(-30.0f, 10.0f, 0.0f);
	mEmitterLowerHalf.mfSizeMin = 0.1f;
	mEmitterLowerHalf.mfSizeMax = 0.25f;
	mEmitterLowerHalf.mlMaxParticles = 100;
	mEmitterLowerHalf.mfParticlesPerSec = 10.0f;
	mEmitterLowerHalf.mfNewParticleTimer = 0.0f;
	mEmitterLowerHalf.mfMinLife = 15.0f;
	mEmitterLowerHalf.mfMaxLife = 35.0f;
	mEmitterLowerHalf.mColorStartMin = cColor(0.7f, 0.25f);
	mEmitterLowerHalf.mColorStartMax = cColor(0.7f, 0.75f);
	mEmitterLowerHalf.mColorMulStart = cColor(1, 1);
	mEmitterLowerHalf.mColorMulMiddle = cColor(1, 1);
	mEmitterLowerHalf.mColorMulEnd = cColor(1, 0);
	mEmitterLowerHalf.mfColorMulEndStartTime = 0.8f;

	mEmitterUpperHalf.mvGfx.push_back(pDustLightTiny);
	mEmitterUpperHalf.mMaterial = eGuiMaterial_Alpha;
	mEmitterUpperHalf.mvMin = cVector3f(kVirtualCanvas.x, kVirtualCanvas.y * 0.0f, 13.0f);
	mEmitterUpperHalf.mvMax = cVector3f(kVirtualCanvas.x, kVirtualCanvas.y * 0.5f, 13.0f);
	mEmitterUpperHalf.mvVelocityMin = cVector3f(-10.0f, 0.0f, 0.0f);
	mEmitterUpperHalf.mvVelocityMax = cVector3f(-30.0f, 10.0f, 0.0f);
	mEmitterUpperHalf.mfSizeMin = 0.1f;
	mEmitterUpperHalf.mfSizeMax = 0.25f;
	mEmitterUpperHalf.mlMaxParticles = 100;
	mEmitterUpperHalf.mfParticlesPerSec = 5.0f;
	mEmitterUpperHalf.mfNewParticleTimer = 0.0f;
	mEmitterUpperHalf.mfMinLife = 5.0f;
	mEmitterUpperHalf.mfMaxLife = 20.0f;
	mEmitterUpperHalf.mColorStartMin = cColor(0.7f, 0.25f);
	mEmitterUpperHalf.mColorStartMax = cColor(0.7f, 0.75f);
	mEmitterUpperHalf.mColorMulStart = cColor(1, 1);
	mEmitterUpperHalf.mColorMulMiddle = cColor(1, 1);
	mEmitterUpperHalf.mColorMulEnd = cColor(1, 0);
	mEmitterUpperHalf.mfColorMulEndStartTime = 0.8f;

	mEmitterLarge.mvGfx.push_back(pDustLightTiny);
	mEmitterLarge.mMaterial = eGuiMaterial_Alpha;
	mEmitterLarge.mvMin = cVector3f(kVirtualCanvas.x, kVirtualCanvas.y * 0.0f, 13.0f);
	mEmitterLarge.mvMax = cVector3f(kVirtualCanvas.x, kVirtualCanvas.y * 1.0f, 13.0f);
	mEmitterLarge.mvVelocityMin = cVector3f(-10.0f, 0.0f, 0.0f);
	mEmitterLarge.mvVelocityMax = cVector3f(-30.0f, 10.0f, 0.0f);
	mEmitterLarge.mfSizeMin = 0.3f;
	mEmitterLarge.mfSizeMax = 0.5f;
	mEmitterLarge.mlMaxParticles = 100;
	mEmitterLarge.mfParticlesPerSec = 2.0f;
	mEmitterLarge.mfNewParticleTimer = 0.0f;
	mEmitterLarge.mfMinLife = 5.0f;
	mEmitterLarge.mfMaxLife = 20.0f;
	mEmitterLarge.mColorStartMin = cColor(0.5f, 0.25f);
	mEmitterLarge.mColorStartMax = cColor(0.5f, 0.75f);
	mEmitterLarge.mColorMulStart = cColor(1, 1);
	mEmitterLarge.mColorMulMiddle = cColor(1, 1);
	mEmitterLarge.mColorMulEnd = cColor(1, 0);
	mEmitterLarge.mfColorMulEndStartTime = 0.8f;

	// Real mEmitterSmoke uses 4 UV-quadrant variants of the same
	// "dust_cloud.dds" atlas via cImGuiGfx.mvUVMin/mvUVMax - cGui's own
	// CreateGfxTexture(iTexture*, ...) overload takes the same start/end UV
	// pair directly. Real cImGuiParticleEmitter always draws gfx index 0
	// regardless (see cSomaMenuParticleEmitter's own comment in the header)
	// - all 4 are still created here to mirror the real class faithfully,
	// even though only the first is ever actually drawn, matching the real
	// game's own observed behaviour.
	iTexture *pDustCloudTex = mpEngine->GetResources()->GetTextureManager()->Create2D("dust_cloud.dds", true, eTextureType_2D);
	if (pDustCloudTex)
	{
		static const cVector2f kUvMins[4] = {cVector2f(0.0f, 0.0f), cVector2f(0.0f, 0.5f), cVector2f(0.5f, 0.0f), cVector2f(0.5f, 0.5f)};
		static const cVector2f kUvMaxs[4] = {cVector2f(0.5f, 0.5f), cVector2f(0.5f, 1.0f), cVector2f(1.0f, 0.5f), cVector2f(1.0f, 1.0f)};
		for (int i = 0; i < 4; ++i)
		{
			cGuiGfxElement *pQuadrant = mpGui->CreateGfxTexture(pDustCloudTex, false, eGuiMaterial_Additive, cColor(1, 1), true, kUvMins[i], kUvMaxs[i]);
			if (pQuadrant)
				mEmitterSmoke.mvGfx.push_back(pQuadrant);
		}
	}
	mEmitterSmoke.mMaterial = eGuiMaterial_Additive;
	mEmitterSmoke.mvMin = cVector3f(kVirtualCanvas.x, kVirtualCanvas.y * 0.0f, 13.0f);
	mEmitterSmoke.mvMax = cVector3f(kVirtualCanvas.x, kVirtualCanvas.y * 1.0f, 13.0f);
	mEmitterSmoke.mvVelocityMin = cVector3f(-20.0f, 0.0f, 0.0f);
	mEmitterSmoke.mvVelocityMax = cVector3f(-20.0f, 10.0f, 0.0f);
	mEmitterSmoke.mfSizeMin = 1.0f;
	mEmitterSmoke.mfSizeMax = 3.0f;
	mEmitterSmoke.mlMaxParticles = 50;
	mEmitterSmoke.mfParticlesPerSec = 0.5f;
	mEmitterSmoke.mfNewParticleTimer = 0.0f;
	mEmitterSmoke.mfMinLife = 30.0f;
	mEmitterSmoke.mfMaxLife = 100.0f;
	mEmitterSmoke.mColorStartMin = cColor(0.25f, 1.0f);
	mEmitterSmoke.mColorStartMax = cColor(0.5f, 1.0f);
	mEmitterSmoke.mColorMulStart = cColor(1, 1);
	mEmitterSmoke.mColorMulMiddle = cColor(0.5f, 0.5f);
	mEmitterSmoke.mColorMulEnd = cColor(0, 0);
	mEmitterSmoke.mfColorMulEndStartTime = 0.8f;

	// Real DrawParticles() runs each emitter's Update() 1200 times before
	// the first Draw() so the menu never opens on a completely empty sky -
	// particles are already mid-flight, scattered across their lifespans,
	// the first time this menu is shown. 1/60s per step is a plain,
	// reasonable stand-in for the real script's own per-frame afTimeStep
	// (unspecified/variable in the real code - only the "run it forward a
	// few thousand times" intent matters here, not an exact frame time).
	for (int i = 0; i < 1200; ++i)
	{
		UpdateParticleEmitter(mEmitterLowerHalf, 1.0f / 60.0f);
		UpdateParticleEmitter(mEmitterUpperHalf, 1.0f / 60.0f);
		UpdateParticleEmitter(mEmitterLarge, 1.0f / 60.0f);
		UpdateParticleEmitter(mEmitterSmoke, 1.0f / 60.0f);
	}
}

//-----------------------------------------------------------------------

static cVector3f RandRectVec3(const cVector3f &aMin, const cVector3f &aMax)
{
	return cVector3f(cMath::RandRectf(aMin.x, aMax.x), cMath::RandRectf(aMin.y, aMax.y), cMath::RandRectf(aMin.z, aMax.z));
}

void cSomaMainMenu::UpdateParticleEmitter(cSomaMenuParticleEmitter &aEmitter, float afTimeStep)
{
	aEmitter.mfNewParticleTimer -= afTimeStep;
	if (aEmitter.mfNewParticleTimer <= 0.0f && (int)aEmitter.mvParticles.size() < aEmitter.mlMaxParticles)
	{
		cSomaMenuParticle particle;
		particle.mvPos = RandRectVec3(aEmitter.mvMin, aEmitter.mvMax);
		particle.mvVel = RandRectVec3(aEmitter.mvVelocityMin, aEmitter.mvVelocityMax);
		particle.mfSize = cMath::RandRectf(aEmitter.mfSizeMin, aEmitter.mfSizeMax);
		particle.mfLife = 0.0f;

		float fLifeSecs = cMath::RandRectf(aEmitter.mfMinLife, aEmitter.mfMaxLife);
		particle.mfLifeStepMul = (fLifeSecs > 0.0f) ? (1.0f / fLifeSecs) : 1.0f;

		float fColorT = cMath::RandRectf(0.0f, 1.0f);
		particle.mStartColor = aEmitter.mColorStartMin * (1.0f - fColorT) + aEmitter.mColorStartMax * fColorT;

		aEmitter.mvParticles.push_back(particle);
		aEmitter.mfNewParticleTimer = (aEmitter.mfParticlesPerSec > 0.0f) ? (1.0f / aEmitter.mfParticlesPerSec) : 1.0f;
	}

	for (size_t i = 0; i < aEmitter.mvParticles.size();)
	{
		cSomaMenuParticle &particle = aEmitter.mvParticles[i];
		particle.mfLife += afTimeStep * particle.mfLifeStepMul;

		if (particle.mfLife >= 1.0f)
		{
			aEmitter.mvParticles.erase(aEmitter.mvParticles.begin() + i);
			continue;
		}

		particle.mvPos += particle.mvVel * afTimeStep;
		++i;
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::DrawParticleEmitter(cSomaMenuParticleEmitter &aEmitter, float afZ)
{
	if (aEmitter.mvGfx.empty())
		return;

	// Real cImGuiParticleEmitter::Draw() always uses gfx index 0 - see the
	// header's comment on cSomaMenuParticleEmitter::mvGfx.
	cGuiGfxElement *pGfx = aEmitter.mvGfx[0];
	cVector2f vNativeSize = pGfx->GetImageSize();

	for (size_t i = 0; i < aEmitter.mvParticles.size(); ++i)
	{
		const cSomaMenuParticle &particle = aEmitter.mvParticles[i];

		cColor colorMul;
		if (particle.mfLife < aEmitter.mfColorMulEndStartTime)
		{
			// Real script also has an intermediate "MulMiddleStartTime"
			// (default 0.25) segment; every real DrawParticles() emitter
			// leaves mfColorMulMiddleStartTime at its class default and
			// mColorMulStart==mColorMulMiddle for all 4 real emitters here,
			// so that first segment is always a no-op lerp between two
			// identical colours - collapsed to a flat mColorMulMiddle for
			// the whole pre-fade-out span rather than reproducing a lerp
			// that's always constant anyway.
			colorMul = aEmitter.mColorMulMiddle;
		}
		else
		{
			float fSpan = 1.0f - aEmitter.mfColorMulEndStartTime;
			float fT = (fSpan > 0.0f) ? (particle.mfLife - aEmitter.mfColorMulEndStartTime) / fSpan : 1.0f;
			colorMul = aEmitter.mColorMulMiddle * (1.0f - fT) + aEmitter.mColorMulEnd * fT;
		}

		cColor col = particle.mStartColor * colorMul;
		cVector2f vSize = vNativeSize * particle.mfSize;
		// Real ImGui_SetAlignment(eImGuiAlign_CenterCenter) - particle.mvPos
		// is the sprite's centre, not its top-left corner (same convention
		// DrawCathFacePart() already uses above).
		cVector3f vDrawPos(particle.mvPos.x - vSize.x * 0.5f, particle.mvPos.y - vSize.y * 0.5f, afZ);

		mpGuiSet->DrawGfx(pGfx, vDrawPos, vSize, col, aEmitter.mMaterial);
	}
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

	// Real DrawParticles() runs whenever mbMainMenuActive is true, which
	// covers the Options sub-tree too (an overlay on the main menu, not a
	// separate mode) - same "never goes away behind Options" rule
	// DrawBackground()/DrawTitle() already follow in OnDraw().
	UpdateParticleEmitter(mEmitterLowerHalf, afTimeStep);
	UpdateParticleEmitter(mEmitterUpperHalf, afTimeStep);
	UpdateParticleEmitter(mEmitterLarge, afTimeStep);
	UpdateParticleEmitter(mEmitterSmoke, afTimeStep);

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
	int lPrevHovered = mlHoveredItem;
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

	// Real OptionMenu_UpdateFocus(): plays frontend_menu_focus exactly when
	// a new item becomes focused (ImGui_PrevBecameInFocus()), not every
	// frame the mouse merely stays over one - same shared helper the real
	// script uses for both main menu items and Options rows.
	if (mlHoveredItem != -1 && mlHoveredItem != lPrevHovered)
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::FocusSound());
}

//-----------------------------------------------------------------------

void cSomaMainMenu::ClickItem(cSomaMainMenuItem &aItem)
{
	// Real OptionMenu_ButtonMainMenu()/GuiMainMenuSelection(): a click
	// starts a 0.15s "ButtonClicked" flash (jitter background) and only
	// performs the actual action once that timer elapses. Real
	// OptionMenu_ButtonMainMenu() plays frontend_menu_select on the click
	// itself (not frontend_menu_change - that's only for Options rows, see
	// ClickOptionsRow()).
	mlClickedItem = mlHoveredItem;
	mfButtonClickedTimer = 0.15f;
	mPendingAction = aItem.mAction;
	PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::SelectSound());
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
		// Real ClickNewGame(): "New game sting" plays right when a fresh
		// install's New Game is confirmed (no save exists, so no
		// confirmation box is needed - same as here).
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::NewGameSting());
		SetVisible(false);
		break;
	}
	case eSomaMainMenuAction_Options:
		// Real menu's full tree is Gameplay/Controls/Video{Display,
		// PostEffect,World,Gamma}/Audio (eMainMenuGroup_Options*) - now
		// reproduced in full, see BuildOptionsRows().
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

	// Real DrawParticles() is the very last thing drawn each frame (called
	// after GuiBackground()/GuiOptions()/etc in MenuHandler.hps), on top of
	// everything else including the dirt-corner vignette - matched here by
	// using z=13, above the corners' own z=12.5 (see DrawBackground()).
	DrawParticleEmitter(mEmitterLowerHalf, 13.0f);
	DrawParticleEmitter(mEmitterUpperHalf, 13.0f);
	DrawParticleEmitter(mEmitterLarge, 13.0f);
	DrawParticleEmitter(mEmitterSmoke, 13.0f);
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

		// Real GuiBackground(): "menu_glitch" plays once per glitch burst
		// (when the "TitleGlitch" timer is (re)started), not once per
		// individual flicker frame - the real event has 12 layered wave
		// variants FMOD itself picks between; this port just picks one.
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::GlitchSound(cMath::RandRectl(1, cSomaMenuSfx::GlitchSoundCount())));
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

	// Display-only defaults for disabled toggle/slider rows below - these
	// mirror the real script's own mpConfig.GetBool/GetFloat(...) default
	// argument (see MenuHandler.hps's GuiOptionsGameplay()/
	// GuiOptionsVideoPostEffect()/GuiOptionsVideoWorld()/GuiOptionsAudio()),
	// not a live setting - the row is disabled, so nothing ever writes back
	// to these.
	static bool bShowHints = true;
	static bool bScreenDistortion = true;
	static bool bColorSeparation = true;
	static bool bCrosshairSimple = false;
	static bool bSubtitles = true;
	static bool bHearingAid = false;
	static bool bSSAO = true;
	static bool bBloom = true;
	static bool bReflection = true;
	static bool bRefraction = true;
	static float fFOV = 70.0f;

	switch (mScreen)
	{
	case eSomaMenuScreen_OptionsRoot:
		// Real GuiOptions(): Gameplay/Controls/Video/Audio/Back, in that
		// order (real id gap at 4 - Video and PS4/XBO's "Gamma" alias share
		// index 2 in the real script - not reproduced, this engine is never
		// PS4/XBO). Every one of these has *some* real content on the other
		// side (even if some of it is entirely disabled rows, e.g. Controls/
		// Gameplay), so all four are real, enabled, navigable categories.
		mOptionsRows.push_back(MakeCategoryRow(_W("GAME"), eSomaMenuScreen_OptionsGameplay));
		mOptionsRows.push_back(MakeCategoryRow(_W("CONTROLS"), eSomaMenuScreen_OptionsControls));
		mOptionsRows.push_back(MakeCategoryRow(_W("VIDEO"), eSomaMenuScreen_OptionsVideo));
		mOptionsRows.push_back(MakeCategoryRow(_W("AUDIO"), eSomaMenuScreen_OptionsAudio));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_Main));
		break;

	case eSomaMenuScreen_OptionsGameplay:
		// Real GuiOptionsGameplay() - none of these have a backend in this
		// engine (no language switcher, no hint system, no screen-distortion/
		// colour-separation post effect, no crosshair-style setting), so
		// every row here is disabled.
		mOptionsRows.push_back(MakeMultiSelectRow(_W("LANGUAGE"), {_W("ENGLISH")}, 0));
		mOptionsRows.push_back(MakeToggleRow(_W("HINTS"), &bShowHints, false));
		mOptionsRows.push_back(MakeToggleRow(_W("VIDEO DISTORTION EFFECTS"), &bScreenDistortion, false));
		mOptionsRows.push_back(MakeToggleRow(_W("COLOR SEPARATION"), &bColorSeparation, false));
		mOptionsRows.push_back(MakeToggleRow(_W("SIMPLE INTERACTION ICONS"), &bCrosshairSimple, false));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_OptionsRoot));
		break;

	case eSomaMenuScreen_OptionsControls:
		// Real GuiOptionsInput() top level - Keybind/MouseOptions/
		// GamepadOptions (EyeTracking omitted: real script only shows it
		// when EyeTracking_IsAvailable(), never true here). No keybinding/
		// mouse-sensitivity/gamepad backend exists in this engine at all, so
		// all three are disabled rather than navigating to an empty screen.
		mOptionsRows.push_back(MakeDisabledActionRow(_W("KEYBINDINGS"), eSomaMenuScreen_OptionsControls));
		mOptionsRows.push_back(MakeDisabledActionRow(_W("MOUSE OPTIONS"), eSomaMenuScreen_OptionsControls));
		mOptionsRows.push_back(MakeDisabledActionRow(_W("CONTROLLER OPTIONS"), eSomaMenuScreen_OptionsControls));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_OptionsRoot));
		break;

	case eSomaMenuScreen_OptionsVideo:
		// Real GuiOptionsVideo(): AutoDetect/Display/PostEffect/Rendering/
		// Gamma/Back. AutoDetect just pops a "detect best settings" message
		// box in the real game - no detection logic exists here, disabled.
		mOptionsRows.push_back(MakeDisabledActionRow(_W("AUTO DETECT SETTINGS"), eSomaMenuScreen_OptionsVideo));
		mOptionsRows.push_back(MakeCategoryRow(_W("DISPLAY"), eSomaMenuScreen_OptionsVideoDisplay));
		mOptionsRows.push_back(MakeCategoryRow(_W("POST EFFECT"), eSomaMenuScreen_OptionsVideoPostEffect));
		mOptionsRows.push_back(MakeCategoryRow(_W("RENDERING"), eSomaMenuScreen_OptionsVideoWorld));
		mOptionsRows.push_back(MakeCategoryRow(_W("GAMMA"), eSomaMenuScreen_OptionsVideoGamma));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_OptionsRoot));
		break;

	case eSomaMenuScreen_OptionsVideoDisplay:
	{
		// Real GuiOptionsVideoDisplay(): Resolution/DisplayMode/VSync/
		// RefreshRate/AA/FOV/Back. DisplayMode and VSync are the two rows
		// with a real backend (cSomaConfig::mbFullscreen/mbVSync) - real
		// DisplayMode is a 3-way Fullscreen/Windowed/Borderless multi-select
		// and real VSync is On/Adaptive/Off, both collapsed to a 2-state
		// toggle here since this engine only has a bool for each (no
		// borderless window mode, no adaptive-vsync getter - see
		// SomaConfig.h). Resolution/RefreshRate/AA/FOV have no backend at
		// all (no resolution-switching, no AA, no FOV/projection control).
		const cVector2l &vScreenSize = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeInt();
		tWString sResolution = cString::ToStringW(vScreenSize.x) + _W("x") + cString::ToStringW(vScreenSize.y);
		mOptionsRows.push_back(MakeMultiSelectRow(_W("RESOLUTION"), {sResolution}, 0));
		mOptionsRows.push_back(MakeToggleRow(_W("DISPLAY MODE"), &pCfg->mbFullscreen));
		mOptionsRows.push_back(MakeToggleRow(_W("V-SYNC"), &pCfg->mbVSync));
		mOptionsRows.push_back(MakeMultiSelectRow(_W("REFRESH RATE"), {_W("AUTO")}, 0));
		mOptionsRows.push_back(MakeMultiSelectRow(_W("ANTI-ALIASING"), {_W("OFF"), _W("FXAA")}, 1));
		mOptionsRows.push_back(MakeSliderRow(_W("HORIZONTAL FOV"), &fFOV, 50.0f, 83.0f, 0.05f, false));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_OptionsVideo));
		break;
	}

	case eSomaMenuScreen_OptionsVideoPostEffect:
		// Real GuiOptionsVideoPostEffect() - no depth-of-field/SSAO/bloom
		// post effects exist in this engine's SOMA renderer yet, all disabled.
		mOptionsRows.push_back(MakeMultiSelectRow(_W("DEPTH OF FIELD"), {_W("LOW"), _W("MEDIUM"), _W("HIGH")}, 2));
		mOptionsRows.push_back(MakeToggleRow(_W("SSAO"), &bSSAO, false));
		mOptionsRows.push_back(MakeToggleRow(_W("BLOOM"), &bBloom, false));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_OptionsVideo));
		break;

	case eSomaMenuScreen_OptionsVideoWorld:
		// Real GuiOptionsVideoWorld() (captioned "Rendering" in the real
		// menu) - no texture-quality/filtering/shadow-quality/reflection/
		// refraction settings exist in this engine's SOMA renderer yet.
		mOptionsRows.push_back(MakeMultiSelectRow(_W("TEXTURE QUALITY"), {_W("HIGH"), _W("MEDIUM"), _W("LOW")}, 0));
		mOptionsRows.push_back(MakeMultiSelectRow(_W("TEXTURE FILTER"), {_W("BILINEAR"), _W("TRILINEAR"), _W("AFx2"), _W("AFx4"), _W("AFx8"), _W("AFx16")}, 0));
		mOptionsRows.push_back(MakeMultiSelectRow(_W("SHADOW QUALITY"), {_W("OFF"), _W("LOW"), _W("MEDIUM"), _W("HIGH"), _W("VERY HIGH")}, 3));
		mOptionsRows.push_back(MakeToggleRow(_W("REFLECTION"), &bReflection, false));
		mOptionsRows.push_back(MakeToggleRow(_W("REFRACTION"), &bRefraction, false));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_OptionsVideo));
		break;

	case eSomaMenuScreen_OptionsVideoGamma:
		// Real GuiOptionsVideoGamma() - the one real Video sub-screen that's
		// just a single slider, same live backend cSomaGammaScreen's
		// first-boot calibration already uses.
		mOptionsRows.push_back(MakeSliderRow(_W("GAMMA"), &pCfg->mfGamma, 0.3f, 2.0f, 0.05f));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_OptionsVideo));
		break;

	case eSomaMenuScreen_OptionsAudio:
		// Real GuiOptionsAudio(): SpeakerType (PS4/XBO only, never shown
		// here)/Volume/Subtitles/HearingAid/Back. Volume is the only row
		// with a real backend (cSound); this engine has no subtitle
		// rendering or closed-caption system at all yet.
		mOptionsRows.push_back(MakeSliderRow(_W("VOLUME"), &pCfg->mfMasterVolume, 0.0f, 1.0f, 0.1f));
		mOptionsRows.push_back(MakeToggleRow(_W("SUBTITLES"), &bSubtitles, false));
		mOptionsRows.push_back(MakeToggleRow(_W("CLOSED CAPTION"), &bHearingAid, false));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_OptionsRoot));
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::UpdateOptionsMouseHitTest()
{
	int lPrevHovered = mlOptionsHoveredRow;
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
		if (mOptionsRows[i].mbEnabled == false)
			continue; // real-but-unbacked row - never hoverable/clickable, drawn grayed only

		float fTop = kMainMenuButtonPos.y + kOptionMenuButtonSpacing * (float)i;
		float fBottom = fTop + kOptionMenuButtonSpacing;
		if (vMouse.x >= kMainMenuButtonPos.x && vMouse.x <= kVirtualCanvas.x && vMouse.y >= fTop && vMouse.y <= fBottom)
		{
			mlOptionsHoveredRow = (int)i;
			break;
		}
	}

	// Real OptionMenu_UpdateFocus() - same shared focus-change sound the
	// main menu's own items use (see UpdateMouseHitTest()).
	if (mlOptionsHoveredRow != -1 && mlOptionsHoveredRow != lPrevHovered)
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::FocusSound());
}

//-----------------------------------------------------------------------

void cSomaMainMenu::ClickOptionsRow(int alIndex)
{
	if (alIndex < 0 || alIndex >= (int)mOptionsRows.size())
		return;

	cSomaOptionsRow &row = mOptionsRows[alIndex];
	cSomaConfig *pCfg = mpBase->GetConfig();

	if (row.mbEnabled == false)
		return; // real-but-unbacked row - UpdateOptionsMouseHitTest() already keeps these unhoverable, but guard anyway

	switch (row.mKind)
	{
	case cSomaOptionsRow::eKind_MultiSelect:
		// No real multi-select row is ever built with mbEnabled true yet
		// (see MakeMultiSelectRow()), so this is unreachable today - kept so
		// the day one of these gets a real backend, wiring it in here is a
		// one-line addition rather than a new switch case.
		break;

	case cSomaOptionsRow::eKind_Category:
	case cSomaOptionsRow::eKind_Back:
		// Real OptionMenu_ButtonOptions(): plays frontend_menu_change on
		// click (distinct from the main menu's own frontend_menu_select -
		// see ClickItem()).
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::ChangeSound());
		NavigateTo(row.mTarget);
		break;

	case cSomaOptionsRow::eKind_Toggle:
		if (row.mpBoolValue)
		{
			*row.mpBoolValue = !(*row.mpBoolValue);

			// Real OptionMenu_ButtonOptionsToggle(): plays frontend_menu_select
			// on click (helper_imgui_options.hps).
			PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::SelectSound());

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

			// Real OptionMenu_ButtonOptionsSlider(): plays frontend_menu_slider
			// whenever a click-to-step actually changes the value.
			PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::SliderSound());

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

	// Real OptionMenu_ButtonOptionsSlider()'s repeat-button/drag branch:
	// plays frontend_menu_slider every frame the dragged value actually
	// changes (matches the real script's own per-frame behaviour here).
	PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::SliderSound());

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

	// Real-but-unbacked rows (mbEnabled false) are never selected (see
	// UpdateOptionsMouseHitTest()) and always draw with the same disabled
	// grey the main menu's own Continue/LoadGame labels use, per the user's
	// ask to mark these clearly rather than omit them.
	cColor textCol = kDeselectedTextColor;
	if (aRow.mbEnabled == false)
		textCol = kDisabledColor;
	else if (abSelected)
		textCol = kSelectedTextColor;

	if (mpButtonFont)
		mpGuiSet->DrawFont(aRow.msLabel, mpButtonFont, vTextPos, cVector2f(36, 36), textCol, eFontAlign_Left);

	// Widget tint for disabled rows - same dark grey as the label, applied
	// in place of the normal on/highlighted colour so the checkbox/slider/
	// value text reads as inert at a glance.
	const cColor &widgetOnCol = aRow.mbEnabled ? kMainMenuButtonBgColor : kDisabledColor;
	const cColor widgetArrowCol = aRow.mbEnabled ? (abSelected ? cColor(1, 1) : cColor(0, 1)) : kDisabledColor;

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
			mpGuiSet->DrawGfx(mpOptionsCheckOffGfx, vOffPos, vBoxSize, bChecked ? widgetOnCol : cColor(1, 1));
		if (mpOptionsCheckOnGfx)
			mpGuiSet->DrawGfx(mpOptionsCheckOnGfx, vOnPos, vBoxSize, bChecked ? cColor(1, 1) : widgetOnCol);

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
			mpGuiSet->DrawGfx(mpOptionsMeterGfx, vMeterPos, kOptionsSliderSize, widgetOnCol);
		}

		if (mpOptionsArrowGfx)
		{
			cVector3f vArrowL(kMainMenuButtonPos.x + kOptionsSliderArrowOffsetR.x, fRowY + kOptionsSliderArrowOffsetR.y, 2.0f);
			cVector3f vArrowR(kMainMenuButtonPos.x + kOptionsSliderArrowOffsetL.x, fRowY + kOptionsSliderArrowOffsetL.y, 2.0f);
			mpGuiSet->DrawGfx(mpOptionsArrowGfx, vArrowL, kOptionsSliderArrowSize, widgetArrowCol, eGuiMaterial_LastEnum, 180.0f);
			mpGuiSet->DrawGfx(mpOptionsArrowGfx, vArrowR, kOptionsSliderArrowSize, widgetArrowCol);
		}

		if (mpOptionsBarGfx)
		{
			cVector3f vTrackPos(kMainMenuButtonPos.x + kOptionsSliderBarOffset.x, fRowY + kOptionsSliderBarOffset.y, 2.0f);
			mpGuiSet->DrawGfx(mpOptionsBarGfx, vTrackPos, kOptionsSliderBarSize, cColor(0, 1));

			cVector3f vHandlePos = vTrackPos + cVector3f(kOptionsSliderBarSize.x * fNorm - 3.0f, -6.0f, 0.1f);
			mpGuiSet->DrawGfx(mpOptionsBarGfx, vHandlePos, cVector2f(6, 16), aRow.mbEnabled ? cColor(0, 1) : kDisabledColor);
		}
		break;
	}

	case cSomaOptionsRow::eKind_MultiSelect:
	{
		// Real OptionMenu_ButtonOptionsMultiSelect(): value text centred in
		// the same track rect a slider uses, with an arrow either side -
		// every row of this kind is built disabled (see MakeMultiSelectRow()),
		// so this always renders the grey/inert look.
		if (mpOptionsMeterGfx)
		{
			cVector3f vMeterPos(kMainMenuButtonPos.x + kOptionsSliderOffset.x, fRowY + kOptionsSliderOffset.y, 1.5f);
			mpGuiSet->DrawGfx(mpOptionsMeterGfx, vMeterPos, kOptionsSliderSize, widgetOnCol);
		}

		if (mpOptionsArrowGfx)
		{
			cVector3f vArrowL(kMainMenuButtonPos.x + kOptionsSliderArrowOffsetR.x, fRowY + kOptionsSliderArrowOffsetR.y, 2.0f);
			cVector3f vArrowR(kMainMenuButtonPos.x + kOptionsSliderArrowOffsetL.x, fRowY + kOptionsSliderArrowOffsetL.y, 2.0f);
			mpGuiSet->DrawGfx(mpOptionsArrowGfx, vArrowL, kOptionsSliderArrowSize, widgetArrowCol, eGuiMaterial_LastEnum, 180.0f);
			mpGuiSet->DrawGfx(mpOptionsArrowGfx, vArrowR, kOptionsSliderArrowSize, widgetArrowCol);
		}

		if (mpButtonFont && aRow.mlOptionIndex >= 0 && aRow.mlOptionIndex < (int)aRow.mOptions.size())
		{
			cVector3f vValuePos(kMainMenuButtonPos.x + kOptionsSliderOffset.x + kOptionsSliderSize.x * 0.5f,
								 fRowY + kOptionsSliderOffset.y + kOptionsSliderSize.y * 0.5f, 2.0f);
			mpGuiSet->DrawFont(aRow.mOptions[aRow.mlOptionIndex], mpButtonFont, vValuePos, cVector2f(24, 24), kDisabledColor, eFontAlign_Center);
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
	cVector2f vPanelSize(760, fPanelHeight);

	DrawOptionsPanel(kOptionsBgPos, vPanelSize);

	// Real OptionMenu_SectionTitle(asTitle, avPos, avSize) 3-arg overload:
	// right-aligned title text along the panel's own bottom edge.
	tWString sTitle;
	switch (mScreen)
	{
	case eSomaMenuScreen_OptionsGameplay: sTitle = _W("GAME"); break;
	case eSomaMenuScreen_OptionsControls: sTitle = _W("CONTROLS"); break;
	case eSomaMenuScreen_OptionsVideo: sTitle = _W("VIDEO"); break;
	case eSomaMenuScreen_OptionsVideoDisplay: sTitle = _W("DISPLAY"); break;
	case eSomaMenuScreen_OptionsVideoPostEffect: sTitle = _W("POST EFFECT"); break;
	case eSomaMenuScreen_OptionsVideoWorld: sTitle = _W("RENDERING"); break;
	case eSomaMenuScreen_OptionsVideoGamma: sTitle = _W("GAMMA"); break;
	case eSomaMenuScreen_OptionsAudio: sTitle = _W("AUDIO"); break;
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
