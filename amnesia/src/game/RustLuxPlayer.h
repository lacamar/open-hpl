#ifndef LUX_RUST_LUX_PLAYER_H
#define LUX_RUST_LUX_PLAYER_H

// C ABI of rust/src/luxplayer/. Steps edit POD copies of the C++ members and return ordered ops; C++ replays them.

#include "hpl.h"

#include <cstddef>
#include <cstdint>



enum OhplGa2OpKind : uint32_t
{
	kOhplGa2SetHealth = 1,
	kOhplGa2SetSanity = 2,
	kOhplGa2SetOil = 3,
	kOhplGa2DamageSplash = 4,
	kOhplGa2HitFlash = 5,
	kOhplGa2SpinHead = 6,
	kOhplGa2DeathStart = 7,
	kOhplGa2CollapseStart = 8,
	kOhplGa2SanityLost = 9,
	kOhplGa2SanityGainFlash = 10,
	kOhplGa2Kill = 11,

	kOhplGa2SetFov = 20,
	kOhplGa2SetAspect = 21,
	kOhplGa2SetPitch = 22,
	kOhplGa2SetRoll = 23,

	kOhplGa2SetHeadAdd = 30,
	kOhplGa2FadeRoll = 31,
	kOhplGa2SetCollapseSpeedMul = 32,
	kOhplGa2SetCrouchDisabled = 33,
	kOhplGa2SetJumpDisabled = 34,
	kOhplGa2PlayRandomSound = 35,
	kOhplGa2RandCount = 36,
	kOhplGa2WakeFx = 37,
	kOhplGa2LoopFadeOut = 38,
	kOhplGa2PlayerSetSanity = 39,
	kOhplGa2ListClear = 40,
	kOhplGa2ListPush = 41,
	kOhplGa2ListPopFront = 42,
	kOhplGa2PlayAwaken = 45,

	kOhplGa2SetHurtMoveSpeedMul = 50,
	kOhplGa2PlayHurtPant = 51,
	kOhplGa2SetHurtHeadAdd = 52,

	kOhplGa2LookAddPitch = 60,
	kOhplGa2LookAddYaw = 61,
	kOhplGa2LookSyncBodyYaw = 62,
	kOhplGa2LookAtTarget = 63,

	kOhplGa2FlashMid = 70,
	kOhplGa2FlashbackBegin = 71,
	kOhplGa2FlashbackEnd = 72,
	kOhplGa2FlashbackStartQueued = 73,

	kOhplGa2LanternColor = 100,
	kOhplGa2LanternOutOfOil = 101,
	kOhplGa2LanternSetActive = 102,
	kOhplGa2LanternHintNoItem = 103,
	kOhplGa2LanternHintNoOil = 104,
	kOhplGa2LanternSoundOut = 105,
	kOhplGa2LanternSoundOn = 106,
	kOhplGa2LanternSoundOff = 107,
	kOhplGa2LanternHand = 108,
	kOhplGa2LanternCallback = 109,
	kOhplGa2LanternSetOil = 110,

	kOhplGa2AmbientOff = 120,
	kOhplGa2AmbientOn = 121,
	kOhplGa2DarknessLowerSanity = 122,
	kOhplGa2HintDarkness = 123,

	kOhplGa2DeathPermaSound = 131,
	kOhplGa2DeathResetGame = 132,
	kOhplGa2DeathFlash = 133,
	kOhplGa2DeathAwakenSound = 134,
	kOhplGa2DeathReset = 135,
	kOhplGa2DeathAutoload = 136,
	kOhplGa2DeathExit = 137,

	kOhplGa2HandsAttachDraw = 150,
	kOhplGa2HandsDisable = 151,
	kOhplGa2HandsIdle = 152,

	kOhplGa2SanSetSanity = 200,
	kOhplGa2SanLower = 201,
	kOhplGa2SanEnemyUnseenBlur = 202,
	kOhplGa2SanHintEnemySeen = 203,
	kOhplGa2SanBlurEnemy = 204,
	kOhplGa2SanImageTrail = 205,
	kOhplGa2SanFadeAspect = 206,
	kOhplGa2SanFadeFov = 207,
	kOhplGa2SanInsanityActive = 208,
	kOhplGa2SanInsanityFx = 209,
	kOhplGa2SanHeartbeat = 210,
	kOhplGa2SanDrain = 211,
	kOhplGa2SanHintLow = 212,
	kOhplGa2SanRandPant = 213,
	kOhplGa2SanPant = 214,
};


