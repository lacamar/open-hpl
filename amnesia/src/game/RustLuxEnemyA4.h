#ifndef LUX_RUST_ENEMY_A4_H
#define LUX_RUST_ENEMY_A4_H

// C ABI of rust/src/luxenemy/a4_*.rs: the iLuxEnemy update/animation/sound/glow/damage logic lives in a Rust POD
// embedded in the enemy (members are references into it); engine objects go through the host table.

#include "RustLuxEnemy.h"

#ifndef OHPL_A4_NO_FWD
namespace hpl
{
class cAnimationState;
class cSoundEntity;
}
using hpl::cAnimationState;
using hpl::cSoundEntity;
#endif

struct OhplA4Io
{
	void *p[3];
	int32_t i[4];
	float f[12];
};

struct OhplA4Host
{
	void *ctx;
	int32_t (*call)(void *ctx, uint32_t op, OhplA4Io *io);
};

enum OhplA4Kind : int32_t
{
	kOhplA4Kind_Grunt = 0,
	kOhplA4Kind_WaterLurker = 1,
	kOhplA4Kind_ManPig = 2,
};

enum OhplA4Op : uint32_t
{
	kOhplA4Op_GA3 = 1,
	kOhplA4Op_STEP_SIZE = 2,
	kOhplA4Op_UPDATE_STEP = 3,
	kOhplA4Op_ANIM_NAME_EQ = 10,
	kOhplA4Op_ANIM_IS_ACTIVE = 11,
	kOhplA4Op_ANIM_IS_OVER = 12,
	kOhplA4Op_ANIM_FIND = 13,
	kOhplA4Op_WARN_NO_ANIM = 14,
	kOhplA4Op_ANIM_SET_ACTIVE = 15,
	kOhplA4Op_ANIM_FADE_OUT = 16,
	kOhplA4Op_ANIM_IS_FADING = 17,
	kOhplA4Op_ANIM_SET_WEIGHT = 18,
	kOhplA4Op_ANIM_FADE_IN = 19,
	kOhplA4Op_ANIM_SET_LOOP = 20,
	kOhplA4Op_ANIM_REL_TIME = 21,
	kOhplA4Op_ANIM_SET_REL_TIME = 22,
	kOhplA4Op_ANIM_SET_TIME = 23,
	kOhplA4Op_ANIM_IS_LOOPING = 24,
	kOhplA4Op_ANIM_PREV_TIME = 25,
	kOhplA4Op_ANIM_EVENT_TIME = 26,
	kOhplA4Op_ANIM_TIME = 27,
	kOhplA4Op_ANIM_SET_SPEED = 28,
	kOhplA4Op_ANIM_LENGTH = 29,
	kOhplA4Op_SOUND_EXISTS = 40,
	kOhplA4Op_SOUND_FADE_OUT = 41,
	kOhplA4Op_SOUND_CREATE = 42,
	kOhplA4Op_SOUND_ID = 43,
	kOhplA4Op_AMBIENT_EMPTY = 44,
	kOhplA4Op_CAM_COLLIDE_MESH = 50,
	kOhplA4Op_CAM_POS = 51,
	kOhplA4Op_PLAYER_SANITY = 52,
	kOhplA4Op_SUBMESH_NUM = 53,
	kOhplA4Op_ADD_GLOW = 54,
	kOhplA4Op_LIGHT_LEVEL_AT = 55,
	kOhplA4Op_MESH_VISIBLE = 56,
	kOhplA4Op_FADE_TO_SMOKE = 60,
	kOhplA4Op_GROUND_RAY = 62,
	kOhplA4Op_SET_ENTITY_OFFSET = 63,
	kOhplA4Op_DEBUG_DAMAGE = 70,
	kOhplA4Op_DAMAGE_MUL = 71,
	kOhplA4Op_ON_DAMAGE = 72,
	kOhplA4Op_CALLBACK_DEATH = 73,
	kOhplA4Op_ATTACK_SHAPE = 74,
	kOhplA4Op_ATTACK_HIT_SOUND = 75,
	kOhplA4Op_ON_SET_ACTIVE_SPECIFIC = 80,
	kOhplA4Op_BODY_EXISTS = 81,
	kOhplA4Op_MESH_EXISTS = 82,
	kOhplA4Op_MESH_SET_ACTIVE = 83,
	kOhplA4Op_ON_RESET_PROPERTIES = 84,
	kOhplA4Op_BODY_SET_FEET = 85,
	kOhplA4Op_FRUSTUM_BODY = 90,
	kOhplA4Op_FRUSTUM_BODY_AT = 91,
	kOhplA4Op_FOOD_BEGIN = 100,
	kOhplA4Op_FOOD_IS_OBJECT = 101,
	kOhplA4Op_FOOD_IS_FOOD = 102,
	kOhplA4Op_FOOD_LOWEST_Y = 103,
	kOhplA4Op_FOOD_POS = 104,
	kOhplA4Op_BODY_RIGHT_UP_FWD = 110,
	kOhplA4Op_PATROL_COUNT = 120,
};

