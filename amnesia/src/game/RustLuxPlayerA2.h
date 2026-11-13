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

extern "C" {
void ohpl_a2_vitals(OhplA2Core *c, const OhplA2Host *h, int32_t which, float amount, int32_t ty, uint32_t flags);
void ohpl_a2_spin_head(OhplA2Core *c, const OhplA2Host *h, float speed);
void ohpl_a2_update_camera(OhplA2Core *c, const OhplA2Host *h, float dt);
void ohpl_a2_update_head_spin(OhplA2Core *c, const OhplA2Host *h, float dt);
void ohpl_a2_update_terror(OhplA2Core *c, const OhplA2Host *h, float dt);
void ohpl_a2_update_focus_alpha(OhplA2Core *c, const OhplA2Host *h, float dt);
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
bool ohpl_a2b_dark_in_darkness(const OhplA2bDark *s, float ext);
void ohpl_a2b_lantern_update(OhplA2bLantern *s, const OhplA2bHost *h, float dt);
void ohpl_a2b_lantern_set_active(OhplA2bLantern *s, const OhplA2bHost *h, bool active, bool useEffects, bool checkOilItems, bool checkAllowed);
void ohpl_a2b_lantern_set_disabled(OhplA2bLantern *s, const OhplA2bHost *h, bool disabled);
void ohpl_a2b_lantern_reset(OhplA2bLantern *s);
}

#endif
