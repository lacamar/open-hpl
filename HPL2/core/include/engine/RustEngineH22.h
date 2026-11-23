#ifndef HPL_RUST_ENGINE_H22_H
#define HPL_RUST_ENGINE_H22_H

// C ABI of rust/src/engine/h22_hc_ffi.rs and h22_engine_ffi.rs.

#include <cstddef>
#include <cstdint>

#include "engine/RustEngine.h"
#include "system/RustSystem.h"

struct OhplH22Hc;

typedef void (*OhplH22Sink)(void*, const uint8_t*, size_t);

struct OhplH22HcHost
{
	void* ctx;
	uint64_t (*now_ms)(void*);
	uint32_t (*rendered_frames)(void*);
	void (*log)(void*, int, const uint8_t*, size_t); // 0 Log, 1 Warning, 2 Error
	void (*exit)(void*);
	void (*set_wait_focus)(void*, bool);
	bool (*dev_hud_active)(void*);
	void (*set_dev_hud)(void*, bool);
	bool (*screen_stats)(void*, float*, float*, float*);
	bool (*force_window_size)(void*, int, int);
	int (*shader_fail_count)(void*);
	void (*shader_report_json)(void*, bool, OhplH22Sink, void*);
	bool (*frame_stats)(void*, OhplH22Sink, void*, OhplH22Sink, void*);
	void (*xdg_cache_home)(void*, OhplH22Sink, void*);
	void (*ensure_folder)(void*, const uint8_t*, size_t);
	int (*screenshot)(void*, const uint8_t*, size_t); // 0 saved, 1 copy failed, 2 save failed
	void (*inject_text)(void*, const uint8_t*, size_t);
	void (*run_message)(void*, int);
	void (*call_handler)(void*, size_t, const uint8_t*, size_t, OhplH22Sink, void*); // blob: u64 len + bytes, key then value per field
};

struct OhplH22HudHost
{
	void* ctx;
	uint64_t (*app_time_ms)(void*);
	bool (*init)(void*);
	void (*capture_bug_shot)(void*);
	void (*screen_size)(void*, int*, int*);
	void (*set_virtual_size)(void*, float, float);
	float (*fps)(void*);
	float (*avg_frame_ms)(void*);
	size_t (*system_line_count)(void*);
	void (*system_line)(void*, size_t, OhplH22Sink, void*);
	float (*font_length)(void*, float, const uint32_t*, size_t);
	void (*draw_gfx)(void*, float, float, float, float, float);
	void (*draw_font)(void*, const uint32_t*, size_t, float, float, float, float, float, float, float, float);
	void (*set_frame_buffer_null)(void*);
	void (*render_set)(void*);
};

typedef double (*OhplH22Call)(void*, int, double, double, double, double);

// Ops of OhplH22Call, mirrored in h22_engine_ffi.rs `op`. Gaps in the numbering are ops the Rust loop does itself.
enum
{
	OHPL_H22_OP_LOG = 0,
	OHPL_H22_OP_LOG_MEDIUM = 1,
	OHPL_H22_OP_WINDOW_INPUT_FOCUS = 11,
	OHPL_H22_OP_INPUT_UPDATE = 13,
	OHPL_H22_OP_HC_LOGIC_STEP = 16,
	OHPL_H22_OP_HC_UPDATE = 17,
	OHPL_H22_OP_QUIT_POSTED = 18,
	OHPL_H22_OP_RESET_QUIT = 19,
	OHPL_H22_OP_WINDOW_FOCUS_BITS = 20,
	OHPL_H22_OP_TAKE_PLUGGED = 21,
	OHPL_H22_OP_TAKE_REMOVED = 22,
	OHPL_H22_OP_UPDATE_LOG_ACTIVE = 23,
	OHPL_H22_OP_CLEAR_UPDATE_LOG = 24,
	OHPL_H22_OP_SWAP = 27,
	OHPL_H22_OP_TIMING = 28,
	OHPL_H22_OP_FT_START = 29,
	OHPL_H22_OP_FT_STOP = 30,
	OHPL_H22_OP_FT_SEC = 31,
	OHPL_H22_OP_SCENE_RENDER = 32,
	OHPL_H22_OP_DEVHUD_RENDER = 33,
	OHPL_H22_OP_FLUSH = 34,
	OHPL_H22_OP_DIAG_END_FRAME = 35,
	OHPL_H22_OP_DIAG_ADD_TIMING = 36,
	OHPL_H22_OP_STEP = 40,
};

// Engine members the Rust loop reads and drives itself.
struct OhplH22Refs
{
	OhplEnUpdater* updater;
	OhplEnVisitFn visit;
	OhplLogicTimer* timer;
	OhplEnFps* fps;
	const bool* gameDone;
	const bool* paused;
	const bool* renderOnce;
	const bool* limitFps;
	const bool* waitFocus;
	void* const* headless;
};

extern "C" {
	OhplH22Hc* ohpl_h22_hc_new(const OhplH22HcHost* host, const uint8_t* path, size_t n);
	void ohpl_h22_hc_free(OhplH22Hc* s);
	size_t ohpl_h22_hc_register(const OhplH22Hc* s, const uint8_t* name, size_t n);
	bool ohpl_h22_hc_listening(const OhplH22Hc* s);
	void ohpl_h22_hc_push_log(const OhplH22Hc* s, const uint8_t* line, size_t n);
	void ohpl_h22_hc_update(const OhplH22Hc* s, const OhplH22HcHost* host, bool loading);
	void ohpl_h22_hc_logic_step(const OhplH22Hc* s, const OhplH22HcHost* host);
	void ohpl_h22_hc_acquire_lock(void (*log)(void*, int, const uint8_t*, size_t), void* ctx);

	bool ohpl_h22_hud_render(const OhplH22HudHost* host, bool hud, bool bugOpen, bool bugCapture, uint64_t savedTime,
							 const uint32_t* text, size_t textN, const uint8_t* last, size_t lastN);

	void ohpl_h22_engine_run(OhplH22Call call, void* ctx, const OhplH22Refs* refs, OhplEnLoop* loop, float* frameTime, uint32_t* focusBits);
	void ohpl_h22_engine_init(OhplH22Call call, void* ctx);
	void ohpl_h22_engine_exit(OhplH22Call call, void* ctx);
}

#endif
