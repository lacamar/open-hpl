#ifndef SOMA_RUST_SOMA_ENTITY_H
#define SOMA_RUST_SOMA_ENTITY_H

// C ABI of rust/src/somaent/ffi.rs (layouts: rust/src/somaent/{motion,procanim,geom}.rs).

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"

struct OhplGs1Move
{
	bool moving = false;
	hpl::cVector3f goal = hpl::cVector3f(0);
	float acc = 0, max_speed = 0, slowdown = 0, speed = 0;
};

struct OhplGs1Rotate
{
	int mode = 0; // 1 align to goal, 2 constant speed
	hpl::cMatrixf goal = hpl::cMatrixf::Identity;
	hpl::cVector3f axis = hpl::cVector3f(0), pivot = hpl::cVector3f(0);
	float acc = 0, max_speed = 0, slowdown = 0, speed = 0;
};

struct OhplGs1Proc
{
	bool active = false, looping = false;
	float time = 0, speed = 0, speed_target = 0, speed_step = 0, amount = 1, amount_step = 0;
};

struct OhplGs1Obb
{
	hpl::cVector3f center;
	hpl::cVector3f axis[3];
	hpl::cVector3f half;
};

static_assert(sizeof(OhplGs1Move) == 32 && offsetof(OhplGs1Move, goal) == 4, "OhplGs1Move layout");
static_assert(sizeof(OhplGs1Rotate) == 108 && offsetof(OhplGs1Rotate, goal) == 4, "OhplGs1Rotate layout");
static_assert(sizeof(OhplGs1Proc) == 28 && offsetof(OhplGs1Proc, time) == 4, "OhplGs1Proc layout");
static_assert(sizeof(OhplGs1Obb) == 60, "OhplGs1Obb layout");

struct OhplGs1VarsOps
{
	void (*get_string)(void *ctx, const unsigned char *name, size_t n, void (*out)(void *, const unsigned char *, size_t), void *out_ctx);
	bool (*get_bool)(void *ctx, const unsigned char *name, size_t n, bool def);
	float (*get_float)(void *ctx, const unsigned char *name, size_t n, float def);
	int (*get_int)(void *ctx, const unsigned char *name, size_t n, int def);
};

struct OhplGs1Vars
{
	const OhplGs1VarsOps *ops;
	void *ctx;
};

// kind: 0 interact callback (s0, b0 auto remove), 1 look-at (s0, b0 auto remove, b1 check centre, b2 check ray, f0 max
// distance, f1 delay), 2 collide callback (s0 child, s1 func, b0 on the player), 3 user var (s0), 4 connection callback
// (s0), 5 connection (s0 entity, b0 invert, i0 states used), 6 health (f0), 7 breakable, 8 max interact distance (f0),
// 9 interaction disabled
struct OhplGs1Op
{
	unsigned kind;
	const unsigned char *s0;
	size_t n0;
	const unsigned char *s1;
	size_t n1;
	float f0, f1;
	bool b0, b1, b2;
	int i0;
};

enum
{
	kOhplGs1EffectsUpdateMesh = 1,
	kOhplGs1EffectsUpdateEffects = 2,
	kOhplGs1LiquidIdle = 0,
	kOhplGs1LiquidSnap = 1,
	kOhplGs1LiquidMoved = 2,
	kOhplGs1MoveSoundKeep = 0,
	kOhplGs1MoveSoundReposition = 1,
	kOhplGs1MoveSoundSwitch = 2,
	kOhplGs1AnimNone = 0,
	kOhplGs1AnimNext = 1,
	kOhplGs1AnimFinish = 2,
	kOhplGs1AnimFinishCallback = 3,
	kOhplGs1PlayNone = 0,
	kOhplGs1PlayMissingError = 1,
	kOhplGs1PlayFade = 2,
	kOhplGs1PlayNow = 3,
	kOhplGs1StopClearCurrent = 1,
	kOhplGs1StopFade = 2,
	kOhplGs1StopOff = 4,
};

