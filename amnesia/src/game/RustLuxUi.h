#ifndef LUX_RUST_UI_H
#define LUX_RUST_UI_H

// C ABI of rust/src/luxui/ffi.rs: the decisions, timing and layout math of the inventory,
// journal, item types, hints, messages, commentary icons and key field. Calls that produce
// ordered side effects fill an OhplGa8Ev array; OhplGa8Take copies it (strings included) before
// any event runs, since running one may call back into Rust.

#include <cstddef>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

struct OhplGa8Str
{
	const unsigned char *p;
	size_t n;
};

struct OhplGa8Ev
{
	unsigned kind;
	int a, b;
	float v[10];
	OhplGa8Str s0, s1;
};
static_assert(sizeof(OhplGa8Ev) == 88, "OhplGa8Ev layout");

#define OHPL_GA8_EVMAX 64

struct OhplGa8Event
{
	unsigned kind;
	int a, b;
	float v[10];
	std::string s0, s1;
};

inline std::vector<OhplGa8Event> OhplGa8Take(const OhplGa8Ev *apEv, size_t alNum)
{
	std::vector<OhplGa8Event> vOut;
	vOut.reserve(alNum);
	for(size_t i=0; i<alNum; ++i)
	{
		OhplGa8Event e;
		e.kind = apEv[i].kind;
		e.a = apEv[i].a;
		e.b = apEv[i].b;
		for(int j=0; j<10; ++j) e.v[j] = apEv[i].v[j];
		if(apEv[i].s0.p) e.s0.assign((const char*)apEv[i].s0.p, apEv[i].s0.n);
		if(apEv[i].s1.p) e.s1.assign((const char*)apEv[i].s1.p, apEv[i].s1.n);
		vOut.push_back(e);
	}
	return vOut;
}

struct OhplGa8Buf
{
	OhplGa8Ev mvRaw[OHPL_GA8_EVMAX];
};

inline OhplGa8Str OhplGa8S(const std::string &asX)
{
	OhplGa8Str s = { (const unsigned char*)asX.data(), asX.size() };
	return s;
}

typedef void (*OhplGa8Sink)(void*, const unsigned char*, size_t);
typedef void (*OhplGa8Sink32)(void*, const unsigned*, size_t);
typedef void (*OhplGa8Replace)(void*, const unsigned*, size_t, int, OhplGa8Sink32, void*);

inline void OhplGa8StrSink(void *apCtx, const unsigned char *apData, size_t alLen)
{
	((std::string*)apCtx)->assign((const char*)apData, alLen);
}

inline void OhplGa8WideSink(void *apCtx, const unsigned *apData, size_t alLen)
{
	((std::wstring*)apCtx)->assign((const wchar_t*)apData, alLen);
}

struct OhplGa8ItInfo
{
	OhplGa8Str name;
	int has_count;
	int max_count;
	int show_pickup;
};

struct OhplGa8ItItem
{
	OhplGa8Str name, sval, extra;
	float amount;
};

struct OhplGa8ItPlayer
{
	int coins;
	float health, sanity, lamp_oil;
	int has_lantern, lantern_active, use_item_state;
};

struct OhplGa8HintState
{
	unsigned char active;
	float alpha, show_count, y_pos;
	float font_size[2];
	float duration_mul;
	float osc[6];
};
static_assert(sizeof(OhplGa8HintState) == 52, "OhplGa8HintState layout");

struct OhplGa8MessageState
{
	unsigned char show_subtitles, show_effect_subtitles, pause_active, quest_active, yes_no;
	float pause_alpha, message_alpha, message_time, quest_alpha, quest_time;
	float font_size[2];
	float quest_osc[6];
};
static_assert(sizeof(OhplGa8MessageState) == 60, "OhplGa8MessageState layout");

