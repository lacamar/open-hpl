/*
 * A real, interactive main menu for the SOMA Phase 0/1 scaffold.
 *
 * SOMA's actual main menu is driven by script/modules/MenuHandler.hps (a
 * cScrMenuHandler AngelScript module, ~7300 lines) running on top of
 * Frictional's own "cImGui" immediate-mode drawing system - not the
 * cGuiSet/cWidget system Dark Descent's cLuxMainMenu uses. That real script
 * is not a recreation, it's ground truth: read directly out of SOMA's own
 * shipped game data (script/modules/MenuHandler.hps and
 * script/custom_depth/helper_custom_depth_imgui/helper_imgui_options.hps
 * ship as plain uncompiled AngelScript text alongside the compiled
 * Soma.bin.x86_64, which itself has full debug info - both were used to
 * confirm this class's approach). This class ports that real menu's
 * observable behaviour into native C++ against this engine's existing
 * cGuiSet primitives (DrawGfx/DrawFont, no AngelScript/cImGui integration
 * exists here), rather than recreating a plausible-looking approximation:
 *
 *  - The real menu's "values based on 1280x720 resolution" comment plus
 *    OptionMenu_GetScaledSize()/GetTopLeftOffset() (which scale a design
 *    canvas by the real screen's aspect ratio) is exactly what cGuiSet's
 *    own SetVirtualSize() mechanism already does elsewhere in this engine
 *    (Dark Descent's menus use it too) - so this class sets a 1280x720
 *    virtual size and then uses the real script's own design-space
 *    constants (kMainMenuButtonPos, kTitlePos, kTitleSize, ...) verbatim.
 *  - Background is the real "menu_background.tga" (not "startmenu_
 *    background.tga", which the real script never actually draws for the
 *    main menu - a wrong guess in an earlier pass), with the real
 *    phase-1 dirt-corner overlays and the animated 3-layer "cathedral"
 *    ghost-trail effect (GuiBackground_DrawCathFacePart) and title
 *    glitch-flicker (real random timers/constants, ported to plain
 *    countdown floats rather than the named-timer hash the script uses).
 *  - Menu items are real text buttons (Sansation Large Bold, real
 *    MainMenu.Continue/NewGame/LoadGame/Options/Exit captions from
 *    config/base_english.lang), not skin-drawn graphical buttons - the
 *    real OptionMenu_ButtonMainMenu() never draws button art for the main
 *    menu at all, only a highlight bar behind the focused item.
 *
 * Not reproduced (real, but out of scope for this scaffold - no save
 * system, no controller support exist here yet): the exit confirmation
 * message box, click/glitch sound effects, save-game-dependent Continue/
 * LoadGame enablement (always shows disabled, same as a real fresh
 * install), and background phases 2-5 (progress-gated, this scaffold has
 * no progress tracking so always renders phase 1).
 *
 * The Options screen (eSomaMenuScreen_Options*, reached from the main
 * menu's Options button) is the same kind of port: real layout constants
 * and widget conventions read directly out of SOMA's own
 * script/custom_depth/helper_custom_depth_imgui/helper_imgui_options.hps
 * (OptionMenu_ButtonOptions()/OptionMenu_ButtonOptionsSlider()/
 * OptionMenu_ButtonOptionsToggle()/OptionMenu_ButtonOptionsMultiSelect(),
 * kOptionsBgPos/kOptionMenu_* constants) and script/modules/MenuHandler.hps
 * (GuiOptions()/GuiOptionsGameplay()/GuiOptionsInput()/GuiOptionsVideo()/
 * GuiOptionsVideoDisplay()/GuiOptionsVideoPostEffect()/
 * GuiOptionsVideoWorld()/GuiOptionsVideoGamma()/GuiOptionsAudio()) - the
 * real menu tree is Gameplay/Controls/Video{Display,PostEffect,World,
 * Gamma}/Audio, each its own sub-screen, cross-referenced against
 * config/base_english.lang's "Menu" category for exact real caption text.
 * The FULL real tree/order/captions are reproduced (an earlier pass here
 * scoped the UI down to only rows with a live backend and omitted
 * everything else outright - user feedback that this made the Options
 * screen "not correct" compared to the real game). Rows this engine has no
 * real working backend for yet (no texture/shadow/AA/resolution system, no
 * keybinding rebinder, no subtitle renderer, ...) are still listed in their
 * real position with their real caption, just drawn grayed-out and
 * non-interactive (cSomaOptionsRow::mbEnabled) rather than omitted or
 * pretending they work - see SomaMainMenu.cpp's BuildOptionsRows() for the
 * exact per-row enabled/disabled call and why. Same real corner/border
 * frame textures (graphics/startmenu/gfx/window/menu_*.tga) and
 * options-row highlight bar ("startmenu_options_button_long.tga", NOT the
 * main menu's own "startmenu_button_long.tga" - confirmed distinct real
 * assets) the real script itself uses for this screen.
 *
 * Owned by cSomaBase, created once by InitMainMenuScene() and attached to
 * the same real camera+world viewport (mpDebugViewport) that scene already
 * creates - not a separate GUI-only viewport like cSomaSplash uses, since
 * there's a real scene behind this menu (even though main_menu.hpm itself
 * is empty).
 *
 * iUpdateable (registered via cUpdater::AddGlobalUpdate(), same idiom as
 * cSomaSplash/cSomaDebugFreeCamera) because cGui does NOT automatically
 * route mouse input to widgets - nothing in HPL2/core polls iMouse and
 * calls cGui::SendMousePos()/SendMouseClickDown() for you; the real Dark
 * Descent game module does that itself every frame in
 * amnesia/src/game/LuxInputHandler.cpp, which this Phase 0 scaffold has no
 * equivalent of. Menu items are hit-tested directly against
 * mpGuiSet->GetMousePos() (already virtual-size-converted by
 * cGui::SendMousePos) each frame, the same immediate-mode style the real
 * script's own ImGui_DoButtonExt() uses - not cWidgetButton/cGui focus
 * routing, which the real menu doesn't use either.
 */

