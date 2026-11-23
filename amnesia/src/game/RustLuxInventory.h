#ifndef LUX_RUST_INVENTORY_H
#define LUX_RUST_INVENTORY_H

// C ABI of rust/src/luxui/invffi.rs and the host table of rust/src/luxui/invhost.rs (LuxInventoryHost.cpp).
// The inventory lives in Rust: cLuxInventory_Item* is an arena handle (0 = null), never dereferenced here.

#include <cstddef>
#include <cstdint>
#include <string>
#include "system/SystemTypes.h"

struct OhplA11aStr
{
	const unsigned char *p;
	size_t n;
};

struct OhplA11aWStr
{
	const unsigned int *p;
	size_t n;
};

struct OhplA11aArgs
{
	uintptr_t p[4];
	int32_t i[4];
	float f[12];
	double d;
	OhplA11aStr s[2];
	OhplA11aWStr w[2];
	uintptr_t r;
	OhplA11aStr rs;
	OhplA11aWStr rw;
};
static_assert(sizeof(OhplA11aArgs) == 208, "OhplA11aArgs layout");

typedef void (*OhplA11aSink)(void*, const unsigned char*, size_t);

inline OhplA11aStr OhplA11aS(const std::string &asX)
{
	OhplA11aStr s = { (const unsigned char*)asX.data(), asX.size() };
	return s;
}

inline void OhplA11aStrSink(void *apCtx, const unsigned char *apData, size_t alLen)
{
	((std::string*)apCtx)->assign((const char*)apData, alLen);
}

class cLuxInventory_Item;

