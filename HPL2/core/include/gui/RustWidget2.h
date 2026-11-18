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




OhplW2Combo ohpl_w2_combo_init();

OhplW2Text ohpl_w2_tb_init(bool numeric);

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
