#ifndef LUX_RUST_LUX_PLAYER_A2_H
#define LUX_RUST_LUX_PLAYER_A2_H

// C ABI of rust/src/luxplayer/a2_*.rs: Rust-owned state embedded in the C++ objects, host callbacks for the rest.

#include "RustLuxPlayer.h"

struct OhplA2Core
{
	OhplGa2CamState cam;
	float mfHealth, mfSanity, mfLampOil, mfTerror;
	hpl::cVector2f mvHeadSpinSpeed;
	float mfFocusTextAlpha;
	float mfHeadSpinDamageSpeed, mfHeadSpinDeacc, mfTerrorIncSpeed, mfTerrorDecSpeed;
};

enum OhplA2Query : uint32_t
{
	kOhplA2QCamPitch = 1,
	kOhplA2QPitchMin = 2,
	kOhplA2QPitchMax = 3,
	kOhplA2QFreeCam = 4,
	kOhplA2QHardMode = 5,
	kOhplA2QTerrorEnemies = 6,
	kOhplA2QFocusHide = 7,
	kOhplA2QRandYaw = 8,
	kOhplA2QRandPitch = 9,
};

enum OhplA2HostOp : uint32_t
{
	kOhplA2HeadSpinCam = 200,
	kOhplA2TerrorSound = 201,
};

struct OhplA2Host
{
	void *ctx;
	void (*op)(void *ctx, uint32_t kind, int32_t i, float a, float b, float c, float d);
	float (*query)(void *ctx, uint32_t kind);
};

struct OhplA2bHost
{
	void *ctx;
	void (*op)(void *ctx, uint32_t kind, int32_t i, float a, float b, float c, float d);
	float (*query)(void *ctx, uint32_t kind, float a, float b);
	void (*fill)(void *ctx, uint32_t kind, float *buf);
};

enum OhplA2bQuery : uint32_t
{
	kOhplA2bQHealth = 1,
	kOhplA2bQDead = 2,
	kOhplA2bQSpeed = 3,
	kOhplA2bQSanity = 4,
	kOhplA2bQCamPitch = 5,
	kOhplA2bQCamYaw = 6,
	kOhplA2bQRand = 7,
	kOhplA2bQVoiceDone = 8,
	kOhplA2bQQueueNonEmpty = 9,
	kOhplA2bQHasCallback = 10,
	kOhplA2bQLanternActive = 11,
	kOhplA2bQHardMode = 12,
	kOhplA2bQFlashbackActive = 13,
	kOhplA2bQDrainDisabled = 14,
	kOhplA2bQEmotionFlash = 15,
	kOhplA2bQOil = 16,
	kOhplA2bQAllowed = 17,
	kOhplA2bQHasItem = 18,
	kOhplA2bQRandInt = 19,
};

enum OhplA2bFill : uint32_t
{
	kOhplA2bFGoalAngle = 1,
	kOhplA2bFBody = 2,
	kOhplA2bFLights = 3,
	kOhplA2bFDarkLevels = 4,
};

enum OhplA2bOp : uint32_t
{
	kOhplA2bAddHealth = 300,
	kOhplA2bAmbientPos = 301,
	kOhplA2bLanternMatrix = 302,
};

struct OhplA2bCamDir
{
	OhplGa2CamDir k;
	uint32_t mlCount;
	hpl::cVector2f mvList[256];
};

struct OhplA2bHurt
{
	OhplGa2Hurt k;
	float mfHealthRegainSpeed, mfHealthRegainLimit;
};

struct OhplA2bLookAt
{
	OhplGa2LookAt k;
	uint32_t mbActive;
	hpl::cVector3f mvTargetPos;
	float mfDestFovMul, mfFov, mfFovSpeed, mfFovMaxSpeed;
};

struct OhplA2bLight
{
	float mfExtendedLightLevel, mfNormalLightLevel, mfUpdateCount, mfRadiusAdd;
};

