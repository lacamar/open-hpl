#pragma once
// Fake game for the GA2a differential: real math, hand-copied member lists of the player classes,
// recording stand-ins for everything else. Function bodies come verbatim from orig.inc / new.inc.
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
#include "system/String.h"
#include "graphics/Color.h"

using hpl::cVector2f;
using hpl::cVector3f;
using hpl::cMatrixf;
using hpl::cMath;
using hpl::cColor;
using hpl::tString;
using hpl::tWString;
using hpl::tVector2fListIt;
using hpl::tVector3fListIt;
using hpl::eEulerRotationOrder_XYZ;
using hpl::eEulerRotationOrder_ZXY;
#define hplNew(cls, args) new cls args
#define hplDelete(p) delete p
#define _W(x) L##x

#ifdef NEWSIDE
#include "RustLuxPlayerA2.h"
#include "impl/RustBackends.h"
using hpl::OhplBackendRustRequested;
#endif

extern std::string gLog;
void Rec(const char *fmt, ...);
uint32_t Bits(float f);

enum eGuiMaterial { eGuiMaterial_Alpha };
enum eCharDir { eCharDir_Forward, eCharDir_Right, eCharDir_LastEnum };
enum eLuxDamageType { eLuxDamageType_BloodSplat, eLuxDamageType_Claws, eLuxDamageType_Slash, eLuxDamageType_LastEnum };
enum eLuxHeadPosAdd
{
	eLuxHeadPosAdd_Main, eLuxHeadPosAdd_Bob, eLuxHeadPosAdd_ClimbLedge, eLuxHeadPosAdd_Lean, eLuxHeadPosAdd_Death, eLuxHeadPosAdd_ScreenShake,
	eLuxHeadPosAdd_Script, eLuxHeadPosAdd_Hurt, eLuxHeadPosAdd_InsanityCollapse, eLuxHeadPosAdd_LastEnum
};
enum eLuxPlayerState { eLuxPlayerState_Normal, eLuxPlayerState_Other };
enum eLuxMoveState { eLuxMoveState_Normal, eLuxMoveState_Other };
enum eLuxPlayerAction { eLuxPlayerAction_Lantern, eLuxPlayerAction_Other };
enum eSoundEntryType { eSoundEntryType_World, eSoundEntryType_Gui };
enum eSoundEntityType { eSoundEntityType_Main };
enum eLuxGlobalVolumeType { eLuxGlobalVolumeType_InsanityCollapse, eLuxGlobalVolumeType_Flashback, eLuxGlobalVolumeType_Death };
enum eLuxItemType { eLuxItemType_Lantern };
enum eLuxEntityType { eLuxEntityType_Prop, eLuxEntityType_Enemy };
enum eLuxPropType { eLuxPropType_Critter, eLuxPropType_Other };
enum eCollision { eCollision_Outside, eCollision_Inside };
enum eLuxHandsState { eLuxHandsState_Disabled, eLuxHandsState_Holster, eLuxHandsState_Draw, eLuxHandsState_Idle, eLuxHandsState_HandObject };
enum eLuxUpdateableMessage { eUpdateableMessage_Reset };

inline tWString kTranslate(const char *, const char *) { return L"t"; }
inline void Log(const char *, ...) {}
inline void Error(const char *, ...) {}

struct cSoundChannel { void Stop() { Rec("ChannelStop"); } };
struct cSoundEntry
{
	int mlId;
	void SetVolumeMul(float f) { Rec("SndVolMul %08x", Bits(f)); }
	int GetId() { return mlId; }
	cSoundChannel *GetChannel() { static cSoundChannel c; return &c; }
	void Stop() { Rec("SndStop"); }
	void FadeIn(float a, float b) { Rec("SndFadeIn %08x %08x", Bits(a), Bits(b)); }
	void FadeOut(float a) { Rec("SndFadeOut %08x", Bits(a)); }
};
struct cSoundHandler
{
	bool mbPlayOk, mbValid;
	cSoundEntry mEntry;
	cSoundEntry *PlayGui(const tString &s, bool loop, float vol) { rand(); Rec("PlayGui %s %d %08x", s.c_str(), loop, Bits(vol)); return mbPlayOk ? &mEntry : NULL; }
	bool IsValid(cSoundEntry *, int) { return mbValid; }
	void FadeGlobalVolume(float a, float b, eSoundEntryType t, eLuxGlobalVolumeType t2, bool c) { Rec("FadeVol %08x %08x %d %d %d", Bits(a), Bits(b), (int)t, (int)t2, c); }
	void FadeGlobalSpeed(float a, float b, eSoundEntryType t, eLuxGlobalVolumeType t2, bool c) { Rec("FadeSpeed %08x %08x %d %d %d", Bits(a), Bits(b), (int)t, (int)t2, c); }
};
struct cSound { cSoundHandler mHandler; cSoundHandler *GetSoundHandler() { return &mHandler; } };
struct cEngine { cSound mSound; cSound *GetSound() { return &mSound; } void Exit() { Rec("EngineExit"); } };

