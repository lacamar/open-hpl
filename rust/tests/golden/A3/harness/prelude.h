#pragma once
// Fake game for the GA2b differential: real math (Rust-backed cMath), hand-copied member lists of
// the player and move states, recording stand-ins for everything else. Function bodies come
// verbatim from orig.inc / new.inc.
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "system/SystemTypes.h"
#include "math/Math.h"
#include "math/MathTypes.h"
#include "math/PidController.h"
#include "system/String.h"
#include "graphics/Color.h"

using hpl::cVector2f;
using hpl::cVector3f;
using hpl::cMatrixf;
using hpl::cMath;
using hpl::cColor;
using hpl::tString;
using hpl::tWString;
using hpl::cPidController;
using hpl::eEulerRotationOrder_XYZ;
#define hplNew(cls, args) new cls args
#define hplDelete(p) delete p
#define _W(x) L##x

#include "RustLuxPlayer.h"
typedef OhplGa2bPid3 PidT;
#ifdef NEWSIDE
#include "RustLuxPlayerA3.h"
#include "impl/RustBackends.h"
using hpl::OhplBackendRustRequested;
#endif
struct cGuiSet;
using hpl::cLinearOscillation;

extern std::string gLog;
void Rec(const char *fmt, ...);
uint32_t Bits(float f);
inline void RecV(const char *n, const cVector3f &v) { Rec("%s %08x %08x %08x", n, Bits(v.x), Bits(v.y), Bits(v.z)); }
inline void RecV2(const char *n, const cVector2f &v) { Rec("%s %08x %08x", n, Bits(v.x), Bits(v.y)); }
inline void RecM(const char *n, const cMatrixf &m)
{
	std::string s = n;
	for(int r = 0; r < 4; ++r) for(int c = 0; c < 4; ++c) { char b[16]; snprintf(b, sizeof b, " %08x", Bits(m.m[r][c])); s += b; }
	Rec("%s", s.c_str());
}
uint32_t HashV(const cVector3f &v, uint32_t salt);
float WorldRand(uint32_t h);

enum eCharDir { eCharDir_Forward, eCharDir_Right, eCharDir_LastEnum };
enum eLuxDamageType { eLuxDamageType_BloodSplat, eLuxDamageType_Claws, eLuxDamageType_Slash, eLuxDamageType_LastEnum };
enum eLuxHeadPosAdd { eLuxHeadPosAdd_Main, eLuxHeadPosAdd_Bob, eLuxHeadPosAdd_ClimbLedge, eLuxHeadPosAdd_Other };
enum eLuxMoveState { eLuxMoveState_Normal, eLuxMoveState_ClimbLedge, eLuxMoveState_LastEnum };
enum eLuxPlayerState
{
	eLuxPlayerState_Normal, eLuxPlayerState_HandObject, eLuxPlayerState_UseItem, eLuxPlayerState_InteractGrab, eLuxPlayerState_InteractPush,
	eLuxPlayerState_InteractSwingDoor, eLuxPlayerState_InteractLever, eLuxPlayerState_InteractWheel, eLuxPlayerState_InteractSlide,
	eLuxPlayerState_Ladder, eLuxPlayerState_LastEnum
};
enum eLuxPlayerAction { eLuxPlayerAction_Interact, eLuxPlayerAction_Attack, eLuxPlayerAction_Ignite, eLuxPlayerAction_Holster, eLuxPlayerAction_Lantern, eLuxPlayerAction_LastEnum };
enum eLuxAction { eLuxAction_Rotate };
enum eSoundEntryType { eSoundEntryType_World };
enum eSoundEntityType { eSoundEntityType_Main };
enum eLuxFootSound { eLuxFootSound_Step, eLuxFootSound_Landing };
enum eLuxFocusCrosshair
{
	eLuxFocusCrosshair_Default, eLuxFocusCrosshair_Grab, eLuxFocusCrosshair_Push, eLuxFocusCrosshair_Ignite, eLuxFocusCrosshair_Pick,
	eLuxFocusCrosshair_LevelDoor, eLuxFocusCrosshair_Ladder, eLuxFocusCrosshair_LastEnum
};
enum eLuxFocusIconStyle { eLuxFocusIconStyle_Default, eLuxFocusIconStyle_Simple };
enum eLuxEntityType { eLuxEntityType_Prop, eLuxEntityType_Other };
enum eLuxPropType { eLuxPropType_Item, eLuxPropType_Other };
inline int32_t OhplGa2bActOf(eLuxPlayerAction aAction)
{
	if(aAction == eLuxPlayerAction_Holster) return kOhplGa2bActHolster;
	if(aAction == eLuxPlayerAction_Attack) return kOhplGa2bActAttack;
	if(aAction == eLuxPlayerAction_Interact) return kOhplGa2bActInteract;
	if(aAction == eLuxPlayerAction_Lantern) return kOhplGa2bActLantern;
	return kOhplGa2bActOther;
}
enum eGuiMaterial { eGuiMaterial_Alpha, eGuiMaterial_Additive };

inline tWString kTranslateW(const char *, const char *) { return L"t"; }
#define kTranslate(a, b) kTranslateW(a, b)
inline void Log(const char *, ...) {}
inline void Error(const char *, ...) {}

namespace hpl { struct iPhysicsBody; struct iPhysicsJoint; struct cShape {}; }
using hpl::iPhysicsBody;
using hpl::iPhysicsJoint;
using hpl::cShape;
typedef std::list<iPhysicsBody *> tPhysicsBodyList;
typedef tPhysicsBodyList::iterator tPhysicsBodyListIt;

