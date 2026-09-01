/*
 * Phase 0 scaffolding for an Amnesia: Rebirth game module running on the
 * HPL2 engine. Same shape as soma/src/game/SomaBase.h (see that file for
 * the rationale) - trimmed to the bare minimum needed to boot the engine
 * against Rebirth's real config files, load its own declared StartMap, and
 * fly around it with a debug camera. No splash sequence, main menu, player
 * controller, or scripting yet - later phases, same as SOMA's.
 *
 * Unlike SOMA's Phase 0 (which hardcoded a hand-picked small test map and
 * its camera start position/facing, read out of the map file by hand),
 * InitTestMap() here resolves both generically: the map filename comes
 * straight from main_init.cfg's own <StartMap File=... Pos=.../> entry, and
 * the camera position comes from the matching cStartPosEntity the map file
 * itself declares (cWorld::GetStartPosEntity()) - no per-game guesswork.
 */

#ifndef REBIRTH_BASE_H
#define REBIRTH_BASE_H

#include "hpl.h"

#include "DebugFreeCamera.h"

using namespace hpl;

//----------------------------------------------

class cRebirthBase
{
public:
	cRebirthBase();
	~cRebirthBase();

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

	// <StartMap File="..." Pos="..."/> - the map to boot straight into, and
	// the named cStartPosEntity in it to place the debug camera at.
	tString msStartMapFile;
	tString msStartMapPos;

	/////////////////////////
	// Test map + debug camera state
	cWorld *mpTestWorld;
	cCamera *mpDebugCamera;
	cViewport *mpDebugViewport;
	cRebirthDebugFreeCamera *mpDebugCameraController;
};

//----------------------------------------------

extern cRebirthBase *gpRebirthBase;

//----------------------------------------------

#endif // REBIRTH_BASE_H
