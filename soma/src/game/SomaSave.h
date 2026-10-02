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

	static void OnMapEnter(const tString &asMapFile, const tString &asStartPos);
	static bool ApplyPendingState();
	static bool Save(const tWString &asFile);
	static bool AutoSave(bool abCheckpoint);
	// Immediate from native code (menu); scripts change map on the next update
	static bool Load(const tWString &asFile, bool abImmediate = false);

	static void RegisterNatives(asIScriptEngine *apEngine);
};

tString SomaSerializeGlobalVars();
void SomaDeserializeGlobalVars(const tString &asData);

#endif // SOMA_SAVE_H
