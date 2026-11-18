#ifndef LUX_RUST_UI_A11B_H
#define LUX_RUST_UI_A11B_H

// C ABI of rust/src/luxui/handlers_ffi.rs and host.rs: the hint and message handlers live in Rust;
// the C++ shells hold a handle and forward. OhplA11bHost is the engine side of host.rs.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct OhplA11bBytes
{
	const unsigned char *p;
	size_t n;
};

typedef void (*OhplA11bSink)(void*, const unsigned char*, size_t);
typedef void (*OhplA11bSink32)(void*, const unsigned*, size_t);

struct OhplA11bHost
{
	void (*menu_str)(OhplA11bBytes, OhplA11bBytes, OhplA11bBytes, OhplA11bSink, void*);
	float (*menu_f32)(OhplA11bBytes, OhplA11bBytes, float);
	void (*menu_vec2)(OhplA11bBytes, OhplA11bBytes, float, float*);
	int (*user_bool)(OhplA11bBytes, OhplA11bBytes, int);
	void (*set_user_bool)(OhplA11bBytes, OhplA11bBytes, int);
	void (*wrap_rows)(size_t, float, float, const float*, const unsigned*, size_t, OhplA11bSink32, void*);
	float (*font_len)(size_t, const float*, const unsigned*, size_t);
	void (*font_range)(size_t, unsigned*);
	int (*font_glyph)(size_t, unsigned, float*);
	void (*parse_string)(const unsigned*, size_t, OhplA11bSink32, void*);
	float (*string_duration)(const unsigned*, size_t);
	void (*gui_sound)(OhplA11bBytes);
	void (*set_update_active)(int);
	void (*input_name)(OhplA11bBytes, OhplA11bSink32, void*);
	void (*hud)(float*);
	int (*ptest)();
	void (*draw_font)(size_t, size_t, const float*, const float*, const float*, int, int, const unsigned*, size_t, int);
	void (*translate)(OhplA11bBytes, OhplA11bBytes, OhplA11bSink32, void*);
	int (*chinese)();
	void (*callback)(size_t, int);
	void (*log_gfx)(const float*, const float*, const float*, int);
	void (*ans)(OhplA11bBytes, OhplA11bBytes, OhplA11bBytes);
};

extern "C"
{
	void ohpl_a11b_set_host(const OhplA11bHost *apHost);

	uint64_t ohpl_a11b_hint_new();
	void ohpl_a11b_hint_free(uint64_t h);
	void ohpl_a11b_hint_font(uint64_t h, size_t font);
	void ohpl_a11b_hint_reset(uint64_t h);
	void ohpl_a11b_hint_load_user_config(uint64_t h);
	void ohpl_a11b_hint_save_user_config(uint64_t h);
	void ohpl_a11b_hint_update(uint64_t h, float dt);
	void ohpl_a11b_hint_draw(uint64_t h, size_t set);
	void ohpl_a11b_hint_set_active(uint64_t h, int on);
	int ohpl_a11b_hint_is_active(uint64_t h);
	int ohpl_a11b_hint_add(uint64_t h, const char *name, size_t nn, const unsigned *text, size_t tn, float time);
	int ohpl_a11b_hint_remove(uint64_t h, const char *name, size_t n);
	void ohpl_a11b_hint_block(uint64_t h, const char *name, size_t n, int on);
	void ohpl_a11b_hint_names(uint64_t h, int blocked, OhplA11bSink sink, void *ctx);
	void ohpl_a11b_hint_names_clear(uint64_t h, int blocked);
	void ohpl_a11b_hint_names_add(uint64_t h, int blocked, const char *name, size_t n);

	uint64_t ohpl_a11b_msg_new();
	void ohpl_a11b_msg_free(uint64_t h);
	void ohpl_a11b_msg_font(uint64_t h, size_t font);
	void ohpl_a11b_msg_reset(uint64_t h);
	void ohpl_a11b_msg_load_user_config(uint64_t h);
	void ohpl_a11b_msg_save_user_config(uint64_t h);
	void ohpl_a11b_msg_update(uint64_t h, float dt);
	void ohpl_a11b_msg_draw(uint64_t h);
	void ohpl_a11b_msg_start_quest(uint64_t h);
	void ohpl_a11b_msg_start_pause(uint64_t h, const unsigned *text, size_t n, int yes_no, size_t cb);
	void ohpl_a11b_msg_set_message(uint64_t h, const unsigned *text, size_t n, float time);
	void ohpl_a11b_msg_do_action(uint64_t h, int action, int pressed);
	void ohpl_a11b_msg_set_pause(uint64_t h, int on);
	int ohpl_a11b_msg_flag(uint64_t h, int which);
	void ohpl_a11b_msg_set_flag(uint64_t h, int which, int on);
}

void OhplA11bInitHost();

inline const unsigned *OhplA11bW(const std::wstring &asX)
{
	return (const unsigned*)asX.data();
}

#endif
