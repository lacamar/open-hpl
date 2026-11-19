#ifndef SOMA_RUST_CORE_H
#define SOMA_RUST_CORE_H

// rust/src/somaent/ent_core.rs: plain-data state of cSomaLuxEntity, allocated and freed by Rust.

#include <cstddef>
#include <cstdint>
#include "RustSomaEntity.h"

// Layout of hpl::cID (script tID); the map's UID="a b c" attribute
struct cSomaID
{
	uint8_t mA = 0;
	int32_t mB = 0;
	int32_t mC = 0;
	bool operator==(const cSomaID &o) const { return mA == o.mA && mB == o.mB && mC == o.mC; }
};

using hpl::cColor;
using hpl::cMatrixf;
using hpl::cVector3f;

struct OhplSeCore
{
	bool mbSpawned;
	int meType;
	cSomaID mID;
	int mlParentType;
	cSomaID mParentID;
	bool mbActive;
	bool mbSleeping;
	bool mbAutoSleep;
	bool mbShowMesh;
	bool mbIsDoor;
	bool mbIsClosedDoor;
	bool mbInteractionDisabled;
	float mfMaxInteractDistance;
	float mfVariableUpdateRate;
	float mfVariableUpdateAcc;
	bool mbInteractedWith;
	cMatrixf m_mtxOnLoad;
	cVector3f mvScale;
	cVector3f mvMeshScaleMul;
	bool mbGlobalSpaceAnim;
	cVector3f mvSize;
	bool mbCheckOutsidePlayer;
	bool mbCharCollisionUntilStopped;
	bool mbCharCollisionUntilStoppedStarted;
	cVector3f mvBodyOffset;
	bool mbInteractCallbackAutoRemove;
	bool mbAnimMessageCallbackAutoRemove;
	bool mbLookAtCallbackAutoRemove;
	bool mbLookAtCheckCenter;
	bool mbLookAtCheckRay;
	float mfLookAtMaxDistance;
	float mfLookAtDelay;
	bool mbForceLookAtCheck;
	float mfLookAtCheckTimer;
	float mfLookAtCountdown;
	bool mbLookedAt;
	bool mbEffectsActive;
	bool mbStaticPhysics;
	bool mbAllowMapTransfer;
	float mfEffectsAlpha;
	float mfEffectsFadeSpeed;
	float mfEffectsOffAlpha;
	cColor mIllumBaseColor;
	bool mbIllumBaseSet;
	cSomaID mInteractLeaderID;
	float mfInteractConnectionState;
	cColor mEffectBaseColor;
	cColor mEffectColorFrom;
	cColor mEffectColorTo;
	float mfEffectColorTime;
	float mfEffectColorT;
	float mfMeshFadeAlpha;
	float mfMeshFadeSpeed;
	float mfLiquidAmount;
	float mfLiquidTarget;
	float mfLiquidSpeed;
	float mfHealth;
	bool mbBroken;
	bool mbBreakable;
	bool mbScriptCollision;
	bool mbProcDirty;
	bool mbStaticMoveSound;
	int mlStaticMoveLoopID;
	bool mbCameraInLiquid;
	bool mbPlayerInLiquid;
	float mfLiquidTime;
	float mfLiquidSurfaceY;
	cMatrixf m_mtxLiquidPlaced;
	bool mbCheckCollision;
	bool mbCheckCenterInArea;
	bool mbCheckDynamic;
	bool mbCheckStatic;
	bool mbCheckCharacters;
	float mfTimeSinceCheck;
	OhplGs1Move mMove;
	OhplGs1Rotate mRotate;
	int mlCurrentAnim;
	int mlGuiDraws;
	int mlGuiCalls;
	bool mbGuiActive;
	float mfGuiFade;
	float mfGuiFadeSpeed;
	float mfGuiFPS;
	float mfGuiTimeAcc;
	bool mbGuiSetUseInput;
	bool mbGuiUpdateWhenOutOfView;
	bool mbGuiDirty;
	float mfGuiCacheAtScreenSize;
	bool mbGuiCacheForced;
	cVector3f mvGuiOrigin;
	cVector3f mvGuiRight;
	cVector3f mvGuiDown;
	int mlAnimLoopSoundID;
};

static_assert(sizeof(OhplSeCore) == 664, "OhplSeCore layout");

extern "C"
{
	OhplSeCore *ohpl_se_core_new();
	void ohpl_se_core_free(OhplSeCore *);
}

#endif
