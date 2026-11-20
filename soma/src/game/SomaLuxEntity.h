#ifndef SOMA_LUX_ENTITY_H
#define SOMA_LUX_ENTITY_H

#include "RustSomaCore.h"
#include "SomaLuxScriptable.h"

#include <map>

class cSomaLuxMap;

enum eSomaLuxEntityType
{
	eSomaLuxEntityType_Prop,
	eSomaLuxEntityType_Area,
	eSomaLuxEntityType_LiquidArea,
	eSomaLuxEntityType_ReverbArea,
	eSomaLuxEntityType_Critter,
	eSomaLuxEntityType_Agent,
	eSomaLuxEntityType_Player,
	eSomaLuxEntityType_LastEnum,
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
	OhplSeCore *mpCore = ohpl_se_core_new();

	tString msName;
	tString msClassName; // EntityType / AreaType
	tString msScriptClassName;
	iEntity3D *mpCameraNode = nullptr;
	bool &mbSpawned = mpCore->mbSpawned;
	tString msFileName;
	int &meType = mpCore->meType;
	cSomaID &mID = mpCore->mID;
	int &mlParentType = mpCore->mlParentType;
	cSomaID &mParentID = mpCore->mParentID;
	tString msParentName;
	bool &mbActive = mpCore->mbActive;
	bool &mbSleeping = mpCore->mbSleeping;
	bool &mbAutoSleep = mpCore->mbAutoSleep;
	void WakeUp();
	void SetAutoSleep(bool abX);
	bool &mbShowMesh = mpCore->mbShowMesh;
	bool &mbIsDoor = mpCore->mbIsDoor;
	bool &mbIsClosedDoor = mpCore->mbIsClosedDoor;
	bool &mbInteractionDisabled = mpCore->mbInteractionDisabled;
	float &mfMaxInteractDistance = mpCore->mfMaxInteractDistance;
	float &mfVariableUpdateRate = mpCore->mfVariableUpdateRate;
	float &mfVariableUpdateAcc = mpCore->mfVariableUpdateAcc;
	bool &mbInteractedWith = mpCore->mbInteractedWith;
	cMatrixf &m_mtxOnLoad = mpCore->m_mtxOnLoad;
	cVector3f &mvScale = mpCore->mvScale;
	cVector3f &mvMeshScaleMul = mpCore->mvMeshScaleMul;
	bool &mbGlobalSpaceAnim = mpCore->mbGlobalSpaceAnim;
	cVector3f &mvSize = mpCore->mvSize;

	cMeshEntity *mpMesh = NULL;
	std::vector<iPhysicsBody *> mvBodies;
	std::vector<bool> mvDefaultCollideCharacter;
	bool &mbCheckOutsidePlayer = mpCore->mbCheckOutsidePlayer;
	bool &mbCharCollisionUntilStopped = mpCore->mbCharCollisionUntilStopped;
	bool &mbCharCollisionUntilStoppedStarted = mpCore->mbCharCollisionUntilStoppedStarted;
	void SetDisableCollisionUntilOutSidePlayer(bool abX);
	void UpdateCharCollision();
	std::vector<cEntityBodyExtraData> mvBodyExtraData;
	iPhysicsBody *mpMainBody = NULL;
	cVector3f &mvBodyOffset = mpCore->mvBodyOffset;
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
	iEntity3D *GetAttachEntity();
	cSoundEntity *mpAnimLoopSound = NULL;
	int &mlAnimLoopSoundID = mpCore->mlAnimLoopSoundID;
	tString msAnimLoopSound;

	void ResolveConnectedLights();

	cResourceVarsObject mVars;		   // .ent UserDefinedVariables
	cResourceVarsObject mInstanceVars; // map UserVariables
	std::map<tString, tString> mmapScriptVars;

