#ifndef LUX_RUST_PROP_H
#define LUX_RUST_PROP_H

// C ABI of rust/src/luxprop/ffi.rs. The POD structs mirror Rust repr(C) structs field for field and
// live inside the C++ props, which pass them to Rust by pointer. Rust returns joints, bodies, sounds,
// particles and script calls as OhplGa4Op lists that C++ replays in order after the call.

#include <cstddef>
#include <cstdint>

struct OhplGa4Op
{
	int code;
	int i;
	int j;
	float f[4];
	float v[3];
	float m[16];
};

struct OhplGa4Sink
{
	void *ctx;
	void (*push)(void *ctx, const OhplGa4Op *op);
};

struct OhplGa4Hinge
{
	float angle;
	float min_angle;
	float max_angle;
	float pin_dir[3];
	float pivot[3];
	float child[16];
	float child_ang_vel[3];
};

struct OhplGa4Slider
{
	float distance;
	float pin_dir[3];
	float child_lin_vel[3];
	float child_mass;
};

struct OhplGa4Host
{
	void *ctx;
	void (*move_push)(void *ctx, const float *add, float *out);
	bool (*outside_collides)(void *ctx, int i);
	bool (*insanity_in_fov)(void *ctx);
	void (*hinge)(void *ctx, int i, OhplGa4Hinge *out);
	void (*slider)(void *ctx, OhplGa4Slider *out);
};

struct OhplGa4VarsOps
{
	void (*get_string)(void *ctx, const unsigned char *name, size_t len, const unsigned char *def, size_t def_len, void (*out)(void *, const unsigned char *, size_t), void *out_ctx);
	bool (*get_bool)(void *ctx, const unsigned char *name, size_t len, bool def);
	float (*get_float)(void *ctx, const unsigned char *name, size_t len, float def);
	int (*get_int)(void *ctx, const unsigned char *name, size_t len, int def);
	void (*get_vec3)(void *ctx, const unsigned char *name, size_t len, const float *def, float *out);
	void (*get_color)(void *ctx, const unsigned char *name, size_t len, const float *def, float *out);
	void (*put)(void *ctx, int slot, const unsigned char *s, size_t len);
};

struct OhplGa4Vars
{
	const OhplGa4VarsOps *ops;
	void *ctx;
};

enum
{
	kOhplGa4Op_FxPsCreateAll = 1,
	kOhplGa4Op_FxLightFadeOn = 2,
	kOhplGa4Op_FxLightSetOn = 3,
	kOhplGa4Op_FxSoundFadeIn = 4,
	kOhplGa4Op_FxSoundPlay = 5,
	kOhplGa4Op_FxPsKillAll = 6,
	kOhplGa4Op_FxLightOff = 7,
	kOhplGa4Op_FxSoundFadeOut = 8,
	kOhplGa4Op_FxSoundStop = 9,
	kOhplGa4Op_FxChangeSound = 10,
	kOhplGa4Op_FxLightFlicker = 11,
	kOhplGa4Op_FxBillboards = 12,
	kOhplGa4Op_FxMeshIllum = 13,
	kOhplGa4Op_Coverage = 14,
	kOhplGa4Op_SndMoveStart = 15,
	kOhplGa4Op_SndMoveStop = 16,
	kOhplGa4Op_MoveLoopStart = 17,
	kOhplGa4Op_MoveLoopVolume = 18,
	kOhplGa4Op_MoveLoopKill = 19,
	kOhplGa4Op_StaticLinearMove = 20,
	kOhplGa4Op_StaticAngularMove = 21,
	kOhplGa4Op_OnStartMove = 22,
	kOhplGa4Op_OutsideImpulse = 23,
	kOhplGa4Op_OutsideCollideChar = 24,
	kOhplGa4Op_ConnLimit = 25,
	kOhplGa4Op_ConnTurn = 26,
	kOhplGa4Op_ConnState = 27,
	kOhplGa4Op_RunAnimCallback = 28,
	kOhplGa4Op_PlaySound = 29,
	kOhplGa4Op_Break = 30,
	kOhplGa4Op_SetJointMinMax = 31,
	kOhplGa4Op_JointWake = 32,
	kOhplGa4Op_InsanityVisible = 33,
	kOhplGa4Op_FoodAttract = 34,
	kOhplGa4Op_HingeMin = 35,
	kOhplGa4Op_HingeMax = 36,
	kOhplGa4Op_HingeSticky = 37,
	kOhplGa4Op_BodiesWake = 38,
	kOhplGa4Op_DoorBodyMatrix = 39,
	kOhplGa4Op_DoorDamageLevel = 40,
	kOhplGa4Op_DoorDisableBodies = 41,
	kOhplGa4Op_DoorBrokenEntity = 42,
	kOhplGa4Op_DoorEffects = 43,
	kOhplGa4Op_PidTorque = 44,
	kOhplGa4Op_PidReset = 45,
	kOhplGa4Op_BodyAngVel = 46,
	kOhplGa4Op_SliderMin = 47,
	kOhplGa4Op_SliderMax = 48,
	kOhplGa4Op_SliderWake = 49,
	kOhplGa4Op_SliderForce = 50,
	kOhplGa4Op_RunChangeCallback = 51,
	kOhplGa4Op_ChangeSound = 52,
	kOhplGa4Op_LimitSound = 53,
	kOhplGa4Op_SetEffects = 54,
	kOhplGa4Op_RunBreakCallback = 55,
	kOhplGa4Op_DoorDestroyBroken = 56,
	kOhplGa4Op_BodyZeroVel = 57,

