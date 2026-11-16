#ifndef SOMA_RUST_SOMA_LUX_S5_H
#define SOMA_RUST_SOMA_LUX_S5_H

// C ABI of rust/src/somalux/s5_*: scriptable timers and faders, input handler.

#include <cstddef>
#include <cstdint>

struct OhplS5Timer
{
	uint64_t id;
	float length, time_left;
	bool repeat, paused;
};

static_assert(sizeof(OhplS5Timer) == 24);

struct OhplS5Io
{
	void *p[2];
	int32_t i[4];
	float f[4];
	const unsigned char *s;
	size_t n;
};

struct OhplS5Host
{
	void *ctx;
	int32_t (*call)(void *ctx, uint32_t op, OhplS5Io *io);
};

enum OhplS5InOp : uint32_t
{
	kOhplS5In_EXISTS_ID = 1, kOhplS5In_EXISTS_NAME, kOhplS5In_DESTROY_ID, kOhplS5In_DESTROY_NAME, kOhplS5In_CREATE, kOhplS5In_CREATE_MOUSE,
	kOhplS5In_ID_OF, kOhplS5In_TRIGGERED, kOhplS5In_ADD_KEY, kOhplS5In_ADD_BUTTON, kOhplS5In_KEY_DOWN, kOhplS5In_KEY_NAME,
	kOhplS5In_BUTTON_DOWN, kOhplS5In_BUTTON_NAME, kOhplS5In_WARN, kOhplS5In_LOG_ACTIONS, kOhplS5In_CALL, kOhplS5In_CALL_KEY_CONFIG,
	kOhplS5In_BROADCAST_ACTION, kOhplS5In_BROADCAST_ANALOG, kOhplS5In_MOUSE_REL, kOhplS5In_SCREEN_HEIGHT, kOhplS5In_USER_CONFIG,
	kOhplS5In_ARR_SIZE, kOhplS5In_ARR_AT, kOhplS5In_STR_REF, kOhplS5In_ENGINE_ACTION_NAME, kOhplS5In_ENGINE_ACTION_BINDS, kOhplS5In_DUMP,
};

// cSomaLuxGame host ops (OhplS5Host, ops below): the scriptable thunks and engine objects the game layer reaches
enum OhplS5cOp : uint32_t
{
	kOhplS5c_DUMP_ENABLED = 1, kOhplS5c_DUMP_LOG, kOhplS5c_SELF_TEST, kOhplS5c_NEW, kOhplS5c_SET_SCRIPT_NAME, kOhplS5c_PLAYER_CAMERA,
	kOhplS5c_LOAD_SCRIPT, kOhplS5c_DELETE, kOhplS5c_SC, kOhplS5c_SCRIPT_OBJECT, kOhplS5c_CFG_STRING, kOhplS5c_CFG_FLOAT,
	kOhplS5c_LOAD_CONFIGS, kOhplS5c_LOAD_LANGUAGE, kOhplS5c_ADD_VOICE, kOhplS5c_PLAYER_EXISTS,
	kOhplS5c_INPUT_EXISTS, kOhplS5c_XML_FLAT, kOhplS5c_PLAYER_ADD_STATE, kOhplS5c_MOUSE_REL, kOhplS5c_DIALOG_UPDATE,
	kOhplS5c_VOICE_UPDATE, kOhplS5c_PAUSED, kOhplS5c_MAP_ACTION, kOhplS5c_UPDATE_CAMERA_TEXTURES, kOhplS5c_LEAVE_MAP,
	kOhplS5c_GUI_FOCUS_HISTORY, kOhplS5c_GUI_FOCUS, kOhplS5c_GUI_POINTER_MODE, kOhplS5c_GUI_SEND_MOUSE, kOhplS5c_GUI_HUD_BEGIN,
	kOhplS5c_GUI_MAP_ON_GUI, kOhplS5c_GUI_HUD_END, kOhplS5c_HUD_FOCUS_SET, kOhplS5c_HUD_FOCUS_GET, kOhplS5c_LOG, kOhplS5c_DUMP_ANALOG, kOhplS5c_SET_SAVED,
};

