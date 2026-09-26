/*
 * Checkpoint saves: the map and start position the player entered it at, the script global
 * variables from before the map started and the visited maps. Loading re-enters that map, so a
 * save resumes at the map's entry point. Files live in $XDG_DATA_HOME/open-hpl/soma/saves.
 */

#ifndef SOMA_SAVE_H
#define SOMA_SAVE_H

#include "hpl.h"

#include <angelscript.h>

using namespace hpl;

class cSomaSaveHandler
{
public:
	static tWString GetSaveDir();
	static tWString GetLatestSave();

	// Called by cSomaBase::LoadMap before the map script starts
	static void OnMapEnter(const tString &asMapFile, const tString &asStartPos);
	static bool Save(const tWString &asFile, bool abCurrentVars);
	static bool AutoSave(bool abCheckpoint, bool abCurrentVars);
	// Immediate from native code (menu); scripts change map on the next update
	static bool Load(const tWString &asFile, bool abImmediate = false);

	static void RegisterNatives(asIScriptEngine *apEngine);
};

// Script global variables (cScript_SetGlobalVar*), see SomaScriptGlobals.cpp
tString SomaSerializeGlobalVars();
void SomaDeserializeGlobalVars(const tString &asData);

#endif // SOMA_SAVE_H