	tString msInteractCallback;
	bool &mbInteractCallbackAutoRemove = mpCore->mbInteractCallbackAutoRemove;
	tString msAnimMessageCallback;
	bool &mbAnimMessageCallbackAutoRemove = mpCore->mbAnimMessageCallbackAutoRemove;
	tString msLookAtCallback;
	bool &mbLookAtCallbackAutoRemove = mpCore->mbLookAtCallbackAutoRemove;
	bool &mbLookAtCheckCenter = mpCore->mbLookAtCheckCenter;
	bool &mbLookAtCheckRay = mpCore->mbLookAtCheckRay;
	float &mfLookAtMaxDistance = mpCore->mfLookAtMaxDistance;
	float &mfLookAtDelay = mpCore->mfLookAtDelay;
	bool &mbForceLookAtCheck = mpCore->mbForceLookAtCheck;
	float &mfLookAtCheckTimer = mpCore->mfLookAtCheckTimer;
	float &mfLookAtCountdown = mpCore->mfLookAtCountdown;
	bool &mbLookedAt = mpCore->mbLookedAt;
	// cLuxPropLoader::AfterLoad: callbacks and interaction settings from the map's UserVariables
	void ApplyInstanceVars(cSomaLuxEntity *apPlayer);
	bool &mbEffectsActive = mpCore->mbEffectsActive;

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
	bool &mbStaticPhysics = mpCore->mbStaticPhysics;
	bool &mbAllowMapTransfer = mpCore->mbAllowMapTransfer;
	std::vector<float> mvDynamicMass;
	void SetEffectsActive(bool abX, bool abFade = false);
	float &mfEffectsAlpha = mpCore->mfEffectsAlpha;
	float &mfEffectsFadeSpeed = mpCore->mfEffectsFadeSpeed;
	float &mfEffectsOffAlpha = mpCore->mfEffectsOffAlpha;
	cColor &mIllumBaseColor = mpCore->mIllumBaseColor;
	bool &mbIllumBaseSet = mpCore->mbIllumBaseSet;
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
	struct cInteractConnection
	{
		tString msName;
		cSomaID mPropID;
		bool mbInvert;
		int mlEasing;
	};
	std::vector<cInteractConnection> mvInteractConnections;
	cSomaID &mInteractLeaderID = mpCore->mInteractLeaderID;
	float &mfInteractConnectionState = mpCore->mfInteractConnectionState;
	void UpdateInteractConnectionLeader();
	void ChangeInteractConnectionState(float afT);
	tString msConnectionCallback;
	void ChangeConnectionState(int alState);

	cColor &mEffectBaseColor = mpCore->mEffectBaseColor;
	cColor &mEffectColorFrom = mpCore->mEffectColorFrom;
	cColor &mEffectColorTo = mpCore->mEffectColorTo;
	float &mfEffectColorTime = mpCore->mfEffectColorTime;
	float &mfEffectColorT = mpCore->mfEffectColorT;
	std::vector<cColor> mvEffectDefaults;
	void SetEffectBaseColor(const cColor &aCol);
	void FadeEffectBaseColor(const cColor &aCol, float afTime);
	void UpdateEffectColor(float afTimeStep);
	float &mfMeshFadeAlpha = mpCore->mfMeshFadeAlpha;
	float &mfMeshFadeSpeed = mpCore->mfMeshFadeSpeed;
	float &mfLiquidAmount = mpCore->mfLiquidAmount;
	float &mfLiquidTarget = mpCore->mfLiquidTarget;
	float &mfLiquidSpeed = mpCore->mfLiquidSpeed;
	void SetLiquidAmount(float afX);
	void FadeLiquidAmountTo(float afTarget, float afTime);
	void UpdateLiquidAmount(float afTimeStep);
	bool CollidesWithPlayer();

	float &mfHealth = mpCore->mfHealth;
	bool &mbBroken = mpCore->mbBroken;
	void SetHealth(float afX);
	void GiveDamage(float afAmount, int alStrength, const tString &asType, const tString &asSource);
	void Break();
	void DoBreak();
	bool &mbBreakable = mpCore->mbBreakable;
	bool &mbScriptCollision = mpCore->mbScriptCollision;
	void AddBodyCallback();
	void OnPhysicsCollision(iPhysicsBody *apBody, iPhysicsBody *apCollideBody, const cPhysicsContactData &aData);
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
	void StopAnimation(int alIdx, float afFadeTime);
	void StopAnimations(float afFadeTime);
	void UpdateAnimation(float afTimeStep);

	// cMeshEntity ProcAnimations: eased submesh offsets from the .ent
	struct cProcTrack
	{
		cSubMeshEntity *mpSub;
		cMatrixf m_mtxBase;
		bool mbRotate, mbReverse;
		cVector3f mvAxes;
		float mfMin, mfMax;
		int mlCycles;
		tString msEasing;
	};
	struct cProcAnim
	{
		tString msName;
		std::vector<cProcTrack> mvTracks;
		OhplGs1Proc mState;
	};
	std::vector<cProcAnim> mvProcAnims;
	bool &mbProcDirty = mpCore->mbProcDirty;
	void PlayProcAnimation(const tString &asName, float afLength, bool abLoop, float afAmountFadeTime, float afSpeedFadeTime);
	void UpdateProcAnimations(float afTimeStep);

