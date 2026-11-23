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
