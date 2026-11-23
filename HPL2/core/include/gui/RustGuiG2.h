#ifndef HPL_RUST_GUI_G2_H
#define HPL_RUST_GUI_G2_H

// C ABI of rust/src/gui/g2_popup.rs: the Rust-owned gui popups.
#include <cstddef>
#include <cstdint>

namespace hpl {

class iGuiPopUp;
class iWidget;
class cGuiSet;
class cGuiSkin;

extern "C" {

iGuiPopUp* ohpl_g2_popup_msgbox(cGuiSet*, cGuiSkin*, const uint32_t* label, size_t labelN, const uint32_t* text, size_t textN,
								const uint32_t* b1, size_t b1N, const uint32_t* b2, size_t b2N, uintptr_t cbObj, uintptr_t cbFn);
iGuiPopUp* ohpl_g2_popup_color(cGuiSet*, cGuiSkin*, uintptr_t dest, uintptr_t cbObj, uintptr_t cbFn);
iGuiPopUp* ohpl_g2_popup_filepicker(cGuiSet*, cGuiSkin*, int32_t kind, const uint32_t* path, size_t pathN, const uint32_t* name, size_t nameN,
									bool hidden, uintptr_t cbObj, uintptr_t cbFn);
iGuiPopUp* ohpl_g2_popup_kb_for(uintptr_t target);
void ohpl_g2_popup_add_destroy_cb(iGuiPopUp*, uintptr_t obj, uintptr_t fn);
void ohpl_g2_popup_set_kill_on_escape(iGuiPopUp*, bool);
bool ohpl_g2_popup_kill_on_escape(iGuiPopUp*);
void ohpl_g2_popup_set_next_focus(iGuiPopUp*, uintptr_t);
cGuiSet* ohpl_g2_popup_set(iGuiPopUp*);
void ohpl_g2_popup_fp_init(iGuiPopUp*);
void ohpl_g2_popup_fp_save_dest(iGuiPopUp*, void* dest, const uint32_t* name, size_t nameN);
void ohpl_g2_popup_fp_load_dest(iGuiPopUp*, void* dest);
int ohpl_g2_popup_fp_add_category(iGuiPopUp*, const uint32_t*, size_t, const uint32_t*, size_t);
void ohpl_g2_popup_fp_add_filter(iGuiPopUp*, int, const uint32_t*, size_t);

}

}

#endif
