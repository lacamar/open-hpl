#pragma once
// Fake game for the GA3a differential: the real math, the real class text of the enemy, and recording
// stand-ins for everything else.
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
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
#include "math/BoundingVolume.h"
#include "system/String.h"
#include "graphics/Color.h"

using namespace hpl;
typedef std::string string;
#define OHPL_A4_NO_FWD
#define hplNew(cls, args) new cls args
#define hplDelete(p) delete p
#define _W(x) L##x

#include "RustLuxEnemy.h"
struct cSoundEntity;
class cAnimationState;
#include "RustLuxEnemyA4.h"
#include "impl/RustBackends.h"

extern std::string gLog;
void Rec(const char *fmt, ...);
uint32_t Bits(float f);
uint64_t Mix(uint64_t a, uint64_t b);
uint64_t HV(const cVector3f &v);

void Error(const char *fmt, ...);
void Warning(const char *fmt, ...);
void FatalError(const char *fmt, ...);

#include "enums.inc"

enum eCharDir { eCharDir_Forward, eCharDir_Right, eCharDir_LastEnum };
enum eLuxDamageType { eLuxDamageType_BloodSplat, eLuxDamageType_Claws, eLuxDamageType_LastEnum };
enum eLuxWeaponHitType { eLuxWeaponHitType_Sword, eLuxWeaponHitType_LastEnum };
enum eLuxEntityType { eLuxEntityType_Prop, eLuxEntityType_Enemy, eLuxEntityType_LastEnum };
enum eLuxPropType { eLuxPropType_SwingDoor, eLuxPropType_Other, eLuxPropType_Object };
enum eLuxIdleBehavior { eLuxIdleBehavior_None, eLuxIdleBehavior_Stalk, eLuxIdleBehavior_Track };
enum eLuxMoveState { eLuxMoveState_Normal, eLuxMoveState_Other };
enum eLuxProgressLogLevel { eLuxProgressLogLevel_Low };
enum eAnimationEventType { eAnimationEventType_Step };
typedef int tAIFreePathFlag;
#define eAIFreePathFlag_SkipDynamic 2

struct cSoundEntity { int mlID; int GetCreationID() { return mlID; } void FadeOut(float f) { Rec("Sound.FadeOut %d %08x", mlID, Bits(f)); } };
struct cSoundEntry;
struct cParticleSystem { void SetPosition(const cVector3f &p); };
struct cWorld
{
	cParticleSystem mPS;
	bool SoundEntityExists(cSoundEntity *s, int id);
	cParticleSystem *CreateParticleSystem(const std::string &n, const std::string &t, float f);
};
struct cLuxMap;
struct cRendererCallbackFunctions;
struct cGuiSet;
struct iFontData;
struct cSurfaceData;
struct cBillboard;
struct cBeam;
struct iLight;
struct cLuxProp_Object;
struct iCollideShape;
struct iLuxEntity_SaveData;
struct cLuxEntityIterator;
struct iLuxProp;
struct cXmlElement;
struct iLuxEnemy;
struct cResourceVarsObject;
struct iEntity3D;
class cLuxEnemy_Grunt;
class cLuxEnemy_ManPig;
class cLuxEnemy_WaterLurker;
class cLuxEnemyLoader_ManPig;
class cLuxEnemyLoader_WaterLurker;
class cLuxEnemyPathfinder;
class cLuxEnemyMover;
enum eLuxFocusCrosshair { eLuxFocusCrosshair_Default };
enum eCollideShapeType { eCollideShapeType_Box };
typedef std::list<float> tFloatList;
struct cEnemyAttackSizeData;
typedef tFloatList::iterator tFloatListIt;

class iPhysicsBody
{
public:
	cBoundingVolume mBV;
	float mfMass;
	cVector3f mvLocal, mvWorld;
	cBoundingVolume *GetBoundingVolume() { return &mBV; }
	float GetMass() { return mfMass; }
	cVector3f GetLocalPosition() { return mvLocal; }
	cVector3f GetWorldPosition() { return mvWorld; }
	void AddImpulse(const cVector3f &v) { Rec("Impulse %016llx", (unsigned long long)HV(v)); }
};

