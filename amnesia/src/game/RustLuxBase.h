#ifndef LUX_RUST_BASE_H
#define LUX_RUST_BASE_H

// C ABI of rust/src/luxbase/ffi.rs. Rust owns the order of every init / config / input / headless
// decision; the engine and game calls it needs come back through one callback
// call(ctx, op, args, nargs, ret). Op numbers are the Rust `op` module. Strings come back
// through ohpl_ga9_ret_bytes / ohpl_ga9_ret_wide or an emit callback (ctx, tag, ptr, len).

#include <cstddef>
#include <cstdint>
#include <string>

struct OhplGa9Arg
{
	int64_t i;
	float f;
	const unsigned char *p;
	size_t n;
};

struct OhplGa9Ret
{
	int64_t i;
	float f;
	double d;
	void *bytes;
	void *wide;
};

typedef void (*OhplGa9Call)(void *ctx, uint32_t op, const OhplGa9Arg *args, size_t nargs, OhplGa9Ret *ret);
typedef void (*OhplGa9Emit)(void *ctx, uint32_t tag, const void *p, size_t n);

struct OhplGa9Str
{
	const unsigned char *p;
	size_t n;
};

enum eOhplGa9Op
{
	eOhplGa9Op_CfgBool = 0,
	eOhplGa9Op_CfgInt = 1,
	eOhplGa9Op_CfgFloat = 2,
	eOhplGa9Op_CfgStr = 3,
	eOhplGa9Op_CfgStrW = 4,
	eOhplGa9Op_CfgSetBool = 5,
	eOhplGa9Op_CfgSetInt = 6,
	eOhplGa9Op_CfgSetFloat = 7,
	eOhplGa9Op_CfgSetStr = 8,
	eOhplGa9Op_Cfg2 = 16,
	eOhplGa9Op_LoadInit = 40,
	eOhplGa9Op_LoadDefaults = 41,
	eOhplGa9Op_Warn = 42,
	eOhplGa9Op_RaiseFirstStart = 44,
	eOhplGa9Op_Gamma = 45,
	eOhplGa9Op_GBufferType = 46,
	eOhplGa9Op_NumGBuffer = 47,
	eOhplGa9Op_Volume = 48,
	eOhplGa9Op_LimitFps = 49,
	eOhplGa9Op_WaitFocus = 50,
	eOhplGa9Op_FirstStartFlag = 51,
	eOhplGa9Op_FileExistsW = 52,
	eOhplGa9Op_MapFolder = 53,
	eOhplGa9Op_LangClear = 54,
	eOhplGa9Op_LangFound = 55,
	eOhplGa9Op_LangAdd = 56,
	eOhplGa9Op_ActionCreate = 60,
	eOhplGa9Op_ActionClear = 61,
	eOhplGa9Op_ActionAdd = 62,
	eOhplGa9Op_ActionCount = 63,
	eOhplGa9Op_ActionType = 64,
	eOhplGa9Op_ActionInput = 65,
	eOhplGa9Op_MouseAnyButton = 70,
	eOhplGa9Op_MouseAbs = 71,
	eOhplGa9Op_MouseTime = 72,
	eOhplGa9Op_MousePad = 73,
	eOhplGa9Op_MouseUiDir = 74,
	eOhplGa9Op_MouseScreen = 75,
	eOhplGa9Op_MouseHide = 76,
	eOhplGa9Op_LookTriggered = 80,
	eOhplGa9Op_LookAddLean = 81,
	eOhplGa9Op_LookSetLean = 82,
	eOhplGa9Op_LookScroll = 83,
	eOhplGa9Op_LookStop = 84,
	eOhplGa9Op_LookButton = 85,
	eOhplGa9Op_LookAxis = 86,
	eOhplGa9Op_LookDead = 87,
	eOhplGa9Op_LookFrame = 88,
	eOhplGa9Op_LookStep = 89,
	eOhplGa9Op_ReqHas = 100,
	eOhplGa9Op_ReqStr = 101,
	eOhplGa9Op_ReqInt = 102,
	eOhplGa9Op_ReqFloat = 103,
	eOhplGa9Op_ReqBool = 104,
	eOhplGa9Op_RespError = 110,
	eOhplGa9Op_RespStr = 111,
	eOhplGa9Op_RespInt = 112,
	eOhplGa9Op_RespFloat = 113,
	eOhplGa9Op_RespBool = 114,
	eOhplGa9Op_RespRaw = 115,
	eOhplGa9Op_MapLoaded = 120,
	eOhplGa9Op_RunScript = 121,
	eOhplGa9Op_SetGBuffer = 122,
	eOhplGa9Op_Deferred = 123,
	eOhplGa9Op_GBufferSize = 124,
	eOhplGa9Op_GBufferPixels = 126,
	eOhplGa9Op_MapName = 127,
	eOhplGa9Op_MapFile = 128,
	eOhplGa9Op_Body = 129,
	eOhplGa9Op_BodyValue = 130,
	eOhplGa9Op_Fps = 131,
	eOhplGa9Op_Occlusion = 132,
	eOhplGa9Op_SetOcclusion = 133,
	eOhplGa9Op_GpuTiming = 134,
	eOhplGa9Op_RenderJson = 135,
	eOhplGa9Op_SaveGame = 136,
	eOhplGa9Op_FileExists = 137,
	eOhplGa9Op_LoadGame = 138,
	eOhplGa9Op_Player = 139,
	eOhplGa9Op_Teleport = 140,
	eOhplGa9Op_HaveUserCfg = 141,
	eOhplGa9Op_DefaultProfile = 142,
	eOhplGa9Op_InitUserCfg = 143,
	eOhplGa9Op_EnterGame = 144,
	eOhplGa9Op_StartGame = 145,
	eOhplGa9Op_Camera = 146,
	eOhplGa9Op_CameraValue = 147,
	eOhplGa9Op_SetCameraPos = 148,
	eOhplGa9Op_SetPitch = 149,
	eOhplGa9Op_SetYaw = 150,
};

