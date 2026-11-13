#ifndef HPL_RUST_GUI_H19_H
#define HPL_RUST_GUI_H19_H

// C ABI of rust/src/gui/h19_ffi.rs: iWidget base state, hierarchy, message dispatch and draw traversal.

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

typedef uintptr_t OhplH19W;
typedef void* OhplH19Ctx;
typedef bool (*OhplH19Callback)(void*, OhplH19W, const cGuiMessageData*);

struct OhplH19Host {
	OhplH19Ctx ctx;
	OhplH19Base* (*base)(OhplH19Ctx, OhplH19W);
	bool (*call)(OhplH19Ctx, OhplH19W, int, OhplH19W, const cGuiMessageData*, float);
	uintptr_t (*childBegin)(OhplH19Ctx, OhplH19W);
	bool (*childEnd)(OhplH19Ctx, OhplH19W, uintptr_t);
	OhplH19W (*childGet)(OhplH19Ctx, OhplH19W, uintptr_t);
	uintptr_t (*childNext)(OhplH19Ctx, OhplH19W, uintptr_t);
	void (*childErase)(OhplH19Ctx, OhplH19W, uintptr_t);
	void (*childPush)(OhplH19Ctx, OhplH19W, OhplH19W);
	void (*needsSorting)(OhplH19Ctx, OhplH19W);
	void (*virtualSize)(OhplH19Ctx, OhplH19W, float*);
	uintptr_t (*clipChild)(OhplH19Ctx, OhplH19W, uintptr_t, const float*, const float*);
	void (*setClip)(OhplH19Ctx, OhplH19W, uintptr_t);
};

extern "C" {

void ohpl_h19_base_init(OhplH19Base* b, bool withCallbacks);
void ohpl_h19_base_free(OhplH19Base* b);

bool ohpl_h19_process_message(const OhplH19Host* vt, OhplH19W w, int msg, const cGuiMessageData* d, bool skipVis, bool skipEnabled);
void ohpl_h19_add_callback(const OhplH19Host* vt, OhplH19W w, int msg, void* obj, OhplH19Callback f);
void ohpl_h19_update(const OhplH19Host* vt, OhplH19W w, float dt);
void ohpl_h19_draw(const OhplH19Host* vt, OhplH19W w, float dt, uintptr_t clip);
bool ohpl_h19_clips_graphics(const OhplH19Host* vt, OhplH19W w);
bool ohpl_h19_is_connected_to(const OhplH19Host* vt, OhplH19W w, OhplH19W other, bool start);
bool ohpl_h19_is_enabled(const OhplH19Host* vt, OhplH19W w);
bool ohpl_h19_is_visible(const OhplH19Host* vt, OhplH19W w);
void ohpl_h19_set_enabled(const OhplH19Host* vt, OhplH19W w, bool v);
void ohpl_h19_set_visible(const OhplH19Host* vt, OhplH19W w, bool v);
bool ohpl_h19_point_is_inside(const OhplH19Host* vt, OhplH19W w, float x, float y, bool onlyClipped);
void ohpl_h19_set_position_updated(const OhplH19Host* vt, OhplH19W w);
void ohpl_h19_refresh_global(const OhplH19Host* vt, OhplH19W w);
void ohpl_h19_set_position(const OhplH19Host* vt, OhplH19W w, const cVector3f* p);
void ohpl_h19_set_global_position(const OhplH19Host* vt, OhplH19W w, const cVector3f* p);
void ohpl_h19_set_children_offset(const OhplH19Host* vt, OhplH19W w, const cVector3f* p);
void ohpl_h19_set_scroll_amount(const OhplH19Host* vt, OhplH19W w, const cVector3f* p);
void ohpl_h19_set_affected_by_scroll(const OhplH19Host* vt, OhplH19W w, bool v);
void ohpl_h19_center_in_set(const OhplH19Host* vt, OhplH19W w);
void ohpl_h19_set_size(const OhplH19Host* vt, OhplH19W w, const cVector2f* p);
void ohpl_h19_text_changed(const OhplH19Host* vt, OhplH19W w);
void ohpl_h19_attach_child(const OhplH19Host* vt, OhplH19W w, OhplH19W child);
void ohpl_h19_remove_child(const OhplH19Host* vt, OhplH19W w, OhplH19W child);
void ohpl_h19_set_focus_navigation(const OhplH19Host* vt, OhplH19W w, int dir, OhplH19W to);
OhplH19W ohpl_h19_focus_navigation(const OhplH19Host* vt, OhplH19W w, int dir);
bool ohpl_h19_has_focus_navigation(const OhplH19Host* vt, OhplH19W w);
bool ohpl_h19_focus_message(const OhplH19Host* vt, OhplH19W w, int msg, int op, const cGuiMessageData* d);
void ohpl_h19_forward_to_parent(const OhplH19Host* vt, OhplH19W w, int op, OhplH19W child, const cGuiMessageData* d);

}

}

#endif
