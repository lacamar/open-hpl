#ifndef SOMA_LUX_H
#define SOMA_LUX_H

#include "hpl.h"
#include "RustSomaCore.h"

#include <angelscript.h>
#include <list>
#include <map>
#include <set>
#include <tuple>

using namespace hpl;

class cSomaScriptRuntime;
class cSomaLuxEntity;

struct OhplSoRoster;
extern "C"
{
	OhplSoRoster *ohpl_so_roster_new();
	void ohpl_so_roster_free(OhplSoRoster *);
	void ohpl_so_roster_add(OhplSoRoster *, size_t ent, const OhplSeCore *, const OhplSeNames *);
	bool ohpl_so_roster_remove(OhplSoRoster *, size_t ent);
	cSomaLuxEntity *const *ohpl_so_roster_live(const OhplSoRoster *, size_t *len);
	cSomaLuxEntity *const *ohpl_so_roster_destroyed(const OhplSoRoster *, size_t *len);
	void ohpl_so_roster_flush(OhplSoRoster *);
	cSomaLuxEntity *ohpl_so_roster_by_name(const OhplSoRoster *, const unsigned char *, size_t);
	cSomaLuxEntity *ohpl_so_roster_by_id(const OhplSoRoster *, uint8_t, int32_t, int32_t);
	cSomaLuxEntity *const *ohpl_so_roster_children(OhplSoRoster *, const unsigned char *, size_t, size_t *n);
}

// View of a Rust-owned entity array; valid until the roster changes
struct cSomaEntSpan
{
	cSomaLuxEntity *const *mp;
	size_t mn;
	cSomaLuxEntity *const *begin() const { return mp; }
	cSomaLuxEntity *const *end() const { return mp + mn; }
	size_t size() const { return mn; }
	bool empty() const { return mn == 0; }
	cSomaLuxEntity *operator[](size_t i) const { return mp[i]; }
	std::vector<cSomaLuxEntity *> Vec() const { return std::vector<cSomaLuxEntity *>(mp, mp + mn); }
};

class cSomaLuxMap
{
public:
	cSomaLuxMap(cWorld *apWorld, const tString &asFileName);
	// World-only stand-in for the streaming map (cLux_GetPreloadMap)
	explicit cSomaLuxMap(cWorld *apWorld) : mpWorld(apWorld), mpRuntime(NULL), mpScript(NULL) {}
	~cSomaLuxMap();

	bool CreateScript(cSomaScriptRuntime *apRuntime, const tString &asScriptFile);
	// Before a save is applied: cLuxMapHandler::SetCurrentMap runs ahead of LoadSavedGame_PostMapLoadSetup
	void Setup();
	// OnStart() the first time and OnEnter() unless loading a save (cLuxMap::OnEnter)
	void OnEnter(bool abRunScript, bool abFirstTime);
	void OnLeave();
	void Update(float afTimeStep);
	void PostUpdate(float afTimeStep);
	void OnAction(int alAction, bool abPressed);

	void FireTimer(const tString &asName, const tString &asFunction);
	double GetTime();
	int GetTimeStamp();
	void AddDissolveEntity(cMeshEntity *apMeshEntity, float afTime);
	// cLuxPreloadCache: held until the map goes
	void PreloadEntity(const tString &asFile);
	void PreloadParticleSystem(const tString &asFile);
	void PreloadMaterial(const tString &asFile);

	cWorld *GetWorld() { return mpWorld; }
	void SetWorld(cWorld *apWorld) { mpWorld = apWorld; }
	asIScriptObject *GetScript() { return mpScript; }

	cSomaLuxEntity *GetEntity(const tString &asName);
	cSomaLuxEntity *GetEntity(const struct cSomaID &aID);
	cSomaEntSpan GetEntities() const
	{
		cSomaEntSpan s;
		s.mp = ohpl_so_roster_live(mpRoster, &s.mn);
		return s;
	}
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
	OhplSoRoster *mpRoster = ohpl_so_roster_new();

	void UpdateCollideCallbacks();
	void UpdateLookAtCallbacks(float afTimeStep);
	bool SetupEntityScript(cSomaLuxEntity *apEnt);
	void AddEntity(cSomaLuxEntity *apEnt);
public:
	int mlNextId = 1;
	std::vector<cSomaLuxEntity *> mvPendingBreaks;
	struct cPendingCollision { cSomaLuxEntity *mpEntity; iPhysicsBody *mpBody, *mpCollideBody; cPhysicsContactData mData; };
	std::vector<cPendingCollision> mvPendingCollisions;
	std::set<std::tuple<cSomaLuxEntity *, cSomaLuxEntity *, tString>> msetColliding;
private:
	struct cDissolve { cMeshEntity *mpEntity; float mfAlpha, mfSpeed; };
	std::vector<cDissolve> mvDissolves;
	std::set<tString> msetPreloaded;
	bool FirstPreload(const tString &asFile, const char *asKind);
	std::vector<cMesh *> mvPreloadedMeshes;
	std::vector<cMaterial *> mvPreloadedMaterials;
};

// Advances the current map's script every frame (cUpdater has no remove, so this persists).
class cSomaLuxUpdater : public iUpdateable
{
public:
	cSomaLuxUpdater() : iUpdateable("SomaLuxUpdater") {}
	void Update(float afTimeStep);
	void PostUpdate(float afTimeStep);
	void OnDraw(float afFrameTime);
	void AppLostInputFocus();
	void OnQuit();

private:
	bool mbUpdated = false;
};

void SomaRequestMapChange(const tString &asMap, const tString &asStart);
tString SomaPreloadMap();
void SomaSetPreloadMap(const tString &asMap);
cWorld *SomaTakePreloadedWorld(const tString &asMap, tString *apLoadReport = NULL);
void SomaSetGamePaused(bool abX, bool abMusic = true, bool abSounds = true, bool abAmbience = true, tFlag alTypes = eSoundEntryType_All);
bool SomaRunGlobalFunc(const tString &asObject, const tString &asClass, const tString &asFunc);
float SomaStartYaw(const cMatrixf &a_mtxArea);
float SomaLightLevelAtPos(const cVector3f &p, iLight *pSkip, float fAdd);
void SomaUpdateLightConnections();
unsigned int SomaCollideFlag(const tString &asGroups);
int32_t OhplS9HostCall(uint32_t alOp, struct OhplS5Io *io);
void SomaRequestNewGame(const tString &asMap, const tString &asStart);

#endif // SOMA_LUX_H