	kOhplGa4Snd_CloseOn = 0,
	kOhplGa4Snd_CloseOff = 1,
	kOhplGa4Snd_LockOn = 2,
	kOhplGa4Snd_LockOff = 3,
	kOhplGa4Snd_InteractLocked = 4,
	kOhplGa4Snd_WheelStuck = 5,
	kOhplGa4Snd_LeverStuck = 6,
	kOhplGa4Snd_MultiStuck = 7,

	kOhplGa4Slot_MainBody = 0,
	kOhplGa4Slot_EffectsOnSound = 1,
	kOhplGa4Slot_EffectsOffSound = 2,
	kOhplGa4Slot_MoveStartSound = 3,
	kOhplGa4Slot_MoveStopSound = 4,
	kOhplGa4Slot_MoveLoopSound = 5,
	kOhplGa4Slot_BreakEntity = 10,
	kOhplGa4Slot_BreakAlignBody = 11,
	kOhplGa4Slot_BreakSound = 12,
	kOhplGa4Slot_BreakPs = 13,
	kOhplGa4Slot_ContainedItem = 14,
	kOhplGa4Slot_CallbackFunc = 20,
	kOhplGa4Slot_ConnChange = 21,
	kOhplGa4Slot_LookAt = 22,
	kOhplGa4Slot_Interact = 23,
	kOhplGa4Slot_ConnectedProp = 24,
	kOhplGa4Slot_CloseOn = 30,
	kOhplGa4Slot_CloseOff = 31,
	kOhplGa4Slot_LockOn = 32,
	kOhplGa4Slot_LockOff = 33,
	kOhplGa4Slot_InteractLocked = 34,
	kOhplGa4Slot_DamageMesh1 = 35,
	kOhplGa4Slot_DamageMesh2 = 36,
	kOhplGa4Slot_BrokenEntity = 37,
	kOhplGa4Slot_DamageSound = 38,
	kOhplGa4Slot_DamagePs = 39,
	kOhplGa4Slot_DoorBreakSound = 40,
	kOhplGa4Slot_DoorBreakPs = 41,
	kOhplGa4Slot_WheelMinSound = 50,
	kOhplGa4Slot_WheelMaxSound = 51,
	kOhplGa4Slot_WheelStuckSound = 52,
	kOhplGa4Slot_LeverMinSound = 60,
	kOhplGa4Slot_LeverMaxSound = 61,
	kOhplGa4Slot_LeverStuckSound = 62,
	kOhplGa4Slot_ConnectedLight = 70,
	kOhplGa4Slot_ChangeStateSound = 80,
	kOhplGa4Slot_MultiStuckSound = 81,
	kOhplGa4Slot_ChangeStateCallback = 82,
	kOhplGa4Slot_AngularOffsetArea = 90,
};

