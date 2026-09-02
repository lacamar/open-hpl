/*
 * Phase 0 scaffolding for an Amnesia: The Bunker game module running on the
 * HPL2 engine. Same shape as soma/src/game/SomaBase.h / rebirth/src/game/
 * RebirthBase.h (see the former for the rationale) - trimmed to the bare
 * minimum needed to boot the engine against the Bunker's real config files,
 * load its own declared StartMap, and fly around it with a debug camera. No
 * splash sequence, main menu, player controller, or scripting yet - later
 * phases, same as SOMA's.
 *
 * The Bunker's main_init.cfg <StartMap> entry differs from SOMA's/Rebirth's:
 * File is a comma-separated "Label:file.hpm" list (e.g.
 * "Main:trenches.hpm, PostIntro:officer_hub.hpm") rather than a single bare
 * filename - presumably selecting among a few entry points depending on
 * story-progress state that this scaffold has no concept of. Phase 0 just
 * takes the first entry ("Main"'s map). Pos ("Start_Begin") resolves via a
 * real PlayerStart-Area loader (see BunkerAreaLoader.h) - the Bunker's maps
 * have no cStartPosEntity at all, unlike what an earlier pass of this file
 * assumed (see BunkerAreaLoader.h for why, and PORTING_NOTES.md for the
 * "using world origin" fallback this replaces).
 */

#ifndef BUNKER_BASE_H
#define BUNKER_BASE_H

#include "hpl.h"

#include "BunkerAreaLoader.h"
#include "DebugFreeCamera.h"

using namespace hpl;

//----------------------------------------------

class cBunkerBase
{
public:
	cBunkerBase();
	~cBunkerBase();

	bool Init(const tString &asCommandline);
	void Exit();

	void Run();

private:
	bool ParseCommandLine(const tString &asCommandline);

	bool InitMainConfig();

	bool InitEngine();
	void ExitEngine();

	bool InitTestMap();
	void ExitTestMap();

public:
	cEngine *mpEngine;

	tString msGameName;
	tWString msErrorMessage;

	cCamera* GetDebugCamera(){ return mpDebugCamera; }

private:
	/////////////////////////
	// Config file paths, loaded from main_init.cfg
	tWString msInitConfigFile;

	tString msResourceConfigPath;
	tString msMaterialConfigPath;

	// First "Label:file.hpm" entry out of <StartMap>'s comma-separated File
	// list (see this file's header comment), and its Pos attribute verbatim.
	tString msStartMapFile;
	tString msStartMapPos;

	/////////////////////////
	// Test map + debug camera state
	cWorld *mpTestWorld;
	cCamera *mpDebugCamera;
	cViewport *mpDebugViewport;
	cBunkerDebugFreeCamera *mpDebugCameraController;
};

//----------------------------------------------

extern cBunkerBase *gpBunkerBase;

//----------------------------------------------

#endif // BUNKER_BASE_H
