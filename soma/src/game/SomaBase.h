/*
 * Phase 0 scaffolding for a SOMA game module running on the HPL2 engine.
 *
 * cSomaBase mirrors the *shape* of amnesia/src/game/LuxBase.h's cLuxBase
 * (Init()/InitEngine()/Run()/Exit()) but is trimmed to the bare minimum
 * needed to boot the engine against SOMA's real config files, open a
 * window, and run an empty main loop. No map loading, scripting, player
 * controller, or Amnesia-specific subsystems (Journal/Achievements/
 * SaveGame/MainMenu GUI etc.) - those are out of scope for this milestone.
 */

#ifndef SOMA_BASE_H
#define SOMA_BASE_H

#include "hpl.h"

#include "DebugFreeCamera.h"
#include "SomaPlayer.h"
#include "SomaSplash.h"
#include "SomaGammaScreen.h"
#include "SomaMainMenu.h"
#include "SomaConfig.h"
#include "SomaIntroSequence.h"
#include "SomaApartmentIntroCall.h"

using namespace hpl;

//----------------------------------------------

class cSomaBase
{
public:
	cSomaBase();
	~cSomaBase();

	bool Init(const tString &asCommandline);
	void Exit();

	void Run();

	// Called by cSomaSplash once its sequence finishes (or is skipped).
	// Public because cSomaSplash calls it back via the global gpSomaBase
	// pointer, same idiom as gpBase-> calls throughout amnesia/src/game.
	// Shows cSomaGammaScreen first on a fresh install (see
	// cSomaGammaScreen::ShouldShowAndMarkSeen()), otherwise goes straight
	// to ProceedPastBoot() (below).
	void OnSplashFinished();

	// Called by cSomaGammaScreen once its sequence finishes (or is
	// skipped) - only reached on a fresh install, see OnSplashFinished().
	void OnGammaScreenFinished();

	// SomaSplash.cpp task (real boot-progress work): loads the real main
	// menu world (main_init.cfg's <MainMenu> entry) exactly once and caches
	// it so InitMainMenuScene() below can consume it without loading twice.
	// Called by cSomaSplash::EnterPhase() while its own boot-init screen is
	// still on screen, so this real, heavy load now genuinely happens
	// *during* the splash instead of only after it finishes - see
	// SomaSplash.h's "Two real phases" comment for why. Small, additive,
	// flagged touch to this normally splash-owned file: only this method,
	// mpPreloadedMainMenuWorld, and the two call sites that consume/free it
	// in InitMainMenuScene()/ProceedPastBoot() below were added/changed.
	// Returns the loaded cWorld* (NULL on failure), and is safe to call
	// more than once (a no-op after the first attempt) or never at all
	// (InitMainMenuScene() falls back to loading directly, unchanged from
	// before this pass, for any caller that never runs cSomaSplash).
	cWorld* PreloadMainMenuWorld();

	// Called by cSomaIntroSequence (see SomaIntroSequence.h) once its
	// hardcoded 00_00_intro.hpm slideshow finishes - loads the real next map
	// (00_01_apartment.hpm/PlayerStartArea_1, same as the real script's
	// Event_EndSlideShow/TimerNextMap) and re-enables the player controller
	// that StartNewGame() suppressed for the slideshow's duration.
	void OnIntroSequenceFinished();

private:
	bool ParseCommandLine(const tString &asCommandline);

	void SetupLogFile();
	bool InitMainConfig();

	bool InitEngine();
	void ExitEngine();

	// Shared tail end of OnSplashFinished()/OnGammaScreenFinished() - the
	// OPENHPL_SOMA_MAP test-map escape hatch, falling back to
	// InitMainMenuScene()/InitTestMap().
	void ProceedPastBoot();

	////////////////////////////////////////
	// Real boot sequence, one step further than Phase 0: after the splash
	// (see cSomaSplash), load SOMA's own declared main menu scene
	// (main_init.cfg's <MainMenu> entry) with the same debug free-fly
	// camera as InitTestMap() below, plus a real interactive GuiSet menu
	// (see SomaMainMenu.h) - a plain native replacement for the real
	// ImGui-based menu this port doesn't integrate with.
	bool InitMainMenuScene();

	////////////////////////////////////////
	// Original Phase 1 hardcoded test map - no longer called from Init()
	// automatically (OnSplashFinished() calls InitMainMenuScene() instead,
	// falling back to this only if that fails to load), but kept available
	// as a known-good manual fallback: call this instead of
	// InitMainMenuScene() from OnSplashFinished() to go back to it.
	bool InitTestMap();
	void ExitTestMap();

