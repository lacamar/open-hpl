#![allow(dead_code)]
//! Gui, set, gfx and simple-widget calls for Rust callers that used to go through a C++ host
//! table. Pointers are the addresses the host handed out (`usize`).

use std::ptr;

use super::g1_gfx::{self, Element};
use super::g1_gui::{self, Gui};
use super::g1_set::{self, Set};
use super::g1_skin::Skin;
use super::gw::{self, read};
use super::gw_kinds::{
    self, KIND_BUTTON, KIND_CHECK, KIND_DUMMY, KIND_FRAME, KIND_GROUP, KIND_IMAGE, KIND_LABEL,
    KIND_SLIDER, KIND_WINDOW,
};
use crate::types::{Color, Vector2f, Vector3f};

pub(crate) const MAT_ALPHA: i32 = 1;
pub(crate) const MAT_LAST: i32 = 6;
pub(crate) const TEXTURE_RECT: i32 = 2;
pub(crate) const WHITE: Color = Color {
    r: 1.0,
    g: 1.0,
    b: 1.0,
    a: 1.0,
};

fn gui() -> *mut Gui {
    g1_gui::current()
}

fn set(s: usize) -> *mut Set {
    s as *mut Set
}

pub(crate) fn create_set(name: &[u8], skin: usize) -> usize {
    // SAFETY: the gui is live for the process; `name` is readable.
    unsafe { g1_gui::ohpl_g1_gui_create_set(gui(), name.as_ptr(), name.len(), skin as *mut Skin) }
}

pub(crate) fn create_skin(file: &[u8]) -> usize {
    // SAFETY: the gui is live; `file` is readable.
    unsafe { g1_gui::ohpl_g1_gui_create_skin(gui(), file.as_ptr(), file.len()) as usize }
}

pub(crate) fn set_virtual_size(
    s: usize,
    size: [f32; 2],
    min_z: f32,
    max_z: f32,
    offset: [f32; 2],
    ignore_scale: bool,
) {
    let v = |a: [f32; 2]| Vector2f { x: a[0], y: a[1] };
    // SAFETY: `s` is a live set; the vectors outlive the call.
    unsafe {
        g1_set::ohpl_g1_set_set_virtual_size(
            set(s),
            &v(size),
            min_z,
            max_z,
            &v(offset),
            ignore_scale,
        );
    }
}

pub(crate) fn set_active(s: usize, on: bool) {
    g1_set::set_active(set(s), on);
}

pub(crate) fn set_draw_mouse(s: usize, on: bool) {
    g1_set::set_draw_mouse(set(s), on);
}

pub(crate) fn set_skin(s: usize, skin: usize) {
    // SAFETY: `s` is a live set; the skin is live or null.
    unsafe { g1_set::ohpl_g1_set_set_skin(set(s), skin as *mut Skin) };
}

pub(crate) fn gui_focus(s: usize) {
    g1_gui::focus_set(gui(), s);
}

pub(crate) fn set_default_nav(s: usize, w: usize) {
    g1_set::set_default_nav(set(s), w);
}

pub(crate) fn set_focused(s: usize, w: usize) {
    g1_set::set_focused(set(s), w);
}

pub(crate) fn gfx_rect(color: Color, material: i32, add: bool) -> usize {
    g1_gui::create_gfx_filled_rect(gui(), color, material, add) as usize
}

pub(crate) fn gfx_image(file: &[u8], material: i32) -> usize {
    g1_gui::create_gfx_image(gui(), file, material, WHITE, true) as usize
}

pub(crate) fn gfx_texture_file(file: &[u8], material: i32, texture_type: i32) -> usize {
    // SAFETY: the gui is live; `file` and the color are readable.
    unsafe {
        g1_gui::ohpl_g1_gui_create_gfx_texture_file(
            gui(),
            file.as_ptr(),
            file.len(),
            material,
            texture_type,
            &WHITE,
            false,
            true,
        ) as usize
    }
}

pub(crate) fn gfx_from_texture(texture: usize, auto_destroy: bool, material: i32) -> usize {
    g1_gui::create_gfx_texture(
        gui(),
        texture,
        auto_destroy,
        material,
        WHITE,
        true,
        [0.0, 0.0],
        [1.0, 1.0],
    ) as usize
}

pub(crate) fn gfx_destroy(g: usize) {
    g1_gui::destroy_gfx(gui(), g as *mut Element);
}

