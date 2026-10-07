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

class cSomaLuxEntity;

class cSomaGuiScreenRenderer : public iRendererCallback
{
public:
	static cSomaGuiScreenRenderer *Get();
	void Register();
	void Forget(cSomaLuxEntity *apEnt);
	void OnPostSolidDraw(cRendererCallbackFunctions *apFunctions) override;
	void OnPostTranslucentDraw(cRendererCallbackFunctions *) override {}

	struct cTarget
	{
		iTexture *mpTexture = NULL;
		iFrameBuffer *mpBuffer = NULL;
		cVector2l mvSize = 0;
		int mlGuiCalls = -1;
		bool mbGuiActive = false;
	};

private:
	cTarget &GetTarget(cSomaLuxEntity *apEnt, const cVector2l &avSize);

	cViewport *mpViewport = NULL;
	std::map<cSomaLuxEntity *, cTarget> mmapTargets;
};

class cSomaLuxEntity : public cSomaLuxScriptable, public cMeshEntityCallback
{
public:
	~cSomaLuxEntity();

	tString msName;
	tString msClassName; // EntityType / AreaType
	tString msScriptClassName;
	bool mbCameraProxy = false;
	bool mbSpawned = false;
	tString msFileName;
	int meType = eSomaLuxEntityType_Prop;
	cSomaID mID;
	int mlParentType = -1;
	cSomaID mParentID;
	tString msParentName;
	bool mbActive = true;
	bool mbShowMesh = true;
	bool mbIsDoor = false, mbIsClosedDoor = false;
	bool mbInteractionDisabled = false;
	float mfMaxInteractDistance = -1;
	bool mbInteractedWith = false;
	cMatrixf m_mtxOnLoad = cMatrixf::Identity;
	cVector3f mvScale = 1;
	cVector3f mvMeshScaleMul = 1;
	bool mbGlobalSpaceAnim = false;
	cVector3f mvSize = 1; // areas

	cMeshEntity *mpMesh = NULL;
	std::vector<iPhysicsBody *> mvBodies;
	std::vector<cEntityBodyExtraData> mvBodyExtraData;
	iPhysicsBody *mpMainBody = NULL;
	std::vector<iPhysicsJoint *> mvJoints;
	std::vector<iPhysicsJoint *> &Joints();
	// cLuxCritter members at the official offsets, see SomaNewPropBlock
	char *mpCritterProps = NULL;
	std::vector<iLight *> mvLights;
	std::vector<cParticleSystem *> mvParticleSystems;
	std::vector<cBillboard *> mvBillboards;
	std::vector<cLensFlare *> mvLensFlares;
	std::vector<cSoundEntity *> mvSoundEntities;

	// ent <Socket>s: a bone (or the mesh) plus an offset taken in the bind pose
	struct cSocket
	{
		tString msName;
		cBoneState *mpBone;
		cMatrixf m_mtxOffset;
		cNode3D *mpNode = nullptr;
	};
	std::vector<cSocket> mvSockets;
	bool GetSocketMatrix(const tString &asName, cMatrixf &a_mtxOut);
	cNode3D *GetSocketNode(int alIdx);
	void UpdateSocketNodes();

	// iLuxEntity::AttachToEntity/AttachToSocket, driven from the parent after updates
	struct cAttachment
	{
		tString msParent;
		iPhysicsBody *mpBody = NULL;
		tString msSocket;
		bool mbUseRotation = false, mbLocked = false;
		cMatrixf m_mtxParentPrev, m_mtxOffset;
	};
	cAttachment *mpAttachment = NULL;
	iPhysicsBody *GetBodyFromName(const tString &asName);
	void AttachTo(cSomaLuxEntity *apParent, iPhysicsBody *apBody, const tString &asSocket, bool abUseRotation, bool abSnap, bool abLocked);
	void RemoveAttachment();
	bool GetAttachmentParentMatrix(cMatrixf &a_mtxOut);
	void UpdateAttachment();

	void AfterAnimationUpdate(cMeshEntity *, float) override {}
	bool OnAnimationEvent(cMeshEntity *apMesh, cAnimationEvent *apEvent) override;
	void AttachAnimationEventEntity(const tString &asSocket, iEntity3D *apEntity);
	cSoundEntity *mpAnimLoopSound = NULL;
	int mlAnimLoopSoundID = -1;
	tString msAnimLoopSound;

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
	bool mbForceLookAtCheck = false;
	float mfLookAtTime = 0;
	bool mbLookedAt = false;
	// cLuxPropLoader::AfterLoad: callbacks and interaction settings from the map's UserVariables
	void ApplyInstanceVars(cSomaLuxEntity *apPlayer);
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
	void MakeDynamic();
	bool mbStaticPhysics = false;
	bool mbAllowMapTransfer = true;
	std::vector<float> mvDynamicMass;
	void SetEffectsActive(bool abX, bool abFade = false);
	float mfEffectsAlpha = 1, mfEffectsFadeSpeed = 0;
	void ApplyEffectsAlpha();
	void CaptureEffectDefaults();
	cVector3f GetPosition();
	cMatrixf GetMatrix();
	void SetMatrix(const cMatrixf &a_mtx, bool abMainBodyOnly = false);
	iPhysicsBody *GetMainBody() { return mpMainBody ? mpMainBody : mvBodies.empty() ? NULL : mvBodies[0]; }
	void RemoveCollideCallbacks(const tString &asChild);

	struct cConnection
	{
		tString msName;
		tString msEntity;
		bool mbInvert;
		int mlStatesUsed;
	};
	std::vector<cConnection> mvConnections;
	tString msConnectionCallback;
	void ChangeConnectionState(int alState);