#ifndef SOMA_MAIN_MENU_H
#define SOMA_MAIN_MENU_H

#include "hpl.h"

#include <vector>

using namespace hpl;

class cSomaBase;

//----------------------------------------------

enum eSomaMainMenuAction
{
	eSomaMainMenuAction_None,
	eSomaMainMenuAction_NewGame,
	eSomaMainMenuAction_Options,
	eSomaMainMenuAction_Exit,
};

struct cSomaMainMenuItem
{
	tWString msLabel;
	bool mbEnabled;
	eSomaMainMenuAction mAction;
	float mfRowY; // kMainMenuButtonPos.y + kOptionMenu_ButtonSpacing * row index
};

//----------------------------------------------
// Options screen - see the class comment above for what real script this
// was read out of. eSomaMenuScreen_Main is the original 5-item list;
// everything else is the Options tree, collapsed from the real game's
// Gameplay/Controls/Video{Display,PostEffect,World,Gamma}/Audio tabs down
// to just the two that have a real, live backend in this engine.

enum eSomaMenuScreen
{
	eSomaMenuScreen_Main,
	eSomaMenuScreen_OptionsRoot,		// real GuiOptions(): Gameplay/Controls/Video/Audio/Back
	eSomaMenuScreen_OptionsGameplay,	// real GuiOptionsGameplay()
	eSomaMenuScreen_OptionsControls,	// real GuiOptionsInput() top level (Keybind/MouseOptions/GamepadOptions/Back)
	eSomaMenuScreen_OptionsVideo,		// real GuiOptionsVideo() (AutoDetect/Display/PostEffect/Rendering/Gamma/Back)
	eSomaMenuScreen_OptionsVideoDisplay,	// real GuiOptionsVideoDisplay()
	eSomaMenuScreen_OptionsVideoPostEffect, // real GuiOptionsVideoPostEffect()
	eSomaMenuScreen_OptionsVideoWorld,		// real GuiOptionsVideoWorld() (captioned "Rendering")
	eSomaMenuScreen_OptionsVideoGamma,		// real GuiOptionsVideoGamma()
	eSomaMenuScreen_OptionsAudio,			// real GuiOptionsAudio()
};

