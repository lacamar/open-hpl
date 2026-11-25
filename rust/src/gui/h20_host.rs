use super::g1_set;
use super::g1_skin;
use super::gw::{self, skin_of};
use super::gw_h20::{self, KIND_TAB, KIND_TABLABEL, KIND_TEXTBOX, P};
use super::gw_host::host;
use super::gw_items as gi;
use super::gw_kinds as gk;
use super::h19_widget::CallbackFn;
use super::h20_ffi as f;
use super::set::{Msg, W};
use crate::types::{Color, Vector2f, Vector3f};
use std::ffi::c_void;

pub const TYPE_ROOT: i32 = 0;
pub const TYPE_WINDOW: i32 = 1;

const WHITE: Color = Color {
    r: 1.0,
    g: 1.0,
    b: 1.0,
    a: 1.0,
};

type Text = (*const u32, usize);

fn slice<'a>(t: Text) -> &'a [u32] {
    if t.0.is_null() {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(t.0, t.1) }
    }
}

fn pubf<T: Default>(w: W, f: impl FnOnce(&gw::Pubf) -> T) -> T {
    gw::read(w, |g| f(&g.p))
}

pub fn skin_font_size(w: W) -> Vector2f {
    gw::read(w, |g| {
        unsafe { g.font.as_ref() }.map_or(Vector2f::default(), |f| f.size)
    })
}

fn disabled_color(w: W) -> Color {
    let f = gw::skin_font(skin_of(w), "Disabled");
    unsafe { f.as_ref() }.map_or(WHITE, |f| f.color)
}

fn mul(c: Color, m: Color) -> Color {
    Color {
        r: c.r * m.r,
        g: c.g * m.g,
        b: c.b * m.b,
        a: c.a * m.a,
    }
}

fn draw_text_in(w: W, text: &[u32], pos: Vector3f, align: i32, col: Option<Color>) {
    let (font, size, fc, cm) = gw::read(w, |g| {
        (g.font_type, g.p.font_size, g.p.font_color, g.p.color_mul)
    });
    if font == 0 {
        return;
    }
    gw::draw_font(
        gw::set_of(w),
        font,
        text,
        size,
        pos,
        mul(col.unwrap_or(fc), cm),
        align,
    );
}

pub fn draw_text(w: W, text: Text, pos: Vector3f, align: i32, highlight: bool) {
    let hl = highlight.then(|| pubf(w, |p| p.font_highlight));
    draw_text_in(w, slice(text), pos, align, hl);
}

pub fn draw_text_enabled(w: W, text: Text, pos: Vector3f, align: i32) {
    let c = if gw::is_enabled(w) {
        pubf(w, |p| p.font_color)
    } else {
        disabled_color(w)
    };
    draw_text_in(w, slice(text), pos, align, Some(c));
}

pub fn draw_text_state(w: W, text: Text, pos: Vector3f, align: i32, state: i32) {
    let c = match state {
        0 => disabled_color(w),
        1 => pubf(w, |p| p.font_highlight),
        _ => pubf(w, |p| p.font_color),
    };
    draw_text_in(w, slice(text), pos, align, Some(c));
}

pub fn slider_set_value(w: W, v: i32, flag: Option<bool>) {
    gk::ohpl_gw_slider_set_value(w, v, flag.unwrap_or(true));
}

pub fn slider_value(w: W) -> i32 {
    gk::ohpl_gw_slider_value(w)
}

pub fn slider_set_bar_size(w: W, v: i32) {
    gk::ohpl_gw_slider_set_bar_size(w, v);
}

pub fn slider_set_max(w: W, v: i32) {
    gk::ohpl_gw_slider_set_max(w, v);
}

pub fn slider_set_button_add(w: W, v: i32) {
    gk::ohpl_gw_slider_set_button_add(w, v);
}

pub fn slider_set_click_add(w: W, v: i32) {
    gk::ohpl_gw_slider_set_click_add(w, v);
}

pub fn slider_button_add(w: W) -> i32 {
    gk::ohpl_gw_slider_button_add(w)
}

pub fn set_size(w: W, s: Vector2f) {
    gw::set_size(w, s);
}