struct OhplGa8InvState
{
	unsigned char active, exit_to_journal, enter_from_journal, message_active, picked_moved;
	float fade_in_time, fade_out_time, alpha, old_desc_alpha, message_life, message_alpha, timer, oil_t;
	double picked_at;
	float bubble_pos[3][3];
	float bubble_speed[3];
	float pulse_middle[4];
	float pulse_freq[4];
};
static_assert(sizeof(OhplGa8InvState) == 128, "OhplGa8InvState layout");

struct OhplGa8SlotDraw
{
	float pos[3];
	int glow;
	float glow_alpha;
	float glow_pos[3];
	float count_pos[3];
};

struct OhplGa8FrameIn
{
	float widget_pos[3], widget_size[2], hpad[2], vpad[2], offset[3], size[2], c0[2], c1[2], c3[2];
	float alpha;
};

struct OhplGa8GridIn
{
	float widget_size[2], h_ext[2], v_ext[2], separation[3], slot_size[2], start[3], frame_hpad[2], frame_vpad[2],
		frame_offset[3], grid_offset[3], color[4];
	int rows, cols;
	float alpha;
};

struct OhplGa8Pages;

// Item type events (ohpl_ga8_it_*).
enum
{
	eOhplGa8It_ProgLog = 1,
	eOhplGa8It_Completion,
	eOhplGa8It_SetCoins,
	eOhplGa8It_Hint,
	eOhplGa8It_SetContainerJournal,
	eOhplGa8It_ForceInstantExit,
	eOhplGa8It_OpenNote,
	eOhplGa8It_RunMapScript,
	eOhplGa8It_OpenDiary,
	eOhplGa8It_DiaryLastRead,
	eOhplGa8It_LanternToggle,
	eOhplGa8It_SetHealth,
	eOhplGa8It_AddSanity,
	eOhplGa8It_SetLampOil,
	eOhplGa8It_RemoveItem,
	eOhplGa8It_Sound,
	eOhplGa8It_MessageKey,
	eOhplGa8It_UseItemStateReset,
	eOhplGa8It_UseItemEnter,
	eOhplGa8It_ExitPressed,
	eOhplGa8It_Equip,
	eOhplGa8It_AddTinderbox,
};

enum
{
	eOhplGa8Completion_Item = 0,
	eOhplGa8Completion_Note = 1,
	eOhplGa8Completion_Diary = 2,
};

// Message events (ohpl_ga8_msg_*).
enum
{
	eOhplGa8Msg_QuestSound = 1,
	eOhplGa8Msg_SetUpdateActive,
	eOhplGa8Msg_Callback,
	eOhplGa8Msg_Row,
	eOhplGa8Msg_PauseBg,
	eOhplGa8Msg_PauseRow,
	eOhplGa8Msg_Yes,
	eOhplGa8Msg_YesKey,
	eOhplGa8Msg_No,
	eOhplGa8Msg_NoKey,
	eOhplGa8Msg_QuestIcon,
	eOhplGa8Msg_QuestGlow,
};

enum
{
	eOhplGa8MsgAction_Interact = 0,
	eOhplGa8MsgAction_Attack = 1,
};

// Inventory draw events and plans.
enum
{
	eOhplGa8Draw_Corner = 1,
	eOhplGa8Draw_Border,
	eOhplGa8Draw_GridLine,
	eOhplGa8Draw_DescRow,
	eOhplGa8Draw_ClipPush,
	eOhplGa8Draw_ClipPop,
	eOhplGa8Draw_OilLiquid,
	eOhplGa8Draw_OilBubble,
};

enum
{
	eOhplGa8Exit_None = 0,
	eOhplGa8Exit_ToJournal = 1,
	eOhplGa8Exit_ToDefault = 2,
};

enum
{
	eOhplGa8Press_None = 0,
	eOhplGa8Press_ClearMessage = 1,
	eOhplGa8Press_Exit = 2,
};

enum
{
	eOhplGa8Add_Reject = 0,
	eOhplGa8Add_Count = 1,
	eOhplGa8Add_Create = 2,
};