enum eOhplGa9Cmd
{
	eOhplGa9Cmd_RunScript = 0,
	eOhplGa9Cmd_SetDebugGBuffer = 1,
	eOhplGa9Cmd_ReadGBufferStats = 2,
	eOhplGa9Cmd_State = 3,
	eOhplGa9Cmd_RenderStats = 4,
	eOhplGa9Cmd_SaveGame = 5,
	eOhplGa9Cmd_LoadGame = 6,
	eOhplGa9Cmd_Teleport = 7,
	eOhplGa9Cmd_StartMap = 8,
	eOhplGa9Cmd_CameraState = 9,
	eOhplGa9Cmd_SetCamera = 10,
};

// Emit tags.
enum
{
	eOhplGa9Tag_MainLang = 1, eOhplGa9Tag_MainExt = 2, eOhplGa9Tag_MainRenderer = 3, eOhplGa9Tag_MainColor = 4,
	eOhplGa9Tag_InitFile = 1, eOhplGa9Tag_DefaultInit = 2,
	eOhplGa9Tag_Error = 1,
	eOhplGa9Tag_AppSaveFolder = 10, eOhplGa9Tag_AppUserCfg = 11, eOhplGa9Tag_AppUserKeys = 12, eOhplGa9Tag_AppMainCfg = 13,
	eOhplGa9Tag_AppGameCfg = 14, eOhplGa9Tag_AppMenuCfg = 15, eOhplGa9Tag_AppPreMenuCfg = 16, eOhplGa9Tag_AppDemoCfg = 17,
	eOhplGa9Tag_AppResources = 20, eOhplGa9Tag_AppMaterials = 21, eOhplGa9Tag_AppBaseLang = 22, eOhplGa9Tag_AppGameLang = 23,
	eOhplGa9Tag_AppBaseLangFolder = 24, eOhplGa9Tag_AppGameLangFolder = 25, eOhplGa9Tag_AppCustomStoryPath = 26,
	eOhplGa9Tag_AppGameName = 27, eOhplGa9Tag_AppAllowHardmode = 28, eOhplGa9Tag_AppStartFile = 29,
	eOhplGa9Tag_AppStartFolder = 30, eOhplGa9Tag_AppStartPos = 31,
	eOhplGa9Tag_PathMigrate = 1, eOhplGa9Tag_PathCreateIfMissing = 2, eOhplGa9Tag_PathMkdir = 3,
	eOhplGa9Tag_PathMoveFrom = 4, eOhplGa9Tag_PathMoveTo = 5, eOhplGa9Tag_PathBaseSave = 6,
	eOhplGa9Tag_PathBaseConfig = 7, eOhplGa9Tag_PathBaseState = 8, eOhplGa9Tag_PathCrashFlag = 9,
	eOhplGa9Tag_PathFirstStartFlag = 10, eOhplGa9Tag_PathLogFile = 11, eOhplGa9Tag_PathUpdateLogFile = 12,
	eOhplGa9Tag_PathGameRelative = 13,
	eOhplGa9Tag_VarsDefaultProfile = 1,
	eOhplGa9Tag_StoryMapsFolder = 1, eOhplGa9Tag_StoryStartMap = 2, eOhplGa9Tag_StoryStartPos = 3, eOhplGa9Tag_StoryName = 4,
	eOhplGa9Tag_StoryAuthor = 5, eOhplGa9Tag_StoryImgFile = 6, eOhplGa9Tag_StoryExtraPrefix = 7,
	eOhplGa9Tag_StoryDefaultExtra = 8, eOhplGa9Tag_StoryError = 9,
	eOhplGa9Tag_StartFile = 1, eOhplGa9Tag_StartFolder = 2, eOhplGa9Tag_StartPos = 3,
	eOhplGa9Tag_ProfileSavePrevious = 20, eOhplGa9Tag_ProfileMainSave = 21, eOhplGa9Tag_ProfileMainConfig = 22,
	eOhplGa9Tag_ProfileSave = 23,
	eOhplGa9Tag_AmfpResources = 1, eOhplGa9Tag_AmfpMaterials = 2, eOhplGa9Tag_AmfpGameName = 3, eOhplGa9Tag_AmfpError = 4,
};

