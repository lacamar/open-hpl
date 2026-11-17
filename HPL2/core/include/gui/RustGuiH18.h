#ifndef HPL_RUST_GUI_H18_H
#define HPL_RUST_GUI_H18_H

// Layout of SetState in rust/src/gui/set.rs: the first member of the Rust-owned cGuiSet.

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

}

#endif
