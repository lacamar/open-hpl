#ifndef SOMA_RUST_AGENT_S4_H
#define SOMA_RUST_AGENT_S4_H

// C ABI of rust/src/somaagent/s4.rs

#include <cstddef>
#include <cstdint>

struct OhplS4Str
{
	const unsigned char *ptr;
	size_t len;
};

struct OhplS4Track
{
	OhplS4Str node;
	float min_wait, max_wait;
	OhplS4Str anim;
	bool loop_anim;
};

struct OhplS4PathPod
{
	bool moving, exact, in_map, track_active, track_paused, track_loop, at_node;
	float goal[3];
	int track_idx;
	float track_wait, track_freq;
};

struct OhplS4DebugIn
{
	bool has_sm, has_pf, has_cm;
	bool pf_moving, pf_track_active, pf_paused, pf_at_node;
	bool cm_moving, cm_turning, cm_wall, cm_object_avoid;
	OhplS4Str sm_cur, sm_prev, sm_next, cm_anim;
	int sm_timers, pf_path_idx, pf_path_len, pf_track_idx, pf_track_len, cm_speed_state, cm_idle_extra_count, cm_wall_hits;
	float pf_goal[3], cm_goal[3];
	float pf_wait, cm_fwd, cm_stuck, cm_idle_extra_wait, cm_roll;
};

extern "C"
{
	void ohpl_s4_path_save(const OhplS4PathPod *pod, OhplS4Str result, OhplS4Str end_cb, OhplS4Str track_cb, const OhplS4Track *tracks, size_t ntracks, void *ctx,
						   void (*sink)(void *, const unsigned char *, size_t));
	void ohpl_s4_path_load(const unsigned char *data, size_t len, OhplS4PathPod *pod, void *ctx, void (*on_str)(void *, uint32_t, const unsigned char *, size_t),
						   void (*on_track)(void *, const OhplS4Track *));
	void ohpl_s4_nav_path_text(const float *nodes, size_t n, void *ctx, void (*sink)(void *, const unsigned char *, size_t));
	void ohpl_s4_debug_text(const OhplS4DebugIn *d, void *ctx, void (*sink)(void *, const unsigned char *, size_t));
}

#endif