// One row of the Options screen, built fresh each frame by
// BuildOptionsRows() (cheap - a handful of entries, and keeps the row list
// always in sync with the live config values it points at rather than
// risking a stale cached copy).
//
// mbEnabled reflects whether THIS engine has a real working backend for the
// row: every row here is real (present, in the real order, with the real
// caption) per script/modules/MenuHandler.hps + helper_imgui_options.hps,
// but rows this engine can't actually act on yet (no texture/shadow quality
// system, no keybinding rebinder, no subtitle renderer, ...) are still
// listed - just drawn grayed-out/non-interactive (see DrawOptionsRow()/
// UpdateOptionsMouseHitTest()) rather than omitted, per user feedback that
// omitting them made the menu look wrong compared to the real game.
struct cSomaOptionsRow
{
	enum eKind
	{
		eKind_Category,		// navigates to another eSomaMenuScreen on click (real OptionMenu_ButtonOptions)
		// real OptionMenu_ButtonOptionsToggle - NOT a checkbox/on-off-switch visually (an
		// earlier pass here got this wrong): the real widget is the exact same left/right-
		// arrow value-cycle bar eKind_MultiSelect below draws, just always exactly 2 values
		// (confirmed against real helper_imgui_options.hps's OptionMenu_OptionsToggle(), which
		// literally calls the same cycle-bar drawing as OptionMenu_OptionsSlider/MultiSelect
		// share, and against config/base_english.lang's single shared "On"/"Off" caption pair
		// used by every toggle-shaped row) - see DrawOptionsCycleControl().
		eKind_Toggle,
		eKind_Slider,		// real OptionMenu_ButtonOptionsSlider - click-to-step or drag
		eKind_Back,			// same as eKind_Category but always navigates "up"
		eKind_MultiSelect,	// real OptionMenu_ButtonOptionsMultiSelect - cycles a fixed value list
	};

	eKind mKind;
	tWString msLabel;
	bool mbEnabled;

	// eKind_Category/eKind_Back
	eSomaMenuScreen mTarget;

	// eKind_Toggle
	bool *mpBoolValue;

	// eKind_Slider - mpFloatValue points at the real backing value in its
	// own real-world units (e.g. Gamma is 0.3-2.0, Volume is 0-1); mfMin/
	// mfMax give that range so the widget can normalize it to the 0..1 bar
	// position/step math OptionMenu_ButtonOptionsSlider() itself uses.
	float *mpFloatValue;
	float mfMin;
	float mfMax;
	float mfStep; // step size in normalized 0..1 units, same as the real afStepSize param

	// eKind_Slider only - real OptionMenu_ButtonOptionsSlider()'s optional
	// "asTextValue" param: empty for Gamma/Volume (real script passes ""),
	// a formatted number for FOV (the one real slider that shows a trailing
	// value) - real helper_imgui_options.hps draws a visually narrower bar
	// with the value text after it when this is non-empty (see
	// kOptionMenu_TextedSlider* constants in DrawOptionsRow()'s eKind_Slider
	// case), a plain full-width bar with no text at all otherwise.
	tWString mSliderValueText;

	// eKind_MultiSelect - no live backend exists for any real multi-select
	// setting yet (Resolution/RefreshRate/TextureQuality/TextureFiltering/
	// ShadowQuality/AA/Language), so these are always built with mbEnabled
	// false and mlOptionIndex fixed at the real script's own default value -
	// display-only, real captions/value lists, real position in the tree.
	//
	// eKind_Toggle reuses these same two fields rather than duplicating the
	// cycle-widget machinery: mOptions = {offLabel, onLabel} (real captions,
	// e.g. {"WINDOWED","FULLSCREEN"} for Display Mode, {"OFF","ON"} for
	// everything else per base_english.lang), mlOptionIndex = current bool
	// state (0/1) - see MakeToggleRow()/DrawOptionsCycleControl().
	std::vector<tWString> mOptions;
	int mlOptionIndex;
};

//----------------------------------------------
// Real "ocean detritus floating over the menu" effect - MenuHandler.hps's
// cImGuiParticleEmitter class + DrawParticles() (real menu-only particle
// systems, reusing the same "dust_light_tiny.dds"/"dust_cloud.dds" sprites
// the real in-game world particle systems under particles/dust/ use, not
// dedicated startmenu-only art). Ported field-for-field rather than
// approximated - see SomaMainMenu.cpp's CreateParticleEmitters() for the
// real constants (spawn rect/velocity/size/life/colour) copied straight out
// of DrawParticles(), and UpdateParticleEmitter()/DrawParticleEmitter() for
// the real per-particle spawn/fade logic.

struct cSomaMenuParticle
{
	cVector3f mvPos;
	cVector3f mvVel;
	float mfSize;
	float mfLife; // 0..1 normalized, real "mvParticleLifeValues"
	float mfLifeStepMul; // 1 / lifetime-in-seconds
	cColor mStartColor;
};

