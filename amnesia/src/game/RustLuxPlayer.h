#ifndef LUX_RUST_LUX_PLAYER_H
#define LUX_RUST_LUX_PLAYER_H

// C ABI of rust/src/luxplayer/. Steps edit POD copies of the C++ members and return ordered ops; C++ replays them.

#include "hpl.h"

#include <cstddef>
#include <cstdint>

struct OhplGa2Op
{
	uint32_t kind;
	int32_t i;
	float a, b, c, d;
};

struct OhplGa2Ops
{
	uint32_t n;
	OhplGa2Op ops[16];
};

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

struct OhplGa2Vitals
{
	float mfHealth, mfSanity, mfLampOil;
	uint32_t mbHardMode;
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

struct OhplGa2LanternSet
{
	bool mbActive, mbUseEffects, mbCheckOilItems, mbCheckAllowed, mbCurrent, mbAllowed, mbHasItem, mbDisabled;
	float mfOil;
};

struct OhplGa2Darkness
{
	float mfMinDarknessLightLevel, mfAmbientLightMinLightLevel, mfLoopSoundStartupTime, mfSanityLossPerSecond;
	uint32_t mbAmbientLightIsOn, mbInDarkness;
	float mfLoopSoundCount, mfSanityLossMul, mfShowHintTimer;
};

struct OhplGa2DarknessIn
{
	float mfExtLevel, mfNormalLevel;
	bool mbHardMode, mbFlashbackActive, mbDrainDisabled, mbEmotionFlash;
};

struct OhplGa2Death
{
	uint32_t mbActive, mbShowHint;
	int32_t mlState;
	float mfHeightAddSpeed, mfRollSpeed, mfFadeOutTime, mfHeightAdd, mfRoll, mfT, mfMinHeightAdd, mfFadeAlpha, mfTextAlpha1,
		mfTextOnScreenCount, mfWhiteCount;
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
		mfSanityLowLimitMaxTime, mfSanityLowNewSanityAmount, mfCheckNearEnemyInterval;
};

struct OhplGa2HandsPos
{
	hpl::cVector3f mvCamPos, mvRight, mvUp, mvForward, mvBob, mvShake, mvHurt;
	float mfPosAddMul;
};

extern "C" {
void ohpl_ga2_vitals(int32_t which, const OhplGa2Vitals *v, float amount, int32_t ty, uint32_t flags, OhplGa2Ops *out);
void ohpl_ga2_update_camera(OhplGa2CamState *s, float dt, OhplGa2Ops *out);
float ohpl_ga2_clamp_pitch_goal(float x, float min, float max);
void ohpl_ga2_update_head_pos_add(OhplGa2HeadPosAdd *adds, size_t n, const hpl::cVector3f *cameraPosAdd, float dt, hpl::cVector3f *total,
	hpl::cVector3f *sum);
void ohpl_ga2_spin_head(float rx, float ry, float speed, hpl::cVector2f *out);
bool ohpl_ga2_update_head_spin(hpl::cVector2f *spin, float deacc, float dt, hpl::cVector2f *add);
float ohpl_ga2_terror_step(float terror, bool enemies, float inc, float dec, float dt);
float ohpl_ga2_focus_alpha(float alpha, bool hide, float dt);
bool ohpl_ga2_prev_speeds_pop(size_t size, int32_t max);
float ohpl_ga2_avg_speed(const float *p, size_t n);
void ohpl_ga2_collapse_update(OhplGa2Collapse *s, float dt, OhplGa2Ops *out);
void ohpl_ga2_camdir_update(OhplGa2CamDir *s, const hpl::cVector2f *list, size_t n, float sanity, float dt, OhplGa2Ops *out);
float ohpl_ga2_camdir_add(OhplGa2CamDir *s, bool listEmpty, bool yaw, float x);
bool ohpl_ga2_hurt_regain(float health, float limit, float speed, float dt, float *amount);
void ohpl_ga2_hurt_update(OhplGa2Hurt *s, float health, bool dead, float speed, float dt, OhplGa2Ops *out);
void ohpl_ga2_lookat_update(OhplGa2LookAt *s, const hpl::cVector3f *goal, float pitch, float yaw, float dt, OhplGa2Ops *out);
void ohpl_ga2_flashback_update(OhplGa2Flashback *s, bool queued, bool voiceDone, bool hasCallback, float dt, OhplGa2Ops *out);
bool ohpl_ga2_lantern_update(OhplGa2Lantern *s, float oil, bool emotionFlash, float dt, OhplGa2Ops *out);
void ohpl_ga2_lantern_set_active(const OhplGa2LanternSet *p, OhplGa2Ops *out);
void ohpl_ga2_light_positions(const hpl::cVector3f *pos, const hpl::cVector3f *size, const hpl::cVector3f *forward, hpl::cVector3f *out);
bool ohpl_ga2_light_timer(float *count, float dt);
void ohpl_ga2_light_combine(bool lantern, const float *ext, const float *normal, float *outExt, float *outNormal);
void ohpl_ga2_darkness_update(OhplGa2Darkness *s, const OhplGa2DarknessIn *i, float dt, OhplGa2Ops *out);
void ohpl_ga2_death_phase1(OhplGa2Death *s, bool perma, float dt, OhplGa2Ops *out);
void ohpl_ga2_death_phase2(OhplGa2Death *s, bool hardModeActive, float dt, OhplGa2Ops *out);
void ohpl_ga2_death_phase3(int32_t state, bool voiceGone, OhplGa2Ops *out);
void ohpl_ga2_sanity_begin(OhplGa2Sanity *s, float sanity, float dt, OhplGa2Ops *out);
bool ohpl_ga2_sanity_scan_due(OhplGa2Sanity *s, float dt);
void ohpl_ga2_sanity_scan_result(OhplGa2Sanity *s, bool seenEnemy, float enemyDecrease, OhplGa2Ops *out);
void ohpl_ga2_sanity_enemy_seen(OhplGa2Sanity *s, float dt, OhplGa2Ops *out);
void ohpl_ga2_sanity_hit(OhplGa2Sanity *s, bool dead, float dt, OhplGa2Ops *out);
void ohpl_ga2_sanity_effects(OhplGa2Sanity *s, float sanity, bool dead, float dt, OhplGa2Ops *out);
bool ohpl_ga2_hands_update(int32_t state, bool hasObject, bool animOver, bool objectAnimOver, int32_t *newState, OhplGa2Ops *out);
bool ohpl_ga2_hands_action_gate(int32_t state, bool hasObject);
int32_t ohpl_ga2_hands_switch_plan(bool hasCurrent, bool hasNew);
bool ohpl_ga2_rotations_pop(size_t size, int32_t max);
void ohpl_ga2_hands_rotation(const hpl::cVector3f *p, size_t n, hpl::cVector3f *out);
void ohpl_ga2_hands_translation(const OhplGa2HandsPos *p, hpl::cVector3f *out);
}

// ---- GA2b: player and move states (rust/src/luxplayer/states_*.rs, move_normal.rs) ----

enum OhplGa2bAct : int32_t { kOhplGa2bActOther = 0, kOhplGa2bActHolster = 1, kOhplGa2bActAttack = 2, kOhplGa2bActInteract = 3, kOhplGa2bActLantern = 4 };

// Player state codes in OhplGa2bAction::change: 0 none, 1 normal, 2 hand object, 3 previous state.
enum OhplGa2bPs : int32_t { kOhplGa2bPsNone = 0, kOhplGa2bPsNormal = 1, kOhplGa2bPsHand = 2, kOhplGa2bPsPrevious = 3, kOhplGa2bPsPush = 4, kOhplGa2bPsGrab = 5, kOhplGa2bPsOther = 9 };

enum OhplGa2bPlan : int32_t { kOhplGa2bPlanNone = 0, kOhplGa2bPlanCallback = 1, kOhplGa2bPlanDoesNotWork = 2, kOhplGa2bPlanNoObject = 3 };
enum OhplGa2bDraw : int32_t { kOhplGa2bDrawNo = 0, kOhplGa2bDrawCheckCallback = 1, kOhplGa2bDrawYes = 2 };
enum OhplGa2bSound : int32_t { kOhplGa2bSoundNone = 0, kOhplGa2bSoundUp = 1, kOhplGa2bSoundDown = 2 };
enum OhplGa2bGrabStatus : int32_t { kOhplGa2bGrabTooFar = 0, kOhplGa2bGrabForce = 1, kOhplGa2bGrabForceTorque = 2 };
enum OhplGa2bBottom : int32_t { kOhplGa2bBottomNo = 0, kOhplGa2bBottomYes = 1, kOhplGa2bBottomCheckRays = 2 };

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

struct OhplGa2bGrabEnterIn
{
	hpl::cVector3f camPos, camRot;
	hpl::cMatrixf bodyMatrix;
	hpl::cVector3f bodyPos, focusPos, rotOffset;
	uint32_t useOffset, useDepth;
	float depth, maxDepth, properMassSum, minSlowMass, maxSlowMass, minSlowMul;
};

struct OhplGa2bGrabEnterOut
{
	hpl::cMatrixf bodyRot;
	hpl::cVector3f localOffset;
	float depth, maxDistance;
	uint32_t slow;
	float slowMul;
};

struct OhplGa2bGrabPost
{
	hpl::cVector3f camPos, camRot;
	hpl::cMatrixf bodyMatrix;
	hpl::cVector3f bodyPos, bodyAngVel;
	hpl::cMatrixf inertia, bodyRot;
	hpl::cVector3f localOffset, posOffset;
	float depth, maxDistance;
	uint32_t useOffset, useRotation;
	float massSum, maxForce, maxTorque, maxAngularSpeed, forceMul, torqueMul;
};

struct OhplGa2bGrabPostOut
{
	int32_t status;
	hpl::cVector3f force, torque;
};

struct OhplGa2bPushEnterIn
{
	float pitch, yaw, pitchMinRange, pitchMaxRange, yawRange;
	uint32_t atPoint;
	hpl::cMatrixf bodyMatrix;
	hpl::cVector3f bodyPos, feet, interactPos;
};

struct OhplGa2bPushEnterOut
{
	float pitchMin, pitchMax, yawMin, yawMax;
	hpl::cVector3f localAttach;
	float maxAttachDist;
};

struct OhplGa2bPushBegin
{
	uint32_t atPoint;
	hpl::cMatrixf bodyMatrix;
	hpl::cVector3f localAttach, bodyPos, feet;
	float maxAttachDist;
	hpl::cVector3f forward, right;
	uint32_t moving[2];
	float totalMoveMul[2];
};

struct OhplGa2bPushBeginOut
{
	uint32_t tooFar;
	hpl::cVector3f attach, pushDir;
	uint32_t noMovement;
};

struct OhplGa2bPushDrive
{
	hpl::cVector3f pushDir, vel;
	float mass, maxForce, maxSpeed, pushForce, forceMul;
	hpl::cMatrixf bodyMatrix;
	hpl::cVector3f forward, right;
};

struct OhplGa2bPushDriveOut
{
	hpl::cVector3f correctForce;
	uint32_t hasPushForce;
	hpl::cVector3f pushForce;
	hpl::cMatrixf nextTransform;
	float maxSpeed[2];
};

struct OhplGa2bSlideIn
{
	hpl::cVector3f camPos, up, right;
	hpl::cMatrixf bodyMatrix;
	hpl::cVector3f localInteract;
	hpl::cVector2f mouse;
	hpl::cVector3f pin, vel;
	float mass, maxDistance, slowDownFactor, speedFactor, maxSpeed, maxForce;
};

struct OhplGa2bSlideOut
{
	uint32_t outOfRange;
	hpl::cVector3f force;
};

struct OhplGa2bRotateTorqueIn
{
	hpl::cVector3f pin, angVel;
	hpl::cMatrixf inertia;
	float speedAdd, speedFactor, maxSpeed, maxTorque;
};

struct OhplGa2bJointGeom
{
	hpl::cMatrixf bodyMatrix;
	hpl::cVector3f massCentre, pivot, pin, up, right, forward;
	hpl::cVector2f mouse;
};

struct OhplGa2bLadderEnterIn
{
	hpl::cVector3f pos, start;
	float maxY, startYaw, pitch, yaw;
};

struct OhplGa2bLadderEnterOut
{
	float time;
	hpl::cVector3f posAdd;
	hpl::cVector2f rotAdd;
};

struct OhplGa2bLadderState
{
	int32_t state;
	float timeCount, stepCount, moveMul, leaveAtTop;
	uint32_t playedSound;
	hpl::cVector3f charPos, posAdd;
	hpl::cVector2f rotAdd;
};

struct OhplGa2bLadderAttachOut
{
	float pitchAdd, yawAdd;
	uint32_t done;
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
bool ohpl_ga2b_default_base_interact(int32_t act, bool pressed, bool canInteract, bool *interact);
int32_t ohpl_ga2b_interact_action(int32_t act, bool pressed);
bool ohpl_ga2b_ladder_action(int32_t act, bool pressed, bool *release);
bool ohpl_ga2b_focus_in_range(bool hasEntity, float dist, float maxFocus);
bool ohpl_ga2b_focus_can_interact(bool inRange, bool canInteract, bool disabled);
int32_t ohpl_ga2b_crosshair_pick(int32_t crosshair, int32_t defaultCrosshair, int32_t lastEnum, bool disabled, bool styleDefault);
bool ohpl_ga2b_outline_gate(bool hasEntity, bool isProp, float dist, float maxFocus);
bool ohpl_ga2b_show_outline(bool isItem, bool canInteract, bool disabled);
float ohpl_ga2b_use_item_max_focus(float maxFocus, float minUse);
int32_t ohpl_ga2b_use_item_focus(bool hasItem, bool hasEntity, bool hasMap, float dist, float max);
int32_t ohpl_ga2b_use_item_plan(bool callback);
int32_t ohpl_ga2b_use_item_draw(bool hasEntity, float dist, float max, bool isProp, bool isItem);
void ohpl_ga2b_pid_init(OhplGa2bPid3 *p, int32_t n, float kp, float ki, float kd);
void ohpl_ga2b_pid_reset(OhplGa2bPid3 *p);
void ohpl_ga2b_grab_enter(const OhplGa2bGrabEnterIn *i, OhplGa2bGrabEnterOut *o);
void ohpl_ga2b_grab_post(const OhplGa2bGrabPost *i, OhplGa2bPid3 *forcePid, OhplGa2bPid3 *torquePid, float dt, OhplGa2bGrabPostOut *o);
bool ohpl_ga2b_leave_limit(const hpl::cVector3f *v, float max, hpl::cVector3f *out);
float ohpl_ga2b_grab_scroll(float depth, float amount, float inc, float min, float max);
void ohpl_ga2b_grab_rotate(const hpl::cMatrixf *rot, bool pitch, float amount, hpl::cMatrixf *out);
void ohpl_ga2b_push_enter(const OhplGa2bPushEnterIn *i, OhplGa2bPushEnterOut *o);
void ohpl_ga2b_push_begin(const OhplGa2bPushBegin *i, OhplGa2bPushBeginOut *o);
void ohpl_ga2b_push_stop(OhplGa2bPid3 *pid, const hpl::cVector3f *vel, float mass, float maxForce, float dt, hpl::cVector3f *out);
void ohpl_ga2b_push_drive(OhplGa2bPid3 *pid, const OhplGa2bPushDrive *i, float dt, OhplGa2bPushDriveOut *o);
void ohpl_ga2b_push_pick(bool normalMove, bool running, bool crouching, const float *walk, const float *run, const float *crouch, float *out);
void ohpl_ga2b_slide_update(const OhplGa2bSlideIn *i, float *speed, OhplGa2bPid3 *pid, float dt, OhplGa2bSlideOut *o);
void ohpl_ga2b_slide_throw(const hpl::cVector3f *pin, const hpl::cVector3f *camFwd, float impulse, hpl::cVector3f *out);
float ohpl_ga2b_rotate_max_distance(const hpl::cVector3f *feet, const hpl::cVector3f *pivot);
bool ohpl_ga2b_rotate_far(const hpl::cVector3f *feet, const hpl::cVector3f *pivot, float max);
void ohpl_ga2b_rotate_slow(float *speed, const hpl::cVector2f *mouse, float slowFactor, float dt);
void ohpl_ga2b_rotate_torque(const OhplGa2bRotateTorqueIn *i, float *speed, OhplGa2bPid3 *pid, float dt, hpl::cVector3f *out);
float ohpl_ga2b_lever_speed_add(const OhplGa2bJointGeom *g);
float ohpl_ga2b_swing_door_speed_add(const OhplGa2bJointGeom *g);
void ohpl_ga2b_swing_door_throw(const OhplGa2bJointGeom *g, float impulse, hpl::cVector3f *jointForward, hpl::cVector3f *out);
bool ohpl_ga2b_wheel_pop(size_t size);
float ohpl_ga2b_wheel_speed_add(const hpl::cVector2f *hist, size_t n, const hpl::cVector2f *mouse, const hpl::cVector3f *pin,
	const hpl::cVector3f *forward);
void ohpl_ga2b_move_to_mouse(bool forward, float mul, float factor, hpl::cVector2f *mouse);
void ohpl_ga2b_ladder_enter(const OhplGa2bLadderEnterIn *i, OhplGa2bLadderEnterOut *o);
void ohpl_ga2b_ladder_attach(OhplGa2bLadderState *s, float dt, OhplGa2bLadderAttachOut *o);
int32_t ohpl_ga2b_ladder_move_pre(OhplGa2bLadderState *s, const hpl::cVector3f *pos, float up, float down, float dt);
int32_t ohpl_ga2b_ladder_move_post(OhplGa2bLadderState *s, float up, float down, float stepLen, float dt);
void ohpl_ga2b_ladder_ground_rays(const hpl::cVector3f *feet, const hpl::cVector3f *ladderFwd, float sizeX, hpl::cVector3f *out);
bool ohpl_ga2b_ladder_at_top(float moveMul, float y, float sizeY, float maxY);
int32_t ohpl_ga2b_ladder_at_bottom(float moveMul, float y, float sizeY, float minY);
void ohpl_ga2b_ladder_top_rays(const hpl::cVector3f *feet, hpl::cVector3f *out);
bool ohpl_ga2b_ladder_leave_tick(float *count, float dt);
void ohpl_ga2b_ladder_on_move(int32_t state, bool right, float mul, float *moveMul);
bool ohpl_ga2b_ladder_camera_free(int32_t state);
void ohpl_ga2b_ladder_head_limits(float yaw, float pitch, float *out);
}

#endif
