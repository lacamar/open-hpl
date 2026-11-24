#ifndef HPL_RUST_GUI_H19_H
#define HPL_RUST_GUI_H19_H

// Layout of the iWidget base state owned by rust/src/gui/gw.rs, and the virtual numbering of its foreign calls.

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"
#include "gui/GuiTypes.h"

namespace hpl {

class iWidget;
class cGuiClipRegion;

struct OhplH19Base {
	cVector3f pos;
	cVector3f global;
	cVector3f childrenOffset;
	cVector3f scroll;
	cVector2f size;
	iWidget* parent;
	iWidget* focusNav[4];
	void* callbacks;
	int positionCount;
	bool affectedByScroll, enabled, visible, mouseOver, clips, connectedToChildren, callbacksDisabled, positionUpdated, textChanged;
};

static_assert(sizeof(cVector3f) == 12 && sizeof(cVector2f) == 8, "vector layout");
static_assert(sizeof(OhplH19Base) == 120 && offsetof(OhplH19Base, size) == 48 && offsetof(OhplH19Base, parent) == 56 &&
			  offsetof(OhplH19Base, focusNav) == 64 && offsetof(OhplH19Base, callbacks) == 96 &&
			  offsetof(OhplH19Base, positionCount) == 104 && offsetof(OhplH19Base, affectedByScroll) == 108 &&
			  offsetof(OhplH19Base, textChanged) == 116, "OhplH19Base ABI");
static_assert(eGuiMessage_LastEnum == 32, "callback list count");
static_assert(offsetof(cGuiMessageData, mfVal) == 48 && offsetof(cGuiMessageData, mpData) == 56 &&
			  offsetof(cGuiMessageData, mMessage) == 64, "cGuiMessageData payload offsets");

enum OhplH19Op {
	kOhplH19OnMessage = 0,
	kOhplH19Virtual = 1,
	kOhplH19ChangePosition = 2,
	kOhplH19ChangeSize = 3,
	kOhplH19ChangeText = 4,
	kOhplH19ChildUpdate = 5,
	kOhplH19OnAttachChild = 6,
	kOhplH19RemoveChild = 7,
	kOhplH19ChildGotFocus = 8,
	kOhplH19ChildGotTabFocus = 9,
	kOhplH19ChildLostFocus = 10,
	kOhplH19Update = 11,
	kOhplH19Draw = 12,
	kOhplH19DrawAfterClip = 13,
	kOhplH19PointIsInside = 14,
	kOhplH19OnRemoveChild = 15
};


}

#endif