struct cBoundingVolume {};
struct cFrustum { eCollision CollideBoundingVolume(cBoundingVolume *); };
struct iPhysicsBody
{
	cVector3f mvLocal;
	cBoundingVolume mBV;
	cVector3f GetLocalPosition() { return mvLocal; }
	cBoundingVolume *GetBoundingVolume() { return &mBV; }
};
class iCharacterBody
{
public:
	cVector3f mvPos, mvSize, mvForward, mvVel;
	iPhysicsBody mBody;
	cVector3f GetPosition() { return mvPos; }
	cVector3f GetFeetPosition() { return mvPos; }
	cVector3f GetSize() { return mvSize; }
	cVector3f GetForward() { return mvForward; }
	cVector3f GetVelocity(float) { return mvVel; }
	iPhysicsBody *GetCurrentBody() { return &mBody; }
	void SetCameraPosAdd(const cVector3f &v) { Rec("CamPosAdd %08x %08x %08x", Bits(v.x), Bits(v.y), Bits(v.z)); }
	void SetYaw(float f) { Rec("BodyYaw %08x", Bits(f)); }
};

class cCamera
{
public:
	float mfPitch, mfYaw, mfRoll, mfPitchLimitMin, mfPitchLimitMax;
	cVector3f mvPos, mvRight, mvUp, mvForward;
	cFrustum mFrustum;
	float GetPitch() { return mfPitch; }
	float GetYaw() { return mfYaw; }
	float GetRoll() { return mfRoll; }
	float GetPitchMinLimit() { return mfPitchLimitMin; }
	float GetPitchMaxLimit() { return mfPitchLimitMax; }
	cVector3f GetPosition() { return mvPos; }
	cVector3f GetRight() { return mvRight; }
	cVector3f GetUp() { return mvUp; }
	cVector3f GetForward() { return mvForward; }
	cFrustum *GetFrustum() { return &mFrustum; }
	void SetPitch(float f)
	{
		Rec("SetPitch %08x", Bits(f));
		mfPitch = f;
		if(mfPitchLimitMin != 0 || mfPitchLimitMax != 0)
		{
			if(mfPitch > mfPitchLimitMax) mfPitch = mfPitchLimitMax;
			if(mfPitch < mfPitchLimitMin) mfPitch = mfPitchLimitMin;
		}
	}
	void SetYaw(float f) { mfYaw = f; }
	void AddPitch(float f) { Rec("AddPitch %08x", Bits(f)); mfPitch += f; SetPitchQuiet(); }
	void AddYaw(float f) { Rec("AddYaw %08x", Bits(f)); mfYaw += f; }
	void SetPitchQuiet()
	{
		if(mfPitchLimitMin != 0 || mfPitchLimitMax != 0)
		{
			if(mfPitch > mfPitchLimitMax) mfPitch = mfPitchLimitMax;
			if(mfPitch < mfPitchLimitMin) mfPitch = mfPitchLimitMin;
		}
	}
	void SetRoll(float f) { Rec("SetRoll %08x", Bits(f)); mfRoll = f; }
	void SetFOV(float f) { Rec("SetFOV %08x", Bits(f)); }
	void SetAspect(float f) { Rec("SetAspect %08x", Bits(f)); }
};

struct iLight { };
struct cLightPoint : public iLight
{
	float mfRadius;
	void SetDiffuseColor(const cColor &c) { Rec("LightColor %08x %08x %08x %08x", Bits(c.r), Bits(c.g), Bits(c.b), Bits(c.a)); }
	void SetPosition(const cVector3f &v) { Rec("LightPos %08x %08x %08x", Bits(v.x), Bits(v.y), Bits(v.z)); }
	void FadeTo(const cColor &c, float r, float t) { Rec("LightFade %08x %08x %08x %08x %08x %08x", Bits(c.r), Bits(c.g), Bits(c.b), Bits(c.a), Bits(r), Bits(t)); }
	float GetRadius() { return mfRadius; }
	void SetMatrix(const cMatrixf &m) { uint32_t h = 0; for(int i=0;i<16;++i) h = h*31u + Bits(m.v[i]); Rec("LightMtx %08x", h); }
};

struct cLuxPlayer;
struct iLuxPlayerHelper
{
	cLuxPlayer *mpPlayer;
	iLuxPlayerHelper(cLuxPlayer *p) : mpPlayer(p) {}
};