enum OhplGa2VitalsOp
{
	kOhplGa2GiveDamage = 0,
	kOhplGa2SetHealthOp = 1,
	kOhplGa2SetSanityOp = 2,
	kOhplGa2LowerSanityOp = 3,
	kOhplGa2AddHealthOp = 4,
	kOhplGa2AddSanityOp = 5,
	kOhplGa2AddLampOilOp = 6,
};

struct OhplGa2CamState
{
	float mfFOV, mfAspect;
	float mfFOVMul, mfFOVMulGoal, mfFOVMulSpeed;
	float mfAspectMul, mfAspectMulGoal, mfAspectMulSpeed;
	float mfRoll, mfRollGoal, mfRollSpeedMul, mfRollMaxSpeed;
	float mfLeanRoll, mfLeanRollGoal, mfLeanRollSpeedMul, mfLeanRollMaxSpeed;
	float mfPitchGoal, mfPitchSpeedMul, mfPitchMaxSpeed;
	hpl::cVector3f mvCamAnimPos, mvCamAnimPosGoal;
	float mfCamAnimPosSpeedMul, mfCamAnimPosMaxSpeed;
	uint32_t mbFadingPitch, mbFreeCamera;
	float mfCamPitch, mfPitchMin, mfPitchMax;
};

struct OhplGa2HeadPosAdd
{
	uint32_t mbMoving;
	hpl::cVector3f mvMoveGoal;
	float mfMoveSpeed, mfMoveSlowDownDist;
	hpl::cVector3f mvAdd;
};

struct OhplGa2Collapse
{
	uint32_t mbActive;
	int32_t mlState;
	float mfHeightAdd, mfRoll, mfT, mfRandomCount;
	float mfHeightAddGoal, mfHeightAddCollapseSpeed, mfHeightAddAwakeSpeed, mfRollCollapseSpeed;
	float mfSleepTime, mfWakeUpSpeedMul, mfAwakenSanity, mfSleepRandomMinTime, mfSleepRandomMaxTime;
};

struct OhplGa2CamDir
{
	float mfStartSwayMaxSanity;
	uint32_t mbSwayActive;
	hpl::cVector2f mvSwayAdd;
	int32_t mlMaxPositions;
	float mfSwayAlpha;
	hpl::cVector2f mvNextAdd;
};

struct OhplGa2Hurt
{
	float mfEffectStartHealth, mfMinMoveMul, mfMaxPantCount, mfPantSpeed, mfPantSize;
	int32_t mlCurrentNoise;
	float mfNoiseUpdateCount, mfNoiseFreq, mfPantCount, mfPantPosAdd, mfPantPosAddVel, mfPantPosAddDir, mfAlpha;
	int32_t mlNoiseCount;
};

struct OhplGa2LookAt
{
	hpl::cVector3f mvCurrentSpeed;
	float mfSpeedMul, mfMaxSpeed;
};

struct OhplGa2Flashback
{
	float mfRadialBlurSize, mfRadialBlurStartDist, mfMoveSpeedMul, mfRunSpeedMul, mfFlashDelay;
	uint32_t mbActive;
	float mfFlashbackStartCount;
};

struct OhplGa2Lantern
{
	float mColor[4];
	float mfFadeLightOilAmount, mfLowerOilSpeed;
	uint32_t mbActive;
	float mfAlpha;
};


struct OhplGa2Darkness
{
	float mfMinDarknessLightLevel, mfAmbientLightMinLightLevel, mfLoopSoundStartupTime, mfSanityLossPerSecond;
	uint32_t mbAmbientLightIsOn, mbInDarkness;
	float mfLoopSoundCount, mfSanityLossMul, mfShowHintTimer;
};


struct OhplGa2Death
{
	uint32_t mbActive, mbShowHint;
	int32_t mlState;
	float mfHeightAddSpeed, mfRollSpeed, mfFadeOutTime, mfHeightAdd, mfRoll, mfT, mfMinHeightAdd, mfFadeAlpha, mfTextAlpha1,
		mfTextOnScreenCount, mfWhiteCount, mfHeightAddGoal, mfHeightAddGoalCrouch;
};

