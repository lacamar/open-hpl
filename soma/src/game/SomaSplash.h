/*
 * Splash-screen sequence shown before SOMA's main menu scene loads.
 *
 * PHASE ORDER CORRECTED this pass. The user reported (backed by their own
 * reference screenshots) that the real boot order is the glitchy boot-init
 * screen (Premenu.png/red loading bar/brain icon) FIRST, then the
 * Frictional Games logo SECOND - the opposite of what this class had. Real,
 * primary-evidence confirmation via the unstripped Soma.bin.x86_64:
 *   - `nm -C`/`objdump -d` on cLuxBase::Init() (vaddr 0x8baf40) disassembles
 *     to a fixed call sequence: InitApp() -> InitConfig() -> InitEngine()
 *     -> CheckFeatureSupport() -> InitScript() -> InitLoadHandler() -> [a
 *     vtable dispatch that starts the real Run() loop]. InitLoadHandler()
 *     (0x8d14f0) itself calls `cLuxLoadHandler::cLuxLoadHandler()`
 *     (0xb1a6f0 - the ctor that reads LoadingIcon/SplashScreen/LoadingBar/
 *     LoadingFrame/SplashScreenMusic from game.cfg, see point 1 below) and
 *     immediately `cLuxLoadHandler::AddJob()` (0xb1d060) - i.e. the native
 *     boot-init screen and its first background job are set up and start
 *     rendering/running BEFORE the main loop (and therefore before any
 *     script) ever runs a single frame.
 *   - `script/modules/MenuHandler.hps`'s `GuiPreMenu()` (the FG-logo phase,
 *     see point 5 below) is only ever reached from `cScrMenuHandler`'s main
 *     Update() switch, from the `case eMainMenuGroup_Main:` branch, ONLY
 *     when `mbMainMenuActive` is already true (~line 1611) - i.e. it is
 *     part of the *already-loaded, already-active main menu's own* update
 *     loop, not something that can run before the main menu scene exists.
 *   Together these prove the real order: the native cLuxLoadHandler boot
 *   splash (Premenu.png/loading bar/brain icon) renders first, while the
 *   main menu's resources/scene load as a background job; only once that
 *   job finishes and the main menu becomes active does script code start
 *   running GuiPreMenu()'s FG-logo sub-state. This class's phase enum/
 *   EnterPhase()/AdvanceToNextPhase() below now start at eSomaSplashPhase_
 *   BootInit and advance to eSomaSplashPhase_FGLogo, matching this.
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
 * needing to model that as a distinct third phase). ORDER (corrected this
 * pass, see the file-top note above): ePhase_BootInit runs FIRST,
 * ePhase_FGLogo SECOND.
 *   ePhase_BootInit  - Premenu.png full-bleed + loading_bar.dds (clipped
 *                       horizontally to reveal a growing fraction) and
 *                       loading_frame.dds (static decoration) overlaid.
 *                       This port used to complete all real engine boot
 *                       work (resource dir mounts, shader compiles, etc.)
 *                       *before* cSomaSplash was even constructed, leaving
 *                       no real, granular progress signal to sample here
 *                       (same "no true percentage tracking anywhere in
 *                       this engine" finding documented elsewhere in
 *                       PORTING_NOTES.md for Dark Descent's own loading
 *                       screen). This pass moved the one remaining real,
 *                       heavy piece of boot work still left to do at this
 *                       point - loading the main menu's actual cWorld -
 *                       into this phase itself (see EnterPhase()'s own
 *                       comment and cSomaBase::PreloadMainMenuWorld()), so
 *                       there are now two real (not fabricated) steps this
 *                       class can observe complete: engine+resources
 *                       initialized (always already true by the time this
 *                       class exists) and the main menu world loaded (now
 *                       genuinely triggered from here). The bar's own
 *                       moment-to-moment FILL ANIMATION is still an honest
 *                       smooth TIME-based ramp across the phase's fixed
 *                       cosmetic duration - true sub-second granular
 *                       progress within loading one .hpm world isn't
 *                       recoverable without a full threaded job queue
 *                       (out of scope - see the reverted real-tonemap-pass
 *                       precedent in PORTING_NOTES.md for why that class of
 *                       change is deliberately not attempted here) - but
 *                       mbRealBootWorkDone (set the instant that real load
 *                       call returns, in EnterPhase()) now REALLY gates
 *                       skip/phase-advance out of this phase (see Update()),
 *                       instead of relying on an accidental side effect of
 *                       unrelated init-order code elsewhere. The bar/frame's
 *                       on-screen size and horizontal position ARE real
 *                       evidence (not a guess) - `nm -C`/`objdump -d` on the
 *                       real, unstripped Soma.bin.x86_64 locates the native
 *                       cLuxLoadHandler class that game.cfg's <General>
 *                       block belongs to (its constructor reads
 *                       "LoadingIcon"/"SplashScreen"/"LoadingBar"/
 *                       "LoadingFrame"/"SplashScreenMusic" in that exact
 *                       order) and its OnDraw() disassembles to a DrawGfx()
 *                       call for the bar/frame with a literal, hardcoded
 *                       (1024, 128) size - the assets' own exact native
 *                       pixel dimensions - and a "-512.0" position constant
 *                       (exactly half that width) applied to a
 *                       screen-width*0.5 term, confirming horizontal
 *                       centering. See DrawBootInitPhase()'s own comment in
 *                       SomaSplash.cpp for the full citation, including the
 *                       vertical position (a "-256.0" constant read as a
 *                       bottom-edge anchor - this part is this pass's best
 *                       inference, not runtime-confirmed).
 *   ePhase_FGLogo    - frictional_games_logo.dds, real ~2.5s/~0.5s/~2s
 *                       fade-in/hold/fade-out timing (see point 5 above).
 *                       Only reached once ePhase_BootInit's own real-work
 *                       gate has opened, so by construction the real main
 *                       menu world is always already loaded by the time
 *                       this phase can ever be skipped.
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
 *
 * Fixes/investigation added this pass:
 *
 * 7. Real "brainscan" loading icon, missing entirely until now.
 *    config/game.cfg's <General> block also has `LoadingIcon =
 *    "brain_01.dds"` - read by the same native cLuxLoadHandler constructor
 *    cited in point 6 above, right alongside SplashScreen/LoadingBar/
 *    LoadingFrame, i.e. it's meant to composite with the boot-init phase
 *    too. The real install ships a genuine 26-frame animated sequence at
 *    graphics/general/loadscreen/brainAnim/brain_01.dds .. brain_26.dds
 *    (each a real, distinct 512x512 DDS frame - confirmed via `identify`
 *    across the whole set, not just brain_01). DrawBrainIcon() loads all
 *    26 once and cycles them in the bottom-right corner for the whole
 *    boot-init phase.
 *    Follow-up disassembly this pass (`cLuxLoadHandler::DrawBigIcon(int,
 *    float)`, vaddr 0xb1e710, and `::DrawSmallIcon(int, float)`, 0xb1e420 -
 *    both called from a branch in `OnDraw()` on a `mbUseSmallIcon` flag at
 *    offset 0x9c that the constructor never explicitly sets, i.e. defaults
 *    false/big): DrawBigIcon's own `DrawGfx()` call uses a literal,
 *    hardcoded (512.0, 512.0) size immediate - the brain frames' own exact
 *    native pixel dimensions, i.e. real evidence the icon is meant to
 *    render at full native res, not scaled down small. Also found: OnDraw()
 *    accumulates an elapsed-time counter multiplied by a literal double
 *    constant 15.0 (vaddr 0x19bf748) and compares it against 2x the icon's
 *    real frame count (a virtual `GetNumOfFrames()`-style call, which for
 *    the 26-frame brain sequence would read 26) using an abs-value/subtract
 *    pattern - i.e. a real ~15fps PING-PONG bounce (0->25->0), not a
 *    simple forward loop with wraparound.
 *    NOT applied wholesale this pass, and left as the previous "plausible,
 *    not fully confirmed" value: the exact on-screen SIZE and WHICH of
 *    Big/Small the very first boot (before any script/menu exists) reaches
 *    couldn't be pinned down with full confidence in the time available -
 *    `mbUseSmallIcon` defaulting false is suggestive but not a
 *    slam-dunk given this same class is also reused for later in-game
 *    level-load screens (which show a screenshot + small icon + percent
 *    text per script/modules/MenuHandler.hps's own `GuiLoadingScreen`-style
 *    block, a visibly different composition from the boot splash), and a
 *    previous session's choice of a modest corner icon was itself already
 *    checked directly against the user's own reference screenshot. This
 *    pass DID adopt the ~15fps/bounce timing finding (see mfBrainFrameRate
 *    and DrawBrainIcon()'s own comment in SomaSplash.cpp) since that part
 *    of the evidence is unambiguous and low-risk to apply regardless of
 *    which Draw*Icon() variant it came from.
 *
 * 8. Menu ambient ("MenuBGNoise") never stopped - a real, user-reported
 *    bug. EnterPhase() below starts this loop via PlayGui() fire-and-
 *    forget, storing nothing - so it played forever, audible under any
 *    map loaded from the main menu (e.g. New Game). Real script/modules/
 *    MenuHandler.hps calls `Sound_Stop("MenuBGNoise", ...)` in every real
 *    path that leaves the menu (grep for "MenuBGNoise" in that file - a
 *    dozen+ call sites). This class now stores the cSoundEntry* + id
 *    PlayGui() returns for this one sound (mpMenuAmbientSound/
 *    mlMenuAmbientSoundId) and exposes StopMenuAmbient() to stop it -
 *    wired up from soma/src/game/SomaMainMenu.cpp's SetVisible(false),
 *    the same call site that already stops the menu music. See
 *    StopMenuAmbient()'s own comment in SomaSplash.cpp for the pointer/id
 *    validity pattern this reuses from amnesia/src/game/LuxEnemy_ManPig.cpp.
 */