struct cLuxSoundExtraData { cSoundEntry *mpSoundEntry; };
struct cLuxHelpFuncs
{
	bool mbExtraOk;
	cSoundEntry mEntry;
	bool PlayGuiSoundData(const tString &s, eSoundEntryType t, float vol = 1, eSoundEntityType e = eSoundEntityType_Main, bool b = false, cLuxSoundExtraData *x = NULL)
	{
		rand();
		Rec("PlayGuiSound %s %d %08x %d", s.c_str(), (int)t, Bits(vol), b);
		if(x) x->mpSoundEntry = &mEntry;
		return mbExtraOk;
	}
};
struct cLuxHintHandler { void Add(const tString &s, const tWString &, int) { Rec("Hint %s", s.c_str()); } };
struct cLuxMessageHandler { bool mbPause; bool IsPauseMessageActive() { return mbPause; } };
struct cFlashFx { void Start(float a, float b, float c) { Rec("FxFlash %08x %08x %08x", Bits(a), Bits(b), Bits(c)); } };
struct cSimpleFx
{
	void FadeTo(float a, float b) { Rec("FxFadeTo %08x %08x", Bits(a), Bits(b)); }
	void SetBlurStartDist(float a) { Rec("FxBlurStart %08x", Bits(a)); }
};
struct cSanityGainFlashFx { void Start() { Rec("SanityGainFlash"); } };
struct cPlayVoiceFx
{
	bool mbDone;
	bool VoiceDonePlaying() { return mbDone; }
	void AddVoice(const tString &, const tString &, const tString &, const tString &, bool, int, int, int) { Rec("AddVoice"); }
};
struct cEmotionFlashFx { bool mbOn; bool IsActive() { return mbOn; } };
struct cLuxEffectHandler
{
	cFlashFx mFlash;
	cSimpleFx mTrail, mBlur, mSepia;
	cSanityGainFlashFx mGain;
	cPlayVoiceFx mVoice;
	cEmotionFlashFx mEmotion;
	cFlashFx *GetFlash() { return &mFlash; }
	cSimpleFx *GetImageTrail() { return &mTrail; }
	cSimpleFx *GetRadialBlur() { return &mBlur; }
	cSimpleFx *GetSepiaColor() { return &mSepia; }
	cSanityGainFlashFx *GetSanityGainFlash() { return &mGain; }
	cPlayVoiceFx *GetPlayVoice() { return &mVoice; }
	cEmotionFlashFx *GetEmotionFlash() { return &mEmotion; }
};
struct cInsanityFx
{
	void SetActive(bool b) { Rec("InsActive %d", b); }
	void SetWaveAlpha(float f) { Rec("InsWaveAlpha %08x", Bits(f)); }
	void SetWaveSpeed(float f) { Rec("InsWaveSpeed %08x", Bits(f)); }
	void SetZoomAlpha(float f) { Rec("InsZoomAlpha %08x", Bits(f)); }
};
struct cLuxPostEffectHandler { cInsanityFx mIns; cInsanityFx *GetInsanity() { return &mIns; } };
struct cLuxInventory { bool mbHas; bool HasItemOfType(eLuxItemType) { return mbHas; } };
struct cLuxSaveHandler { bool mbOk; bool AutoLoad(bool b) { Rec("AutoLoad %d", b); return mbOk; } };

struct iLuxEntity
{
	bool mbActive;
	eLuxEntityType mType;
	bool IsActive() { return mbActive; }
	eLuxEntityType GetEntityType() { return mType; }
};
struct iLuxProp : public iLuxEntity
{
	eLuxPropType mPropType;
	iPhysicsBody mBody;
	eLuxPropType GetPropType() { return mPropType; }
	iPhysicsBody *GetBody(int) { return &mBody; }
};
struct iLuxProp_CritterBase : public iLuxProp
{
	bool mbCausesSanity;
	bool CausesSanityDecrease() { return mbCausesSanity; }
};
struct iLuxEnemy : public iLuxEntity
{
	bool mbCausesSanity;
	iCharacterBody mBody;
	int mlId;
	bool CausesSanityDecrease() { return mbCausesSanity; }
	iCharacterBody *GetCharacterBody() { return &mBody; }
	void SetIsSeenByPlayer(bool b) { Rec("EnemySeen %d %d", mlId, b); }
	void AlertOfPlayerPresence() { Rec("EnemyAlert %d", mlId); }
};
struct cLuxEntityIterator
{
	std::vector<iLuxEntity *> *mv;
	size_t mi;
	bool HasNext() { return mi < mv->size(); }
	iLuxEntity *Next() { return (*mv)[mi++]; }
};
struct cLuxEnemyIterator
{
	std::vector<iLuxEnemy *> *mv;
	size_t mi;
	bool HasNext() { return mi < mv->size(); }
	iLuxEnemy *Next() { return (*mv)[mi++]; }
};
struct cLuxMap
{
	std::vector<iLuxEntity *> mEntities;
	std::vector<iLuxEnemy *> mEnemies;
	tString msLantern;
	cLuxEntityIterator GetEntityIterator() { cLuxEntityIterator i = {&mEntities, 0}; return i; }
	cLuxEnemyIterator GetEnemyIterator() { cLuxEnemyIterator i = {&mEnemies, 0}; return i; }
	void RunScript(const tString &s) { Rec("RunScript %s", s.c_str()); }
	const tString &GetLanternLitCallback() { return msLantern; }
};
struct cLuxMapHandler { cLuxMap mMap; cLuxMap *GetCurrentMap() { return &mMap; } };
struct cLuxMapHelper
{
	float mfLevel[16];
	int mlCalls;
	bool mbLos[64];
	float GetLightLevelAtPos(const cVector3f &p, std::vector<iLight *> *, float add)
	{
		float f = std::fabs(p.x) * 0.01f + std::fabs(p.y) * 0.003f + add * 0.05f + mfLevel[mlCalls++ & 15];
		Rec("LightAt %08x %08x %08x %08x -> %08x", Bits(p.x), Bits(p.y), Bits(p.z), Bits(add), Bits(f));
		return f;
	}
	bool CheckLineOfSight(const cVector3f &a, const cVector3f &b, bool) { bool r = mbLos[mlCalls++ & 63]; Rec("Los %d", r); return r; }
};
struct cLuxMoveStateNormal { bool mbCrouch; };