pub fn size(w: W) -> Vector2f {
    gw::size(w)
}

pub fn set_position(w: W, p: Vector3f) {
    gw::set_position(w, p);
}

pub fn position(w: W) -> Vector3f {
    gw::read(w, |g| g.base.pos)
}

pub fn item_num(container: usize) -> i32 {
    gi::num(container) as i32
}

pub fn item(container: usize, idx: i32) -> usize {
    gi::get(container, idx) as usize
}

pub fn item_selected(item: usize) -> bool {
    gi::ohpl_gwi_selected(item as *mut _)
}

pub fn item_selectable(item: usize) -> bool {
    gi::ohpl_gwi_selectable(item as *mut _)
}

pub fn item_set_selected(item: usize, v: bool) {
    gi::set_selected(item as *mut _, v);
}

pub fn item_index(item: usize) -> i32 {
    gi::ohpl_gwi_index(item as *mut _)
}

pub fn item_text(item: usize) -> Text {
    let mut n = 0;
    let p = gi::ohpl_gwi_text(item as *mut _, &mut n);
    (p, n)
}

pub fn item_prop(item: usize, col: i32) -> usize {
    gi::ohpl_gwi_prop(item as *mut _, col) as usize
}

pub fn prop_is_text(prop: usize) -> bool {
    gi::ohpl_gwi_prop_type(prop as *mut _) as usize == 0
}

pub fn prop_text(prop: usize) -> Text {
    let mut n = 0;
    let p = gi::ohpl_gwi_prop_text(prop as *mut _, &mut n);
    (p, n)
}

pub fn prop_gfx(prop: usize) -> usize {
    gi::ohpl_gwi_prop_gfx(prop as *mut _)
}

pub fn process(w: W, msg: i32, val: i32, data: Option<&Msg>) -> bool {
    match data {
        Some(d) => gw::process_message(w, msg, d, false, false),
        None => gw::process_message(w, msg, &Msg::with_val(val), false, false),
    }
}

pub fn press(w: W, msg: i32, data: &Msg) {
    gw::process_message(w, msg, data, true, true);
}

pub fn world_to_local(w: W, p: [f32; 2]) -> Vector3f {
    gw::world_to_local(
        w,
        Vector3f {
            x: p[0],
            y: p[1],
            z: 0.0,
        },
    )
}

pub fn global(w: W) -> Vector3f {
    gw::global_position(w)
}

pub fn font_size(w: W) -> Vector2f {
    pubf(w, |p| p.font_size)
}

pub fn set_font_size(w: W, s: Vector2f) {
    gw::set_default_font_size(w, s);
}

pub fn has_focus(w: W) -> bool {
    gw::has_focus(w)
}

pub fn add_callback(w: W, msg: i32, obj: usize, f: usize) {
    let cb: CallbackFn = unsafe { std::mem::transmute(f) };
    gw::add_callback(w, msg, obj as *mut c_void, cb);
}

pub fn draw_gfx(w: W, gfx: usize, pos: Vector3f, size: Vector2f, color: Option<[f32; 4]>) {
    let c = color.map_or(WHITE, |c| Color {
        r: c[0],
        g: c[1],
        b: c[2],
        a: c[3],
    });
    g1_set::draw_gfx_default(gw::set_of(w), gfx as *mut _, pos, size, c);
}

pub fn draw_gfx_native(w: W, gfx: usize, pos: Vector3f) {
    g1_set::draw_gfx_default(
        gw::set_of(w),
        gfx as *mut _,
        pos,
        Vector2f { x: -1.0, y: -1.0 },
        WHITE,
    );
}

pub fn draw_borders(
    w: W,
    bg: usize,
    (borders, corners): (&[usize; 4], &[usize; 4]),
    pos: Vector3f,
    size: Vector2f,
    flag: Option<bool>,
) {
    gw::draw_borders(
        w,
        bg as *mut _,
        &borders.map(|g| g as *mut _),
        &corners.map(|g| g as *mut _),
        pos,
        size,
        flag == Some(true),
    );
}

pub fn skin_gfx(w: W, idx: i32) -> usize {
    unsafe { g1_skin::ohpl_g1_skin_gfx(skin_of(w), idx) as usize }
}