struct OhplA2bLantern
{
	OhplGa2Lantern k;
	uint32_t mbDisabled;
};

struct OhplA2bDark
{
	OhplGa2Darkness k;
	uint32_t mbActive;
};

enum OhplA2cQuery : uint32_t
{
	kOhplA2cHardActive = 20,
	kOhplA2cPerma = 21,
	kOhplA2cScan = 22,
	kOhplA2cLeanCollide = 23,
	kOhplA2cHudImages = 24,
};

enum OhplA2cFill : uint32_t
{
	kOhplA2cFHudImageSize = 20,
	kOhplA2cFHudCenter = 21,
};

enum OhplA2cOp : uint32_t
{
	kOhplA2cLeanRoll = 320,
	kOhplA2cLeanHead = 321,
};

struct OhplA2cLean
{
	float mfDir, mfDirAdd, mfMovement, mfRotation, mfMaxMovement, mfMaxRotation;
	uint32_t mbPressed;
};

struct OhplA2cSplash
{
	uint32_t mlImage;
	hpl::cVector3f mvPos;
	float mvSize[2];
	float mfAlpha, mfAlphaMul;
	hpl::cVector3f mvPosVel;
	float mvSizeVel[2];
	float mfAlphaVel, mfAlphaMoveStart;
};

enum { kOhplA2cSplashCap = 64 };

struct OhplA2cHud
{
	float mfFlashAlphaSpeed, mfFlashAlphaOutSpeed, mfFlashAlpha;
	uint32_t mbFlashActive, mlCount;
	OhplA2cSplash mvSplashes[kOhplA2cSplashCap];
};

struct OhplA2cSpsCfg
{
	float mfRadius, mfDensity, mfHeightFromFeet, mfHeightAddMin, mfHeightAddMax;
};

struct OhplA2cSpsHost
{
	void *ctx;
	size_t (*create)(void *ctx, const hpl::cVector3f *pos);
	bool (*exists)(void *ctx, size_t ps);
	void (*kill)(void *ctx, size_t ps);
	void (*destroy)(void *ctx, size_t ps);
	float (*rand)(void *ctx, float min, float max);
	void (*feet)(void *ctx, hpl::cVector3f *out);
};

struct OhplA2cSps;

extern "C" {
void ohpl_a2c_sanity_update(OhplGa2Sanity *s, const OhplA2bHost *h, float dt);
void ohpl_a2c_sanity_reset(OhplGa2Sanity *s);
void ohpl_a2c_death_update(OhplGa2Death *s, const OhplA2bHost *h, float dt);
void ohpl_a2c_death_reset(OhplGa2Death *s);
void ohpl_a2c_death_start(OhplGa2Death *s, bool crouching);
void ohpl_a2c_lean_update(OhplA2cLean *s, const OhplA2bHost *h, float dt);
void ohpl_a2c_lean_reset(OhplA2cLean *s);
void ohpl_a2c_lean_set(OhplA2cLean *s, float mul);
void ohpl_a2c_lean_add(OhplA2cLean *s, float add);
void ohpl_a2c_hud_reset(OhplA2cHud *s);
void ohpl_a2c_hud_flash(OhplA2cHud *s, float inTime, float outTime);
void ohpl_a2c_hud_update(OhplA2cHud *s, float dt);
void ohpl_a2c_hud_add_splash(OhplA2cHud *s, const OhplA2bHost *h, int32_t damageType);
OhplA2cSps *ohpl_a2c_sps_new();
void ohpl_a2c_sps_free(OhplA2cSps *s);
void ohpl_a2c_sps_start(OhplA2cSps *s, const OhplA2cSpsHost *h, OhplA2cSpsCfg cfg);
void ohpl_a2c_sps_reset(OhplA2cSps *s, const OhplA2cSpsHost *h, bool map);
void ohpl_a2c_sps_respawn(OhplA2cSps *s, const OhplA2cSpsHost *h);
void ohpl_a2c_sps_update(OhplA2cSps *s, const OhplA2cSpsHost *h);
void ohpl_a2c_sps_set_active(OhplA2cSps *s, bool active);
bool ohpl_a2c_sps_active(const OhplA2cSps *s);
size_t ohpl_a2c_sps_count(const OhplA2cSps *s);
}