struct cLuxStateData { bool mbLantern; bool AllowLantern() { return mbLantern; } };
struct cAnimationState {};
struct cMeshEntity
{
	int mlAnims;
	bool mbObjActive;
	void SetVisible(bool b) { Rec("HandsVisible %d", b); }
	void SetMatrix(const cMatrixf &m) { Rec("HandsMtx %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x", Bits(m.v[0]),Bits(m.v[1]),Bits(m.v[2]),Bits(m.v[3]),Bits(m.v[4]),Bits(m.v[5]),Bits(m.v[6]),Bits(m.v[7]),Bits(m.v[8]),Bits(m.v[9]),Bits(m.v[10]),Bits(m.v[11]),Bits(m.v[12]),Bits(m.v[13]),Bits(m.v[14]),Bits(m.v[15])); }
	int GetAnimationStateNum() { return mlAnims; }
	cAnimationState *GetAnimationState(int) { return NULL; }
	bool IsActive() { return mbObjActive; }
};
struct cLuxHandObject
{
	bool mbAnimOver, mbDoAction, mbMesh;
	cMeshEntity mMesh;
	bool AnimationIsOver() { return mbAnimOver; }
	bool DoAction(eLuxPlayerAction a, bool b) { Rec("HandObjDoAction %d %d", (int)a, b); return mbDoAction; }
	tString GetAnimDraw() { return "draw"; }
	tString GetAnimIdle() { return "idle"; }
	tString GetAnimHolster() { return "holster"; }
	void Update(float f) { Rec("HandObjUpdate %08x", Bits(f)); }
	cMeshEntity *GetMeshEntity() { return mbMesh ? &mMesh : NULL; }
};
typedef cLuxHandObject iLuxHandObject;

struct cLuxPlayerHudEffect
{
	void AddDamageSplash(eLuxDamageType t) { rand(); Rec("Splash %d", (int)t); }
	void Flash(const cColor &c, eGuiMaterial m, float a, float b) { Rec("HudFlash %08x %08x %08x %08x %d %08x %08x", Bits(c.r), Bits(c.g), Bits(c.b), Bits(c.a), (int)m, Bits(a), Bits(b)); }
};
struct cLuxPlayerDeath;
struct cLuxPlayerInsanityCollapse;
struct cLuxPlayerSanity;
struct cLuxPlayerLantern;
struct cLuxPlayerLightLevel;
struct cLuxPlayerInDarkness;
struct cLuxPlayerFlashback;
struct cLuxPlayerHands;

struct cLuxHeadPosAdd
{
	cVector3f mvAdd;
	bool mbMoving;
	cVector3f mvMoveGoal;
	float mfMoveSpeed;
	float mfMoveSlowDownDist;
	cLuxHeadPosAdd() : mvAdd(0), mbMoving(false) {}
};

struct cLuxPlayer
{
	cCamera *mpCamera;
	iCharacterBody *mpCharBody;
	bool mbFreeCameraActive;
	OhplA2Core mCore;
	float &mfHealth = mCore.mfHealth, &mfSanity = mCore.mfSanity, &mfLampOil = mCore.mfLampOil, &mfTerror = mCore.mfTerror;
	bool mbPressedMove;
	bool mbJumpDisabled, mbCrouchDisabled, mbSanityDrainDisabled, mbUsePermaDeath, mbActive;
	float mfEventMoveSpeedMul, mfEventRunSpeedMul, mfHurtMoveSpeedMul, mfInsanityCollapseSpeedMul;
	cVector2f &mvHeadSpinSpeed = mCore.mvHeadSpinSpeed;
	cVector3f mvHeadPosAddSum;
	std::set<int> m_setTerrorEnemies;
	cSoundEntry *mpTerrorSound;
	int mlTerrorSoundID;
	float &mfAspectMul = mCore.cam.mfAspectMul, &mfFOVMul = mCore.cam.mfFOVMul, &mfAspectMulGoal = mCore.cam.mfAspectMulGoal, &mfFOVMulGoal = mCore.cam.mfFOVMulGoal, &mfAspectMulSpeed = mCore.cam.mfAspectMulSpeed, &mfFOVMulSpeed = mCore.cam.mfFOVMulSpeed;
	float &mfRoll = mCore.cam.mfRoll, &mfRollGoal = mCore.cam.mfRollGoal, &mfRollSpeedMul = mCore.cam.mfRollSpeedMul, &mfRollMaxSpeed = mCore.cam.mfRollMaxSpeed;
	uint32_t &mbFadingPitch = mCore.cam.mbFadingPitch;
	float &mfPitchGoal = mCore.cam.mfPitchGoal, &mfPitchSpeedMul = mCore.cam.mfPitchSpeedMul, &mfPitchMaxSpeed = mCore.cam.mfPitchMaxSpeed;
	float &mfLeanRoll = mCore.cam.mfLeanRoll, &mfLeanRollGoal = mCore.cam.mfLeanRollGoal, &mfLeanRollSpeedMul = mCore.cam.mfLeanRollSpeedMul, &mfLeanRollMaxSpeed = mCore.cam.mfLeanRollMaxSpeed;
	cVector3f &mvCamAnimPos = mCore.cam.mvCamAnimPos, &mvCamAnimPosGoal = mCore.cam.mvCamAnimPosGoal;
	float &mfCamAnimPosSpeedMul = mCore.cam.mfCamAnimPosSpeedMul, &mfCamAnimPosMaxSpeed = mCore.cam.mfCamAnimPosMaxSpeed;
	tWString msFocusText, msLastFocusText;
	float &mfFocusTextAlpha = mCore.mfFocusTextAlpha;
	std::list<float> mlstPrevSpeeds;
	int mlMaxPrevSpeeds;
	float mfAvgSpeed;
	cVector3f mvCameraPosAdd;
	float &mfHeadSpinDamageSpeed = mCore.mfHeadSpinDamageSpeed, &mfHeadSpinDeacc = mCore.mfHeadSpinDeacc, &mfAspect = mCore.cam.mfAspect, &mfFOV = mCore.cam.mfFOV, &mfTerrorIncSpeed = mCore.mfTerrorIncSpeed, &mfTerrorDecSpeed = mCore.mfTerrorDecSpeed;
	tString msTerrorSound;
	std::vector<cLuxHeadPosAdd> mvHeadPosAdds;
	cLuxPlayerHudEffect *mpHudEffect;
	cLuxPlayerDeath *mpDeath;
	cLuxPlayerInsanityCollapse *mpInsanityCollapse;
	cLuxPlayerSanity *mpSanity;
	cLuxPlayerLantern *mpLantern;
	cLuxPlayerLightLevel *mpHelperLightLevel;
	cLuxPlayerInDarkness *mpHelperInDarkness;
	cLuxPlayerFlashback *mpFlashback;
	cLuxPlayerHands *mpHands;
	cLuxStateData mState;
	cLuxMoveStateNormal mMove;