// kOhplS5c_SC sub-ops: virtuals of cSomaLuxScriptable
enum OhplS5cSc : int32_t
{
	kOhplS5cSc_UPDATE, kOhplS5cSc_POST_UPDATE, kOhplS5cSc_VARIABLE_UPDATE, kOhplS5cSc_GUI, kOhplS5cSc_DRAW, kOhplS5cSc_ACTION,
	kOhplS5cSc_ANALOG, kOhplS5cSc_MAP_MESSAGE, kOhplS5cSc_MESSAGE, kOhplS5cSc_CALL_OBJECT,
};

struct OhplGs3Fader;

extern "C" {
	// scriptable timers and faders: one Rust table per object address
	void ohpl_s5_ts_new(void *self);
	void ohpl_s5_ts_free(void *self);
	bool ohpl_s5_ts_idle(void *self);
	void ohpl_s5_ts_update(void *self, float dt, void (*due)(void *ctx, uint64_t id, const unsigned char *func, size_t n), void *ctx);
	void ohpl_s5_ts_describe(void *self, void (*sink)(void *ctx, const unsigned char *s, size_t n), void *ctx);
	bool ohpl_s5_ts_timer_at(void *self, size_t i, OhplS5Timer *out, void (*sink)(void *ctx, const unsigned char *s, size_t n), void *ctx);
	bool ohpl_s5_ts_fader_at(void *self, size_t i, OhplGs3Fader *out);
	void ohpl_s5_ts_clear(void *self);
	float ohpl_s5_ts_op(void *self, unsigned op, uint64_t id, float f0, float f1, int i0, unsigned flags, const unsigned char *func, size_t n);
	void ohpl_s5_ts_push_timer(void *self, uint64_t id, float length, float time_left, bool repeat, bool paused, const unsigned char *func, size_t n);
	void ohpl_s5_ts_push_fader(void *self, const OhplGs3Fader *f);
	void ohpl_s5_register_timer_natives(void *engine, const char *type);

	// input handler
	void ohpl_s5_in_install_host(const OhplS5Host *host);
	void ohpl_s5_in_new(int max_smooth, float prev_mul, size_t keys, size_t buttons);
	void ohpl_s5_in_load_user_config();
	void ohpl_s5_in_load_script();
	void ohpl_s5_in_load_key_config();
	void ohpl_s5_in_latch();
	void ohpl_s5_in_clear_gui();
	void ohpl_s5_in_update(bool game_input);
	void ohpl_s5_in_dump();
	void ohpl_s5_register_input_natives(void *engine);

	// game layer (cSomaLuxGame)
	void ohpl_s5c_game_install_host(const OhplS5Host *host);
	void ohpl_s5c_game_new();
	void ohpl_s5c_game_free();
	void ohpl_s5c_game_load();
	void ohpl_s5c_game_update(float dt, bool paused);
	void ohpl_s5c_game_post_update(float dt);
	void ohpl_s5c_game_draw(float dt);
	void ohpl_s5c_game_reset_scriptables();
	void ohpl_s5c_game_reload_user_config();
	void ohpl_s5c_game_preload(void *map);
	void ohpl_s5c_game_enter_map(void *map);
	void ohpl_s5c_game_modules_map_enter(void *map);
	void ohpl_s5c_game_leave_map(void *map);
	void ohpl_s5c_game_broadcast_action(int32_t action, bool pressed);
	void ohpl_s5c_game_broadcast_analog(int32_t id, float x, float y, float z);
	void *ohpl_s5c_game_module_by_id(int32_t id);
	void *ohpl_s5c_game_module_by_name(const unsigned char *name, size_t n);
	void *ohpl_s5c_game_effect_by_id(int32_t id);
	void *ohpl_s5c_game_handler(const unsigned char *name, size_t n);
	bool ohpl_s5c_game_entity_script(const unsigned char *group, size_t gn, const unsigned char *type, size_t tn,
									 void (*sink)(void *ctx, const unsigned char *file, size_t fn, const unsigned char *cls, size_t cn), void *ctx);
	float ohpl_s5c_game_interact_distance(int32_t entity_type);
	void ohpl_s5c_game_set_input(bool on);
	void ohpl_s5c_game_mouse_rel(int32_t *x, int32_t *y);
	// natives, split around the C++ registrations that must keep their order
	void ohpl_s5c_game_register1(void *engine);
	void ohpl_s5c_game_register2(void *engine, int32_t module_id_off, int32_t effect_id_off);
	void ohpl_s5c_game_register3(void *engine);
}

#endif
