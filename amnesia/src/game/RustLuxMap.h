#ifndef LUX_RUST_LUX_MAP_H
#define LUX_RUST_LUX_MAP_H

// C ABI of rust/src/luxmap/. Texts are (pointer, length) in; results leave through sinks.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct OhplGa5Plane
{
	float a, b, c, d;
};

struct OhplGa5Vec3
{
	float x, y, z;
};

struct OhplGa5Mtx
{
	float m[4][4];
};

struct OhplGa5BuoyIn
{
	bool inside, allowed, active, hasWaves;
};

struct OhplGa5BuoyOut
{
	bool deactivate, setup, waves;
};

struct OhplGa5StickyIn
{
	OhplGa5Mtx start, areaWorld;
	OhplGa5Vec3 areaPos, massCentre, attachedPos;
	float poseTime, step, setTime;
	bool moveBody, rotateBody;
};

struct OhplGa5StickyOut
{
	OhplGa5Mtx mtx;
	OhplGa5Vec3 pos;
	float setTime;
	int mode;
	bool done;
};

struct OhplGa5ExamineIn
{
	bool hasCat, hasInsaneCat, hasInsaneEntry, hasSound, hasInsaneSound;
};

struct OhplGa5ExamineOut
{
	bool showText, insaneText, playSound, insaneSound;
};

struct OhplGa5LadderSetup
{
	OhplGa5Vec3 forward;
	float maxY, minY;
};

struct OhplGa5PlayerStart
{
	OhplGa5Vec3 pos;
	float angle;
};

struct OhplGa5SubMesh
{
	bool parentIsBody, hasMaterial, translucent, shadowCaster;
};

struct OhplGa5DamageFilter
{
	bool active, collide, hasUserData, checkProps, isChar, isPlayerBody, checkPlayer, checkEnemies;
};

enum { kOhplGa5AttachNone, kOhplGa5AttachSetMatrix, kOhplGa5AttachSetPosition };
enum { kOhplGa5CharNone, kOhplGa5CharEnter, kOhplGa5CharLeave };
enum { kOhplGa5InsideProceed, kOhplGa5InsideSkip, kOhplGa5InsideForgetLast };
enum { kOhplGa5RopeReturn, kOhplGa5RopeRelink, kOhplGa5RopeCreate };
enum { kOhplGa5ScriptNone, kOhplGa5ScriptPlain, kOhplGa5ScriptCompressed };
enum { kOhplGa5EnterStart = 1, kOhplGa5EnterCompletion = 2, kOhplGa5EnterEnter = 4 };
enum { kOhplGa5ListEnemy = 1, kOhplGa5ListSticky = 2 };
enum
{
	kOhplGa5CompleteNone, kOhplGa5CompletePuzzle, kOhplGa5CompleteNote, kOhplGa5CompleteDiary,
	kOhplGa5CompleteChest, kOhplGa5CompleteFlashback,
};
enum { kOhplGa5EventEscapeArtist = 1, kOhplGa5EventDescendant = 2, kOhplGa5EventSanity = 4 };
enum
{
	kOhplGa5SetLeave = 1, kOhplGa5SetDestroyWorldEntities, kOhplGa5SetMapLeave, kOhplGa5SetAssign,
	kOhplGa5SetCreateWorldEntities, kOhplGa5SetMapEnter, kOhplGa5SetPlacePlayer, kOhplGa5SetAutoCheckpoint,
	kOhplGa5SetEnter, kOhplGa5SetViewWorld, kOhplGa5SetViewNoWorld,
};
enum { kOhplGa5ClosestReject, kOhplGa5ClosestAccept, kOhplGa5ClosestAsk };
enum { kOhplGa5TargetNone, kOhplGa5TargetEntity, kOhplGa5TargetPlayer };
enum { kOhplGa5LightBox, kOhplGa5LightNeedsLos, kOhplGa5LightFalloff };

typedef void (*OhplGa5Sink8)(void *, const unsigned char *, size_t);
typedef bool (*OhplGa5FitsFn)(void *, const OhplGa5Vec3 *);

namespace ohpl_ga5
{
inline void Collect(void *ctx, const unsigned char *p, size_t n)
{
	static_cast<std::string *>(ctx)->assign(reinterpret_cast<const char *>(p), n);
}
}

extern "C" {
	void ohpl_ga5_call1(const char *func, size_t func_n, const char *name, size_t name_n, OhplGa5Sink8 sink, void *ctx);
	void ohpl_ga5_call2(const char *func, size_t func_n, const char *a, size_t a_n, const char *b, size_t b_n, OhplGa5Sink8 sink, void *ctx);
}

