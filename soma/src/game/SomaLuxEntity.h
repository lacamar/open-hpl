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

class cSomaGuiScreenRenderer : public iRendererCallback
{
public:
	void Register();
	void OnPostSolidDraw(cRendererCallbackFunctions *) override {}
	void OnPostTranslucentDraw(cRendererCallbackFunctions *apFunctions) override;

private:
	cViewport *mpViewport = NULL;
};

class cSomaLuxEntity : public cSomaLuxScriptable
{
public:
	~cSomaLuxEntity();

	tString msName;
	tString msClassName; // EntityType / AreaType
	tString msScriptClassName;
	bool mbCameraProxy = false;
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
	std::vector<iPhysicsJoint *> mvJoints;
	// cLuxCritter members at the official offsets, see SomaNewPropBlock
	char *mpCritterProps = NULL;
	std::vector<iLight *> mvLights;
	std::vector<cParticleSystem *> mvParticleSystems;
	std::vector<cBillboard *> mvBillboards;
	std::vector<cSoundEntity *> mvSoundEntities;
	struct cConnectedLight
	{
		iLight *mpLight;
		cColor mBaseColor;
		float mfAmount;
		bool mbMul;
	};
	std::vector<cConnectedLight> mvConnectedLights;
	bool mbConnectedLightsResolved = false;
	void ResolveConnectedLights();

	cResourceVarsObject mVars;		   // .ent UserDefinedVariables
	cResourceVarsObject mInstanceVars; // map UserVariables
	std::map<tString, tString> mmapScriptVars;

	tString msInteractCallback;
	bool mbInteractCallbackAutoRemove = false;
	tString msLookAtCallback;
	bool mbLookAtCallbackAutoRemove = true;
	bool mbLookAtCheckCenter = true;
	bool mbLookAtCheckRay = true;
	float mfLookAtMaxDistance = -1;
	float mfLookAtDelay = 0;
	float mfLookAtTime = 0;
	bool mbLookedAt = false;
	// cLuxPropLoader::AfterLoad: callbacks and interaction settings from the map's UserVariables
	void ApplyInstanceVars();
	bool mbEffectsActive = true;

	struct cCollideCallback
	{
		tString msChild;
		tString msFunc;
	};
	std::vector<cCollideCallback> mvCollideCallbacks;

	cSomaLuxMap *mpMap = NULL;

	void SetActive(bool abX);
	void SetStaticPhysics(bool abX);
	bool mbStaticPhysics = false;
	std::vector<float> mvDynamicMass;
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
	void MoveLinearTo(const cVector3f &avGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed, const tString &asCallback);
	void UpdateMove(float afTimeStep);
	bool mbMoving = false;
	cVector3f mvMoveGoal;
	float mfMoveAcc = 0, mfMoveMaxSpeed = 0, mfMoveSlowdownDist = 0, mfMoveSpeed = 0;
	tString msMoveCallback;
	int mlCurrentAnim = -1;

	// cLuxProp::CreateAndSetupGui: an ImGui drawn by the map's OnGui function
	class cSomaImGui *mpImGui = NULL;
	tString msOnGuiFunc;
	bool mbGuiActive = false;
	// Prop GUI drawn on a submesh: a 3D gui set over the screen rectangle fitted from its UVs
	cSubMeshEntity *mpGuiSubMesh = NULL;
	cVector3f mvGuiOrigin = 0, mvGuiRight = 0, mvGuiDown = 0;
	void SetupGuiScreen(const tString &asSubMesh);
	void UpdateGuiScreen();
	void UpdateGui(float afTimeStep);
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
// tIDs of engine objects, typed by their script type; lookups check the type and liveness
cSomaID SomaObjectID(void *apObj, const tString &asType);
void *SomaObjectFromID(const cSomaID &aID, const tString &asType);
void SomaClearObjectIDs();

// Oriented boxes of bodies, areas and the player's character body
bool SomaEntitiesCollide(cSomaLuxEntity *apA, cSomaLuxEntity *apB);
// Ray against the entity's boxes; afDistOut is the entry distance
bool SomaRayHitsEntity(cSomaLuxEntity *apEnt, const cVector3f &avStart, const cVector3f &avDir, float afMaxDist, float &afDistOut);
// No colliding body other than apIgnore's between the points
bool SomaLineOfSight(const cVector3f &avStart, const cVector3f &avEnd, cSomaLuxEntity *apIgnore);

// "*" matches any run of characters, as HPL3's wildcard entity names
bool SomaWildcardMatch(const tString &asPattern, const tString &asName);

#endif // SOMA_LUX_ENTITY_H