namespace hpl {
struct iPhysicsJoint
{
	cVector3f mvPin, mvPivot;
	iPhysicsBody *mpChild, *mpParent;
	cVector3f GetPinDir() { return mvPin; }
	cVector3f GetPivotPoint() { return mvPivot; }
	iPhysicsBody *GetChildBody() { return mpChild; }
	iPhysicsBody *GetParentBody() { return mpParent; }
};
struct iPhysicsBody
{
	int mlId = 0;
	cMatrixf mMtx;
	cVector3f mvVel, mvAng, mvCentre;
	float mfMass = 1;
	cMatrixf mInertia;
	bool mbGravity = true, mbCollideChar = true;
	std::vector<iPhysicsJoint *> mJoints;
	cShape mShape;
	void *mpUser = NULL;
	const cMatrixf &GetLocalMatrix() { return mMtx; }
	cVector3f GetLocalPosition() { return mMtx.GetTranslation(); }
	cVector3f GetLinearVelocity() { return mvVel; }
	cVector3f GetAngularVelocity() { return mvAng; }
	cVector3f GetVelocityAtPosition(const cVector3f &p) { return mvVel + cVector3f(mvAng.x * p.y, mvAng.y * p.z, mvAng.z * p.x); }
	cMatrixf GetInertiaMatrix() { return mInertia; }
	cVector3f GetMassCentre() const { return mvCentre; }
	float GetMass() { return mfMass; }
	bool GetGravity() { return mbGravity; }
	bool GetCollideCharacter() { return mbCollideChar; }
	int GetJointNum() { return (int)mJoints.size(); }
	iPhysicsJoint *GetJoint(int i) { return mJoints[i]; }
	cShape *GetShape() { return &mShape; }
	void *GetUserData() { return mpUser; }
	void SetGravity(bool b) { mbGravity = b; Rec("B.Gravity %d", b); }
	void SetCollideCharacter(bool b) { mbCollideChar = b; Rec("B.CollideChar %d", b); }
	void SetMass(float f) { mfMass = f; Rec("B.Mass %08x", Bits(f)); }
	void SetAngularVelocity(const cVector3f &v) { mvAng = v; RecV("B.SetAng", v); }
	void SetLinearVelocity(const cVector3f &v) { mvVel = v; RecV("B.SetLin", v); }
	void AddForce(const cVector3f &v) { RecV("B.Force", v); }
	void AddTorque(const cVector3f &v) { RecV("B.Torque", v); }
	void AddImpulse(const cVector3f &v) { RecV("B.Impulse", v); }
	void AddForceAtPosition(const cVector3f &v, const cVector3f &p) { RecV("B.ForceAt", v); RecV("B.ForceAtP", p); }
	void AddImpulseAtPosition(const cVector3f &v, const cVector3f &p) { RecV("B.ImpulseAt", v); RecV("B.ImpulseAtP", p); }
	void SetMatrix(const cMatrixf &) {}
};
}

struct cContactPoint { cVector3f mvNormal; };
struct iPhysicsJointHinge : public iPhysicsJoint {};
struct cCollideData { cContactPoint mvContactPoints[1]; void SetMaxSize(int) {} };
struct iPhysicsWorld
{
	cVector3f mvGravity;
	cVector3f GetGravity() { return mvGravity; }
	bool CheckShapeCollision(cShape *, const cMatrixf &a, cShape *, const cMatrixf &b, cCollideData &, int, bool)
	{
		RecM("W.ShapeA", a);
		RecM("W.ShapeB", b);
		return WorldRand(HashV(b.GetTranslation(), 11)) < 0.3f;
	}
};

struct iCharacterBody;
struct iCharacterBodyCallback { virtual ~iCharacterBodyCallback() {} };
struct iPhysicsMaterialSurface { std::string GetStepType() { return "stone"; } };
struct iPhysicsMaterial { iPhysicsMaterialSurface s; iPhysicsMaterialSurface *GetSurfaceData() { return &s; } };

struct iCharacterBody
{
	cVector3f mvPos, mvFeet, mvSize, mvForward, mvRight, mvVel, mvForceVel, mvCustomGravity;
	bool mbOnGround, mbMoved, mbCustomGravityActive;
	float mfMass;
	iPhysicsMaterial mMat;
	cShape mShape;
	iCharacterBodyCallback *mpCallback;
	cVector3f GetPosition() { return mvPos; }
	cVector3f GetFeetPosition() { return mvFeet; }
	cVector3f GetSize() { return mvSize; }
	cVector3f GetForward() { return mvForward; }
	cVector3f GetRight() { return mvRight; }
	cVector3f GetVelocity(float) { return mvVel; }
	cVector3f GetForceVelocity() { return mvForceVel; }
	cVector3f GetCustomGravity() { return mvCustomGravity; }
	bool GetCustomGravityActive() { return mbCustomGravityActive; }
	bool IsOnGround() { return mbOnGround; }
	bool GetMovedLastUpdate() { return mbMoved; }
	float GetMass() { return mfMass; }
	iPhysicsMaterial *GetGravityCollideMaterial() { return &mMat; }
	cShape *GetCurrentShape() { return &mShape; }
	void SetCallback(iCharacterBodyCallback *p) { mpCallback = p; }
	void SetActiveSize(int i) { Rec("C.ActiveSize %d", i); }
	void SetPosition(const cVector3f &v) { RecV("C.SetPos", v); mvPos = v; }
	void SetFeetPosition(const cVector3f &v) { RecV("C.SetFeet", v); }
	void SetForceVelocity(const cVector3f &v) { RecV("C.SetForceVel", v); }
	void SetMass(float f) { Rec("C.Mass %08x", Bits(f)); mfMass = f; }
	void SetYaw(float f) { Rec("C.Yaw %08x", Bits(f)); }
	void SetGravityActive(bool b) { Rec("C.GravActive %d", b); }
	void SetTestCollision(bool b) { Rec("C.TestColl %d", b); }
	void SetDisableDiagSpeedBoost(bool b) { Rec("C.DiagBoost %d", b); }
	void StopMovement() { Rec("C.Stop"); }
	void ResetClimbing() { Rec("C.ResetClimb"); }
	void SetMaxPositiveMoveSpeed(int d, float f) { Rec("C.MaxPos %d %08x", d, Bits(f)); }
	void SetMaxNegativeMoveSpeed(int d, float f) { Rec("C.MaxNeg %d %08x", d, Bits(f)); }
	void SetMoveAcc(int d, float f) { Rec("C.Acc %d %08x", d, Bits(f)); }
	void SetMoveDeacc(int d, float f) { Rec("C.Deacc %d %08x", d, Bits(f)); }
	void SetMoveOppositeDirAccMul(int d, float f) { Rec("C.OppAcc %d %08x", d, Bits(f)); }
	void Move(int d, float f) { Rec("C.Move %d %08x", d, Bits(f)); }
	void AddForce(const cVector3f &v) { RecV("C.Force", v); }
	bool CheckCharacterFits(const cVector3f &p, bool b, int l, cVector3f *pb = NULL, float e = 0)
	{
		RecV("C.Fits", p);
		return WorldRand(HashV(p, 3 + l)) < 0.6f;
	}
	bool CheckRayIntersection(const cVector3f &a, const cVector3f &b, float *, void *)
	{
		RecV("C.RayA", a);
		RecV("C.RayB", b);
		return WorldRand(HashV(a, 5)) < 0.4f;
	}
};

