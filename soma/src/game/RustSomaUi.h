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

struct OhplGs7ToneFade
{
	float goal, speed;
};

struct OhplGs7ToneState
{
	float world_exposure, world_white_cut, world_fade_time, exposure, white_cut, transition_time, grading_blend, grading_fade_time;
	OhplGs7ToneFade exposure_fade, white_cut_fade;
};
static_assert(sizeof(OhplGs7ToneState) == 48, "OhplGs7ToneState layout");

struct OhplGs7ExposureArea
{
	float inv_transform[16];
	float half_size[3];
	float exposure, white_cut, transition_time;
};
static_assert(sizeof(OhplGs7ExposureArea) == 88, "OhplGs7ExposureArea layout");

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

struct OhplGs7Chromatic
{
	float offset_a[2], offset_b[2], offset_c[2], color_a[4], color_b[4], color_c[4];
};
static_assert(sizeof(OhplGs7Chromatic) == 72, "OhplGs7Chromatic layout");

struct OhplGs7LevelSize
{
	int w, h, div;
};

struct OhplGs7BlurWeights
{
	float weights[7], off_h[7][2], off_v[7][2];
};
static_assert(sizeof(OhplGs7BlurWeights) == 140, "OhplGs7BlurWeights layout");

enum { OHPL_GS7_BLUR_DOWNSAMPLE = 0, OHPL_GS7_BLUR_BLUR = 1, OHPL_GS7_BLUR_FINAL = 2 };

struct OhplGs7BlurOp
{
	int kind, level;
	bool vertical;
	int program, slot;
	float alpha;
	int tex0_level;
	bool tex0_b;
	int tex1_level;
};
static_assert(sizeof(OhplGs7BlurOp) == 36, "OhplGs7BlurOp layout");

struct OhplGs7BlurPlan
{
	size_t n_ops;
	OhplGs7BlurOp ops[104];
	OhplGs7BlurWeights weights[2];
	bool passthrough;
};
static_assert(sizeof(OhplGs7BlurPlan) == 4040, "OhplGs7BlurPlan layout");

struct OhplGs7TentacleFades;

struct OhplGs7ImState
{
	int int_;
	float float_, vec[3], col[4];
	bool set_int, set_float, set_vec, set_col, in_focus;
	int def_int;
	float def_float;
	bool has_def_int, has_def_float;
};
static_assert(sizeof(OhplGs7ImState) == 56, "OhplGs7ImState layout");

struct OhplGs7ImFade
{
	int ty;
	float start[4], goal[4], time, count;
	int easing;
};
static_assert(sizeof(OhplGs7ImFade) == 48, "OhplGs7ImFade layout");

struct OhplGs7ImLayout
{
	int ty;
	float start[3], cursor[3], size[2], spacing[2], line_max;
};
static_assert(sizeof(OhplGs7ImLayout) == 48, "OhplGs7ImLayout layout");

struct OhplGs7ImPrev
{
	bool pressed, became_pressed, in_focus, became_in_focus, was_in_focus, mouse_over, updated;
	float pos[3], size[2];
};
static_assert(sizeof(OhplGs7ImPrev) == 28, "OhplGs7ImPrev layout");

struct OhplGs7ImNav
{
	uint64_t id;
	int pos[2], size[2], wrap, group;
};
static_assert(sizeof(OhplGs7ImNav) == 32, "OhplGs7ImNav layout");

struct OhplGs7ImFocus
{
	uint64_t focus, prev_focus;
	int last_dir[2], last_count;
	bool found_focus;
	int wrap_mode, group_flags, mouse_lock;
};

struct OhplGs7ImActions
{
	bool (*down)[10];
	bool (*triggered)[10];
	bool (*used)[10];
};

struct OhplGs7ImMouseIn
{
	float screen[2], virtual_size[2], virtual_offset[2];
	bool is_3d;
	float cursor_3d[2];
};

struct OhplGs7ImMouseOut
{
	float pos[2], rel[2], cursor_3d[2];
};

struct OhplGs7ImWidgetIn
{
	bool use_input, show_mouse, has_focus, first_run, use_ui_pos;
	float mouse_rel[2], exp_hori[2], exp_vert[2], pos[3], size[2], mouse[2];
};

struct OhplGs7ImWidgetOut
{
	bool in_focus, mouse_over, has_nav;
	OhplGs7ImNav nav;
};

struct OhplGs7ImButtonFlags
{
	bool use_trig_gfx, use_focus_gfx, use_trig_color, use_trig_focus_color, use_focus_color;
};

struct OhplGs7ImButtonOut
{
	bool clicked, down, result, trig, focus;
	int color;
};

enum { OHPL_GS7_IM_COLOR_BASE = 0, OHPL_GS7_IM_COLOR_TRIG_FOCUS = 1, OHPL_GS7_IM_COLOR_TRIG = 2, OHPL_GS7_IM_COLOR_FOCUS = 3 };

struct OhplGs7ImSliderIn
{
	bool in_focus, over;
	float min, max, step;
	bool vertical;
	float pos[3], size[2], mouse[2];
};

struct OhplGs7ImMultiIn
{
	bool over, mouse_over;
	int num;
	float mouse_x, pos_x, size_x;
};

struct OhplGs7ImWindowIn
{
	float pos[3], size[2];
	bool use_header;
	int header_type;
	float label_offset[3], label_pad[4], label_min_width, font_size[2], caption_length;
	int font_align;
	float pad[4];
};