struct OhplGa9MainCfg
{
	bool load_debug_menu, first_start, force_cache, fullscreen, vsync, adaptive_vsync;
	bool fast_physics, fast_static, fast_entity, occlusion_test_lights, shadows_active;
	bool world_reflection, refraction, edge_smooth, ssao_active, parallax_enabled, force_sm34_off;
	int32_t screen_width, screen_height, display, shadow_quality, shadow_res, gui_scale;
	int32_t ssao_samples, ssao_resolution, parallax_quality, texture_quality, texture_filter;
	int32_t sound_dev_id, max_sound_channels, sound_stream_buffers, sound_stream_buffer_size;
	float paper_white, peak_nits, texture_anisotropy;
	OhplGa9Str lang_file, screenshot_ext, renderer, color_output;
};

struct OhplGa9Restart
{
	bool needed, dialog_shown;
};

struct OhplGa9StartArgs
{
	OhplGa9Str file, folder, pos;
	bool has_story;
	OhplGa9Str story_map, story_folder, story_pos;
	OhplGa9Str def_file, def_folder, def_pos;
	bool ptest;
};

struct OhplGa9LangArgs
{
	OhplGa9Str name;
	bool force;
	OhplGa9Str current;
	bool has_story;
	OhplGa9Str story_root, story_prefix, story_default;
	OhplGa9Str game_folder, base_folder, default_game, default_base;
};

struct OhplGa9Action
{
	int32_t id;
	bool configurable;
	int32_t cat;
};

struct OhplGa9Settings
{
	bool invert_mouse, smooth_mouse, gamepad_look_invert;
	float mouse_sensitivity, gamepad_walk_sensitivity, gamepad_look_sensitivity;
};

struct OhplGa9MouseAct
{
	int32_t last_x, last_y;
	double active_at;
};

struct OhplGa9LookIn
{
	int32_t mouse_rel_x, mouse_rel_y;
	float mouse_sensitivity, screen_height;
	bool smooth_mouse, invert_mouse, pad_present;
	float look_sensitivity;
	bool look_invert;
};

// First member of a Rust debug handle: the flags the C++ reads and writes directly.
struct OhplGa9DebugFlags
{
	bool show_fps, show_sound_playing, show_player_info, show_entity_info, show_debug_messages, show_error_messages;
	bool script_debug_on, inspection_mode, disable_flashbacks, draw_physics, reload_from_current_position;
	bool allow_quick_save, window_active, fast_forward, first_update_on_map;
	int32_t temp_count;
};

