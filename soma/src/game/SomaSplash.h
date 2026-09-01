/*
 * Splash-screen sequence shown before SOMA's main menu scene loads.
 *
 * SOMA's real install has no plain-text pre_menu.cfg like Dark Descent's -
 * its actual splash content is just graphics/startmenu/premenu/
 * frictional_games_logo.dds (the "premenu" folder itself, alongside a
 * loading_bar.dds/loading_frame.dds pair, confirms this is the real boot
 * splash asset) plus graphics/imgui/credits/soma_logo_splash_static.dds
 * (the "static" in the name marks it as the non-animated variant meant for
 * exactly this kind of use, as opposed to fg_logo_splash.dds which is the
 * animated closing-credits version - not used here).
 *
 * Modeled on the *mechanism* amnesia/src/game/LuxPreMenu.cpp uses (a
 * cGuiSet with cGuiGfxElement images drawn via DrawGfx on a GUI-only
 * viewport - see cLuxPreMenuSection::CreateBackground()/cLuxPreMenu::
 * OnDraw()), not the class itself - LuxPreMenu is wired into LuxBase's
 * container/state machinery this Phase 0 scaffold doesn't have.
 *
 * Note this codebase's iUpdateable objects, once registered with
 * cUpdater::AddGlobalUpdate(), can never be removed (see cSomaBase::
 * ExitTestMap()'s comment on cSomaDebugFreeCamera for the same
 * constraint) - so this class stays alive for the whole process and just
 * goes inert (mbFinished) once its sequence is done, rather than actually
 * being torn down.
 */

#ifndef SOMA_SPLASH_H
#define SOMA_SPLASH_H

#include "hpl.h"

#include <vector>

using namespace hpl;

class cSomaBase;

//----------------------------------------------

struct cSomaSplashImage
{
	tString msFile;
	float mfHoldTime; // includes fade in/out, see mfFadeTime
};

//----------------------------------------------

class cSomaSplash : public iUpdateable
{
public:
	cSomaSplash(cEngine *apEngine, cSomaBase *apBase);
	~cSomaSplash();

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);

private:
	void LoadCurrentImage();
	void AdvanceToNextImage();
	void Finish();

	bool AnySkipInputThisFrame();
	float ComputeFadeAlpha() const;

	cEngine *mpEngine;
	cSomaBase *mpBase;

	cGui *mpGui;
	cGuiSkin *mpGuiSkin;
	cGuiSet *mpGuiSet;
	cViewport *mpViewport;

	cVector2f mvScreenSize;

	cGuiGfxElement *mpBlackBg;
	cGuiGfxElement *mpCurrentImage;

	std::vector<cSomaSplashImage> mvImages;
	size_t mlCurrentIndex;

	float mfTimer;

	bool mbFinished;
	bool mbMouseWasDown;

	static const float mfFadeTime;
};

//----------------------------------------------

#endif // SOMA_SPLASH_H