class iCharacterBody
{
public:
	cVector3f mvPos, mvLast, mvSize, mvForward, mvVel;
	float mfYaw, mfWanted, mfMoveDelay, mfMaxPos, mfAcc, mfDeacc;
	bool mbMove;
	iPhysicsBody mBody;
	iCharacterBody() : mvSize(0.8f, 1.8f, 0.8f), mvForward(0, 0, -1), mfYaw(0), mfWanted(0), mfMoveDelay(0), mfMaxPos(0), mfAcc(1), mfDeacc(1), mbMove(false) {}
	cVector3f GetPosition() { return mvPos; }
	cVector3f GetLastPosition() { return mvLast; }
	cVector3f GetFeetPosition() { return mvPos - cVector3f(0, mvSize.y / 2, 0); }
	cVector3f GetSize() { return mvSize; }
	cVector3f GetForward() { return mvForward; }
	cVector3f GetVelocity(float) { return mvVel; }
	float GetYaw() { return mfYaw; }
	float GetMoveSpeed(eCharDir) { return mfWanted; }
	float GetMoveDelay() { return mfMoveDelay; }
	iPhysicsBody *GetCurrentBody() { return &mBody; }
	void Move(eCharDir, float f) { Rec("Move %08x", Bits(f)); mbMove = true; }
	void AddYaw(float f) { Rec("AddYaw %08x", Bits(f)); mfYaw += f; }
	void SetMaxPositiveMoveSpeed(eCharDir, float f) { Rec("MaxPos %08x", Bits(f)); mfMaxPos = f; }
	void SetMaxNegativeMoveSpeed(eCharDir, float f) { Rec("MaxNeg %08x", Bits(f)); }
	void SetMoveAcc(eCharDir, float f) { Rec("Acc %08x", Bits(f)); mfAcc = f; }
	void SetMoveDeacc(eCharDir, float f) { Rec("Deacc %08x", Bits(f)); mfDeacc = f; }
	void SetActive(bool b) { Rec("BodyActive %d", b); }
	void SetMoveDelay(float f) { Rec("MoveDelay %08x", Bits(f)); mfMoveDelay = f; }
	void SetFeetPosition(const cVector3f &p) { Rec("SetFeet %016llx", (unsigned long long)HV(p)); mvPos = p + cVector3f(0, mvSize.y / 2, 0); }
	cVector3f GetRight() { return cVector3f(1, 0, 0); }
	cVector3f GetUp() { return cVector3f(0, 1, 0); }
	void SetEntityOffset(const cMatrixf &m) { Rec("EntityOffset %016llx", (unsigned long long)Mix(Mix(Bits(m.m[0][3]), Bits(m.m[1][3])), Bits(m.m[2][3]))); }
};

struct cFakeLantern { bool mbOn; bool IsActive() { return mbOn; } };
struct cFakeLight { float mfLevel; float GetNormalLightLevel() { return mfLevel; } };
struct cFakeFlash { bool mbOn; bool IsActive() { return mbOn; } };
struct cFrustumFake
{
	uint64_t mSalt;
	cVector3f mvOrigin;
	eCollision CollideBoundingVolume(cBoundingVolume *bv)
	{
		uint64_t h = Mix(Mix(HV(bv->GetWorldCenter()), mSalt), 77);
		unsigned k = (unsigned)(h % 7);
		return k < 2 ? eCollision_Outside : (k < 4 ? eCollision_Intersect : eCollision_Inside);
	}
	cVector3f GetOrigin() { return mvOrigin; }
};
#define cFrustum cFrustumFake
#define cCamera cFakeCam
struct cFakeCam { cVector3f mvPos; cFrustumFake mFrustum; cVector3f GetPosition() { return mvPos; } cFrustumFake *GetFrustum() { return &mFrustum; } };
struct cLuxMoveState_Normal { bool mbCrouch; bool IsCrouching() { return mbCrouch; } };