struct OhplGa9Toggle
{
	int32_t kind;
	bool on;
	float value;
};

struct OhplGa9Smooth;
struct OhplGa9Debug;

extern "C"
{
	void ohpl_ga9_ret_bytes(OhplGa9Ret *r, const unsigned char *p, size_t n);
	void ohpl_ga9_ret_wide(OhplGa9Ret *r, const uint32_t *p, size_t n);

	void ohpl_ga9_load_main_cfg(void *ctx, OhplGa9Call call, const unsigned char *def_lang, size_t n, OhplGa9MainCfg *out, OhplGa9Emit emit, void *ectx);
	void ohpl_ga9_save_main_cfg(void *ctx, OhplGa9Call call, const OhplGa9MainCfg *m);
	void ohpl_ga9_restart_set(OhplGa9Restart *r);
	bool ohpl_ga9_restart_due(OhplGa9Restart *r);

	void ohpl_ga9_parse_cmdline(const unsigned char *cmd, size_t n, bool *out, OhplGa9Emit emit, void *ectx);
	void ohpl_ga9_error_text(const uint32_t *msg, size_t n, OhplGa9Emit emit, void *ectx);
	bool ohpl_ga9_init_app(void *ctx, OhplGa9Call call, const uint32_t *init_file, size_t init_n, const uint32_t *default_init, size_t default_n, OhplGa9Emit emit, void *ectx);
	void ohpl_ga9_plan_paths(OhplGa9Str personal, OhplGa9Str config_home, OhplGa9Str state_home, OhplGa9Str save_folder, OhplGa9Str custom_story, bool headless, int32_t pid, OhplGa9Emit emit, void *ectx);
	bool ohpl_ga9_use_wanted_config(bool force_default, bool wanted_exists);
	void ohpl_ga9_main_vars(void *ctx, OhplGa9Call call, bool *out, OhplGa9Emit emit, void *ectx);
	bool ohpl_ga9_feature_error(int32_t sm2, int32_t draw_buffers, int32_t packed, OhplGa9Emit emit, void *ectx);
	int32_t ohpl_ga9_start_mode(bool show_pre_menu, bool show_menu);
	int32_t ohpl_ga9_direct_start_input(bool debug_window_active, bool fullscreen);
	bool ohpl_ga9_custom_story(void *ctx, OhplGa9Call call, const uint32_t *root, size_t root_n, const unsigned char *default_lang, size_t lang_n, OhplGa9Emit emit, void *ectx);
	void ohpl_ga9_custom_story_save_path(const uint32_t *profile, size_t profile_n, const uint32_t *root, size_t root_n, OhplGa9Emit emit, void *ectx);
	void ohpl_ga9_resolve_start(void *ctx, OhplGa9Call call, const OhplGa9StartArgs *a, OhplGa9Emit emit, void *ectx);
	bool ohpl_ga9_load_language(void *ctx, OhplGa9Call call, const OhplGa9LangArgs *a, OhplGa9Emit emit, void *ectx);
	void ohpl_ga9_set_profile(OhplGa9Str prev, bool have_user_config, OhplGa9Str name, OhplGa9Str base_save, OhplGa9Str base_config, OhplGa9Emit emit, void *ectx);
	bool ohpl_ga9_profile_folder(OhplGa9Str base_save, OhplGa9Str name, bool exists, OhplGa9Emit emit, void *ectx);
	void ohpl_ga9_calc_gui_set_offset(float vx, float vy, float sx, float sy, float *out);
	bool ohpl_ga9_chinese_font(const unsigned char *cur, size_t n);
	bool ohpl_ga9_achievement(size_t index, int32_t *id, OhplGa9Emit emit, void *ectx);
	int32_t ohpl_ga9_axis_from_name(const unsigned char *s, size_t n, bool *ok);

	size_t ohpl_ga9_action_count();
	bool ohpl_ga9_action(size_t index, OhplGa9Action *out, OhplGa9Emit emit, void *ectx);
	size_t ohpl_ga9_input_count();
	bool ohpl_ga9_input(size_t index, int32_t *value, int32_t *action, OhplGa9Emit emit, void *ectx);
	int32_t ohpl_ga9_action_by_name(const unsigned char *s, size_t n);
	int32_t ohpl_ga9_action_by_id(int32_t id);
	size_t ohpl_ga9_actions_by_category(int32_t cat, int32_t *out, size_t cap);
	size_t ohpl_ga9_default_inputs(int32_t id, int32_t *out, size_t cap);
	void ohpl_ga9_create_actions(void *ctx, OhplGa9Call call);
	void ohpl_ga9_load_user_config(void *ctx, OhplGa9Call call, OhplGa9Settings *out);
	void ohpl_ga9_save_user_config(void *ctx, OhplGa9Call call, const OhplGa9Settings *s);
	OhplGa9Smooth *ohpl_ga9_smooth_new(int32_t max, float prev_mul);
	void ohpl_ga9_smooth_free(OhplGa9Smooth *s);
	void ohpl_ga9_smooth_reset(OhplGa9Smooth *s);
	void ohpl_ga9_smooth_push(OhplGa9Smooth *s, float x, float y, float *out);
	bool ohpl_ga9_mouse_step(OhplGa9MouseAct *m, void *ctx, OhplGa9Call call);
	void ohpl_ga9_look(OhplGa9Smooth *s, const OhplGa9LookIn *i, void *ctx, OhplGa9Call call, float *out);
	bool ohpl_ga9_state_sends_to_gui(int32_t state, bool inventory_message_active);
	void ohpl_ga9_screenshot_name(const uint32_t *dir, size_t dir_n, const unsigned char *ext, size_t ext_n, void *ctx, OhplGa9Call call, OhplGa9Emit emit, void *ectx);
	bool ohpl_ga9_ui_map(int32_t kind, size_t index, int32_t *action, int32_t *id);

	OhplGa9Debug *ohpl_ga9_debug_new();
	void ohpl_ga9_debug_free(OhplGa9Debug *d);
	OhplGa9DebugFlags *ohpl_ga9_debug_flags(OhplGa9Debug *d);
	void ohpl_ga9_debug_load_user_config(OhplGa9Debug *d, bool ptest, void *ctx, OhplGa9Call call);
	void ohpl_ga9_debug_save_user_config(OhplGa9Debug *d, void *ctx, OhplGa9Call call);
	void ohpl_ga9_debug_reset(OhplGa9Debug *d);
	void ohpl_ga9_debug_on_map_enter(OhplGa9Debug *d);
	void ohpl_ga9_debug_add_error(OhplGa9Debug *d, const uint32_t *p, size_t n);
	void ohpl_ga9_debug_add_message(OhplGa9Debug *d, const uint32_t *p, size_t n, bool check_duplicates);
	void ohpl_ga9_debug_update_messages(OhplGa9Debug *d, float dt);
	size_t ohpl_ga9_debug_message_count(const OhplGa9Debug *d);
	float ohpl_ga9_debug_message(const OhplGa9Debug *d, size_t index, OhplGa9Emit emit, void *ectx);
	float ohpl_ga9_debug_set_fast_forward(OhplGa9Debug *d, bool on);
	void ohpl_ga9_debug_update_tick(OhplGa9Debug *d, bool *first, bool *due);
	void ohpl_ga9_debug_batch_pop(OhplGa9Debug *d, OhplGa9Emit emit, void *ectx);
	void ohpl_ga9_debug_load_batch(OhplGa9Debug *d, const unsigned char *p, size_t n);
	bool ohpl_ga9_debug_window_change(bool load_debug_menu, bool active, bool fullscreen, bool *lock);
	void ohpl_ga9_debug_change_text(OhplGa9Debug *d, int32_t num, int32_t val, OhplGa9Toggle *out);
	void ohpl_ga9_inspection_line(const float *start, const float *fwd, float *out);

	void ohpl_ga9_headless(uint32_t cmd, void *ctx, OhplGa9Call call);
	void ohpl_ga9_amfp_init_config_file(const unsigned char *cmd, size_t n, OhplGa9Emit emit, void *ectx);
	bool ohpl_ga9_amfp_load_vars(void *ctx, OhplGa9Call call, bool loaded, const uint32_t *file, size_t n, OhplGa9Emit emit, void *ectx);
	void ohpl_ga9_amfp_text(int32_t kind, const unsigned char *name, size_t n, OhplGa9Emit emit, void *ectx);
}