struct OhplGa2Sanity
{
	float mfHitAlpha;
	uint32_t mbHitActive;
	float mfSanityLostCount, mfAtLowSanityCount;
	uint32_t mbHitIsUpdated, mbSanityLostIsUpdated;
	float mfPantCount, mfCheckEnemySeenCount;
	uint32_t mbSanityEffectUpdated;
	float mfT, mfInsaneWaveAlpha, mfSanityDrainCount, mfSanityDrainVolume, mfSanityHeartbeatCount, mfSeenEnemyCount;
	uint32_t mbEnemyIsSeen;
	float mfShowHintTimer;
	float mfHitZoomInSpeed, mfHitZoomOutSpeed, mfHitZoomInFOVMul, mfHitZoomInAspectMul, mfSanityRegainSpeed, mfSanityRegainLimit,
		mfSanityVeryLowLimit, mfSanityEffectsStart, mfSanityWaveAlphaMul, mfSanityWaveSpeedMul, mfSanityLowLimit,
		mfSanityLowLimitMaxTime, mfSanityLowNewSanityAmount, mfCheckNearEnemyInterval, mfNearEnemyDecrease;
};


extern "C" {
float ohpl_ga2_clamp_pitch_goal(float x, float min, float max);
void ohpl_ga2_update_head_pos_add(OhplGa2HeadPosAdd *adds, size_t n, const hpl::cVector3f *cameraPosAdd, float dt, hpl::cVector3f *total,
	hpl::cVector3f *sum);
}

// ---- GA2b: player and move states (rust/src/luxplayer/states_*.rs, move_normal.rs) ----

enum OhplGa2bAct : int32_t { kOhplGa2bActOther = 0, kOhplGa2bActHolster = 1, kOhplGa2bActAttack = 2, kOhplGa2bActInteract = 3, kOhplGa2bActLantern = 4 };

// Player state codes in OhplGa2bAction::change: 0 none, 1 normal, 2 hand object, 3 previous state.
enum OhplGa2bPs : int32_t { kOhplGa2bPsNone = 0, kOhplGa2bPsNormal = 1, kOhplGa2bPsHand = 2, kOhplGa2bPsPrevious = 3, kOhplGa2bPsPush = 4, kOhplGa2bPsGrab = 5, kOhplGa2bPsOther = 9 };

enum OhplGa2bPlan : int32_t { kOhplGa2bPlanNone = 0, kOhplGa2bPlanCallback = 1, kOhplGa2bPlanDoesNotWork = 2, kOhplGa2bPlanNoObject = 3 };

struct OhplGa2bPid3
{
	float p = 0, i = 0, d = 0;
	int32_t n = 0, errorNum = 0, lastNum = -1;
	hpl::cVector3f errors[20];
	float steps[20] = {};
	hpl::cVector3f integral, derivative;
};

struct OhplGa2bMoveMulIn
{
	float interaction, script, event, hurt, collapse, water;
	uint32_t inWater;
};

struct OhplGa2bSpeedCfg
{
	float crouchFwd, crouchBack, crouchSide, runFwd, runBack, runSide, airFwd, airBack, airSide;
};

struct OhplGa2bSpeedMuls
{
	float fwd, back, side, defaultFwd;
};

struct OhplGa2bMoveLimits
{
	float fwdPos, fwdNeg, sidePos, sideNeg;
};

struct OhplGa2bJumpState
{
	float count, max;
	uint32_t jumping;
};

struct OhplGa2bFallCfg
{
	float speedMin, damageMin, speedMed, damageMed, speedMax, damageMax;
};

struct OhplGa2bBobCfg
{
	hpl::cVector2f walkMax, runMax, crouchMax;
	float walkMinSpeed, walkMaxSpeed, runMinSpeed, runMaxSpeed, crouchMinSpeed, crouchMaxSpeed;
	float maxForwardSpeed, bounceSize, bounceSpeed;
};

