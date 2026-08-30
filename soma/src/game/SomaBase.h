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
	cSomaDebugFreeCamera *mpDebugCameraController;
};

//----------------------------------------------

extern cSomaBase *gpSomaBase;

//----------------------------------------------

#endif // SOMA_BASE_H