struct cLuxPlayer
{
	iCharacterBody mBody;
	cFakeLantern mLantern;
	cFakeLight mLight;
	cFakeFlash mFlash;
	cFakeCam mCam;
	cLuxMoveState_Normal mNormal;
	bool mbDead;
	int mMoveState;
	float mfAvgSpeed, mfTerror;
	iCharacterBody *GetCharacterBody() { return &mBody; }
	bool IsDead() { return mbDead; }
	cFakeLantern *GetHelperLantern() { return &mLantern; }
	cFakeLight *GetHelperLightLevel() { return &mLight; }
	cFakeFlash *GetHelperFlashback() { return &mFlash; }
	cFakeCam *GetCamera() { return &mCam; }
	int GetCurrentMoveState() { return mMoveState; }
	cLuxMoveState_Normal *GetCurrentMoveStateData() { return &mNormal; }
	float GetAvgSpeed() { return mfAvgSpeed; }
	float GetTerror() { return mfTerror; }
	float mfSanity;
	float GetSanity() { return mfSanity; }
	void SetTerror(float f) { Rec("SetTerror %08x", Bits(f)); mfTerror = f; }
	void AddTerrorEnemy(iLuxEnemy *) { Rec("AddTerror"); }
	void RemoveTerrorEnemy(iLuxEnemy *) { Rec("RemoveTerror"); }
};

struct cLuxMapHelper
{
	int mlDensity;
	bool CheckLineOfSight(const cVector3f &a, const cVector3f &b, bool);
	float GetLightLevelAtPos(const cVector3f &p, std::vector<iLight *> * = NULL, float = 0);
	bool GetClosestCharCollider(const cVector3f &s, const cVector3f &d, float len, float *dist, cVector3f *n, iPhysicsBody **b);
	bool ShapeDamage(iCollideShape *s, const cMatrixf &m, const cVector3f &o, float mn, float mx, float force, float imp, int str, float hs, eLuxDamageType dt, eLuxWeaponHitType wt,
					 bool ce, bool cp, bool cpr, bool lethal, bool *hitPlayer);
};
struct cLuxEffectRenderer
{
	void AddEnemyGlow(int sub, float a) { Rec("Glow %d %08x", sub, Bits(a)); }
};
struct cLuxMusicHandler
{
	void AddEnemy(eLuxEnemyMusic t, iLuxEnemy *) { Rec("MusicAdd %d", (int)t); }
	void RemoveEnemy(eLuxEnemyMusic t, iLuxEnemy *) { Rec("MusicRemove %d", (int)t); }
};
struct cLuxDebugHandler
{
	void AddMessage(const std::wstring &s, bool);
};
struct cLuxGlobalDataHandler
{
	bool mbAllowed;
	bool GetEnemyActivateSoundAllowed() { return mbAllowed; }
	void SetEnemyActivateSoundMade() { Rec("ActivateSoundMade"); }
};
struct cLuxProgressLogHandler
{
	void AddLog(eLuxProgressLogLevel, const std::string &s) { Rec("ProgressLog %s", s.c_str()); }
};
struct cEngine { float GetStepSize() { return 1.0f / 60.0f; } };
struct cGameCfg { float GetFloat(const char *, const char *, float f) { return f; } };
struct cLuxBase
{
	cEngine *mpEngine;
	cLuxPlayer *mpPlayer;
	cLuxMapHelper *mpMapHelper;
	cLuxMusicHandler *mpMusicHandler;
	cLuxDebugHandler *mpDebugHandler;
	cLuxGlobalDataHandler *mpGlobalDataHandler;
	cLuxProgressLogHandler *mpProgressLogHandler;
	cGameCfg *mpGameCfg;
	cLuxEffectRenderer *mpEffectRenderer;
};
extern cLuxBase *gpBase;