enum
{
	eOhplGa8Up_None = 0,
	eOhplGa8Up_Combine = 1,
};

enum
{
	eOhplGa8Enter_Combine = 1,
	eOhplGa8Enter_Item = 2,
	eOhplGa8Enter_Clear = 3,
};

enum
{
	eOhplGa8Release_Ignore = 0,
	eOhplGa8Release_DoubleClick = 1,
	eOhplGa8Release_MouseUp = 2,
};

enum
{
	eOhplGa8Nav_None = 0,
	eOhplGa8Nav_Tinderbox = 1,
	eOhplGa8Nav_Oil = 2,
	eOhplGa8Nav_Journal = 3,
	eOhplGa8Nav_Sanity = 4,
	eOhplGa8Nav_Health = 5,
};

enum
{
	eOhplGa8MouseOver_Invalid = -1,
	eOhplGa8MouseOver_None = 0,
	eOhplGa8MouseOver_Show = 1,
	eOhplGa8MouseOver_Hide = 2,
	eOhplGa8MouseOver_HideAndClear = 3,
};

enum
{
	eOhplGa8Ps_None = 0,
	eOhplGa8Ps_Create = 1,
	eOhplGa8Ps_Kill = 2,
};

// Journal plans.
enum
{
	eOhplGa8JPress_None = 0,
	eOhplGa8JPress_NextPage = 1,
	eOhplGa8JPress_ExitHint = 2,
	eOhplGa8JPress_Exit = 3,
	eOhplGa8JPress_ChangeState = 4,
};

enum
{
	eOhplGa8JBtn_BackArrow = 1,
	eOhplGa8JBtn_FwdArrow = 2,
	eOhplGa8JBtn_Exit = 3,
};

enum
{
	eOhplGa8Archivist_None = 0,
	eOhplGa8Archivist_Tdd = 1,
	eOhplGa8Archivist_Justine = 2,
};

enum
{
	eOhplGa8Quest_Missing = 0,
	eOhplGa8Quest_Disabled = 1,
	eOhplGa8Quest_AlreadyOff = 2,
};