	void GiveDamage(float afAmount, int alStrength, eLuxDamageType aType, bool abSpinHead, bool abLethal);
	void LowerSanity(float afAmount, bool abUseEffect);
	void SetHealth(float afX);
	void SetSanity(float afX);
	void AddHealth(float afX);
	void AddSanity(float afX, bool abShowEffect = true);
	void AddLampOil(float afX);
	void FadePitchTo(float afX, float afSpeedMul, float afMaxSpeed);
	void FadeFOVMulTo(float afX, float afSpeed);
	void FadeAspectMulTo(float afX, float afSpeed);
	void FadeRollTo(float afX, float afSpeedMul, float afMaxSpeed);
	void FadeLeanRollTo(float afX, float afSpeedMul, float afMaxSpeed);
	void SetRoll(float afX);
	void UpdateHeadPosAdd(float afTimeStep);
	void UpdateCamera(float afTimeStep);
	void UpdateTerror(float afTimeStep);
	void UpdateTerrorSound();
	static void A2HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD);
	static float A2HostQuery(void *apCtx, uint32_t alKind);
	void SpinHead(float afSpeed);
	void UpdateHeadSpin(float afTimeStep);
	void UpdateFocusText(float afTimeStep);
	void UpdateAvgSpeed(float afTimeStep);
#ifdef NEWSIDE
	void RunVitals(int alWhich, float afAmount, int alType, unsigned int alFlags);
#endif

	float GetHealth() { return mfHealth; }
	float GetSanity() { return mfSanity; }
	float GetLampOil() { return mfLampOil; }
	void SetLampOil(float f) { Rec("SetLampOil %08x", Bits(f)); mfLampOil = f; }
	bool IsDead() { return mfHealth <= 0; }
	bool IsActive() { return mbActive; }
	bool UsePermaDeath() { return mbUsePermaDeath; }
	const tString &GetCurrentPermaDeathSound() { static tString s = "perma"; return s; }
	cCamera *GetCamera() { return mpCamera; }
	iCharacterBody *GetCharacterBody() { return mpCharBody; }
	const cVector3f &GetHeadPosAdd(eLuxHeadPosAdd t) { return mvHeadPosAdds[t].mvAdd; }
	void SetHeadPosAdd(eLuxHeadPosAdd t, const cVector3f &v) { Rec("HeadPosAdd %d %08x %08x %08x", (int)t, Bits(v.x), Bits(v.y), Bits(v.z)); mvHeadPosAdds[t].mvAdd = v; }
	void SetInsanityCollapseSpeedMul(float f) { Rec("CollapseSpeedMul %08x", Bits(f)); mfInsanityCollapseSpeedMul = f; }
	void SetCrouchDisabled(bool b) { Rec("CrouchDisabled %d", b); mbCrouchDisabled = b; }
	void SetJumpDisabled(bool b) { Rec("JumpDisabled %d", b); mbJumpDisabled = b; }
	void SetHurtMoveSpeedMul(float f) { Rec("HurtMoveSpeedMul %08x", Bits(f)); mfHurtMoveSpeedMul = f; }
	void SetEventMoveSpeedMul(float f) { Rec("EventMove %08x", Bits(f)); mfEventMoveSpeedMul = f; }
	void SetEventRunSpeedMul(float f) { Rec("EventRun %08x", Bits(f)); mfEventRunSpeedMul = f; }
	bool GetSanityDrainDisabled() { return mbSanityDrainDisabled; }
	void SetCurrentHandObjectDrawn(bool b) { Rec("HandDrawn %d", b); }
	void ChangeState(eLuxPlayerState s) { Rec("ChangeState %d", (int)s); }
	void ChangeMoveState(eLuxMoveState s) { Rec("ChangeMoveState %d", (int)s); }
	cLuxStateData *GetCurrentStateData() { return &mState; }
	cLuxPlayerHands *GetHands() { return mpHands; }
	cLuxPlayerLantern *GetHelperLantern() { return mpLantern; }
	cLuxPlayerInDarkness *GetHelperInDarkness() { return mpHelperInDarkness; }
	cLuxPlayerLightLevel *GetHelperLightLevel() { return mpHelperLightLevel; }
	cLuxPlayerFlashback *GetHelperFlashback() { return mpFlashback; }
	cLuxPlayerSanity *GetHelperSanity() { return mpSanity; }
	cLuxPlayerInsanityCollapse *GetInsanityCollapse() { return mpInsanityCollapse; }
};