class cAnimationState
{
public:
	std::string msName;
	bool mbActive, mbOver, mbFading, mbLoop;
	float mfSpecial, mfPrev, mfTime, mfRel;
	const std::string &GetName() { return msName; }
	bool IsActive() { return mbActive; }
	bool IsOver() { return mbOver; }
	bool IsFading() { return mbFading; }
	bool IsLooping() { return mbLoop; }
	void SetActive(bool b) { Rec("A.SetActive %s %d", msName.c_str(), b); mbActive = b; }
	void SetWeight(float f) { Rec("A.Weight %s %08x", msName.c_str(), Bits(f)); }
	void FadeIn(float f) { Rec("A.FadeIn %s %08x", msName.c_str(), Bits(f)); mbFading = true; }
	void FadeOut(float f) { Rec("A.FadeOut %s %08x", msName.c_str(), Bits(f)); }
	void SetLoop(bool b) { Rec("A.Loop %s %d", msName.c_str(), b); mbLoop = b; }
	void SetRelativeTimePosition(float f) { Rec("A.RelTime %s %08x", msName.c_str(), Bits(f)); }
	float GetRelativeTimePosition() { return mfRel; }
	void SetTimePosition(float f) { Rec("A.Time %s %08x", msName.c_str(), Bits(f)); }
	float GetTimePosition() { return mfTime; }
	float GetPreviousTimePosition() { return mfPrev; }
	float GetSpecialEventTime() { return mfSpecial; }
	void SetSpeed(float f) { Rec("A.Speed %s %08x", msName.c_str(), Bits(f)); }
	float GetLength() { return 2.0f; }
};

struct cMeshEntity
{
	std::map<std::string, cAnimationState *> mAnims;
	cAnimationState *GetAnimationStateFromName(const std::string &s);
	cBoundingVolume mBV;
	bool mbVisible;
	void SetActive(bool b) { Rec("Mesh.Active %d", b); }
	void SetVisible(bool b) { Rec("Mesh.Visible %d", b); mbVisible = b; }
	bool IsVisible() { return mbVisible; }
	cBoundingVolume *GetBoundingVolume() { return &mBV; }
	int GetSubMeshEntityNum() { return 3; }
	int GetSubMeshEntity(int i) { return i; }
};

struct cAINode
{
	cVector3f mvPos;
	int mlID;
	cVector3f GetPosition() { return mvPos; }
	int GetID() { return mlID; }
};
typedef std::list<cAINode *> tAINodeList;
typedef tAINodeList::iterator tAINodeListIt;

struct cAINodeIterator
{
	std::vector<cAINode *> mv;
	size_t mi;
	bool HasNext() { return mi < mv.size(); }
	cAINode *Next() { return mv[mi++]; }
};

struct cAINodeContainer
{
	std::vector<cAINode *> mvNodes;
	int GetNodeNum() { return (int)mvNodes.size(); }
	bool GetNodeIsAtCenter() { return false; }
	cVector3f GetCollideSize() { return cVector3f(0.8f, 1.8f, 0.8f); }
	cAINodeIterator GetNodeIterator(const cVector3f &p, float d);
	bool FreePath(const cVector3f &a, const cVector3f &b, int rays, int flags);
};

struct cAStarHandler
{
	cAINodeContainer *mpC;
	bool GetPath(const cVector3f &a, const cVector3f &b, tAINodeList *l);
};

class iLuxEntity;
struct iEntity3D
{
	cVector3f mvPos;
	cVector3f GetWorldPosition() { return mvPos; }
};

