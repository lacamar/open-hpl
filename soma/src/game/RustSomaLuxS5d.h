#ifndef SOMA_RUST_SOMA_LUX_S5D_H
#define SOMA_RUST_SOMA_LUX_S5D_H

// C ABI of rust/src/somalux/s5d_player.rs: cSomaLuxPlayer. The blocks are the script-visible fields
// AngelScript reads at C++ object offsets; Rust owns their layout and values.

#include "RustSomaLuxS5.h"

#include <cstddef>
#include <cstdint>

struct OhplS5dStateBlock
{
	int32_t mlId;
	void *mpPlayer;
};

struct OhplS5dMoveBlock
{
	int32_t mlId;
	void *mpPlayer;
	float mfMaxForwardSpeed, mfMaxBackwardSpeed, mfMaxSidwaySpeed, mfForwardAcc, mfSidewayAcc, mfForwardDeacc, mfSidewayDeacc,
		mfForwardOppositeDirAccMul, mfSidewayOppositeDirAccMul, mfMaxForwardSpeedMul, mfMaxBackwardSpeedMul, mfMaxSidwaySpeedMul;
};

struct OhplS5dFade
{
	float mfValue, mfGoal, mfSpeed, mfMaxSpeed, mfSpeedMul;
};

struct OhplS5dArg
{
	int32_t kind;
	int32_t i;
	float f;
	void *p;
};

static_assert(sizeof(OhplS5dStateBlock) == 16 && offsetof(OhplS5dStateBlock, mpPlayer) == 8);
static_assert(sizeof(OhplS5dMoveBlock) == 64 && offsetof(OhplS5dMoveBlock, mpPlayer) == 8 && offsetof(OhplS5dMoveBlock, mfMaxSidwaySpeedMul) == 60);
static_assert(sizeof(OhplS5dFade) == 20 && sizeof(OhplS5dArg) == 24);

// OhplS5dArg kinds
enum : int32_t { kOhplS5dArg_DWORD, kOhplS5dArg_FLOAT, kOhplS5dArg_BYTE, kOhplS5dArg_ADDRESS, kOhplS5dArg_OBJECT, kOhplS5dArg_STRING };

// Host ops (OhplS5Host, OhplS5Io): the scriptables, camera, character body, map and entities the player reaches
enum OhplS5dOp : uint32_t
{
	kOhplS5d_DUMP_ENABLED = 1, kOhplS5d_DUMP_LOG, kOhplS5d_CFG_FLOAT, kOhplS5d_NEW_STATE, kOhplS5d_LOAD_STATE, kOhplS5d_DELETE, kOhplS5d_CALL,
	kOhplS5d_BASE, kOhplS5d_MAP_ACTIVE, kOhplS5d_TRANSFER, kOhplS5d_CAM_GET, kOhplS5d_CAM_SET, kOhplS5d_BODY_GET, kOhplS5d_BODY_SET,
	kOhplS5d_LIQUID_SURFACE, kOhplS5d_LIQUID_COUNT, kOhplS5d_LIQUID_ID, kOhplS5d_LIQUID_SET, kOhplS5d_LIQUID_GET, kOhplS5d_ADD_COLLIDE,
	kOhplS5d_REMOVE_COLLIDE, kOhplS5d_HAS_COLLIDE, kOhplS5d_CHECK_COLLIDE, kOhplS5d_DEAD_BROADCAST, kOhplS5d_ERROR_LOG,
};

// kOhplS5d_BASE sub-ops: cSomaLuxScriptable members called non-virtually
enum : int32_t { kOhplS5dBase_UPDATE, kOhplS5dBase_POST_UPDATE, kOhplS5dBase_VARIABLE_UPDATE, kOhplS5dBase_GUI, kOhplS5dBase_MESSAGE, kOhplS5dBase_MAP_MESSAGE,
				 kOhplS5dBase_ACTION, kOhplS5dBase_ANALOG };

extern "C" {
	void ohpl_s5d_install_host(const OhplS5Host *host);
	void ohpl_s5d_new(void *shell);
	void ohpl_s5d_free();
	void ohpl_s5d_register(void *engine, int32_t state_block_off, int32_t move_block_off, int32_t state_name_off, int32_t move_name_off);

	void ohpl_s5d_set_camera(void *camera);
	void *ohpl_s5d_camera();
	void ohpl_s5d_set_body(void *body);
	void *ohpl_s5d_body();
	bool ohpl_s5d_active();
	void ohpl_s5d_set_active(bool on);
	void ohpl_s5d_add_state(const unsigned char *name, size_t n, int32_t id, const unsigned char *file, size_t fn, const unsigned char *cls, size_t cn, bool move);
	void ohpl_s5d_change_state(int32_t id, bool move);
	void *ohpl_s5d_state(bool move);
	int32_t ohpl_s5d_state_id(bool move);
	void ohpl_s5d_place_at_start(const float *feet, float yaw, bool crouching);
	void ohpl_s5d_hit_ground(void *body, const float *vel);

	void ohpl_s5d_update(float dt);
	void ohpl_s5d_post_update(float dt);
	void ohpl_s5d_variable_update(float dt);
	void ohpl_s5d_gui(float dt);
	void ohpl_s5d_message(const unsigned char *decl, size_t n);
	void ohpl_s5d_map_message(const unsigned char *decl, size_t n, void *map);
	void ohpl_s5d_action(int32_t action, bool pressed);
	void ohpl_s5d_analog(int32_t id, const float *amount);

	float ohpl_s5d_health();
	void ohpl_s5d_set_health_raw(float x);
	void ohpl_s5d_set_health(float x);
	bool ohpl_s5d_is_dead();
	bool ohpl_s5d_give_damage(float amount, int32_t type, float min_health, const void *source, int32_t damage_id);
	float ohpl_s5d_visibility_range_mul();
	float ohpl_s5d_visibility_max_range();
	void ohpl_s5d_fov_mul(OhplS5dFade *out, bool aspect);
	void ohpl_s5d_set_fov_mul(const OhplS5dFade *in, bool aspect);
}

#endif