struct cCamera
{
	float mfPitch, mfYaw, mfRoll;
	cVector3f mvPos, mvFwd, mvUp, mvRight;
	float mfPMin, mfPMax;
	float GetPitch() { return mfPitch; }
	float GetYaw() { return mfYaw; }
	float GetRoll() { return mfRoll; }
	cVector3f GetPosition() { return mvPos; }
	cVector3f GetForward() { return mvFwd; }
	cVector3f GetUp() { return mvUp; }
	cVector3f GetRight() { return mvRight; }
	float GetPitchMinLimit() { return mfPMin; }
	float GetPitchMaxLimit() { return mfPMax; }
	void AddPitch(float f) { Rec("Cam.AddPitch %08x", Bits(f)); }
	void AddYaw(float f) { Rec("Cam.AddYaw %08x", Bits(f)); }
	void SetPitchLimits(float a, float b) { Rec("Cam.PitchLim %08x %08x", Bits(a), Bits(b)); }
	void SetYawLimits(float a, float b) { Rec("Cam.YawLim %08x %08x", Bits(a), Bits(b)); }
};

struct cGfx { int mlId; };
typedef cGfx cGuiGfxElement;

struct iLuxEntity;
struct iLuxProp;
struct cLuxInteractData_Grab
{
	bool mbGrabUseDepth, mbGrabUseOffset, mbUseRotation;
	float mfGrabDepth, mfGrabDepthInc, mfGrabMinDepth, mfGrabMaxDepth, mfGrabThrowImpulse, mfGrabMassMul, mfForceMul, mfTorqueMul;
	cVector3f mvGrabPositionOffset, mvGrabRotationOffset;
};
struct cLuxInteractData_Push { bool mbPushAtPoint; float mfPushForceMul, mfPushImpulse; };
struct cLuxInteractData_Slide { float mfSlideMaxSpeed, mfSlideSlowDownFactor, mfSlideSpeedFactor, mfSlideThrowImpulse; };
struct iLuxInteractData_RotateBase { float mfMoveMaxSpeed, mfMoveSlowDownFactor, mfMoveSpeedFactor, mfMoveThrowImpulse; };

struct cSubMeshEntity { iPhysicsBody *mpParent; iPhysicsBody *GetEntityParent() { return mpParent; } };
struct cMeshEntity
{
	std::vector<cSubMeshEntity> mSubs;
	int GetSubMeshEntityNum() { return (int)mSubs.size(); }
	cSubMeshEntity *GetSubMeshEntity(int i) { return &mSubs[i]; }
};

struct iLuxEntity
{
	void InFocusDraw(cGuiSet *, float f);
	eLuxEntityType mType;
	float mfMaxFocus;
	bool mbCanInteract, mbDisabled;
	eLuxFocusCrosshair mCrosshair;
	virtual ~iLuxEntity() {}
	eLuxEntityType GetEntityType() { return mType; }
	float GetMaxFocusDistance() { return mfMaxFocus; }
	bool CanInteract(iPhysicsBody *) { Rec("E.CanInteract"); return mbCanInteract; }
	bool GetInteractionDisabled() { return mbDisabled; }
	eLuxFocusCrosshair GetFocusCrosshair(iPhysicsBody *, const cVector3f &) { Rec("E.Crosshair"); return mCrosshair; }
	void OnInteract(iPhysicsBody *, const cVector3f &) { Rec("E.OnInteract"); }
	void RunInteractCallbackFunc() { Rec("E.InteractCb"); }
	tString GetName() { return "ent"; }
	tWString GetFocusText() { return L"x"; }
};
struct iLuxProp : public iLuxEntity
{
	eLuxPropType mPropType;
	cLuxInteractData_Grab mGrab;
	cLuxInteractData_Push mPush;
	cLuxInteractData_Slide mSlide;
	iLuxInteractData_RotateBase mMove;
	cMeshEntity mMesh;
	bool mbConnected;
	std::vector<iPhysicsBody *> mBodies;
	eLuxPropType GetPropType() { return mPropType; }
	cLuxInteractData_Grab *GetGrabData() { return &mGrab; }
	cLuxInteractData_Push *GetPushData() { return &mPush; }
	cLuxInteractData_Slide *GetSlideData() { return &mSlide; }
	iLuxInteractData_RotateBase *GetMoveBaseData() { return &mMove; }
	cMeshEntity *GetEffectMeshEntity() { return &mMesh; }
	bool ShowOutlinesOnConnectedBodies() { return mbConnected; }
	void SetIsInteractedWith(bool b) { Rec("P.Interacted %d", b); }
	void SetDisableCollisionUntilOutSidePlayer(bool b) { Rec("P.DisableColl %d", b); }
	int GetBodyNum() { return (int)mBodies.size(); }
	iPhysicsBody *GetBody(int i) { return mBodies[i]; }
};

struct cLuxArea_Ladder
{
	float mfMinY, mfMaxY;
	cVector3f mvFwd, mvStartRot;
	std::string msMat;
	float GetMinY() { return mfMinY; }
	float GetMaxY() { return mfMaxY; }
	cVector3f GetForward() { return mvFwd; }
	cVector3f GetStartRotation() { return mvStartRot; }
	std::string GetMaterial() { return msMat; }
};
struct cLuxInventory_Item { cGuiGfxElement mGfx; cGuiGfxElement *GetImage() { return &mGfx; } tString GetName() { return "item"; } };

struct cLuxUseItemCallback { bool mbAutoDestroy; tString msName, msFunction, msItem, msEntity; };
struct cLuxMap
{
	iPhysicsWorld mWorld;
	cLuxUseItemCallback mCb;
	bool mbHasCb;
	cLuxMap *GetWorld() { return this; }
	iPhysicsWorld *GetPhysicsWorld() { return &mWorld; }
	cLuxUseItemCallback *GetUseItemCallback(const tString &, const tString &) { Rec("M.GetCb"); return mbHasCb ? &mCb : NULL; }
	void RunScript(const tString &s) { Rec("M.Script %s", s.c_str()); }
	void RemoveUseItemCallback(cLuxUseItemCallback *, const tString &s) { Rec("M.RemoveCb %s", s.c_str()); }
	void BroadcastEnemySoundMessage(const cVector3f &p, float a, float b, float c) { RecV("M.Enemy", p); Rec("M.EnemyV %08x %08x %08x", Bits(a), Bits(b), Bits(c)); }
};