pub(crate) fn gfx_image_size(g: usize) -> [f32; 2] {
    let mut v = Vector2f::default();
    // SAFETY: `g` is a live element.
    unsafe { g1_gfx::ohpl_g1_gfx_image_size(g as *mut Element, &mut v) };
    [v.x, v.y]
}

pub(crate) fn gfx_active_size(g: usize) -> [f32; 2] {
    // SAFETY: `g` is a live element; the size lives as long as it.
    unsafe { g1_gfx::ohpl_g1_gfx_active_size(g as *mut Element).as_ref() }
        .map_or([0.0; 2], |v| [v.x, v.y])
}

pub(crate) fn draw_gfx(
    s: usize,
    g: usize,
    pos: [f32; 3],
    size: [f32; 2],
    color: Color,
    material: i32,
    angle: f32,
) {
    g1_set::draw_gfx_traced(
        set(s),
        g as *mut Element,
        Vector3f {
            x: pos[0],
            y: pos[1],
            z: pos[2],
        },
        Vector2f {
            x: size[0],
            y: size[1],
        },
        color,
        material,
        angle,
    );
}

fn vec3(p: [f32; 3]) -> Vector3f {
    Vector3f {
        x: p[0],
        y: p[1],
        z: p[2],
    }
}

fn vec2(p: [f32; 2]) -> Vector2f {
    Vector2f { x: p[0], y: p[1] }
}

#[allow(clippy::too_many_arguments)]
fn create(
    s: usize,
    kind: i32,
    pos: [f32; 3],
    size: Option<[f32; 2]>,
    text: &[u32],
    parent: usize,
    a: i32,
    b: i32,
    c: i32,
    img: usize,
) -> usize {
    let (pos, size) = (vec3(pos), size.map(vec2));
    // SAFETY: `s` is a live set; every pointer is readable for the call.
    unsafe {
        gw_kinds::ohpl_gw_create(
            s as *mut _,
            kind,
            &pos,
            size.as_ref().map_or(ptr::null(), ptr::from_ref),
            text.as_ptr(),
            text.len(),
            parent,
            ptr::null(),
            0,
            a,
            b,
            c,
            img as *mut Element,
        )
    }
}

pub(crate) fn widget_label(
    s: usize,
    pos: [f32; 3],
    size: [f32; 2],
    text: &[u32],
    parent: usize,
) -> usize {
    create(s, KIND_LABEL, pos, Some(size), text, parent, 0, 0, 0, 0)
}

pub(crate) fn widget_button(
    s: usize,
    pos: [f32; 3],
    size: [f32; 2],
    text: &[u32],
    parent: usize,
) -> usize {
    create(s, KIND_BUTTON, pos, Some(size), text, parent, 0, 0, 0, 0)
}

pub(crate) fn widget_dummy(s: usize, pos: [f32; 3], parent: usize) -> usize {
    create(s, KIND_DUMMY, pos, None, &[], parent, 0, 0, 0, 0)
}

/// `cGuiSet::CreateWidgetImage`.
pub(crate) fn widget_image(
    s: usize,
    file: &[u8],
    pos: [f32; 3],
    size: [f32; 2],
    material: i32,
    animate: bool,
    parent: usize,
) -> usize {
    let mut gfx = 0;
    if !file.is_empty() {
        gfx = if animate {
            g1_gui::create_gfx_image_buffer(gui(), file, material, true, WHITE, true) as usize
        } else {
            gfx_image(file, material)
        };
    }
    let mut size = size;
    if gfx != 0 && size[0] < 0.0 {
        size = gfx_image_size(gfx);
    }
    create(s, KIND_IMAGE, pos, Some(size), &[], parent, 0, 0, 0, gfx)
}

pub(crate) fn widget_slider(
    s: usize,
    orientation: i32,
    pos: [f32; 3],
    size: [f32; 2],
    max: i32,
    parent: usize,
) -> usize {
    create(
        s,
        KIND_SLIDER,
        pos,
        Some(size),
        &[],
        parent,
        orientation,
        0,
        max,
        0,
    )
}

pub(crate) fn widget_destroy(s: usize, w: usize) {
    g1_set::destroy_widget(set(s), w, false);
}

pub(crate) fn widget_pos(w: usize, p: [f32; 3]) {
    gw::set_position(w, vec3(p));
}

