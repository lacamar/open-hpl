#ifndef SOMA_RUST_SOMA_LUX_H
#define SOMA_RUST_SOMA_LUX_H

// C ABI of rust/src/somalux/. Plans and decisions come back as values or ordered ops; C++ replays them.

#include "hpl.h"

#include <cstddef>
#include <cstdint>

// Prefix of cSomaLuxScriptable::cTimer; fields keep the Lux names
struct OhplGs3Timer
{
	uint64_t mlId;
	float mfLength;
	float mfTimeLeft;
	bool mbRepeat;
	bool mbPaused;
};

struct OhplGs3Fader
{
	uint64_t mlId;
	float mfValue, mfStart, mfGoal, mfSpeed;
	bool mbReverseAtEnd, mbPaused;
};

struct OhplGs3Fade
{
	float mfValue = 0, mfGoal = 0, mfSpeed = 0, mfMaxSpeed = 0, mfSpeedMul = 0;
};

struct OhplGs3MoveVec
{
	hpl::cVector3f mvValue = 0;
	hpl::cVector3f mvGoal = 0;
	float mfSpeed = 0, mfAcc = 0, mfMaxSpeed = 0, mfSlowdownDist = 0;
	bool mbMoving = false;
};

struct OhplGs3MoveSpeeds
{
	float mfMaxForwardSpeed, mfMaxBackwardSpeed, mfMaxSidwaySpeed, mfForwardAcc, mfSidewayAcc, mfForwardDeacc, mfSidewayDeacc,
		mfForwardOppositeDirAccMul, mfSidewayOppositeDirAccMul, mfMaxForwardSpeedMul, mfMaxBackwardSpeedMul, mfMaxSidwaySpeedMul;
};

struct OhplGs3Pause
{
	bool mbMusic, mbSounds, mbAmbience, mbSavingEnabled;
	float mfSavingDisabledTime;
};

struct OhplGs3Preload
{
	int mlPrio;
	unsigned int mlLastFrame;
	double mfDebtMs;
};

struct OhplGs3Begin
{
	bool mbUseTransfer, mbPreloaded, mbLookupArea;
};

struct OhplGs3Rel
{
	hpl::cMatrixf mtxRel, mtxInv;
	float mfYawRel, mfPitch;
	int mlActiveSize;
};

struct OhplGs3Arrival
{
	hpl::cVector3f mvFeet;
	float mfYaw;
	bool mbCrouching;
};

struct OhplGs3Tick
{
	bool mbDropOld, mbStart, mbStep;
	uint64_t mlBudgetMs;
};

struct OhplGs3Op
{
	int mlCode;
	uint32_t mlArg;
};

struct OhplGs3RotateIn
{
	float mfSpeed, mfAcc, mfSpeedMul, mfMaxSpeed;
	bool mbLocal;
	hpl::cVector3f mvTarget;
};

struct OhplGs3RotateOut
{
	float mfSpeed;
	bool mbApply;
	float mfAddYaw, mfAddPitch;
};

// OhplGs3Op codes
enum
{
	kOhplGs3OpSetScriptPaused, kOhplGs3OpClearUpdateWhenPaused, kOhplGs3OpWorldActive, kOhplGs3OpMusicResume, kOhplGs3OpMusicPause,
	kOhplGs3OpSoundsResumeAll, kOhplGs3OpSoundsPause, kOhplGs3OpSoundscapePaused, kOhplGs3OpReapplyVoicePauses,
};
enum { kOhplGs3OpCapacity = 12 };