	void MoveLinearTo(const cVector3f &avGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed, const tString &asCallback);
	void UpdateMove(float afTimeStep);
	void UpdateStaticMoveSound();
	cSoundEntity *mpStaticMoveLoop = NULL;
	int &mlStaticMoveLoopID = mpCore->mlStaticMoveLoopID;
	bool &mbStaticMoveSound = mpCore->mbStaticMoveSound;
	void UpdateCheckCollision(float afTimeStep);
	void UpdateLiquid(float afTimeStep);
	void CreateLiquidGraphics(cWorld *apWorld);
	void PlaceLiquidGraphics();
	bool &mbCameraInLiquid = mpCore->mbCameraInLiquid;
	bool &mbPlayerInLiquid = mpCore->mbPlayerInLiquid;
	float &mfLiquidTime = mpCore->mfLiquidTime;
	float &mfLiquidSurfaceY = mpCore->mfLiquidSurfaceY;
	cMeshEntity *mpLiquidMesh = NULL;
	cFogArea *mpLiquidFog = NULL;
	cMatrixf &m_mtxLiquidPlaced = mpCore->m_mtxLiquidPlaced;
	bool &mbCheckCollision = mpCore->mbCheckCollision;
	bool &mbCheckCenterInArea = mpCore->mbCheckCenterInArea;
	bool &mbCheckDynamic = mpCore->mbCheckDynamic;
	bool &mbCheckStatic = mpCore->mbCheckStatic;
	bool &mbCheckCharacters = mpCore->mbCheckCharacters;
	float &mfTimeSinceCheck = mpCore->mfTimeSinceCheck;
	OhplGs1Move &mMove = mpCore->mMove;
	tString msMoveCallback;
	void MoveAngularTo(const cMatrixf &a_mtxGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed, const cVector3f &avPivotLocal, const tString &asCallback);
	void RotateAtSpeed(float afAcc, float afGoalSpeed, const cVector3f &avAxis, bool abResetSpeed, const cVector3f &avPivotLocal);
	void StopMove();
	OhplGs1Rotate &mRotate = mpCore->mRotate;
	tString msRotateCallback;
	int &mlCurrentAnim = mpCore->mlCurrentAnim;

	// cLuxProp::CreateAndSetupGui: an ImGui drawn by the map's OnGui function
	class cSomaImGui *mpImGui = NULL;
	tString msOnGuiFunc;
	int &mlGuiDraws = mpCore->mlGuiDraws;
	int &mlGuiCalls = mpCore->mlGuiCalls;
	bool &mbGuiActive = mpCore->mbGuiActive;
	// cGuiSetEntity::FadeIn/FadeOut
	float &mfGuiFade = mpCore->mfGuiFade;
	float &mfGuiFadeSpeed = mpCore->mfGuiFadeSpeed;
	void SetGuiActive(bool abX, float afFadeTime);
	// cLuxProp::OnVariableUpdate gating of unfocused screens
	float &mfGuiFPS = mpCore->mfGuiFPS;
	float &mfGuiTimeAcc = mpCore->mfGuiTimeAcc;
	bool &mbGuiSetUseInput = mpCore->mbGuiSetUseInput;
	bool &mbGuiUpdateWhenOutOfView = mpCore->mbGuiUpdateWhenOutOfView;
	bool &mbGuiDirty = mpCore->mbGuiDirty;
	float &mfGuiCacheAtScreenSize = mpCore->mfGuiCacheAtScreenSize;
	bool &mbGuiCacheForced = mpCore->mbGuiCacheForced;
	// Prop GUI drawn on a submesh: a 3D gui set over the screen rectangle fitted from its UVs
	cSubMeshEntity *mpGuiSubMesh = NULL;
	cVector3f &mvGuiOrigin = mpCore->mvGuiOrigin;
	cVector3f &mvGuiRight = mpCore->mvGuiRight;
	cVector3f &mvGuiDown = mpCore->mvGuiDown;
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
iPhysicsBody *SomaAgentBody(cSomaLuxEntity *apEnt);
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