// Adapters: lambdas as C callbacks, and argument / result helpers.
template <class F> struct OhplGa9CallOf
{
	static void Thunk(void *c, uint32_t op, const OhplGa9Arg *a, size_t na, OhplGa9Ret *r) { (*static_cast<F *>(c))(op, a, na, r); }
	static OhplGa9Call Fn() { return &Thunk; }
};

template <class F> struct OhplGa9EmitOf
{
	static void Thunk(void *c, uint32_t tag, const void *p, size_t n) { (*static_cast<F *>(c))(tag, p, n); }
	static OhplGa9Emit Fn() { return &Thunk; }
};

inline OhplGa9Arg OhplGa9ArgI(long long v) { OhplGa9Arg a = {v, 0, nullptr, 0}; return a; }
inline OhplGa9Arg OhplGa9ArgF(float v) { OhplGa9Arg a = {0, v, nullptr, 0}; return a; }
inline OhplGa9Arg OhplGa9ArgS(const std::string &s) { OhplGa9Arg a = {0, 0, (const unsigned char *)s.data(), s.size()}; return a; }
inline OhplGa9Arg OhplGa9ArgW(const std::wstring &s) { OhplGa9Arg a = {0, 0, (const unsigned char *)s.data(), s.size()}; return a; }
inline std::string OhplGa9GetS(const OhplGa9Arg &a) { return std::string((const char *)a.p, a.n); }
inline std::wstring OhplGa9GetW(const OhplGa9Arg &a) { return std::wstring((const wchar_t *)a.p, a.n); }
inline OhplGa9Str OhplGa9StrOf(const std::string &s) { OhplGa9Str r = {(const unsigned char *)s.data(), s.size()}; return r; }
inline OhplGa9Str OhplGa9StrOfW(const std::wstring &s) { OhplGa9Str r = {(const unsigned char *)s.data(), s.size()}; return r; }
inline void OhplGa9RetS(OhplGa9Ret *r, const std::string &s) { ohpl_ga9_ret_bytes(r, (const unsigned char *)s.data(), s.size()); }
inline void OhplGa9RetW(OhplGa9Ret *r, const std::wstring &s) { ohpl_ga9_ret_wide(r, (const uint32_t *)s.data(), s.size()); }
inline std::string OhplGa9EmitS(const void *p, size_t n) { return std::string((const char *)p, n); }
inline std::wstring OhplGa9EmitW(const void *p, size_t n) { return std::wstring((const wchar_t *)p, n); }

