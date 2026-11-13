#ifndef HPL_RUST_GUI_H20_H
#define HPL_RUST_GUI_H20_H

// C ABI of rust/src/gui/h20_*.rs: text box, list boxes, combo box, tab frame and menus (OHPL_IMPL=rust).
// Widget state structs are embedded in the C++ widgets and mutated only by Rust; OhplH20Host::op reaches
// back for items, sliders, skin and drawing (RustGuiH20.cpp). Op argument use is documented there.

#include <cstddef>
#include <cstdint>
#include "gui/RustGuiH19.h"
#include "gui/RustWidget2.h"

namespace hpl {

enum OhplH20Op {
	kOhplH20Op_SLIDER_SET_VALUE = 1,
	kOhplH20Op_SLIDER_GET_VALUE = 2,
	kOhplH20Op_SLIDER_SET_BAR_VALUE_SIZE = 3,
	kOhplH20Op_SLIDER_SET_MAX_VALUE = 4,
	kOhplH20Op_SLIDER_SET_BUTTON_VALUE_ADD = 5,
	kOhplH20Op_SLIDER_SET_BAR_CLICK_VALUE_ADD = 6,
	kOhplH20Op_SLIDER_GET_BUTTON_VALUE_ADD = 7,
	kOhplH20Op_WIDGET_SET_SIZE = 8,
	kOhplH20Op_WIDGET_SET_POSITION = 9,
	kOhplH20Op_WIDGET_GET_SIZE = 10,
	kOhplH20Op_ITEM_NUM = 11,
	kOhplH20Op_ITEM_GET = 12,
	kOhplH20Op_ITEM_IS_SELECTED = 13,
	kOhplH20Op_ITEM_SET_SELECTED = 14,
	kOhplH20Op_ITEM_IS_SELECTABLE = 15,
	kOhplH20Op_ITEM_INDEX = 16,
	kOhplH20Op_ITEM_TEXT = 17,
	kOhplH20Op_PROCESS_MESSAGE = 18,
	kOhplH20Op_WORLD_TO_LOCAL = 19,
	kOhplH20Op_GET_GLOBAL = 20,
	kOhplH20Op_GET_FONT_SIZE = 21,
	kOhplH20Op_HAS_FOCUS = 22,
	kOhplH20Op_CREATE_SLIDER = 23,
	kOhplH20Op_ADD_CALLBACK = 24,
	kOhplH20Op_DRAW_GFX = 25,
	kOhplH20Op_DRAW_BORDERS = 26,
	kOhplH20Op_SKIN_GFX = 27,
	kOhplH20Op_SKIN_ATTRIBUTE = 28,
	kOhplH20Op_GFX_ACTIVE_SIZE = 29,
	kOhplH20Op_GFX_IMAGE_SIZE = 30,
	kOhplH20Op_CLIP_CREATE_CHILD = 31,
	kOhplH20Op_SET_CLIP = 32,
	kOhplH20Op_DRAW_TEXT = 33,
	kOhplH20Op_GFX_RECT = 34,
	kOhplH20Op_CREATE_LABEL = 35,
	kOhplH20Op_LABEL_SET_COLOR = 36,
	kOhplH20Op_LABEL_SET_FONT_SIZE = 37,
	kOhplH20Op_TEXT_LENGTH = 38,
	kOhplH20Op_ITEM_PROP_GET = 39,
	kOhplH20Op_PROP_TYPE = 40,
	kOhplH20Op_PROP_TEXT = 41,
	kOhplH20Op_PROP_GFX = 42,
	kOhplH20Op_WIDGET_GET_TEXT = 43,
	kOhplH20Op_DESTROY_WIDGET = 44,
	kOhplH20Op_SET_DESTROYING = 45,
	kOhplH20Op_WIDGET_SET_VISIBLE = 46,
	kOhplH20Op_WIDGET_SET_ENABLED = 47,
	kOhplH20Op_WIDGET_IS_ENABLED = 48,
	kOhplH20Op_CLIP_GET_CURRENT = 49,
	kOhplH20Op_SET_CALL = 50,
	kOhplH20Op_WIDGET_SET_TEXT = 51,
	kOhplH20Op_WIDGET_SET_FONT_SIZE = 52,
	kOhplH20Op_TEXTBOX_SET_CAN_EDIT = 53,
	kOhplH20Op_TEXTBOX_GET_CAN_EDIT = 54,
	kOhplH20Op_TEXTBOX_BG_SIZE = 55,
	kOhplH20Op_CREATE_TEXTBOX = 56,
	kOhplH20Op_CREATE_BUTTON = 57,
	kOhplH20Op_BUTTON_SET_IMAGE = 58,
	kOhplH20Op_CHECK_POINT_CLIP = 59,
	kOhplH20Op_WIDGET_GET_POSITION = 60,
	kOhplH20Op_WIDGET_IS_VISIBLE = 61,
};

struct OhplH20Args {
	uintptr_t w;
	uintptr_t p[3];
	int32_t i[4];
	float f[10];
	const wchar_t* text;
	size_t textLen;
	uintptr_t rp;
	int32_t ri[2];
	float rf[3];
};
static_assert(sizeof(OhplH20Args) == 136 && offsetof(OhplH20Args, text) == 88 && offsetof(OhplH20Args, rp) == 104 &&
			  offsetof(OhplH20Args, rf) == 120, "OhplH20Args ABI");

struct OhplH20Host {
	void* ctx;
	uintptr_t (*op)(void*, int, OhplH20Args*);
};

extern const OhplH20Host gH20Host;

struct OhplH20ListBase {
	OhplW2List list;
	void* selected;
	void* columns;
	uintptr_t slider;
	uintptr_t gfx[10];
	float bgZ;
	float sliderW;
	bool clearing;
	int32_t kind;
	uintptr_t self;
	uintptr_t container;
	OhplH19Base* base;
};
static_assert(sizeof(OhplH20ListBase) == 184 && offsetof(OhplH20ListBase, selected) == 40 &&
			  offsetof(OhplH20ListBase, gfx) == 64 && offsetof(OhplH20ListBase, bgZ) == 144 &&
			  offsetof(OhplH20ListBase, kind) == 156 && offsetof(OhplH20ListBase, base) == 176, "OhplH20ListBase ABI");

struct OhplH20Combo {
	OhplW2Combo combo;
	uintptr_t text, button, slider;
	uintptr_t gfx[10];
	float buttonW, sliderW;
	uintptr_t self;
	uintptr_t container;
	OhplH19Base* base;
	const OhplH20Host* host;
};
static_assert(sizeof(OhplH20Combo) == 184 && offsetof(OhplH20Combo, text) == 40 && offsetof(OhplH20Combo, gfx) == 64 &&
			  offsetof(OhplH20Combo, buttonW) == 144 && offsetof(OhplH20Combo, self) == 152 && offsetof(OhplH20Combo, host) == 176,
			  "OhplH20Combo ABI");

extern "C" {

void ohpl_h20_lb_init(const OhplH20Host*, OhplH20ListBase*, uintptr_t self, uintptr_t container, OhplH19Base* base, int kind);
void ohpl_h20_lb_free(OhplH20ListBase*);
void ohpl_h20_lb_destroy_slider(const OhplH20Host*, OhplH20ListBase*);
void ohpl_h20_lb_destroy_columns(const OhplH20Host*, OhplH20ListBase*);
void ohpl_h20_lb_set_selected_item(const OhplH20Host*, OhplH20ListBase*, int x, bool moveList, bool callback, int type, bool clearPrevious);
int ohpl_h20_lb_selected_item(const OhplH20Host*, OhplH20ListBase*);
int ohpl_h20_lb_multi_selection(const OhplH20Host*, OhplH20ListBase*, int idx);
int ohpl_h20_lb_multi_selection_num(const OhplH20Host*, OhplH20ListBase*);
void ohpl_h20_lb_clear_selection(const OhplH20Host*, OhplH20ListBase*);
void ohpl_h20_lb_update_properties(const OhplH20Host*, OhplH20ListBase*);
void ohpl_h20_lb_on_change_size(const OhplH20Host*, OhplH20ListBase*);
void ohpl_h20_lb_on_init(const OhplH20Host*, OhplH20ListBase*);
void ohpl_h20_lb_add_item_to_selection(const OhplH20Host*, OhplH20ListBase*, uintptr_t item);
void ohpl_h20_lb_remove_item_from_selection(const OhplH20Host*, OhplH20ListBase*, uintptr_t item);
void ohpl_h20_lb_on_load_graphics(const OhplH20Host*, OhplH20ListBase*);
void ohpl_h20_lb_on_draw(const OhplH20Host*, OhplH20ListBase*);
void ohpl_h20_lb_on_draw_after_clip(const OhplH20Host*, OhplH20ListBase*, uintptr_t clip);
void ohpl_h20_lb_set_cursor_pos(const OhplH20Host*, OhplH20ListBase*, int pos, bool moveList, bool clamp);
bool ohpl_h20_lb_on_mouse_down(const OhplH20Host*, OhplH20ListBase*, const cGuiMessageData*);
bool ohpl_h20_lb_on_mouse_double_click(const OhplH20Host*, OhplH20ListBase*, const cGuiMessageData*);
bool ohpl_h20_lb_on_key_press(const OhplH20Host*, OhplH20ListBase*, const cGuiMessageData*);
bool ohpl_h20_lb_on_ui_arrow_press(const OhplH20Host*, OhplH20ListBase*, const cGuiMessageData*);
bool ohpl_h20_lb_on_ui_button_press(const OhplH20Host*, OhplH20ListBase*, const cGuiMessageData*);
bool ohpl_h20_lb_on_ui_button_release(const OhplH20Host*, OhplH20ListBase*);
bool ohpl_h20_lb_on_ui_button_double_press(const OhplH20Host*, OhplH20ListBase*, const cGuiMessageData*);
void ohpl_h20_lb_add_column(const OhplH20Host*, OhplH20ListBase*, const wchar_t* text, size_t len, int index, int align);
void ohpl_h20_lb_set_column_width(const OhplH20Host*, OhplH20ListBase*, int idx, float width);
void ohpl_h20_lb_item_start_pos(const OhplH20Host*, OhplH20ListBase*, cVector3f* out);

void ohpl_h20_cb_init(const OhplH20Host*, OhplH20Combo*, uintptr_t self, uintptr_t container, OhplH19Base* base);
void ohpl_h20_cb_destroy_children(const OhplH20Host*, OhplH20Combo*);
void ohpl_h20_cb_set_selected_item(const OhplH20Host*, OhplH20Combo*, int x, bool moveList, bool callback);
void ohpl_h20_cb_set_can_edit(const OhplH20Host*, OhplH20Combo*, bool v);
bool ohpl_h20_cb_can_edit(const OhplH20Host*, OhplH20Combo*);
void ohpl_h20_cb_set_max_shown_items(const OhplH20Host*, OhplH20Combo*, int v);
void ohpl_h20_cb_set_default_font_size(const OhplH20Host*, OhplH20Combo*, float x, float y);
void ohpl_h20_cb_update_properties(const OhplH20Host*, OhplH20Combo*);
void ohpl_h20_cb_on_init(const OhplH20Host*, OhplH20Combo*);
void ohpl_h20_cb_on_change_size(const OhplH20Host*, OhplH20Combo*);
void ohpl_h20_cb_on_change_text(const OhplH20Host*, OhplH20Combo*);
void ohpl_h20_cb_on_load_graphics(const OhplH20Host*, OhplH20Combo*);
void ohpl_h20_cb_on_draw(const OhplH20Host*, OhplH20Combo*);
bool ohpl_h20_cb_on_mouse_move(const OhplH20Host*, OhplH20Combo*, const cGuiMessageData*);
bool ohpl_h20_cb_on_mouse_down(const OhplH20Host*, OhplH20Combo*, const cGuiMessageData*);
bool ohpl_h20_cb_on_mouse_up(const OhplH20Host*, OhplH20Combo*, const cGuiMessageData*);
bool ohpl_h20_cb_on_ui_arrow_press(const OhplH20Host*, OhplH20Combo*, const cGuiMessageData*);
bool ohpl_h20_cb_on_ui_button_press(const OhplH20Host*, OhplH20Combo*, const cGuiMessageData*);
bool ohpl_h20_cb_on_ui_button_release(const OhplH20Host*, OhplH20Combo*, const cGuiMessageData*);
bool ohpl_h20_cb_on_lost_focus(const OhplH20Host*, OhplH20Combo*, const cGuiMessageData*);
bool ohpl_h20_cb_point_is_inside(const OhplH20Host*, OhplH20Combo*, float x, float y, bool onlyClipped);

}

}

#endif
