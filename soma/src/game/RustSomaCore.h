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

struct OhplSeHost
{
	void (*get_matrix)(void *, cMatrixf *);
	void (*set_matrix_main)(void *, const cMatrixf *);
	void (*run_callback)(void *, int which); // 0 move, 1 rotate
	bool (*has_mesh)(void *);
	void (*mesh_coverage)(void *, float);
	void (*mesh_liquid)(void *, float);
	void (*capture_effect_defaults)(void *);
	void (*apply_effects_alpha)(void *);
};

extern "C"
{
	void ohpl_se_stop_move(OhplSeCore *);
	void ohpl_se_update_move(OhplSeCore *, void *ent, const OhplSeHost *, float dt);
	void ohpl_se_set_liquid(OhplSeCore *, void *ent, const OhplSeHost *, float x);
	void ohpl_se_fade_liquid(OhplSeCore *, void *ent, const OhplSeHost *, float target, float time);
	void ohpl_se_update_liquid(OhplSeCore *, void *ent, const OhplSeHost *, float dt);
	void ohpl_se_set_effect_color(OhplSeCore *, void *ent, const OhplSeHost *, const cColor *);
	void ohpl_se_fade_effect_color(OhplSeCore *, void *ent, const OhplSeHost *, const cColor *, float time);
	void ohpl_se_update_effect_color(OhplSeCore *, void *ent, const OhplSeHost *, float dt);
	void ohpl_se_register_entity(void *engine, const char *type, size_t core_offset);
	void ohpl_se_register_props(void *engine, size_t core_offset);
	OhplSeCore *ohpl_se_core_new();
	void ohpl_se_core_free(OhplSeCore *);
	struct OhplSeNames;
	OhplSeNames *ohpl_se_names_new();
	void ohpl_se_names_free(OhplSeNames *);
	const unsigned char *ohpl_se_names_get(const OhplSeNames *, size_t, size_t *len);
	const unsigned char *ohpl_se_var_get(OhplSeNames *, const unsigned char *, size_t, bool insert, size_t *len);
	void ohpl_se_var_set(OhplSeNames *, const unsigned char *, size_t, const unsigned char *, size_t);
	void ohpl_se_var_clear(OhplSeNames *);
	const unsigned char *ohpl_se_var_at(const OhplSeNames *, size_t, const unsigned char **k, size_t *kl, size_t *vl);
	void ohpl_se_names_set(OhplSeNames *, size_t, const unsigned char *, size_t len);
}

#endif
