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

struct OhplGa8Pages;

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

enum
{
	eOhplGa8Exit_None = 0,
	eOhplGa8Exit_ToJournal = 1,
	eOhplGa8Exit_ToDefault = 2,
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
