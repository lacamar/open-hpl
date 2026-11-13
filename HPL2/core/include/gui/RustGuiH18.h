#ifndef HPL_RUST_GUI_H18_H
#define HPL_RUST_GUI_H18_H

// C ABI of rust/src/gui/ffi_set.rs: cGuiSet focus, attention, tab order, windows and input dispatch.

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"
#include "gui/GuiTypes.h"

namespace hpl {

class iWidget;

struct OhplH18SetState {
	iWidget* focused = nullptr;
	iWidget* attention = nullptr;
	iWidget* defaultNav = nullptr;
	iWidget* tabWidget = nullptr;
	iWidget* tooltipWidget = nullptr;
	float tooltipTimer = 0;
	cVector2f mousePos;
	float windowZ = 0;
	bool mouseDown[9] = {};
	bool mouseMovement = true;
	void* lists = nullptr;
};

static_assert(sizeof(OhplH18SetState) == 80 && offsetof(OhplH18SetState, tooltipTimer) == 40 &&
			  offsetof(OhplH18SetState, mousePos) == 44 && offsetof(OhplH18SetState, windowZ) == 52 &&
			  offsetof(OhplH18SetState, mouseDown) == 56 && offsetof(OhplH18SetState, mouseMovement) == 65 &&
			  offsetof(OhplH18SetState, lists) == 72, "OhplH18SetState ABI");
static_assert(sizeof(cGuiMessageData) == 72 && offsetof(cGuiMessageData, mvPos) == 0 && offsetof(cGuiMessageData, mlVal) == 16 &&
			  offsetof(cGuiMessageData, mKeyPress) == 20 && offsetof(cKeyPress, mKey) == 0 && offsetof(cKeyPress, mlModifier) == 8,
			  "cGuiMessageData layout");
static_assert(sizeof(tWidgetListIt) == sizeof(uintptr_t), "list iterator is one word");
static_assert(eGuiMessage_MouseMove == 0 && eGuiMessage_MouseDown == 1 && eGuiMessage_MouseUp == 2 && eGuiMessage_MouseDoubleClick == 3 &&
			  eGuiMessage_MouseEnter == 4 && eGuiMessage_MouseLeave == 5 && eGuiMessage_GotFocus == 6 && eGuiMessage_LostFocus == 8 &&
			  eGuiMessage_KeyPress == 14 && eGuiMessage_KeyRelease == 15 && eGuiMessage_GamepadInput == 16 &&
			  eGuiMessage_UIArrowPress == 17 && eGuiMessage_UIArrowRelease == 18 && eGuiMessage_UIButtonPress == 19 &&
			  eGuiMessage_UIButtonRelease == 20 && eGuiMessage_UIButtonDoublePress == 21 && eGuiMessage_GetUINavFocus == 22 &&
			  eGuiMessage_LoseUINavFocus == 23, "eGuiMessage values");
static_assert(eKey_Tab == 1 && eKey_Up == 88 && eKey_Down == 89 && eKey_Right == 90 && eKey_Left == 91, "eKey values");
static_assert(eUIArrow_Up == 1 && eUIArrow_Right == 2 && eUIArrow_Down == 4 && eUIArrow_Left == 8 && eUIArrow_LastEnum == 0,
			  "eUIArrow values");
static_assert(eKeyModifier_None == 0 && eKeyModifier_Shift == 2, "eKeyModifier values");

typedef uintptr_t OhplH18W;
typedef void* OhplH18Ctx;

struct OhplH18Host {
	OhplH18Ctx ctx;
	bool (*isVisible)(OhplH18Ctx, OhplH18W);
	bool (*isEnabled)(OhplH18Ctx, OhplH18W);
	bool (*isConnectedTo)(OhplH18Ctx, OhplH18W, OhplH18W);
	bool (*pointIsInside)(OhplH18Ctx, OhplH18W, float, float);
	bool (*mouseIsOver)(OhplH18Ctx, OhplH18W);
	void (*setMouseIsOver)(OhplH18Ctx, OhplH18W, bool);
	bool (*processMessage)(OhplH18Ctx, OhplH18W, int, const cGuiMessageData*, bool, bool);
	bool (*getFocus)(OhplH18Ctx, OhplH18W, const cGuiMessageData*);
	bool (*tooltipEnabled)(OhplH18Ctx, OhplH18W);
	bool (*hasFocusNav)(OhplH18Ctx, OhplH18W);
	OhplH18W (*parent)(OhplH18Ctx, OhplH18W);
	uintptr_t (*widgetSet)(OhplH18Ctx, OhplH18W);
	uintptr_t (*pointerGfx)(OhplH18Ctx, OhplH18W);
	OhplH18W (*focusNav)(OhplH18Ctx, OhplH18W, int);
	bool (*globalKeyListener)(OhplH18Ctx, OhplH18W);
	bool (*globalUIListener)(OhplH18Ctx, OhplH18W);
	bool (*gotTabFocus)(OhplH18Ctx, OhplH18W, const cGuiMessageData*);
	bool (*isValid)(OhplH18Ctx, OhplH18W);
	uintptr_t (*listBegin)(OhplH18Ctx);
	bool (*listEnd)(OhplH18Ctx, uintptr_t);
	OhplH18W (*listGet)(OhplH18Ctx, uintptr_t);
	uintptr_t (*listNext)(OhplH18Ctx, uintptr_t);
	void (*sortWidgets)(OhplH18Ctx);
	uintptr_t (*currentPointer)(OhplH18Ctx);
	void (*setCurrentPointer)(OhplH18Ctx, uintptr_t);
	uintptr_t (*skinNormalPointer)(OhplH18Ctx);
	bool (*execShortcut)(OhplH18Ctx, const cGuiMessageData*);
	void (*globalPos)(OhplH18Ctx, OhplH18W, float*);
	void (*setGlobalPos)(OhplH18Ctx, OhplH18W, const float*);
};

extern "C" {

void* ohpl_h18_lists_new();
void ohpl_h18_lists_free(void* lists);

void ohpl_h18_set_focused(OhplH18SetState* s, const OhplH18Host* vt, OhplH18W w, bool check);
void ohpl_h18_push_focused(OhplH18SetState* s);
void ohpl_h18_pop_focused(OhplH18SetState* s, const OhplH18Host* vt);
void ohpl_h18_set_attention(OhplH18SetState* s, const OhplH18Host* vt, OhplH18W w, bool clear, bool check);
void ohpl_h18_push_attention(OhplH18SetState* s);
void ohpl_h18_pop_attention(OhplH18SetState* s, const OhplH18Host* vt, bool clear);
void ohpl_h18_set_default_nav(OhplH18SetState* s, const OhplH18Host* vt, OhplH18W w, bool check);
void ohpl_h18_push_default_nav(OhplH18SetState* s);
void ohpl_h18_pop_default_nav(OhplH18SetState* s, const OhplH18Host* vt);
void ohpl_h18_add_to_tab_order(OhplH18SetState* s, OhplH18W w);
void ohpl_h18_clear_tab_order(OhplH18SetState* s);
void ohpl_h18_add_window(OhplH18SetState* s, OhplH18W w);
void ohpl_h18_remove_window(OhplH18SetState* s, OhplH18W w);
void ohpl_h18_set_window_on_top(OhplH18SetState* s, const OhplH18Host* vt, OhplH18W w);
void ohpl_h18_destroy_prelude(OhplH18SetState* s, OhplH18W w);
bool ohpl_h18_send_message(OhplH18SetState* s, const OhplH18Host* vt, int msg, const cGuiMessageData* data);

}
}

#endif