struct cLuxSoundExtraData { float mfVolume = 0.7f, mfMinDistance = 2, mfMaxDistance = 9; };
struct cLuxHelpFuncs
{
	void PlayGuiSoundData(const tString &s, int, float = 1, int = 0, bool = false, cLuxSoundExtraData * = NULL) { Rec("S.Play %s", s.c_str()); }
};
struct cLuxMapHandler { cLuxMap mMap; cLuxMap *GetCurrentMap() { return &mMap; } };
struct cLuxMessageHandler { void SetMessage(const tWString &, int) { Rec("Msg"); } };
struct cLuxEffectRenderer
{
	void ClearOutlineObjects() { Rec("Fx.Clear"); }
	void AddOutlineObject(cSubMeshEntity *s) { Rec("Fx.Add %d", s->mpParent ? s->mpParent->mlId : -1); }
};
struct cLuxMapHelper
{
	iPhysicsBody *mpBody;
	iLuxEntity *mpEntity;
	float mfDist;
	void GetClosestEntity(const cVector3f &, const cVector3f &, float, float *d, iPhysicsBody **b, iLuxEntity **e) { *d = mfDist; *b = mpBody; *e = mpEntity; }
};
struct cLuxGameCfg
{
	float GetFloat(const char *a, const char *k, float def);
	tString GetString(const char *, const char *k, const char *) { return std::string("snd_") + k; }
	cVector2f GetVector2f(const char *a, const char *k, float def) { return cVector2f(GetFloat(a, k, def), GetFloat(a, k, def) * 0.5f); }
};
struct cGuiSet { void DrawGfx(cGuiGfxElement *, const cVector3f &, const cVector2f &, const cColor &, eGuiMaterial) { Rec("Gui.Draw"); } };
struct cGui
{
	cGuiGfxElement mGfx[16];
	int mlN = 0;
	cGuiGfxElement *CreateGfxImage(const char *, eGuiMaterial) { mGfx[mlN].mlId = mlN; return &mGfx[mlN++]; }
};
struct cInput { bool mbTrig; bool IsTriggerd(int) { return mbTrig; } };
struct cEngine { cGui mGui; cInput mInput; cGui *GetGui() { return &mGui; } cInput *GetInput() { return &mInput; } };
struct cLuxInputHandler
{
	bool mbSmooth, mbInvert;
	cVector2f mvSmooth;
	bool GetSmoothMouse() { return mbSmooth; }
	cVector2f GetSmoothMousePos(const cVector2f &v) { return mvSmooth; }
	void ResetSmoothMousePos() { Rec("In.Reset"); }
	bool GetInvertMouse() { return mbInvert; }
	bool IsGamepadPresent() { return false; }
	bool GetInvertGamepadLook() { return false; }
};

struct cLuxPlayer;
struct cLuxInsanity { bool mbActive; bool IsActive() { return mbActive; } };
struct cLuxLantern
{
	bool mbActive;
	bool IsActive() { return mbActive; }
	void SetActive(bool a, bool b, bool c, bool d) { Rec("Lantern %d %d %d %d", a, b, c, d); }
};
struct cLuxHands { void DoAction(eLuxPlayerAction a, bool p) { Rec("Hands.DoAction %d %d", (int)a, p); } };
class iLuxMoveState;
class iLuxPlayerState;
struct cLuxBase;
extern cLuxBase *gpBase;

struct cLuxPlayer
{
	cCamera mCam;
	iCharacterBody mBody;
	cLuxInsanity mCollapse;
	cLuxLantern mLantern;
	cLuxHands mHands;
	iLuxMoveState *mpMoveNormal, *mpMoveLedge;
	eLuxMoveState mMoveState;
	float mfInteractMul, mfScriptMul, mfEventMul, mfHurtMul, mfCollapseMul, mfWaterMul, mfScriptRun, mfEventRun, mfJumpMul, mfDefaultMass;
	cVector3f mvBodySize, mvCrouchSize, mvHeadAdd;
	bool mbCrouchDisabled, mbInWater, mbPressRun, mbPressedMove, mbNoFall, mbDrag;
	eLuxFocusIconStyle mIcon;
	cCamera *GetCamera() { return &mCam; }
	iCharacterBody *GetCharacterBody() { return &mBody; }
	cLuxInsanity *GetInsanityCollapse() { return &mCollapse; }
	cLuxLantern *GetHelperLantern() { return &mLantern; }
	cLuxHands *GetHands() { return &mHands; }
	eLuxMoveState GetCurrentMoveState() { return mMoveState; }
	iLuxMoveState *GetMoveStateData(eLuxMoveState s) { return s == eLuxMoveState_Normal ? mpMoveNormal : mpMoveLedge; }
	iLuxMoveState *GetCurrentMoveStateData() { return GetMoveStateData(mMoveState); }
	void ChangeState(eLuxPlayerState s) { Rec("P.ChangeState %d", (int)s); }
	void ChangeMoveState(eLuxMoveState s) { Rec("P.ChangeMove %d", (int)s); }
	float GetInteractionMoveSpeedMul() { return mfInteractMul; }
	float GetScriptMoveSpeedMul() { return mfScriptMul; }
	float GetEventMoveSpeedMul() { return mfEventMul; }
	float GetHurtMoveSpeedMul() { return mfHurtMul; }
	float GetInsanityCollapseSpeedMul() { return mfCollapseMul; }
	float GetWaterSpeedMul() { return mfWaterMul; }
	float GetScriptRunSpeedMul() { return mfScriptRun; }
	float GetEventRunSpeedMul() { return mfEventRun; }
	float GetScriptJumpForceMul() { return mfJumpMul; }
	float GetDefaultMass() { return mfDefaultMass; }
	bool IsInWater() { return mbInWater; }
	bool GetCrouchDisabled() { return mbCrouchDisabled; }
	bool IsPressingRun() { return mbPressRun; }
	bool GetPressedMove() { return mbPressedMove; }
	bool GetNoFallDamage() { return mbNoFall; }
	bool UsesDragFootsteps() { return mbDrag; }
	std::string GetWaterStepSound() { return "water"; }
	cVector3f GetBodySize() { return mvBodySize; }
	cVector3f GetBodyCrouchSize() { return mvCrouchSize; }
	cVector3f GetHeadPosAdd(eLuxHeadPosAdd) { return mvHeadAdd; }
	void SetHeadPosAdd(eLuxHeadPosAdd t, const cVector3f &v) { Rec("P.SetHeadAdd %d", (int)t); RecV("P.SetHeadAddV", v); }
	void MoveHeadPosAdd(eLuxHeadPosAdd t, const cVector3f &v, float a, float b) { Rec("P.MoveHeadAdd %d %08x %08x", (int)t, Bits(a), Bits(b)); RecV("P.MoveHeadAddV", v); }
	void GiveDamage(float a, float b, eLuxDamageType t, bool c, bool d) { Rec("P.Damage %08x %08x %d %d %d", Bits(a), Bits(b), (int)t, c, d); }
	void SetInteractionMoveSpeedMul(float f) { Rec("P.InteractMul %08x", Bits(f)); mfInteractMul = f; }
	void SetCurrentHandObjectDrawn(bool b) { Rec("P.HandDrawn %d", b); }
	eLuxFocusIconStyle GetFocusIconStyle() { return mIcon; }
	void SetEntityInFocus(iLuxEntity *) {}
	void SetBodyInFocus(iPhysicsBody *) {}
	void SetCurrentFocusDistance(float f) { Rec("P.FocusDist %08x", Bits(f)); }
	void SetFocusText(const tWString &) { Rec("P.FocusText"); }
};

