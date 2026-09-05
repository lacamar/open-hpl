/*
 * Splash-screen sequence shown before SOMA's main menu scene loads.
 *
 * Fully reverse-engineered this pass (previous version of this class was a
 * folder/naming-convention guess - two logos crossfading - which the user
 * confirmed did not match the real game). Real evidence, all cited inline
 * below and in PORTING_NOTES.md:
 *
 * 1. SOMA's real, NATIVE (compiled into Soma.bin.x86_64, not AngelScript)
 *    boot splash is declared by config/game.cfg's <General> block:
 *        SplashScreen        = "Premenu.png"
 *        SplashScreenMusic   = "loadscreen_background"
 *        SplashScreenMusicVol= "0.15"
 *        LoadingBar          = "graphics/startmenu/premenu/loading_bar.dds"
 *        LoadingFrame        = "graphics/startmenu/premenu/loading_frame.dds"
 *    Confirmed native (not script): `strings Soma.bin.x86_64` contains the
 *    literal config-key names "SplashScreen"/"SplashScreenMusic"/
 *    "SplashScreenMusicVol"/"SplashScreenMusicFreq" plus the literal value
 *    "Premenu.png" - these are read directly by compiled code, not exposed
 *    anywhere in script/modules/MenuHandler.hps (grepped, zero hits) or any
 *    .lang file (also zero hits for "INITIALIZATION" anywhere in game data -
 *    that text is baked directly into the Premenu.png pixels, not drawn by
 *    a separate text call).
 * 2. graphics/startmenu/premenu/Premenu.png (1920x1080) is a real reference
 *    composite shipped by Frictional themselves: viewed directly, it shows
 *    a glitch/scanline effect, "INITIALIZATION..." text, and a fading-in
 *    preview of "LOAD"/"OPTIONS" menu buttons - i.e. the real boot splash
 *    *is* this exact glitchy image, not a clean logo card.
 * 3. graphics/startmenu/premenu/loading_bar.dds (1024x128) decodes (real
 *    DDS header, confirmed via `identify -verbose`) to base color
 *    srgba(255,0,0,0) - i.e. genuinely RED - matching the user's own
 *    description of "a red progress-bar-like graphic". Visually it's a
 *    jagged/stepped waveform-style readout, not a plain rectangle.
 *    loading_frame.dds (same 1024x128 dims) is a thin cyan tick-mark/grid
 *    decoration meant to sit over/around it.
 * 4. graphics/imgui/credits/soma_logo_splash_static.dds - the previous
 *    version of this class's second "logo" - is NOT part of the boot
 *    splash at all. Opened directly: it's the glitchy "SOMA" wordmark with
 *    a targeting-reticle "O", and its sibling fg_logo_splash.dds (same
 *    folder, "animated" per its lack of "_static") is a second FRICTIONAL
 *    GAMES gear logo, not a SOMA variant. Both live under
 *    graphics/imgui/credits/ - real evidence (by location, since neither
 *    filename is referenced anywhere in script/lang data either) that
 *    these two are end-credits assets, unrelated to the boot splash. Using
 *    soma_logo_splash_static.dds here was a real bug in the previous
 *    version of this file - removed.
 * 5. script/modules/MenuHandler.hps's cScrMenuHandler::GuiPreMenu() is a
 *    SEPARATE, later phase - the *scripted* pre-menu (FG logo -> optional
 *    controller "Press X" engagement -> optional gamma calibration ->
 *    main menu). Its FG-logo sub-state (mlPreMenuState==0) gives exact,
 *    real timing/SFX this class now replicates for its own FG-logo phase:
 *      - `ImGui_AddTimer("FGLogoOver", 3)` - 3s hold timer, started the
 *        instant the phase begins (concurrently with fade-in, not after).
 *      - Fade-in rate 0.4/s while entering this phase (`mfPreMenuFadeAmount
 *        += afTimeStep*0.4`), i.e. ~2.5s to reach full opacity.
 *      - Fade-out rate 0.5/s once the 3s timer fires (`mfPreMenuFadeDest=0`
 *        makes fMul resolve to 0.5 for this phase) - i.e. ~2s fade-out.
 *      - `Sound_PlayGui("special_fx/frontend/FG_Menu_Sting", ...)` and
 *        `Music_PlayExt("IngameMenu_Music", ...)` fire the instant the FG
 *        logo phase begins.
 *    Real FG logo image/box size in script is
 *    `cVector2f(1024, 351) * 0.87` via a menu-only helper
 *    (`OptionMenu_GetCenterOffset`) this scaffold has no equivalent of;
 *    kept this class's existing (already visually correct per user
 *    feedback) full-screen aspect-fit sizing for frictional_games_logo.dds
 *    rather than risk a regression chasing that exact undocumented box.
 * 6. SFX: `special_fx/frontend/FG_Menu_Sting` and
 *    `special_fx/frontend/main_menu_bg` are both FMOD-banked (the
 *    human-readable special_fx.fdp "project" file lists both event names,
 *    but is not itself playable audio - the real compiled sample data
 *    lives in sounds/special/special_fx.fsb and
 *    sounds/special/special_fx_stream.fsb). A concurrent session this pass
 *    was already building soma/src/game/SomaMenuSfx.{h,cpp} - a real,
 *    bounded FSB5 parser (see that file's own extensive header comment)
 *    for SOMA's menu click/hover/glitch SFX - so per this class's own
 *    scope note ("reuse it rather than duplicating"), that file's PCM16
 *    extraction path (already used for `new_game_sting`) was extended
 *    with two more real sample names this pass found via `strings` on
 *    special_fx_stream.fsb: `FG_Logo_Sting` and `menu_bg_noise` -
 *    confirming that file's own documented guess ("FG_Menu_Sting ... may
 *    resolve to FG_Logo_Sting") and resolving `main_menu_bg` too. Exposed
 *    as `cSomaMenuSfx::FGLogoSting()`/`MenuBgNoise()`, played here via the
 *    same `cSoundHandler::PlayGui()` call soma/src/game/SomaMainMenu.cpp
 *    already uses for its own menu SFX - both fire the instant the FG
 *    logo phase begins, matching the real script's `Sound_PlayGui(...
 *    FG_Menu_Sting)`/`Sound_CreateAtEntity(... main_menu_bg)` calls at
 *    `mlPreMenuState==0` (see point 5's citation for that block).
 *    `SplashScreenMusic="loadscreen_background"` resolves to
 *    music/LoadScreen/loadscreen_background.ogg - a REAL PLAIN VORBIS OGG
 *    FILE (confirmed via `file`), playable through this engine's existing
 *    cMusicHandler the exact way soma/src/game/SomaMainMenu.cpp already
 *    plays "Menu_Music.ogg" - wired in for the boot/init phase below.
 *
 * Two real phases result, both drawn over a constant opaque black base
 * layer (so each phase's own fade-in starts from a true black, matching
 * the real script's separate black-screen fade-up before FG logo without
 * needing to model that as a distinct third phase):
 *   ePhase_FGLogo    - frictional_games_logo.dds, real ~2.5s/~0.5s/~2s
 *                       fade-in/hold/fade-out timing (see point 5 above).
 *   ePhase_BootInit  - Premenu.png full-bleed + loading_bar.dds (clipped
 *                       horizontally to reveal a growing fraction) and
 *                       loading_frame.dds (static decoration) overlaid.
 *                       This port completes all real engine boot work
 *                       (resource dir mounts, shader compiles, etc.)
 *                       *before* cSomaSplash is even constructed - there
 *                       is no real, granular progress signal left to
 *                       sample by this point (same "no true percentage
 *                       tracking anywhere in this engine" finding
 *                       documented elsewhere in PORTING_NOTES.md for
 *                       Dark Descent's own loading screen) - so the bar's
 *                       fill fraction here is a smooth, honest TIME-based
 *                       animation across the phase's fixed duration, not
 *                       a fabricated "phase" breakdown that would imply
 *                       real work is being tracked when it isn't. The
 *                       bar/frame's on-screen position is this class's own
 *                       reasonable placement (roughly under where
 *                       Premenu.png's own baked "INITIALIZATION..." text
 *                       sits) - the real position is native/closed code
 *                       with no further evidence recovered this pass.
 *
 * Modeled on the *mechanism* amnesia/src/game/LuxPreMenu.cpp uses (a
 * cGuiSet with cGuiGfxElement images drawn via DrawGfx on a GUI-only
 * viewport), not the class itself.
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

using namespace hpl;

class cSomaBase;

//----------------------------------------------

enum eSomaSplashPhase
{
	eSomaSplashPhase_FGLogo,
	eSomaSplashPhase_BootInit,
	eSomaSplashPhase_Done
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
	void EnterPhase(eSomaSplashPhase aPhase);
	void AdvanceToNextPhase();
	void Finish();

	bool AnySkipInputThisFrame();

	void DrawFGLogoPhase();
	void DrawBootInitPhase();

	cEngine *mpEngine;
	cSomaBase *mpBase;

	cGui *mpGui;
	cGuiSkin *mpGuiSkin;
	cGuiSet *mpGuiSet;
	cViewport *mpViewport;

	cVector2f mvScreenSize;

	cGuiGfxElement *mpBlackBg;
	cGuiGfxElement *mpFGLogo;
	cGuiGfxElement *mpPremenuBg;
	cGuiGfxElement *mpLoadingBar;
	cGuiGfxElement *mpLoadingFrame;

	// Persistent (created once, reused every frame) clip region used to
	// reveal only the left mfBarFillFraction of mpLoadingBar - cGuiSet's
	// DrawGfx() has no source-rect/UV-crop parameter of its own, but
	// cGuiClipRegion::CreateChild() allocates a new heap node on every
	// call and is only ever freed when its *parent* region is destroyed
	// (see cGuiClipRegion::Clear()/STLDeleteAll in GuiSet.cpp) - calling
	// it every frame here would leak one node per frame for the life of
	// the process, so this is allocated exactly once and only its mRect
	// is mutated per frame instead.
	cGuiClipRegion *mpBarClipRegion;

	eSomaSplashPhase mPhase;
	float mfPhaseElapsed;

	bool mbFinished;
	bool mbMouseWasDown;
	bool mbSplashMusicStarted;

	static const float mfFGFadeInTime;
	static const float mfFGHoldTimerTotal;
	static const float mfFGFadeOutTime;

	static const float mfBootFadeTime;
	static const float mfBootHoldTime;
};

//----------------------------------------------

#endif // SOMA_SPLASH_H
