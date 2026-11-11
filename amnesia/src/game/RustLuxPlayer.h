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

#endif
