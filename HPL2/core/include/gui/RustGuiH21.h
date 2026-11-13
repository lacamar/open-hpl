#ifndef HPL_RUST_GUI_H21_H
#define HPL_RUST_GUI_H21_H

// C ABI of rust/src/gui/h21_ffi.rs: the five GUI popups. Rust runs the popup logic and drives the
// C++ widgets through one command channel (OhplH21Cmd); callbacks come back as (tag, widget, data).

#include <cstddef>
#include <cstdint>
#include "gui/GuiTypes.h"

namespace hpl {

struct OhplH21Popup;

enum OhplH21Op {
	kOhplH21VirtualSize,
	kOhplH21PopupZ,
	kOhplH21IncPopupZ,
	kOhplH21PopupCount,
	kOhplH21GetAttention,
	kOhplH21GetFocused,
	kOhplH21GetDefaultFocus,
	kOhplH21SetAttention,
	kOhplH21SetFocused,
	kOhplH21SetDefaultFocus,
	kOhplH21PushAttention,
	kOhplH21PushFocused,
	kOhplH21PushDefaultFocus,
	kOhplH21PopAttention,
	kOhplH21PopFocused,
	kOhplH21PopDefaultFocus,
	kOhplH21DestroyWidget,
	kOhplH21CreateWindow,
	kOhplH21CreateButton,
	kOhplH21CreateLabel,
	kOhplH21CreateSlider,
	kOhplH21CreateTextbox,
	kOhplH21CreateFrame,
	kOhplH21CreateCombo,
	kOhplH21CreateList,
	kOhplH21MessageBox,
	kOhplH21DestroyPopup,
	kOhplH21AddResourceDir,
	kOhplH21RunCallback,
	kOhplH21CallDestroyCb,
	kOhplH21AddCallback,
	kOhplH21SetText,
	kOhplH21GetText,
	kOhplH21SetPosition,
	kOhplH21SetSize,
	kOhplH21SetGlobalPosition,
	kOhplH21GetGlobalPosition,
	kOhplH21GetSize,
	kOhplH21SetEnabled,
	kOhplH21SetVisible,
	kOhplH21SetFocusNav,
	kOhplH21SetGlobalUiListener,
	kOhplH21WinCloseDisables,
	kOhplH21WinEscCloses,
	kOhplH21WinGetEsc,
	kOhplH21WinStatic,
	kOhplH21WinDrawLabel,
	kOhplH21WinFontSize,
	kOhplH21InsideBounds,
	kOhplH21BtnUpImage,
	kOhplH21SliderGet,
	kOhplH21SliderSet,
	kOhplH21TbGetNum,
	kOhplH21TbSetNum,
	kOhplH21TbLower,
	kOhplH21TbUpper,
	kOhplH21TbDecimals,
	kOhplH21TbForceEnter,
	kOhplH21TbLostFocus,
	kOhplH21TbSelectAll,
	kOhplH21TbKbOpen,
	kOhplH21TbKbClose,
	kOhplH21TbUserData,
	kOhplH21TbKeySkip,
	kOhplH21TbKey,
	kOhplH21FrameColor,
	kOhplH21FrameDrawBg,
	kOhplH21FrameBgZ,
	kOhplH21CbCanEdit,
	kOhplH21CbItemNum,
	kOhplH21CbSelected,
	kOhplH21CbItemText,
	kOhplH21CbAddItem,
	kOhplH21CbClear,
	kOhplH21CbSetSelected,
	kOhplH21MlAddColumn,
	kOhplH21MlColWidth,
	kOhplH21MlSelected,
	kOhplH21MlProp1,
	kOhplH21MlClear,
	kOhplH21MlAddRow,
	kOhplH21FbNavUp,
	kOhplH21FbNavTo,
	kOhplH21FbFullPath,
	kOhplH21FbPathCount,
	kOhplH21FbPathPop,
	kOhplH21FbPathClear,
	kOhplH21FbPathEntry,
	kOhplH21FbDirs,
	kOhplH21FbCatCount,
	kOhplH21FbCatName,
	kOhplH21FbCatFilterCount,
	kOhplH21FbCatFilter,
	kOhplH21FbInit,
	kOhplH21FbRootCount,
	kOhplH21FbRoot,
	kOhplH21FbList,
	kOhplH21FbRow,
	kOhplH21FpLoadGfx,
	kOhplH21FileExists,
	kOhplH21FolderExists,
	kOhplH21SaveDest,
	kOhplH21LoadDest,
	kOhplH21TextLen,
	kOhplH21FontSize,
	kOhplH21ColorRead,
	kOhplH21ColorWrite,
	kOhplH21OpCount
};

struct OhplH21Cmd {
	int32_t op;
	int32_t i[3];
	uintptr_t w[2];
	float f[6];
	uintptr_t p[2];
	const uint32_t* s[4];
	size_t n[4];
};

struct OhplH21Ret {
	uintptr_t w;
	int32_t i;
	float f[4];
	const uint32_t* s[3];
	size_t n[3];
};

struct OhplH21Host {
	void* ctx;
	void (*exec)(void*, const OhplH21Cmd*, OhplH21Ret*);
};

static_assert(sizeof(OhplH21Cmd) == 136 && offsetof(OhplH21Cmd, w) == 16 && offsetof(OhplH21Cmd, f) == 32 &&
			  offsetof(OhplH21Cmd, p) == 56 && offsetof(OhplH21Cmd, s) == 72 && offsetof(OhplH21Cmd, n) == 104, "OhplH21Cmd ABI");
static_assert(sizeof(OhplH21Ret) == 80 && offsetof(OhplH21Ret, i) == 8 && offsetof(OhplH21Ret, f) == 12 &&
			  offsetof(OhplH21Ret, s) == 32 && offsetof(OhplH21Ret, n) == 56, "OhplH21Ret ABI");
static_assert(sizeof(wchar_t) == 4, "UTF-32 strings");

extern "C" {

OhplH21Popup* ohpl_h21_msgbox_new(const OhplH21Host* vt, const uint32_t* label, size_t labelN, const uint32_t* text, size_t textN,
								  const uint32_t* b1, size_t b1N, const uint32_t* b2, size_t b2N, uintptr_t cbObj, uintptr_t cbFn);
OhplH21Popup* ohpl_h21_color_new(const OhplH21Host* vt, uintptr_t dest, uintptr_t cbObj, uintptr_t cbFn);
OhplH21Popup* ohpl_h21_fp_new(const OhplH21Host* vt, int32_t kind, const uint32_t* path, size_t pathN, const uint32_t* name, size_t nameN,
							  uintptr_t cbObj, uintptr_t cbFn);
OhplH21Popup* ohpl_h21_kb_new(const OhplH21Host* vt, uintptr_t target);
bool ohpl_h21_event(const OhplH21Host* vt, const OhplH21Popup* p, int32_t tag, uintptr_t w, const cGuiMessageData* d);
void ohpl_h21_destroy(const OhplH21Host* vt, OhplH21Popup* p);
uintptr_t ohpl_h21_window(const OhplH21Popup* p);
void ohpl_h21_set_destroy_callback(const OhplH21Popup* p, uintptr_t obj, uintptr_t f);
void ohpl_h21_set_next_focus(const OhplH21Popup* p, uintptr_t w);
void ohpl_h21_set_kill_on_escape(const OhplH21Host* vt, const OhplH21Popup* p, bool x);
bool ohpl_h21_kill_on_escape(const OhplH21Host* vt, const OhplH21Popup* p);
void ohpl_h21_fp_init(const OhplH21Host* vt, const OhplH21Popup* p);
void ohpl_h21_fp_set_save_dest(const OhplH21Host* vt, const OhplH21Popup* p, const uint32_t* name, size_t nameN);
void ohpl_h21_fp_on_add_filter(const OhplH21Host* vt, const OhplH21Popup* p);
void ohpl_h21_fp_on_navigate(const OhplH21Host* vt, const OhplH21Popup* p);
void ohpl_h21_log_start();
bool ohpl_h21_log_write(const char* path);

}

}

#endif