#ifndef SOMA_SPLASH_H
#define SOMA_SPLASH_H

#include "hpl.h"

using namespace hpl;

class cSomaBase;

//----------------------------------------------

enum eSomaSplashPhase
{
	// Order matches the real game - see the file-top comment above for the
	// disassembly/script evidence this was corrected from (previously
	// FGLogo first, BootInit second - backwards).
	eSomaSplashPhase_BootInit,
	eSomaSplashPhase_FGLogo,
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

	// Stops the "MenuBGNoise" ambient (special_fx/frontend/main_menu_bg)
	// this class starts in EnterPhase() - see that call site's own comment
	// and this method's definition in SomaSplash.cpp for the real bug this
	// fixes.
	void StopMenuAmbient();

private:
	void EnterPhase(eSomaSplashPhase aPhase);
	void AdvanceToNextPhase();
	void Finish();

	bool AnySkipInputThisFrame();

	void DrawFGLogoPhase();
	void DrawBootInitPhase();
	void DrawBrainIcon(float afAlpha, float afPremenuScale);

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

	// config/game.cfg's General block: LoadingIcon = "brain_01.dds" - see
	// DrawBrainIcon()'s own comment in SomaSplash.cpp for the real 26-frame
	// sequence this loads (graphics/general/loadscreen/brainAnim/brain_01.dds
	// .. brain_26.dds).
	static const int mlBrainFrameCount = 26;
	cGuiGfxElement *mvBrainFrames[mlBrainFrameCount];

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