struct cLuxBase
{
	cEngine *mpEngine;
	cLuxGameCfg *mpGameCfg;
	cLuxHelpFuncs *mpHelpFuncs;
	cLuxMapHandler *mpMapHandler;
	cLuxMessageHandler *mpMessageHandler;
	cLuxEffectRenderer *mpEffectRenderer;
	cLuxMapHelper *mpMapHelper;
	cLuxInputHandler *mpInputHandler;
	cLuxPlayer *mpPlayer;
	cGuiSet *mpGameHudSet;
	bool mbHardMode;
};

class iLuxMoveState
{
public:
	iLuxMoveState(cLuxPlayer *apPlayer);
	virtual ~iLuxMoveState() {}
	void Update(float afTimeStep);
	virtual void OnUpdate(float afTimeStep) = 0;
	virtual bool GetAllowsInteraction() = 0;
	cLuxPlayer *mpPlayer;
	float mfMaxForwardSpeed, mfMaxBackwardSpeed, mfMaxSidwaySpeed, mfForwardAcc, mfSidewayAcc, mfForwardDeacc, mfSidewayDeacc;
	float mfForwardOppositeDirAccMul, mfSidewayOppositeDirAccMul, mfMaxForwardSpeedMul, mfMaxBackwardSpeedMul, mfMaxSidwaySpeedMul;
};

class cLuxMoveState_Normal;
class cLuxMoveState_Normal_Callback : public iCharacterBodyCallback
{
public:
	cLuxMoveState_Normal_Callback(cLuxMoveState_Normal *apMoveState) { mpMoveState = apMoveState; }
	void OnGravityCollide(iCharacterBody *apCharBody, iPhysicsBody *apBody, cCollideData *apCollideData);
	cLuxMoveState_Normal *mpMoveState;
};

class cLuxMoveState_Normal : public iLuxMoveState
{
public:
	cLuxMoveState_Normal(cLuxPlayer *apPlayer);
	~cLuxMoveState_Normal() { delete mpCallback; }
	void OnEnterState(eLuxMoveState aPrevState);
	void OnUpdate(float afTimeStep);
	bool GetAllowsInteraction() { return true; }
	void OnRun(bool abActive);
	void OnCrouch(bool abActive);
	void OnJump(bool abActive);
	void Jump();
	bool IsRunning() { return mbRunning; }
	bool IsCrouching() { return mbCrouching; }
	void SetCrouch(bool abActive);
	void ResetJumping();
	float GetMoveSpeedMul();
	float GetRunSpeedMul();
	bool UpdateLedgeCheck(float) { return false; }
	void UpdateMovement(float afTimeStep);
	void UpdateSpeedMultipliers(float afTimeStep);
	void UpdateJumpAndGroundCheck(float afTimeStep);
	void UpdateHeadBob(float afTimeStep);
	void FootSound(eLuxFootSound aType);
	void FallDamage(float afYSpeed);
	cLuxMoveState_Normal_Callback *mpCallback;
	tString msJumpSound, msCrouchSound, msStandSound;
	bool mbWasMoving;
	float mfRunForwardMul, mfRunBackwardMul, mfRunSidewayMul, mfCrouchForwardMul, mfCrouchBackwardMul, mfCrouchSidewayMul;
	float mfInAirForwardMul, mfInAirBackwardMul, mfInAirSidewayMul, mfDefaultForwardMul, mfMinHitGroundBounceSpeed;
	bool mbRunning, mbCrouching, mbJumping;
	float mfJumpStartForce, mfJumpCrouchStartForce, mfMaxJumpCount, mfJumpCount;
	cVector3f mvHeadBob;
	float mfPrevHeadBobCount, mfHeadBobCount;
	bool mbBobbing, mbWasOnGround, mbHeadGroundBounceActive;
	float mfHeadGroundBounce;
	cVector2f mvWalkBobMax, mvRunBobMax, mvCrouchBobMax;
	float mfWalkMinBobSpeed, mfWalkMaxBobSpeed, mfRunMinBobSpeed, mfRunMaxBobSpeed, mfCrouchMinBobSpeed, mfCrouchMaxBobSpeed;
	cVector2f mvCurrentBobMax, mvBobMaxGoal;
	float mfGroundBounceSize, mfGroundBounceSpeed, mfBounceSizeMul, mfBounceSpeedMul, mfClimbLedgeCount, mfMaxClimbLedgeCount;
	float mfClimbLedgeCheckDistForward, mfClimbLedgeCheckDistFromTop, mfClimbLedgeCheckDistTopToEnd, mfClimbLedgeMaxPushDist;
	float mfFallDamageBounceSizeMul, mfFallDamageBounceSpeedMul, mfFallDamageSpeed_Min, mfFallDamage_Min;
	tString msFallDamageSound_Min;
	float mfFallDamageSpeed_Med, mfFallDamage_Med;
	tString msFallDamageSound_Med;
	float mfFallDamageSpeed_Max, mfFallDamage_Max;
	tString msFallDamageSound_Max;
};