struct cLuxBase
{
	bool mbHardMode;
	cEngine *mpEngine;
	cLuxPlayer *mpPlayer;
	cLuxHelpFuncs *mpHelpFuncs;
	cLuxHintHandler *mpHintHandler;
	cLuxMessageHandler *mpMessageHandler;
	cLuxEffectHandler *mpEffectHandler;
	cLuxPostEffectHandler *mpPostEffectHandler;
	cLuxInventory *mpInventory;
	cLuxSaveHandler *mpSaveHandler;
	cLuxMapHandler *mpMapHandler;
	cLuxMapHelper *mpMapHelper;
};
extern cLuxBase *gpBase;

struct cLuxPlayerInsanityCollapse : public iLuxPlayerHelper
{
	bool mbActive;
	int mlState;
	float mfHeightAddCollapseSpeed, mfHeightAddAwakeSpeed, mfRollCollapseSpeed, mfRollAwakeSpeed, mfHeightAddGoal, mfAwakenSanity, mfSleepTime,
		mfSleepSpeedMul, mfWakeUpSpeedMul;
	tString msStartSound, msAwakenSound, msSleepLoopSound;
	float mfSleepLoopSoundVolume;
	tString msSleepRandomSound;
	float mfSleepRandomMinTime, mfSleepRandomMaxTime, mfHeightAdd, mfRoll, mfT, mfRandomCount;
	cSoundEntry *mpLoopSound;
	int mlLoopSoundID;
	cLuxPlayerInsanityCollapse(cLuxPlayer *p) : iLuxPlayerHelper(p) {}
	void Start() { Rec("CollapseStart"); }
	void Update(float afTimeStep);
};

struct cLuxPlayerCamDirEffects : public iLuxPlayerHelper
{
	float mfStartSwayMaxSanity;
	bool mbSwayActive;
	std::list<cVector2f> mlstPrevAdd;
	cVector2f mvSwayAdd;
	int mlMaxPositions;
	float mfSwayAlpha;
	cVector2f mvNextAdd;
	cLuxPlayerCamDirEffects(cLuxPlayer *p) : iLuxPlayerHelper(p) {}
	void Update(float afTimeStep);
	float AddAndGetYawAdd(float afX);
	float AddAndGetPitchAdd(float afX);
#ifndef NEWSIDE
	void SetSwayActive(bool abX);
	void UpdateSway(float afTimeStep);
#endif
};

struct cLuxPlayerHurt : public iLuxPlayerHelper
{
	std::vector<int> mvNoiseGfx;
	float mfEffectStartHealth, mfMinMoveMul, mfMaxPantCount, mfPantSpeed, mfPantSize;
	int mlCurrentNoise;
	float mfNoiseUpdateCount, mfNoiseAlpha, mfNoiseFreq;
	float mfPantCount, mfPantPosAdd, mfPantPosAddVel, mfPantPosAddDir, mfAlpha, mfHealthRegainSpeed, mfHealthRegainLimit;
	cLuxPlayerHurt(cLuxPlayer *p) : iLuxPlayerHelper(p) {}
	void Update(float afTimeStep);
};

struct cLuxFlashbackData
{
	tString msFile, msCallback;
};
struct cLuxPlayerFlashback : public iLuxPlayerHelper
{
	float mfRadialBlurSize, mfRadialBlurStartDist, mfWorldSoundVolume, mfMoveSpeedMul, mfRunSpeedMul;
	float mfFlashDelay;
	bool mbActive;
	float mfFlashbackStartCount;
	tString msFlashbackFile, msCallback;
	std::list<cLuxFlashbackData> mlstFlashbackQueue;
	cLuxPlayerFlashback(cLuxPlayer *p) : iLuxPlayerHelper(p) {}
	void Update(float afTimeStep);
	void Start(const tString &f, const tString &c);
	void LoadAndPlayFlashbackFile(const tString &f) { Rec("LoadFlashFile %s", f.c_str()); }
	bool IsActive() { return mbActive; }
};

struct cLuxPlayerLookAt : public iLuxPlayerHelper
{
	bool mbActive;
	cVector3f mvTargetPos, mvCurrentSpeed;
	float mfSpeedMul, mfMaxSpeed;
	tString msAtTargetCallback;
	cLuxPlayerLookAt(cLuxPlayer *p) : iLuxPlayerHelper(p) {}
	void Update(float afTimeStep);
};

