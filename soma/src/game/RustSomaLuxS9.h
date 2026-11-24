#ifndef SOMA_RUST_SOMA_LUX_S9_H
#define SOMA_RUST_SOMA_LUX_S9_H

// C ABI of rust/src/somalux/s9_map.rs: map timers, map-change requests, collide groups, preload/pause state.

#include "RustSomaLux.h"

#include <cstddef>
#include <cstdint>

struct OhplS9Pending
{
	const unsigned char *map;
	size_t map_n;
	const unsigned char *start;
	size_t start_n;
	const unsigned char *transfer;
	size_t transfer_n;
};

struct OhplS9Timer
{
	const unsigned char *name;
	size_t name_n;
	const unsigned char *func;
	size_t func_n;
	const unsigned char *user;
	size_t user_n;
	float time, user_float, length;
	int32_t user_int;
	bool paused, live;
};

// ops of the game host table (OhplS5Host) that S9 adds
enum OhplS9Op : uint32_t
{
	kOhplS9_FIRE = 101, kOhplS9_STR_REF, kOhplS9_INIT_CONFIG, kOhplS9_EXIT, kOhplS9_SCRIPT_PAUSED, kOhplS9_SET_PAUSED, kOhplS9_PRELOAD_READY,
	kOhplS9_HIDE_CROSSHAIR, kOhplS9_UPDATES_PER_SEC, kOhplS9_ARR_PUSH_STR, kOhplS9_ARR_RESIZE, kOhplS9_ARR_SET, kOhplS9_APP_TIME, kOhplS9_SLEEP, kOhplS9_GAME_TIME,
	kOhplS9_MAP, kOhplS9_ENT,
};

// sub-ops in io->i[0] of kOhplS9_MAP (io->p[0] = map) and kOhplS9_ENT (io->p[0] = entity); rust/src/somalux/map_loop.rs
enum
{
	kOhplS9Map_DUMP_ENTER, kOhplS9Map_DISSOLVE, kOhplS9Map_CALL_F, kOhplS9Map_CRITTER_FRAME, kOhplS9Map_LIGHT_CONN, kOhplS9Map_SOUNDSCAPE,
	kOhplS9Map_PENDING, kOhplS9Map_TICK, kOhplS9Map_LOOK_CAMERA, kOhplS9Map_LOOKAT_CALL,
};
enum
{
	kOhplS9Ent_HAS_SCRIPT, kOhplS9Ent_TIMERS, kOhplS9Ent_CALL_F, kOhplS9Ent_AUTOSLEEP, kOhplS9Ent_CRITTER, kOhplS9Ent_COMPONENTS, kOhplS9Ent_AGENT,
	kOhplS9Ent_CHECK_COLLISION, kOhplS9Ent_LIQUID, kOhplS9Ent_ANIMATION, kOhplS9Ent_SOCKETS, kOhplS9Ent_MOVE, kOhplS9Ent_CHAR_COLLISION,
	kOhplS9Ent_PROC_ANIMATIONS, kOhplS9Ent_EFFECT_COLOR, kOhplS9Ent_LIQUID_AMOUNT, kOhplS9Ent_GUI, kOhplS9Ent_ATTACHMENT, kOhplS9Ent_LOOKS_AT,
	kOhplS9Ent_COLL_NUM, kOhplS9Ent_COLL_GET, kOhplS9Ent_COLL_REMOVE, kOhplS9Ent_COLL_WAS, kOhplS9Ent_COLL_NOW, kOhplS9Ent_COLL_SWAP, kOhplS9Ent_COLL_CALL,
};

struct OhplSoRoster;

extern "C" {
	OhplGs3Preload *ohpl_s9_preload_state();
	OhplGs3Pause *ohpl_s9_pause_state();
	bool ohpl_s9_collide_match(uint32_t a, uint32_t b);
	uint32_t ohpl_s9_collide_flag(const unsigned char *s, size_t n);
	void ohpl_s9_request_map_change(const unsigned char *a, size_t an, const unsigned char *b, size_t bn, bool new_game);
	bool ohpl_s9_take_pending(OhplS9Pending *out);
	bool ohpl_s9_take_new_game();
	float ohpl_s9_take_fade();
	void ohpl_s9_set_transfer(bool on);
	bool ohpl_s9_is_transfer();
	void ohpl_s9_set_preload_map(const unsigned char *s, size_t n);
	const unsigned char *ohpl_s9_preload_map(size_t *n);
	void ohpl_s9_uwp_clear();
	size_t ohpl_s9_uwp_list(size_t *out, size_t cap);
	void ohpl_s9_register_natives(void *engine);
	void ohpl_s9_map_free(size_t map);
	double ohpl_s9_map_time(size_t map);
	int32_t ohpl_s9_map_stamp(size_t map);
	bool ohpl_s9_timer_get(size_t map, size_t i, OhplS9Timer *out);
	void ohpl_s9_timers_clear(size_t map);
	void ohpl_s9_timer_push_back(size_t map, const OhplS9Timer *t);
	void ohpl_slx_map_update(size_t map, const OhplSoRoster *roster, float dt);
	void ohpl_slx_map_post_update(size_t map, OhplSoRoster *roster, float dt);
}

static_assert(sizeof(OhplS9Timer) == 72);

#endif
