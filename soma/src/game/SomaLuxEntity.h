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
	OhplSeNames *mpNames = ohpl_se_names_new();
	enum eSeStr { eSeStr_Name, eSeStr_ClassName, eSeStr_ScriptClassName, eSeStr_FileName, eSeStr_ParentName, eSeStr_AnimLoopSound, eSeStr_InteractCallback, eSeStr_AnimMessageCallback, eSeStr_LookAtCallback, eSeStr_ConnectionCallback, eSeStr_MoveCallback, eSeStr_RotateCallback, eSeStr_AnimCallback, eSeStr_OnGuiFunc, eSeStr_LastEnum };
	tString SeStr(eSeStr aI) const
	{
		size_t l;
		const unsigned char *p = ohpl_se_names_get(mpNames, aI, &l);
		return tString((const char *)p, l);
	}
	// script bindings return const tString&: one stable copy per entity and field, like the former members
	const tString &SeStrRef(eSeStr aI) const
	{
		mvSeStrCache[aI] = SeStr(aI);
		return mvSeStrCache[aI];
	}
	void SetSeStr(eSeStr aI, const tString &asX) { ohpl_se_names_set(mpNames, aI, (const unsigned char *)asX.data(), asX.size()); }
	tString GetName() const { return SeStr(eSeStr_Name); }
	void SetName(const tString &asX) { SetSeStr(eSeStr_Name, asX); }
	tString GetClassName() const { return SeStr(eSeStr_ClassName); }
	void SetClassName(const tString &asX) { SetSeStr(eSeStr_ClassName, asX); }
	tString GetScriptClassName() const { return SeStr(eSeStr_ScriptClassName); }
	void SetScriptClassName(const tString &asX) { SetSeStr(eSeStr_ScriptClassName, asX); }
	tString GetFileName() const { return SeStr(eSeStr_FileName); }
	void SetFileName(const tString &asX) { SetSeStr(eSeStr_FileName, asX); }
	tString GetParentName() const { return SeStr(eSeStr_ParentName); }
	void SetParentName(const tString &asX) { SetSeStr(eSeStr_ParentName, asX); }
	tString GetAnimLoopSound() const { return SeStr(eSeStr_AnimLoopSound); }
	void SetAnimLoopSound(const tString &asX) { SetSeStr(eSeStr_AnimLoopSound, asX); }
	tString GetInteractCallback() const { return SeStr(eSeStr_InteractCallback); }
	void SetInteractCallback(const tString &asX) { SetSeStr(eSeStr_InteractCallback, asX); }
	tString GetAnimMessageCallback() const { return SeStr(eSeStr_AnimMessageCallback); }
	void SetAnimMessageCallback(const tString &asX) { SetSeStr(eSeStr_AnimMessageCallback, asX); }
	tString GetLookAtCallback() const { return SeStr(eSeStr_LookAtCallback); }
	void SetLookAtCallback(const tString &asX) { SetSeStr(eSeStr_LookAtCallback, asX); }
	tString GetConnectionCallback() const { return SeStr(eSeStr_ConnectionCallback); }
	void SetConnectionCallback(const tString &asX) { SetSeStr(eSeStr_ConnectionCallback, asX); }
	tString GetMoveCallback() const { return SeStr(eSeStr_MoveCallback); }
	void SetMoveCallback(const tString &asX) { SetSeStr(eSeStr_MoveCallback, asX); }
	tString GetRotateCallback() const { return SeStr(eSeStr_RotateCallback); }
	void SetRotateCallback(const tString &asX) { SetSeStr(eSeStr_RotateCallback, asX); }
	tString GetAnimCallback() const { return SeStr(eSeStr_AnimCallback); }
	void SetAnimCallback(const tString &asX) { SetSeStr(eSeStr_AnimCallback, asX); }
	tString GetOnGuiFunc() const { return SeStr(eSeStr_OnGuiFunc); }
	void SetOnGuiFunc(const tString &asX) { SetSeStr(eSeStr_OnGuiFunc, asX); }

	iEntity3D *mpCameraNode = nullptr;
	void WakeUp();
	void SetAutoSleep(bool abX);

	cMeshEntity *mpMesh = NULL;
	std::vector<iPhysicsBody *> mvBodies;
	std::vector<bool> mvDefaultCollideCharacter;
	void SetDisableCollisionUntilOutSidePlayer(bool abX);
	void UpdateCharCollision();
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
	iEntity3D *GetAttachEntity();
	cSoundEntity *mpAnimLoopSound = NULL;

	void ResolveConnectedLights();

	cResourceVarsObject mVars;		   // .ent UserDefinedVariables
	cResourceVarsObject mInstanceVars; // map UserVariables
	// std::map semantics: Var() inserts an empty value for a missing key, VarFind() does not
	const tString &Var(const tString &asKey)
	{
		size_t l;
		const unsigned char *p = ohpl_se_var_get(mpNames, (const unsigned char *)asKey.data(), asKey.size(), true, &l);
		tString &sOut = mmapVarCache[asKey];
		sOut.assign((const char *)p, l);
		return sOut;
	}
	mutable tString mvSeStrCache[eSeStr_LastEnum];
	std::map<tString, tString> mmapVarCache;
	bool VarFind(const tString &asKey, tString &asOut)
	{
		size_t l;
		const unsigned char *p = ohpl_se_var_get(mpNames, (const unsigned char *)asKey.data(), asKey.size(), false, &l);
		if (!p)
			return false;
		asOut.assign((const char *)p, l);
		return true;
	}
	void SetVar(const tString &asKey, const tString &asVal) { ohpl_se_var_set(mpNames, (const unsigned char *)asKey.data(), asKey.size(), (const unsigned char *)asVal.data(), asVal.size()); }
	void ClearVars() { ohpl_se_var_clear(mpNames); }
	bool VarAt(size_t aI, tString &asKey, tString &asVal)
	{
		const unsigned char *k;
		size_t kl, vl;
		const unsigned char *v = ohpl_se_var_at(mpNames, aI, &k, &kl, &vl);
		if (!v)
			return false;
		asKey.assign((const char *)k, kl);
		asVal.assign((const char *)v, vl);
		return true;
	}

	// cLuxPropLoader::AfterLoad: callbacks and interaction settings from the map's UserVariables
	void ApplyInstanceVars(cSomaLuxEntity *apPlayer);

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
	std::vector<float> mvDynamicMass;
	void SetEffectsActive(bool abX, bool abFade = false);
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
	void UpdateInteractConnectionLeader();
	void ChangeInteractConnectionState(float afT);
	void ChangeConnectionState(int alState);

	std::vector<cColor> mvEffectDefaults;
	void SetEffectBaseColor(const cColor &aCol);
	void FadeEffectBaseColor(const cColor &aCol, float afTime);
	void UpdateEffectColor(float afTimeStep);
	void SetLiquidAmount(float afX);
	void FadeLiquidAmountTo(float afTarget, float afTime);
	void UpdateLiquidAmount(float afTimeStep);
	bool CollidesWithPlayer();

	void SetHealth(float afX);
	void GiveDamage(float afAmount, int alStrength, const tString &asType, const tString &asSource);
	void Break();
	void DoBreak();
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
	void PlayProcAnimation(const tString &asName, float afLength, bool abLoop, float afAmountFadeTime, float afSpeedFadeTime);
	void UpdateProcAnimations(float afTimeStep);

	void MoveLinearTo(const cVector3f &avGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed, const tString &asCallback);
	void UpdateMove(float afTimeStep);
	void UpdateStaticMoveSound();
	cSoundEntity *mpStaticMoveLoop = NULL;
	void UpdateCheckCollision(float afTimeStep);
	void UpdateLiquid(float afTimeStep);
	void CreateLiquidGraphics(cWorld *apWorld);
	void PlaceLiquidGraphics();
	cMeshEntity *mpLiquidMesh = NULL;
	cFogArea *mpLiquidFog = NULL;
	void MoveAngularTo(const cMatrixf &a_mtxGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed, const cVector3f &avPivotLocal, const tString &asCallback);
	void RotateAtSpeed(float afAcc, float afGoalSpeed, const cVector3f &avAxis, bool abResetSpeed, const cVector3f &avPivotLocal);
	void StopMove();

	// cLuxProp::CreateAndSetupGui: an ImGui drawn by the map's OnGui function
	class cSomaImGui *mpImGui = NULL;
	// cGuiSetEntity::FadeIn/FadeOut
	void SetGuiActive(bool abX, float afFadeTime);
	// cLuxProp::OnVariableUpdate gating of unfocused screens
	// Prop GUI drawn on a submesh: a 3D gui set over the screen rectangle fitted from its UVs
	cSubMeshEntity *mpGuiSubMesh = NULL;
	void SetupGuiScreen(const tString &asSubMesh);
	void UpdateGuiScreen();
	void UpdateGui(float afTimeStep);
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
