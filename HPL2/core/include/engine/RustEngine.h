#ifndef HPL_RUST_ENGINE_H
#define HPL_RUST_ENGINE_H

// C ABI of rust/src/engine/ffi.rs and ffi_devhud.rs.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct OhplEnUpdater;
struct OhplEnSetupVars;
struct OhplEnRegistry;

struct OhplEnFps
{
	float fps;
	float updateRate;
	int frameCounter;
	float frametimeStart;
	float frametime;
};

struct OhplEnLoop
{
	double gameTime;
	double numOfTimes;
	double logicMs;
	double swapMs;
	int steps;
	int clearCheck;
	bool isUpdated;
	bool bufferSwap;
	bool swappedOnce;
};

struct OhplEnFrameTiming
{
	double logicMs;
	double swapMs;
	int steps;
};

struct OhplEnMsEntry
{
	float val;
	float dest;
	float speed;
	unsigned int types;
	bool destroyAtDest;
};

struct OhplEnCursor
{
	int key;
	uint64_t seq;
	bool started;
};

struct OhplEnBytes
{
	const uint8_t* ptr;
	size_t len;
};

struct OhplEnBytesPair
{
	OhplEnBytes key;
	OhplEnBytes val;
};

struct OhplEnHudLayout
{
	float font;
	float pad;
	float lineH;
	float bgW;
	float bgH;
	float textX;
	float textY;
};

struct OhplEnBugMetrics
{
	float font;
	float pad;
	float lineH;
	float boxW;
	float textW;
};

struct OhplEnBugBox
{
	float x;
	float y;
	float w;
	float h;
	float textX;
	float textY;
};

struct OhplEnReport
{
	OhplEnBytes id;
	OhplEnBytes time;
	OhplEnBytes description;
	OhplEnBytes version;
	OhplEnBytes exe;
	OhplEnBytes cwd;
	const OhplEnBytes* args;
	size_t argsN;
	const OhplEnBytesPair* env;
	size_t envN;
	const OhplEnBytes* system;
	size_t systemN;
	int resX;
	int resY;
	float fps;
	float frameMs;
	float gameTime;
	bool hasScene;
	OhplEnBytes map;
	float pos[3];
	float forward[3];
	OhplEnBytes worldJson;
	OhplEnBytes renderJson;
	OhplEnBytes files;
};

struct OhplEnSystemInfo
{
	OhplEnBytes version;
	OhplEnBytes device;
	OhplEnBytes machine;
	int cores;
	int memGb;
	OhplEnBytes os;
	OhplEnBytes kernel;
};

typedef void (*OhplEnSink)(void*, const uint8_t*, size_t);
typedef void (*OhplEnWSink)(void*, const uint32_t*, size_t);
// op: 0 message, 1 timed Update message, 2 leave container (name), 3 enter container (name)
typedef void (*OhplEnVisitFn)(void*, size_t, int, int, float, const uint8_t*, size_t);

