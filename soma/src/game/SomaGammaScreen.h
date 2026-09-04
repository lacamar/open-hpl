/*
 * First-boot gamma calibration screen, shown once between the splash logos
 * and the main menu.
 *
 * Real SOMA gates this via MenuHandler.hps's mbPremenuActive flag ->
 * GuiPreMenu()/GuiGammaCorrection() (confirmed by reading that file
 * directly out of a real install), using real assets
 * graphics/startmenu/misc/gamma.tga (a checkerboard test pattern) and
 * gamma_background.tga. Also reachable later via the real Options menu,
 * not reproduced here - this class is only the first-boot calibration
 * step.
 *
 * The actual gamma-adjustment backend (SetGammaCorrection()/
 * GetGammaCorrection() on iLowLevelGraphics, backed by real
 * SDL_SetWindowBrightness/SDL gamma ramp calls) already exists and is
 * proven working in Dark Descent's own cLuxPreMenu
 * (amnesia/src/game/LuxPreMenu.{h,cpp}) - this class mirrors that
 * mechanism (a cWidgetSlider driving the same backend), not Dark
 * Descent's own multi-section pre-menu state machine, which this
 * Phase 0/1 scaffold has no equivalent of.
 *
 * "First boot" is tracked by a marker file under this project's own XDG
 * state directory (same $XDG_STATE_HOME/open-hpl/soma/ SomaBase.cpp
 * already uses for hpl.log) - not persisted game state, just "has this
 * install seen the calibration screen once."
 */

#ifndef SOMA_GAMMA_SCREEN_H
#define SOMA_GAMMA_SCREEN_H

#include "hpl.h"

using namespace hpl;

class cSomaBase;

//----------------------------------------------

class cSomaGammaScreen : public iUpdateable
{
public:
	cSomaGammaScreen(cEngine *apEngine, cSomaBase *apBase);
	~cSomaGammaScreen();

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);

	// Returns true exactly once, the first time this install has ever
	// reached this check - used by cSomaBase::OnSplashFinished() to decide
	// whether to show this screen at all. Also marks the flag as "seen" as
	// a side effect (matches the real game only ever showing this once per
	// install, not once per boot).
	static bool ShouldShowAndMarkSeen();

private:
	void Finish();
	bool AnyContinueInputThisFrame();

	static bool GammaSliderMoved_static_gui(void *apObject, iWidget *apWidget, const cGuiMessageData &aData);
	bool GammaSliderMoved(iWidget *apWidget, const cGuiMessageData &aData);

	static bool ContinuePressed_static_gui(void *apObject, iWidget *apWidget, const cGuiMessageData &aData);
	bool ContinuePressed(iWidget *apWidget, const cGuiMessageData &aData);

	cEngine *mpEngine;
	cSomaBase *mpBase;

	cGui *mpGui;
	cGuiSkin *mpGuiSkin;
	cGuiSet *mpGuiSet;
	cViewport *mpViewport;

	cVector2f mvScreenSize;

	cGuiGfxElement *mpBackgroundGfx;
	cGuiGfxElement *mpCheckerboardGfx;
	cVector2f mvCheckerboardPos;
	cVector2f mvCheckerboardSize;

	cWidgetSlider *mpSlider;
	cWidgetButton *mpContinueButton;

	float mfGammaMinValue;
	float mfGammaMaxValue;

	bool mbFinished;
	bool mbMouseWasDown;
};

//----------------------------------------------

#endif // SOMA_GAMMA_SCREEN_H
