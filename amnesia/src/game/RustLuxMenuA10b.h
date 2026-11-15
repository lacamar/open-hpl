#ifndef LUX_RUST_MENU_A10B_H
#define LUX_RUST_MENU_A10B_H

// C ABI of rust/src/luxmenu2/a10b_ffi.rs: the pre-menu, credits, demo end, load screen and
// progress log classes. Rust owns their state; every engine call goes through OhplA10bHost.

#include <cstddef>
#include <cstdint>

struct OhplA10bArg
{
	uint64_t v;
	size_t n;
};

typedef void (*OhplA10bSink)(void*, const uint32_t*, size_t);

struct OhplA10bCall
{
	int op;
	uint32_t na;
	const OhplA10bArg *a;
	uint64_t u[8];
	OhplA10bSink sink;
	void *sinkCtx;
};

struct OhplA10bHost
{
	void *ctx;
	void (*call)(void*, OhplA10bCall*);
};

enum eOhplA10bOp
{
	kOhplA10b_GUI_CREATE_SET = 1,
	kOhplA10b_GUI_CREATE_SKIN = 2,
	kOhplA10b_SET_VIRTUAL_SIZE = 3,
	kOhplA10b_SET_ACTIVE = 4,
	kOhplA10b_SET_DRAW_MOUSE = 5,
	kOhplA10b_SET_SKIN = 6,
	kOhplA10b_GUI_SET_FOCUS = 7,
	kOhplA10b_CALC_OFFSET = 8,
	kOhplA10b_VIEWPORT_NEW = 9,
	kOhplA10b_VIEWPORT_ACTIVE = 10,
	kOhplA10b_VIEWPORT_VISIBLE = 11,
	kOhplA10b_VIEWPORT_ADD_SET = 12,
	kOhplA10b_GFX_RECT = 13,
	kOhplA10b_GFX_TEX_FILE = 14,
	kOhplA10b_TEX_2D = 15,
	kOhplA10b_GFX_FROM_TEX = 16,
	kOhplA10b_GFX_DESTROY = 17,
	kOhplA10b_GFX_SIZE = 18,
	kOhplA10b_DRAW_GFX = 19,
	kOhplA10b_DRAW_GFX_POS = 20,
	kOhplA10b_DRAW_FONT = 21,
	kOhplA10b_FONT_LOAD = 22,
	kOhplA10b_FONT_WRAP = 23,
	kOhplA10b_FONT_LEN = 24,
	kOhplA10b_FONT_HEIGHT = 25,
	kOhplA10b_TRANSLATE = 26,
	kOhplA10b_CFG_STRING = 27,
	kOhplA10b_CFG_FLOAT = 28,
	kOhplA10b_CFG_VEC2 = 29,
	kOhplA10b_CFG_COLOR = 30,
	kOhplA10b_CFG_BOOL = 31,
	kOhplA10b_CFG_SET_BOOL = 32,
	kOhplA10b_WIDGET_LABEL = 33,
	kOhplA10b_WIDGET_BUTTON = 34,
	kOhplA10b_WIDGET_DUMMY = 35,
	kOhplA10b_WIDGET_IMAGE = 36,
	kOhplA10b_WIDGET_SLIDER = 37,
	kOhplA10b_WIDGET_DESTROY = 38,
	kOhplA10b_WIDGET_POS = 39,
	kOhplA10b_WIDGET_SIZE = 40,
	kOhplA10b_WIDGET_GET_SIZE = 41,
	kOhplA10b_WIDGET_GET_LOCAL_POS = 42,
	kOhplA10b_WIDGET_VISIBLE = 43,
	kOhplA10b_WIDGET_ENABLED = 44,
	kOhplA10b_WIDGET_IS_VISIBLE = 45,
	kOhplA10b_WIDGET_TEXT = 46,
	kOhplA10b_WIDGET_FONT_COLOR = 47,
	kOhplA10b_WIDGET_FONT_SIZE = 48,
	kOhplA10b_WIDGET_ALIGN = 49,
	kOhplA10b_WIDGET_WRAP = 50,
	kOhplA10b_WIDGET_FONT_TYPE = 51,
	kOhplA10b_WIDGET_AUTOSIZE = 52,
	kOhplA10b_WIDGET_DEFAULT_FONT = 53,
	kOhplA10b_WIDGET_CALLBACK = 54,
	kOhplA10b_SLIDER_MAX = 55,
	kOhplA10b_SLIDER_BAR = 56,
	kOhplA10b_SLIDER_SET = 57,
	kOhplA10b_SLIDER_GET = 58,
	kOhplA10b_SLIDER_GET_MAX = 59,
	kOhplA10b_SET_DEFAULT_FOCUS_NAV = 60,
	kOhplA10b_SET_FOCUSED = 61,
	kOhplA10b_MUSIC_PLAY = 62,
	kOhplA10b_MUSIC_STOP = 63,
	kOhplA10b_MUSIC_PAUSE = 64,
	kOhplA10b_MUSIC_RESUME = 65,
	kOhplA10b_UPDATER_CONTAINER = 66,
	kOhplA10b_UPDATER_RESET_ALL = 67,
	kOhplA10b_ENGINE_EXIT = 68,
	kOhplA10b_INPUT_STATE = 69,
	kOhplA10b_LOCK_INPUT = 70,
	kOhplA10b_RELATIVE_MOUSE = 71,
	kOhplA10b_GAMMA_GET = 72,
	kOhplA10b_GAMMA_SET = 73,
	kOhplA10b_BASE_FLAG = 74,
	kOhplA10b_PREMENU_PATH = 75,
	kOhplA10b_XML_OPEN = 76,
	kOhplA10b_XML_CLOSE = 77,
	kOhplA10b_XML_CHILD = 78,
	kOhplA10b_XML_CHILD_COUNT = 79,
	kOhplA10b_XML_STR = 80,
	kOhplA10b_XML_FLOAT = 81,
	kOhplA10b_XML_BOOL = 82,
	kOhplA10b_XML_COLOR = 83,
	kOhplA10b_XML_VEC2 = 84,
	kOhplA10b_XML_VEC3 = 85,
	kOhplA10b_BASE_CREATE_PROFILE = 86,
	kOhplA10b_BASE_SET_PROFILE = 87,
	kOhplA10b_BASE_INIT_USER_CONFIG = 88,
	kOhplA10b_BASE_EXIT = 89,
	kOhplA10b_BASE_START_GAME = 90,
	kOhplA10b_HELP_SET = 91,
	kOhplA10b_DRAW_SET_TO_SCREEN = 92,
	kOhplA10b_STRING_DURATION = 93,
	kOhplA10b_PLAY_GUI_SOUND = 94,
	kOhplA10b_OPEN_BROWSER = 95,
	kOhplA10b_DATE = 96,
	kOhplA10b_PROFILE_NAME = 97,
	kOhplA10b_STATE_PATH = 98,
	kOhplA10b_LOG_ERROR = 99,
	kOhplA10b_MAP_NAME = 100,
	kOhplA10b_PLAYER_VITALS = 101,
	kOhplA10b_WIDGET_GLOBAL_UI = 102,
};

