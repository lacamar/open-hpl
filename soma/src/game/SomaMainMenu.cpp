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

// Real kOptionMenu_CheckboxOffset/Size - no longer used to draw an actual
// on/off checkbox pair (see the eKind_Toggle comment in SomaMainMenu.h: the
// real widget is the same cycle-bar every other row uses), kept only because
// eKind_Toggle/eKind_MultiSelect share kOptionsSliderOffset/Size below for
// that cycle bar's own position - nothing here references these two anymore.

// Real kOptionMenu_SliderOffset/Size - the cycle-bar background rect shared
// by eKind_Toggle/eKind_Slider/eKind_MultiSelect alike (real
// OptionMenu_OptionsToggle()/OptionMenu_OptionsSlider()/
// OptionMenu_OptionsMultiSelect() all draw "startmenu_options_button_meter"
// at this exact same offset/size regardless of kind).
static const cVector2f kOptionsSliderOffset(305, 2);
static const cVector2f kOptionsSliderSize(368, 46);
static const float kOptionsSliderTrackLocalMinX = 325; // relative to row pos.x
static const float kOptionsSliderTrackLocalMaxX = 570;
static const cVector2f kOptionsSliderBarOffset(325, 16);
static const cVector2f kOptionsSliderBarSize(245, 5);
static const cVector2f kOptionsSliderArrowOffsetL(570, -1);
static const cVector2f kOptionsSliderArrowOffsetR(305, -2);
static const cVector2f kOptionsSliderArrowSize(20, 40);

// Real kOptionMenu_TextedSlider* (helper_imgui_options.hps) - used only when
// a slider row has a non-empty value string to show after it (real script:
// "asTextValue.length() > 0"), which today is FOV alone (Gamma/Volume both
// pass "" for asTextValue in MenuHandler.hps and use the plain kOptionsSlider*
// constants above unchanged). Same overall bar rect as the plain slider, but
// the track/right-arrow stop short at x=510 instead of x=570 to leave room
// for the trailing number before the bar's own right-edge notch.
static const float kOptionsTextedSliderArrowRightX = 510; // real kOptionMenu_TextedSliderArrowOffsetL.x
static const cVector2f kOptionsTextedSliderBarOffset(325, 16);
static const cVector2f kOptionsTextedSliderBarSize(185, 5);
static const cVector2f kOptionsTextedSliderTextOffset(530, 16); // real kOptionMenu_TextedSliderTextOffset

// Real mGfxFrame.mGfxBackground.mColor (MenuHandler.hps's Init()).
static const cColor kOptionsFrameFillColor(5.0f / 255.0f, 60.0f / 255.0f, 72.0f / 255.0f, 0.25f);

//---------------------------------------
// Real GuiGameModeSelection() layout constants, verbatim from
// MenuHandler.hps (kGameModeBgPos/kGameModeBgSize/kGameModeArrowPosLeft/
// kGameModeArrowPosRight/kGameModeArrowSize) - see
// DrawNewGameDifficultyScreen(). The GameMode row itself, the StartGame
// button (real row index 4) and Back button (real row index 5) all still
// reuse kMainMenuButtonPos/kOptionMenuButtonSpacing above, same as every
// other real screen in this file.

static const cVector2f kGameModeBgPos(100, 260);
static const cVector2f kGameModeBgSize(663, 260);

// Dark overlay drawn over the live 3D scene while paused - see
// DrawPauseBackground()'s comment in SomaMainMenu.h for why this is a
// solid, high-alpha dark fill rather than a lightly-translucent tint over
// the paused scene (this port's own viewport renders solid white, not the
// paused scene, once cSomaPlayer::SetActive(false) takes effect - confirmed
// live by testing alpha 0/0.65/1.0 in turn - so a low-alpha tint would show
// that white through, not a dim view of the game).
static const cColor kPauseBgDarkenColor(0.0f, 0.0f, 0.0f, 0.9f);

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
	row.mOptionId = cSomaOptionsRow::eOptionId_None;
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

// asOnLabel/asOffLabel default to the real generic "On"/"Off" captions
// (config/base_english.lang) every toggle-shaped row except Display Mode
// uses; Display Mode passes the real "Fullscreen"/"Windowed" pair instead
// (see BuildOptionsRows()).
static cSomaOptionsRow MakeToggleRow(const tWString &asLabel, bool *apValue, bool abEnabled = true,
									  const tWString &asOnLabel = _W("ON"), const tWString &asOffLabel = _W("OFF"))
{
	cSomaOptionsRow row;
	row.mKind = cSomaOptionsRow::eKind_Toggle;
	row.msLabel = asLabel;
	row.mbEnabled = abEnabled;
	row.mTarget = eSomaMenuScreen_Main;
	row.mpBoolValue = apValue;
	row.mpFloatValue = NULL;
	row.mfMin = row.mfMax = row.mfStep = 0;
	// Real cycle-bar widget, see the eKind_Toggle comment in SomaMainMenu.h -
	// mOptions/mlOptionIndex are reused from eKind_MultiSelect rather than
	// duplicating them.
	row.mOptions.push_back(asOffLabel);
	row.mOptions.push_back(asOnLabel);
	row.mlOptionIndex = (apValue && *apValue) ? 1 : 0;
	row.mOptionId = cSomaOptionsRow::eOptionId_None;
	return row;
}

// asValueText: real OptionMenu_ButtonOptionsSlider()'s optional "asTextValue"
// - empty for every slider except FOV (see cSomaOptionsRow::mSliderValueText).
static cSomaOptionsRow MakeSliderRow(const tWString &asLabel, float *apValue, float afMin, float afMax, float afStep, bool abEnabled = true,
									  const tWString &asValueText = _W(""))
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
	row.mSliderValueText = asValueText;
	row.mlOptionIndex = 0;
	row.mOptionId = cSomaOptionsRow::eOptionId_None;
	return row;
}

// Real OptionMenu_ButtonOptionsMultiSelect() rows. abEnabled/aOptionId default
// to "no live backend" (disabled, mlOptionIndex fixed at the real script's
// own default index - see BuildOptionsRows() call sites for which real
// default) for the settings this engine still can't act on at all
// (RefreshRate/TextureQuality/TextureFilter/ShadowQuality/Language/
// DepthOfField) - Resolution/AntiAliasing pass abEnabled=true and their real
// eOptionId explicitly (see ClickOptionsRow()'s eKind_MultiSelect case).
static cSomaOptionsRow MakeMultiSelectRow(const tWString &asLabel, const std::vector<tWString> &aOptions, int alDefaultIndex,
										   bool abEnabled = false, cSomaOptionsRow::eOptionId aOptionId = cSomaOptionsRow::eOptionId_None)
{
	cSomaOptionsRow row;
	row.mKind = cSomaOptionsRow::eKind_MultiSelect;
	row.msLabel = asLabel;
	row.mbEnabled = abEnabled;
	row.mTarget = eSomaMenuScreen_Main;
	row.mpBoolValue = NULL;
	row.mpFloatValue = NULL;
	row.mfMin = row.mfMax = row.mfStep = 0;
	row.mOptions = aOptions;
	row.mlOptionIndex = aOptions.empty() ? 0 : cMath::Clamp(alDefaultIndex, 0, (int)aOptions.size() - 1);
	row.mOptionId = aOptionId;
	return row;
}

// Real OptionMenu_ButtonKeybind() row - asKeyName is the action's current
// bound key display string (cSomaBase::GetPlayerActionKeyName()), rebuilt
// fresh every BuildOptionsRows() call like everything else here so a
// just-completed rebind shows up immediately on the very next frame.
static cSomaOptionsRow MakeKeybindRow(const tWString &asLabel, cSomaBase::eSomaPlayerAction aAction, const tWString &asKeyName)
{
	cSomaOptionsRow row;
	row.mKind = cSomaOptionsRow::eKind_Keybind;
	row.msLabel = asLabel;
	row.mbEnabled = true;
	row.mTarget = eSomaMenuScreen_Main;
	row.mpBoolValue = NULL;
	row.mpFloatValue = NULL;
	row.mfMin = row.mfMax = row.mfStep = 0;
	row.mSliderValueText = asKeyName;
	row.mlOptionIndex = (int)aAction;
	row.mOptionId = cSomaOptionsRow::eOptionId_None;
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
	mbPaused = false;

	mbShowExitConfirm = false;
	mbExitConfirmSaveAndExit = false;
	mlExitConfirmHovered = -1;
	mpMsgBoxButtonLeftGfx = mpMsgBoxButtonLeftActiveGfx = NULL;
	mpMsgBoxButtonRightGfx = mpMsgBoxButtonRightActiveGfx = NULL;

	mpBodyFont = NULL;
	mlNewGameMode = 0;
	mlNewGameHoveredControl = eSomaNewGameControl_None;

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
	mlAwaitingKeybindRow = -1;

	mpFrameCornerTL = mpFrameCornerTR = mpFrameCornerBL = mpFrameCornerBR = NULL;
	mpFrameBorderTop = mpFrameBorderBottom = mpFrameBorderLeft = mpFrameBorderRight = NULL;
	mpFrameFillGfx = NULL;
	mpOptionsHighlightGfx = mpOptionsMeterGfx = mpOptionsArrowGfx = NULL;
	mpOptionsBarGfx = NULL;
	mpOptionsToggleOnGfx = mpOptionsToggleOffGfx = NULL;

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
	BuildResolutionList();

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

	// Real GuiGameModeSelection() description body font (Sansation Large,
	// non-bold - see DrawNewGameDifficultyScreen()).
	mpBodyFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("sansation_large.fnt");

	// Real exit-confirm dialog Yes/No buttons (helper_imgui_options.hps'
	// OptionMenu_MessageBox_Proper()) - see DrawExitConfirmDialog().
	mpMsgBoxButtonLeftGfx = CreateGfx("startmenu_options_msgbox_button_left.tga", eGuiMaterial_Alpha);
	mpMsgBoxButtonLeftActiveGfx = CreateGfx("startmenu_options_msgbox_button_left_active.tga", eGuiMaterial_Alpha);
	mpMsgBoxButtonRightGfx = CreateGfx("startmenu_options_msgbox_button_right.tga", eGuiMaterial_Alpha);
	mpMsgBoxButtonRightActiveGfx = CreateGfx("startmenu_options_msgbox_button_right_active.tga", eGuiMaterial_Alpha);

	BuildMainMenuItems();
}

