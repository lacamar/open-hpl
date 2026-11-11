#ifndef HPL_RUST_WIDGET3_H
#define HPL_RUST_WIDGET3_H

// C ABI of rust/src/gui/ffi_widget3.rs: menu and popup logic.
// The widget and popup classes stay C++; Rust decides, C++ applies.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "math/RustTypes.h"
#include "graphics/Color.h"
#include "system/String.h"

namespace hpl {

enum { kOhplW3KindOther = 0, kOhplW3KindContext = 1, kOhplW3KindMain = 2 };
enum { kOhplW3ParentRoot = 0, kOhplW3ParentWindow = 1, kOhplW3ParentOther = 2 };
enum { kOhplW3OpGfx = 0, kOhplW3OpText = 1 };
enum { kOhplW3GfxLine = 0, kOhplW3GfxArrow = 1, kOhplW3GfxCheckedDisabled = 2, kOhplW3GfxCheckedEnabled = 3, kOhplW3GfxSelection = 4 };
enum { kOhplW3TextShortcut = 0, kOhplW3TextLabel = 1 };
enum { kOhplW3ColorDisabled = 0, kOhplW3ColorHighlight = 1, kOhplW3ColorDefault = 2 };
enum { kOhplW3MaxItemOps = 4 };
enum { kOhplW3DirNone = 0, kOhplW3DirClear = 1, kOhplW3DirPop = 2 };
enum { kOhplW3DirRoot = -1, kOhplW3DirPersonal = -2, kOhplW3DirGame = -3 };
enum { kOhplW3FpNone = 0, kOhplW3FpErrorEmpty = 1, kOhplW3FpSaveConfirm = 2, kOhplW3FpSaveDone = 3, kOhplW3FpLoadMissing = 4, kOhplW3FpLoadDone = 5 };
enum { kOhplW3KbDigit = 0, kOhplW3KbLetter = 1, kOhplW3KbEnter = 2, kOhplW3KbCancel = 3, kOhplW3KbSpace = 4 };
enum { kOhplW3KbNone = 0, kOhplW3KbCancelAct = 1, kOhplW3KbDelete = 2, kOhplW3KbClear = 3, kOhplW3KbShiftOn = 4 };
enum { kOhplW3KbKeys = 39 };

struct OhplW3MenuOp {
	uint32_t kind, slot, has_size, color;
	int32_t align;
	cVector3f pos;
	cVector2f size;
};
struct OhplW3ItemDrawIn {
	int32_t kind;
	bool separator, highlighted, enabled, widget_enabled, is_parent, checked;
	cVector3f global;
	cVector2f size;
	float separator_padding, separator_height;
	cVector3f arrow_offset, check_offset, text_offset;
};
struct OhplW3ItemUpdate { float timer; bool show_submenu; };
struct OhplW3ItemUp { bool ret, hide_topmost, press; };
struct OhplW3ItemLeave { bool ret, clear_highlight, reset_timer; };
struct OhplW3CtxMetrics {
	float h_padding, check_icon, text_left_padding, text_right_padding, arrow_icon, v_padding, left_border_w, right_border_w;
};
struct OhplW3CtxWidth {
	bool resize_items;
	float item_width;
	cVector2f menu_size;
	cVector3f arrow_pos, text_pos, check_pos;
};
struct OhplW3CtxHide { bool hide_child, notify_parent; };
struct OhplW3CtxUpdate { float timer; bool hide_timer; };
struct OhplW3MainItemSize { cVector2f size; cVector3f text_pos; };
struct OhplW3MainInit { cVector2f size; cVector3f children_offset, pos_delta; };

struct OhplW3PopUp {
	bool att_changed, focus_changed, default_focus_changed;
	size_t new_attention, new_focus, new_default_focus;
};
struct OhplW3MsgBoxLayout {
	cVector3f window_pos;
	cVector2f window_size;
	cVector3f button0_pos, button1_pos, label_pos;
	cVector2f label_size;
};
struct OhplW3ColorLayout {
	cVector3f button_pos[3];
	cVector2f button_size[3];
	cVector3f label_pos[4], slider_pos[4], input_pos[4];
	cVector3f current_frame_pos, previous_frame_pos;
};
struct OhplW3FpLayout {
	cVector3f dir_pos;
	cVector2f dir_size;
	cVector3f button_pos[3];
	cVector2f button_size[3];
	cVector3f name_pos;
	cVector2f name_size;
	cVector3f filter_pos;
	cVector2f filter_size;
	cVector3f list_pos;
	cVector2f list_size;
	int32_t column_width[4];
};
struct OhplW3DirSelect { int32_t action; size_t pops; };
struct OhplW3KbKey {
	int32_t kind, offset;
	cVector2f size;
	int32_t lower, upper;
	bool types_char;
};
struct OhplW3KbLayout { cVector2f window_size; size_t rows; };
struct OhplW3KbNav { int32_t up, right, down, left; };

extern "C" {

size_t ohpl_w3_item_draw(const OhplW3ItemDrawIn* in, OhplW3MenuOp* out, size_t cap);
void ohpl_w3_item_submenu_pos(int kind, const cVector3f* parentGlobal, const cVector3f* itemPos, float itemSizeY, float parentW, float childW, float virtualX, cVector3f* out);
void ohpl_w3_item_init_size(bool separator, float separatorHeight, float vPadding, float fontY, float menuItemWidth, cVector2f* out);
OhplW3ItemUpdate ohpl_w3_item_update(float timer, float dt, float openTime, bool mouseOver, int kind, bool highlightedIsSelf, bool isParent, bool menuOpen);
OhplW3ItemUp ohpl_w3_item_mouse_up(bool separator, bool highlightedIsSelf, bool isParent);
bool ohpl_w3_item_mouse_move_highlights(bool visible, bool drawMouse, bool attentionBlocks, bool mouseOver);
OhplW3ItemLeave ohpl_w3_item_mouse_leave(bool attentionBlocks, bool menuOpen);
size_t ohpl_w3_join_shortcut(const wchar_t* cur, size_t curLen, const wchar_t* add, size_t addLen, wchar_t* out, size_t cap);

float ohpl_w3_ctx_item_width(float sizeX, float left, float right);
void ohpl_w3_ctx_item_pos(float sizeY, float upH, float leftW, size_t numItems, cVector3f* out);
float ohpl_w3_ctx_separation(bool prevSep, bool newSep, float itemSep);
void ohpl_w3_ctx_empty_size(float sizeX, float upH, float downH, cVector2f* out);
void ohpl_w3_ctx_grown_size(const cVector2f* size, float itemH, float separation, cVector2f* out);
void ohpl_w3_ctx_update_width(const OhplW3CtxMetrics* met, float itemTextW, float currentItemWidth, float sizeY, OhplW3CtxWidth* out);
OhplW3CtxHide ohpl_w3_ctx_hide_plan(bool submenuOpen, bool hasParentMenu);
OhplW3CtxUpdate ohpl_w3_ctx_update(float timer, float dt, float hideTime, bool visible, bool parentItemNotHighlighted);
bool ohpl_w3_ctx_lost_focus_hides(bool focusNull, bool connectedToFocus, bool focusConnectedToMenu);
bool ohpl_w3_ctx_key_hides(int key);

void ohpl_w3_main_item_pos(float vPadding, bool hasPrev, float prevX, float prevW, float itemSeparation, cVector3f* out);
void ohpl_w3_main_item_size(float textLen, float textPadding, float fontY, float vPadding, OhplW3MainItemSize* out);
void ohpl_w3_main_init(int parentKind, float virtualX, float parentW, float fontY, OhplW3MainInit* out);

void ohpl_w3_popup_init(size_t attention, size_t focus, size_t defaultFocus, OhplW3PopUp* out);
void ohpl_w3_popup_set_next_focus(OhplW3PopUp* st, size_t widget);
void ohpl_w3_popup_after_callback(OhplW3PopUp* st, const size_t* pre, const size_t* post);
void ohpl_w3_popup_pos(const cVector2f* virtualSize, const cVector2f* size, float popupZ, cVector3f* out);

void ohpl_w3_msgbox_layout(float labelLen, float textLen, float fontY, const cVector2f* virtualSize, bool twoButtons, OhplW3MsgBoxLayout* out);
bool ohpl_w3_msgbox_gamepad(int val, bool hasButton1, bool* useSecond);

void ohpl_w3_color_layout(OhplW3ColorLayout* out);
int ohpl_w3_color_slider_value(float c);
float ohpl_w3_color_from_slider(int v);
int ohpl_w3_color_component(const bool* hits);
bool ohpl_w3_color_changed(const cColor* a, const cColor* b);

size_t ohpl_w3_fp_start_filename(const wchar_t* file, size_t fileLen, const wchar_t* path, size_t pathLen, wchar_t* out, size_t cap);
size_t ohpl_w3_fp_caption(int kind, wchar_t* out, size_t cap);
void ohpl_w3_fp_layout(float fontY, OhplW3FpLayout* out);
size_t ohpl_w3_fp_filter_text(const wchar_t* name, size_t nameLen, const wchar_t* const* filters, const size_t* lens, size_t count, wchar_t* out, size_t cap);
size_t ohpl_w3_fp_dir_entries(bool hasPersonal, bool hasGame, size_t pathLen, int32_t* out, size_t cap);
OhplW3DirSelect ohpl_w3_fp_dir_select(int selection, int numItems, size_t pathLen);
int ohpl_w3_fp_accept(int kind, bool nameEmpty, bool exists);
size_t ohpl_w3_fp_save_extension(const wchar_t* full, size_t fullLen, bool hasFilter, const wchar_t* filter, size_t filterLen, wchar_t* out, size_t cap, bool* append);
bool ohpl_w3_fp_overwrite_confirmed(int kind, int val);

size_t ohpl_w3_kb_keys(OhplW3KbKey* out, size_t cap);
void ohpl_w3_kb_layout(const cVector2f* sizes, size_t n, cVector3f* pos, int32_t* row, OhplW3KbLayout* out);
void ohpl_w3_kb_window_pos(const cVector3f* targetGlobal, const cVector2f* targetSize, float windowW, cVector3f* out);
void ohpl_w3_kb_nav(const size_t* rowLen, size_t rows, OhplW3KbNav* out, size_t cap);
int ohpl_w3_kb_ui_press(int val);
bool ohpl_w3_kb_ui_release(int val);

}

static_assert(sizeof(OhplW3MenuOp) == 40, "OhplW3MenuOp layout");
static_assert(sizeof(OhplW3ItemUpdate) == 8, "OhplW3ItemUpdate layout");
static_assert(sizeof(OhplW3ItemUp) == 3, "OhplW3ItemUp layout");
static_assert(sizeof(OhplW3ItemLeave) == 3, "OhplW3ItemLeave layout");
static_assert(sizeof(OhplW3CtxMetrics) == 32, "OhplW3CtxMetrics layout");
static_assert(sizeof(OhplW3CtxWidth) == 52, "OhplW3CtxWidth layout");
static_assert(sizeof(OhplW3CtxHide) == 2, "OhplW3CtxHide layout");
static_assert(sizeof(OhplW3CtxUpdate) == 8, "OhplW3CtxUpdate layout");
static_assert(sizeof(OhplW3MainItemSize) == 20, "OhplW3MainItemSize layout");
static_assert(sizeof(OhplW3MainInit) == 32, "OhplW3MainInit layout");
static_assert(sizeof(OhplW3PopUp) == 32, "OhplW3PopUp layout");
static_assert(sizeof(OhplW3MsgBoxLayout) == 64, "OhplW3MsgBoxLayout layout");
static_assert(sizeof(OhplW3ColorLayout) == 12 * 15 + 8 * 3 + 12 * 2, "OhplW3ColorLayout layout");
static_assert(sizeof(OhplW3FpLayout) == 156, "OhplW3FpLayout layout");
static_assert(sizeof(OhplW3DirSelect) == 16, "OhplW3DirSelect layout");
static_assert(sizeof(OhplW3KbKey) == 28, "OhplW3KbKey layout");
static_assert(sizeof(OhplW3KbLayout) == 16, "OhplW3KbLayout layout");
static_assert(sizeof(OhplW3KbNav) == 16, "OhplW3KbNav layout");
static_assert(sizeof(wchar_t) == 4, "tWString is handed to Rust as u32 code units");

}

#endif // HPL_RUST_WIDGET3_H