	cColor mEffectBaseColor = cColor(1, 1);
	cColor mEffectColorFrom, mEffectColorTo;
	float mfEffectColorTime = 0, mfEffectColorT = 0;
	std::vector<cColor> mvEffectDefaults;
	void SetEffectBaseColor(const cColor &aCol);
	void FadeEffectBaseColor(const cColor &aCol, float afTime);
	void UpdateEffectColor(float afTimeStep);
	bool CollidesWithPlayer();

	float mfHealth = 100;
	bool mbBroken = false;
	void SetHealth(float afX);
	void GiveDamage(float afAmount, int alStrength, const tString &asType, const tString &asSource);
	void Break();
	void DoBreak();
	class cBreakBodyCallback : public iPhysicsBodyCallback
	{
	public:
		cSomaLuxEntity *mpEntity = NULL;
		double mfStartTime = 0;
		bool OnAABBCollide(iPhysicsBody *, iPhysicsBody *) override { return true; }
		void OnBodyCollide(iPhysicsBody *apBody, iPhysicsBody *apCollideBody, cPhysicsContactData *apContactData) override;
	} mBreakCallback;

	// iLuxEntity animation controller on the mesh's animation states
	int PlayAnimation(const tString &asName, float afFadeTime, bool abLoop, const tString &asCallback);
	bool GetAnimationIsPlaying();
	void StopAnimations(float afFadeTime);
	void UpdateAnimation(float afTimeStep);
	void MoveLinearTo(const cVector3f &avGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed, const tString &asCallback);
	void UpdateMove(float afTimeStep);
	void UpdateRotate(float afTimeStep);
	void UpdateCheckCollision(float afTimeStep);
	void UpdateLiquid(float afTimeStep);
	void CreateLiquidGraphics(cWorld *apWorld);
	void PlaceLiquidGraphics();
	bool mbCameraInLiquid = false;
	bool mbPlayerInLiquid = false;
	float mfLiquidTime = 0;
	float mfLiquidSurfaceY = 0;
	cMeshEntity *mpLiquidMesh = NULL;
	cFogArea *mpLiquidFog = NULL;
	cMatrixf m_mtxLiquidPlaced = cMatrixf::Identity;
	bool mbCheckCollision = false, mbCheckCenterInArea = false, mbCheckDynamic = true, mbCheckStatic = false;
	float mfTimeSinceCheck = 0;
	bool mbMoving = false;
	cVector3f mvMoveGoal;
	float mfMoveAcc = 0, mfMoveMaxSpeed = 0, mfMoveSlowdownDist = 0, mfMoveSpeed = 0;
	tString msMoveCallback;
	void MoveAngularTo(const cMatrixf &a_mtxGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed, const cVector3f &avPivotLocal, const tString &asCallback);
	void RotateAtSpeed(float afAcc, float afGoalSpeed, const cVector3f &avAxis, bool abResetSpeed, const cVector3f &avPivotLocal);
	void StopMove();
	int mlRotateMode = 0; // 1 align to goal, 2 constant speed
	cMatrixf m_mtxRotateGoal = cMatrixf::Identity;
	cVector3f mvRotateAxis = 0, mvPivotLocal = 0;
	float mfRotateAcc = 0, mfRotateMaxSpeed = 0, mfRotateSlowdown = 0, mfRotateSpeed = 0;
	tString msRotateCallback;
	int mlCurrentAnim = -1;

	// cLuxProp::CreateAndSetupGui: an ImGui drawn by the map's OnGui function
	class cSomaImGui *mpImGui = NULL;
	tString msOnGuiFunc;
	int mlGuiDraws = 0;
	int mlGuiCalls = 0;
	bool mbGuiActive = true;
	// cGuiSetEntity::FadeIn/FadeOut
	float mfGuiFade = 1, mfGuiFadeSpeed = 0;
	void SetGuiActive(bool abX, float afFadeTime);
	// cLuxProp::OnVariableUpdate gating of unfocused screens
	float mfGuiFPS = 30, mfGuiTimeAcc = 0;
	bool mbGuiSetUseInput = true;
	bool mbGuiUpdateWhenOutOfView = false, mbGuiDirty = true;
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
void SomaRegisterBodyIDs(const std::vector<cSomaLuxEntity *> &avEnts);

// Oriented boxes of bodies, areas and the player's character body
bool SomaEntitiesCollide(cSomaLuxEntity *apA, cSomaLuxEntity *apB);
bool SomaEntityCollidesAABB(cSomaLuxEntity *apEnt, const cVector3f &avMin, const cVector3f &avMax);
float SomaPlayerLiquidSurface();
extern int glSomaUnderwaterUsers;
extern bool gbSomaUnderwaterEffects;
// Ray against the entity's boxes; afDistOut is the entry distance
bool SomaRayHitsEntity(cSomaLuxEntity *apEnt, const cVector3f &avStart, const cVector3f &avDir, float afMaxDist, float &afDistOut);
// No colliding body other than apIgnore's between the points
bool SomaLineOfSight(const cVector3f &avStart, const cVector3f &avEnd, cSomaLuxEntity *apIgnore);
bool SomaPlayerLooksAt(cSomaLuxEntity *apEnt, cCamera *apCam);
bool SomaEntityIsOnScreen(cSomaLuxEntity *apEnt, bool abRayCast);
bool SomaEntityInPlayerLOS(cSomaLuxEntity *apEnt, bool abCheckFOV);

#endif // SOMA_LUX_ENTITY_H