//-----------------------------------------------------------------------

void cSomaMainMenu::BuildMainMenuItems()
{
	////////////////////////////////////
	// Real menu item list/order/captions (MainMenu.Continue/NewGame/
	// LoadGame/Options/Exit in config/base_english.lang) and real
	// enable rule: with no save system in this scaffold, mbCanContinue is
	// always false, same as a real fresh install - Continue/LoadGame show
	// as disabled labels rather than buttons (GuiMainMenuSelection()).
	mItems.clear();
	mItems.resize(5);

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

	for (size_t i = 0; i < mItems.size(); ++i)
		mItems[i].mfRowY = kMainMenuButtonPos.y + kOptionMenuButtonSpacing * (float)i;
}

//-----------------------------------------------------------------------

// Paused-mode item list (see ShowPaused()) - real GuiPauseMenuSelection()
// (script/modules/MenuHandler.hps), confirmed by reading it directly: real
// item order/captions are RETURN TO THE GAME(0)/OPTIONS(1)/EXIT(2)/SAVE AND
// EXIT(3), all real config/base_english.lang "Menu" category captions -
// replaces an earlier pass's placeholder RESUME/OPTIONS/QUIT TO MAIN MENU
// 3-item list (real precedent for THAT list never existed - it was modeled
// on Dark Descent's own pause menu instead of SOMA's real one). Real EXIT
// and SAVE AND EXIT both route through the same confirm dialog - see
// eSomaMainMenuAction_PauseExit/PauseSaveAndExit and
// DrawExitConfirmDialog(). SaveAndExit is real-but-conditionally-disabled
// in the real game (mbSaveLoadEnabled/Map_IsChanging()/InIntro checks,
// MenuHandler.hps:2701-2725) - this port has no such conditions to check
// (no save system, no map-transition/intro-lock state this menu tracks), so
// it's always enabled here.
void cSomaMainMenu::BuildPausedMenuItems()
{
	mItems.clear();
	mItems.resize(4);

	mItems[0].msLabel = _W("RETURN TO THE GAME");
	mItems[0].mbEnabled = true;
	mItems[0].mAction = eSomaMainMenuAction_Resume;

	mItems[1].msLabel = _W("OPTIONS");
	mItems[1].mbEnabled = true;
	mItems[1].mAction = eSomaMainMenuAction_Options;

	mItems[2].msLabel = _W("EXIT");
	mItems[2].mbEnabled = true;
	mItems[2].mAction = eSomaMainMenuAction_PauseExit;

	mItems[3].msLabel = _W("SAVE AND EXIT");
	mItems[3].mbEnabled = true;
	mItems[3].mAction = eSomaMainMenuAction_PauseSaveAndExit;

	for (size_t i = 0; i < mItems.size(); ++i)
		mItems[i].mfRowY = kMainMenuButtonPos.y + kOptionMenuButtonSpacing * (float)i;
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
	// Real "startmenu_options_button_on/off.tga" - the genuinely distinct
	// on/off switch widget eKind_Toggle uses (see the eKind_Toggle comment in
	// SomaMainMenu.h and DrawOptionsToggleControl()) - an earlier pass here
	// concluded these were unused/wrong and skipped loading them; confirmed
	// wrong by reading helper_imgui_options.hps's OptionMenu_ButtonOptionsToggle()
	// directly, which calls a distinct OptionMenu_OptionsCheckbox() function,
	// not the shared cycle-bar eKind_MultiSelect still correctly uses below.
	mpOptionsToggleOnGfx = CreateGfx("startmenu_options_button_on.tga", eGuiMaterial_Alpha);
	mpOptionsToggleOffGfx = CreateGfx("startmenu_options_button_off.tga", eGuiMaterial_Alpha);
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

		// Real script/modules/MenuHandler.hps stops a SECOND, separate
		// looping ambient here too (Sound_Stop("MenuBGNoise", ...), source
		// special_fx/frontend/main_menu_bg) - previously never stopped by
		// this port at all (cSomaSplash::EnterPhase() starts it fire-and-
		// forget), so it kept looping under any map loaded from the menu.
		// See cSomaSplash::StopMenuAmbient()'s own comment for the fix.
		if (mpBase && mpBase->GetSplash())
			mpBase->GetSplash()->StopMenuAmbient();
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
	// DrawBackground()/DrawTitle() already follow in OnDraw(). Never runs
	// while paused (mbMainMenuActive false) - see OnDraw()'s own comment.
	if (mbPaused == false)
	{
		UpdateParticleEmitter(mEmitterLowerHalf, afTimeStep);
		UpdateParticleEmitter(mEmitterUpperHalf, afTimeStep);
		UpdateParticleEmitter(mEmitterLarge, afTimeStep);
		UpdateParticleEmitter(mEmitterSmoke, afTimeStep);
	}

	if (mbPaused && mbShowExitConfirm)
	{
		// Real mbShowExit overlay - suppresses all normal item-list
		// hit-testing/clicking underneath it while shown (see
		// UpdateExitConfirmDialog()).
		UpdateExitConfirmDialog(bDown, bPressedEdge);
	}
	else if (mScreen == eSomaMenuScreen_Main)
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
	else if (mScreen == eSomaMenuScreen_NewGameDifficulty)
	{
		UpdateNewGameDifficultyMouseHitTest();

		if (bPressedEdge && mlNewGameHoveredControl != eSomaNewGameControl_None)
			ClickNewGameDifficultyControl(mlNewGameHoveredControl);
	}
	else
	{
		// Options screen - see BuildOptionsRows()/UpdateOptionsMouseHitTest()/
		// ClickOptionsRow() below. No 0.15s click-flash delay here: the real
		// script's OptionMenu_ButtonOptions() (unlike OptionMenu_
		// ButtonMainMenu()) acts immediately on click.
		BuildOptionsRows();

		// Real OptionMenu_ButtonKeybind() capture mode: while waiting for a
		// key press to bind, every other mouse hit-test/click/drag is
		// suppressed (matches the real script's own kKeybindFocusSlot
		// exclusivity) - only UpdateKeybindCapture() below runs.
		if (mlAwaitingKeybindRow != -1)
		{
			UpdateKeybindCapture();
		}
		else
		{
			UpdateOptionsMouseHitTest();

			if (bPressedEdge && mlOptionsHoveredRow != -1)
				ClickOptionsRow(mlOptionsHoveredRow);

			if (bDown && mlDraggingSliderRow != -1)
				UpdateOptionsSliderDrag();

			if (bDown == false)
				mlDraggingSliderRow = -1;
		}
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
	for (size_t i = 0; i < mItems.size(); ++i)
	{
		if (mItems[i].mbEnabled == false)
			continue;

		float fTop = mItems[i].mfRowY;
		float fBottom = fTop + kOptionMenuButtonSpacing;
		if (vMouse.x >= kMainMenuButtonPos.x && vMouse.x <= kVirtualCanvas.x && vMouse.y >= fTop && vMouse.y <= fBottom)
		{
			mlHoveredItem = (int)i;
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
		// Real GuiMainMenuSelection() case 1 (NewGame): opens the
		// difficulty-select screen (GuiGameModeSelection()) whenever
		// cLux_GetSupportExplorationMode() is true - true on every real SOMA
		// release, so this port always takes that branch - rather than
		// starting the game directly. StartNewGame() itself (see below) now
		// only runs from that screen's own START GAME button
		// (ClickNewGameDifficultyControl()), not from here anymore.
		mlNewGameMode = 0; // real SetMenuActive(true)'s mlSelectedGameMode = 0 reset
		mlNewGameHoveredControl = eSomaNewGameControl_None;
		NavigateTo(eSomaMenuScreen_NewGameDifficulty);
		break;
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

	case eSomaMainMenuAction_Resume:
		// Routed through cSomaBase::SetGameplayPaused() (rather than calling
		// HidePaused() directly here) so the player controller's SetActive()
		// call and this menu's own hide both happen from the same single
		// place - see SomaBase.cpp.
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::SelectSound());
		if (mpBase)
			mpBase->SetGameplayPaused(false);
		break;

	case eSomaMainMenuAction_PauseExit:
		// Real GuiPauseMenuSelection() case 2: sets mbShowExit (real
		// msMessageBoxFocus default "No") - the actual quit-to-menu action
		// only happens once the confirm dialog's YES is clicked, see
		// UpdateExitConfirmDialog().
		mbShowExitConfirm = true;
		mbExitConfirmSaveAndExit = false;
		mlExitConfirmHovered = 1; // real default focus "No"
		break;

	case eSomaMainMenuAction_PauseSaveAndExit:
		// Real GuiPauseMenuSelection() case 3: same mbShowExit dialog as
		// PauseExit above, just mbSaveAndExit=true too (picks the
		// "...EXIT TO MENU?" wording instead of "...WITHOUT SAVING?" and
		// attempts Game_AutoSave() first on a real install - see
		// UpdateExitConfirmDialog()'s own comment for why this port can't do
		// that last part).
		mbShowExitConfirm = true;
		mbExitConfirmSaveAndExit = true;
		mlExitConfirmHovered = 1;
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------

// Real precedent (Dark Descent's eLuxAction_Exit, LuxInputHandler.cpp)
// fully swaps back to the title-screen container/scene. This scaffold has
// no such scene-swap plumbing for a live gameplay map (would need cSomaBase
// to unload the current map, reload main_init.cfg's <MainMenu> scene into
// mpDebugViewport, and switch mpDebugCameraController back on - a real
// restructure, out of scope here per this class's own file-ownership
// boundary). Closest safe approximation: just re-show the full title-screen
// item list over the CURRENT gameplay map, deliberately leaving the player
// controller inactive (same as while paused - not reactivated here, unlike
// the Resume case in RunPendingAction()) so nothing moves behind the menu.
// Called once the real EXIT/SAVE AND EXIT confirm dialog's YES is clicked
// (UpdateExitConfirmDialog()) - both real actions land here in this port,
// see that function's own comment for the honest SAVE AND EXIT limitation.
void cSomaMainMenu::DoQuitToMainMenu()
{
	PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::ChangeSound());
	mbPaused = false;
	mbShowExitConfirm = false;
	BuildMainMenuItems();
	mScreen = eSomaMenuScreen_Main;
	SetVisible(true);
}

//-----------------------------------------------------------------------

void cSomaMainMenu::ShowPaused()
{
	if (mbPaused)
		return; // already showing - matches SetVisible()'s own idempotency

	mbPaused = true;
	BuildPausedMenuItems();
	mScreen = eSomaMenuScreen_Main;
	mlHoveredItem = -1;
	mlClickedItem = -1;
	mfButtonClickedTimer = 0;
	mPendingAction = eSomaMainMenuAction_None;

	mbShowExitConfirm = false;
	mbExitConfirmSaveAndExit = false;
	mlExitConfirmHovered = -1;

	// Same gui-activation calls SetVisible(true) makes, deliberately without
	// its Menu_Music.ogg swap - pausing shouldn't cut off whatever's already
	// playing in-game.
	mbVisible = true;
	mpGuiSet->SetActive(true);
	mpGuiSet->SetDrawMouse(true);
	mpGui->SetFocus(mpGuiSet);
}

//-----------------------------------------------------------------------

void cSomaMainMenu::HidePaused()
{
	if (mbPaused == false)
		return;

	mbPaused = false;
	mbVisible = false;
	mpGuiSet->SetActive(false);
	mpGuiSet->SetDrawMouse(false);
	if (mpGui->GetFocusedSet() == mpGuiSet)
		mpGui->SetFocus(NULL);
}

//-----------------------------------------------------------------------

void cSomaMainMenu::OnDraw(float afFrameTime)
{
	if (mbVisible == false)
		return;

	if (mbPaused)
	{
		// Real GuiBackground() draws NOTHING while paused (mbMainMenuActive
		// false) - no menu_background.tga, no title, no cathedral-face/
		// particles either (see DrawPauseBackground()'s own comment in
		// SomaMainMenu.h) - the earlier pass this replaced wrongly kept
		// drawing the full title-screen chrome behind the reduced pause item
		// list, which is what actually read as "weird"/wrong here.
		DrawPauseBackground();
	}
	else
	{
		// Background/title never go away behind the Options screen - matches
		// the real game (GuiOptions() etc. are drawn as an overlay on top of
		// GuiBackground(), never a scene replacement).
		DrawBackground(afFrameTime);
		DrawTitle(afFrameTime);
	}

	if (mScreen == eSomaMenuScreen_Main)
	{
		DrawMenuItems();
	}
	else if (mScreen == eSomaMenuScreen_NewGameDifficulty)
	{
		DrawNewGameDifficultyScreen();
	}
	else
	{
		BuildOptionsRows();
		DrawOptionsScreen();
	}

	if (mbPaused && mbShowExitConfirm)
		DrawExitConfirmDialog();

	// Real DrawParticles() only ever runs while mbMainMenuActive (see
	// DrawPauseBackground()'s comment) - never drawn while paused, same as
	// DrawBackground()/DrawTitle() above. Last thing drawn each frame
	// otherwise (called after GuiBackground()/GuiOptions()/etc in
	// MenuHandler.hps), on top of everything else including the dirt-corner
	// vignette - matched here by using z=13, above the corners' own z=12.5
	// (see DrawBackground()).
	if (mbPaused == false)
	{
		DrawParticleEmitter(mEmitterLowerHalf, 13.0f);
		DrawParticleEmitter(mEmitterUpperHalf, 13.0f);
		DrawParticleEmitter(mEmitterLarge, 13.0f);
		DrawParticleEmitter(mEmitterSmoke, 13.0f);
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
	for (size_t i = 0; i < mItems.size(); ++i)
	{
		cSomaMainMenuItem &item = mItems[i];
		cVector3f vPos(kMainMenuButtonPos.x, item.mfRowY, 1.0f);

		if (item.mbEnabled == false)
		{
			if (mpButtonFont)
				mpGuiSet->DrawFont(item.msLabel, mpButtonFont, vPos, cVector2f(36, 36), kDisabledColor, eFontAlign_Left);
			continue;
		}

		bool bSelected = ((int)i == mlHoveredItem && mlClickedItem == -1) || (int)i == mlClickedItem;

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
// Pause background - see the class comment on DrawPauseBackground() in
// SomaMainMenu.h for the real precedent/limitation this follows.
//
//-----------------------------------------------------------------------

void cSomaMainMenu::DrawPauseBackground()
{
	// Plain translucent dark overlay directly over the still-live-rendering
	// 3D scene - see this function's own comment in SomaMainMenu.h for why
	// (a real framebuffer-capture-and-tint was tried and abandoned: came
	// back solid white in live testing, this engine's frame loop has no
	// well-defined point to read back "the last composited frame" from).
	if (mpFrameFillGfx)
		mpGuiSet->DrawGfx(mpFrameFillGfx, cVector3f(0, 0, -10.0f), kVirtualCanvas, kPauseBgDarkenColor);
}

//-----------------------------------------------------------------------
//
// Real EXIT/SAVE AND EXIT confirm dialog - see the class comment on
// DrawExitConfirmDialog() in SomaMainMenu.h.
//
//-----------------------------------------------------------------------

void cSomaMainMenu::DrawExitConfirmDialog()
{
	// Real OptionMenu_MessageBox_Proper(): a full-screen darken first
	// (helper_imgui_options.hps' cColor(0,0.75)), THEN the dialog frame.
	if (mpFrameFillGfx)
		mpGuiSet->DrawGfx(mpFrameFillGfx, cVector3f(0, 0, 5.0f), kVirtualCanvas, cColor(0, 0.75f));

	cVector2f vSize(560, 200);
	cVector2f vPos((kVirtualCanvas.x - vSize.x) * 0.5f, (kVirtualCanvas.y - vSize.y) * 0.5f);

	// Real same corner/border frame set (graphics/startmenu/gfx/window/
	// menu_*.tga) the Options panel uses - confirmed distinct from a
	// dedicated messagebox frame by comparing background colours.
	DrawOptionsPanel(vPos, vSize);

	// Real base_english.lang "Menu" category strings - see
	// MessageBoxExitFromPauseMenu() in script/modules/MenuHandler.hps and
	// PORTING_NOTES.md for the full citation.
	tWString sMessage = mbExitConfirmSaveAndExit
		? _W("ARE YOU SURE YOU WANT TO EXIT TO MENU?")
		: _W("ARE YOU SURE YOU WANT TO EXIT WITHOUT SAVING?");

	if (mpButtonFont)
	{
		tWStringVec vRows;
		cVector2f vFontSize(28, 28);
		mpButtonFont->GetWordWrapRows(vSize.x - 60.0f, vFontSize.y, vFontSize, sMessage, &vRows);

		float fTextTop = vPos.y + 36.0f;
		for (size_t i = 0; i < vRows.size(); ++i)
		{
			cVector3f vRowPos(vPos.x + vSize.x * 0.5f, fTextTop + (float)i * (vFontSize.y + 6.0f), 6.0f);
			mpGuiSet->DrawFont(vRows[i], mpButtonFont, vRowPos, vFontSize, cColor(1, 1), eFontAlign_Center);
		}
	}

	// Real "startmenu_options_msgbox_button_left/right(_active).tga" - Yes
	// is the LEFT button, No the right (OptionMenu_MessageBox_Proper(text,
	// "Yes","No",...)).
	bool bYesHovered = (mlExitConfirmHovered == 0);
	bool bNoHovered = (mlExitConfirmHovered == 1);

	cGuiGfxElement *pYesGfx = bYesHovered ? mpMsgBoxButtonLeftActiveGfx : mpMsgBoxButtonLeftGfx;
	cGuiGfxElement *pNoGfx = bNoHovered ? mpMsgBoxButtonRightActiveGfx : mpMsgBoxButtonRightGfx;

	// Real "startmenu_options_msgbox_button_left/right.tga" are plain
	// untextured white shapes (same convention as this file's own
	// mpOptionsToggleOnGfx/OffGfx - see DrawOptionsToggleControl()'s
	// comment), so the caption needs a dark colour to actually read against
	// them - white-on-white was invisible here in live testing.
	const cColor kMsgBoxButtonTextCol(0, 0, 0, 1);

	float fButtonY = vPos.y + vSize.y - 70.0f;
	if (pYesGfx)
	{
		cVector2f vBtnSize = pYesGfx->GetImageSize();
		cVector3f vBtnPos(vPos.x + vSize.x * 0.5f - vBtnSize.x - 10.0f, fButtonY, 6.0f);
		mpGuiSet->DrawGfx(pYesGfx, vBtnPos, vBtnSize, cColor(1, 1));
		if (mpButtonFont)
			mpGuiSet->DrawFont(_W("Yes"), mpButtonFont, cVector3f(vBtnPos.x + vBtnSize.x * 0.5f, vBtnPos.y + (vBtnSize.y - 28.0f) * 0.5f, 7.0f),
								cVector2f(28, 28), kMsgBoxButtonTextCol, eFontAlign_Center);
	}
	if (pNoGfx)
	{
		cVector2f vBtnSize = pNoGfx->GetImageSize();
		cVector3f vBtnPos(vPos.x + vSize.x * 0.5f + 10.0f, fButtonY, 6.0f);
		mpGuiSet->DrawGfx(pNoGfx, vBtnPos, vBtnSize, cColor(1, 1));
		if (mpButtonFont)
			mpGuiSet->DrawFont(_W("No"), mpButtonFont, cVector3f(vBtnPos.x + vBtnSize.x * 0.5f, vBtnPos.y + (vBtnSize.y - 28.0f) * 0.5f, 7.0f),
								cVector2f(28, 28), kMsgBoxButtonTextCol, eFontAlign_Center);
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::UpdateExitConfirmDialog(bool abMouseDown, bool abPressedEdge)
{
	int lPrevHovered = mlExitConfirmHovered;
	mlExitConfirmHovered = -1;

	const cVector2f &vMouse = mpGuiSet->GetMousePos();
	cVector2f vSize(560, 200);
	cVector2f vPos((kVirtualCanvas.x - vSize.x) * 0.5f, (kVirtualCanvas.y - vSize.y) * 0.5f);
	float fButtonY = vPos.y + vSize.y - 70.0f;

	if (mpMsgBoxButtonLeftGfx)
	{
		cVector2f vBtnSize = mpMsgBoxButtonLeftGfx->GetImageSize();
		cVector2f vBtnPos(vPos.x + vSize.x * 0.5f - vBtnSize.x - 10.0f, fButtonY);
		if (vMouse.x >= vBtnPos.x && vMouse.x <= vBtnPos.x + vBtnSize.x && vMouse.y >= vBtnPos.y && vMouse.y <= vBtnPos.y + vBtnSize.y)
			mlExitConfirmHovered = 0;
	}
	if (mlExitConfirmHovered == -1 && mpMsgBoxButtonRightGfx)
	{
		cVector2f vBtnSize = mpMsgBoxButtonRightGfx->GetImageSize();
		cVector2f vBtnPos(vPos.x + vSize.x * 0.5f + 10.0f, fButtonY);
		if (vMouse.x >= vBtnPos.x && vMouse.x <= vBtnPos.x + vBtnSize.x && vMouse.y >= vBtnPos.y && vMouse.y <= vBtnPos.y + vBtnSize.y)
			mlExitConfirmHovered = 1;
	}

	if (mlExitConfirmHovered != -1 && mlExitConfirmHovered != lPrevHovered)
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::FocusSound());

	if (abPressedEdge == false || mlExitConfirmHovered == -1)
		return;

	if (mlExitConfirmHovered == 0) // Yes
	{
		if (mbExitConfirmSaveAndExit)
		{
			// Honest scope limitation (see SomaMainMenu.h/PORTING_NOTES.md):
			// HPL2/core has a generic iSaveObject/iSaveData serialization
			// framework and Dark Descent has a concrete cLuxSaveHandler built
			// on it, but nothing SOMA-specific exists in soma/src/game yet -
			// so SAVE AND EXIT performs the exact same close action as EXIT,
			// without actually saving anything.
			Log("SOMA main menu: SAVE AND EXIT confirmed - no SOMA save system exists in this engine yet, behaving identically to EXIT (nothing saved)\n");
		}
		DoQuitToMainMenu();
	}
	else // No
	{
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::ChangeSound());
		mbShowExitConfirm = false;
	}
}

//-----------------------------------------------------------------------
//
// Real GuiGameModeSelection() difficulty-select screen - see the class
// comment on eSomaMenuScreen_NewGameDifficulty in SomaMainMenu.h.
//
//-----------------------------------------------------------------------

void cSomaMainMenu::DrawNewGameDifficultyScreen()
{
	DrawOptionsPanel(kGameModeBgPos, kGameModeBgSize);

	// Real OptionMenu_SectionTitle("NewGame", ...) - same title-placement
	// convention DrawOptionsScreen() uses for its own real per-screen titles.
	if (mpButtonFont)
	{
		cVector3f vTitlePos(kGameModeBgPos.x, kGameModeBgPos.y + kGameModeBgSize.y + 5.0f, 2.0f);
		mpGuiSet->DrawFont(_W("NEW GAME"), mpButtonFont, vTitlePos, cVector2f(46, 46), cColor(1, 1), eFontAlign_Left);
	}

	float fModeRowY = kMainMenuButtonPos.y;
	bool bModeHovered = (mlNewGameHoveredControl == eSomaNewGameControl_LeftArrow || mlNewGameHoveredControl == eSomaNewGameControl_RightArrow);

	if (mpButtonFont)
		mpGuiSet->DrawFont(_W("GAME MODE:"), mpButtonFont, cVector3f(kMainMenuButtonPos.x, fModeRowY, 2.0f), cVector2f(36, 36), cColor(1, 1), eFontAlign_Left);

	// Real "NormalMode"/"ExplorationMode" captions ("NORMAL"/"SAFE") - reuses
	// this file's existing real cycle-bar widget (same
	// "startmenu_options_button_meter"/"startmenu_options_arrow" assets the
	// Options screen's own eKind_MultiSelect rows use) rather than hand-
	// rolling the real script's own bespoke GuiGameModeSelection() arrow
	// layout a second time - see SomaMainMenu.h's comment on this screen.
	tWString sModeLabel = (mlNewGameMode == 0) ? _W("NORMAL") : _W("SAFE");
	cColor barCol = bModeHovered ? kMainMenuButtonBgColor : (kMainMenuButtonBgColor * 0.7f);
	DrawOptionsCycleControl(fModeRowY, sModeLabel, barCol, cColor(1, 1), cColor(0, 0, 0, 1));

	// Real mode descriptions (base_english.lang "NormalModeDescription"/
	// "ExplorationModeDescription") - wrapped with the real non-bold
	// "Sansation Large" body font (see mpBodyFont's comment in SomaMainMenu.h).
	tWString sDesc = (mlNewGameMode == 0)
		? _W("Monsters are dangerous and can kill you. You need to think and sneak to survive. The way the game was designed from the start.")
		: _W("Monsters are still creepy, but can't kill you. You don't need to worry about stealth as you play.");

	if (mpBodyFont)
	{
		tWStringVec vRows;
		cVector2f vDescFontSize(24, 24);
		float fMaxWidth = kGameModeBgSize.x - 40.0f;
		mpBodyFont->GetWordWrapRows(fMaxWidth, vDescFontSize.y, vDescFontSize, sDesc, &vRows);

		for (size_t i = 0; i < vRows.size(); ++i)
		{
			cVector3f vRowPos(kMainMenuButtonPos.x, fModeRowY + 48.0f + (float)i * (vDescFontSize.y + 4.0f), 2.0f);
			mpGuiSet->DrawFont(vRows[i], mpBodyFont, vRowPos, vDescFontSize, cColor(1, 1), eFontAlign_Left);
		}
	}

	// Real row indices 4 (StartGame, main-menu-style button) and 5 (Back,
	// options-style button) - see GuiGameModeSelection()'s own
	// OptionMenu_ButtonMainMenu("StartGame", kMainMenuButtonPos, 4, ...) /
	// OptionMenu_ButtonOptions("Back", kMainMenuButtonPos, 5, ...) calls.
	float fStartY = kMainMenuButtonPos.y + kOptionMenuButtonSpacing * 4.0f;
	float fBackY = kMainMenuButtonPos.y + kOptionMenuButtonSpacing * 5.0f;

	bool bStartSelected = (mlNewGameHoveredControl == eSomaNewGameControl_StartGame);
	if (bStartSelected && mpButtonBarGfx)
	{
		cVector3f vBarPos(kMainMenuButtonPos.x - 22.0f, fStartY, 0.5f);
		mpGuiSet->DrawGfx(mpButtonBarGfx, vBarPos, kOptionMenuButtonBgSize, kMainMenuButtonBgColor);
	}
	if (mpButtonFont)
		mpGuiSet->DrawFont(_W("START GAME"), mpButtonFont, cVector3f(kMainMenuButtonPos.x, fStartY, 1.0f), cVector2f(36, 36),
							bStartSelected ? kSelectedTextColor : kDeselectedTextColor, eFontAlign_Left);

	bool bBackSelected = (mlNewGameHoveredControl == eSomaNewGameControl_Back);
	if (bBackSelected && mpOptionsHighlightGfx)
	{
		cVector3f vBarPos(kMainMenuButtonPos.x - 22.0f, fBackY, 0.5f);
		mpGuiSet->DrawGfx(mpOptionsHighlightGfx, vBarPos, kOptionMenuButtonBgSize, kMainMenuButtonBgColor);
	}
	if (mpButtonFont)
		mpGuiSet->DrawFont(_W("BACK"), mpButtonFont, cVector3f(kMainMenuButtonPos.x, fBackY, 1.0f), cVector2f(36, 36),
							bBackSelected ? kSelectedTextColor : kDeselectedTextColor, eFontAlign_Left);
}

//-----------------------------------------------------------------------

void cSomaMainMenu::UpdateNewGameDifficultyMouseHitTest()
{
	int lPrevHovered = mlNewGameHoveredControl;
	mlNewGameHoveredControl = eSomaNewGameControl_None;

	const cVector2f &vMouse = mpGuiSet->GetMousePos();
	float fModeRowY = kMainMenuButtonPos.y;

	// Same cycle-bar rect DrawOptionsCycleControl()/eKind_MultiSelect's own
	// click-side test uses (kOptionsSliderOffset/Size) - see ClickOptionsRow().
	cVector2f vBoxPos(kMainMenuButtonPos.x + kOptionsSliderOffset.x, fModeRowY + kOptionsSliderOffset.y);
	cVector2f vBoxSize = kOptionsSliderSize;

	if (vMouse.x >= vBoxPos.x && vMouse.x <= vBoxPos.x + vBoxSize.x && vMouse.y >= vBoxPos.y && vMouse.y <= vBoxPos.y + vBoxSize.y)
	{
		float fMid = kMainMenuButtonPos.x + (kOptionsSliderTrackLocalMinX + kOptionsSliderTrackLocalMaxX) * 0.5f;
		mlNewGameHoveredControl = (vMouse.x < fMid) ? eSomaNewGameControl_LeftArrow : eSomaNewGameControl_RightArrow;
	}
	else
	{
		float fStartY = kMainMenuButtonPos.y + kOptionMenuButtonSpacing * 4.0f;
		float fBackY = kMainMenuButtonPos.y + kOptionMenuButtonSpacing * 5.0f;

		if (vMouse.x >= kMainMenuButtonPos.x && vMouse.x <= kVirtualCanvas.x && vMouse.y >= fStartY && vMouse.y <= fStartY + kOptionMenuButtonSpacing)
			mlNewGameHoveredControl = eSomaNewGameControl_StartGame;
		else if (vMouse.x >= kMainMenuButtonPos.x && vMouse.x <= kVirtualCanvas.x && vMouse.y >= fBackY && vMouse.y <= fBackY + kOptionMenuButtonSpacing)
			mlNewGameHoveredControl = eSomaNewGameControl_Back;
	}

	if (mlNewGameHoveredControl != eSomaNewGameControl_None && mlNewGameHoveredControl != lPrevHovered)
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::FocusSound());
}

//-----------------------------------------------------------------------

void cSomaMainMenu::ClickNewGameDifficultyControl(int aControl)
{
	switch (aControl)
	{
	case eSomaNewGameControl_LeftArrow:
	case eSomaNewGameControl_RightArrow:
		// Only 2 real modes exist, so either arrow just flips it - same
		// "any click toggles" simplification ClickOptionsRow()'s
		// eKind_MultiSelect case documents for a 2-value list.
		mlNewGameMode = (mlNewGameMode == 0) ? 1 : 0;
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::ChangeSound());
		break;

	case eSomaNewGameControl_StartGame:
	{
		// Real ClickNewGame() - moved here from the old direct-from-title-
		// screen NEW GAME click (RunPendingAction()'s old
		// eSomaMainMenuAction_NewGame case) now that this difficulty screen
		// sits in between, matching the real click order (StartGame ->
		// ClickNewGame(), not NEW GAME -> ClickNewGame() directly). No
		// overwrite-save confirmation dialog: this port's own mbCanContinue
		// equivalent (BuildMainMenuItems()'s CONTINUE row) is always false -
		// see the class comment in SomaMainMenu.h - so the real branch this
		// actually hits is GuiGameModeSelection()'s own "else ClickNewGame()"
		// (mbCanContinue == false), the same as a real fresh SOMA install
		// with no existing save, not a fabricated skip.
		tString sError;
		if (mpBase->StartNewGame(sError) == false)
		{
			Log("SOMA main menu: New Game failed to load the start map (%s)\n", sError.c_str());
			break;
		}
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::NewGameSting());
		SetVisible(false);
		break;
	}

	case eSomaNewGameControl_Back:
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::ChangeSound());
		NavigateTo(eSomaMenuScreen_Main);
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------
//
// Options screen
//
// Real script tree (script/modules/MenuHandler.hps): GuiOptions() lists
// Gameplay/Controls/Video/Audio/Back, each of which opens its own
// sub-screen (GuiOptionsVideo() further splits into Display/PostEffect/
// World/Gamma, GuiOptionsInput() into Keybind/MouseOptions/GamepadOptions).
// The FULL real tree/order/captions is reproduced (see the class comment in
// SomaMainMenu.h for the earlier, narrower pass this superseded) - every row
// this engine has no real backend for is still listed, just drawn
// grayed-out/non-interactive (cSomaOptionsRow::mbEnabled).
//
// Rows with a real, live/persisted backend as of this pass: master volume +
// Subtitles (cSound/cSomaIntroSequence), Gamma/VSync/Fullscreen/Resolution/
// Anti-Aliasing/Horizontal FOV (cLowLevelGraphics/cRenderSettings/cCamera -
// Resolution and Fullscreen are restart-required, same contract; the rest
// are live), and MouseOptions' Sensitivity/InvertMouseY (cSomaPlayer). See
// each row's own build-site comment below and SomaConfig.h for exactly which
// real script/config key each maps to. Still honestly grayed - no working
// backend exists in this engine yet: Gameplay's whole tab (language/hints/
// screen-distortion/colour-separation/crosshair-style), Keybindings/
// Controller Options (no rebindable-action UI built against HPL2/core's real
// cAction system yet, see eSomaMenuScreen_OptionsControls's comment),
// Refresh Rate (no real distinct per-mode value on this engine's SDL2
// backend - see the Resolution row's comment), PostEffect/Rendering tabs
// (no depth-of-field/SSAO/bloom/texture/shadow/reflection/refraction
// systems), Smooth Mouse, and Closed Caption (HearingAid).
//
//-----------------------------------------------------------------------

void cSomaMainMenu::NavigateTo(eSomaMenuScreen aScreen)
{
	mScreen = aScreen;
	mlOptionsHoveredRow = -1;
	mlDraggingSliderRow = -1;
	mlAwaitingKeybindRow = -1;
}

//-----------------------------------------------------------------------

// Real Resolution row's value list - see the mvResolutions comment in
// SomaMainMenu.h. cPlatform::GetAvailableVideoModes() is the same real API
// amnesia/src/game/LuxMainMenu_Options.cpp's own Resolution dropdown uses
// (see cLuxMainMenu_Options::UpdateResolutions() there); this engine's SDL2
// backend enumerates every display mode across every connected display plus
// a synthetic (0,0) "current desktop" marker per display (see
// PlatformSDL.cpp's cPlatform::GetAvailableVideoModes()) - only the primary
// display's (mlDisplay==0) real, non-zero sizes are kept here, deduplicated,
// since this scaffold has no multi-monitor picker at all (real SOMA's own
// Resolution row doesn't show a separate per-display list either).
void cSomaMainMenu::BuildResolutionList()
{
	mvResolutions.clear();

	tVideoModeVec vModes;
	cPlatform::GetAvailableVideoModes(vModes, 32);

	for (size_t i = 0; i < vModes.size(); ++i)
	{
		const cVideoMode &mode = vModes[i];
		if (mode.mlDisplay != 0)
			continue;
		if (mode.mvScreenSize.x <= 0 || mode.mvScreenSize.y <= 0)
			continue; // the synthetic "current desktop" marker - not a real explicit size to switch to

		bool bDup = false;
		for (size_t j = 0; j < mvResolutions.size(); ++j)
		{
			if (mvResolutions[j] == mode.mvScreenSize) { bDup = true; break; }
		}
		if (bDup == false)
			mvResolutions.push_back(mode.mvScreenSize);
	}

	// Headless/no-display environments (this project's own established
	// test convention - see the class comment's "headless-control" mentions
	// elsewhere in this codebase) may enumerate zero real modes - fall back
	// to whatever the engine actually booted at, so the Resolution row and
	// ClickOptionsRow()'s index math always have at least one real entry.
	if (mvResolutions.empty())
		mvResolutions.push_back(mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeInt());

	// Make sure the currently-configured resolution (which may not be a
	// mode SDL enumerated - e.g. a leftover value from a display that's
	// since been unplugged) is always representable, same "(Custom)"
	// fallback idea as LuxMainMenu_Options.cpp's own resolution list.
	cSomaConfig *pCfg = mpBase->GetConfig();
	cVector2l vCurRes(pCfg->mlScreenWidth, pCfg->mlScreenHeight);
	bool bHasCurrent = false;
	for (size_t i = 0; i < mvResolutions.size(); ++i)
	{
		if (mvResolutions[i] == vCurRes) { bHasCurrent = true; break; }
	}
	if (bHasCurrent == false)
		mvResolutions.push_back(vCurRes);
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
	static bool bHearingAid = false;
	static bool bSSAO = true;
	static bool bBloom = true;
	static bool bReflection = true;
	static bool bRefraction = true;

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
		// when EyeTracking_IsAvailable(), never true here). Keybindings and
		// MouseOptions both have a real backend now (see
		// eSomaMenuScreen_OptionsControlsKeybind/Mouse below); no gamepad
		// support exists at all, so Controller Options stays disabled.
		mOptionsRows.push_back(MakeCategoryRow(_W("KEYBINDINGS"), eSomaMenuScreen_OptionsControlsKeybind));
		mOptionsRows.push_back(MakeCategoryRow(_W("MOUSE OPTIONS"), eSomaMenuScreen_OptionsControlsMouse));
		mOptionsRows.push_back(MakeDisabledActionRow(_W("CONTROLLER OPTIONS"), eSomaMenuScreen_OptionsControls));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_OptionsRoot));
		break;

	case eSomaMenuScreen_OptionsControlsKeybind:
		// Real GuiOptionsInputKeybind() - simplified to this scaffold's
		// actual action set (cSomaBase::eSomaPlayerAction: Forward/Backward/
		// Left/Right/Jump, no secondary bind slot, no gamepad rebinding -
		// see SomaBase.h's own scope note). Each row shows the action's real
		// current key (cSomaBase::GetPlayerActionKeyName()) and, on click,
		// waits for the next real key press to rebind it (see
		// UpdateKeybindCapture()).
		mOptionsRows.push_back(MakeKeybindRow(_W("MOVE FORWARD"), cSomaBase::eSomaPlayerAction_Forward,
											   cString::To16Char(mpBase->GetPlayerActionKeyName(cSomaBase::eSomaPlayerAction_Forward))));
		mOptionsRows.push_back(MakeKeybindRow(_W("MOVE BACKWARD"), cSomaBase::eSomaPlayerAction_Backward,
											   cString::To16Char(mpBase->GetPlayerActionKeyName(cSomaBase::eSomaPlayerAction_Backward))));
		mOptionsRows.push_back(MakeKeybindRow(_W("MOVE LEFT"), cSomaBase::eSomaPlayerAction_Left,
											   cString::To16Char(mpBase->GetPlayerActionKeyName(cSomaBase::eSomaPlayerAction_Left))));
		mOptionsRows.push_back(MakeKeybindRow(_W("MOVE RIGHT"), cSomaBase::eSomaPlayerAction_Right,
											   cString::To16Char(mpBase->GetPlayerActionKeyName(cSomaBase::eSomaPlayerAction_Right))));
		mOptionsRows.push_back(MakeKeybindRow(_W("JUMP"), cSomaBase::eSomaPlayerAction_Jump,
											   cString::To16Char(mpBase->GetPlayerActionKeyName(cSomaBase::eSomaPlayerAction_Jump))));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_OptionsControls));
		break;

	case eSomaMenuScreen_OptionsControlsMouse:
	{
		// Real GuiOptionsInputMouse(): MouseSens/InvertMouseY/SmoothMouse/
		// Back. MouseSens and InvertMouseY are real, live settings (see
		// cSomaPlayer::Update()); SmoothMouse has no backend (this engine's
		// mouse-look applies the raw per-frame delta directly, with no
		// smoothing/filter buffer to gate), stays disabled.
		static bool bSmoothMouse = true;
		mOptionsRows.push_back(MakeSliderRow(_W("MOUSE SENSITIVITY"), &pCfg->mfMouseSensitivity, 0.01f, 4.01f, 0.05f));
		mOptionsRows.push_back(MakeToggleRow(_W("INVERT MOUSE Y"), &pCfg->mbInvertMouseY));
		mOptionsRows.push_back(MakeToggleRow(_W("SMOOTH MOUSE"), &bSmoothMouse, false));
		mOptionsRows.push_back(MakeBackRow(eSomaMenuScreen_OptionsControls));
		break;
	}

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
		// RefreshRate/AA/FOV/Back. DisplayMode and VSync are collapsed to a
		// 2-state toggle here since this engine only has a bool for each (no
		// borderless window mode, no adaptive-vsync getter - see
		// SomaConfig.h); Resolution/AA/FOV are now real too (see below).
		// RefreshRate stays disabled: this engine's own SDL2 video-mode
		// enumeration (HPL2/core/sources/impl/PlatformSDL.cpp's
		// cPlatform::GetAvailableVideoModes()) hardcodes every mode's
		// mlRefreshRate to a literal 1 rather than reading
		// SDL_DisplayMode::refresh_rate - there is no real distinct
		// per-resolution refresh rate value anywhere in this engine to offer
		// a choice between, on this platform/backend (out of this task's
		// file scope to fix - HPL2/core is shared with Dark Descent/AMFP and
		// other agents' concurrent work).
		int lCurRes = 0;
		std::vector<tWString> vResOptions;
		for (size_t i = 0; i < mvResolutions.size(); ++i)
		{
			vResOptions.push_back(cString::ToStringW(mvResolutions[i].x) + _W("x") + cString::ToStringW(mvResolutions[i].y));
			if (mvResolutions[i].x == pCfg->mlScreenWidth && mvResolutions[i].y == pCfg->mlScreenHeight)
				lCurRes = (int)i;
		}
		mOptionsRows.push_back(MakeMultiSelectRow(_W("RESOLUTION"), vResOptions, lCurRes, true, cSomaOptionsRow::eOptionId_Resolution));

		// Real captions "FULLSCREEN"/"WINDOWED" (base_english.lang's
		// Fullscreen/Windowed entries) - the real 3-way Fullscreen/Windowed/
		// Borderless multi-select collapsed to this engine's single bool,
		// same scope note as above.
		mOptionsRows.push_back(MakeToggleRow(_W("DISPLAY MODE"), &pCfg->mbFullscreen, true, _W("FULLSCREEN"), _W("WINDOWED")));
		mOptionsRows.push_back(MakeToggleRow(_W("V-SYNC"), &pCfg->mbVSync));
		mOptionsRows.push_back(MakeMultiSelectRow(_W("REFRESH RATE"), {_W("AUTO")}, 0));
		// Live: cRenderSettings::mbUseFxaa
		mOptionsRows.push_back(MakeMultiSelectRow(_W("ANTI-ALIASING"), {_W("OFF"), _W("FXAA")}, pCfg->mbAntiAliasing ? 1 : 0,
												   true, cSomaOptionsRow::eOptionId_AntiAliasing));

		// Real MenuHandler.hps's FOV row is the one slider that shows a
		// trailing numeric value (Gamma/Volume don't - see
		// cSomaOptionsRow::mSliderValueText) - real formula verbatim from
		// GuiOptionsVideoDisplay(): the stored 50-83 value is treated as a
		// vertical-ish FOV and converted to a horizontal degrees figure using
		// the real screen aspect ratio before display. Now a real, live
		// setting: cSomaConfig::mfFOV is applied to the real player camera
		// every cSomaPlayer::Update() (see SomaPlayer.cpp).
		const cVector2l &vScreenSize = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeInt();
		float fAspect = (vScreenSize.y != 0) ? (float)vScreenSize.x / (float)vScreenSize.y : 16.0f / 9.0f;
		float fHorizontalFovRad = 2.0f * atanf(tanf(cMath::ToRad(pCfg->mfFOV) * 0.5f) * fAspect);
		tWString sFovText = cString::ToStringW((int)(cMath::ToDeg(fHorizontalFovRad) + 0.5f));
		mOptionsRows.push_back(MakeSliderRow(_W("HORIZONTAL FOV"), &pCfg->mfFOV, 50.0f, 83.0f, 0.05f, true, sFovText));
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
		// here)/Volume/Subtitles/HearingAid/Back. Volume and Subtitles now
		// both have a real backend: Subtitles (real key Sound/ShowSubtitles)
		// genuinely gates cSomaIntroSequence::DrawSubtitle(), this engine's
		// only subtitle-rendering content so far (see SomaIntroSequence.cpp).
		// HearingAid ("CLOSED CAPTION" - real key
		// Sound/ForceShowSubtitleCharacterName) stays disabled: the real
		// setting forces the speaker-name prefix onto subtitle lines that
		// would otherwise omit it when the speaker is visually unambiguous:
		// this port's subtitle line always includes the speaker name
		// already (no contextual suppression logic exists to "force"
		// anything on top of - see DrawSubtitle()), so there is no real,
		// distinguishable behaviour left for this toggle to control without
		// inventing a rule the real game doesn't have.
		mOptionsRows.push_back(MakeSliderRow(_W("VOLUME"), &pCfg->mfMasterVolume, 0.0f, 1.0f, 0.1f));
		mOptionsRows.push_back(MakeToggleRow(_W("SUBTITLES"), &pCfg->mbShowSubtitles));
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
	{
		// Only Resolution/AntiAliasing are ever built with mbEnabled true
		// (see MakeMultiSelectRow() call sites in BuildOptionsRows()) -
		// every other multi-select row's mOptionId is eOptionId_None and
		// mbEnabled is false, already filtered out by the guard above.
		if (row.mOptions.empty())
			break;

		// Real OptionMenu_ButtonOptionsMultiSelect(): clicking left/right of
		// the widget's own centre cycles the value list backward/forward -
		// same click-side test as the eKind_Slider click-to-step branch
		// below (this cycle-bar widget occupies the exact same on-screen
		// rect as the slider one, see DrawOptionsCycleControl()).
		float fLocalX = mpGuiSet->GetMousePos().x - kMainMenuButtonPos.x;
		float fMid = (kOptionsSliderTrackLocalMinX + kOptionsSliderTrackLocalMaxX) * 0.5f;
		int lDir = (fLocalX < fMid) ? -1 : 1;
		int lCount = (int)row.mOptions.size();
		int lNewIndex = ((row.mlOptionIndex + lDir) % lCount + lCount) % lCount;

		// Real OptionMenu_ButtonOptionsMultiSelect() plays frontend_menu_select
		// on click (helper_imgui_options.hps).
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::SelectSound());

		switch (row.mOptionId)
		{
		case cSomaOptionsRow::eOptionId_Resolution:
			// Live, same contract as Anti-Aliasing below (see SomaConfig.h's
			// mlScreenWidth/mlScreenHeight comment) - mvResolutions is the
			// same cached list the row's value text came from in
			// BuildOptionsRows(), so the index lines up. ForceWindowSize()
			// resizes the real live window immediately;
			// cLowLevelGraphicsSDL::CheckAndUpdateScreenSize() (already
			// called every frame from cGraphics::Update()) picks up the new
			// size and reconciles every render target/viewport from it
			// within a frame, with no restart needed.
			if (lNewIndex >= 0 && lNewIndex < (int)mvResolutions.size())
			{
				pCfg->mlScreenWidth = mvResolutions[lNewIndex].x;
				pCfg->mlScreenHeight = mvResolutions[lNewIndex].y;
				mpEngine->GetGraphics()->GetLowLevel()->ForceWindowSize(pCfg->mlScreenWidth, pCfg->mlScreenHeight);
				Log("SOMA options: Resolution changed to %dx%d - applied live\n",
					pCfg->mlScreenWidth, pCfg->mlScreenHeight);
			}
			break;

		case cSomaOptionsRow::eOptionId_AntiAliasing:
			// Off/FXAA only, so either direction flips it
			pCfg->mbAntiAliasing = (lNewIndex != 0);
			if (mpViewport) mpViewport->GetRenderSettings()->mbUseFxaa = pCfg->mbAntiAliasing;
			break;

		default:
			break;
		}

		pCfg->Save();
		break;
	}

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
			// checkbox has. Confirmed no live fullscreen<->windowed mode-
			// switch call exists anywhere in cLowLevelGraphicsSDL (only
			// ForceWindowSize(), which resizes within the current mode - see
			// the Resolution case above and iLowLevelGraphics::
			// GetFullscreenModeActive(), the only fullscreen-related call
			// that isn't Init()-only) - a real live setter would need new SDL
			// plumbing, out of scope for this pass.
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
			else if (row.mpFloatValue == &pCfg->mfFOV && mpBase->GetDebugCamera())
				// Live-applied here too (not just read every cSomaPlayer::
				// Update() - see SomaPlayer.cpp) so this is visibly correct
				// even from the menu's own free-fly camera, before any real
				// game map/player controller exists.
				mpBase->GetDebugCamera()->SetFOV(cMath::ToRad(pCfg->mfFOV));

			pCfg->Save();
		}
		break;
	}

	case cSomaOptionsRow::eKind_Keybind:
	{
		// Real OptionMenu_ButtonKeybind(): clicking a bind slot starts
		// capture mode rather than acting immediately - see
		// UpdateKeybindCapture() (driven from Update() instead of here,
		// since it must keep running across frames until a key is pressed).
		//
		// Drain any already-queued key presses first (iKeyboard::GetKey()'s
		// queue is fed by every real keypress regardless of what's focused -
		// e.g. whatever key, if any, was down in the same frame as this
		// click) - without this, capture mode could instantly "consume" a
		// stale press from before the row was even clicked instead of
		// genuinely waiting for the next one.
		iKeyboard *pKeyboard = mpEngine->GetInput()->GetKeyboard();
		while (pKeyboard->KeyIsPressed())
			pKeyboard->GetKey();

		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::SelectSound());
		mlAwaitingKeybindRow = alIndex;
		break;
	}
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::UpdateKeybindCapture()
{
	if (mlAwaitingKeybindRow < 0 || mlAwaitingKeybindRow >= (int)mOptionsRows.size())
	{
		mlAwaitingKeybindRow = -1;
		return;
	}

	const cSomaOptionsRow &row = mOptionsRows[mlAwaitingKeybindRow];
	if (row.mKind != cSomaOptionsRow::eKind_Keybind)
	{
		mlAwaitingKeybindRow = -1;
		return;
	}

	// Real iKeyboard::GetKey() - "can be checked many times to see all key
	// presses" (see Keyboard.h) - designed for exactly this kind of polling
	// capture loop, same idea as amnesia/src/game/LuxMainMenu_KeyConfig.cpp's
	// own key-press interception, just polled directly here rather than
	// routed through a cWidget focus message (this menu doesn't use
	// cWidget/cGui's widget system at all - see the class comment).
	// KeyIsPressed() MUST be checked first - GetKey() calls .front() on its
	// internal queue unconditionally (see KeyboardSDL.cpp), undefined
	// behaviour on an empty one.
	iKeyboard *pKeyboard = mpEngine->GetInput()->GetKeyboard();
	if (pKeyboard->KeyIsPressed() == false)
		return; // still waiting

	cKeyPress keyPress = pKeyboard->GetKey();

	// Escape cancels the capture without rebinding anything - same "back
	// out of a modal without side effects" convention every other Options
	// sub-screen's Back row already follows.
	if (keyPress.mKey != eKey_Escape)
	{
		cSomaBase::eSomaPlayerAction action = (cSomaBase::eSomaPlayerAction)row.mlOptionIndex;
		mpBase->RebindPlayerAction(action, keyPress.mKey);
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::SelectSound());
	}
	else
	{
		PlaySomaMenuSfx(mpEngine, cSomaMenuSfx::ChangeSound());
	}

	mlAwaitingKeybindRow = -1;
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
	else if (row.mpFloatValue == &pCfg->mfFOV && mpBase->GetDebugCamera())
		mpBase->GetDebugCamera()->SetFOV(cMath::ToRad(pCfg->mfFOV)); // see the matching branch in ClickOptionsRow() above

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
		// Real on/off checkbox-pair widget (see the eKind_Toggle comment in
		// SomaMainMenu.h and DrawOptionsToggleControl()) - NOT the cycle-bar
		// eKind_MultiSelect below still correctly uses. Real
		// OptionMenu_OptionsCheckbox() always draws its "Off"/"On" labels in
		// plain black regardless of any "disabled" concept (which doesn't
		// exist in the real game at all - see kDisabledColor's own doc
		// comment) - kept black here too even for a disabled row, same
		// reasoning as the eKind_Slider case below.
		bool bValue = aRow.mpBoolValue ? *aRow.mpBoolValue : (aRow.mlOptionIndex != 0);
		tWString sOff = aRow.mOptions.size() > 0 ? aRow.mOptions[0] : _W("OFF");
		tWString sOn = aRow.mOptions.size() > 1 ? aRow.mOptions[1] : _W("ON");
		cColor activeCol = aRow.mbEnabled ? cColor(1, 1) : kDisabledColor;
		DrawOptionsToggleControl(fRowY, bValue, sOff, sOn, widgetOnCol, activeCol, cColor(0, 1));
		break;
	}

	case cSomaOptionsRow::eKind_Slider:
	{
		float fValue = aRow.mpFloatValue ? *aRow.mpFloatValue : 0;
		float fNorm = cMath::Clamp((fValue - aRow.mfMin) / (aRow.mfMax - aRow.mfMin), 0.0f, 1.0f);
		bool bTexted = aRow.mSliderValueText.empty() == false; // real "asTextValue.length() > 0" branch (FOV only)

		if (mpOptionsMeterGfx)
		{
			cVector3f vMeterPos(kMainMenuButtonPos.x + kOptionsSliderOffset.x, fRowY + kOptionsSliderOffset.y, 1.5f);
			mpGuiSet->DrawGfx(mpOptionsMeterGfx, vMeterPos, kOptionsSliderSize, widgetOnCol);
		}

		// Real kOptionMenu_SliderArrowOffsetL/TextedSliderArrowOffsetL: the
		// right arrow sits at the bar's own right edge (x=570) for a plain
		// slider, or short of it (x=510) to leave room for the trailing
		// value text on a texted one (Gamma/Volume vs FOV).
		float fRightArrowX = bTexted ? kOptionsTextedSliderArrowRightX : kOptionsSliderArrowOffsetL.x;
		if (mpOptionsArrowGfx)
		{
			cVector3f vArrowL(kMainMenuButtonPos.x + kOptionsSliderArrowOffsetR.x, fRowY + kOptionsSliderArrowOffsetR.y, 2.0f);
			cVector3f vArrowR(kMainMenuButtonPos.x + fRightArrowX, fRowY + kOptionsSliderArrowOffsetL.y, 2.0f);
			mpGuiSet->DrawGfx(mpOptionsArrowGfx, vArrowL, kOptionsSliderArrowSize, widgetArrowCol, eGuiMaterial_LastEnum, 180.0f);
			mpGuiSet->DrawGfx(mpOptionsArrowGfx, vArrowR, kOptionsSliderArrowSize, widgetArrowCol);
		}

		if (mpOptionsBarGfx)
		{
			const cVector2f &vBarOffset = bTexted ? kOptionsTextedSliderBarOffset : kOptionsSliderBarOffset;
			const cVector2f &vBarSize = bTexted ? kOptionsTextedSliderBarSize : kOptionsSliderBarSize;
			// Real gfxBar draws (both OptionMenu_OptionsSlider() branches)
			// are unconditionally cColor(0,1) - kept black even when
			// mbEnabled is false (this engine's own "grey out unbacked rows"
			// convention, which the real game has no equivalent of) so the
			// track/handle stay visible against the also-grey disabled bar
			// fill instead of disappearing into it.
			cVector3f vTrackPos(kMainMenuButtonPos.x + vBarOffset.x, fRowY + vBarOffset.y, 2.0f);
			mpGuiSet->DrawGfx(mpOptionsBarGfx, vTrackPos, vBarSize, cColor(0, 1));

			cVector3f vHandlePos = vTrackPos + cVector3f(vBarSize.x * fNorm - 3.0f, -6.0f, 0.1f);
			mpGuiSet->DrawGfx(mpOptionsBarGfx, vHandlePos, cVector2f(6, 16), cColor(0, 1));
		}

		// Real kOptionMenu_TextedSliderTextOffset - FOV only (see
		// cSomaOptionsRow::mSliderValueText). Real script draws this in
		// plain black unconditionally too - see the track/handle comment
		// above for why that's kept even though this row is currently
		// disabled (this row was previously disabled AND silently blank - a
		// real fresh SOMA install still shows "96" etc, just non-
		// interactive, so this now matches).
		if (bTexted && mpButtonFont)
		{
			// eFontAlign_Center only recentres X (see DrawTextFromCharArry()
			// in HPL2/core/sources/gui/GuiSet.cpp) - avPos.y is always the
			// TOP of the glyphs, never vertically centred by the align
			// param, so this vertically centres a 28-tall value inside the
			// 46-tall bar by hand.
			const float fFontH = 28.0f;
			cVector3f vValuePos(kMainMenuButtonPos.x + kOptionsTextedSliderTextOffset.x,
								 fRowY + kOptionsSliderOffset.y + (kOptionsSliderSize.y - fFontH) * 0.5f, 2.0f);
			mpGuiSet->DrawFont(aRow.mSliderValueText, mpButtonFont, vValuePos, cVector2f(fFontH, fFontH),
							   cColor(0, 1), eFontAlign_Center);
		}
		break;
	}

	case cSomaOptionsRow::eKind_MultiSelect:
	{
		// Real OptionMenu_ButtonOptionsMultiSelect() - every row of this kind
		// is built disabled (see MakeMultiSelectRow()), so this always
		// renders the grey/inert bar/arrows via widgetOnCol/widgetArrowCol
		// above - value text stays black regardless, same reasoning as
		// eKind_Toggle/eKind_Slider above (grey-on-grey would be unreadable).
		tWString sValue = (aRow.mlOptionIndex >= 0 && aRow.mlOptionIndex < (int)aRow.mOptions.size())
							   ? aRow.mOptions[aRow.mlOptionIndex]
							   : tWString();
		DrawOptionsCycleControl(fRowY, sValue, widgetOnCol, widgetArrowCol, cColor(0, 1));
		break;
	}

	case cSomaOptionsRow::eKind_Keybind:
	{
		// Real OptionMenu_ButtonKeybind() - same meter-bar background as
		// every other widget here, no left/right arrows (this isn't a
		// cycle - a single click starts capture instead), showing either
		// the bound key's real name or, while this exact row is being
		// captured (see mlAwaitingKeybindRow), a prompt telling the player
		// to press a key.
		if (mpOptionsMeterGfx)
		{
			cVector3f vMeterPos(kMainMenuButtonPos.x + kOptionsSliderOffset.x, fRowY + kOptionsSliderOffset.y, 1.5f);
			mpGuiSet->DrawGfx(mpOptionsMeterGfx, vMeterPos, kOptionsSliderSize, widgetOnCol);
		}

		bool bAwaitingThisRow = (mlAwaitingKeybindRow == alIndex);
		tWString sValue = bAwaitingThisRow ? _W("PRESS A KEY...") : aRow.mSliderValueText;

		if (mpButtonFont)
		{
			const float fFontH = 24.0f;
			cVector3f vValuePos(kMainMenuButtonPos.x + kOptionsSliderOffset.x + kOptionsSliderSize.x * 0.5f,
								 fRowY + kOptionsSliderOffset.y + (kOptionsSliderSize.y - fFontH) * 0.5f, 2.0f);
			mpGuiSet->DrawFont(sValue, mpButtonFont, vValuePos, cVector2f(fFontH, fFontH),
								bAwaitingThisRow ? kSelectedTextColor : cColor(0, 1), eFontAlign_Center);
		}
		break;
	}
	}
}