struct cSomaMenuParticleEmitter
{
	// Real cImGuiParticleEmitter always draws gfx index 0 regardless of how
	// many variants mvGfx holds (its own random-index pick is commented out
	// in the real script) - kept as a vector anyway to mirror the real
	// class shape/intent (see CreateParticleEmitters() for the real
	// "menu_bg_noise"-style dust_cloud.dds 4-quadrant variants this
	// preserves without ever actually cycling through them, matching the
	// real game's own observed behaviour exactly, quirk included).
	std::vector<cGuiGfxElement *> mvGfx;
	eGuiMaterial mMaterial;

	cVector3f mvMin, mvMax;
	cVector3f mvVelocityMin, mvVelocityMax;
	float mfSizeMin, mfSizeMax;
	int mlMaxParticles;
	float mfParticlesPerSec;
	float mfNewParticleTimer;
	float mfMinLife, mfMaxLife;
	cColor mColorStartMin, mColorStartMax;
	cColor mColorMulStart, mColorMulMiddle, mColorMulEnd;
	float mfColorMulEndStartTime;

	std::vector<cSomaMenuParticle> mvParticles;
};

//----------------------------------------------

class cSomaMainMenu : public iUpdateable
{
public:
	cSomaMainMenu(cEngine *apEngine, cSomaBase *apBase, cViewport *apViewport);
	~cSomaMainMenu();

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);

	// Hides the menu and stops it from eating mouse input, without
	// destroying it - used once "New Game" has actually loaded a map, so
	// the buttons don't keep drawing/intercepting clicks over gameplay.
	void SetVisible(bool abVisible);