extern "C" {
	void ohpl_gs1_move_start(OhplGs1Move *, const hpl::cVector3f *goal, float acc, float max_speed, float slowdown, bool reset);
	int ohpl_gs1_move_step(OhplGs1Move *, float dt, const hpl::cVector3f *pos, hpl::cVector3f *out);
	void ohpl_gs1_move_stop(OhplGs1Move *);
	void ohpl_gs1_rotate_align(OhplGs1Rotate *, const hpl::cMatrixf *goal, float acc, float max_speed, float slowdown, bool reset, const hpl::cVector3f *pivot);
	void ohpl_gs1_rotate_speed(OhplGs1Rotate *, float acc, float goal_speed, const hpl::cVector3f *axis, bool reset, const hpl::cVector3f *pivot);
	void ohpl_gs1_rotate_stop(OhplGs1Rotate *);
	int ohpl_gs1_rotate_step(OhplGs1Rotate *, float dt, const hpl::cMatrixf *m, hpl::cMatrixf *out);
	bool ohpl_gs1_static_move_enter(bool moving, bool flag, bool loop_present);
	int ohpl_gs1_static_move_plan(bool moving, bool flag, bool loop_alive);
	bool ohpl_gs1_is_moving(bool moving, int rotate_mode, float rotate_max_speed);
	bool ohpl_gs1_char_stopped_step(size_t n, const bool *collide, const bool *default_collide, const float *lin_sqr, const float *ang_sqr, bool *clear);
	bool ohpl_gs1_outside_push(const hpl::cVector3f *body_pos, const hpl::cVector3f *player_pos, const hpl::cVector3f *lin_vel, hpl::cVector3f *out);

	float ohpl_gs1_effects_fade_setup(bool on, float fade_time, float off_alpha, float *alpha);
	void ohpl_gs1_effect_color(const hpl::cColor *def, const hpl::cColor *base, float alpha, hpl::cColor *out);
	bool ohpl_gs1_effects_on(float alpha, bool active);
	bool ohpl_gs1_light_lit(const hpl::cColor *col, bool on);
	unsigned ohpl_gs1_effects_update_fades(float *mesh_alpha, float mesh_speed, bool has_mesh, float *alpha, float *speed, float off_alpha, float dt);
	bool ohpl_gs1_color_fade_begin(float time, float *color_time, float *color_t);
	bool ohpl_gs1_color_fade_step(const hpl::cColor *from, const hpl::cColor *to, float *color_time, float *color_t, float dt, hpl::cColor *out);
	bool ohpl_gs1_liquid_fade(float target, float time, float amount, float *cur_target, float *speed);
	int ohpl_gs1_liquid_update(float dt, float *amount, float target, float speed);

	void ohpl_gs1_proc_play(OhplGs1Proc *, float length, bool looping, float amount_fade, float speed_fade);
	bool ohpl_gs1_proc_step(OhplGs1Proc *, float dt);
	void ohpl_gs1_proc_track(const OhplGs1Proc *, bool rotate, bool reverse, const hpl::cVector3f *axes, float min, float max, int cycles,
							 const unsigned char *easing, size_t easing_len, hpl::cMatrixf *out);

	float ohpl_gs1_damage_amount(float amount, int strength, int toughness);
	bool ohpl_gs1_health_breaks(float health, bool disable_breakable);
	bool ohpl_gs1_break_gate(bool breakable, bool broken, bool active, float speed, bool has_map, double time, double start);
	bool ohpl_gs1_break_energy_exceeds(float speed, float mass, float other_mass, float v1, float v2, float min_energy);
	int ohpl_gs1_anim_update_action(bool has_mesh, int current, bool playing, bool queued, bool has_callback);
	int ohpl_gs1_play_plan(bool has_mesh, int idx, bool name_empty, int num_states, float fade);
	int ohpl_gs1_stop_plan(bool has_mesh, int idx, int num_states, int current, bool active, float fade);
	float ohpl_gs1_max_interact_distance(float own, float def);
	bool ohpl_gs1_connection_state(int state, int states_used, bool invert, int *out);
	float ohpl_gs1_interact_connection_input(float t, bool invert);
	bool ohpl_gs1_body_name_matches(const unsigned char *body, size_t body_len, const unsigned char *want, size_t want_len);
	void ohpl_gs1_gui_fade_step(float *fade, float *speed, bool *active, float dt);
	bool ohpl_gs1_gui_gate(bool focused, float fps, bool update_out_of_view, bool has_sub_mesh, bool sub_mesh_rendered, float *acc, bool *dirty, float dt,
						   float *step);

	void ohpl_gs1_aabb_to_obb(const hpl::cVector3f *min, const hpl::cVector3f *max, OhplGs1Obb *out);
	void ohpl_gs1_matrix_obb(const hpl::cMatrixf *m, const hpl::cVector3f *min, const hpl::cVector3f *max, OhplGs1Obb *out);
	bool ohpl_gs1_point_in_obb(const OhplGs1Obb *b, const hpl::cVector3f *p);
	bool ohpl_gs1_obb_overlap(const OhplGs1Obb *a, const OhplGs1Obb *b);
	bool ohpl_gs1_ray_hits_obb(const OhplGs1Obb *b, const hpl::cVector3f *start, const hpl::cVector3f *dir, float max_dist, float *t);
	void ohpl_gs1_area_body(const hpl::cMatrixf *m, const hpl::cVector3f *size, hpl::cMatrixf *out, hpl::cVector3f *out_size);
	void ohpl_gs1_relative_matrix(const hpl::cMatrixf *base, const hpl::cMatrixf *m, hpl::cMatrixf *out);
	bool ohpl_gs1_attachment_step(const hpl::cMatrixf *parent, const hpl::cMatrixf *prev, const hpl::cMatrixf *offset, const hpl::cMatrixf *cur,
								  bool use_rotation, bool locked, hpl::cMatrixf *out);
	void ohpl_gs1_liquid_extent(const OhplGs1Obb *b, hpl::cVector3f *out);
	float ohpl_gs1_liquid_wave_height(float surface_y, float amp, float freq, float time, float cx, float cz);
	bool ohpl_gs1_liquid_body_under(float bv_max_y, float center_y, float ext_y);
	float ohpl_gs1_liquid_surface(const OhplGs1Obb *b);

	void ohpl_gs1_apply_instance_vars(const OhplGs1Vars *inst, const OhplGs1Vars *vars, void (*sink)(void *, const OhplGs1Op *), void *sink_ctx);
}

#endif