// cConfigFile as a Rust config host: runs ops 0..8 (get/set bool, int, float, string, wide string).
template <class C> inline bool OhplGa9CfgCall(C *c, uint32_t op, const OhplGa9Arg *a, OhplGa9Ret *r)
{
	if(op > eOhplGa9Op_CfgSetStr) return false;
	std::string sLevel = OhplGa9GetS(a[0]), sKey = OhplGa9GetS(a[1]);
	switch(op)
	{
	case eOhplGa9Op_CfgBool: r->i = c->GetBool(sLevel, sKey, a[2].i != 0); break;
	case eOhplGa9Op_CfgInt: r->i = c->GetInt(sLevel, sKey, (int)a[2].i); break;
	case eOhplGa9Op_CfgFloat: r->f = c->GetFloat(sLevel, sKey, a[2].f); break;
	case eOhplGa9Op_CfgStr: OhplGa9RetS(r, c->GetString(sLevel, sKey, OhplGa9GetS(a[2]))); break;
	case eOhplGa9Op_CfgStrW: OhplGa9RetW(r, c->GetStringW(sLevel, sKey, OhplGa9GetW(a[2]))); break;
	case eOhplGa9Op_CfgSetBool: c->SetBool(sLevel, sKey, a[2].i != 0); break;
	case eOhplGa9Op_CfgSetInt: c->SetInt(sLevel, sKey, (int)a[2].i); break;
	case eOhplGa9Op_CfgSetFloat: c->SetFloat(sLevel, sKey, a[2].f); break;
	case eOhplGa9Op_CfgSetStr: c->SetString(sLevel, sKey, OhplGa9GetS(a[2])); break;
	}
	return true;
}

#endif // LUX_RUST_BASE_H