pub fn skin_attr(w: W, idx: i32) -> Vector3f {
    unsafe { g1_skin::ohpl_g1_skin_attribute(skin_of(w), idx).as_ref() }
        .copied()
        .unwrap_or_default()
}

pub fn gfx_active_size(gfx: usize) -> Vector2f {
    gw::metrics(gfx as *mut _).active
}

pub fn gfx_image_size(gfx: usize) -> Vector2f {
    gw::metrics(gfx as *mut _).image
}

pub fn gfx_rect() -> usize {
    super::g1_gui::ohpl_g1_gfx_rect() as usize
}

pub fn gfx_anim_reset(gfx: usize) {
    unsafe { super::g1_gfx::ohpl_g1_gfx_set_animation_time(gfx as *mut _, 0.0) };
}

pub fn clip_child(clip: usize, pos: Vector3f, size: Vector2f) -> usize {
    unsafe { g1_set::ohpl_g1_clip_create_child(clip as *mut _, &pos, &size) as usize }
}

pub fn set_clip(w: W, clip: usize) {
    unsafe { g1_set::ohpl_g1_set_set_current_clip(gw::set_of(w), clip as *mut _) };
}

pub fn push_attention(w: W) {
    unsafe { g1_set::ohpl_g1_set_push_attention(gw::set_of(w)) };
}

pub fn push_focused(w: W) {
    unsafe { g1_set::ohpl_g1_set_push_focused(gw::set_of(w)) };
}

pub fn push_default_nav(w: W) {
    unsafe { g1_set::ohpl_g1_set_push_default_nav(gw::set_of(w)) };
}

pub fn set_attention(w: W, target: usize) {
    unsafe { g1_set::ohpl_g1_set_set_attention(gw::set_of(w), target, true, false) };
}

pub fn set_attention_no_clear(w: W, target: usize) {
    unsafe { g1_set::ohpl_g1_set_set_attention(gw::set_of(w), target, false, false) };
}

pub fn set_focused(w: W, target: usize) {
    unsafe { g1_set::ohpl_g1_set_set_focused(gw::set_of(w), target, false) };
}

pub fn pop_attention(w: W) {
    unsafe { g1_set::ohpl_g1_set_pop_attention(gw::set_of(w), true) };
}

pub fn pop_focused(w: W) {
    unsafe { g1_set::ohpl_g1_set_pop_focused(gw::set_of(w)) };
}

pub fn pop_default_nav(w: W) {
    unsafe { g1_set::ohpl_g1_set_pop_default_nav(gw::set_of(w)) };
}

pub fn popup_z(w: W) -> f32 {
    unsafe { g1_set::ohpl_g1_set_popup_z(gw::set_of(w)) }
}

pub fn virtual_size(w: W) -> [f32; 2] {
    let r = g1_set::virtual_rect(gw::set_of(w));
    [r[0], r[1]]
}

pub fn dec_context_z(w: W) {
    unsafe { g1_set::ohpl_g1_set_add_context_menu_z(gw::set_of(w), -5.0) };
}

pub fn attention(w: W) -> usize {
    g1_set::attention_widget(gw::set_of(w))
}

pub fn focused(w: W) -> usize {
    g1_set::focused_widget(gw::set_of(w))
}

pub fn base_clip(w: W) -> usize {
    unsafe { g1_set::ohpl_g1_set_base_clip(gw::set_of(w)) as usize }
}

pub fn current_clip(w: W) -> usize {
    unsafe { g1_set::ohpl_g1_set_current_clip(gw::set_of(w)) as usize }
}

pub fn show_context_menu(w: W, menu: usize, pos: Vector3f) {
    super::gw_menu::show_context_menu(gw::set_of(w), menu, pos);
}

pub fn draw_mouse(w: W) -> bool {
    g1_set::draw_mouse(gw::set_of(w))
}

#[allow(clippy::too_many_arguments)]
fn create(
    w: W,
    kind: i32,
    pos: Vector3f,
    size: Vector2f,
    txt: &[u32],
    parent: W,
    b: i32,
    num: f32,
) -> usize {
    unsafe {
        gw_h20::ohpl_gw_create_c(
            gw::set_of(w).cast(),
            kind,
            &pos,
            &size,
            txt.as_ptr(),
            txt.len(),
            parent,
            std::ptr::null(),
            0,
            0,
            b,
            num,
        )
    }
}

