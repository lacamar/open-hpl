/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef HPL_RUST_GUI_G1_H
#define HPL_RUST_GUI_G1_H

// C ABI of rust/src/gui/g1_*.rs: Rust-owned gui objects.
#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"
#include "graphics/GraphicsTypes.h"
#include "gui/GuiTypes.h"
#include "gui/RustGuiH18.h"
#include "gui/RustGui.h"

namespace hpl {

class cGuiGfxElement;
class cGuiGfxAnimation;
class iGuiMaterial;
class cGuiSkin;
class cGuiSet;
class cGuiClipRegion;
class cGuiGlobalShortcut;
class cGuiSkinFont;
class cGui;
class iFontData;

struct OhplG1Gui;
struct OhplG1TexInfo { float width; float height; bool rect; };
struct OhplG1ImageInfo { uintptr_t texture; float tex[4][3]; int width; int height; int updateCount; };

struct OhplG1Engine {
	void (*texInfo)(uintptr_t, OhplG1TexInfo*);
	void (*imageInfo)(uintptr_t, OhplG1ImageInfo*);
	void (*imageFlush)(uintptr_t);
	void (*imageDestroy)(void*, uintptr_t);
	void (*textureDestroy)(void*, uintptr_t);
};


struct OhplG1Host {
	void (*log)(int, const char*, size_t);
	uintptr_t (*materialNew)(int, void*);
	void (*materialDelete)(uintptr_t);
	uintptr_t (*imageCreate)(void*, const char*, size_t);
	uintptr_t (*textureCreate2D)(void*, const char*, size_t, bool, int);
	bool (*textureRenderTarget)(uintptr_t);
	bool (*fileFound)(void*, const char*, size_t);
	uintptr_t (*fontCreate)(void*, const char*, size_t);
	void (*findFile)(void*, const char*, size_t, void (*)(void*, const uint32_t*, size_t), void*);
	int (*readFile)(const uint32_t*, size_t, void (*)(void*, const char*, size_t), void*);
	void (*screenSize)(void*, float*);
};

// Plain fields of the Rust-owned cGuiSet (rust/src/gui/g1_set.rs Pub), read by inline accessors.
struct OhplG1SetPub {
	OhplH18SetState state;
	cVector2f virtualSize;
	cVector2f virtualOffset;
	cVector3f drawOffset;
	cVector3f size3D;
	float mtx3D[16];
	float mouseZ;
	int drawPrio;
	uintptr_t programs[2];
	bool active, drawMouse, is3D, rendersBeforePost, cullBackface, flipY, drawFocus, destroying, sortWidgets;
};
static_assert(offsetof(OhplG1SetPub, virtualSize) == 80 && offsetof(OhplG1SetPub, drawOffset) == 96 && offsetof(OhplG1SetPub, size3D) == 108 &&
			  offsetof(OhplG1SetPub, mtx3D) == 120 && offsetof(OhplG1SetPub, mouseZ) == 184 && offsetof(OhplG1SetPub, programs) == 192 &&
			  offsetof(OhplG1SetPub, active) == 208 && offsetof(OhplG1SetPub, sortWidgets) == 216, "OhplG1SetPub layout");


enum { kOhplG1MaterialDiffuse = 0, kOhplG1MaterialAlpha, kOhplG1MaterialFontNormal, kOhplG1MaterialAdditive,
	   kOhplG1MaterialModulative, kOhplG1MaterialPremulAlpha };
static_assert(eGuiMaterial_Diffuse == 0 && eGuiMaterial_PremulAlpha == 5 && eGuiMaterial_LastEnum == 6, "gui material order");
static_assert(eGuiMessage_MouseDown == 1 && eGuiMessage_MouseUp == 2 && eGuiMessage_MouseDoubleClick == 3 &&
			  eGuiMessage_KeyPress == 14 && eGuiMessage_KeyRelease == 15 && eGuiMessage_UIArrowPress == 17 &&
			  eGuiMessage_UIArrowRelease == 18 && eGuiMessage_UIButtonPress == 19 && eGuiMessage_UIButtonRelease == 20 &&
			  eGuiMessage_UIButtonDoublePress == 21, "eGuiMessage values");
static_assert(sizeof(cKeyPress) == 12 && sizeof(cGamepadInputData) == 16, "input data layout");
static_assert(eTextureType_2D == 1 && eTextureType_Rect == 2, "texture types");

enum { kOhplSkinFont = 0, kOhplSkinAttribute = 1, kOhplSkinGfx = 2 };

extern "C" {
void ohpl_g1_set_engine(OhplG1Engine* engine);
void ohpl_g1_set_host(OhplG1Host* host);


cGuiGfxElement* ohpl_g1_gfx_new(void* resources);
void ohpl_g1_gfx_free(cGuiGfxElement*);
void ohpl_g1_gfx_update(cGuiGfxElement*, float);
void ohpl_g1_gfx_add_image(cGuiGfxElement*, uintptr_t);
void ohpl_g1_gfx_add_texture(cGuiGfxElement*, uintptr_t, const cVector2f*, const cVector2f*);
void ohpl_g1_gfx_add_image_to_buffer(cGuiGfxElement*, uintptr_t);
int ohpl_g1_gfx_texture_num(cGuiGfxElement*);
void ohpl_g1_gfx_set_destroy_texture(cGuiGfxElement*, bool);
bool ohpl_g1_gfx_destroy_texture(cGuiGfxElement*);
const void* ohpl_g1_gfx_texture(cGuiGfxElement*, int);
uintptr_t ohpl_g1_gfx_image(cGuiGfxElement*, int);
void ohpl_g1_gfx_set_offset(cGuiGfxElement*, const cVector3f*);
const cVector3f* ohpl_g1_gfx_offset(cGuiGfxElement*);
void ohpl_g1_gfx_set_active_size(cGuiGfxElement*, const cVector2f*);
const cVector2f* ohpl_g1_gfx_active_size(cGuiGfxElement*);
void ohpl_g1_gfx_image_size(cGuiGfxElement*, cVector2f*);
cGuiGfxAnimation* ohpl_g1_gfx_create_animation(cGuiGfxElement*);
void ohpl_g1_gfx_play_animation(cGuiGfxElement*, int);
cGuiGfxAnimation* ohpl_g1_gfx_animation(cGuiGfxElement*, int);
int ohpl_g1_gfx_animation_num(cGuiGfxElement*);
void ohpl_g1_gfx_set_animation_time(cGuiGfxElement*, float);
void ohpl_g1_gfx_set_paused(cGuiGfxElement*, bool);
bool ohpl_g1_gfx_paused(cGuiGfxElement*);
void ohpl_g1_gfx_set_material(cGuiGfxElement*, const iGuiMaterial*);
void ohpl_g1_gfx_set_color(cGuiGfxElement*, const cColor*);
void ohpl_g1_gfx_set_flip(cGuiGfxElement*, bool);
bool ohpl_g1_gfx_flip(cGuiGfxElement*);
void ohpl_g1_gfx_flush(cGuiGfxElement*);
void ohpl_g1_gfx_info(cGuiGfxElement*, OhplGuiGfxInfo*);
void ohpl_g1_anim_add_frame(cGuiGfxAnimation*, int);
void ohpl_g1_anim_set_type(cGuiGfxAnimation*, int);
void ohpl_g1_anim_set_frame_length(cGuiGfxAnimation*, float);


OhplG1Gui* ohpl_g1_gui_new(void* shell);
void ohpl_g1_gui_free(OhplG1Gui*);
void ohpl_g1_gui_init(OhplG1Gui*, void*, void*, void*, void*, void*);
cGuiGfxElement* ohpl_g1_gfx_rect();
void ohpl_g1_gui_update(OhplG1Gui*, float);
void ohpl_g1_gui_on_draw(OhplG1Gui*, float);
void ohpl_g1_gui_on_post_buffer_swap(OhplG1Gui*);
const void* ohpl_g1_gui_material(OhplG1Gui*, int);
cGuiSkin* ohpl_g1_gui_create_skin(OhplG1Gui*, const char*, size_t);
uint32_t ohpl_g1_gui_skin_from_name(OhplG1Gui*, int, const char*, size_t);
uintptr_t ohpl_g1_gui_create_set(OhplG1Gui*, const char*, size_t, cGuiSkin*);
uintptr_t ohpl_g1_gui_set_from_name(OhplG1Gui*, const char*, size_t);
void ohpl_g1_gui_set_focus(OhplG1Gui*, uintptr_t);
void ohpl_g1_gui_set_focus_by_name(OhplG1Gui*, const char*, size_t);
uintptr_t ohpl_g1_gui_focused_set(OhplG1Gui*);
void ohpl_g1_gui_destroy_set(OhplG1Gui*, uintptr_t);
void ohpl_g1_gui_destroy_gfx(OhplG1Gui*, cGuiGfxElement*);
cGuiGfxElement* ohpl_g1_gui_create_gfx_filled_rect(OhplG1Gui*, const cColor*, int, bool);
cGuiGfxElement* ohpl_g1_gui_create_gfx_image(OhplG1Gui*, const char*, size_t, int, const cColor*, bool);
cGuiGfxElement* ohpl_g1_gui_create_gfx_image_buffer(OhplG1Gui*, const char*, size_t, int, bool, const cColor*, bool);
cGuiGfxElement* ohpl_g1_gui_create_gfx_texture_file(OhplG1Gui*, const char*, size_t, int, int, const cColor*, bool, bool);
cGuiGfxElement* ohpl_g1_gui_create_gfx_texture(OhplG1Gui*, uintptr_t, bool, int, const cColor*, bool, const cVector2f*, const cVector2f*);
bool ohpl_g1_gui_send_mouse_pos(OhplG1Gui*, int, int, int, int);
bool ohpl_g1_gui_send_mouse_button(OhplG1Gui*, int, int, int);
bool ohpl_g1_gui_send_key(OhplG1Gui*, int, const cKeyPress*);
bool ohpl_g1_gui_send_gamepad(OhplG1Gui*, const cGamepadInputData*);
bool ohpl_g1_gui_send_value(OhplG1Gui*, int, int);
void* ohpl_g1_gui_resources(OhplG1Gui*);
void* ohpl_g1_gui_input(OhplG1Gui*);
void ohpl_gui_skin_name(int, uint32_t, void (*)(void*, const char*, size_t), void*);
int ohpl_gui_mouse_to_mouse(int);
int ohpl_gui_mouse_to_gui(int);


void ohpl_g1_set_update(cGuiSet*, float);
void ohpl_g1_set_draw_all(cGuiSet*, float);
bool ohpl_g1_set_send_message(cGuiSet*, int, const cGuiMessageData*);
void ohpl_g1_set_clear_render_objects(cGuiSet*);
void ohpl_g1_set_draw_gfx(cGuiSet*, cGuiGfxElement*, const cVector3f*, const cVector2f*, const cColor*, int, float, const cVector3f*);
void ohpl_g1_set_draw_text(cGuiSet*, const OhplGuiGlyphTable*, const uint32_t*, size_t, const cVector2f*, const cVector3f*, const cColor*, int, int);
void ohpl_g1_set_add_widget(cGuiSet*, uintptr_t, uintptr_t);
uintptr_t ohpl_g1_set_widget_from_name(cGuiSet*, const char*, size_t);
void ohpl_g1_set_destroy_widget(cGuiSet*, uintptr_t, bool);
void ohpl_g1_set_destroy_all_widgets(cGuiSet*);
bool ohpl_g1_set_is_valid_widget(cGuiSet*, uintptr_t);
void ohpl_g1_set_reset_mouse_over(cGuiSet*);
void ohpl_g1_set_destroy_popup(cGuiSet*, uintptr_t);
bool ohpl_g1_set_popup_is_active(cGuiSet*);
void ohpl_g1_set_add_popup_count(cGuiSet*, int);
float ohpl_g1_set_popup_z(cGuiSet*);
void ohpl_g1_set_inc_popup_z(cGuiSet*, int);
float ohpl_g1_set_context_menu_z(cGuiSet*);
void ohpl_g1_set_add_context_menu_z(cGuiSet*, float);
void* ohpl_g1_set_gui(cGuiSet*);
void* ohpl_g1_set_resources(cGuiSet*);
void* ohpl_g1_set_graphics(cGuiSet*);
void* ohpl_g1_set_sound(cGuiSet*);
void* ohpl_g1_set_scene(cGuiSet*);
cGuiSkin* ohpl_g1_set_skin(cGuiSet*);
uintptr_t ohpl_g1_set_root(cGuiSet*);
void ohpl_g1_set_root_clips_set(cGuiSet*, bool);
bool ohpl_g1_set_root_clips(cGuiSet*);
bool ohpl_g1_set_has_focus(cGuiSet*);
void ohpl_g1_set_position_inside_bounds(cGuiSet*, uintptr_t);
uintptr_t ohpl_gw_create_image(cGuiSet*, const char*, size_t, const cVector3f*, const cVector2f*, int, bool, uintptr_t, const char*, size_t);
void* ohpl_g1_set_list(cGuiSet*);
cGuiGfxElement* ohpl_g1_set_pointer(cGuiSet*);
void ohpl_g1_set_set_pointer(cGuiSet*, cGuiGfxElement*);
void ohpl_g1_set_set_skin(cGuiSet*, cGuiSkin*);
float ohpl_g1_set_virtual_min_z(cGuiSet*);
float ohpl_g1_set_virtual_max_z(cGuiSet*);
cGuiClipRegion* ohpl_g1_set_base_clip(cGuiSet*);
cGuiClipRegion* ohpl_g1_set_current_clip(cGuiSet*);
void ohpl_g1_set_set_current_clip(cGuiSet*, cGuiClipRegion*);
void ohpl_g1_set_set_focus_draw_callback(cGuiSet*, void*, tGuiCallbackFunc);
const char* ohpl_g1_set_name(cGuiSet*, size_t*);
void ohpl_g1_set_set_virtual_size(cGuiSet*, const cVector2f*, float, float, const cVector2f*, bool);
void ohpl_g1_set_set_global_scale(float);
float ohpl_g1_set_global_scale();
void ohpl_g1_set_set_3d_transform(cGuiSet*, const float*);
uintptr_t ohpl_g1_set_tip_frame(cGuiSet*);
void ohpl_g1_set_set_focused(cGuiSet*, uintptr_t, bool);
void ohpl_g1_set_push_focused(cGuiSet*);
void ohpl_g1_set_pop_focused(cGuiSet*);
void ohpl_g1_set_set_attention(cGuiSet*, uintptr_t, bool, bool);
void ohpl_g1_set_push_attention(cGuiSet*);
void ohpl_g1_set_pop_attention(cGuiSet*, bool);
void ohpl_g1_set_set_default_nav(cGuiSet*, uintptr_t, bool);
void ohpl_g1_set_push_default_nav(cGuiSet*);
void ohpl_g1_set_pop_default_nav(cGuiSet*);
void ohpl_g1_set_add_to_tab_order(cGuiSet*, uintptr_t);
void ohpl_g1_set_clear_tab_order(cGuiSet*);
void ohpl_g1_set_add_window(cGuiSet*, uintptr_t);
void ohpl_g1_set_remove_window(cGuiSet*, uintptr_t);
void ohpl_g1_set_set_window_on_top(cGuiSet*, uintptr_t);
void ohpl_g1_set_set_last_window_z(cGuiSet*, float);
bool ohpl_g1_set_draw_mouse(void*, iWidget*, const cGuiMessageData&);
bool ohpl_g1_set_draw_focus(void*, iWidget*, const cGuiMessageData&);
cGuiGlobalShortcut* ohpl_g1_set_add_global_shortcut(cGuiSet*, int, int, uintptr_t, int, bool, bool);
void ohpl_g1_set_remove_global_shortcut(cGuiSet*, cGuiGlobalShortcut*);
void ohpl_g1_set_clear_global_shortcuts(cGuiSet*);
const cKeyPress* ohpl_g1_shortcut_key(cGuiGlobalShortcut*);
bool ohpl_g1_shortcut_enabled(cGuiGlobalShortcut*);
void ohpl_g1_shortcut_set_enabled(cGuiGlobalShortcut*, bool);
bool ohpl_g1_shortcut_process_key_press(cGuiGlobalShortcut*, const cKeyPress*);
bool ohpl_g1_shortcut_exec(cGuiGlobalShortcut*);
void ohpl_g1_shortcut_to_string(cGuiGlobalShortcut*, void (*)(void*, const char*, size_t), void*);
cGuiClipRegion* ohpl_g1_clip_create_child(cGuiClipRegion*, const cVector3f*, const cVector2f*);
cGuiClipRegion* ohpl_g1_clip_new();
void ohpl_g1_clip_set_rect(cGuiClipRegion*, float, float, float, float);
void ohpl_g1_clip_free(cGuiClipRegion*);
void ohpl_g1_clip_rect(const cGuiClipRegion*, cRect2f*);
void ohpl_g1_set_clear_base_clip(cGuiSet*);
void ohpl_g1_set_scissor(cGuiSet*, const cGuiClipRegion*, const cVector2l*, cVector2l*, cVector2l*);
void ohpl_g1_set_trace_fn(void (*)(void*, void*, const cVector3f*, const cVector2f*, const cColor*, int));

const char* ohpl_g1_skin_name(cGuiSkin*, size_t*);
bool ohpl_g1_skin_load(cGuiSkin*, const uint32_t*, size_t);
cGuiGfxElement* ohpl_g1_skin_gfx(cGuiSkin*, int);
cGuiSkinFont* ohpl_g1_skin_font(cGuiSkin*, int);
const cVector3f* ohpl_g1_skin_attribute(cGuiSkin*, int);
}

}

#endif