	// Creates cSomaBase::eSomaPlayerAction's 5 real cAction objects on
	// mpEngine->GetInput() and binds each to its persisted key (see
	// SomaConfig.h's msKeyForward/etc, falling back to the hardcoded
	// default for any that fail to parse - e.g. a hand-edited config with a
	// typo). Called once from InitEngine(), before any map/menu loads, so
	// the real player controller (SomaPlayer.cpp) and the Options screen's
	// KEYBINDINGS row (SomaMainMenu.cpp) both see fully-bound actions from
	// the very first frame.
	void CreateInputActions();

public:
	// Generic map loader for the "start_map" headless command (see
	// SomaBase.cpp) - tears down whatever world/camera/viewport is
	// currently active (properly, via cScene::Destroy*(), unlike
	// ExitTestMap()'s "owned by cScene, left for engine teardown" shortcut,
	// since this can be called many times in one process) and loads
	// asMapFile fresh with a debug free-fly camera. If asStartPosName is
	// non-empty and the loaded map has a PlayerStart Area of that name (see
	// SomaLoaders.h - requires RegisterSomaLoaders() to have run), the
	// camera spawns there instead of avStartPos, which is used as a
	// fallback/default whenever the name is empty or not found. Returns
	// false (asErrorOut set) if the map fails to load.
	bool LoadMap(const tString &asMapFile, const cVector3f &avStartPos, tString &asErrorOut,
				 const tString &asStartPosName = "");

	// Reads the real <StartMap File=".../" Pos="..."/> entry out of
	// main_init.cfg (same file/pattern InitMainMenuScene() already uses for
	// <MainMenu>) and loads it via LoadMap() - the actual first map a new
	// game should open on, e.g. SOMA's own real "00_00_intro.hpm"/
	// "PlayerStartArea_1". Used by the main menu's "New Game" action instead
	// of a hardcoded map file.
	bool StartNewGame(tString &asErrorOut);

	cEngine *mpEngine;

	tString msGameName;
	tWString msErrorMessage;

	// NULL until InitMainMenuScene()/InitTestMap() runs; used by the
	// headless-control camera_state/set_camera commands (see SomaBase.cpp).
	cCamera* GetDebugCamera(){ return mpDebugCamera; }

	// Lets cSomaMainMenu reach the splash instance to stop its still-
	// looping menu ambient when leaving the menu (see SomaMainMenu.cpp's
	// SetVisible(false) and cSomaSplash::StopMenuAmbient()'s own comment
	// for the real bug this fixes) - same "mpBase already holds the other
	// side" wiring cSomaMainMenu uses for everything else in this class.
	cSomaSplash* GetSplash(){ return mpSplash; }

	// Small, deliberately minimal hook (tasks 2/3 - real interact system):
	// lets cSomaApartmentIntroCall (constructed by LoadMap() with only an
	// (mpEngine, this) pair, same as cSomaIntroSequence) reach the one
	// cSomaPlayer instance to register/query the real phone interact point -
	// see SomaPlayer.h's RegisterInteractPoint()/WasInteractedWith(). NULL
	// whenever mbUseRealPlayer is false (OPENHPL_SOMA_FREECAM) or before the
	// first real game map has loaded - same nullability as GetDebugCamera().
	cSomaPlayer* GetPlayer(){ return mpPlayer; }

	// Persisted settings backing the real Options screen (see
	// SomaMainMenu.cpp) - loaded once in InitEngine(), mutated live by the
	// Options screen itself via this same instance (it calls Save() after
	// each change, see SomaConfig.h).
	cSomaConfig* GetConfig(){ return &mConfig; }

	// Real HPL2 cAction/cInput system (HPL2/core/include/input/Action.h) -
	// cSomaPlayer's 5 movement/jump actions, same real API
	// amnesia/src/game/LuxInputHandler.cpp's own action table uses. Created
	// once on mpEngine->GetInput() by InitEngine() (see CreateInputActions()
	// below) - deliberately NOT owned by cSomaPlayer itself (which doesn't
	// exist yet the first time the Options screen's KEYBINDINGS sub-screen
	// is reachable, straight from the main menu before any game map has
	// loaded) so rebinding works from the main menu exactly like every other
	// Options row, not just mid-game.
	enum eSomaPlayerAction
	{
		eSomaPlayerAction_Forward,
		eSomaPlayerAction_Backward,
		eSomaPlayerAction_Left,
		eSomaPlayerAction_Right,
		eSomaPlayerAction_Jump,
		eSomaPlayerAction_LastEnum
	};

	// Internal cAction name (never shown to the user - see
	// GetPlayerActionKeyName() for the real display string).
	static const char* GetPlayerActionName(eSomaPlayerAction aAction);

