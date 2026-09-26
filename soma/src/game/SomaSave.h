/*
 * Save games: map, start position, visited maps, script global variables and the world state
 * (map, entity, module and player script objects by member name, entity flags and callbacks,
 * dynamic bodies, timers, player pose). Loading enters the map without OnStart and restores the
 * state before OnEnter. Files live in $XDG_DATA_HOME/open-hpl/soma/saves.
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
	// Restores a loaded save into the current map; false when there is none
	static bool ApplyPendingState();
	static bool Save(const tWString &asFile);
	static bool AutoSave(bool abCheckpoint);
	// Immediate from native code (menu); scripts change map on the next update
	static bool Load(const tWString &asFile, bool abImmediate = false);

	static void RegisterNatives(asIScriptEngine *apEngine);
};

// Script global variables (cScript_SetGlobalVar*), see SomaScriptGlobals.cpp
tString SomaSerializeGlobalVars();
void SomaDeserializeGlobalVars(const tString &asData);

#endif // SOMA_SAVE_H
