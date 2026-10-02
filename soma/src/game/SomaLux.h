#ifndef SOMA_LUX_H
#define SOMA_LUX_H

#include "hpl.h"

#include <angelscript.h>
#include <map>
#include <set>
#include <tuple>

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
	float mfLength = 0;
};

class cSomaLuxMap
{
public:
	cSomaLuxMap(cWorld *apWorld, const tString &asFileName);
	~cSomaLuxMap();

	bool CreateScript(cSomaScriptRuntime *apRuntime, const tString &asScriptFile);
	// Setup(), then OnStart() the first time and OnEnter() (cLuxMapHandler::SetCurrentMap)
	void OnEnter(bool abFirstTime);
	void OnLeave();
	void Update(float afTimeStep);
	void OnAction(int alAction, bool abPressed);

	void AddTimer(const tString &asName, float afTime, const tString &asFunction);
	// cLuxMap::RestartCurrentTimer, only valid inside a timer callback
	void RestartCurrentTimer(float afTime);
	std::vector<cSomaLuxTimer> &GetTimers() { return mvTimers; }
	double GetTime() { return mfTime; }
	void RemoveTimer(const tString &asName);
	cSomaLuxTimer *GetTimer(const tString &asName);

	cWorld *GetWorld() { return mpWorld; }
	asIScriptObject *GetScript() { return mpScript; }

	cSomaLuxEntity *GetEntity(const tString &asName);
	cSomaLuxEntity *GetEntity(const struct cSomaID &aID);
	const std::vector<cSomaLuxEntity *> &GetEntities() { return mvEntities; }
	cSomaLuxEntity *CreateEntity(const tString &asName, const tString &asFile, const cMatrixf &a_mtx, const cVector3f &avScale);
	void DestroyEntity(cSomaLuxEntity *apEnt);
	cSomaLuxEntity *mpLatestEntity = NULL;
	const tString &GetName() const { return msName; }
	const tString &GetFileName() const { return msFileName; }
	tString msDisplayNameEntry;
	float mfMaxInteractDistance = 3; // cLuxMap::cLuxMap

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

	void UpdateCollideCallbacks();
	void UpdateLookAtCallbacks(float afTimeStep);
	bool SetupEntityScript(cSomaLuxEntity *apEnt);
	void AddEntity(cSomaLuxEntity *apEnt);
	int mlNextId = 1;
	std::vector<cSomaLuxEntity *> mvDestroyed;
public:
	std::vector<cSomaLuxEntity *> mvPendingBreaks;
private:
	cSomaLuxTimer *mpFiringTimer = NULL;
	double mfTime = 0;
	std::set<std::tuple<cSomaLuxEntity *, cSomaLuxEntity *, tString>> msetColliding;
};

// Advances the current map's script every frame (cUpdater has no remove, so this persists).
class cSomaLuxUpdater : public iUpdateable
{
public:
	cSomaLuxUpdater() : iUpdateable("SomaLuxUpdater") {}
	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);

private:
};

void SomaRequestMapChange(const tString &asMap, const tString &asStart);
float SomaStartYaw(const cMatrixf &a_mtxArea);
bool SomaStartPosCrouching(const tString &asName);
void SomaUpdateLightConnections();
unsigned int SomaCollideFlag(const tString &asGroups);
void SomaRequestNewGame(const tString &asMap, const tString &asStart);

#endif // SOMA_LUX_H
