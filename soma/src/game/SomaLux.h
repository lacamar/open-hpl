#ifndef SOMA_LUX_H
#define SOMA_LUX_H

#include "hpl.h"

#include <angelscript.h>
#include <list>
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
	bool mbRemoved = false;
};

class cSomaLuxMap
{
public:
	cSomaLuxMap(cWorld *apWorld, const tString &asFileName);
	~cSomaLuxMap();

	bool CreateScript(cSomaScriptRuntime *apRuntime, const tString &asScriptFile);
	// Before a save is applied: cLuxMapHandler::SetCurrentMap runs ahead of LoadSavedGame_PostMapLoadSetup
	void Setup();
	// OnStart() the first time and OnEnter() unless loading a save (cLuxMap::OnEnter)
	void OnEnter(bool abRunScript, bool abFirstTime);
	void OnLeave();
	void Update(float afTimeStep);
	void OnAction(int alAction, bool abPressed);

	void AddTimer(const tString &asName, float afTime, const tString &asFunction);
	// cLuxMap::RestartCurrentTimer, only valid inside a timer callback
	void RestartCurrentTimer(float afTime);
	std::list<cSomaLuxTimer> &GetTimers() { return mvTimers; }
	double GetTime() { return mfTime; }
	void RemoveTimer(const tString &asName);
	cSomaLuxTimer *GetTimer(const tString &asName);
	void SetTimerPaused(const tString &asName, bool abX);

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
	bool mbActive = true;
	bool mbIsUnderwater = false;

	static cSomaLuxMap *GetCurrent() { return mpCurrent; }
	static void SetCurrent(cSomaLuxMap *apMap) { mpCurrent = apMap; }

private:
	static cSomaLuxMap *mpCurrent;

	cWorld *mpWorld;
	tString msName;
	tString msFileName;
	cSomaScriptRuntime *mpRuntime;
	asIScriptObject *mpScript;
	// newest first, like cLuxMap: a re-added timer shadows the one firing
	std::list<cSomaLuxTimer> mvTimers;
	std::vector<cSomaLuxEntity *> mvEntities;
	std::map<tString, cSomaLuxEntity *> mmapEntities;
	std::map<tString, std::vector<cSomaLuxEntity *>> mmapWildcardCache;

	void UpdateCollideCallbacks();
	void UpdateLookAtCallbacks(float afTimeStep);
	bool SetupEntityScript(cSomaLuxEntity *apEnt);
	void AddEntity(cSomaLuxEntity *apEnt);
	std::vector<cSomaLuxEntity *> mvDestroyed;
public:
	int mlNextId = 1;
	std::vector<cSomaLuxEntity *> mvPendingBreaks;
	std::set<std::tuple<cSomaLuxEntity *, cSomaLuxEntity *, tString>> msetColliding;
private:
	bool mbUpdatingTimers = false;
	cSomaLuxTimer *mpFiringTimer = NULL;
	double mfTime = 0;
};

// Advances the current map's script every frame (cUpdater has no remove, so this persists).
class cSomaLuxUpdater : public iUpdateable
{
public:
	cSomaLuxUpdater() : iUpdateable("SomaLuxUpdater") {}
	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);
	void AppLostInputFocus();
};

void SomaRequestMapChange(const tString &asMap, const tString &asStart);
tString &SomaPreloadMap();
void SomaSetGamePaused(bool abX);
bool SomaRunGlobalFunc(const tString &asObject, const tString &asClass, const tString &asFunc);
float SomaStartYaw(const cMatrixf &a_mtxArea);
bool SomaStartPosCrouching(const tString &asName);
void SomaUpdateLightConnections();
unsigned int SomaCollideFlag(const tString &asGroups);
void SomaRequestNewGame(const tString &asMap, const tString &asStart);

#endif // SOMA_LUX_H
