#ifndef HPL_RUST_GUI_GW_H
#define HPL_RUST_GUI_GW_H

// C ABI of the widget kinds in rust/src/gui/gw_kinds.rs (the first argument is the widget shell's address).

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"
#include "gui/GuiTypes.h"

namespace hpl {

class cGuiGfxElement;
class cGuiSet;

enum { kOhplGwButton = 1, kOhplGwCheck = 2, kOhplGwLabel = 3, kOhplGwImage = 4, kOhplGwDummy = 5, kOhplGwGroup = 6,
	   kOhplGwSlider = 7, kOhplGwFrame = 8, kOhplGwWindow = 9 };

extern "C" {
uintptr_t ohpl_gw_create(cGuiSet*, int kind, const cVector3f* pos, const cVector2f* size, const uint32_t* text, size_t textLen,
						 uintptr_t parent, const char* name, size_t nameLen, int a, int b, int c, cGuiGfxElement* img);

void ohpl_gw_button_set_image(uintptr_t, cGuiGfxElement*, bool);
cGuiGfxElement* ohpl_gw_button_image(uintptr_t);
void ohpl_gw_button_set_repeat_active(uintptr_t, bool);
bool ohpl_gw_button_repeat_active(uintptr_t);
void ohpl_gw_button_set_repeat_freq(uintptr_t, float);
void ohpl_gw_button_set_toggleable(uintptr_t, bool);
bool ohpl_gw_button_toggleable(uintptr_t);
bool ohpl_gw_button_pressed(uintptr_t);
void ohpl_gw_button_set_pressed(uintptr_t, bool, bool);

void ohpl_gw_check_set_checked(uintptr_t, bool, bool);
bool ohpl_gw_check_checked(uintptr_t);

void ohpl_gw_label_set_align(uintptr_t, int);
int ohpl_gw_label_align(uintptr_t);
void ohpl_gw_label_set_word_wrap(uintptr_t, bool);
bool ohpl_gw_label_word_wrap(uintptr_t);
void ohpl_gw_label_set_max_length(uintptr_t, int);
int ohpl_gw_label_max_length(uintptr_t);
void ohpl_gw_label_set_autogen(uintptr_t, bool);
bool ohpl_gw_label_autogen(uintptr_t);
void ohpl_gw_label_set_draw_bg(uintptr_t, bool);
bool ohpl_gw_label_draw_bg(uintptr_t);
void ohpl_gw_label_set_bg_color(uintptr_t, const cColor*);
const cColor* ohpl_gw_label_bg_color(uintptr_t);
void ohpl_gw_label_set_scroll_wait(uintptr_t, float);
float ohpl_gw_label_scroll_wait(uintptr_t);
void ohpl_gw_label_set_scroll_offset(uintptr_t, float);
void ohpl_gw_label_set_scroll_speed(uintptr_t, float);
float ohpl_gw_label_scroll_speed(uintptr_t);

void ohpl_gw_image_set(uintptr_t, cGuiGfxElement*);
cGuiGfxElement* ohpl_gw_image(uintptr_t);

void ohpl_gw_dummy_set_enclose(uintptr_t, bool);

const uint32_t* ohpl_gw_group_header(uintptr_t, size_t*);

int ohpl_gw_slider_value(uintptr_t);
void ohpl_gw_slider_set_value(uintptr_t, int, bool);
int ohpl_gw_slider_max(uintptr_t);
void ohpl_gw_slider_set_max(uintptr_t, int);
int ohpl_gw_slider_button_add(uintptr_t);
void ohpl_gw_slider_set_button_add(uintptr_t, int);
int ohpl_gw_slider_click_add(uintptr_t);
void ohpl_gw_slider_set_click_add(uintptr_t, int);
int ohpl_gw_slider_bar_size(uintptr_t);
void ohpl_gw_slider_set_bar_size(uintptr_t, int);
float ohpl_gw_slider_button_size(uintptr_t);

void ohpl_gw_frame_set_draw_frame(uintptr_t, bool);
bool ohpl_gw_frame_draw_frame(uintptr_t);
void ohpl_gw_frame_set_draw_bg(uintptr_t, bool);
bool ohpl_gw_frame_draw_bg(uintptr_t);
void ohpl_gw_frame_set_bg_z(uintptr_t, float);
float ohpl_gw_frame_bg_z(uintptr_t);
void ohpl_gw_frame_set_bg_color(uintptr_t, const cColor*);
const cColor* ohpl_gw_frame_bg_color(uintptr_t);
void ohpl_gw_frame_scroll_to(uintptr_t, float, float);

void ohpl_gw_window_set_static(uintptr_t, bool);
bool ohpl_gw_window_static(uintptr_t);
void ohpl_gw_window_set_draw_label(uintptr_t, bool);
void ohpl_gw_window_set_close_disables(uintptr_t, bool);
bool ohpl_gw_window_close_disables(uintptr_t);
void ohpl_gw_window_set_escape_closes(uintptr_t, bool);
bool ohpl_gw_window_escape_closes(uintptr_t);
}

}

#endif
