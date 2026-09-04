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
 * system, no options menu, no controller support exist here yet): the
 * exit confirmation message box, click/glitch sound effects, save-game-
 * dependent Continue/LoadGame enablement (always shows disabled, same as
 * a real fresh install), and background phases 2-5 (progress-gated, this
 * scaffold has no progress tracking so always renders phase 1).
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
};

//----------------------------------------------

#endif // SOMA_MAIN_MENU_H