pub(crate) fn widget_size(w: usize, s: [f32; 2]) {
    gw::set_size(w, vec2(s));
}

pub(crate) fn widget_get_size(w: usize) -> [f32; 2] {
    read(w, |g| [g.base.size.x, g.base.size.y])
}

pub(crate) fn widget_get_local_pos(w: usize) -> [f32; 3] {
    read(w, |g| [g.base.pos.x, g.base.pos.y, g.base.pos.z])
}

pub(crate) fn widget_text(w: usize, t: &[u32]) {
    gw::set_text(w, t);
}

pub(crate) fn widget_font_color(w: usize, c: Color) {
    gw::set_default_font_color(w, c);
}

pub(crate) fn widget_font_size(w: usize, s: [f32; 2]) {
    gw::set_default_font_size(w, vec2(s));
}

pub(crate) fn widget_font_type(w: usize, f: usize) {
    gw::set_default_font_type(w, f);
}

pub(crate) fn widget_default_font(w: usize) -> usize {
    read(w, |g| g.font_type)
}

pub(crate) fn label_align(w: usize, a: i32) {
    gw_kinds::ohpl_gw_label_set_align(w, a);
}

pub(crate) fn label_wrap(w: usize, on: bool) {
    gw_kinds::ohpl_gw_label_set_word_wrap(w, on);
}

pub(crate) fn label_autosize(w: usize, on: bool) {
    gw_kinds::ohpl_gw_label_set_autogen(w, on);
}

pub(crate) fn slider_max(w: usize, v: i32) {
    gw_kinds::ohpl_gw_slider_set_max(w, v);
}

pub(crate) fn slider_bar(w: usize, v: i32) {
    gw_kinds::ohpl_gw_slider_set_bar_size(w, v);
}

pub(crate) fn slider_set(w: usize, v: i32, callback: bool) {
    gw_kinds::ohpl_gw_slider_set_value(w, v, callback);
}

pub(crate) fn slider_get(w: usize) -> i32 {
    gw_kinds::ohpl_gw_slider_value(w)
}

pub(crate) fn slider_get_max(w: usize) -> i32 {
    gw_kinds::ohpl_gw_slider_max(w)
}

pub(crate) fn send_mouse_button(msg: i32, button: i32) {
    // SAFETY: the gui is live.
    unsafe { g1_gui::ohpl_g1_gui_send_mouse_button(gui(), msg, button, 0) };
}

pub(crate) fn send_value(msg: i32, value: i32) {
    // SAFETY: the gui is live.
    unsafe { g1_gui::ohpl_g1_gui_send_value(gui(), msg, value) };
}

/// `gpBase->mpGameHudSet`.
pub(crate) fn hud_set() -> usize {
    let name = b"GameHud";
    // SAFETY: the gui is live; `name` is readable.
    unsafe { g1_gui::ohpl_g1_gui_set_from_name(gui(), name.as_ptr(), name.len()) }
}

pub(crate) fn widget_frame(s: usize, pos: [f32; 3], size: [f32; 2], parent: usize) -> usize {
    widget_frame_scroll(s, pos, size, parent, false, false)
}

pub(crate) fn widget_frame_scroll(
    s: usize,
    pos: [f32; 3],
    size: [f32; 2],
    parent: usize,
    hscroll: bool,
    vscroll: bool,
) -> usize {
    create(
        s,
        KIND_FRAME,
        pos,
        Some(size),
        &[],
        parent,
        i32::from(hscroll),
        i32::from(vscroll),
        0,
        0,
    )
}

pub(crate) fn widget_window(
    s: usize,
    close: bool,
    pos: [f32; 3],
    size: [f32; 2],
    text: &[u32],
) -> usize {
    create(
        s,
        KIND_WINDOW,
        pos,
        Some(size),
        text,
        0,
        i32::from(close),
        0,
        0,
        0,
    )
}

pub(crate) fn widget_checkbox(
    s: usize,
    pos: [f32; 3],
    size: [f32; 2],
    text: &[u32],
    parent: usize,
) -> usize {
    create(s, KIND_CHECK, pos, Some(size), text, parent, 0, 0, 0, 0)
}

pub(crate) fn widget_group(
    s: usize,
    pos: [f32; 3],
    size: [f32; 2],
    text: &[u32],
    parent: usize,
) -> usize {
    create(s, KIND_GROUP, pos, Some(size), text, parent, 0, 0, 0, 0)
}