struct cLuxPlayerSanity : public iLuxPlayerHelper
{
	float mfHitAlpha;
	bool mbHitActive;
	float mfSanityLostCount, mfAtLowSanityCount;
	bool mbHitIsUpdated, mbSanityLostIsUpdated;
	float mfPantCount, mfCheckEnemySeenCount;
	bool mbSanityEffectUpdated;
	float mfT, mfInsaneWaveAlpha, mfSanityDrainCount, mfSanityDrainVolume, mfSanityHeartbeatCount, mfSeenEnemyCount;
	bool mbEnemyIsSeen;
	float mfShowHintTimer;
	float mfHitZoomInSpeed, mfHitZoomOutSpeed, mfHitZoomInFOVMul, mfHitZoomInAspectMul, mfSanityRegainSpeed, mfSanityRegainLimit,
		mfSanityVeryLowLimit, mfSanityEffectsStart, mfSanityWaveAlphaMul, mfSanityWaveSpeedMul, mfSanityLowLimit, mfSanityLowLimitMaxTime,
		mfSanityLowNewSanityAmount, mfCheckNearEnemyInterval, mfNearEnemyDecrease, mfNearCritterDecrease;
	cLuxPlayerSanity(cLuxPlayer *p) : iLuxPlayerHelper(p) {}
	void StartHit() { Rec("SanityHitStart"); }
	void SetSanityLost() { Rec("SanityLost"); mfSanityLostCount = 1.0f; }
	void Update(float afTimeStep);
	void UpdateInsaneEffects(float afTimeStep);
	void UpdateCheckEnemySeen(float afTimeStep);
	void UpdateHit(float afTimeStep);
	void UpdateEnemySeenEffect(float afTimeStep);
#ifndef NEWSIDE
	void UpdateInsanityVisuals(float afTimeStep);
	void UpdateLosingSanity(float afTimeStep);
	void UpdateLowSanity(float afTimeStep);
#else
	void ToRust(OhplGa2Sanity &s);
	void FromRust(const OhplGa2Sanity &s);
	void Replay(const OhplGa2Ops &ops);
#endif
};

struct cLuxPlayerLantern : public iLuxPlayerHelper
{
	cColor mDefaultColor;
	tString msTurnOnSound, msTurnOffSound, msOutOfOilSound;
	cVector3f mvLocalOffset;
	float mfLowerOilSpeed, mfFadeLightOilAmount;
	bool mbDisabled, mbActive;
	float mfAlpha;
	cLightPoint *mpLight;
	cLuxPlayerLantern(cLuxPlayer *p) : iLuxPlayerHelper(p) {}
	void Update(float afTimeStep);
	void SetActive(bool abX, bool abUseEffects, bool abCheckForOilAndItems = true, bool abCheckIfAllowed = true);
	bool IsActive() { return mbActive; }
};

struct cLuxPlayerLightLevel : public iLuxPlayerHelper
{
	float mfRadiusAdd, mfExtendedLightLevel, mfNormalLightLevel, mfUpdateCount;
	cLuxPlayerLightLevel(cLuxPlayer *p) : iLuxPlayerHelper(p) {}
	void Update(float afTimeStep);
	float GetExtendedLightLevel() { return mfExtendedLightLevel; }
	float GetNormalLightLevel() { return mfNormalLightLevel; }
};

struct cLuxPlayerInDarkness : public iLuxPlayerHelper
{
	bool mbActive;
	cLightPoint *mpAmbientLight;
	bool mbAmbientLightIsOn, mbInDarkness;
	float mfMinDarknessLightLevel, mfAmbientLightMinLightLevel, mfAmbientLightRadius, mfAmbientLightIntensity, mfAmbientLightFadeInTime,
		mfAmbientLightFadeOutTime;
	cColor mAmbientLightColor;
	float mfLoopSoundCount, mfLoopSoundStartupTime, mfSanityLossPerSecond, mfSanityLossMul, mfShowHintTimer;
	cLuxPlayerInDarkness(cLuxPlayer *p) : iLuxPlayerHelper(p) {}
	void Update(float afTimeStep);
	iLight *GetAmbientLight() { return mpAmbientLight; }
};

struct cLinearOscillation
{
	float mfT;
	void Update(float f) { Rec("OscUpdate %08x", Bits(f)); }
};
struct cLuxPlayerDeath : public iLuxPlayerHelper
{
	bool mbToMainMenu, mbActive, mbShowHint;
	int mlState;
	tString msStartSound, msAwakenSound;
	bool mbSkipStartSound;
	float mfHeightAddSpeed, mfRollSpeed, mfHeightAddGoal, mfHeightAddGoalCrouch, mfFadeOutTime, mfHeightAdd, mfRoll, mfT, mfMinHeightAdd;
	tString msHintCat, msHintEntry;
	float mfFadeAlpha, mfTextAlpha1, mfTextAlpha2, mfTextOnScreenCount, mfWhiteCount;
	cLinearOscillation mFlashOscill;
	cSoundEntry *mpVoiceEntry;
	int mlVoiceEntryId;
	cLuxPlayerDeath(cLuxPlayer *p) : iLuxPlayerHelper(p) {}
	void Start() { Rec("DeathStart"); }
	void Update(float afTimeStep);
	void ResetGame() { Rec("DeathResetGame"); }
	void Reset() { Rec("DeathReset"); mlState = 0; mbActive = false; mfFadeAlpha = 0; mfTextAlpha1 = 0; }
#ifdef NEWSIDE
	void ToRust(OhplGa2Death &s);
	void FromRust(const OhplGa2Death &s);
	void Replay(const OhplGa2Ops &ops);
#endif
};