enum eOhplA10bFlag
{
	kOhplA10bFlag_Fullscreen,
	kOhplA10bFlag_FirstStart,
	kOhplA10bFlag_ShowMenu,
	kOhplA10bFlag_PTest,
	kOhplA10bFlag_DebugWindow,
	kOhplA10bFlag_MapLoaded,
};

enum eOhplA10bInput
{
	kOhplA10bInput_Null,
	kOhplA10bInput_Game,
	kOhplA10bInput_PreMenu,
	kOhplA10bInput_Credits,
	kOhplA10bInput_DemoEnd,
	kOhplA10bInput_LoadScreen,
};

enum eOhplA10bMsg
{
	kOhplA10bMsg_ButtonPressed = 1,
	kOhplA10bMsg_UIArrow,
	kOhplA10bMsg_UIButton,
	kOhplA10bMsg_SliderMove,
	kOhplA10bMsg_OnUpdate,
	kOhplA10bMsg_OnDraw,
	kOhplA10bMsg_MouseUp,
	kOhplA10bMsg_MouseEnter,
	kOhplA10bMsg_MouseLeave,
};

extern "C" {

uint64_t ohpl_a10b_pm_new(const OhplA10bHost *h, uint64_t *hOut);
void ohpl_a10b_pm_delete(uint64_t o);
void ohpl_a10b_pm_enter(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_pm_leave(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_pm_update(const OhplA10bHost *h, uint64_t o, float dt);
void ohpl_a10b_pm_draw(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_pm_button_pressed(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_pm_focus(const OhplA10bHost *h, bool got);
size_t ohpl_a10b_pm_set(uint64_t o);
int ohpl_a10b_pm_state(uint64_t o);
bool ohpl_a10b_pm_continue_visible(const OhplA10bHost *h, uint64_t o);
bool ohpl_a10b_pm_cb(const OhplA10bHost *h, uint64_t o, int tag, int lval);

uint64_t ohpl_a10b_ls_new(const OhplA10bHost *h);
void ohpl_a10b_ls_delete(uint64_t o);
void ohpl_a10b_ls_load_fonts(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_ls_reset(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_ls_update(const OhplA10bHost *h, uint64_t o, float dt);
void ohpl_a10b_ls_enter(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_ls_leave(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_ls_draw(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_ls_load_done(const OhplA10bHost *h, uint64_t o, const char *snd, size_t sndLen, float loadTime);
void ohpl_a10b_ls_setup(const OhplA10bHost *h, uint64_t o, const char *cat, size_t catLen, const char *entry, size_t entryLen, int randNum, const char *image, size_t imageLen);
void ohpl_a10b_ls_draw_game_screen(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_ls_draw_menu_screen(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_ls_exit_pressed(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_ls_text(uint64_t o, int which, void (*sink)(void*, const unsigned char*, size_t), void *ctx);
int ohpl_a10b_ls_rand_num(uint64_t o);
int ohpl_a10b_ls_prev_bits(uint64_t o);
void ohpl_a10b_ls_set_prev_bits(uint64_t o, int v);

uint64_t ohpl_a10b_cr_new(const OhplA10bHost *h);
void ohpl_a10b_cr_delete(uint64_t o);
void ohpl_a10b_cr_load_fonts(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_cr_reset(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_cr_enter(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_cr_leave(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_cr_update(const OhplA10bHost *h, uint64_t o, float dt);
void ohpl_a10b_cr_add_key(const OhplA10bHost *h, uint64_t o, int key);
void ohpl_a10b_cr_setup(const OhplA10bHost *h, uint64_t o, const char *music, size_t musicLen, bool loop, const char *cat, size_t catLen, const char *entry, size_t entryLen, int endNum);
void ohpl_a10b_cr_exit_pressed(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_cr_draw(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_cr_focus(const OhplA10bHost *h, bool got);

uint64_t ohpl_a10b_de_new(const OhplA10bHost *h);
void ohpl_a10b_de_delete(uint64_t o);
bool ohpl_a10b_de_show_on_all_exit(uint64_t o);
void ohpl_a10b_de_load_fonts(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_de_reset(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_de_enter(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_de_leave(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_de_update(const OhplA10bHost *h, uint64_t o, float dt);
void ohpl_a10b_de_exit(const OhplA10bHost *h, uint64_t o, bool website);
void ohpl_a10b_de_draw(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_de_focus(const OhplA10bHost *h, bool got);
bool ohpl_a10b_de_cb(const OhplA10bHost *h, uint64_t o, int tag, size_t widget, float fval);

uint64_t ohpl_a10b_log_new();
void ohpl_a10b_log_delete(uint64_t o);
void ohpl_a10b_log_load_user_config(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_log_save_user_config(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_log_create(const OhplA10bHost *h, uint64_t o);
void ohpl_a10b_log_update(uint64_t o);
void ohpl_a10b_log_add(const OhplA10bHost *h, uint64_t o, int level, const char *msg, size_t msgLen);

}

#endif