class iLuxEntity
{
public:
	std::string msName;
	cLuxMap *mpMap;
	int mlID;
	bool mbActive, mbDestroy;
	eLuxEntityType mType;
	iEntity3D mAttach;
	iLuxEntity(const std::string &n, int id, cLuxMap *m, eLuxEntityType t) : msName(n), mpMap(m), mlID(id), mbActive(true), mbDestroy(false), mType(t) {}
	virtual ~iLuxEntity() {}
	const std::string &GetName() { return msName; }
	int GetID() { return mlID; }
	eLuxEntityType GetEntityType() { return mType; }
	bool GetDestroyMe() { return mbDestroy; }
	bool IsActive() { return mbActive; }
	virtual iEntity3D *GetAttachEntity() { return &mAttach; }
	void SetActive(bool abX);
	virtual void OnSetActive(bool) {}
	void RunCallbackFunc(const std::string &s) { Rec("Callback %s", s.c_str()); }
	virtual void OnUpdate(float) {}
	virtual void SetupAfterLoad(cWorld *) {}
	virtual void AfterWorldLoad() {}
	virtual void OnMapEnter() {}
	virtual void OnRenderSolid(cRendererCallbackFunctions *) {}
	virtual bool CanInteract(iPhysicsBody *) { return false; }
	virtual bool OnInteract(iPhysicsBody *, const cVector3f &) { return false; }
	virtual void OnConnectionStateChange(iLuxEntity *, int) {}
	virtual void SaveToSaveData(iLuxEntity_SaveData *) {}
	virtual void LoadFromSaveData(iLuxEntity_SaveData *) {}
	virtual void SetupSaveData(iLuxEntity_SaveData *) {}
};

struct iLuxProp : public iLuxEntity
{
	eLuxPropType mPropType;
	std::vector<iPhysicsBody *> mvBodies;
	iPhysicsBody mMain;
	iLuxProp(int id) : iLuxEntity("door", id, NULL, eLuxEntityType_Prop), mPropType(eLuxPropType_SwingDoor) {}
	eLuxPropType GetPropType() { return mPropType; }
	int GetBodyNum() { return (int)mvBodies.size(); }
	iPhysicsBody *GetBody(int i) { return mvBodies[i]; }
	iPhysicsBody *GetMainBody() { return &mMain; }
};

struct cLuxProp_Object : public iLuxProp
{
	bool mbFood;
	float mfHealth;
	cLuxProp_Object(int id) : iLuxProp(id), mbFood(true), mfHealth(10) { mPropType = eLuxPropType_Object; }
	bool IsFood() { return mbFood; }
	float GetHealth() { return mfHealth; }
	void GiveDamage(float a, int s) { Rec("FoodDamage %08x %d", Bits(a), s); mfHealth -= a; }
};

struct cLuxEntityIterator
{
	std::vector<iLuxEntity *> mv;
	size_t mi;
	bool HasNext() { return mi < mv.size(); }
	iLuxEntity *Next() { return mv[mi++]; }
};

struct cLuxMap
{
	std::vector<iLuxEntity *> mvEntities, mvAll;
	std::vector<iLuxEnemy *> mvEnemies;
	std::set<int> msBroken;
	cWorld *mpWorld;
	cWorld *GetWorld() { return mpWorld; }
	int mlUsedMask;
	iLuxEntity *GetEntityByID(int id, eLuxEntityType t = eLuxEntityType_LastEnum, int s = -1);
	bool DoorIsBroken(int id) { return msBroken.count(id) != 0; }
	bool AINodeIsUsedAsGoal(cAINode *n) { return (Mix(n->mlID, mlUsedMask) & 7) == 0; }
	cLuxEntityIterator GetEntityIterator() { cLuxEntityIterator it; it.mv = mvEntities; it.mi = 0; return it; }
	void BroadcastEnemyMessage(eLuxEnemyMessage aType, bool abHasPosition, const cVector3f &avPos, float afRadius, float afTime, bool abLocalScope,
							   const cVector3f &avX, float afX = 0, int alX = 0);
};

namespace hpl
{
bool CheckBVIntersectionFake(cBoundingVolume &, cBoundingVolume &);
}