	// Task 3a: set the instant cSomaBase::PreloadMainMenuWorld() (the one
	// real, heavy piece of boot work this phase now triggers - see
	// EnterPhase()'s own comment) returns, whether it succeeded or not - a
	// real completion signal, not a timer. Update() below refuses to let
	// eSomaSplashPhase_BootInit advance (by input skip OR by its own
	// duration expiring) until this is true, so the splash can never be
	// skipped past while the real main menu world is still loading. In
	// this engine's current fully single-threaded architecture that call
	// always finishes synchronously before EnterPhase() itself returns, so
	// this flag is already true by the time Update() is ever called for
	// this phase - this check exists for correctness/documentation (a
	// previously *implicit*, accidental guarantee - see the file-top
	// comment - is now an explicit, verifiable one) and so it keeps working
	// correctly if that load is ever made to genuinely span multiple
	// frames in the future.
	bool mbRealBootWorkDone;

	static const float mfFGFadeInTime;
	static const float mfFGHoldTimerTotal;
	static const float mfFGFadeOutTime;

	static const float mfBootFadeTime;
	static const float mfBootHoldTime;

	static const float mfBrainFrameRate;

	// Task 2 fix: the only stored handle anywhere in this codebase for the
	// menu ambient loop this class fires-and-forgets in EnterPhase() (see
	// StopMenuAmbient()'s own comment below for why a handle is needed at
	// all). Same cSoundEntry*/id pair + iSoundManager::IsValid() pattern
	// amnesia/src/game/LuxEnemy_ManPig.cpp's mpMindFuckSound/
	// mlMindFuckSoundId already establishes for a looping PlayGui() sound
	// whose cSoundEntry may be recycled/destroyed by the sound handler
	// before this class gets around to stopping it.
	cSoundEntry *mpMenuAmbientSound;
	int mlMenuAmbientSoundId;
};

//----------------------------------------------

#endif // SOMA_SPLASH_H