extern "C" {
	// scriptable
	uint64_t ohpl_gs3_hash64(const unsigned char *s, size_t n);
	float ohpl_gs3_easing(int kind, float t);
	size_t ohpl_gs3_timers_update(void *base, size_t n, size_t stride, float dt, uint32_t *due, uint8_t *keep);
	void ohpl_gs3_faders_update(OhplGs3Fader *f, size_t n, float dt);
	float ohpl_gs3_fade_speed(float value, float goal, float time);
	float ohpl_gs3_timer_value(bool found, float length, float left, float min, float max, int kind, bool abs);
	bool ohpl_gs3_timer_has_passed(bool found, float length, float left, float wanted);
	float ohpl_gs3_fader_value(bool found, float value, float min, float max, int kind, bool abs);
	int ohpl_gs3_timer_add(bool exists, bool create_if_exist);

	// map change and preload
	bool ohpl_gs3_same_map(const unsigned char *a, size_t an, const unsigned char *b, size_t bn);
	void ohpl_gs3_map_begin(bool start_empty, bool pending_transfer_empty, bool has_map, const unsigned char *preload, size_t preload_n,
							const unsigned char *map, size_t map_n, OhplGs3Begin *out);
	float ohpl_gs3_start_yaw(const hpl::cMatrixf *m);
	void ohpl_gs3_capture(const hpl::cMatrixf *area, const hpl::cVector3f *feet, float yaw, int active_size, bool has_camera, float pitch,
						  OhplGs3Rel *out);
	bool ohpl_gs3_carries(const hpl::cMatrixf *inv, const hpl::cVector3f *pos, const hpl::cVector3f *size);
	void ohpl_gs3_arrive(const hpl::cMatrixf *new_area, const OhplGs3Rel *rel, OhplGs3Arrival *out);
	int ohpl_gs3_after_load(bool loaded, bool captured, bool has_current_map);
	int ohpl_gs3_restore_prop(bool found, bool bodies_match, bool is_static);
	void ohpl_gs3_preload_init(OhplGs3Preload *p);
	void ohpl_gs3_preload_tick(OhplGs3Preload *st, const unsigned char *want, size_t want_n, const unsigned char *loaded, size_t loaded_n,
							   bool running, uint32_t frame, OhplGs3Tick *out);
	void ohpl_gs3_preload_stepped(OhplGs3Preload *st, uint64_t elapsed_ms, uint64_t budget_ms);
	int ohpl_gs3_take_plan(const unsigned char *loaded, size_t loaded_n, const unsigned char *map, size_t map_n);
	int ohpl_gs3_taken(bool have_world, bool same);
	bool ohpl_gs3_preload_ready(const unsigned char *loaded, size_t loaded_n, bool running, const unsigned char *map, size_t map_n);

	// pause
	void ohpl_gs3_pause_init(OhplGs3Pause *p);
	size_t ohpl_gs3_set_paused(OhplGs3Pause *st, bool script_paused, bool pause, bool music, bool sounds, bool ambience, uint32_t types,
							   OhplGs3Op *ops);
	void ohpl_gs3_disable_saving(OhplGs3Pause *st, float duration);
	void ohpl_gs3_tick_saving(OhplGs3Pause *st, float dt);
	uint32_t ohpl_gs3_frame_plan(bool has_game, bool has_map, bool map_active, bool uses_real_player, bool paused);
	uint32_t ohpl_gs3_post_plan(bool updated, bool scripts_held, bool paused, bool has_game, bool has_map, bool map_active);

	// game
	bool ohpl_gs3_route_action(bool paused);
	bool ohpl_gs3_analog_allowed(bool paused);
	bool ohpl_gs3_visits_effect(bool effect_active, bool active_only);
	float ohpl_gs3_string_duration(float min, float start, float per_char, size_t chars);
	int ohpl_gs3_pointer_mode(int current, bool focus_present, bool focus_is_3d);
	bool ohpl_gs3_pointer_moved(bool before, int rel_x, int rel_y);
	int ohpl_gs3_action_edge(int id, int max_actions, bool down, bool prev_down);
	int ohpl_gs3_analog_fire(int analog_id, int axis, float action_mul, bool triggered, float *mul);

	// player
	float ohpl_gs3_fade_towards(OhplGs3Fade *x, float dt);
	float ohpl_gs3_angle_delta(float from, float to);
	void ohpl_gs3_step_move_vec(OhplGs3MoveVec *m, float dt);
	float ohpl_gs3_fov_mul(float base, const float *v, size_t n);
	float ohpl_gs3_visibility_range_mul(const float *v, size_t n);
	float ohpl_gs3_visibility_max_range(const float *v, size_t n);
	void ohpl_gs3_step_camera_rotate(const OhplGs3RotateIn *r, const hpl::cVector3f *cam_pos, float body_yaw, float cam_pitch, float dt,
									 OhplGs3RotateOut *out);
	bool ohpl_gs3_step_automove(const hpl::cVector3f *target, const hpl::cVector3f *feet, const hpl::cVector3f *forward,
								const hpl::cVector3f *right_dir, float speed_mul, float *fwd, float *right);
	void ohpl_gs3_update_average_move(float *speed, hpl::cVector3f *dir, const hpl::cVector3f *vel);
	void ohpl_gs3_move_speed_settings(const OhplGs3MoveSpeeds *s, float *out);
	int ohpl_gs3_state_change(bool exists, bool is_current);
	int ohpl_gs3_message_kind(const unsigned char *p, size_t n);
	bool ohpl_gs3_drops_state_on_reset(int kind, bool has_state, int state_id);
	bool ohpl_gs3_clears_camera_adds(bool is_transfer);
	int ohpl_gs3_place_plan(bool has_body, bool script_handled);
	bool ohpl_gs3_is_dead(float health, bool can_die);
	bool ohpl_gs3_set_health(float health, float max, bool can_die, float x, float *out);
	int ohpl_gs3_damage_id(const int *ids, size_t n, int id);
	float ohpl_gs3_health_after_damage(float health, float amount, float min_health);
	float ohpl_gs3_health_after_script_damage(float health, float amount, float min_health);
	float ohpl_gs3_add_health(float health, float x, float min_health);
	void ohpl_gs3_mouse_vector(int rel_x, int rel_y, float sensitivity, float height, float *x, float *y);
	void ohpl_gs3_smooth_mouse(const float (*xy)[2], size_t n, float prev_mul, float *x, float *y);
	bool ohpl_gs3_pops_oldest_sample(size_t len, int max);
}

static_assert(sizeof(OhplGs3Timer) == 24 && sizeof(OhplGs3Fader) == 32 && sizeof(OhplGs3Fade) == 20);
static_assert(sizeof(OhplGs3MoveVec) == 44 && sizeof(OhplGs3MoveSpeeds) == 48 && sizeof(OhplGs3Pause) == 8);
static_assert(sizeof(OhplGs3Preload) == 16 && sizeof(hpl::cMatrixf) == 64 && sizeof(hpl::cVector3f) == 12);

#endif // SOMA_RUST_SOMA_LUX_H