extern "C" {
	OhplEnUpdater* ohpl_en_upd_new();
	void ohpl_en_upd_free(OhplEnUpdater* u);
	void ohpl_en_upd_add_container(OhplEnUpdater* u, const uint8_t* name, size_t n);
	int ohpl_en_upd_add_update(OhplEnUpdater* u, const uint8_t* name, size_t n, size_t upd);
	void ohpl_en_upd_add_global(OhplEnUpdater* u, size_t upd);
	void ohpl_en_upd_current_name(const OhplEnUpdater* u, OhplEnSink sink, void* ctx);
	void ohpl_en_upd_run_message(OhplEnUpdater* u, int msg, float x, OhplEnVisitFn cb, void* ctx);
	void ohpl_en_upd_broadcast(OhplEnUpdater* u, int msg, float x, OhplEnVisitFn cb, void* ctx);
	bool ohpl_en_upd_set_container(OhplEnUpdater* u, const uint8_t* name, size_t n, OhplEnVisitFn cb, void* ctx);

	void ohpl_en_fps_init(OhplEnFps* f, uint64_t now);
	void ohpl_en_fps_add_frame(OhplEnFps* f, uint64_t now);
	float ohpl_en_fps_avg_frame_ms(const OhplEnFps* f);

	void ohpl_en_loop_init(OhplEnLoop* l);
	void ohpl_en_loop_begin_run(OhplEnLoop* l);
	void ohpl_en_loop_step_begin(OhplEnLoop* l);
	void ohpl_en_loop_step_end(OhplEnLoop* l);
	void ohpl_en_loop_pause_tick(OhplEnLoop* l);
	void ohpl_en_loop_advance_game_time(OhplEnLoop* l, float step);
	bool ohpl_en_loop_update_log_tick(OhplEnLoop* l);
	void ohpl_en_loop_add_logic_ms(OhplEnLoop* l, double ms);
	void ohpl_en_loop_add_swap_ms(OhplEnLoop* l, double ms);
	bool ohpl_en_loop_skip_render_once(OhplEnLoop* l, bool renderOnce);
	bool ohpl_en_loop_take_swap(OhplEnLoop* l);
	bool ohpl_en_loop_swap_done(OhplEnLoop* l, bool renderOnce);
	bool ohpl_en_loop_want_render(OhplEnLoop* l, bool limitFps);
	double ohpl_en_loop_mean_framerate(OhplEnLoop* l, uint64_t elapsedMs);
	void ohpl_en_loop_frame_rendered(OhplEnLoop* l, OhplEnFrameTiming* out);
	float ohpl_en_frame_time(double timerSec);
	size_t ohpl_en_focus_messages(uint32_t had, uint32_t now, int* out);
	bool ohpl_en_dev_hud_active(bool requested, int envFirst);
	int ohpl_en_engine_type_from_string(const uint8_t* s, size_t n);
	bool ohpl_en_engine_type_name(int i, OhplEnSink sink, void* ctx);

	OhplEnSetupVars* ohpl_en_vars_new();
	void ohpl_en_vars_free(OhplEnSetupVars* v);
	void ohpl_en_vars_add_string(OhplEnSetupVars* v, const uint8_t* name, size_t n, const uint8_t* val, size_t valN);
	void ohpl_en_vars_add_int(OhplEnSetupVars* v, const uint8_t* name, size_t n, int x);
	void ohpl_en_vars_add_float(OhplEnSetupVars* v, const uint8_t* name, size_t n, float x);
	void ohpl_en_vars_add_bool(OhplEnSetupVars* v, const uint8_t* name, size_t n, bool x);
	void ohpl_en_vars_get_string(const OhplEnSetupVars* v, const uint8_t* name, size_t n, OhplEnSink sink, void* ctx);
	float ohpl_en_vars_get_float(const OhplEnSetupVars* v, const uint8_t* name, size_t n, float def);
	int ohpl_en_vars_get_int(const OhplEnSetupVars* v, const uint8_t* name, size_t n, int def);
	bool ohpl_en_vars_get_bool(const OhplEnSetupVars* v, const uint8_t* name, size_t n, bool def);

	void ohpl_en_ms_fade_to(OhplEnMsEntry* e, float dest, float speed, bool destroy);
	void ohpl_en_ms_set_val_and_dest(OhplEnMsEntry* e, float val);
	bool ohpl_en_ms_update(OhplEnMsEntry* e, float dt);
	float ohpl_en_ms_calc_results(const OhplEnMsEntry* const* slots, size_t n, unsigned int types, int calc, float def);
	int ohpl_en_ms_slot_action(int idx, size_t len, bool occupied, bool force);
	size_t ohpl_en_ms_create_slot(const OhplEnMsEntry* const* slots, size_t n);
	size_t ohpl_en_ms_entry_layout(size_t* out);

	OhplEnRegistry* ohpl_en_reg_new();
	void ohpl_en_reg_free(OhplEnRegistry* r);
	void ohpl_en_reg_add(OhplEnRegistry* r, int key, size_t obj);
	size_t ohpl_en_reg_get(const OhplEnRegistry* r, int key);
	size_t ohpl_en_reg_size(const OhplEnRegistry* r);
	void ohpl_en_reg_clear(OhplEnRegistry* r);
	bool ohpl_en_reg_has_next(const OhplEnRegistry* r, const OhplEnCursor* c);
	size_t ohpl_en_reg_peek(const OhplEnRegistry* r, const OhplEnCursor* c);
	size_t ohpl_en_reg_next(const OhplEnRegistry* r, OhplEnCursor* c);
	int ohpl_en_save_next_id(int* counter);
	int ohpl_en_reg_set_up_all(OhplEnRegistry* r, int (*visit)(void*, size_t), void* ctx);

	float ohpl_en_hud_font_size(int screenY);
	OhplEnHudLayout ohpl_en_hud_layout(float font, float maxWidth, size_t lines);
	void ohpl_en_hud_stats_line(int w, int h, float fps, float frameMs, OhplEnSink sink, void* ctx);
	OhplEnBugMetrics ohpl_en_bug_metrics(int sx, int sy);
	float ohpl_en_bug_toast_box_w(OhplEnBugMetrics m, float pathWidth);
	OhplEnBugBox ohpl_en_bug_box(OhplEnBugMetrics m, float boxW, size_t lines, int sx, int sy);
	bool ohpl_en_toast_active(uint64_t savedMs, uint64_t nowMs);
	bool ohpl_en_should_render(bool hud, bool bugOpen, bool toast);
	uint32_t ohpl_en_cursor_char(uint64_t nowMs);
	void ohpl_en_wrap_bug_text(const uint32_t* text, size_t n, float textW, float (*measure)(void*, const uint32_t*, size_t), OhplEnWSink line, void* ctx);
	void ohpl_en_ctrl_backspace(const uint32_t* text, size_t n, OhplEnWSink sink, void* ctx);
	int ohpl_en_key_action(int key, bool ctrl, bool shift, bool textEmpty);
	bool ohpl_en_swallowed_event(uint32_t type);
	bool ohpl_en_opens_report(uint32_t type, int key, bool repeat);
	void ohpl_en_first_line(const uint8_t* file, size_t n, OhplEnSink sink, void* ctx);
	void ohpl_en_os_name(const uint8_t* file, size_t n, OhplEnSink sink, void* ctx);
	int64_t ohpl_en_mem_gb(int64_t pages, int64_t pageSize);
	void ohpl_en_system_lines(const OhplEnSystemInfo* i, OhplEnSink sink, void* ctx);
	void ohpl_en_unique_name(const uint8_t* base, size_t n, bool (*exists)(void*, const uint8_t*, size_t), OhplEnSink sink, void* ctx);
	void ohpl_en_files_json(bool screenshot, bool log, OhplEnSink sink, void* ctx);
	bool ohpl_en_env_pair(const uint8_t* var, size_t n, OhplEnSink sink, void* ctx);
	void ohpl_en_cmdline_args(const uint8_t* raw, size_t n, OhplEnSink sink, void* ctx);
	void ohpl_en_report_json(const OhplEnReport* r, OhplEnSink sink, void* ctx);
	void ohpl_en_index_line(OhplEnBytes id, OhplEnBytes map, OhplEnBytes description, OhplEnBytes dir, OhplEnSink sink, void* ctx);
}

