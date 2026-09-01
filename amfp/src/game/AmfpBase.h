/*
 * Phase 0 scaffolding for an Amnesia: A Machine for Pigs game module running
 * on the HPL2 engine.
 *
 * Unlike soma/, AMFP's own shipped data (resources.cfg, materials.cfg,
 * main_init.cfg, and the .glsl shaders under core/shaders/) is in the same
 * format Dark Descent's HPL2 engine already understands - confirmed by
 * inspection: AMFP ships deferred_base_vtx.glsl and the rest of the
 * standard HPL2 deferred-renderer shader set (unlike SOMA, which is HPL3 and
 * ships .hpsl shaders instead - see PORTING_NOTES.md). So this reuses the
 * same shared HPL2 engine library as amnesia/ and soma/ with no renderer
 * changes expected, at least at this stage.
 *
 * cAmfpBase mirrors the *shape* of amnesia/src/game/LuxBase.h's cLuxBase
 * (Init()/InitEngine()/Run()/Exit()) but is trimmed to the bare minimum
 * needed to boot the engine against AMFP's real config files, open a
 * window, and run an empty main loop. No map loading, scripting, player
 * controller, or Amnesia-specific subsystems (Journal/Achievements/
 * SaveGame/MainMenu GUI etc.) - those are out of scope for this milestone.
 * (AMFP's own game-logic source, comparable to amnesia/src/game's Lux*
 * classes, was never open-sourced - only its data ships with the game - so
 * there is no equivalent to reuse here.)
 */

#ifndef AMFP_BASE_H
#define AMFP_BASE_H

#include "hpl.h"

#include "DebugFreeCamera.h"

using namespace hpl;

//----------------------------------------------

class cAmfpBase
{
public:
	cAmfpBase();
	~cAmfpBase();

	bool Init(const tString &asCommandline);
	void Exit();

	void Run();

private:
	bool ParseCommandLine(const tString &asCommandline);

	bool InitMainConfig();

	bool InitEngine();
	void ExitEngine();

	////////////////////////////////////////
	// Phase 1: data loading - load a hardcoded test map and set up a
	// debug free-fly camera to look at it with. No player controller,
	// no scripts.
	bool InitTestMap();
	void ExitTestMap();

public:
	cEngine *mpEngine;

	tString msGameName;
	tWString msErrorMessage;

	// NULL until InitTestMap() runs; used by the headless-control
	// camera_state/set_camera commands (see AmfpBase.cpp).
	cCamera* GetDebugCamera(){ return mpDebugCamera; }

private:
	/////////////////////////
	// Config file paths, loaded from main_init.cfg
	tWString msInitConfigFile;

	tString msResourceConfigPath;
	tString msMaterialConfigPath;

	/////////////////////////
	// Phase 1 test map + debug camera state
	cWorld *mpTestWorld;
	cCamera *mpDebugCamera;
	cViewport *mpDebugViewport;
	cAmfpDebugFreeCamera *mpDebugCameraController;
};

//----------------------------------------------

extern cAmfpBase *gpAmfpBase;

//----------------------------------------------

#endif // AMFP_BASE_H
