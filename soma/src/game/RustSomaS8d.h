#ifndef RUST_SOMA_S8D_H
#define RUST_SOMA_S8D_H

#include <cstddef>
#include <cstdint>

struct OhplS8dIm;

struct OhplS8dGfx
{
	const unsigned char *file;
	size_t file_n;
	int type, material;
	float uv_min[2], uv_max[2], alpha_limit[2];
};

struct OhplS8dDraw
{
	const uint32_t *text;
	size_t text_n;
	float pos[3], size[2], color[4];
	int material, align;
	float angle;
};

struct OhplS8dTimer
{
	uint64_t id;
	float time;
	bool repeat, touched;
};
static_assert(sizeof(OhplS8dTimer) == 16, "OhplS8dTimer layout");

typedef void (*OhplS8dBytes)(void *, const unsigned char *, size_t);
typedef void (*OhplS8dWords)(void *, const uint32_t *, size_t);
typedef void (*OhplS8dSub)(void *, int kind, int code, const unsigned char *name, size_t n);

struct OhplS8dHost
{
	void (*string)(size_t, OhplS8dBytes, void *);
	size_t (*intern)(const unsigned char *, size_t);
	size_t (*gfx_element)(const OhplS8dGfx *);
	void (*gfx_name)(size_t, OhplS8dBytes, void *);
	size_t (*white)();
	size_t (*font)(const unsigned char *, size_t);
	void (*font_name)(size_t, OhplS8dBytes, void *);
	float (*font_length)(size_t, const float *, const uint32_t *, size_t);
	void (*font_wrap)(size_t, float, float, const float *, const uint32_t *, size_t, OhplS8dWords, void *);
	void (*draw_font)(size_t, size_t, const OhplS8dDraw *);
	void (*screen_size)(float *, int *);
	size_t (*create_set)(const unsigned char *, size_t);
	void (*set_hud_virtual)(size_t, const float *, const float *);
	void (*set_render)(size_t);
	void (*wstr)(size_t, OhplS8dWords, void *);
	size_t (*array_size)(size_t);
	size_t (*array_at)(size_t, size_t);
	void (*array_resize)(size_t, size_t);
	void (*array_push_wstring)(size_t, const uint32_t *, size_t);
	void (*array_push_bool)(size_t, bool);
	void (*array_push_i32)(size_t, int);
	int (*action_subs)(const unsigned char *, size_t, OhplS8dSub, void *);
	void (*translate_button)(const unsigned char *, size_t, OhplS8dWords, void *);
	void (*key_name)(int, OhplS8dBytes, void *);
	float (*string_duration)(const uint32_t *, size_t);
	void (*camtex_create)(const unsigned char *, size_t, int, int, unsigned, float, float, float);
	void (*camtex_destroy)(const unsigned char *, size_t);
	void (*camtex_settings)(const unsigned char *, size_t, float, float, float);
	void (*camtex_matrix)(const unsigned char *, size_t, size_t);
	void (*camtex_attach)(const unsigned char *, size_t, size_t);
};

struct OhplS8dState;

extern "C"
{
	void ohpl_s8d_install(const OhplS8dHost *);
	void ohpl_s8d_register(void *engine);
	OhplS8dIm *ohpl_s8d_new(const unsigned char *name, size_t n, size_t set);
	OhplS8dIm *ohpl_s8d_hud();
	void ohpl_s8d_draw_imguis();
	void ohpl_s8d_render_imguis();
	OhplS8dIm *ohpl_s8d_current();
	void ohpl_s8d_set_current(OhplS8dIm *);
	OhplS8dIm *ohpl_s8d_input_focus();
	OhplS8dIm *ohpl_s8d_script_input_focus();
	OhplS8dIm *ohpl_s8d_prev_input_focus();
	void ohpl_s8d_set_input_focus(OhplS8dIm *, bool show_mouse);
	void ohpl_s8d_update_focus_history();
	bool ohpl_s8d_game_hud_focus();
	void ohpl_s8d_set_game_hud_focus(bool);
	size_t ohpl_s8d_set(OhplS8dIm *);
	size_t ohpl_s8d_name(OhplS8dIm *);
	void ohpl_s8d_begin(OhplS8dIm *, float dt);
	void ohpl_s8d_end(OhplS8dIm *);
	void ohpl_s8d_draw_all(OhplS8dIm *);
	size_t ohpl_s8d_drawn(OhplS8dIm *);
	void ohpl_s8d_debug_ops(OhplS8dIm *, size_t max, OhplS8dBytes, void *);
	void ohpl_s8d_send_mouse(OhplS8dIm *, int x, int y, int rx, int ry);
	void ohpl_s8d_send_mouse_virtual(OhplS8dIm *, const float *pos, const float *rel);
	float *ohpl_s8d_mouse_pos(OhplS8dIm *);
	float *ohpl_s8d_cursor_3d(OhplS8dIm *);
	float *ohpl_s8d_screen_clear(OhplS8dIm *, bool offline);
	bool ohpl_s8d_show_mouse(OhplS8dIm *);
	void ohpl_s8d_set_show_mouse(OhplS8dIm *, bool);
	OhplS8dState *ohpl_s8d_state(OhplS8dIm *, uint64_t id);
	size_t ohpl_s8d_state_count(OhplS8dIm *);
	OhplS8dState *ohpl_s8d_state_at(OhplS8dIm *, size_t i, uint64_t *id);
	void ohpl_s8d_state_op(OhplS8dIm *, uint64_t id, int op, int kind, const void *in, void *out);
	size_t ohpl_s8d_timer_count(OhplS8dIm *);
	const OhplS8dTimer *ohpl_s8d_timer_at(OhplS8dIm *, size_t i);
	void ohpl_s8d_timer_push(OhplS8dIm *, const OhplS8dTimer *);
	void ohpl_s8d_fade(OhplS8dIm *, uint64_t id, int type, const float *goal, float time, int easing, bool replace);
	void ohpl_s8d_stop_fade(OhplS8dIm *, uint64_t id);
	void ohpl_s8d_preload_gfx(const unsigned char *file, size_t n, int type);
	void ohpl_s8d_parse_string(const uint32_t *text, size_t n, OhplS8dWords, void *);
}

#endif