	// Real display label for the Options screen's KEYBINDINGS row (e.g.
	// "FORWARD") - not a real base_english.lang lookup (this scaffold has
	// no in-game action names to translate, only these 5 fixed labels).
	static const wchar_t* GetPlayerActionLabel(eSomaPlayerAction aAction);

	// Rebinds the given action to a single key (real cAction::
	// ClearSubActions()+AddKey()), persists it to cSomaConfig, and saves -
	// used by SomaMainMenu.cpp's "press a key" KEYBINDINGS flow.
	void RebindPlayerAction(eSomaPlayerAction aAction, eKey aKey);

	// The action's current bound key, as a display string (real
	// iKeyboard::KeyToString(), via the action's own sole sub-action) - "-"
	// if unbound (should not normally happen - every action always has
	// exactly one key bound, see CreateInputActions()).
	tString GetPlayerActionKeyName(eSomaPlayerAction aAction);

	// ESC pause menu (task 3) - single bridge point between mpPlayer and
	// mpMainMenu (both private below), so cSomaPlayer/cSomaMainMenu never
	// need a direct pointer to each other: cSomaPlayer::Update() calls these
	// on a fresh Escape press (see its own comment for why it checks the
	// raw keyboard queue instead of a cAction), and cSomaMainMenu's own
	// Resume button routes back through SetGameplayPaused(false) rather than
	// unpausing itself directly, so the player's SetActive() call and the
	// menu's own show/hide always happen together. mpMainMenu is NULL only
	// before InitMainMenuScene() has run (i.e. before any player could exist
	// either), so both are guarded.
	void SetGameplayPaused(bool abPaused);
	bool IsGameplayPaused();

private:
	/////////////////////////
	// Config file paths, loaded from main_init.cfg
	tWString msInitConfigFile;

	tString msResourceConfigPath;
	tString msMaterialConfigPath;

	/////////////////////////
	// Persisted settings (Volume/Gamma/VSync/Fullscreen) - see SomaConfig.h.
	cSomaConfig mConfig;

	/////////////////////////
	// Splash sequence, shown before the map below loads
	cSomaSplash *mpSplash;

	// First-boot-only gamma calibration screen, shown between the splash
	// and the main menu - see SomaGammaScreen.h. NULL once past boot (or
	// on any boot after the first, when it's never constructed at all).
	cSomaGammaScreen *mpGammaScreen;

	// NULL except while the main menu scene (InitMainMenuScene()) is the
	// active scene - not created for InitTestMap()'s fallback path or for
	// OPENHPL_SOMA_MAP boot-time overrides, both of which skip the menu
	// entirely by design (see OnSplashFinished()).
	cSomaMainMenu *mpMainMenu;

	/////////////////////////
	// Phase 1 test map + debug camera state (also used by
	// InitMainMenuScene() for the main menu scene - same shape, one
	// camera/viewport/world set at a time)
	cWorld *mpTestWorld;
	cCamera *mpDebugCamera;
	cViewport *mpDebugViewport;
	cSomaDebugFreeCamera *mpDebugCameraController;

	// See PreloadMainMenuWorld()'s own comment above. Always NULL again by
	// the time either InitMainMenuScene() or ProceedPastBoot()'s
	// OPENHPL_SOMA_MAP branch returns - one of them always consumes
	// (uses) or frees (cScene::DestroyWorld()) whatever this holds.
	cWorld *mpPreloadedMainMenuWorld;
	bool mbMainMenuWorldPreloadAttempted;

	// Real physics-based player controller (see SomaPlayer.h) - created
	// instead of mpDebugCameraController by LoadMap() (real game maps) when
	// mbUseRealPlayer is true, sharing the same mpDebugCamera/mpDebugViewport.
	// InitMainMenuScene()/InitTestMap() always keep using the free-fly
	// camera instead (no player body makes sense in the menu scene).
	cSomaPlayer *mpPlayer;
	bool mbUseRealPlayer;

	// Real 00_00_intro.hpm opening slideshow (see SomaIntroSequence.h) -
	// created once by StartNewGame() the first time that specific map loads,
	// NULL otherwise. Kept alive for the rest of the process, same
	// no-remove-from-cUpdater constraint as every other iUpdateable here.
	cSomaIntroSequence *mpIntroSequence;

	// Real 00_01_apartment.hpm Munshi phone-call hand-port (see
	// SomaApartmentIntroCall.h) - created once by LoadMap() the first time
	// that specific map loads, NULL otherwise. Same never-destroyed
	// no-remove-from-cUpdater constraint as mpIntroSequence above.
	cSomaApartmentIntroCall *mpApartmentIntroCall;
};

//----------------------------------------------

extern cSomaBase *gpSomaBase;

//----------------------------------------------

#endif // SOMA_BASE_H