struct OhplGs7ImWindowOut
{
	bool has_header;
	float header_pos[3], header_size[2], text_pos[3], group_pos[3], group_size[2];
};

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

	void ohpl_gs7_tone_state_default(OhplGs7ToneState *);
	int ohpl_gs7_tone_area_at(const OhplGs7ExposureArea *areas, size_t n, const float *pos);
	void ohpl_gs7_tone_map_loaded(OhplGs7ToneState *, float exposure, float white_cut, const OhplGs7ExposureArea *area);
	void ohpl_gs7_tone_update_exposure(OhplGs7ToneState *, bool world_current, const OhplGs7ExposureArea *area, float dt);
	bool ohpl_gs7_tone_update_grading(OhplGs7ToneState *, bool has_target, bool has_texture, float dt);
	size_t ohpl_gs7_tone_layer_insert_index(const int *prios, size_t n, int prio);
	float ohpl_gs7_tone_exposure_scale(float exposure);

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

	bool ohpl_gs7_pfx_set_activity(int type, const float *params, bool was_active, bool *clear);
	void ohpl_gs7_pfx_chromatic(const float *params, float screen_h, const float *tex, OhplGs7Chromatic *out);
	float ohpl_gs7_pfx_trail_alpha(float amount, float frame_time, bool clear);
	size_t ohpl_gs7_pfx_blur_levels(int w, int h, OhplGs7LevelSize *out, size_t max);
	void ohpl_gs7_pfx_blur_plan(float amount, int screen_h, const OhplGs7LevelSize *levels, size_t n, OhplGs7BlurPlan *out);
	OhplGs7TentacleFades *ohpl_gs7_pfx_tentacles_new();
	void ohpl_gs7_pfx_tentacles_free(OhplGs7TentacleFades *);
	void ohpl_gs7_pfx_tentacles_clear(OhplGs7TentacleFades *);
	void ohpl_gs7_pfx_tentacles_set_material(OhplGs7TentacleFades *, size_t tentacles, size_t materials, int idx, float time);
	int ohpl_gs7_pfx_tentacles_material(const OhplGs7TentacleFades *);
	void ohpl_gs7_pfx_tentacles_advance(OhplGs7TentacleFades *, float dt);
	int ohpl_gs7_pfx_tentacles_blend(const OhplGs7TentacleFades *, float *alpha);
	float ohpl_gs7_pfx_tentacle_time(float time, float dt, float speed);
	void ohpl_gs7_pfx_tentacle_speeds(size_t groups, float min, float max, float *out);
	void ohpl_gs7_pfx_tentacle_target(int w, int h, int *out);
	void ohpl_gs7_pfx_tentacle_model(int w, int h, const float *rot, float *out);
	void ohpl_gs7_pfx_tentacle_pos(const float *pos, float spread, float *out);
	float ohpl_gs7_pfx_tentacle_aspect(int w, int h);

	int ohpl_gs7_im_default_or_current_int(OhplGs7ImState *, int def);
	float ohpl_gs7_im_default_or_current_float(OhplGs7ImState *, float def);
	bool ohpl_gs7_im_fade_advance(OhplGs7ImFade *, float dt, float *x);
	void ohpl_gs7_im_fade_apply(const OhplGs7ImFade *, float t, OhplGs7ImState *);
	bool ohpl_gs7_im_fade_blocked(const OhplGs7ImFade *existing, bool replace);
	void ohpl_gs7_im_fade_make(const OhplGs7ImState *, int ty, const float *goal, float time, int easing, OhplGs7ImFade *out);
	void ohpl_gs7_im_mouse_map(const OhplGs7ImMouseIn *, int px, int py, int rx, int ry, OhplGs7ImMouseOut *out);
	void ohpl_gs7_im_layout(float *pos, float *size, const float *default_size, const float *origin, bool do_align, int align_mode);
	void ohpl_gs7_im_align(const float *pos, const float *size, int mode, float *out);
	void ohpl_gs7_im_advance(OhplGs7ImLayout *, const float *pos, const float *size);
	bool ohpl_gs7_im_mouse_over(bool use_input, bool has_focus, const float *mouse, const float *pos, const float *size);
	void ohpl_gs7_im_widget_base(uint64_t id, const OhplGs7ImWidgetIn *, OhplGs7ImFocus *, const OhplGs7ImActions *, OhplGs7ImState *, OhplGs7ImPrev *, OhplGs7ImWidgetOut *out);
	bool ohpl_gs7_im_became_pressed(const OhplGs7ImActions *, bool keys, bool mouse);
	bool ohpl_gs7_im_action_triggered(const OhplGs7ImActions *, int action, bool check_used);
	void ohpl_gs7_im_update_ui_movement(const OhplGs7ImNav *nav, size_t n, OhplGs7ImFocus *, const OhplGs7ImActions *);
	void ohpl_gs7_im_button(OhplGs7ImState *, const OhplGs7ImActions *, bool over, bool mouse_over, int mode, float dt, const OhplGs7ImButtonFlags *, OhplGs7ImButtonOut *out);
	bool ohpl_gs7_im_slider(OhplGs7ImState *, const OhplGs7ImActions *, const OhplGs7ImSliderIn *);
	void ohpl_gs7_im_slider_button_pos(float value, float min, float max, const float *pos, const float *size, const float *button, bool vertical, float *out);
	bool ohpl_gs7_im_checkbox(OhplGs7ImState *, const OhplGs7ImActions *, bool over);
	void ohpl_gs7_im_multi_select(OhplGs7ImState *, const OhplGs7ImActions *, const OhplGs7ImMultiIn *);
	void ohpl_gs7_im_text_origin(const float *pos, const float *size, const float *font, int align, float *out);
	void ohpl_gs7_im_color_mul(const float *a, const float *b, float *out);
	void ohpl_gs7_im_gauge_fill(const float *pos, const float *size, const float *pad, float fill, bool vertical, float *out_pos, float *out_size);
	void ohpl_gs7_im_window(const OhplGs7ImWindowIn *, OhplGs7ImWindowOut *out);
}

#endif
