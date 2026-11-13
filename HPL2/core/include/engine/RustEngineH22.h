#ifndef HPL_RUST_ENGINE_H22_H
#define HPL_RUST_ENGINE_H22_H

// C ABI of rust/src/engine/h22_hc_ffi.rs and h22_engine_ffi.rs.

#include <cstddef>
#include <cstdint>

struct OhplH22Hc;
struct OhplEnLoop;

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

// Ops of OhplH22Call, mirrored in h22_engine_ffi.rs `op`.
enum
{
	OHPL_H22_OP_LOG = 0, OHPL_H22_OP_LOG_MEDIUM, OHPL_H22_OP_BROADCAST, OHPL_H22_OP_RUN_MESSAGE,
	OHPL_H22_OP_CONTAINER_IS_DEFAULT, OHPL_H22_OP_LT_RESET, OHPL_H22_OP_WANT_UPDATE, OHPL_H22_OP_END_UPDATE_LOOP,
	OHPL_H22_OP_STEP_SIZE, OHPL_H22_OP_GAME_IS_DONE, OHPL_H22_OP_WAIT_OUT_OF_FOCUS, OHPL_H22_OP_WINDOW_INPUT_FOCUS,
	OHPL_H22_OP_SLEEP, OHPL_H22_OP_INPUT_UPDATE, OHPL_H22_OP_PAUSED, OHPL_H22_OP_HAS_HEADLESS,
	OHPL_H22_OP_HC_LOGIC_STEP, OHPL_H22_OP_HC_UPDATE, OHPL_H22_OP_QUIT_POSTED, OHPL_H22_OP_RESET_QUIT,
	OHPL_H22_OP_WINDOW_FOCUS_BITS, OHPL_H22_OP_TAKE_PLUGGED, OHPL_H22_OP_TAKE_REMOVED, OHPL_H22_OP_UPDATE_LOG_ACTIVE,
	OHPL_H22_OP_CLEAR_UPDATE_LOG, OHPL_H22_OP_RENDER_ONCE, OHPL_H22_OP_LIMIT_FPS, OHPL_H22_OP_SWAP,
	OHPL_H22_OP_TIMING, OHPL_H22_OP_FT_START, OHPL_H22_OP_FT_STOP, OHPL_H22_OP_FT_SEC,
	OHPL_H22_OP_SCENE_RENDER, OHPL_H22_OP_DEVHUD_RENDER, OHPL_H22_OP_FLUSH, OHPL_H22_OP_DIAG_END_FRAME,
	OHPL_H22_OP_DIAG_ADD_TIMING, OHPL_H22_OP_FPS_ADD, OHPL_H22_OP_APP_TIME, OHPL_H22_OP_STEADY_NS,
	OHPL_H22_OP_STEP
};

extern "C" {
	OhplH22Hc* ohpl_h22_hc_new(const OhplH22HcHost* host, const uint8_t* path, size_t n);
	void ohpl_h22_hc_free(OhplH22Hc* s);
	bool ohpl_h22_hc_listening(const OhplH22Hc* s);
	void ohpl_h22_hc_push_log(const OhplH22Hc* s, const uint8_t* line, size_t n);
	void ohpl_h22_hc_update(const OhplH22Hc* s, const OhplH22HcHost* host, bool loading);
	void ohpl_h22_hc_logic_step(const OhplH22Hc* s, const OhplH22HcHost* host);
	void ohpl_h22_hc_acquire_lock(void (*log)(void*, int, const uint8_t*, size_t), void* ctx);

	bool ohpl_h22_hud_render(const OhplH22HudHost* host, bool hud, bool bugOpen, bool bugCapture, uint64_t savedTime,
							 const uint32_t* text, size_t textN, const uint8_t* last, size_t lastN);

	void ohpl_h22_engine_run(OhplH22Call call, void* ctx, OhplEnLoop* loop, float* frameTime, uint32_t* focusBits);
	void ohpl_h22_engine_init(OhplH22Call call, void* ctx);
	void ohpl_h22_engine_exit(OhplH22Call call, void* ctx);
}

#endif