extern "C" {
void ohpl_a2_vitals(OhplA2Core *c, const OhplA2Host *h, int32_t which, float amount, int32_t ty, uint32_t flags);
void ohpl_a2_spin_head(OhplA2Core *c, const OhplA2Host *h, float speed);
void ohpl_a2b_collapse_update(OhplGa2Collapse *s, const OhplA2bHost *h, float dt);
void ohpl_a2b_collapse_clear(OhplGa2Collapse *s, bool active, bool resetRandom);
void ohpl_a2b_camdir_update(OhplA2bCamDir *s, const OhplA2bHost *h, float dt);
float ohpl_a2b_camdir_add(OhplA2bCamDir *s, bool yaw, float x);
void ohpl_a2b_camdir_reset(OhplA2bCamDir *s);
void ohpl_a2b_hurt_update(OhplA2bHurt *s, const OhplA2bHost *h, float dt);
void ohpl_a2b_hurt_reset(OhplA2bHurt *s);
void ohpl_a2b_lookat_update(OhplA2bLookAt *s, const OhplA2bHost *h, float dt);
void ohpl_a2b_lookat_reset(OhplA2bLookAt *s);
void ohpl_a2b_lookat_set_target(OhplA2bLookAt *s, float x, float y, float z, float speedMul, float maxSpeed);
void ohpl_a2b_lookat_set_active(OhplA2bLookAt *s, bool active);
void ohpl_a2b_flashback_update(OhplGa2Flashback *s, const OhplA2bHost *h, float dt);
void ohpl_a2b_flashback_reset(OhplGa2Flashback *s);
void ohpl_a2b_light_update(OhplA2bLight *s, const OhplA2bHost *h, float dt);
void ohpl_a2b_light_reset(OhplA2bLight *s);
void ohpl_a2b_light_map_enter(OhplA2bLight *s);
void ohpl_a2b_dark_update(OhplA2bDark *s, const OhplA2bHost *h, float dt);
void ohpl_a2b_dark_reset(OhplA2bDark *s);
void ohpl_a2b_dark_map_enter(OhplA2bDark *s);
void ohpl_a2b_dark_set_active(OhplA2bDark *s, const OhplA2bHost *h, bool active);
void ohpl_a2b_lantern_update(OhplA2bLantern *s, const OhplA2bHost *h, float dt);
void ohpl_a2b_lantern_set_active(OhplA2bLantern *s, const OhplA2bHost *h, bool active, bool useEffects, bool checkOilItems, bool checkAllowed);
void ohpl_a2b_lantern_set_disabled(OhplA2bLantern *s, const OhplA2bHost *h, bool disabled);
void ohpl_a2b_lantern_reset(OhplA2bLantern *s);
}

// A2c: player dispatch (rust/src/luxplayer/a2d_*.rs)
struct OhplA2dDisp
{
	void *mpRing;
	int32_t mlState, mlMoveState, mlCoins, mlTinderboxes, mlMaxPrevSpeeds;
	float mfWaterSpeedMul, mfInteractionMoveSpeedMul, mfLookSpeedMul, mfEventMoveSpeedMul, mfEventRunSpeedMul,
		mfScriptMoveSpeedMul, mfScriptRunSpeedMul, mfHurtMoveSpeedMul, mfInsanityCollapseSpeedMul, mfScriptJumpForceMul,
		mfCurrentFocusDistance, mfFreeCameraSpeed, mfAvgSpeed, mfAutoKillYPos;
	hpl::cVector3f mvHeadPosAddSum;
	bool mbActive, mbNoFallDamage, mbUsesDragFootsteps, mbPressedMove, mbPressingRun, mbPressingJump, mbIsInWater,
		mbJumpDisabled, mbCrouchDisabled, mbSanityDrainDisabled, mbFreeCameraActive, mbScriptShowFocusIconAndCrossHair;
};
static_assert(sizeof(OhplA2dDisp) == 112, "OhplA2dDisp layout");