struct OhplGa4BaseParams
{
	float health;
	int toughness;
	float max_focus_distance;
	bool dissolve_on_destruction;
	float dissolve_time;
	bool show_mesh;
	bool show_hints;
	float effects_on_time;
	float effects_off_time;
	float effects_off_light_color[4];
	float effects_off_light_radius;
	bool move_check_collision;
	bool randomize_anim_start;
};

struct OhplGa4BaseInstance
{
	bool cast_shadows;
	bool full_game_save;
	bool look_at_auto_remove;
	bool interact_auto_remove;
	bool static_physics;
};

struct OhplGa4LightData
{
	float color[4];
	float radius;
	bool flickering;
};

struct OhplGa4Linear
{
	float max_speed;
	float acc;
	float speed;
	float slowdown;
	float goal[3];
};

struct OhplGa4Angular
{
	float max_speed;
	float acc;
	float speed;
	float slowdown;
	float goal[16];
	bool no_goal;
	float no_goal_dir[3];
	bool use_offset;
	float world_offset[3];
	float local_offset[3];
};

struct OhplGa4Base
{
	bool effects_active;
	bool effects_fading;
	float effects_alpha;
	bool check_outside_player;
	bool moving;
	bool moving_linear;
	bool moving_angular;
	float moving_volume;
	float move_start_count;
	OhplGa4Linear linear;
	OhplGa4Angular angular;
	float last_body_matrix[16];
	float fade_in_alpha;
	float fade_in_speed;
};

struct OhplGa4EffectsIn
{
	bool active;
	bool fade;
	int n_ps;
	int n_sounds;
	float on_time;
	float off_time;
	float off_radius;
	bool on_sound;
	bool off_sound;
};

struct OhplGa4MoveIn
{
	float dt;
	float body[16];
	bool check_collision;
	bool loop_present;
	bool loop_alive;
	bool loop_sound;
};

struct OhplGa4OutsideBody
{
	bool collide_character;
	bool default_collide_character;
	float pos[3];
	float vel[3];
};

struct OhplGa4Grab
{
	bool use_depth;
	float depth;
	float depth_inc;
	float min_depth;
	float max_depth;
	bool use_offset;
	float position_offset[3];
	float rotation_offset[3];
	float throw_impulse;
	float mass_mul;
	float force_mul;
	float torque_mul;
	bool use_rotation;
};

struct OhplGa4Push
{
	bool at_point;
	float force_mul;
	float impulse;
};

struct OhplGa4MoveData
{
	float max_speed;
	float slow_down_factor;
	float speed_factor;
	float throw_impulse;
};

struct OhplGa4ObjectParams
{
	int object_type;
	bool bad_sub_type;
	float life_length;
	bool is_food;
	bool break_active;
	bool break_destroy_joints;
	float break_min_energy;
	float break_impulse;
	float hit_damage_amount;
	int hit_damage_strength;
	float min_hit_damage_speed;
	bool grab_skip_non_outer;
	OhplGa4Grab grab;
	OhplGa4Push push;
	OhplGa4MoveData slide;
};

struct OhplGa4ObjectInstance
{
	bool disable_breakable;
	bool is_insanity_vision;
	float vision_max_sanity;
};

struct OhplGa4ObjectState
{
	bool broken;
	float life_length_count;
	int stuck_state;
	float food_attract_count;
	float insanity_vision_count;
	bool insanity_vision_active;
};

struct OhplGa4ObjectUpdateIn
{
	float dt;
	bool has_mesh;
	float sanity;
	bool interacted;
};

struct OhplGa4ImpactBody
{
	float mass;
	float center[3];
	float vel_at_impact[3];
};

struct OhplGa4JointData
{
	float max_limit;
	float min_limit;
	float locked_range;
	int kind;
};

struct OhplGa4DoorParams
{
	OhplGa4MoveData data;
	bool can_interact_static;
	bool breakable;
	float break_impulse;
	float health_damage[2];
};

struct OhplGa4Door
{
	bool locked;
	bool closed;
	int broken_entity_id;
	bool disable_auto_close;
	int current_mesh_entity;
	bool broken;
	float interact_sound_count;
	float goal_open_amount;
	bool opening_towards_max;
	float current_open_amount;
	float open_speed;
	bool is_updating_open_amount;
	bool disable_breakable;
};

struct OhplGa4DoorJoint
{
	float max_angle;
	float min_angle;
};

