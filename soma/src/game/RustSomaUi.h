#ifndef SOMA_RUST_UI_H
#define SOMA_RUST_UI_H

// C ABI of rust/src/somaui/.

#include <cstddef>
#include <cstdint>

struct OhplGs7CritterState
{
	bool flying, test_collision, align_to_ground, can_run_on_walls, use_ray_collision, init;
	float vel[3];
	float wander_angle, wander_pitch, yaw, pitch;
	int playing_anim_state;
	float ground_timer, ground_dist, wall_timer;
};
static_assert(sizeof(OhplGs7CritterState) == 52 && offsetof(OhplGs7CritterState, vel) == 8, "OhplGs7CritterState layout");

struct OhplGs7CritterProps
{
	float wanted_vel[3], gravity_vel[3], ground_normal[3], wall_avoid_normal[3], wall_avoid_position[3];
	float max_velocity, max_turn_speed, turn_speed_mul, max_gravity_velocity, forward_rotation_y_offset;
	int anim_state;
	bool wall_avoid_detected;
};
static_assert(sizeof(OhplGs7CritterProps) == 88, "OhplGs7CritterProps layout");

struct OhplGs7CritterHost
{
	void *ctx;
	bool (*cast)(void *, const float *start, const float *end, float *dist, float *normal);
};

struct OhplGs7FlockMember
{
	float pos[3], vel[3];
	bool is_self;
};

struct OhplGs7Flock
{
	float center_mul, center_y_mul, separation, alignment, cohesion;
	int max_checks;
	float dt;
};

struct OhplGs7CritterStep
{
	float pos[3];
	float yaw, pitch;
	int play_anim; // 0 none, 1 move, 2 idle
};

struct OhplGs7GammaLayout
{
	float checker_pos[2], instructions_pos[2], slider_pos[3], slider_size[2], button_pos[3], button_size[2];
};
static_assert(sizeof(OhplGs7GammaLayout) == 56, "OhplGs7GammaLayout layout");

struct OhplGs7SplashPlan
{
	float alpha, premenu_pos[3], premenu_size[2], bar_pos[3], bar_size[2], bar_clip[4], brain_alpha;
};
static_assert(sizeof(OhplGs7SplashPlan) == 64, "OhplGs7SplashPlan layout");

struct OhplGs7ElementId
{
	uint8_t a;
	int b, c;
};
static_assert(sizeof(OhplGs7ElementId) == 12, "OhplGs7ElementId layout");

extern "C"
{
	void ohpl_gs7_critter_state_default(OhplGs7CritterState *);
	void ohpl_gs7_critter_state_init(OhplGs7CritterState *, float fwd_x, float fwd_z);
	void ohpl_gs7_critter_update(OhplGs7CritterState *, OhplGs7CritterProps *, const OhplGs7CritterHost *, const float *pos, float dt, OhplGs7CritterStep *out);
	void ohpl_gs7_critter_safe_normalize(const float *v, float *out);
	float ohpl_gs7_critter_change_max_speed(float f, float goal, float acc, float dt);
	void ohpl_gs7_critter_toward_add(const float *pos, const float *target, bool normalize, float dt, float *out);
	void ohpl_gs7_critter_stop_add(const float *wanted, float amount, float dt, float *out);
	void ohpl_gs7_critter_toward_center_add(const float *pos, bool has_center, const float *center, float dt, float *out);
	void ohpl_gs7_critter_flocking_add(const float *pos, const float *self_vel, bool has_center, const float *center, const OhplGs7FlockMember *members, size_t n,
									   const OhplGs7Flock *, float *out);
	void ohpl_gs7_critter_wander_add(OhplGs7CritterState *, const float *wanted, float length, float radius, float dt, bool three_d, float *out);
	bool ohpl_gs7_critter_far_exceeded(const float *player, const float *pos, float max_enabled_distance);
	void ohpl_gs7_critter_towards_ground_add(OhplGs7CritterState *, const OhplGs7CritterHost *, const float *pos, float max_height, bool has_mesh, float mesh_height,
											 float dt, bool far_exceeded, float *out);
	void ohpl_gs7_critter_wall_avoid_add(OhplGs7CritterState *, OhplGs7CritterProps *, const OhplGs7CritterHost *, const float *pos, float distance_forward, float dt,
										 bool far_exceeded, float *out);

	float ohpl_gs7_gamma_wrap_width(float screen_w);
	void ohpl_gs7_gamma_layout(const float *screen, const float *checker, float wrap_w, size_t rows, OhplGs7GammaLayout *out);
	int ohpl_gs7_gamma_to_slider(float gamma);
	float ohpl_gs7_gamma_from_slider(int value, int max_value);
	int ohpl_gs7_mouse_edge(bool down, bool was_down);

	bool ohpl_gs7_splash_update(float *elapsed, float dt);
	bool ohpl_gs7_splash_scripts_may_run(bool finished, float elapsed);
	float ohpl_gs7_splash_boot_bar_fraction(float t);
	float ohpl_gs7_splash_virtual_width(const float *screen);
	void ohpl_gs7_splash_virtual_to_screen(const float *pos, float z, float vw, const float *screen, float *out);
	void ohpl_gs7_splash_virtual_size_to_screen(const float *size, float vw, const float *screen, float *out);
	int ohpl_gs7_splash_brain_frame(float time);
	bool ohpl_gs7_splash_load_draw_due(uint64_t now, uint64_t last);
	float ohpl_gs7_splash_boot_elapsed(uint64_t now, uint64_t boot_start);
	float ohpl_gs7_splash_load_fraction(uint64_t now, uint64_t start);
	void ohpl_gs7_splash_plan(const float *screen, float vw, float t, const float *premenu, OhplGs7SplashPlan *out);
	void ohpl_gs7_splash_brain_rect(float vw, const float *screen, float *pos, float *size);

	int ohpl_gs7_load_kind(const unsigned char *name, size_t n); // 0 prop, 1 agent, 2 critter
	bool ohpl_gs7_load_is_dynamic(bool spawned, bool has_anim, bool has_proc_anim, const float *masses, size_t n);
	void ohpl_gs7_load_element_id(const int *uid, size_t n, int id_attr, OhplGs7ElementId *out);
	bool ohpl_gs7_load_before_joints(bool has_pose, bool has_skeleton);
	bool ohpl_gs7_load_align_bodies(bool rest_pose_set);
	float ohpl_gs7_load_mean_scale(const float *s);
	bool ohpl_gs7_load_reparent_children(bool has_mesh, size_t bodies);
	void ohpl_gs7_load_scaled_offset(const float *local, const float *scale, float *out);
	bool ohpl_gs7_load_random_anim_start(bool is_prop, bool randomize);
	bool ohpl_gs7_load_freeze_props(bool is_prop, bool active, bool disable_var, bool skeleton_physics);
	bool ohpl_gs7_load_no_gravity_underwater(bool is_prop, bool var);
	ptrdiff_t ohpl_gs7_load_socket_offset(const unsigned char *name, size_t n);
	void ohpl_gs7_load_color_mul(const float *col, const float *mul, float *out);
	void ohpl_gs7_load_color_mul_all(const float *col, const float *mul, float *out);
	bool ohpl_gs7_load_default_show_mesh(const unsigned char *name, size_t n);

}

#endif
