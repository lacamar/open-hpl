#ifndef LUX_RUST_PROP_A5_H
#define LUX_RUST_PROP_A5_H

// C ABI of rust/src/luxprop/a5_ffi.rs. The state structs live inside the C++ props; ops go through
// the OhplGa4Sink of RustLuxProp.h and are applied by the prop's Ga4ApplyOp.

#include "RustLuxProp.h"

struct OhplA5Button
{
	bool on;
	bool can_on;
	bool can_off;
};

struct OhplA5Photocell
{
	bool lit;
	float light_level;
	float check_count;
	float on_limit;
	float off_limit;
};

struct OhplA5Door
{
	bool locked;
	bool show_stats;
	float locked_count;
};

struct OhplA5Oil
{
	bool calculated;
	float amount;
};

struct OhplA5PhotoHost
{
	void *ctx;
	void (*bounds)(void *ctx, float *size, float *center);
	float (*light_at)(void *ctx, const float *pos);
	bool (*illumination)(void *ctx, float *out);
};

struct OhplA5OilHost
{
	void *ctx;
	bool (*hard_mode)(void *ctx);
	float (*lamp_oil)(void *ctx);
	float (*total_oil)(void *ctx);
};

enum
{
	kOhplA5Op_ConnState = 1001,
	kOhplA5Op_SetEffects = 1002,
	kOhplA5Op_PhotoIllum = 1003,
	kOhplA5Op_DoorLockedFeedback = 1004,
	kOhplA5Op_DoorChangeMap = 1005,
	kOhplA5Op_OilSound = 1006,
	kOhplA5Op_OilMessage = 1007,
	kOhplA5Op_OilSetLamp = 1008,

	kOhplA5Msg_Empty = 0,
	kOhplA5Msg_LanternFull = 1,
	kOhplA5Msg_RefillFull = 2,
	kOhplA5Msg_Refill025 = 3,
	kOhplA5Msg_Refill050 = 4,
	kOhplA5Msg_Refill075 = 5,
	kOhplA5Msg_Refill100 = 6,

	kOhplA5Snd_Empty = 0,
	kOhplA5Snd_Fill = 1,

	kOhplA5Slot_DoorEnter = 200,
	kOhplA5Slot_DoorExit = 201,
	kOhplA5Slot_DoorTextEntry = 202,
	kOhplA5Slot_DoorMapFile = 203,
	kOhplA5Slot_DoorStartPos = 204,
	kOhplA5Slot_DoorLockedSound = 205,
	kOhplA5Slot_DoorLockedCat = 206,
	kOhplA5Slot_DoorLockedEntry = 207,
	kOhplA5Slot_OilFill = 208,
	kOhplA5Slot_OilEmpty = 209,
};

static_assert(sizeof(OhplA5Button) == 3, "OhplA5Button layout");
static_assert(sizeof(OhplA5Photocell) == 20, "OhplA5Photocell layout");
static_assert(sizeof(OhplA5Door) == 8, "OhplA5Door layout");
static_assert(sizeof(OhplA5Oil) == 8, "OhplA5Oil layout");

extern "C"
{
	void ohpl_a5_button_init(OhplA5Button *st);
	void ohpl_a5_button_load_instance(const OhplGa4Vars *v, OhplA5Button *st, const OhplGa4Sink *sink);
	bool ohpl_a5_button_can_interact(const OhplA5Button *st);
	void ohpl_a5_button_interact(OhplA5Button *st, const OhplGa4Sink *sink);
	void ohpl_a5_button_set_on(OhplA5Button *st, bool on, bool effects, const OhplGa4Sink *sink);
	void ohpl_a5_button_conn(OhplA5Button *st, int state, const OhplGa4Sink *sink);
	void ohpl_a5_photocell_load_instance(const OhplGa4Vars *v, OhplA5Photocell *st);
	void ohpl_a5_photocell_update(OhplA5Photocell *st, float dt, const OhplA5PhotoHost *host, const OhplGa4Sink *sink);
	void ohpl_a5_door_load(const OhplGa4Vars *v);
	void ohpl_a5_door_load_instance(const OhplGa4Vars *v, OhplA5Door *st);
	void ohpl_a5_door_update(OhplA5Door *st, float dt);
	void ohpl_a5_door_interact(OhplA5Door *st, const OhplGa4Sink *sink);
	void ohpl_a5_oil_load(const OhplGa4Vars *v);
	int ohpl_a5_oil_focus_tier(OhplA5Oil *st, const OhplA5OilHost *host);
	bool ohpl_a5_oil_interact(OhplA5Oil *st, const OhplA5OilHost *host, const OhplGa4Sink *sink);
}

#endif