namespace ohpl_ga5
{
inline std::string Call1(const std::string &asFunc, const std::string &asName)
{
	std::string s;
	ohpl_ga5_call1(asFunc.data(), asFunc.size(), asName.data(), asName.size(), Collect, &s);
	return s;
}
inline std::string Call2(const std::string &asFunc, const std::string &asA, const std::string &asB)
{
	std::string s;
	ohpl_ga5_call2(asFunc.data(), asFunc.size(), asA.data(), asA.size(), asB.data(), asB.size(), Collect, &s);
	return s;
}
}

extern "C" {
	// area
	bool ohpl_ga5_step_too_small(float step, float engine);
	bool ohpl_ga5_body_skipped(bool collide, bool active, float mass, bool is_char);
	float ohpl_ga5_liquid_surface_y(float pos_y, float size_y);
	float ohpl_ga5_wave_surface_y(float time, float freq, float px, float pz, float amp, float surface_y);
	int ohpl_ga5_liquid_char(bool is_player, bool has_enemy, bool inside, bool in_water);
	int ohpl_ga5_sticky_inside(bool inside, bool is_last);
	bool ohpl_ga5_sticky_after_callback(bool has_callback, bool allow);
	int ohpl_ga5_script_crosshair(bool has_callback, int custom, int def, int grab);
	bool ohpl_ga5_script_can_interact(bool item, bool has_callback);
	int ohpl_ga5_rope_after_world(bool created, bool nothing_linked);
	int ohpl_ga5_rope_max_segments(float max_len, float segment);
	void ohpl_ga5_liquid_surface_plane(float pos_y, float size_y, OhplGa5Plane *out);
	void ohpl_ga5_wave_plane(float surface_y, OhplGa5Plane *out);
	void ohpl_ga5_liquid_body(const OhplGa5BuoyIn *i, float dist_sqr, float max_dist_sqr, OhplGa5BuoyOut *out);
	bool ohpl_ga5_sticky_name(const char *attach, size_t attach_n, const char *body, size_t body_n, bool *allow);
	void ohpl_ga5_sticky_attach(const OhplGa5StickyIn *i, OhplGa5StickyOut *out);
	bool ohpl_ga5_tick(float *count, float step);
	bool ohpl_ga5_tick_random(float *count, float step, float min, float max);
	bool ohpl_ga5_tick_fixed(float *count, float step, float next);
	void ohpl_ga5_examine_tick(float *count, float step);
	void ohpl_ga5_examine_interact(const OhplGa5ExamineIn *i, float sanity, float insane_limit, float sound_count, OhplGa5ExamineOut *out);
	void ohpl_ga5_follow_offset(const OhplGa5Vec3 *offset, const OhplGa5Vec3 *parent, const OhplGa5Vec3 *body, OhplGa5Vec3 *out);
	void ohpl_ga5_attach_offset(const OhplGa5Vec3 *body, const OhplGa5Vec3 *parent, OhplGa5Vec3 *out);
	void ohpl_ga5_ladder_setup(const OhplGa5Mtx *world, float pos_y, float size_y, OhplGa5LadderSetup *out);
	void ohpl_ga5_ladder_start_pos(const OhplGa5Vec3 *area_pos, const OhplGa5Vec3 *forward, const OhplGa5Vec3 *char_size,
									float char_y, float max_y, float min_y, OhplGa5Vec3 *out);
	bool ohpl_ga5_ladder_find_start(OhplGa5Vec3 *start, float max_y, OhplGa5FitsFn fits, void *ctx);
	void ohpl_ga5_player_start(const OhplGa5Mtx *m, OhplGa5PlayerStart *out);
	void ohpl_ga5_node_up(const OhplGa5Vec3 *t, OhplGa5Vec3 *out);

	// map
	float ohpl_ga5_timer_count(float time);
	float ohpl_ga5_timer_step(float count, float step);
	bool ohpl_ga5_timer_due(float count, bool destroy_me);
	bool ohpl_ga5_timer_remove_marks(bool updating);
	float ohpl_ga5_completion_fraction(int current, int total);
	int ohpl_ga5_quest_completion(int total, int quests, int value);
	float ohpl_ga5_dissolve_speed(float time);
	float ohpl_ga5_checkpoint_music_volume(float volume, float volume_mul);
	float ohpl_ga5_hard_tinderbox_every(int tinderboxes);
	float ohpl_ga5_hard_oil_every();
	int ohpl_ga5_script_choice(bool plain, bool compressed_enabled, bool compressed);
	int ohpl_ga5_enter_plan(bool run_script, bool has_script, bool first_time);
	bool ohpl_ga5_run_script_allowed(bool has_script, bool is_current);
	int ohpl_ga5_entity_lists(bool is_enemy, bool is_area, bool is_sticky);
	int ohpl_ga5_world_load_flags(bool entities, bool phys, bool stat, bool ent, int f0, int f1, int f2, int f3);
	int ohpl_ga5_commentary_sync(bool icons_active, bool show);
	void ohpl_ga5_completion_add(int *current, int total, int amount, float *prev, float *now);
	int ohpl_ga5_completion_value(int kind, const int *values);
	bool ohpl_ga5_dissolve_step(float *alpha, float speed, float step);
	bool ohpl_ga5_hard_remove(float *counter, float every);
	int ohpl_ga5_free_entity_id(const int *ids, size_t n);
	bool ohpl_ga5_sound_volume(float dist, float min, float max, float volume, float *out);
	bool ohpl_ga5_callback_remove_ok(const char *name, size_t name_n, const char *cb, size_t cb_n);
	bool ohpl_ga5_use_item_matches(const char *item, size_t item_n, const char *ent, size_t ent_n,
									const char *want_item, size_t want_item_n, const char *want_ent, size_t want_ent_n);
	bool ohpl_ga5_checkpoint_music_pick(bool looped, bool special, const char *file, size_t file_n);
	bool ohpl_ga5_sound_prefix_matches(const char *type_name, size_t type_n, const char *name, size_t name_n);
	void ohpl_ga5_checkpoint_call(int count, const char *cb, size_t cb_n, const char *name, size_t name_n, OhplGa5Sink8 sink, void *ctx);
	void ohpl_ga5_compile_errors(const char *msgs, size_t msgs_n, OhplGa5Sink8 sink, void *ctx);

	// handler
	bool ohpl_ga5_is_new_map(const char *current, size_t current_n, const char *file, size_t file_n);
	int ohpl_ga5_pair_events(bool hard, const char *last, size_t last_n, const char *now, size_t now_n);
	void ohpl_ga5_map_name(const char *file, size_t file_n, OhplGa5Sink8 sink, void *ctx);
	void ohpl_ga5_change_file(const char *name, size_t name_n, OhplGa5Sink8 sink, void *ctx);
	float ohpl_ga5_load_seconds(uint64_t start_ms, uint64_t now_ms);
	bool ohpl_ga5_first_time(bool saved_exists);
	bool ohpl_ga5_commentary_stops(bool show);
	bool ohpl_ga5_change_proceeds(bool active, bool fading);
	size_t ohpl_ga5_set_current_plan(bool has_current, bool has_new, bool same, unsigned char *out);
	bool ohpl_ga5_pause_sounds(bool *paused);
	bool ohpl_ga5_resume_sounds(bool *paused);

	// helper
	bool ohpl_ga5_los_before(bool is_char, bool collide, bool check_shadow, bool blocks_light, bool has_mesh_entity,
								const OhplGa5SubMesh *subs, size_t n);
	int ohpl_ga5_closest_entity_before(bool has_entity, bool entity_active, bool collide, bool is_char);
	bool ohpl_ga5_closer(float dist, float closest);
	bool ohpl_ga5_char_collider_before(bool collide_character, bool is_char);
	bool ohpl_ga5_attack_ray_before(bool is_char, bool collide, bool is_skip);
	bool ohpl_ga5_takes_push(float mass, bool is_char);
	int ohpl_ga5_damage_target(bool has_entity, bool is_player_body);
	float ohpl_ga5_max_rgb(float r, float g, float b);
	int ohpl_ga5_light_kind(bool is_box, bool is_spot, bool casts_shadows);
	float ohpl_ga5_light_falloff_add(float level, float amount, float dist, float radius, float radius_add);
	bool ohpl_ga5_damage_skips(const OhplGa5DamageFilter *f);
	void ohpl_ga5_hit_position(const OhplGa5Vec3 *pts, size_t n, OhplGa5Vec3 *out);
	void ohpl_ga5_hit_direction(const OhplGa5Vec3 *hit, const OhplGa5Vec3 *origin, OhplGa5Vec3 *out);
	void ohpl_ga5_char_push(const OhplGa5Vec3 *dir, float force, OhplGa5Vec3 *out);
	void ohpl_ga5_prop_impulse(const OhplGa5Vec3 *dir, float force, float mass, float max_impulse, OhplGa5Vec3 *out);
}

#endif