struct OhplGa4WheelParams
{
	OhplGa4MoveData data;
	bool can_interact_static;
	float min_limit;
	float max_limit;
	float min_limit_range;
	float max_limit_range;
	bool min_limit_stuck;
	bool max_limit_stuck;
	int spin_dir;
	bool slow_down_rotation;
	bool skip_middle_state;
};

struct OhplGa4Wheel
{
	float angle;
	float prev_angle;
	float stuck_sound_timer;
	float last_to_max;
	float last_to_min;
	int current_state;
	int stuck_state;
	float joint_angle;
	float prev_joint_angle;
	bool auto_moving;
	float auto_move_goal;
	bool interaction_disables_stuck;
};

struct OhplGa4LeverParams
{
	OhplGa4MoveData data;
	bool can_interact_static;
	float min_limit_range;
	float max_limit_range;
	bool min_limit_stuck;
	bool max_limit_stuck;
	float middle_angle_amount;
	bool auto_move_to_angle;
	int auto_move_goal;
	float auto_move_speed_factor;
	float auto_move_max_speed;
};

struct OhplGa4Lever
{
	int current_state;
	int stuck_state;
	bool interaction_disables_stuck;
	float stuck_sound_timer;
	float default_min_angle;
	float default_max_angle;
	float middle_angle;
};

struct OhplGa4LampParams
{
	bool can_be_lit_by_player;
	bool can_be_grabbed;
	OhplGa4Grab grab;
};

struct OhplGa4Lamp
{
	bool lit;
	bool light_connection_setup;
	float connection_light_amount;
	bool connection_light_use_on_color;
	bool connection_light_use_spec;
};

struct OhplGa4LampLink
{
	bool has_lights;
	float color[4];
	bool use_spec;
	float amount;
	float effects_alpha;
};

struct OhplGa4MultiParams
{
	OhplGa4MoveData data;
	int num_of_states;
	bool few_states;
	float stick_to_state_max_dist;
	bool can_interact_static;
	bool auto_move_to_current_state;
	float auto_move_speed_factor;
	float auto_move_max_speed;
};

struct OhplGa4Multi
{
	int current_state;
	int stuck_state;
	float stuck_sound_timer;
	bool interaction_disables_stuck;
	float default_min_dist;
	float default_max_dist;
};

struct OhplGa4MoveParams
{
	int move_type;
	bool bad_type;
	float open_amount;
	int move_axis;
	bool bad_axis;
	float open_acc;
	float open_speed;
	float close_acc;
	float close_speed;
	bool auto_move;
	float auto_move_state_goal;
	float auto_move_acc;
	float auto_move_speed;
	float auto_move_slowdown_dist;
};

struct OhplGa4MoveObj
{
	float angular_offset_pos[3];
	bool use_angular_local_offset;
	bool auto_move_reached_goal;
	float closed[16];
	float open[16];
};

