#ifndef HPL_RUST_GUI_H20_H
#define HPL_RUST_GUI_H20_H

// C ABI of rust/src/gui/h20_*.rs: text box, list boxes, combo box, tab frame and menus (OHPL_IMPL=rust).
// Widget state structs are embedded in the C++ widgets and mutated only by Rust.

#include <cstddef>
#include <cstdint>
#include "gui/RustGuiH19.h"
#include "gui/RustWidget2.h"

namespace hpl {

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
};
static_assert(sizeof(OhplH20Combo) == 176 && offsetof(OhplH20Combo, text) == 40 && offsetof(OhplH20Combo, gfx) == 64 &&
			  offsetof(OhplH20Combo, buttonW) == 144 && offsetof(OhplH20Combo, self) == 152,
			  "OhplH20Combo ABI");

class cWidgetTab;
class cWidgetTabLabel;
class cWidgetFrame;
class cWidgetTabFrame;
class cGuiGfxElement;

struct OhplH20TabFrame {
	OhplW2TabState state;
	cWidgetTab* top;
	uintptr_t arrows[2];
	void* tabs;
	uintptr_t gfx[21];
	float bgZ, fgZ;
	bool hori, vert;
	uintptr_t self;
	OhplH19Base* base;
};
struct OhplH20Tab {
	int32_t index;
	bool onTop;
	cWidgetTabFrame* frame;
	OhplH20TabFrame* fs;
	cWidgetTabLabel* label;
	cWidgetFrame* inner;
	void* ls;
	uintptr_t self;
	OhplH19Base* base;
};
struct OhplH20TabLabel {
	void* tab;
	cGuiGfxElement* icon;
	uintptr_t self;
	OhplH19Base* base;
};
static_assert(sizeof(OhplW2TabState) == 40 && sizeof(OhplH20TabFrame) == 272 && offsetof(OhplH20TabFrame, tabs) == 64 &&
			  offsetof(OhplH20TabFrame, gfx) == 72 && offsetof(OhplH20TabFrame, bgZ) == 240 && offsetof(OhplH20TabFrame, self) == 256 &&
			  sizeof(OhplH20Tab) == 64 && offsetof(OhplH20Tab, label) == 24 &&
			  sizeof(OhplH20TabLabel) == 32, "tab state ABI");

struct OhplH20TextBox {
	OhplW2Text st;
	uintptr_t buttons[2];
	void* illegal;
	void* replay;
	iWidget* prevAttention;
	uintptr_t gfx[11];
	bool numeric, showButtons, replaying;
	uintptr_t self;
	OhplH19Base* base;
};
static_assert(sizeof(OhplW2Text) == 80 && sizeof(OhplH20TextBox) == 232 && offsetof(OhplH20TextBox, buttons) == 80 &&
			  offsetof(OhplH20TextBox, gfx) == 120 && offsetof(OhplH20TextBox, numeric) == 208 &&
			  offsetof(OhplH20TextBox, self) == 216, "OhplH20TextBox ABI");

class iWidgetMenu;

struct OhplH20MenuItem {
	iWidgetMenu* child;
	uintptr_t gfx[5];
	float timer, openTime, separatorPadding, separatorHeight, vPadding;
	bool checked, def;
	int32_t kind;
	uintptr_t self;
	OhplH19Base* base;
};
struct OhplH20CtxMenu {
	float timer, hideTime, hPadding, vPadding, separation, arrowIcon, checkIcon, textLeft, textRight;
	uintptr_t gfx[9];
	uintptr_t self;
	OhplH19Base* base;
};
struct OhplH20MainMenu {
	float textPadding, vPadding, separation;
	uintptr_t gfxBg;
	uintptr_t self;
	OhplH19Base* base;
};
static_assert(sizeof(OhplH20MenuItem) == 96 && offsetof(OhplH20MenuItem, timer) == 48 && offsetof(OhplH20MenuItem, checked) == 68 &&
			  offsetof(OhplH20MenuItem, self) == 80 && sizeof(OhplH20CtxMenu) == 128 && offsetof(OhplH20CtxMenu, gfx) == 40 &&
			  sizeof(OhplH20MainMenu) == 40 && offsetof(OhplH20MainMenu, gfxBg) == 16, "menu state ABI");

extern "C" {

void ohpl_h20_lb_set_selected_item(const OhplH20ListBase*, int x, bool moveList, bool callback, int type, bool clearPrevious);
int ohpl_h20_lb_selected_item(const OhplH20ListBase*);
int ohpl_h20_lb_multi_selection(const OhplH20ListBase*, int idx);
int ohpl_h20_lb_multi_selection_num(const OhplH20ListBase*);
void ohpl_h20_lb_clear_selection(const OhplH20ListBase*);
void ohpl_h20_lb_set_cursor_pos(const OhplH20ListBase*, int pos, bool moveList, bool clamp);
void ohpl_h20_lb_add_column(const OhplH20ListBase*, const wchar_t* text, size_t len, int index, int align);
void ohpl_h20_lb_set_column_width(const OhplH20ListBase*, int idx, float width);
void ohpl_h20_lb_item_start_pos(const OhplH20ListBase*, cVector3f* out);

void ohpl_h20_cb_set_selected_item(const OhplH20Combo*, int x, bool moveList, bool callback);
void ohpl_h20_cb_set_can_edit(const OhplH20Combo*, bool v);
bool ohpl_h20_cb_can_edit(const OhplH20Combo*);
void ohpl_h20_cb_set_max_shown_items(const OhplH20Combo*, int v);

uintptr_t ohpl_h20_tf_add_tab(const OhplH20TabFrame*, const wchar_t* text, size_t len);
void ohpl_h20_tf_clear_tabs(const OhplH20TabFrame*);
int ohpl_h20_tf_tab_num(const OhplH20TabFrame*);
uintptr_t ohpl_h20_tf_tab(const OhplH20TabFrame*, int idx);
int ohpl_h20_tf_tab_on_top_index(const OhplH20TabFrame*);
void ohpl_h20_tf_set_tab_on_top(const OhplH20TabFrame*, uintptr_t tab);
void ohpl_h20_tf_set_tab_on_top_by_index(const OhplH20TabFrame*, int idx);
void ohpl_h20_tf_set_hori_scroll(const OhplH20TabFrame*, bool v);
void ohpl_h20_tf_set_vert_scroll(const OhplH20TabFrame*, bool v);

void ohpl_h20_tab_set_icon(const OhplH20Tab*, uintptr_t icon);
void ohpl_h20_tab_set_on_top(const OhplH20Tab*, bool v);

void ohpl_h20_tb_set_default_decimals(int v);
void ohpl_h20_tb_set_illegal_chars(const OhplH20TextBox*, const wchar_t* text, size_t len);
void ohpl_h20_tb_set_max_text_length(const OhplH20TextBox*, int len);
void ohpl_h20_tb_set_selected_text(const OhplH20TextBox*, int start, int count);
void ohpl_h20_tb_set_numeric_value(const OhplH20TextBox*, float v);
float ohpl_h20_tb_numeric_value(const OhplH20TextBox*);
void ohpl_h20_tb_background_size(const OhplH20TextBox*, cVector2f* out);
void ohpl_h20_tb_set_can_edit(const OhplH20TextBox*, bool v);
void ohpl_h20_tb_on_change_size(const OhplH20TextBox*);

}

}

#endif