struct cLuxPlayerHands : public iLuxPlayerHelper
{
	iLuxHandObject *mpCurrentHandObject;
	cMeshEntity *mpHandsEntity;
	eLuxHandsState mHandState;
	tString msCurrentAnim;
	std::list<cVector3f> mlstCamRotations;
	int mlMaxCamRotations;
	float mfPosAddMul;
	std::vector<iLuxHandObject *> mvHandObjects;
	float mfHandObjectChargeCount;
	int mlHandObjectState;
	bool mbHandObjectAttackDown, mbHandObjectInteractDown;
	bool mbAnimOver;
	cLuxPlayerHands(cLuxPlayer *p) : iLuxPlayerHelper(p) {}
	void Update(float afTimeStep);
	void DoAction(eLuxPlayerAction aAction, bool abPressed);
	void SetCurrentHandObject(iLuxHandObject *apObject);
	void UpdatePlayerHandsPos(float afTimeStep);
	void ResetHandObjectVars();
	void SetActiveHandObject(const tString &s) { Rec("SetActiveHandObject %s", s.c_str()); }
	void PlayAnim(const tString &s, bool loop) { Rec("PlayAnim %s %d", s.c_str(), loop); msCurrentAnim = s; }
	bool AnimOver() { return mbAnimOver; }
	void CreateAndAttachHandObject(cLuxMap *, iLuxHandObject *) { Rec("CreateAndAttach"); }
	void HideAllHandObjects() { Rec("HideAll"); }
};

// golden recorder (A2): wraps the Rust drivers, logs Core before/after plus the host traffic
extern FILE *gRecFile;
extern unsigned gRecCount;
struct RecHost { const OhplA2Host *real; std::vector<uint32_t> ev; };
static void RecOp(void *c, uint32_t k, int32_t i, float a, float b, float cc, float d)
{
	RecHost *r = (RecHost *)c;
	uint32_t w[7] = {1, k, (uint32_t)i};
	memcpy(w + 3, &a, 4); memcpy(w + 4, &b, 4); memcpy(w + 5, &cc, 4); memcpy(w + 6, &d, 4);
	r->ev.insert(r->ev.end(), w, w + 7);
	r->real->op(r->real->ctx, k, i, a, b, cc, d);
}
static float RecQ(void *c, uint32_t k)
{
	RecHost *r = (RecHost *)c;
	float v = r->real->query(r->real->ctx, k);
	uint32_t u;
	memcpy(&u, &v, 4);
	r->ev.push_back(0); r->ev.push_back(k); r->ev.push_back(u);
	return v;
}
template<class F> static void RecCall(uint32_t id, OhplA2Core *c, const OhplA2Host *h, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, F f)
{
	static unsigned n; if(!gRecFile || gRecCount >= 3000 || n++ % 30) { f(h); return; }
	OhplA2Core before = *c;
	RecHost r = {h, {}};
	OhplA2Host h2 = {&r, RecOp, RecQ};
	f(&h2);
	uint32_t hd[6] = {id, a0, a1, a2, a3, (uint32_t)r.ev.size()};
	fwrite(hd, 4, 6, gRecFile);
	fwrite(&before, sizeof(before), 1, gRecFile);
	fwrite(r.ev.data(), 4, r.ev.size(), gRecFile);
	fwrite(c, sizeof(*c), 1, gRecFile);
	++gRecCount;
}
static inline uint32_t FB(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static void rec_vitals(OhplA2Core *c, const OhplA2Host *h, int32_t w, float a, int32_t t, uint32_t f)
{ RecCall(0, c, h, w, FB(a), t, f, [&](const OhplA2Host *x) { ohpl_a2_vitals(c, x, w, a, t, f); }); }
static void rec_spin(OhplA2Core *c, const OhplA2Host *h, float s)
{ RecCall(1, c, h, FB(s), 0, 0, 0, [&](const OhplA2Host *x) { ohpl_a2_spin_head(c, x, s); }); }
static void rec_cam(OhplA2Core *c, const OhplA2Host *h, float s)
{ RecCall(2, c, h, FB(s), 0, 0, 0, [&](const OhplA2Host *x) { ohpl_a2_update_camera(c, x, s); }); }
static void rec_hspin(OhplA2Core *c, const OhplA2Host *h, float s)
{ RecCall(3, c, h, FB(s), 0, 0, 0, [&](const OhplA2Host *x) { ohpl_a2_update_head_spin(c, x, s); }); }
static void rec_terror(OhplA2Core *c, const OhplA2Host *h, float s)
{ RecCall(4, c, h, FB(s), 0, 0, 0, [&](const OhplA2Host *x) { ohpl_a2_update_terror(c, x, s); }); }
static void rec_focus(OhplA2Core *c, const OhplA2Host *h, float s)
{ RecCall(5, c, h, FB(s), 0, 0, 0, [&](const OhplA2Host *x) { ohpl_a2_update_focus_alpha(c, x, s); }); }
#define ohpl_a2_vitals rec_vitals
#define ohpl_a2_spin_head rec_spin
#define ohpl_a2_update_camera rec_cam
#define ohpl_a2_update_head_spin rec_hspin
#define ohpl_a2_update_terror rec_terror
#define ohpl_a2_update_focus_alpha rec_focus
