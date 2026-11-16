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
}

#endif