struct OhplA2dHost
{
	void *ctx;
	float (*call)(void *ctx, uint32_t kind, int32_t i, float a, float b, void *p);
};

enum OhplA2dKind : uint32_t
{
	kOhplA2dMsEnter = 1,
	kOhplA2dMsLeave = 2,
	kOhplA2dMsUpdate = 3,
	kOhplA2dMsOnMove = 4,
	kOhplA2dMsOnYaw = 5,
	kOhplA2dMsOnPitch = 6,
	kOhplA2dMsOnRun = 7,
	kOhplA2dMsOnJump = 8,
	kOhplA2dMsOnCrouch = 9,
	kOhplA2dMsMapEnter = 10,
	kOhplA2dMsOnDraw = 11,
	kOhplA2dStEnter = 20,
	kOhplA2dStLeave = 21,
	kOhplA2dStSetPrev = 22,
	kOhplA2dStUpdate = 23,
	kOhplA2dStPostUpdate = 24,
	kOhplA2dStOnMove = 25,
	kOhplA2dStOnYaw = 26,
	kOhplA2dStOnPitch = 27,
	kOhplA2dStOnScroll = 28,
	kOhplA2dStOnAction = 29,
	kOhplA2dStOnRun = 30,
	kOhplA2dStOnJump = 31,
	kOhplA2dStOnCrouch = 32,
	kOhplA2dStMapEnter = 33,
	kOhplA2dStMapLeave = 34,
	kOhplA2dStOnDraw = 35,
	kOhplA2dFreeMove = 40,
	kOhplA2dBodyMove = 41,
	kOhplA2dFreeYaw = 42,
	kOhplA2dFreePitch = 43,
	kOhplA2dYaw = 44,
	kOhplA2dPitch = 45,
	kOhplA2dLanternToggle = 46,
	kOhplA2dCollapseActive = 47,
	kOhplA2dHeadPosAdd = 48,
	kOhplA2dHelpersUpdate = 49,
	kOhplA2dHelpersPost = 50,
	kOhplA2dHelpersDraw = 51,
	kOhplA2dHelpersLuxMsg = 52,
	kOhplA2dCollisionCb = 53,
	kOhplA2dBodyY = 54,
	kOhplA2dBodySpeed = 55,
	kOhplA2dClearFocusText = 56,
	kOhplA2dNullFocus = 57,
	kOhplA2dDestroyCb = 58,
	kOhplA2dClearTerror = 59,
	kOhplA2dCamMapLeave = 60,
	kOhplA2dPauseActive = 61,
	kOhplA2dFocusCanInteract = 62,
	kOhplA2dDrawCrosshair = 63,
	kOhplA2dFocusTextState = 64,
	kOhplA2dDrawFocusText = 65,
	kOhplA2dFreecamCamera = 66,
	kOhplA2dShowCrosshair = 67,
};