namespace hpl {

	inline OhplEnBytes OhplEnB(const std::string& s)
	{
		return OhplEnBytes{ reinterpret_cast<const uint8_t*>(s.data()), s.size() };
	}

	inline const uint8_t* OhplEnP(const std::string& s)
	{
		return reinterpret_cast<const uint8_t*>(s.data());
	}

	inline void OhplEnAssign(void* apCtx, const uint8_t* apData, size_t alLen)
	{
		static_cast<std::string*>(apCtx)->assign(reinterpret_cast<const char*>(apData), alLen);
	}

	inline void OhplEnAppend(void* apCtx, const uint8_t* apData, size_t alLen)
	{
		static_cast<std::string*>(apCtx)->append(reinterpret_cast<const char*>(apData), alLen);
	}

	inline void OhplEnPush(void* apCtx, const uint8_t* apData, size_t alLen)
	{
		static_cast<std::vector<std::string>*>(apCtx)->emplace_back(reinterpret_cast<const char*>(apData), alLen);
	}

	inline void OhplEnWAssign(void* apCtx, const uint32_t* apData, size_t alLen)
	{
		static_cast<std::wstring*>(apCtx)->assign(reinterpret_cast<const wchar_t*>(apData), alLen);
	}

	inline void OhplEnWPush(void* apCtx, const uint32_t* apData, size_t alLen)
	{
		static_cast<std::vector<std::wstring>*>(apCtx)->emplace_back(reinterpret_cast<const wchar_t*>(apData), alLen);
	}

}

#endif // HPL_RUST_ENGINE_H
