/*
 * The engine side of SOMA's script layer ("Lux" in HPL3): objects scripts see as cLuxMap etc.
 * Behaviour follows the official binary (see scripts/soma-re-script-api.py for the bound natives).
 */

#ifndef SOMA_LUX_H
#define SOMA_LUX_H

#include "hpl.h"

#include <angelscript.h>
#include <map>

using namespace hpl;

class cSomaScriptRuntime;
class cSomaLuxEntity;

struct cSomaLuxTimer
{
	tString msName;
	tString msFunction;
	float mfTime;
	bool mbPaused;
	float mfUserFloat;
	int mlUserInt;
	tString msUserString;
};

// Script type cLuxMap
class cSomaLuxMap
{
public:
	cSomaLuxMap(cWorld *apWorld, const tString &asFileName);
	~cSomaLuxMap();

	// Compiles the map's .hps, creates its cScrMap and runs PreloadData() (cLuxMap::LoadFromFile)
	bool CreateScript(cSomaScriptRuntime *apRuntime, const tString &asScriptFile);
	// Setup(), then OnStart() the first time and OnEnter() (cLuxMapHandler::SetCurrentMap)
	void OnEnter(bool abFirstTime);
	void OnLeave();
	void Update(float afTimeStep);

	void AddTimer(const tString &asName, float afTime, const tString &asFunction);
	void RemoveTimer(const tString &asName);
	cSomaLuxTimer *GetTimer(const tString &asName);

	cWorld *GetWorld() { return mpWorld; }

	cSomaLuxEntity *GetEntity(const tString &asName);
	cSomaLuxEntity *GetEntity(const struct cSomaID &aID);
	const std::vector<cSomaLuxEntity *> &GetEntities() { return mvEntities; }
	const tString &GetName() const { return msName; }
	const tString &GetFileName() const { return msFileName; }
	tString msDisplayNameEntry;

	static cSomaLuxMap *GetCurrent() { return mpCurrent; }
	static void SetCurrent(cSomaLuxMap *apMap) { mpCurrent = apMap; }

private:
	static cSomaLuxMap *mpCurrent;

	cWorld *mpWorld;
	tString msName;
	tString msFileName;
	cSomaScriptRuntime *mpRuntime;
	asIScriptObject *mpScript;
	std::vector<cSomaLuxTimer> mvTimers;
	std::vector<cSomaLuxEntity *> mvEntities;
	std::map<tString, cSomaLuxEntity *> mmapEntities;
};

// Advances the current map's script every frame (cUpdater has no remove, so this persists).
class cSomaLuxUpdater : public iUpdateable
{
public:
	cSomaLuxUpdater() : iUpdateable("SomaLuxUpdater") {}
	void Update(float afTimeStep);
};

#endif // SOMA_LUX_H
