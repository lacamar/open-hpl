#ifndef LUX_RUST_LUX_SCRIPT_H
#define LUX_RUST_LUX_SCRIPT_H

// C ABI of rust/src/luxscript/. Inputs are (pointer, length); outputs go through sinks.

#include <cstddef>
#include <string>
#include <vector>

struct OhplGa1Wild;

struct OhplGa1CollideStep
{
	bool transition;
	bool fire;
	int state;
};

enum
{
	kOhplGa1TypeNo, kOhplGa1TypeYes, kOhplGa1TypeNeedArea, kOhplGa1TypeNeedProp,
};
enum { kOhplGa1OpChar, kOhplGa1OpCommand };
enum { kOhplGa1CmdButton = 1, kOhplGa1CmdLook, kOhplGa1CmdBad };
// Ids returned by ohpl_ga1_achievement / ohpl_ga1_quest_achievement
enum
{
	kOhplGa1AchNone, kOhplGa1AchBenefactor, kOhplGa1AchSurvivor, kOhplGa1AchSacrifice, kOhplGa1AchQuitter,
	kOhplGa1AchEgotist, kOhplGa1AchAltruist, kOhplGa1AchVacillator, kOhplGa1AchAlchemist,
	kOhplGa1AchPipeworker, kOhplGa1AchRestorer,
};

typedef void (*OhplGa1Sink8)(void *, const unsigned char *, size_t);
typedef void (*OhplGa1Sink32)(void *, const unsigned int *, size_t);

extern "C" {
	// lookup
	bool ohpl_ga1_has_wildcard(const char *name, size_t n);
	OhplGa1Wild *ohpl_ga1_wild_new(const char *name, size_t n);
	void ohpl_ga1_wild_free(OhplGa1Wild *w);
	bool ohpl_ga1_wild_matches(const OhplGa1Wild *w, const char *name, size_t n);
	int ohpl_ga1_type_prep(int entity_type, int want_type, int want_sub);
	bool ohpl_ga1_type_sub(int got, int want);
	bool ohpl_ga1_body_matches(const char *body, size_t body_n, int entity_name_len, const char *want, size_t want_n);
	bool ohpl_ga1_searches_bodies(const char *want, size_t n, int body_num);

	// variables and strings
	void ohpl_ga1_var_from_int(int x, OhplGa1Sink8 sink, void *ctx);
	void ohpl_ga1_var_from_float(float x, OhplGa1Sink8 sink, void *ctx);
	void ohpl_ga1_var_add_int(const char *cur, size_t n, int d, OhplGa1Sink8 sink, void *ctx);
	void ohpl_ga1_var_add_float(const char *cur, size_t n, float d, OhplGa1Sink8 sink, void *ctx);
	int ohpl_ga1_var_get_int(const char *s, size_t n);
	float ohpl_ga1_var_get_float(const char *s, size_t n);
	int ohpl_ga1_string_to_int(const char *s, size_t n);
	float ohpl_ga1_string_to_float(const char *s, size_t n);
	bool ohpl_ga1_string_to_bool(const char *s, size_t n);
	bool ohpl_ga1_string_contains(const char *s, size_t n, const char *sub, size_t m);

	// tables and small math
	int ohpl_ga1_progress_level(const char *s, size_t n);
	int ohpl_ga1_damage_type(const char *s, size_t n);
	int ohpl_ga1_crosshair(const char *s, size_t n);
	int ohpl_ga1_pose(const char *s, size_t n);
	int ohpl_ga1_achievement(const char *s, size_t n);
	int ohpl_ga1_quest_achievement(const char *s, size_t n);
	bool ohpl_ga1_achievement_hardmode(int id);
	bool ohpl_ga1_sound_is_entity(const char *s, size_t n);
	bool ohpl_ga1_coord_is_local(const char *s, size_t n);
	float ohpl_ga1_sanity_boost(float s);
	float ohpl_ga1_sanity_boost_small(float s);
	float ohpl_ga1_fade_speed(float t);
	float ohpl_ga1_inverse(float t);
	void ohpl_ga1_local_force(const float *f, const float *r, const float *u, float y, float z, float *out);
	int ohpl_ga1_rope_segments(float total, float segment);

	// script math
	float ohpl_ga1_sin(float x);
	float ohpl_ga1_cos(float x);
	float ohpl_ga1_tan(float x);
	float ohpl_ga1_asin(float x);
	float ohpl_ga1_acos(float x);
	float ohpl_ga1_atan(float x);
	float ohpl_ga1_atan2(float x, float y);
	float ohpl_ga1_sqrt(float x);
	float ohpl_ga1_pow(float b, float e);
	float ohpl_ga1_min(float a, float b);
	float ohpl_ga1_max(float a, float b);
	float ohpl_ga1_clamp(float x, float lo, float hi);
	float ohpl_ga1_abs(float x);

	// help funcs
	void ohpl_ga1_parse_plan(const unsigned int *in, size_t n, OhplGa1Sink32 sink, void *ctx);
	float ohpl_ga1_string_duration(float start, float min, float per_char, size_t len);

	// types
	void ohpl_ga1_alpha_reset(void *fader);
	void ohpl_ga1_alpha_update(void *fader, float dt);
	OhplGa1CollideStep ohpl_ga1_collide_step(bool colliding, int states, bool collide);
	void ohpl_ga1_collide_command(const char *func, size_t func_n, const char *name, size_t name_n, const char *entity,
								   size_t entity_n, int state, OhplGa1Sink8 sink, void *ctx);
	void ohpl_ga1_enemy_sound_update(float *count, float dt);
	bool ohpl_ga1_enemy_sound_allowed(float count);
	float ohpl_ga1_enemy_sound_cooldown();
}

inline void OhplGa1SinkStr(void *ctx, const unsigned char *p, size_t n)
{
	static_cast<std::string *>(ctx)->assign(reinterpret_cast<const char *>(p), n);
}

inline void OhplGa1SinkU32(void *ctx, const unsigned int *p, size_t n)
{
	static_cast<std::vector<unsigned int> *>(ctx)->assign(p, p + n);
}

struct OhplGa1WildPtr
{
	OhplGa1Wild *w;
	explicit OhplGa1WildPtr(const std::string &s) : w(ohpl_ga1_wild_new(s.data(), s.size())) {}
	~OhplGa1WildPtr() { ohpl_ga1_wild_free(w); }
	OhplGa1WildPtr(const OhplGa1WildPtr &) = delete;
	OhplGa1WildPtr &operator=(const OhplGa1WildPtr &) = delete;
	bool Matches(const std::string &s) const { return ohpl_ga1_wild_matches(w, s.data(), s.size()); }
};

#define OHPL_GA1_S(s) (s).data(), (s).size()

#endif // LUX_RUST_LUX_SCRIPT_H