pub fn create_slider(w: W) -> usize {
    gk::create_slider(
        gw::set_of(w),
        1,
        Vector3f::default(),
        Vector2f::default(),
        0,
        w,
        b"",
    ) as usize
}

pub fn create_label(w: W, name: Text, z: f32) -> usize {
    let l = gk::create_label(
        gw::set_of(w),
        Vector3f { x: 0.0, y: 0.0, z },
        Vector2f { x: -1.0, y: -1.0 },
        slice(name),
        w,
        b"",
    );
    gw::set_default_font_size(l, font_size(w));
    gk::label_set_bg_color(
        l,
        Color {
            r: 0.82,
            g: 0.81,
            b: 0.79,
            a: 1.0,
        },
    );
    l
}

pub fn create_textbox(w: W, size: Vector2f) -> usize {
    create(w, KIND_TEXTBOX, Vector3f::default(), size, &[], w, 1, 1.0)
}

pub fn create_button(w: W) -> usize {
    create_button_at(w, Vector3f::default(), Vector2f::default())
}

pub fn create_button_at(w: W, pos: Vector3f, size: Vector2f) -> usize {
    gk::create_button(gw::set_of(w), pos, size, &[], w, false, b"") as usize
}

pub fn button_set_image(button: W, gfx: usize) {
    gk::ohpl_gw_button_set_image(button, gfx as *mut _, false);
}

pub fn button_setup(button: W, font: f32, freq: f32) {
    gw::set_default_font_size(button, Vector2f { x: font, y: font });
    gk::ohpl_gw_button_set_repeat_active(button, true);
    gk::ohpl_gw_button_set_repeat_freq(button, freq);
}

pub fn create_tab(w: W, pos: Vector3f, size: Vector2f) -> usize {
    create(w, KIND_TAB, pos, size, &[], w, 0, 0.0)
}

pub fn create_tab_label(w: W, height: f32) -> usize {
    let t = gw::text(w);
    create(
        w,
        KIND_TABLABEL,
        Vector3f::default(),
        Vector2f { x: 0.0, y: height },
        &t,
        0,
        0,
        0.0,
    )
}

pub fn create_frame(w: W, size: Vector2f, hori: bool, vert: bool) -> usize {
    gk::create_frame(
        gw::set_of(w),
        Vector3f::default(),
        size,
        false,
        0,
        hori,
        vert,
        b"",
    ) as usize
}

pub fn create_popup_keyboard(w: W) {
    let set = gw::set_of(w);
    unsafe {
        super::g2_popup::ohpl_g2_popup_kb(set as usize, g1_set::ohpl_g1_set_skin(set) as usize, w)
    };
}

pub fn destroy_widget(w: W, child: usize) {
    g1_set::destroy_widget(gw::set_of(w), child, false);
}

pub fn set_visible(w: W, v: bool) {
    gw::set_visible(w, v);
}

pub fn set_enabled(w: W, v: bool) {
    gw::set_enabled(w, v);
}

pub fn is_enabled(w: W) -> bool {
    gw::is_enabled(w)
}

pub fn is_visible(w: W) -> bool {
    gw::is_visible(w)
}

pub fn set_clip_active(w: W, v: bool) {
    gw::with(w, |g| g.base.clips = v);
}

pub fn set_text(w: W, text: Text) {
    gw::set_text(w, slice(text));
}

pub fn text(w: W) -> Text {
    gw::read(w, |g| (g.text.as_ptr(), g.text.len()))
}

pub fn set_can_edit(w: W, v: bool) {
    if let Some(P::Text(t)) = gw_h20::ptr(w) {
        unsafe { f::ohpl_h20_tb_set_can_edit(t, v) };
    }
}

pub fn can_edit(w: W) -> bool {
    match gw_h20::ptr(w) {
        Some(P::Text(t)) => unsafe { (*t).st.can_edit },
        _ => false,
    }
}