static_assert(sizeof(OhplGa4BaseParams) == 56, "OhplGa4BaseParams layout");
static_assert(sizeof(OhplGa4BaseInstance) == 5, "OhplGa4BaseInstance layout");
static_assert(sizeof(OhplGa4LightData) == 24, "OhplGa4LightData layout");
static_assert(sizeof(OhplGa4Linear) == 28, "OhplGa4Linear layout");
static_assert(sizeof(OhplGa4Angular) == 124, "OhplGa4Angular layout");
static_assert(sizeof(OhplGa4Base) == 244, "OhplGa4Base layout");
static_assert(sizeof(OhplGa4EffectsIn) == 28, "OhplGa4EffectsIn layout");
static_assert(sizeof(OhplGa4MoveIn) == 72, "OhplGa4MoveIn layout");
static_assert(sizeof(OhplGa4OutsideBody) == 28, "OhplGa4OutsideBody layout");
static_assert(sizeof(OhplGa4Grab) == 68, "OhplGa4Grab layout");
static_assert(sizeof(OhplGa4Push) == 12, "OhplGa4Push layout");
static_assert(sizeof(OhplGa4MoveData) == 16, "OhplGa4MoveData layout");
static_assert(sizeof(OhplGa4ObjectParams) == 136, "OhplGa4ObjectParams layout");
static_assert(sizeof(OhplGa4ObjectInstance) == 8, "OhplGa4ObjectInstance layout");
static_assert(sizeof(OhplGa4ObjectState) == 24, "OhplGa4ObjectState layout");
static_assert(sizeof(OhplGa4ObjectUpdateIn) == 16, "OhplGa4ObjectUpdateIn layout");
static_assert(sizeof(OhplGa4ImpactBody) == 28, "OhplGa4ImpactBody layout");
static_assert(sizeof(OhplGa4JointData) == 16, "OhplGa4JointData layout");
static_assert(sizeof(OhplGa4DoorParams) == 32, "OhplGa4DoorParams layout");
static_assert(sizeof(OhplGa4Door) == 44, "OhplGa4Door layout");
static_assert(sizeof(OhplGa4DoorJoint) == 8, "OhplGa4DoorJoint layout");
static_assert(sizeof(OhplGa4WheelParams) == 48, "OhplGa4WheelParams layout");
static_assert(sizeof(OhplGa4Wheel) == 48, "OhplGa4Wheel layout");
static_assert(sizeof(OhplGa4LeverParams) == 52, "OhplGa4LeverParams layout");
static_assert(sizeof(OhplGa4Lever) == 28, "OhplGa4Lever layout");
static_assert(sizeof(OhplGa4LampParams) == 72, "OhplGa4LampParams layout");
static_assert(sizeof(OhplGa4Lamp) == 12, "OhplGa4Lamp layout");
static_assert(sizeof(OhplGa4LampLink) == 32, "OhplGa4LampLink layout");
static_assert(sizeof(OhplGa4MultiParams) == 40, "OhplGa4MultiParams layout");
static_assert(sizeof(OhplGa4Multi) == 24, "OhplGa4Multi layout");
static_assert(sizeof(OhplGa4MoveParams) == 56, "OhplGa4MoveParams layout");
static_assert(sizeof(OhplGa4MoveObj) == 144, "OhplGa4MoveObj layout");
static_assert(sizeof(OhplGa4Hinge) == 112, "OhplGa4Hinge layout");
static_assert(sizeof(OhplGa4Slider) == 32, "OhplGa4Slider layout");
static_assert(sizeof(OhplGa4Op) == 104, "OhplGa4Op layout");