class cLuxMoveState_ClimbLedge : public iLuxMoveState
{
public:
	cLuxMoveState_ClimbLedge(cLuxPlayer *apPlayer);
	void OnEnterState(eLuxMoveState aPrevState);
	void OnUpdate(float afTimeStep);
	bool GetAllowsInteraction() { return false; }
	float mfHeadMoveSpeed, mfHeadMoveSlowdownDist, mfGivePlayerControlDist, mfMovePitchFactor, mfMaxMovePitchSpeed;
};

struct cLuxPlayerStateVars
{
	static tString msHandObject;
	static cVector3f mvInteractPos;
	static iPhysicsBody *mpInteractBody;
	static iLuxProp *mpInteractProp;
	static cLuxInventory_Item *mpInventoryItem;
	static cLuxArea_Ladder *mpLadder;
	static cVector3f mvLadderStartPos;
	static void SetupInteraction(iPhysicsBody *apBody, const cVector3f &avInteractPos);
};

#ifdef NEWSIDE
#define A3_EXTRA , OhplA3Base *apA3
#define M_REF(type, name, expr) type &name = expr;
#define M_ARR(type, name, n, expr) type(&name)[n] = expr;
#define M_PTR(type, name, expr) type *&name = expr;
#define M_NOINIT(type, name) type &name;
#define HOLD(T) private OhplA3Holder<T>,
#else
#define A3_EXTRA
#define M_REF(type, name, expr) type name;
#define M_ARR(type, name, n, expr) type name[n];
#define M_PTR(type, name, expr) type *name;
#define M_NOINIT(type, name) type name;
#define HOLD(T)
#endif

class iLuxPlayerState
{
public:
	iLuxPlayerState(cLuxPlayer *apPlayer, eLuxPlayerState aType A3_EXTRA);
	virtual ~iLuxPlayerState() {}
	cLuxPlayer *mpPlayer;
	eLuxPlayerState mType, mPreviousState;
	cGuiGfxElement *mpDefaultCrosshairGfx, *mpSimpleInteractCrosshairGfx;
#ifdef NEWSIDE
	OhplA3Base *mpA3;
	static int32_t A3Host(void *apCtx, uint32_t alOp, OhplA3Io *apIo);
	static int32_t A3HostReal(void *apCtx, uint32_t alOp, OhplA3Io *apIo);
	OhplA3Host A3HostTable() { OhplA3Host h = {this, A3Host}; return h; }
#endif
};

class iLuxPlayerState_Interact : public iLuxPlayerState
{
public:
	iLuxPlayerState_Interact(cLuxPlayer *apPlayer, eLuxPlayerState aType A3_EXTRA);
	void SetupInteractVars();
	void ResetInteractVars();
	void OnDestroyEntity(iLuxEntity *apEntity);
	void OnAttachBodyToStickyArea(iPhysicsBody *apBody);
	M_NOINIT(iPhysicsBody *, mpCurrentBody)
	M_NOINIT(iLuxProp *, mpCurrentProp)
	M_NOINIT(cVector3f, mvCurrentFocusPos)
};

struct cGrabbedBodyProperties { iPhysicsBody *mpBody; bool mbHasGravity, mbCollideCharacter; float mfMass; };
struct cGrabbedSubMeshProperties { void *mpCustomTransMaterial; };

class cLuxPlayerState_InteractGrab : HOLD(OhplA3Grab) public iLuxPlayerState_Interact
{
public:
	cLuxPlayerState_InteractGrab(cLuxPlayer *apPlayer);
	~cLuxPlayerState_InteractGrab();
	void OnEnterState(eLuxPlayerState aPrevState);
	void OnLeaveState(eLuxPlayerState aNewState);
	void PostUpdate(float afTimeStep);
	bool OnDoAction(eLuxPlayerAction aAction, bool abPressed);
	void OnScroll(float afAmount);
	bool OnAddYaw(float afAmount);
	bool OnAddPitch(float afAmount);
	void OnSaveBody(iPhysicsBody *apBody, float &afMass, bool &abCollideCharacter);
	bool AllowBuoyancy(iPhysicsBody *apBody);
	void SaveBodyProperties(iPhysicsBody *apBody);
	bool BodyIsAdded(iPhysicsBody *apBody);
	M_PTR(cLuxInteractData_Grab, mpGrabData, mR.data)
	M_REF(float, mfMaxForce, mR.maxForce) M_REF(float, mfMaxTorque, mR.maxTorque) M_REF(float, mfMaxAngularSpeed, mR.maxAngularSpeed)
	M_REF(float, mfMinSlowPlayerMass, mR.minSlowMass) M_REF(float, mfMaxSlowPlayerMass, mR.maxSlowMass) M_REF(float, mfMinSlowPlayerMul, mR.minSlowMul)
	M_REF(float, mfMaxLeaveAngularSpeed, mR.maxLeaveAngular) M_REF(float, mfMaxLeaveLinearSpeed, mR.maxLeaveLinear)
	std::vector<cGrabbedBodyProperties> mvBodyProperties;
	std::vector<cGrabbedSubMeshProperties> mvSubMeshProperties;
	M_REF(bool, mbCustomMaterialsSetup, mR.customMaterials)
	M_REF(cVector3f, mvLocalBodyOffset, mR.localBodyOffset)
	M_REF(cMatrixf, m_mtxBodyRotation, mR.bodyRotation)
	M_REF(PidT, mForcePid, mR.forcePid) M_REF(PidT, mSpeedTorquePid, mR.torquePid)
	M_REF(float, mfDepth, mR.depth) M_REF(float, mfMaxDistance, mR.maxDistance) M_REF(float, mfMassSum, mR.massSum)
};