extern "C" {

void ohpl_inv_set_host(void (*apOp)(int, OhplA11aArgs*));
void ohpl_inv_new();
void ohpl_inv_free();
void ohpl_inv_load_fonts();
void ohpl_inv_reset();
void ohpl_inv_on_game_start();
void ohpl_inv_update(float afDt);
void ohpl_inv_on_enter();
void ohpl_inv_on_leave();
void ohpl_inv_on_draw(float afFrameTime);
void ohpl_inv_exit_pressed();
void ohpl_inv_load_script();
void ohpl_inv_gamepad_changed();
uintptr_t ohpl_inv_add_item(OhplA11aStr name, int type, OhplA11aStr sub, OhplA11aStr image, float amount, OhplA11aStr val, OhplA11aStr extra, int *remove);
void ohpl_inv_remove_by_name(OhplA11aStr name);
void ohpl_inv_remove(uintptr_t item);
void ohpl_inv_remove_of_type(int type);
int ohpl_inv_item_index(uintptr_t item);
uintptr_t ohpl_inv_get_by_name(OhplA11aStr name);
uintptr_t ohpl_inv_get_by_subtype(OhplA11aStr sub);
int ohpl_inv_has_of_type(int type);
uintptr_t ohpl_inv_item_at(int idx);
int ohpl_inv_item_num();
void ohpl_inv_add_combine(OhplA11aStr name, OhplA11aStr a, OhplA11aStr b, OhplA11aStr func, int autoDestroy);
void ohpl_inv_remove_combine(OhplA11aStr name);
int ohpl_inv_combine_num();
int ohpl_inv_find_combine(OhplA11aStr a, OhplA11aStr b);
void ohpl_inv_combine_str(int idx, int which, OhplA11aSink sink, void *ctx);
int ohpl_inv_combine_auto(int idx);
int ohpl_inv_recompile(OhplA11aSink sink, void *ctx);
void ohpl_inv_open_journal();
void ohpl_inv_set_disabled(int v);
int ohpl_inv_disabled();
void ohpl_inv_set_message(const unsigned int *t, size_t n, float life);
int ohpl_inv_message_active();
uintptr_t ohpl_inv_picked();
void ohpl_inv_set_enter_from_journal(int v);
void ohpl_inv_set_equipped(uintptr_t item);
uintptr_t ohpl_inv_equipped();
int ohpl_inv_type_from_string(OhplA11aStr name);
uintptr_t ohpl_inv_get_set();
void ohpl_inv_item_str(uintptr_t item, int which, OhplA11aSink sink, void *ctx);
int ohpl_inv_item_type(uintptr_t item);
int ohpl_inv_item_count(uintptr_t item);
float ohpl_inv_item_amount(uintptr_t item);
uintptr_t ohpl_inv_item_image(uintptr_t item);
void ohpl_inv_item_set_count(uintptr_t item, int n);
void ohpl_inv_item_set_entries(uintptr_t item, OhplA11aStr name, OhplA11aStr desc);
int ohpl_inv_type_show_pickup(int type);
int ohpl_inv_type_has_count(int type);
int ohpl_inv_type_max_count(int type);
int ohpl_inv_type_has_max(int type);
void ohpl_inv_type_name(int type, OhplA11aSink sink, void *ctx);
int ohpl_inv_cb(uintptr_t token, uintptr_t widget, int lval, float fval);

void ohpl_jrn_new();
void ohpl_jrn_free();
void ohpl_jrn_load_fonts();
void ohpl_jrn_reset();
void ohpl_jrn_update(float dt);
void ohpl_jrn_on_enter();
void ohpl_jrn_on_leave();
void ohpl_jrn_on_draw(float frame_time);
uintptr_t ohpl_jrn_get_set();
void ohpl_jrn_exit_pressed(int instant);
void ohpl_jrn_exit();
void ohpl_jrn_set_force_instant_exit(int on);
void ohpl_jrn_change_state(int state);
void ohpl_jrn_open_last_read();
void ohpl_jrn_open_note_idx(int idx, int narration);
void ohpl_jrn_open_diary_idx(int cat, int idx, int narration);
void ohpl_jrn_add_note(OhplA11aStr entry, OhplA11aStr image);
int ohpl_jrn_add_diary(OhplA11aStr entry, OhplA11aStr image);
int ohpl_jrn_add_quest(OhplA11aStr name, OhplA11aStr entry);
int ohpl_jrn_disable_quest(OhplA11aStr name);
int ohpl_jrn_quest_state(OhplA11aStr name);
int ohpl_jrn_cb(uintptr_t token, uintptr_t widget, int lval, float fval);
void ohpl_jrn_last_read(int *cat, int *entry, int *type);
size_t ohpl_jrn_note_count();
void ohpl_jrn_note_field(size_t n, int field, OhplA11aSink sink, void *ctx);
size_t ohpl_jrn_cont_count();
void ohpl_jrn_cont_type(size_t n, OhplA11aSink sink, void *ctx);
size_t ohpl_jrn_diary_count(size_t cont);
void ohpl_jrn_diary_field(size_t cont, size_t n, int field, OhplA11aSink sink, void *ctx);
size_t ohpl_jrn_quest_count();
void ohpl_jrn_quest_field(size_t n, int field, OhplA11aSink sink, void *ctx);
int ohpl_jrn_quest_active(size_t n);
void ohpl_jrn_load_begin(int cat, int entry, int type);
void ohpl_jrn_load_note(OhplA11aStr name, OhplA11aStr text, OhplA11aStr icon, OhplA11aStr image);
void ohpl_jrn_load_container(OhplA11aStr type);
void ohpl_jrn_load_diary(OhplA11aStr name, OhplA11aStr text, OhplA11aStr icon, OhplA11aStr image);
void ohpl_jrn_load_quest(int active, OhplA11aStr name, OhplA11aStr nameEntry, OhplA11aStr textEntry);

}

inline std::string OhplInvStr(cLuxInventory_Item *apItem, int alWhich)
{
	std::string s;
	ohpl_inv_item_str((uintptr_t)apItem, alWhich, OhplA11aStrSink, &s);
	return s;
}

inline std::string OhplInvName(cLuxInventory_Item *p) { return OhplInvStr(p, 0); }
inline std::string OhplInvSubType(cLuxInventory_Item *p) { return OhplInvStr(p, 1); }
inline std::string OhplInvStringVal(cLuxInventory_Item *p) { return OhplInvStr(p, 2); }
inline std::string OhplInvExtraVal(cLuxInventory_Item *p) { return OhplInvStr(p, 3); }
inline std::string OhplInvImageName(cLuxInventory_Item *p) { return OhplInvStr(p, 4); }
inline std::string OhplInvNameEntry(cLuxInventory_Item *p) { return OhplInvStr(p, 5); }
inline std::string OhplInvDescEntry(cLuxInventory_Item *p) { return OhplInvStr(p, 6); }
inline int OhplInvType(cLuxInventory_Item *p) { return ohpl_inv_item_type((uintptr_t)p); }
inline int OhplInvCount(cLuxInventory_Item *p) { return ohpl_inv_item_count((uintptr_t)p); }
inline float OhplInvAmount(cLuxInventory_Item *p) { return ohpl_inv_item_amount((uintptr_t)p); }

#endif // LUX_RUST_INVENTORY_H
