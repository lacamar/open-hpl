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
 * OptionMenu_ButtonOptionsToggle(), kOptionsBgPos/kOptionMenu_* constants)
 * and script/modules/MenuHandler.hps (GuiOptions()/GuiOptionsAudio()/
 * GuiOptionsVideoDisplay()/GuiOptionsVideoGamma() - the real menu tree is
 * Gameplay/Controls/Video{Display,PostEffect,World,Gamma}/Audio, each its
 * own sub-screen), scoped down to only what this engine actually has a
 * live backend for rather than the full 8-tab tree - see SomaMainMenu.cpp's
 * "Real Options screen" comment block for exactly what was kept/cut and
 * why. Same real corner/border frame textures (graphics/startmenu/gfx/
 * window/menu_*.tga) and options-row highlight bar
 * ("startmenu_options_button_long.tga", NOT the main menu's own
 * "startmenu_button_long.tga" - confirmed distinct real assets) the real
 * script itself uses for this screen.
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
	eSomaMenuScreen_OptionsRoot,	// real GuiOptions(): Audio/Display/Back
	eSomaMenuScreen_OptionsAudio,	// real GuiOptionsAudio(), Volume only
	eSomaMenuScreen_OptionsDisplay, // real GuiOptionsVideoDisplay()+VideoGamma(), collapsed into one page
};

// One row of the Options screen, built fresh each frame by
// BuildOptionsRows() (cheap - a handful of entries, and keeps the row list
// always in sync with the live config values it points at rather than
// risking a stale cached copy).
struct cSomaOptionsRow
{
	enum eKind
	{
		eKind_Category, // navigates to another eSomaMenuScreen on click (real OptionMenu_ButtonOptions)
		eKind_Toggle,	// real OptionMenu_ButtonOptionsToggle - On/Off checkbox pair
		eKind_Slider,	// real OptionMenu_ButtonOptionsSlider - click-to-step or drag
		eKind_Back,		// same as eKind_Category but always navigates "up"
	};

	eKind mKind;
	tWString msLabel;

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
	cGuiGfxElement *mpOptionsCheckOnGfx;	// "startmenu_options_button_on"
	cGuiGfxElement *mpOptionsCheckOffGfx;	// "startmenu_options_button_off"
	cGuiGfxElement *mpOptionsBarGfx;		// plain filled rect, slider track/handle
};

//----------------------------------------------

#endif // SOMA_MAIN_MENU_H