struct OhplA4Base
{
	OhplGa3Ctx *c;
	cAnimationState *curAnim;
	cSoundEntity *curSound;
	int32_t kind;
	float glowAlpha, glowGoal, glowUpdateCount, glowMaxDist;
	float hallucinationEndDist, groundRayCount, animSpeedMul, ambientCount;
	float moveSpeedAnimMul;
	int32_t groundN, soundState, curSoundId, toughness;
	float ground[4];
	float ambientMin[4], ambientMax[4];
	bool hallucination, alignGroundRay, useAnimations, animSpeedDependant;
	bool useMoveAnimWhenOver, causesSanity, causesSanityDefault;
	uint8_t pad;
};

struct OhplA4Grunt
{
	OhplA4Base b;
};
struct OhplA4ManPig
{
	OhplA4Base b;
};
struct OhplA4WaterLurker
{
	OhplA4Base b;
};

static_assert(sizeof(OhplA4Io) == 88, "A4 layout");
static_assert(sizeof(OhplA4Base) == 136, "A4 layout");

template<class T> struct OhplA4Holder
{
	T mA4 = {};
};

extern "C" {
void ohpl_a4_init(OhplA4Base *b, OhplGa3Ctx *c, int32_t kind);
void ohpl_a4_on_update(OhplA4Base *b, const OhplA4Host *h, float dt);
void ohpl_a4_give_damage(OhplA4Base *b, const OhplA4Host *h, float amount, int32_t strength);
void ohpl_a4_alert_of_player_presence(OhplA4Base *b, const OhplA4Host *h);
void ohpl_a4_show_player_position(OhplA4Base *b, const OhplA4Host *h);
void ohpl_a4_change_state(OhplA4Base *b, int32_t state);
bool ohpl_a4_play_anim(OhplA4Base *b, const OhplA4Host *h, const char *name, bool loop, float fade, bool dependsOnSpeed, float speedMul, bool sync, bool overideMove, bool useMoveWhenOver);
bool ohpl_a4_play_anim_impl(OhplA4Base *b, const OhplA4Host *h, const char *name, bool loop, float fade, bool dependsOnSpeed, float speedMul, bool sync, bool useMoveWhenOver);
void ohpl_a4_fade_out_current_anim(OhplA4Base *b, const OhplA4Host *h, float fade);
bool ohpl_a4_fade_out_current_anim_impl(OhplA4Base *b, const OhplA4Host *h, float fade);
float ohpl_a4_anim_absolute_time(OhplA4Base *b, const OhplA4Host *h, float rel);
void ohpl_a4_reset_properties(OhplA4Base *b, const OhplA4Host *h);
void ohpl_a4_change_sound_state(OhplA4Base *b, const OhplA4Host *h, int32_t state);
void ohpl_a4_update_sound_state(OhplA4Base *b, const OhplA4Host *h, float dt);
void ohpl_a4_update_animation(OhplA4Base *b, const OhplA4Host *h, float dt);
void ohpl_a4_update_darkness_glow(OhplA4Base *b, const OhplA4Host *h, float dt);
void ohpl_a4_update_hallucination(OhplA4Base *b, const OhplA4Host *h);
void ohpl_a4_update_align_ground_ray(OhplA4Base *b, const OhplA4Host *h, float dt);
bool ohpl_a4_attack(OhplA4Base *b, const OhplA4Host *h, const void *size, const void *damage, float mul);
void ohpl_a4_set_move_speed(OhplA4Base *b, int32_t speed);
void ohpl_a4_damage_shape_matrix(OhplA4Base *b, const OhplA4Host *h, const float *offset, float *out16);
void *ohpl_a4_closest_food(OhplA4Base *b, const OhplA4Host *h, float maxDist, float maxHeightDist);
float ohpl_a4_dist_2d(OhplA4Base *b, const OhplA4Host *h, const float *pos);
float ohpl_a4_dist_to_char(OhplA4Base *b, const OhplA4Host *h, const float *feet);
float ohpl_a4_dist_to_char_2d(OhplA4Base *b, const OhplA4Host *h, const float *feet);
float ohpl_a4_abs_height_dist(OhplA4Base *b, const OhplA4Host *h, const float *feet);
float ohpl_a4_abs_height_dist_to_player(OhplA4Base *b, const OhplA4Host *h);
void ohpl_a4_direction_2d(OhplA4Base *b, const OhplA4Host *h, const float *pos, float *out3);
bool ohpl_a4_is_seen_by_player(OhplA4Base *b, const OhplA4Host *h);
bool ohpl_a4_in_player_fov_at_feet(OhplA4Base *b, const OhplA4Host *h, const float *feet);
bool ohpl_a4_visible_to_player_at_feet(OhplA4Base *b, const OhplA4Host *h, const float *feet);
bool ohpl_a4_outside_start_radius(OhplA4Base *b, const OhplA4Host *h);
void ohpl_a4_on_set_active(OhplA4Base *b, const OhplA4Host *h, bool active);
}

#endif
