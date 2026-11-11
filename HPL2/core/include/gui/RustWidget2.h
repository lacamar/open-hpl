#ifndef HPL_RUST_WIDGET2_H
#define HPL_RUST_WIDGET2_H

// C ABI of rust/src/gui/ffi_widget2.rs: slider, tab frame, list box, combo box and text box logic.
// The widget classes stay C++; Rust decides, C++ applies.

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"
#include "system/String.h"

namespace hpl {

enum { kOhplW2SelSelect = 0, kOhplW2SelToggle = 1, kOhplW2SelFromLastCursor = 2, kOhplW2SelFromAnchor = 3 };
enum { kOhplW2OpBackground = 0, kOhplW2OpSelection = 1, kOhplW2OpText = 2, kOhplW2OpTextHighlight = 3 };
enum { kOhplW2RowNull = 1, kOhplW2RowUnselectable = 2, kOhplW2RowSelected = 4 };
enum { kOhplW2RetFalse = 0, kOhplW2RetTrue = 1, kOhplW2RetEnter = 2 };
enum { kOhplW2TbEmitSetText = 0, kOhplW2TbEmitEnter = 1 };

struct OhplW2Slider {
	bool vertical, pressed;
	float button_size;
	int32_t value, max_value, button_add, click_add, bar_value_size;
	float value_step;
	cVector3f bar_pos;
	cVector2f bar_size;
	float slider_size;
	float rect[4];
	float max_pos, min_pos;
	cVector2f rel_mouse;
};
struct OhplW2SliderDraw { cVector3f track_pos; cVector2f track_size; cVector3f bar_pos; cVector2f bar_size; };
struct OhplW2PosSize { cVector3f pos; cVector2f size; };

struct OhplW2TabState {
	int32_t first_visible, last_visible;
	float button_size, first_tab_offset, tab_separation, active_offset, inactive_offset, icon_padding,
		text_left_padding, text_right_padding;
};
struct OhplW2TabItem { bool visible, set_pos; float pos_x; };
struct OhplW2TabArrows { bool left_enabled, right_enabled, shown; };
struct OhplW2TabLayout { cVector3f label_pos, frame_pos; cVector2f frame_size; };
struct OhplW2TabInit { float label_h; cVector2f frame_size; cVector3f frame_pos; };

struct OhplW2List {
	int32_t first_item, max_items, cursor_pos, last_cursor_pos, anchor_pos, num_clicks, num_blank_rows, num_items;
	bool locked, allow_multi;
};
struct OhplW2ListUpdate { bool big; int32_t bar_value_size, max_value; };
struct OhplW2SliderSet { bool set; int32_t value; };
struct OhplW2SelAction { int32_t kind; bool flag; int32_t start, end; };
struct OhplW2Range { int32_t first, count, step; bool end_valid; };
struct OhplW2ListMouse { bool proceed; int32_t index, ty; bool clear; OhplW2SliderSet cursor; };
struct OhplW2ListKey { bool handled, arrow; OhplW2SliderSet cursor; bool select; int32_t ty; bool clear; };
struct OhplW2ListArrow { bool handled; OhplW2SliderSet cursor; bool unlock; };
struct OhplW2ListButton { int32_t ret; bool init_select, check_locked; };
struct OhplW2RowOp { int32_t kind, item; cVector3f pos; cVector2f size; };

struct OhplW2Combo {
	bool menu_open, clicked_after_open, open_by_ui;
	float menu_height;
	cVector3f menu_pos;
	int32_t mouse_over, selected, first_item, max_items, items_shown;
};
struct OhplW2ComboOpen { bool slider; cVector3f slider_pos; cVector2f slider_size; };
struct OhplW2ComboUiButton { bool ret, clear_open_by_ui; int32_t action; };
struct OhplW2ComboLayout { cVector2f button_size; cVector3f button_pos; };
struct OhplW2ComboDraw { cVector3f bg_pos; cVector2f bg_size; cVector3f border_pos; cVector2f border_size; };

struct OhplW2Text {
	int32_t marker, sel_end, first_visible, visible_size;
	float text_max_size;
	int32_t max_chars;
	float max_size_neg;
	bool can_edit, numeric;
	float numeric_add, numeric_value;
	bool numeric_updated, has_lower;
	float lower;
	bool has_upper;
	float upper;
	int32_t decimals;
	bool limit_enabled;
	uint32_t limit_max, limit_min;
	bool changed_since_enter, force_enter, enter_on_lost_focus, pressed, got_focus_recently;
};
struct OhplW2TextEnv {
	int32_t font_first, font_last;
	cVector2f font_size, corner0;
	int32_t default_decimals;
};
struct OhplW2TextDraw {
	int32_t text_start, text_len;
	cVector3f text_pos;
	bool has_marker;
	cVector3f marker_pos;
	cVector2f marker_size;
	bool has_selection;
	cVector3f sel_pos;
	cVector2f sel_size;
	int32_t hl_start, hl_len;
	cVector3f hl_pos;
};
struct OhplW2TextHost {
	void* ctx;
	bool (*glyph)(void*, int32_t, float*);
	float (*local_x)(void*);
	float (*length)(void*, const wchar_t*, size_t);
	const wchar_t* (*clipboard)(void*, size_t*);
	void (*copy)(void*, const wchar_t*, size_t);
	float (*to_float)(void*, const wchar_t*, size_t, int32_t);
	const wchar_t* (*float_text)(void*, float, int32_t, size_t*);
	void (*emit)(void*, int32_t, const wchar_t*, size_t, const wchar_t*, size_t, bool);
};
struct OhplW2TextCall {
	OhplW2Text* st;
	OhplW2TextEnv env;
	const OhplW2TextHost* host;
	const wchar_t* text;
	size_t text_len;
	const wchar_t* illegal;
	size_t illegal_len;
};

extern "C" {

OhplW2Slider ohpl_w2_slider_init(bool vertical, float buttonSize);
bool ohpl_w2_slider_set_value(OhplW2Slider* st, int v, int* out);
bool ohpl_w2_slider_set_max(OhplW2Slider* st, int m);
bool ohpl_w2_slider_set_bar_size(OhplW2Slider* st, int v);
void ohpl_w2_slider_update_bar(OhplW2Slider* st, const cVector2f* size, const cVector3f* global, const cVector2f* c0, const cVector2f* c3);
bool ohpl_w2_slider_mouse_move(OhplW2Slider* st, const cVector3f* local, const cVector3f* global, int* out);
bool ohpl_w2_slider_mouse_down(OhplW2Slider* st, int val, const cVector2f* pos, const cVector3f* local, const cVector3f* global, int* out, bool* fire);
bool ohpl_w2_slider_release(OhplW2Slider* st);
int ohpl_w2_slider_arrow(bool vertical, int arrow);
void ohpl_w2_slider_draw(const OhplW2Slider* st, const cVector2f* size, const cVector3f* global, OhplW2SliderDraw* out);
void ohpl_w2_slider_buttons(bool vertical, const cVector2f* size, float btn, OhplW2PosSize* out);

bool ohpl_w2_tab_arrow(OhplW2TabState* st, bool right, int tabs);
void ohpl_w2_tab_visibility(OhplW2TabState* st, float sizeX, const float* widths, OhplW2TabItem* items, size_t n, OhplW2TabArrows* out);
float ohpl_w2_tab_label_pos_y(const OhplW2TabState* st, bool onTop);
float ohpl_w2_tab_label_width(const OhplW2TabState* st, float textLen, float bordersW, bool hasIcon, float iconW);
void ohpl_w2_tab_label_text_pos(const OhplW2TabState* st, const cVector3f* global, bool hasIcon, float iconW, cVector3f* out);
void ohpl_w2_tab_arrow_pos(const OhplW2TabState* st, float sizeX, int i, float fgZ, cVector3f* out);
bool ohpl_w2_tab_page_target(int val, int top, int* out);
void ohpl_w2_tab_layout(const cVector2f* size, float fontH, float active, OhplW2TabLayout* out);
void ohpl_w2_tab_init(const cVector2f* size, float fontH, float active, const cVector2f* corners3, OhplW2TabInit* out);
bool ohpl_w2_tab_point_inside(const cVector2f* p, bool labelVisible, const cVector3f* labelPos, const cVector2f* labelSize, const cVector3f* framePos, const cVector2f* frameSize);

OhplW2List ohpl_w2_list_init();
void ohpl_w2_list_update(OhplW2List* st, float sizeY, float fontY, int n, OhplW2ListUpdate* out);
size_t ohpl_w2_list_prune(int* sel, size_t n, int itemsN);
int ohpl_w2_list_multi_selection(const int* sel, size_t n, int idx);
OhplW2SliderSet ohpl_w2_list_cursor1(OhplW2List* st, int pos, bool moveList, bool clamp);
OhplW2SliderSet ohpl_w2_list_cursor2(OhplW2List* st, bool moveList);
OhplW2SelAction ohpl_w2_list_select_action(const OhplW2List* st, int ty, int idx, bool exists, bool selected);
OhplW2Range ohpl_w2_list_range(int start, int end, int n);
int ohpl_w2_list_wheel(int val);
OhplW2ListMouse ohpl_w2_list_mouse_down(OhplW2List* st, float localY, float fontY, int modifier);
int ohpl_w2_list_mouse_down_finish(OhplW2List* st, bool shift, int sel);
bool ohpl_w2_list_double_click(OhplW2List* st, float localX, float sizeX, float sliderW, bool* fire);
OhplW2ListKey ohpl_w2_list_key(OhplW2List* st, int key, int modifier);
OhplW2ListKey ohpl_w2_list_key_finish(OhplW2List* st, int modifier);
OhplW2ListArrow ohpl_w2_list_ui_arrow(OhplW2List* st, int arrow);
OhplW2ListButton ohpl_w2_list_ui_button(OhplW2List* st, bool hasFocus, int val, int selected);
bool ohpl_w2_list_cursor_lines(const OhplW2List* st, const cVector3f* start, float sizeX, float sliderW, float fontY, OhplW2PosSize* out);
size_t ohpl_w2_list_rows(int first, int maxItems, int n, const cVector3f* pos, float sizeX, float fontY, const uint8_t* rows, size_t nRows, OhplW2RowOp* out, size_t cap);

OhplW2Combo ohpl_w2_combo_init();
bool ohpl_w2_combo_select1(OhplW2Combo* st, int x, bool moveList, int n, OhplW2SliderSet* out);
OhplW2SliderSet ohpl_w2_combo_select2(OhplW2Combo* st, bool moveList);
bool ohpl_w2_combo_update(OhplW2Combo* st, float fontY, int n);
bool ohpl_w2_combo_can_open(const OhplW2Combo* st, int n);
void ohpl_w2_combo_open(OhplW2Combo* st, int n, const cVector2f* size, const cVector2f* borders2, float globalY, float popupZ, float virtualY, OhplW2ComboOpen* out);
bool ohpl_w2_combo_close(OhplW2Combo* st);
int ohpl_w2_combo_button_press(OhplW2Combo* st, int val, const cVector3f* local, float sizeX);
bool ohpl_w2_combo_mouse_move(OhplW2Combo* st, bool mouseOver, const cVector3f* local, float sizeX, bool sliderEnabled, float corner0Y, float fontY, int n);
bool ohpl_w2_combo_mouse_down_pre(OhplW2Combo* st, int val, int* wheel);
bool ohpl_w2_combo_mouse_down_closes(const OhplW2Combo* st, float localY);
bool ohpl_w2_combo_mouse_up_pre(const OhplW2Combo* st, int val);
bool ohpl_w2_combo_mouse_up_selects(const OhplW2Combo* st, float localY);
bool ohpl_w2_combo_ui_arrow(OhplW2Combo* st, int arrow, int n, int value, int add, int* set, bool* hasSet);
bool ohpl_w2_combo_lost_focus(const OhplW2Combo* st, const cVector3f* local, float sizeX);
bool ohpl_w2_combo_point_inside(const OhplW2Combo* st, const cVector3f* local, const cVector2f* size);
bool ohpl_w2_combo_slider_arrow(OhplW2Combo* st, int arrow, int n);
OhplW2ComboUiButton ohpl_w2_combo_ui_button(OhplW2Combo* st, bool hasFocus, int val);
void ohpl_w2_combo_layout(const cVector2f* size, const cVector2f* back, float buttonW, OhplW2ComboLayout* out);
void ohpl_w2_combo_draw(const OhplW2Combo* st, const cVector3f* global, float sizeX, OhplW2ComboDraw* out);
size_t ohpl_w2_combo_rows(const OhplW2Combo* st, const cVector3f* global, float sizeX, float fontY, int n, OhplW2RowOp* out, size_t cap);

OhplW2Text ohpl_w2_tb_init(bool numeric);
void ohpl_w2_tb_layout(const OhplW2Text* st, float sizeX, const cVector2f* borders3, float fontH, float buttonW, float* outH, float* outW);
void ohpl_w2_tb_change_text(const OhplW2TextCall* c, bool inSetText);
void ohpl_w2_tb_set_max_len(const OhplW2TextCall* c, int len);
void ohpl_w2_tb_set_selected(const OhplW2TextCall* c, int start, int count);
void ohpl_w2_tb_set_numeric(const OhplW2TextCall* c, float v);
float ohpl_w2_tb_get_numeric(const OhplW2TextCall* c);
int ohpl_w2_tb_button_pressed(const OhplW2TextCall* c, int which, bool enabled);
bool ohpl_w2_tb_enter(const OhplW2TextCall* c);
int ohpl_w2_tb_mouse_move(const OhplW2TextCall* c);
int ohpl_w2_tb_mouse_down(const OhplW2TextCall* c, int val, bool* attention);
bool ohpl_w2_tb_mouse_up(const OhplW2TextCall* c, int val, int* marker, int* sel);
void ohpl_w2_tb_mouse_up_restore(const OhplW2TextCall* c, int marker, int sel);
void ohpl_w2_tb_double_click(const OhplW2TextCall* c, int val);
void ohpl_w2_tb_got_focus(const OhplW2TextCall* c);
void ohpl_w2_tb_got_tab_focus(const OhplW2TextCall* c);
void ohpl_w2_tb_lost_focus(const OhplW2TextCall* c);
int ohpl_w2_tb_key(const OhplW2TextCall* c, int key, int modifier, uint32_t unicode, bool* anim);
bool ohpl_w2_tb_ui_button(const OhplW2TextCall* c, bool hasFocus, int val);
void ohpl_w2_tb_ui_keyboard_open(const OhplW2TextCall* c);
void ohpl_w2_tb_draw(const OhplW2TextCall* c, const cVector3f* global, OhplW2TextDraw* out);

}

static_assert(sizeof(OhplW2Slider) == 88, "OhplW2Slider layout");
static_assert(sizeof(OhplW2SliderDraw) == 40, "OhplW2SliderDraw layout");
static_assert(sizeof(OhplW2PosSize) == 20, "OhplW2PosSize layout");
static_assert(sizeof(OhplW2TabState) == 40, "OhplW2TabState layout");
static_assert(sizeof(OhplW2TabItem) == 8, "OhplW2TabItem layout");
static_assert(sizeof(OhplW2TabArrows) == 3, "OhplW2TabArrows layout");
static_assert(sizeof(OhplW2TabLayout) == 32, "OhplW2TabLayout layout");
static_assert(sizeof(OhplW2TabInit) == 24, "OhplW2TabInit layout");
static_assert(sizeof(OhplW2List) == 36, "OhplW2List layout");
static_assert(sizeof(OhplW2ListUpdate) == 12, "OhplW2ListUpdate layout");
static_assert(sizeof(OhplW2SliderSet) == 8, "OhplW2SliderSet layout");
static_assert(sizeof(OhplW2SelAction) == 16, "OhplW2SelAction layout");
static_assert(sizeof(OhplW2Range) == 16, "OhplW2Range layout");
static_assert(sizeof(OhplW2ListMouse) == 24, "OhplW2ListMouse layout");
static_assert(sizeof(OhplW2ListKey) == 24, "OhplW2ListKey layout");
static_assert(sizeof(OhplW2ListArrow) == 16, "OhplW2ListArrow layout");
static_assert(sizeof(OhplW2ListButton) == 8, "OhplW2ListButton layout");
static_assert(sizeof(OhplW2RowOp) == 28, "OhplW2RowOp layout");
static_assert(sizeof(OhplW2Combo) == 40, "OhplW2Combo layout");
static_assert(sizeof(OhplW2ComboOpen) == 24, "OhplW2ComboOpen layout");
static_assert(sizeof(OhplW2ComboUiButton) == 8, "OhplW2ComboUiButton layout");
static_assert(sizeof(OhplW2ComboLayout) == 20, "OhplW2ComboLayout layout");
static_assert(sizeof(OhplW2ComboDraw) == 40, "OhplW2ComboDraw layout");
static_assert(sizeof(OhplW2Text) == 80, "OhplW2Text layout");
static_assert(sizeof(OhplW2TextEnv) == 28, "OhplW2TextEnv layout");
static_assert(sizeof(OhplW2TextDraw) == 88, "OhplW2TextDraw layout");
static_assert(sizeof(OhplW2TextHost) == 72, "OhplW2TextHost layout");
static_assert(sizeof(OhplW2TextCall) == 80, "OhplW2TextCall layout");

static_assert(sizeof(wchar_t) == 4, "tWString is handed to Rust as u32 code units");

}

#endif // HPL_RUST_WIDGET2_H