pub fn textbox_background_size(w: W) -> Vector2f {
    let mut s = Vector2f::default();
    if let Some(P::Text(t)) = gw_h20::ptr(w) {
        unsafe { f::ohpl_h20_tb_background_size(t, &mut s) };
    }
    s
}

pub fn check_point_clip(w: W, p: [f32; 2]) -> bool {
    let par = gw::parent(w);
    par == 0
        || !gw::clips_graphics(par)
        || gw::point_is_inside(par, Vector2f { x: p[0], y: p[1] }, true)
}

pub fn state_ptr(w: W) -> usize {
    gw_h20::ohpl_gw_state(w) as usize
}

pub fn attach_child(w: W, child: usize, as_widget: bool) {
    if as_widget {
        gw::attach_child(w, child)
    } else {
        gw::attach_child_base(w, child)
    }
}

pub fn remove_child(w: W, child: usize, as_widget: bool) {
    if as_widget {
        gw::remove_child(w, child)
    } else {
        gw::remove_child_base(w, child)
    }
}

pub fn text_length(w: W, of: W) -> f32 {
    let (font, size) = gw::read(w, |g| (g.font_type, g.p.font_size));
    if font == 0 {
        0.0
    } else {
        gw::font_length(font, size, &gw::text(of))
    }
}

pub fn text_length_skin(w: W) -> f32 {
    let (font, t) = gw::read(w, |g| (g.font_type, g.text.clone()));
    if font == 0 {
        0.0
    } else {
        gw::font_length(font, skin_font_size(w), &t)
    }
}

pub fn font_length(w: W, text: &[u32]) -> f32 {
    let (font, size) = gw::read(w, |g| (g.font_type, g.p.font_size));
    if font == 0 {
        0.0
    } else {
        gw::font_length(font, size, text)
    }
}

pub fn font_range(w: W) -> (i32, i32) {
    let font = gw::read(w, |g| g.font_type);
    let (mut lo, mut hi) = (0, 0);
    if font != 0
        && let Some(h) = host()
    {
        unsafe { (h.font_range)(font, &mut lo, &mut hi) };
    }
    (lo, hi)
}

pub fn glyph(w: W, num: i32) -> Option<f32> {
    let font = gw::read(w, |g| g.font_type);
    let mut adv = 0.0;
    let ok = font != 0 && host().is_some_and(|h| unsafe { (h.glyph_advance)(font, num, &mut adv) });
    ok.then_some(adv)
}

unsafe extern "C" fn push_wide(ctx: *mut c_void, p: *const u32, n: usize) {
    unsafe { (*ctx.cast::<Vec<u32>>()).extend_from_slice(std::slice::from_raw_parts(p, n)) };
}

pub fn clipboard() -> Vec<u32> {
    let mut out = Vec::new();
    if let Some(h) = host() {
        unsafe { (h.clipboard_load)((&raw mut out).cast(), push_wide) };
    }
    out
}

pub fn copy(text: &[u32]) {
    if let Some(h) = host() {
        unsafe { (h.clipboard_save)(text.as_ptr(), text.len()) };
    }
}

pub fn to_float(text: &[u32], decimals: i32) -> f32 {
    host().map_or(0.0, |h| unsafe {
        (h.to_float)(text.as_ptr(), text.len(), decimals)
    })
}

pub fn float_text(v: f32, decimals: i32) -> Vec<u32> {
    let mut out = Vec::new();
    if let Some(h) = host() {
        unsafe { (h.float_text)(v, decimals, (&raw mut out).cast(), push_wide) };
    }
    out
}

pub fn set_pointer_gfx(w: W, text: bool) {
    let g = gw::skin_gfx(
        skin_of(w),
        if text { "PointerText" } else { "PointerNormal" },
    );
    gw::with(w, |x| x.pointer_gfx = g);
}

pub fn widget_type(w: W) -> i32 {
    if w == 0 { -1 } else { gw::read(w, |g| g.ty) }
}

pub fn set_children_offset(w: W, o: Vector3f) {
    gw::set_children_offset(w, o);
}

pub fn is_connected(w: W, other: W, explicit_false: bool) -> bool {
    gw::is_connected_to(w, other, !explicit_false)
}