extern "C" {
void ohpl_a2d_init(OhplA2dDisp *d, int32_t maxPrevSpeeds, float autoKillY);
void ohpl_a2d_free(OhplA2dDisp *d);
void ohpl_a2d_reset(OhplA2Core *c, OhplA2dDisp *d, const OhplA2dHost *dh);
void ohpl_a2d_update(OhplA2Core *c, OhplA2dDisp *d, const OhplA2Host *h, const OhplA2dHost *dh, float dt);
void ohpl_a2d_post_update(OhplA2dDisp *d, const OhplA2dHost *dh, float dt);
void ohpl_a2d_on_draw(OhplA2Core *c, OhplA2dDisp *d, const OhplA2dHost *dh, float dt);
void ohpl_a2d_move(OhplA2dDisp *d, const OhplA2dHost *dh, int32_t dir, float mul);
void ohpl_a2d_add_yaw(OhplA2dDisp *d, const OhplA2dHost *dh, float amount);
void ohpl_a2d_add_pitch(OhplA2dDisp *d, const OhplA2dHost *dh, float amount);
void ohpl_a2d_scroll(OhplA2dDisp *d, const OhplA2dHost *dh, float amount);
void ohpl_a2d_do_action(OhplA2dDisp *d, const OhplA2dHost *dh, int32_t action, bool pressed);
void ohpl_a2d_run(OhplA2dDisp *d, const OhplA2dHost *dh, bool pressed);
void ohpl_a2d_jump(OhplA2dDisp *d, const OhplA2dHost *dh, bool pressed);
void ohpl_a2d_crouch(OhplA2dDisp *d, const OhplA2dHost *dh, bool pressed);
void ohpl_a2d_change_state(OhplA2dDisp *d, const OhplA2dHost *dh, int32_t state);
void ohpl_a2d_change_move_state(OhplA2dDisp *d, const OhplA2dHost *dh, int32_t state);
void ohpl_a2d_on_leave_container(OhplA2dDisp *d, const OhplA2dHost *dh);
void ohpl_a2d_set_free_cam(OhplA2dDisp *d, const OhplA2dHost *dh, bool active);
void ohpl_a2d_map_enter(OhplA2Core *c, OhplA2dDisp *d, const OhplA2dHost *dh, void *map);
void ohpl_a2d_map_leave(OhplA2Core *c, OhplA2dDisp *d, const OhplA2dHost *dh, void *map);
}

struct OhplA2dHands
{
	void *mpRing;
	int32_t mlState, mlMaxRotations;
	float mfPosAddMul, mfCharge;
	int32_t mlObjectState;
	float mfAlpha;
	bool mbAttackDown, mbInteractDown;
};
static_assert(sizeof(OhplA2dHands) == 40, "OhplA2dHands layout");

enum OhplA2dHandsKind : uint32_t
{
	kOhplA2dHHasObject = 1,
	kOhplA2dHAnimOver = 2,
	kOhplA2dHObjAnimOver = 3,
	kOhplA2dHAttachDraw = 4,
	kOhplA2dHDisable = 5,
	kOhplA2dHIdle = 6,
	kOhplA2dHUpdateObjects = 7,
	kOhplA2dHObjDoAction = 8,
	kOhplA2dHSameObject = 9,
	kOhplA2dHHolster = 10,
	kOhplA2dHDrawNew = 11,
	kOhplA2dHSetCurrent = 12,
	kOhplA2dHResetPtrs = 13,
	kOhplA2dHHasEntity = 14,
	kOhplA2dHFillRot = 15,
	kOhplA2dHFillPos = 16,
	kOhplA2dHSetMatrix = 17,
};

extern "C" {
void ohpl_a2d_hands_init(OhplA2dHands *d, int32_t maxRotations, float posAddMul);
void ohpl_a2d_hands_free(OhplA2dHands *d);
void ohpl_a2d_hands_reset(OhplA2dHands *d, const OhplA2dHost *dh);
void ohpl_a2d_hands_clear_rotations(OhplA2dHands *d);
void ohpl_a2d_hands_update(OhplA2dHands *d, const OhplA2dHost *dh, float dt);
void ohpl_a2d_hands_do_action(OhplA2dHands *d, const OhplA2dHost *dh, int32_t action, bool pressed);
void ohpl_a2d_hands_set_object(OhplA2dHands *d, const OhplA2dHost *dh, void *obj);
void ohpl_a2d_hands_post_update(OhplA2dHands *d, const OhplA2dHost *dh);
}

#endif