//-----------------------------------------------------------------------

// Shared real "startmenu_options_button_meter" + left/right
// "startmenu_options_arrow" cycle-bar widget - see the eKind_Toggle comment
// in SomaMainMenu.h and DrawOptionsRow()'s eKind_Toggle/eKind_MultiSelect
// cases above, the two callers.
void cSomaMainMenu::DrawOptionsCycleControl(float afRowY, const tWString &asValueText, const cColor &aBarCol, const cColor &aArrowCol, const cColor &aTextCol)
{
	if (mpOptionsMeterGfx)
	{
		cVector3f vMeterPos(kMainMenuButtonPos.x + kOptionsSliderOffset.x, afRowY + kOptionsSliderOffset.y, 1.5f);
		mpGuiSet->DrawGfx(mpOptionsMeterGfx, vMeterPos, kOptionsSliderSize, aBarCol);
	}

	if (mpOptionsArrowGfx)
	{
		cVector3f vArrowL(kMainMenuButtonPos.x + kOptionsSliderArrowOffsetR.x, afRowY + kOptionsSliderArrowOffsetR.y, 2.0f);
		cVector3f vArrowR(kMainMenuButtonPos.x + kOptionsSliderArrowOffsetL.x, afRowY + kOptionsSliderArrowOffsetL.y, 2.0f);
		mpGuiSet->DrawGfx(mpOptionsArrowGfx, vArrowL, kOptionsSliderArrowSize, aArrowCol, eGuiMaterial_LastEnum, 180.0f);
		mpGuiSet->DrawGfx(mpOptionsArrowGfx, vArrowR, kOptionsSliderArrowSize, aArrowCol);
	}

	if (mpButtonFont && asValueText.empty() == false)
	{
		// See the matching comment in the eKind_Slider case above -
		// eFontAlign_Center never centres Y, only X, so this bug (pre-
		// existing in eKind_MultiSelect's own copy of this code before the
		// eKind_Toggle/eKind_MultiSelect draw paths were unified into this
		// function) had every cycle-bar's value text spilling down into the
		// row below it rather than sitting centred in its own bar.
		const float fFontH = 24.0f;
		cVector3f vValuePos(kMainMenuButtonPos.x + kOptionsSliderOffset.x + kOptionsSliderSize.x * 0.5f,
							 afRowY + kOptionsSliderOffset.y + (kOptionsSliderSize.y - fFontH) * 0.5f, 2.0f);
		mpGuiSet->DrawFont(asValueText, mpButtonFont, vValuePos, cVector2f(fFontH, fFontH), aTextCol, eFontAlign_Center);
	}
}