struct OhplGa2bBobState
{
	hpl::cVector3f headBob;
	float prevCount, count;
	uint32_t bobbing, groundBounceActive;
	float groundBounce;
	hpl::cVector2f curMax, maxGoal;
	float bounceSizeMul, bounceSpeedMul, defaultFwdMul;
};

struct OhplGa2bBobIn
{
	hpl::cVector3f velocity;
	uint32_t crouching, running, onGround, movedLastUpdate;
};

struct OhplGa2bLedgeCfg
{
	float giveControlDist, pitchFactor, maxPitchSpeed;
};

struct OhplGa2bAction
{
	uint32_t pass;
	int32_t change;
	uint32_t forwardToHands, useItem;
};














struct OhplGa2bJointGeom
{
	hpl::cMatrixf bodyMatrix;
	hpl::cVector3f massCentre, pivot, pin, up, right, forward;
	hpl::cVector2f mouse;
};





extern "C" {
float ohpl_ga2b_move_speed_mul(const OhplGa2bMoveMulIn *i);
void ohpl_ga2b_speed_multipliers(const OhplGa2bSpeedCfg *c, float moveMul, float runMul, bool onGround, bool crouching, bool running,
	OhplGa2bSpeedMuls *out);
void ohpl_ga2b_move_limits(float maxFwd, float maxBack, float maxSide, const OhplGa2bSpeedMuls *m, OhplGa2bMoveLimits *out);
bool ohpl_ga2b_movement_update(bool onGround, bool pressedMove, bool *wasMoving, bool running, bool crouchDisabled, bool collapseActive);
bool ohpl_ga2b_run_pressed_stands(bool crouchDisabled, bool collapseActive, bool onGround, bool pressedMove);
bool ohpl_ga2b_jump_starts(bool pressed, bool jumping, bool onGround);
float ohpl_ga2b_jump_force(bool crouching, float crouchForce, float force, float scriptMul, float mass);
bool ohpl_ga2b_jump_tick(OhplGa2bJumpState *s, float dt);
float ohpl_ga2b_jump_gravity_force(float count, float max, float gravityY, float mass);
void ohpl_ga2b_ground_check(bool onGround, float forceVelY, bool *was);
bool ohpl_ga2b_bounce_starts(float normalY, float forceVelY, float minSpeed, bool active);
int32_t ohpl_ga2b_fall_damage(const OhplGa2bFallCfg *c, float ySpeed, float *damage);
bool ohpl_ga2b_head_bob(OhplGa2bBobState *s, const OhplGa2bBobCfg *c, const OhplGa2bBobIn *i, float dt);
bool ohpl_ga2b_ledge_update(const OhplGa2bLedgeCfg *c, const hpl::cVector3f *headAdd, float pitch, float dt, float *pitchAdd);
void ohpl_ga2b_action(int32_t which, int32_t act, bool pressed, OhplGa2bAction *out);
bool ohpl_ga2b_hand_object_hides(int32_t newState);
bool ohpl_ga2b_focus_in_range(bool hasEntity, float dist, float maxFocus);
bool ohpl_ga2b_focus_can_interact(bool inRange, bool canInteract, bool disabled);
bool ohpl_ga2b_outline_gate(bool hasEntity, bool isProp, float dist, float maxFocus);
bool ohpl_ga2b_show_outline(bool isItem, bool canInteract, bool disabled);
float ohpl_ga2b_use_item_max_focus(float maxFocus, float minUse);
int32_t ohpl_ga2b_use_item_focus(bool hasItem, bool hasEntity, bool hasMap, float dist, float max);
int32_t ohpl_ga2b_use_item_plan(bool callback);
float ohpl_ga2b_lever_speed_add(const OhplGa2bJointGeom *g);
float ohpl_ga2b_swing_door_speed_add(const OhplGa2bJointGeom *g);
void ohpl_ga2b_swing_door_throw(const OhplGa2bJointGeom *g, float impulse, hpl::cVector3f *jointForward, hpl::cVector3f *out);
bool ohpl_ga2b_wheel_pop(size_t size);
float ohpl_ga2b_wheel_speed_add(const hpl::cVector2f *hist, size_t n, const hpl::cVector2f *mouse, const hpl::cVector3f *pin,
	const hpl::cVector3f *forward);
void ohpl_ga2b_ladder_head_limits(float yaw, float pitch, float *out);
}

#endif