private:
	void CreateGui();
	cGuiGfxElement *CreateGfx(const tString &asFile, eGuiMaterial aMaterial);

	void DrawBackground(float afTimeStep);
	void DrawCathFacePart(cGuiGfxElement *apGfx, const cVector3f &avFrontCenterPos, const cVector3f &avBackCenterPos,
						  float afFrontScale, float afBackScale, const cColor &aFrontCol, const cColor &aBackCol, int alLayers);
	void DrawTitle(float afTimeStep);
	void DrawMenuItems();

	// Real DrawParticles() "ocean detritus" effect - see the class comment
	// above cSomaMenuParticleEmitter.
	void CreateParticleEmitters();
	void UpdateParticleEmitter(cSomaMenuParticleEmitter &aEmitter, float afTimeStep);
	void DrawParticleEmitter(cSomaMenuParticleEmitter &aEmitter, float afZ);

	void UpdateMouseHitTest();
	void ClickItem(cSomaMainMenuItem &aItem);
	void RunPendingAction();

	////////////////////////////////////
	// Options screen (eSomaMenuScreen_Options*) - see the class comment and
	// SomaMainMenu.cpp for the real script this was read out of.
	void CreateOptionsGui();
	void BuildOptionsRows();
	void NavigateTo(eSomaMenuScreen aScreen);

	void DrawOptionsScreen();
	void DrawOptionsPanel(const cVector2f &avPos, const cVector2f &avSize);
	void DrawOptionsRow(const cSomaOptionsRow &aRow, int alIndex, bool abSelected);
	// Shared real "startmenu_options_button_meter" + left/right
	// "startmenu_options_arrow" cycle-bar widget (real
	// OptionMenu_OptionsToggle()/OptionMenu_OptionsMultiSelect() both draw
	// this exact same thing, just with a different value list length) - used
	// by DrawOptionsRow() for both eKind_Toggle and eKind_MultiSelect so the
	// two kinds can never visually drift apart again.
	void DrawOptionsCycleControl(float afRowY, const tWString &asValueText, const cColor &aBarCol, const cColor &aArrowCol, const cColor &aTextCol);

	void UpdateOptionsMouseHitTest();
	void ClickOptionsRow(int alIndex);
	// Continues a slider drag started on a previous frame - called every
	// frame the mouse button stays down after a drag began on
	// mlDraggingRow, same "repeat while held" behaviour as the real
	// script's ImGui_DoRepeatButtonExt() inside OptionMenu_ButtonOptionsSlider().
	void UpdateOptionsSliderDrag();

	cEngine *mpEngine;
	cSomaBase *mpBase;
	cViewport *mpViewport;

	cGui *mpGui;
	cGuiSkin *mpGuiSkin;
	cGuiSet *mpGuiSet;

	iFontData *mpButtonFont;

	// Real SOMA cursor (graphics/imgui/default/imgui_pointer_normal.tga) -
	// wired via cGuiSet::SetCurrentPointer(), see the constructor.
	cGuiGfxElement *mpCursorGfx;

	// Real SOMA menu art (graphics/startmenu/), drawn directly via
	// DrawGfx() rather than cWidgetImage - see SomaMainMenu.cpp for why.
	cGuiGfxElement *mpBackgroundGfx;
	cGuiGfxElement *mpCornerUL;
	cGuiGfxElement *mpCornerUR;
	cGuiGfxElement *mpCornerBL;
	cGuiGfxElement *mpCornerBR;
	cGuiGfxElement *mpCathLeft;
	cGuiGfxElement *mpCathRight;
	cGuiGfxElement *mpCathJaw;
	cGuiGfxElement *mpTitleGfx;
	cGuiGfxElement *mpTitleFlickerGfx[4];
	cGuiGfxElement *mpButtonBarGfx;
	cGuiGfxElement *mpButtonBarJitterGfx[3];

	// Fade-in state (real mbBackgroundShowTitle/Face + mfBackgroundTitle/
	// FaceAlpha - this scaffold skips the premenu cinematic, so both start
	// fading in immediately, same as ShowMainMenu(true) outside premenu).
	float mfTitleAlpha;
	float mfFaceAlpha;
	float mfBGAnimTime;

	// Title glitch-flicker state (real "TitleGlitchWait"/"TitleGlitch"
	// named timers, ported to plain countdowns).
	float mfTitleGlitchWaitTimer;
	int mlTitleGlitchTimes;
	float mfTitleGlitchTimer;
	int mlTitleGlitchPic; // 0 = normal, 1-4 = startmenu_title_flickerN.tga

	// Title colour pulse (real "TitlePulse" timer + "TitleColor" fade).
	float mfTitlePulseTimer;
	cColor mTitleColorStart;
	cColor mTitleColorGoal;
	float mfTitleColorFadeT;
	float mfTitleColorFadeLen;

	cSomaMainMenuItem mItems[5];
	int mlHoveredItem;
	int mlClickedItem;
	float mfButtonClickedTimer; // real "ButtonClicked" 0.15s flash-then-act delay
	eSomaMainMenuAction mPendingAction;

	bool mbVisible;
	bool mbMouseWasDown;

	////////////////////////////////////
	// Options screen state (see SomaMainMenu.h's class comment)
	eSomaMenuScreen mScreen;
	std::vector<cSomaOptionsRow> mOptionsRows; // rebuilt each frame by BuildOptionsRows()
	int mlOptionsHoveredRow;
	int mlDraggingSliderRow; // -1 when not dragging - see UpdateOptionsSliderDrag()

	// Real corner/border frame (graphics/startmenu/gfx/window/menu_*.tga),
	// same asset set MenuHandler.hps's mGfxFrame uses for every Options
	// sub-screen's background panel.
	cGuiGfxElement *mpFrameCornerTL;
	cGuiGfxElement *mpFrameCornerTR;
	cGuiGfxElement *mpFrameCornerBL;
	cGuiGfxElement *mpFrameCornerBR;
	cGuiGfxElement *mpFrameBorderTop;
	cGuiGfxElement *mpFrameBorderBottom;
	cGuiGfxElement *mpFrameBorderLeft;
	cGuiGfxElement *mpFrameBorderRight;
	cGuiGfxElement *mpFrameFillGfx; // plain filled rect, real mGfxFrame.mGfxBackground translucent fill

	// Real Options-row widget art (helper_imgui_options.hps) - distinct
	// from the main menu's own mpButtonBarGfx/mpButtonBarJitterGfx above.
	cGuiGfxElement *mpOptionsHighlightGfx; // "startmenu_options_button_long" - selected-row bar
	cGuiGfxElement *mpOptionsMeterGfx;		// "startmenu_options_button_meter" - slider background
	cGuiGfxElement *mpOptionsArrowGfx;		// "startmenu_options_arrow"
	cGuiGfxElement *mpOptionsBarGfx;		// plain filled rect, slider track/handle

	////////////////////////////////////
	// Real "ocean detritus" particle effect (DrawParticles()) - see the
	// class comment above cSomaMenuParticleEmitter for what real script this
	// was read out of.
	cSomaMenuParticleEmitter mEmitterLowerHalf;
	cSomaMenuParticleEmitter mEmitterUpperHalf;
	cSomaMenuParticleEmitter mEmitterLarge;
	cSomaMenuParticleEmitter mEmitterSmoke;
};

//----------------------------------------------

#endif // SOMA_MAIN_MENU_H
