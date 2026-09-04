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
#include "SomaSplash.h"
#include "SomaMainMenu.h"

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
	void OnSplashFinished();

private:
	bool ParseCommandLine(const tString &asCommandline);

	bool InitMainConfig();

	bool InitEngine();
	void ExitEngine();

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

	cEngine *mpEngine;

	tString msGameName;
	tWString msErrorMessage;

	// NULL until InitMainMenuScene()/InitTestMap() runs; used by the
	// headless-control camera_state/set_camera commands (see SomaBase.cpp).
	cCamera* GetDebugCamera(){ return mpDebugCamera; }

private:
	/////////////////////////
	// Config file paths, loaded from main_init.cfg
	tWString msInitConfigFile;

	tString msResourceConfigPath;
	tString msMaterialConfigPath;

	/////////////////////////
	// Splash sequence, shown before the map below loads
	cSomaSplash *mpSplash;

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
};

//----------------------------------------------

extern cSomaBase *gpSomaBase;

//----------------------------------------------

#endif // SOMA_BASE_H