class cLuxPlayerState_InteractPush : HOLD(OhplA3Push) public iLuxPlayerState_Interact
{
public:
	cLuxPlayerState_InteractPush(cLuxPlayer *apPlayer);
	void OnEnterState(eLuxPlayerState aPrevState);
	void OnLeaveState(eLuxPlayerState aNewState);
	void Update(float afTimeStep);
	void PostUpdate(float afTimeStep);
	bool OnDoAction(eLuxPlayerAction aAction, bool abPressed);
	bool OnMove(eCharDir aDir, float afMul);
	void ClearMoveVars();
	M_PTR(cLuxInteractData_Push, mpPushData, mR.data)
	M_ARR(float, mfTotalMoveMul, 2, mR.totalMoveMul) M_ARR(bool, mbMoving, 2, mR.moving) M_ARR(float, mfMaxSpeedInDir, 2, mR.maxSpeedInDir)
	M_REF(PidT, mVelocityPid, mR.velocityPid) M_REF(PidT, mStopVelocityPid, mR.stopPid)
	M_REF(cVector3f, mvLocalAttachPos, mR.localAttach) M_REF(cVector3f, mvPushDir, mR.pushDir)
	M_REF(float, mfMaxAttachPointDist, mR.maxAttachDist) M_REF(float, mfWalkMaxSpeed, mR.walkMaxSpeed) M_REF(float, mfWalkForce, mR.walkForce)
	M_REF(float, mfRunMaxSpeed, mR.runMaxSpeed) M_REF(float, mfRunForce, mR.runForce) M_REF(float, mfCrouchMaxSpeed, mR.crouchMaxSpeed)
	M_REF(float, mfCrouchForce, mR.crouchForce) M_REF(float, mfPitchMinRange, mR.pitchMinRange) M_REF(float, mfPitchMaxRange, mR.pitchMaxRange)
	M_REF(float, mfYawRange, mR.yawRange) M_REF(float, mfPrevMinPitchLimit, mR.prevMinPitch) M_REF(float, mfPrevMaxPitchLimit, mR.prevMaxPitch)
	M_REF(float, mfMaxForce, mR.maxForce)
};

class cLuxPlayerState_InteractSlide : HOLD(OhplA3Slide) public iLuxPlayerState_Interact
{
public:
	cLuxPlayerState_InteractSlide(cLuxPlayer *apPlayer);
	void OnEnterState(eLuxPlayerState aPrevState);
	void OnLeaveState(eLuxPlayerState aNewState);
	void Update(float afTimeStep);
	bool OnDoAction(eLuxPlayerAction aAction, bool abPressed);
	bool OnAddYaw(float afAmount);
	bool OnAddPitch(float afAmount);
	bool OnMove(eCharDir aDir, float afMul);
	M_PTR(cLuxInteractData_Slide, mpSlideData, mR.data)
	M_REF(PidT, mForcePid, mR.forcePid)
	M_PTR(iPhysicsJoint, mpCurrentJoint, mR.joint)
	M_REF(bool, mbHasGravity, mR.hasGravity)
	M_REF(float, mfMaxDistance, mR.maxDistance)
	M_REF(cVector3f, mvLocalInteractPos, mR.localInteractPos)
	M_REF(cVector2f, mvMouseAdd, mR.mouseAdd) M_REF(cVector2f, mvLastMouseAdd, mR.lastMouseAdd)
	M_REF(cVector3f, mvLastForce, mR.lastForce)
	M_REF(float, mfSlideSpeed, mR.slideSpeed) M_REF(float, mfMoveToMouseAddFactor, mR.moveToMouseFactor) M_REF(float, mfMaxForce, mR.maxForce)
};

class iLuxPlayerState_InteractRotateBase : HOLD(OhplA3Rotate) public iLuxPlayerState_Interact
{
public:
	iLuxPlayerState_InteractRotateBase(cLuxPlayer *apPlayer, eLuxPlayerState aState);
	virtual ~iLuxPlayerState_InteractRotateBase();
	void OnEnterState(eLuxPlayerState aPrevState);
	void OnLeaveState(eLuxPlayerState aNewState);
	void Update(float afTimeStep);
	bool OnDoAction(eLuxPlayerAction aAction, bool abPressed);
	bool OnMove(eCharDir aDir, float afMul);
	bool OnAddYaw(float afAmount);
	bool OnAddPitch(float afAmount);
	virtual float GetSpeedAdd(cCamera *apCam) = 0;
	virtual void OnThrow() = 0;
	virtual void EnterRotateBase(eLuxPlayerState) {}
	virtual void LeaveRotateBase(eLuxPlayerState) {}
	M_PTR(iLuxInteractData_RotateBase, mpMoveBaseData, mR.data)
	M_PTR(iPhysicsJoint, mpCurrentJoint, mR.joint)
	M_REF(float, mfMoveToMouseAddFactor, mR.moveToMouseFactor) M_REF(float, mfMaxTorque, mR.maxTorque)
	M_REF(bool, mbHasGravity, mR.hasGravity)
	M_REF(float, mfMaxDistance, mR.maxDistance)
	M_REF(cVector3f, mvJointRight, mR.jointRight) M_REF(cVector3f, mvJointForward, mR.jointForward)
	M_ARR(cVector3f, mvForceAxis, 2, mR.forceAxis)
	M_REF(cVector2f, mvMouseAdd, mR.mouseAdd) M_REF(cVector2f, mvLastMouseAdd, mR.lastMouseAdd)
	M_REF(float, mfRotSpeed, mR.rotSpeed)
	M_REF(PidT, mRotatePid, mR.rotatePid)
	M_ARR(cVector3f, mvCamDir, 3, mR.camDir)
	M_ARR(float, mfAxisCos, 2, mR.axisCos)
};

class cLuxPlayerState_InteractLever : public iLuxPlayerState_InteractRotateBase
{
public:
	cLuxPlayerState_InteractLever(cLuxPlayer *p) : iLuxPlayerState_InteractRotateBase(p, eLuxPlayerState_InteractLever) {}
	float GetSpeedAdd(cCamera *apCam);
	void OnThrow();
};

class cLuxPlayerState_InteractSwingDoor : public iLuxPlayerState_InteractRotateBase
{
public:
	cLuxPlayerState_InteractSwingDoor(cLuxPlayer *p) : iLuxPlayerState_InteractRotateBase(p, eLuxPlayerState_InteractSwingDoor) {}
	float GetSpeedAdd(cCamera *apCam);
	void OnThrow();
};

