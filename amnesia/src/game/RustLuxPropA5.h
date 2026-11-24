#ifndef LUX_RUST_PROP_A5_H
#define LUX_RUST_PROP_A5_H

// C ABI of rust/src/luxprop/a5_ffi.rs. The state structs live inside the C++ props; ops go through
// the OhplGa4Sink of RustLuxProp.h and are applied by the prop's Ga4ApplyOp.

#include "RustLuxProp.h"

struct OhplA5Door
{
	bool locked;
	bool show_stats;
	float locked_count;
};

struct OhplPropRs;

enum
{
	kOhplPrsButton = 0,
	kOhplPrsPhotocell = 1,
	kOhplPrsEmotion = 2,
	kOhplPrsOil = 3,
	kOhplPrsLever = 4,
	kOhplPrsWheel = 5,
	kOhplPrsMulti = 6,

	kOhplPrsStrDescCat = 0,
	kOhplPrsStrDescEntry = 1,
	kOhplPrsStrTextCat = 2,
	kOhplPrsStrTextEntry = 3,
	kOhplPrsStrSound = 4,
	kOhplPrsStrOilFill = 5,
	kOhplPrsStrOilEmpty = 6,
	kOhplPrsStrMinLimit = 0,
	kOhplPrsStrMaxLimit = 1,
	kOhplPrsStrStuck = 2,
	kOhplPrsStrChangeSound = 0,
	kOhplPrsStrMultiStuck = 1,
	kOhplPrsStrMultiCallback = 2,
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

static_assert(sizeof(OhplA5Door) == 8, "OhplA5Door layout");

extern "C"
{
	OhplPropRs *ohpl_prs_new(int kind);
	void ohpl_prs_free(OhplPropRs *p);
	void ohpl_prs_load_vars(OhplPropRs *p, const OhplGa4Vars *v);
	bool ohpl_prs_move_data(OhplPropRs *p, float *out);
	void ohpl_prs_load_instance(OhplPropRs *p, const OhplGa4Vars *v, const OhplGa4Host *host, const OhplGa4Sink *sink);
	bool ohpl_prs_can_interact(OhplPropRs *p, float mass);
	int ohpl_prs_crosshair(OhplPropRs *p);
	bool ohpl_prs_flag(OhplPropRs *p, int which);
	bool ohpl_prs_interact(OhplPropRs *p, const OhplA5OilHost *oil, const OhplGa4Sink *sink);
	bool ohpl_prs_joint_interact(OhplPropRs *p, bool show_hints, const OhplGa4Host *host, const OhplGa4Sink *sink);
	int ohpl_prs_focus(OhplPropRs *p, const OhplA5OilHost *oil);
	void ohpl_prs_update(OhplPropRs *p, float dt, const OhplA5PhotoHost *photo, const OhplGa4Sink *sink);
	void ohpl_prs_joint_update(OhplPropRs *p, float dt, bool interacted, const OhplGa4Host *host, const OhplGa4Sink *sink);
	void ohpl_prs_setup(OhplPropRs *p, float min, float max, const OhplGa4Sink *sink);
	void ohpl_prs_conn(OhplPropRs *p, int state, const OhplGa4Host *host, const OhplGa4Sink *sink);
	void ohpl_prs_set_on(OhplPropRs *p, bool on, bool effects, const OhplGa4Sink *sink);
	void ohpl_prs_set_can(OhplPropRs *p, bool off, bool v);
	void ohpl_prs_set_stuck(OhplPropRs *p, int state, bool effects, const OhplGa4Host *host, const OhplGa4Sink *sink);
	int ohpl_prs_stuck_state(OhplPropRs *p);
	int ohpl_prs_lever_state(OhplPropRs *p);
	void ohpl_prs_set_disables(OhplPropRs *p, bool v);
	void ohpl_prs_set_angle(OhplPropRs *p, float x, bool auto_move, const OhplGa4Sink *sink);
	void ohpl_prs_wheel(OhplPropRs *p, float *out);
	const unsigned char *ohpl_prs_str(OhplPropRs *p, int i, size_t *n);
	void ohpl_prs_set_str(OhplPropRs *p, int i, const unsigned char *s, size_t n);
	void ohpl_prs_save(OhplPropRs *p, void *d);
	int ohpl_prs_load(OhplPropRs *p, void *d, int phase);
	void ohpl_a5_door_load(const OhplGa4Vars *v);
	void ohpl_a5_door_load_instance(const OhplGa4Vars *v, OhplA5Door *st);
	void ohpl_a5_door_update(OhplA5Door *st, float dt);
	void ohpl_a5_door_interact(OhplA5Door *st, const OhplGa4Sink *sink);
}

#endif
