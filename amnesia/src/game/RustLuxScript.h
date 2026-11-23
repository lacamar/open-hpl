#ifndef LUX_RUST_LUX_SCRIPT_H
#define LUX_RUST_LUX_SCRIPT_H

// C ABI of rust/src/luxscript/. Inputs are (pointer, length); outputs go through sinks.

#include <cstddef>
#include <string>
#include <vector>


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
typedef void (*OhplGa1Sink8)(void *, const unsigned char *, size_t);
typedef void (*OhplGa1Sink32)(void *, const unsigned int *, size_t);

extern "C" {
	// lookup
	int ohpl_ga1_type_prep(int entity_type, int want_type, int want_sub);
	bool ohpl_ga1_type_sub(int got, int want);

	// variables and strings

	// tables and small math

	// script math

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

#define OHPL_GA1_S(s) (s).data(), (s).size()

#endif // LUX_RUST_LUX_SCRIPT_H
