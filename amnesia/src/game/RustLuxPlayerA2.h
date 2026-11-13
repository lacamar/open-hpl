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

extern "C" {
void ohpl_a2_vitals(OhplA2Core *c, const OhplA2Host *h, int32_t which, float amount, int32_t ty, uint32_t flags);
void ohpl_a2_spin_head(OhplA2Core *c, const OhplA2Host *h, float speed);
void ohpl_a2_update_camera(OhplA2Core *c, const OhplA2Host *h, float dt);
void ohpl_a2_update_head_spin(OhplA2Core *c, const OhplA2Host *h, float dt);
void ohpl_a2_update_terror(OhplA2Core *c, const OhplA2Host *h, float dt);
void ohpl_a2_update_focus_alpha(OhplA2Core *c, const OhplA2Host *h, float dt);
}

#endif