extern "C"
{
int ohpl_ga8_key_verify(unsigned char c);
int ohpl_ga8_key_route(int keyboard, int paste, int tab);
int ohpl_ga8_key_action(int composed, int len, int pos, int mark, unsigned char ascii, int* out_pos, int* out_epos, unsigned char* out_ch);
size_t ohpl_ga8_key_paste(const unsigned char* p, size_t n, unsigned char* out, size_t cap);
int ohpl_ga8_it_info(int t, OhplGa8ItInfo* out);
int ohpl_ga8_it_from_string(const unsigned char* p, size_t n);
int ohpl_ga8_it_has_max(int t);
size_t ohpl_ga8_it_before_add(int t, const OhplGa8ItItem* it, const OhplGa8ItPlayer* p, int* consumed, OhplGa8Ev* out, size_t cap);
size_t ohpl_ga8_it_note_before_add(int null, const OhplGa8ItItem* it, OhplGa8Ev* out, size_t cap);
size_t ohpl_ga8_it_diary_before_add(int null, int idx, const OhplGa8ItItem* it, int* go, OhplGa8Ev* out, size_t cap);
size_t ohpl_ga8_it_diary_after(int show, OhplGa8Ev* out, size_t cap);
size_t ohpl_ga8_it_on_use(int t, const OhplGa8ItItem* it, const OhplGa8ItPlayer* p, OhplGa8Ev* out, size_t cap);
OhplGa8Str ohpl_ga8_it_lantern_key(int active);
void ohpl_ga8_hint_init(OhplGa8HintState* s, float y, float fx, float fy, float mul);
void ohpl_ga8_hint_reset(OhplGa8HintState* s);
int ohpl_ga8_hint_set_active(OhplGa8HintState* s, int on);
void ohpl_ga8_hint_update(OhplGa8HintState* s, float dt);
int ohpl_ga8_hint_can_add(const OhplGa8HintState* s, int blocked, int given);
void ohpl_ga8_hint_start(OhplGa8HintState* s, float time, float duration);
int ohpl_ga8_hint_drawable(const OhplGa8HintState* s, int ptest);
void ohpl_ga8_hint_label(const OhplGa8HintState* s, float* out);
void ohpl_ga8_hint_row(const OhplGa8HintState* s, size_t rows, size_t i, float* out);
float ohpl_ga8_hint_wrap_width(void);
float ohpl_ga8_hint_wrap_line(void);
void ohpl_ga8_strip_icons(const unsigned* text, size_t n, OhplGa8Replace replace, void* rctx, OhplGa8Sink32 sink, void* sctx);
void ohpl_ga8_msg_reset(OhplGa8MessageState* s);
void ohpl_ga8_msg_start_quest(OhplGa8MessageState* s);
void ohpl_ga8_msg_set_message(OhplGa8MessageState* s, float time, float duration);
size_t ohpl_ga8_msg_start_pause(OhplGa8MessageState* s, int yes_no, OhplGa8Ev* out, size_t cap);
size_t ohpl_ga8_msg_set_pause(OhplGa8MessageState* s, int on, OhplGa8Ev* out, size_t cap);
size_t ohpl_ga8_msg_do_action(OhplGa8MessageState* s, int action, int pressed, int has_callback, OhplGa8Ev* out, size_t cap);
size_t ohpl_ga8_msg_update(OhplGa8MessageState* s, float dt, OhplGa8Ev* out, size_t cap);
size_t ohpl_ga8_msg_draw_message(const OhplGa8MessageState* s, size_t rows, OhplGa8Ev* out, size_t cap);
size_t ohpl_ga8_msg_draw_pause(const OhplGa8MessageState* s, size_t rows, OhplGa8Ev* out, size_t cap);
size_t ohpl_ga8_msg_draw_quest(const OhplGa8MessageState* s, const float* icon, const float* center, const float* start, OhplGa8Ev* out, size_t cap);
float ohpl_ga8_comm_spin(float y, int playing, float dt);
int ohpl_ga8_comm_set_playing(int current, int next);
void ohpl_ga8_comm_focus_text(const unsigned char* talker, size_t tn, const unsigned char* topic, size_t pn, OhplGa8Sink s, void* c);
void ohpl_ga8_comm_file(const unsigned char* file, size_t n, OhplGa8Sink s, void* c);
void ohpl_ga8_inv_init(OhplGa8InvState* s, float fade_in, float fade_out, const float* bubbles);
void ohpl_ga8_inv_reset(OhplGa8InvState* s);
void ohpl_ga8_inv_reset_session(OhplGa8InvState* s);
int ohpl_ga8_inv_should_drop(int left, int primary);
int ohpl_ga8_inv_update_alpha(OhplGa8InvState* s, float dt);
void ohpl_ga8_inv_update_texts(OhplGa8InvState* s, float dt);
int ohpl_ga8_inv_exit_pressed(OhplGa8InvState* s);
int ohpl_ga8_inv_open_journal(OhplGa8InvState* s);
int ohpl_ga8_inv_pauses_voices(const OhplGa8InvState* s);
int ohpl_ga8_inv_leave(OhplGa8InvState* s);
int ohpl_ga8_inv_set_desc(OhplGa8InvState* s, int header_empty, int normal_empty);
void ohpl_ga8_inv_set_message(OhplGa8InvState* s, float life, float duration);
void ohpl_ga8_inv_set_picked(OhplGa8InvState* s, double t);
void ohpl_ga8_inv_note_current(OhplGa8InvState* s, double t);
int ohpl_ga8_inv_status_index(float x);
float ohpl_ga8_inv_glow_alpha(const OhplGa8InvState* s, size_t idx);
int ohpl_ga8_inv_add_plan(int ty, const unsigned char* sub, size_t subn, int existing, int count);
void ohpl_ga8_inv_tinderbox(const unsigned char* cur, size_t n, int* unlock, OhplGa8Sink s, void* c);
int ohpl_ga8_inv_still_alive(const unsigned char* p, size_t n);
void ohpl_ga8_inv_item_strings(const unsigned char* sub, size_t sn, const unsigned char* image, size_t inn, OhplGa8Sink s, void* c0, void* c1, void* c2);
int ohpl_ga8_inv_remove_one(int has_count, int after);
int ohpl_ga8_inv_combine_matches(OhplGa8Str a, OhplGa8Str b, OhplGa8Str x, OhplGa8Str y);
int ohpl_ga8_inv_special_combine(int a, int b);
void ohpl_ga8_inv_script_cmd(OhplGa8Str f, OhplGa8Str a, OhplGa8Str b, OhplGa8Sink s, void* c);
int ohpl_ga8_inv_slot_up(int item, int picked, int same);
int ohpl_ga8_inv_slot_enter(int item, int message, int picked, int same);
int ohpl_ga8_inv_slot_leave(int item, int current_none);
int ohpl_ga8_inv_slot_release(const OhplGa8InvState* s, int button, int same, double t);
float ohpl_ga8_inv_slot_glow(float glow, int hovered, float dt);
void ohpl_ga8_inv_pick_offset(int mouse_mode, const float* gpos, const float* size, const float* image, const float* mouse, float* out);
void ohpl_ga8_inv_slot_draw(const OhplGa8InvState* s, const float* gpos, const float* size, const float* image, float glow, const float* pulse, const float* count_offset, OhplGa8SlotDraw* out);
int ohpl_ga8_inv_nav(int button, int user);
int ohpl_ga8_inv_mouse_over(int fader, int message, int current, int active, int same);
size_t ohpl_ga8_inv_draw_frame(const float* pos, const float* size, const float* c0, const float* c1, const float* c3, float alpha, OhplGa8Ev* out, size_t cap);
size_t ohpl_ga8_inv_frame_around(const OhplGa8FrameIn* f, OhplGa8Ev* out, size_t cap);
size_t ohpl_ga8_inv_grid(const OhplGa8GridIn* g, OhplGa8Ev* out, size_t cap);
size_t ohpl_ga8_inv_desc_rows(const float* center, float width, const float* font, size_t rows, const float* color, float alpha, OhplGa8Ev* out, size_t cap);
float ohpl_ga8_inv_wrap_line(float font_h);
void ohpl_ga8_inv_picked_pos(const float* offset, int mouse_down, const float* mouse, const float* wpos, const float* wsize, float* out);
void ohpl_ga8_inv_message_bar(float start_x, float width, const float* center, float font_h, float* pos, float* size);
float ohpl_ga8_inv_message_alpha(const OhplGa8InvState* s);
void ohpl_ga8_inv_text_frame_pos(const float* header, float frame_x, const float* c0, float hpad_x, float vpad_y, const float* offset, float* out);
void ohpl_ga8_inv_text_frame_default(float width, float* out);
void ohpl_ga8_inv_element_at_center(const float* center, const float* image, const float* size, float alpha, int mul, float* pos, float* sz, float* col);
float ohpl_ga8_inv_oil_height(float oil, float h);
size_t ohpl_ga8_inv_draw_oil(const OhplGa8InvState* s, const float* center, const float* wpos, const float* wsize, float oil, const float* bubbles, OhplGa8Ev* out, size_t cap);
int ohpl_ga8_inv_oil_update(OhplGa8InvState* s, const float* wpos, const float* wsize, float oil, float dt);
void ohpl_ga8_inv_status_pos(const float* center, const float* image, float* out);
OhplGa8Pages* ohpl_ga8_jrn_pages_new(void);
void ohpl_ga8_jrn_pages_free(OhplGa8Pages* p);
void ohpl_ga8_jrn_load_text(OhplGa8Pages* pg, const unsigned* const* rows, const size_t* lens, size_t n, int max_rows);
void ohpl_ga8_jrn_load_narration(OhplGa8Pages* pg, const unsigned* const* rows, const size_t* lens, size_t n, size_t* cnt, OhplGa8Sink s, void* c);
size_t ohpl_ga8_jrn_page_count(const OhplGa8Pages* pg);
size_t ohpl_ga8_jrn_row_count(const OhplGa8Pages* pg, size_t page);
const unsigned* ohpl_ga8_jrn_row(const OhplGa8Pages* pg, size_t page, size_t row, size_t* n);
OhplGa8Str ohpl_ga8_jrn_voice(const OhplGa8Pages* pg, size_t page);
OhplGa8Str ohpl_ga8_jrn_background(int state);
int ohpl_ga8_jrn_update_alpha(float* alpha, int active, int from_inv, float dt);
float ohpl_ga8_jrn_state_alpha(float alpha, int current, float dt);
float ohpl_ga8_jrn_hover_alpha(float alpha, int hovered, float dt);
int ohpl_ga8_jrn_narration_done(int state, int active, int valid);
int ohpl_ga8_jrn_exit_pressed(int active, int instant, int force, int state, int page, size_t pages, int* arg);
int ohpl_ga8_jrn_list_index(int state);
void ohpl_ga8_jrn_entries(int kind, OhplGa8Str name, int num, OhplGa8Sink s, void* c0, void* c1);
void ohpl_ga8_jrn_image_files(OhplGa8Str image, OhplGa8Sink s, void* c0, void* c1);
int ohpl_ga8_jrn_archivist(OhplGa8Str game, size_t notes);
int ohpl_ga8_jrn_disable_quest(int found, int active);
int ohpl_ga8_jrn_find_index(const size_t* items, size_t n, size_t target);
void ohpl_ga8_jrn_find_diary(const size_t* counts, size_t nc, const size_t* items, size_t ni, size_t target, int* cat, int* entry);
int ohpl_ga8_jrn_last_read(int kind, int cat, int entry, size_t notes, const size_t* counts, size_t nc);
void ohpl_ga8_jrn_note_arrows(int page, size_t pages, int* back, int* fwd);
int ohpl_ga8_jrn_narrated_plays(int page, size_t pages);
float ohpl_ga8_jrn_page_start_y(size_t rows, float dist);
void ohpl_ga8_jrn_nav_positions(float width, float start_y, int max_rows, float dist, float* pos, float* back_x);
size_t ohpl_ga8_jrn_layout_list(size_t entries, size_t* page, float* y);
size_t ohpl_ga8_jrn_layout_quests(const size_t* rows, size_t n, size_t* page, float* y);
int ohpl_ga8_jrn_start_page(size_t pages);
void ohpl_ga8_jrn_list_page(int page, size_t pages, int state, int* no_pages, int* back, int* fwd, int* default_return);
int ohpl_ga8_jrn_main_click(int user);
int ohpl_ga8_jrn_arrow_click(int user, int* kind, int* delta);
int ohpl_ga8_jrn_ui_button(int kind, int button, int back, int fwd);
int ohpl_ga8_jrn_ui_arrow(int kind, int arrow, int back, int fwd, int down_ok);
int ohpl_ga8_jrn_main_ui_button(int button);
void ohpl_ga8_jrn_hover_effect(float pulse, float color_alpha, float effect_alpha, float* out);
void ohpl_ga8_jrn_effect_pos(int centered, const float* gpos, float size_x, float x_add, float* out);
void ohpl_ga8_jrn_effect_size(const float* font, float x_add, float* out);
float ohpl_ga8_jrn_fade_alpha(float alpha);
float ohpl_ga8_msg_wrap_width(int pause);
float ohpl_ga8_msg_line_height(float font_h);
int ohpl_ga8_jrn_is_open(int state);
}

#endif // LUX_RUST_UI_H