extern "C"
{
	void ohpl_ga4_base_init(OhplGa4Base *s);
	void ohpl_ga4_base_load(const OhplGa4Vars *v, OhplGa4BaseParams *out);
	void ohpl_ga4_base_load_instance(const OhplGa4Vars *v, bool force_full_game_save, OhplGa4BaseInstance *out);
	float ohpl_ga4_give_damage(float health, int toughness, float amount, int strength, float *out_amount);
	void ohpl_ga4_effects_set_active(OhplGa4Base *s, const OhplGa4EffectsIn *a, const OhplGa4LightData *lights, int n_lights, const OhplGa4Sink *sink);
	void ohpl_ga4_effects_update(OhplGa4Base *s, float dt, float on_time, float off_time, const OhplGa4LightData *lights, int n_lights, bool has_mesh, const OhplGa4Sink *sink);
	void ohpl_ga4_fade_in_mesh(OhplGa4Base *s, float time, bool has_mesh, const OhplGa4Sink *sink);
	void ohpl_ga4_mesh_fading(OhplGa4Base *s, float dt, bool has_mesh, const OhplGa4Sink *sink);
	void ohpl_ga4_anim_update(int *index, bool over, bool has_callback, const OhplGa4Sink *sink);
	float ohpl_ga4_connection_turn(float angle, float min, float max);
	void ohpl_ga4_reset_move(OhplGa4Base *s);
	void ohpl_ga4_move_linear_to(OhplGa4Base *s, const float *goal, float acc, float max_speed, float slowdown, bool reset, const OhplGa4Sink *sink);
	void ohpl_ga4_move_angular_to(OhplGa4Base *s, const float *goal, float acc, float max_speed, float slowdown, bool reset, bool use_offset, const float *world_offset, const float *local_offset, const OhplGa4Sink *sink);
	void ohpl_ga4_rotate_at_speed(OhplGa4Base *s, float acc, float goal_speed, const float *axis, bool reset, bool use_offset, const float *world_offset, const float *local_offset, const OhplGa4Sink *sink);
	void ohpl_ga4_stop_move(OhplGa4Base *s, const OhplGa4Sink *sink);
	void ohpl_ga4_update_moving(OhplGa4Base *s, const OhplGa4MoveIn *a, const OhplGa4Host *host, const OhplGa4Sink *sink);
	bool ohpl_ga4_outside_player(const OhplGa4OutsideBody *bodies, int n, const float *player_pos, const OhplGa4Host *host, const OhplGa4Sink *sink);

	void ohpl_ga4_object_load(const OhplGa4Vars *v, const unsigned char *sub_type, size_t sub_type_len, float *focus, const float *defaults, OhplGa4ObjectParams *out);
	void ohpl_ga4_object_load_instance(const OhplGa4Vars *v, OhplGa4ObjectState *s, OhplGa4ObjectInstance *out, const OhplGa4Sink *sink);
	bool ohpl_ga4_object_can_interact(const OhplGa4ObjectParams *p, int joint_num, float mass, bool in_sticky_area, bool has_interact_callback);
	int ohpl_ga4_object_focus_crosshair(const OhplGa4ObjectParams *p, bool can_interact, bool custom_is_default);
	bool ohpl_ga4_object_show_outlines(const OhplGa4ObjectParams *p);
	float ohpl_ga4_object_impact_energy(const OhplGa4ImpactBody *bodies, const float *point, const float *normal);
	bool ohpl_ga4_object_hit_hurts(const OhplGa4ObjectParams *p, float speed);
	void ohpl_ga4_object_on_health_change(const OhplGa4ObjectParams *p, float health, const OhplGa4Sink *sink);
	void ohpl_ga4_object_update(OhplGa4ObjectState *s, const OhplGa4ObjectParams *p, const OhplGa4ObjectInstance *i, const OhplGa4ObjectUpdateIn *a, const OhplGa4Host *host, const OhplGa4Sink *sink);
	void ohpl_ga4_object_joint_data(int kind, float min, float max, OhplGa4JointData *out);
	void ohpl_ga4_object_set_stuck_state(OhplGa4ObjectState *s, int state, const OhplGa4JointData *joints, int n, const OhplGa4Sink *sink);
	void ohpl_ga4_object_set_insanity_visible(OhplGa4ObjectState *s, bool visible, const OhplGa4Sink *sink);
	void ohpl_ga4_break_impulse(const float *center, const float *origin, float impulse, const float *add, float *out);
	void ohpl_ga4_bodies_center(const float *pos, int n, float *out);

	void ohpl_ga4_door_load(const OhplGa4Vars *v, OhplGa4DoorParams *out);
	void ohpl_ga4_door_state_new(OhplGa4Door *s);
	void ohpl_ga4_door_load_instance(const OhplGa4Vars *v, OhplGa4Door *s, const OhplGa4DoorJoint *joints, int n, int n_joints, const OhplGa4Host *host, const OhplGa4Sink *sink);
	bool ohpl_ga4_door_can_interact(const OhplGa4DoorParams *p, float mass, bool in_sticky_area);
	void ohpl_ga4_door_set_closed(OhplGa4Door *s, const OhplGa4DoorJoint *joints, int n, int n_joints, bool closed, bool effects, const OhplGa4Sink *sink);
	int ohpl_ga4_door_state(const OhplGa4DoorJoint *joints, int n, const OhplGa4Host *host);
	void ohpl_ga4_door_set_locked(OhplGa4Door *s, const OhplGa4DoorJoint *joints, int n, int n_joints, bool locked, bool effects, const OhplGa4Sink *sink);
	void ohpl_ga4_door_set_damage_level(OhplGa4Door *s, int level, const OhplGa4Sink *sink);
	void ohpl_ga4_door_on_connection_state(OhplGa4Door *s, const OhplGa4DoorJoint *joints, int n, int n_joints, int state, const OhplGa4Sink *sink);
	bool ohpl_ga4_door_on_interact(OhplGa4Door *s, const OhplGa4DoorJoint *joints, int n, int n_joints, bool show_hints, const OhplGa4Sink *sink);
	void ohpl_ga4_door_setup_physics(OhplGa4Door *s, const OhplGa4DoorJoint *joints, int n, int n_joints, float open, bool towards_max, bool apply_always, const OhplGa4Host *host, const OhplGa4Sink *sink);
	void ohpl_ga4_door_set_open_amount(OhplGa4Door *s, const OhplGa4DoorJoint *joints, int n, int n_joints, float open, float duration, bool towards_max, const OhplGa4Host *host, const OhplGa4Sink *sink);
	void ohpl_ga4_door_update(OhplGa4Door *s, const OhplGa4DoorJoint *joints, int n, int n_joints, float dt, bool interacted, const OhplGa4Host *host, const OhplGa4Sink *sink);
	void ohpl_ga4_door_on_health_change(OhplGa4Door *s, const OhplGa4DoorParams *p, float health, bool has_main, const OhplGa4Sink *sink);
	void ohpl_ga4_door_reset(OhplGa4Door *s, const OhplGa4DoorJoint *joints, int n, int n_joints, const OhplGa4Host *host, const OhplGa4Sink *sink);

	void ohpl_ga4_wheel_load(const OhplGa4Vars *v, OhplGa4WheelParams *out);
	void ohpl_ga4_wheel_load_instance(const OhplGa4Vars *v, OhplGa4Wheel *s, OhplGa4WheelParams *p, const OhplGa4Host *host, const OhplGa4Sink *sink);
	bool ohpl_ga4_wheel_can_interact(const OhplGa4WheelParams *p, float mass);
	void ohpl_ga4_wheel_setup(const OhplGa4WheelParams *p, const OhplGa4Sink *sink);
	void ohpl_ga4_wheel_set_stuck_state(OhplGa4Wheel *s, const OhplGa4WheelParams *p, int state, bool effects, const OhplGa4Host *host, const OhplGa4Sink *sink);
	void ohpl_ga4_wheel_set_angle(OhplGa4Wheel *s, const OhplGa4WheelParams *p, float x, bool auto_move, const OhplGa4Sink *sink);
	void ohpl_ga4_wheel_on_connection_state(OhplGa4Wheel *s, const OhplGa4WheelParams *p, int state, const OhplGa4Host *host, const OhplGa4Sink *sink);
	bool ohpl_ga4_wheel_on_interact(OhplGa4Wheel *s, const OhplGa4WheelParams *p, bool show_hints, const OhplGa4Host *host, const OhplGa4Sink *sink);
	void ohpl_ga4_wheel_reset_auto_move(OhplGa4Wheel *s, const OhplGa4Sink *sink);
	void ohpl_ga4_wheel_update(OhplGa4Wheel *s, const OhplGa4WheelParams *p, float dt, bool interacted, const OhplGa4Host *host, const OhplGa4Sink *sink);

	void ohpl_ga4_lever_load(const OhplGa4Vars *v, OhplGa4LeverParams *out);
	void ohpl_ga4_lever_load_instance(const OhplGa4Vars *v, OhplGa4Lever *s, OhplGa4LeverParams *p, const OhplGa4Sink *sink);
	bool ohpl_ga4_lever_can_interact(const OhplGa4LeverParams *p, float mass);
	void ohpl_ga4_lever_setup(OhplGa4Lever *s, const OhplGa4LeverParams *p, float min, float max, const OhplGa4Sink *sink);
	void ohpl_ga4_lever_calculate_middle_angle(OhplGa4Lever *s, const OhplGa4LeverParams *p);
	void ohpl_ga4_lever_set_stuck_state(OhplGa4Lever *s, const OhplGa4LeverParams *p, int state, bool effects, const OhplGa4Sink *sink);
	void ohpl_ga4_lever_on_connection_state(OhplGa4Lever *s, const OhplGa4LeverParams *p, int state, const OhplGa4Sink *sink);
	bool ohpl_ga4_lever_on_interact(OhplGa4Lever *s, const OhplGa4LeverParams *p, bool show_hints, const OhplGa4Sink *sink);
	void ohpl_ga4_lever_update(OhplGa4Lever *s, const OhplGa4LeverParams *p, float dt, bool interacted, const OhplGa4Host *host, const OhplGa4Sink *sink);

	void ohpl_ga4_lamp_load(const OhplGa4Vars *v, OhplGa4LampParams *out);
	void ohpl_ga4_lamp_state_new(OhplGa4Lamp *s);
	void ohpl_ga4_lamp_load_instance(const OhplGa4Vars *v, OhplGa4Lamp *s, OhplGa4Base *base, const OhplGa4EffectsIn *env, const OhplGa4LightData *lights, int n_lights, const OhplGa4Sink *sink);
	void ohpl_ga4_lamp_set_lit(OhplGa4Lamp *s, OhplGa4Base *base, const OhplGa4EffectsIn *env, const OhplGa4LightData *lights, int n_lights, bool lit, bool use_effects, const OhplGa4Sink *sink);
	bool ohpl_ga4_lamp_can_interact(const OhplGa4Lamp *s, const OhplGa4LampParams *p, bool has_interact_callback);
	bool ohpl_ga4_lamp_can_ignite(const OhplGa4Lamp *s, const OhplGa4LampParams *p);
	int ohpl_ga4_lamp_focus_crosshair(const OhplGa4Lamp *s, const OhplGa4LampParams *p, bool custom_is_default, bool has_callback);
	float ohpl_ga4_lamp_sanity_add(float sanity, float min_increase, float max_increase);
	void ohpl_ga4_lamp_on_connection_state(OhplGa4Lamp *s, OhplGa4Base *base, const OhplGa4EffectsIn *env, const OhplGa4LightData *lights, int n_lights, int state, const OhplGa4Sink *sink);
	float ohpl_ga4_lamp_fade_target(const float *light, float light_radius, const float *rgba, float radius, float *out_color);
	void ohpl_ga4_lamp_light_connection_color(const float *default_color, const OhplGa4LampLink *links, int n, float *out);

	void ohpl_ga4_multi_load(const OhplGa4Vars *v, OhplGa4MultiParams *out);
	void ohpl_ga4_multi_state_new(OhplGa4Multi *s);
	int ohpl_ga4_multi_compute_states(float min, float max, int n, float *out);
	bool ohpl_ga4_multi_can_interact(const OhplGa4MultiParams *p, float mass);
	void ohpl_ga4_multi_set_stuck_state(OhplGa4Multi *s, const float *states, int n, int state, bool effects, const OhplGa4Sink *sink);
	void ohpl_ga4_multi_on_interact(OhplGa4Multi *s, const float *states, int n, const OhplGa4Sink *sink);
	void ohpl_ga4_multi_update(OhplGa4Multi *s, const OhplGa4MultiParams *p, const float *states, int n, float dt, bool interacted, const OhplGa4Host *host, const OhplGa4Sink *sink);
	void ohpl_ga4_multi_load_instance(const OhplGa4Vars *v, OhplGa4Multi *s, const float *states, int n, const OhplGa4Sink *sink);

	void ohpl_ga4_move_load(const OhplGa4Vars *v, OhplGa4MoveParams *out);
	void ohpl_ga4_move_load_instance(const OhplGa4Vars *v);
	void ohpl_ga4_move_setup(OhplGa4MoveObj *s, const OhplGa4MoveParams *p, const float *body, const float *local_min, const float *local_max);
	void ohpl_ga4_move_set_angular_offset(OhplGa4MoveObj *s, const OhplGa4MoveParams *p, const float *pos);
	float ohpl_ga4_move_get_state(const OhplGa4MoveObj *s, const OhplGa4MoveParams *p, const float *body);
	void ohpl_ga4_move_to_state(const OhplGa4MoveObj *s, const OhplGa4MoveParams *p, OhplGa4Base *base, float state, float acc, float max_speed, float slowdown, bool reset, const OhplGa4Sink *sink);
	void ohpl_ga4_move_to_state_auto(const OhplGa4MoveObj *s, const OhplGa4MoveParams *p, OhplGa4Base *base, const float *body, float state, const OhplGa4Sink *sink);
	void ohpl_ga4_move_update_auto_move(OhplGa4MoveObj *s, const OhplGa4MoveParams *p, OhplGa4Base *base, const float *body, const OhplGa4Sink *sink);
	void ohpl_ga4_move_on_connection_state(const OhplGa4MoveObj *s, const OhplGa4MoveParams *p, OhplGa4Base *base, const float *body, int state, const OhplGa4Sink *sink);
}

#endif