//-----------------------------------------------------------------------

// Real "startmenu_options_button_on/off" checkbox-pair widget
// (OptionMenu_OptionsCheckbox() in helper_imgui_options.hps) - see the
// eKind_Toggle comment in SomaMainMenu.h and DrawOptionsRow()'s eKind_Toggle
// case above, its one caller. Both textures are plain untextured white
// shapes (confirmed by viewing them directly) tinted at draw time - abValue
// picks which side gets aActiveCol (bright, or whatever the row's own
// selection-tint is) and which gets aInactiveCol (dim), same as the real
// script's abIsChecked ? ... : ... tint swap. Positioned inside the same
// kOptionsSliderOffset/kOptionsSliderSize bar rect DrawOptionsCycleControl()
// above uses (this engine doesn't reproduce the real script's own
// OptionMenu_UpdateExtraWidth() per-label dynamic width fit, so reusing the
// already-working cycle-bar's rect keeps every widget kind lined up in the
// same column) rather than the real script's separate, absolute
// kOptionMenu_CheckboxOffset.
void cSomaMainMenu::DrawOptionsToggleControl(float afRowY, bool abValue, const tWString &asOffLabel, const tWString &asOnLabel,
											  const cColor &aInactiveCol, const cColor &aActiveCol, const cColor &aTextCol)
{
	const float fBoxW = 50.0f; // real kOptionMenu_CheckboxSize is 2*50 wide, 46 tall
	const float fBoxH = kOptionsSliderSize.y;
	const float fPairX = kOptionsSliderOffset.x + (kOptionsSliderSize.x - fBoxW * 2.0f) * 0.5f;

	cVector3f vOffPos(kMainMenuButtonPos.x + fPairX, afRowY + kOptionsSliderOffset.y, 1.5f);
	cVector3f vOnPos(kMainMenuButtonPos.x + fPairX + fBoxW, afRowY + kOptionsSliderOffset.y, 1.5f);

	if (mpOptionsToggleOffGfx)
		mpGuiSet->DrawGfx(mpOptionsToggleOffGfx, vOffPos, cVector2f(fBoxW, fBoxH), abValue ? aInactiveCol : aActiveCol);
	if (mpOptionsToggleOnGfx)
		mpGuiSet->DrawGfx(mpOptionsToggleOnGfx, vOnPos, cVector2f(fBoxW, fBoxH), abValue ? aActiveCol : aInactiveCol);

	if (mpButtonFont)
	{
		const float fFontH = 22.0f;
		float fTextY = afRowY + kOptionsSliderOffset.y + (fBoxH - fFontH) * 0.5f;
		mpGuiSet->DrawFont(asOffLabel, mpButtonFont, cVector3f(vOffPos.x - 8.0f, fTextY, 2.0f), cVector2f(fFontH, fFontH), aTextCol, eFontAlign_Right);
		mpGuiSet->DrawFont(asOnLabel, mpButtonFont, cVector3f(vOnPos.x + fBoxW + 8.0f, fTextY, 2.0f), cVector2f(fFontH, fFontH), aTextCol, eFontAlign_Left);
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
	case eSomaMenuScreen_OptionsControlsMouse: sTitle = _W("MOUSE OPTIONS"); break;
	case eSomaMenuScreen_OptionsControlsKeybind: sTitle = _W("KEYBINDINGS"); break;
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
