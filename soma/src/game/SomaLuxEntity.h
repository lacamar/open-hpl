/*
 * SOMA map entities (iLuxEntity: cLuxProp, cLuxArea, cLuxAgent, cLuxCritter...): the HPL2 objects
 * the loaders created, the per-instance variables and the entity's script class from
 * config/EntityTypes.cfg.
 */

#ifndef SOMA_LUX_ENTITY_H
#define SOMA_LUX_ENTITY_H

#include "SomaLuxScriptable.h"

#include <map>

class cSomaLuxMap;

// Layout of hpl::cID (script tID); the map's UID="a b c" attribute
struct cSomaID
{
	uint8_t mA = 0;
	int32_t mB = 0;
	int32_t mC = 0;
	bool operator==(const cSomaID &o) const { return mA == o.mA && mB == o.mB && mC == o.mC; }
};

enum eSomaLuxEntityType
{
	eSomaLuxEntityType_Prop,
	eSomaLuxEntityType_Area,
	eSomaLuxEntityType_LiquidArea,
	eSomaLuxEntityType_ReverbArea,
	eSomaLuxEntityType_Critter,
	eSomaLuxEntityType_Agent,
	eSomaLuxEntityType_Player,
};

class cSomaLuxEntity : public cSomaLuxScriptable
{
public:
	tString msName;
	tString msClassName; // EntityType / AreaType
	tString msFileName;
	int meType = eSomaLuxEntityType_Prop;
	cSomaID mID;
	bool mbActive = true;
	bool mbInteractionDisabled = false;
	float mfMaxInteractDistance = -1;
	bool mbInteractedWith = false;
	cMatrixf m_mtxOnLoad = cMatrixf::Identity;
	cVector3f mvScale = 1;
	cVector3f mvSize = 1; // areas

	cMeshEntity *mpMesh = NULL;
	std::vector<iPhysicsBody *> mvBodies;
	std::vector<iLight *> mvLights;
	std::vector<cParticleSystem *> mvParticleSystems;
	std::vector<cBillboard *> mvBillboards;
	std::vector<cSoundEntity *> mvSoundEntities;

	cResourceVarsObject mVars;		   // .ent UserDefinedVariables
	cResourceVarsObject mInstanceVars; // map UserVariables
	std::map<tString, tString> mmapScriptVars;

	tString msInteractCallback;
	bool mbInteractCallbackAutoRemove = false;
	tString msLookAtCallback;
	bool mbLookAtCallbackAutoRemove = true;
	bool mbEffectsActive = true;

	struct cCollideCallback
	{
		tString msChild;
		tString msFunc;
	};
	std::vector<cCollideCallback> mvCollideCallbacks;

	cSomaLuxMap *mpMap = NULL;

	void SetActive(bool abX);
	void SetEffectsActive(bool abX);
	cVector3f GetPosition();
	cMatrixf GetMatrix();
	void SetMatrix(const cMatrixf &a_mtx);
	iPhysicsBody *GetMainBody() { return mvBodies.empty() ? NULL : mvBodies[0]; }
	void RemoveCollideCallbacks(const tString &asChild);

	// iLuxEntity animation controller on the mesh's animation states
	int PlayAnimation(const tString &asName, float afFadeTime, bool abLoop, const tString &asCallback);
	bool GetAnimationIsPlaying();
	void StopAnimations(float afFadeTime);
	void UpdateAnimation(float afTimeStep);
	int mlCurrentAnim = -1;
	tString msAnimCallback;
	std::vector<std::pair<tString, bool>> mvAnimQueue;
	float GetMaxInteractDistance();
	bool CanInteract(int alType, iPhysicsBody *apBody);
	bool OnInteract(int alType, iPhysicsBody *apBody, const cVector3f &avFocusPos, const tString &asData);
	// HPL3 areas are raycastable bodies that collide with nothing
	void CreateAreaBody(iPhysicsWorld *apWorld);
	const char *GetBaseTypeName() const;

	// Created by the loaders while a world loads; the next cSomaLuxMap takes them
	static std::vector<cSomaLuxEntity *> &Pending();

	static void RegisterNatives(asIScriptEngine *apEngine);
};

// tIDs for engine objects that have none in HPL2 (bodies, lights...)
cSomaID SomaObjectID(void *apObj);
void *SomaObjectFromID(const cSomaID &aID);

// Oriented boxes of bodies, areas and the player's character body
bool SomaEntitiesCollide(cSomaLuxEntity *apA, cSomaLuxEntity *apB);

// "*" matches any run of characters, as HPL3's wildcard entity names
bool SomaWildcardMatch(const tString &asPattern, const tString &asName);

#endif // SOMA_LUX_ENTITY_H