class cLuxPlayerState_InteractWheel : public iLuxPlayerState_InteractRotateBase
{
public:
	cLuxPlayerState_InteractWheel(cLuxPlayer *apPlayer);
	float GetSpeedAdd(cCamera *apCam);
	void OnThrow();
	void EnterRotateBase(eLuxPlayerState aPrevState);
	std::list<cVector2f> mlstMoveAdds;
};

class cLuxPlayerState_Ladder : HOLD(OhplA3Ladder) public iLuxPlayerState
{
public:
	cLuxPlayerState_Ladder(cLuxPlayer *apPlayer);
	void OnEnterState(eLuxPlayerState aPrevState);
	void OnLeaveState(eLuxPlayerState aNewState);
	void Update(float afTimeStep);
	bool OnMove(eCharDir aDir, float afMul);
	bool OnAddYaw(float afAmount);
	bool OnAddPitch(float afAmount);
	bool OnRun(bool abPressed);
	bool OnJump(bool abPressed);
	bool OnCrouch(bool abPressed);
	bool OnDoAction(eLuxPlayerAction aAction, bool abPressed);
	void PlaySound(const tString &asType);
	void SetupHeadTurnLimits();
	M_REF(int, mlState, mR.state)
	M_PTR(cLuxArea_Ladder, mpLadder, mR.ladder)
	M_REF(cVector3f, mvStartPosition, mR.startPosition)
	M_REF(float, mfTimeCount, mR.timeCount)
	M_REF(cVector3f, mvCharPosition, mR.charPosition) M_REF(cVector3f, mvGoalPos, mR.goalPos) M_REF(cVector3f, mvGoalRot, mR.goalRot)
	M_REF(cVector3f, mvPosAdd, mR.posAdd) M_REF(cVector3f, mvRotAdd, mR.rotAdd)
	M_REF(float, mfMoveMul, mR.moveMul) M_REF(float, mfUpSpeed, mR.upSpeed) M_REF(float, mfDownSpeed, mR.downSpeed)
	M_REF(float, mfStepLength, mR.stepLength) M_REF(float, mfStepCount, mR.stepCount)
	M_REF(bool, mbPlayedSound, mR.playedSound)
	M_REF(float, mfPitchMaxLimit, mR.pitchMaxLimit) M_REF(float, mfPitchMinLimit, mR.pitchMinLimit) M_REF(float, mfLeaveAtTopCount, mR.leaveAtTop)
	M_REF(bool, mbLanternDrawn, mR.lanternDrawn)
};

class iLuxPlayerState_DefaultBase : public iLuxPlayerState
{
public:
	iLuxPlayerState_DefaultBase(cLuxPlayer *apPlayer, eLuxPlayerState aType A3_EXTRA);
	void OnEnterState(eLuxPlayerState aPrevState);
	void OnLeaveState(eLuxPlayerState aNewState);
	void OnMapLeave(cLuxMap *apMap);
	void Update(float afTimeStep);
	virtual void OnDraw(cGuiSet *apGuiSet, float afFrameTime);
	virtual cGuiGfxElement *GetCrosshair();
	bool OnDoAction(eLuxPlayerAction aAction, bool abPressed);
	virtual bool ShowOutlineOnEntity(iLuxEntity *apEntity, iPhysicsBody *apBody, const cVector3f &avFocusPos);
	virtual void ImplementedUpdate(float afTimeStep) {}
	virtual bool ImplementedDoAction(eLuxPlayerAction aAction, bool abPressed) { return true; }
	virtual void ImplementedOnEnterState(eLuxPlayerState aPrevState) {}
	virtual void ImplementedOnLeaveState(eLuxPlayerState aNewState) {}
	bool CanInteractWithEntity();
	void AddOutlineObjects(iPhysicsBody *apBody, iLuxEntity *apEntity, const cVector3f &avFocusPos);
	void GetAttachedBodies(iPhysicsBody *apBody, tPhysicsBodyList &alstBodies);
	cGuiGfxElement *A3Gfx(int32_t alCode);
	M_NOINIT(iPhysicsBody *, mpBodyInFocus)
	M_NOINIT(iLuxEntity *, mpEntityInFocus)
	M_NOINIT(float, mfFocusDistance)
	M_NOINIT(cVector3f, mvFocusPos)
	M_NOINIT(bool, mbCurrentEntityHasOutline)
	std::vector<cGuiGfxElement *> mvFocusCrosshairGfx;
};

class cLuxPlayerState_Normal : HOLD(OhplA3Plain) public iLuxPlayerState_DefaultBase
{
public:
	cLuxPlayerState_Normal(cLuxPlayer *p);
	bool ImplementedDoAction(eLuxPlayerAction aAction, bool abPressed);
};

class cLuxPlayerState_HandObject : HOLD(OhplA3Plain) public iLuxPlayerState_DefaultBase
{
public:
	cLuxPlayerState_HandObject(cLuxPlayer *p);
	bool ImplementedDoAction(eLuxPlayerAction aAction, bool abPressed);
	void ImplementedOnEnterState(eLuxPlayerState aPrevState);
	void ImplementedOnLeaveState(eLuxPlayerState aNewState);
};

class cLuxPlayerState_UseItem : HOLD(OhplA3UseItem) public iLuxPlayerState_DefaultBase
{
public:
	cLuxPlayerState_UseItem(cLuxPlayer *apPlayer);
	void OnDraw(cGuiSet *apGuiSet, float afFrameTime) {}
	bool ImplementedDoAction(eLuxPlayerAction aAction, bool abPressed);
	void ImplementedOnEnterState(eLuxPlayerState aPrevState);
	void ImplementedUpdate(float afTimeStep);
	cGuiGfxElement *GetCrosshair();
	bool OnDrawCrossHair(cGuiGfxElement *apGfx, const cVector3f &avPos, const cVector2f &avSize);
	bool ShowOutlineOnEntity(iLuxEntity *apEntity, iPhysicsBody *apBody, const cVector3f &avFocusPos);
	void UseItem();
#ifdef NEWSIDE
	cLuxInventory_Item *&mpCurrentItem = mR.item;
	cLinearOscillation &mFlashOscill = *reinterpret_cast<cLinearOscillation *>(mR.flash);
	float &mfMinUseItemDistance = mR.minUseDistance;
#else
	cLuxInventory_Item *mpCurrentItem;
	cLinearOscillation mFlashOscill;
	float mfMinUseItemDistance;
#endif
};
