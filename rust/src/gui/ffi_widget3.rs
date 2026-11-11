//! C ABI for the menu and popup logic, declared in `gui/RustWidget3.h`.
//!
//! Pointers are valid, aligned and non-null for the call unless noted; a null input makes the call
//! a no-op that returns the zero value. Text is passed as `u32` units with a length; text results
//! are written into a caller buffer of `cap` units and the call returns the full length.

use super::popup as p;
use super::widgets_menu as m;
use crate::types::{Color, Vector2f, Vector3f};

/// # Safety
/// `p` is null or valid for reads.
unsafe fn rd<'a, T>(p: *const T) -> Option<&'a T> {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }
}

/// # Safety
/// `p` is null or valid for reads and writes.
unsafe fn rw<'a, T>(p: *mut T) -> Option<&'a mut T> {
    // SAFETY: caller contract.
    unsafe { p.as_mut() }
}

/// # Safety
/// `p` is null or valid for writes.
unsafe fn wr<T>(p: *mut T, v: T) {
    // SAFETY: caller contract.
    if let Some(r) = unsafe { p.as_mut() } {
        *r = v;
    }
}

/// # Safety
/// `p` is null or points to `n` readable elements.
unsafe fn items<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// # Safety
/// `p` is null or points to `n` writable elements.
unsafe fn items_mut<'a, T>(p: *mut T, n: usize) -> &'a mut [T] {
    if p.is_null() {
        &mut []
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts_mut(p, n) }
    }
}

/// Copies as much of `v` as fits into `out[..cap]` and returns the full length.
///
/// # Safety
/// `out` is null or points to `cap` writable units.
unsafe fn put<T: Copy>(v: &[T], out: *mut T, cap: usize) -> usize {
    // SAFETY: caller contract.
    let dst = unsafe { items_mut(out, cap) };
    for (d, s) in dst.iter_mut().zip(v) {
        *d = *s;
    }
    v.len()
}

// ---------------------------------------------------------------- menu item

/// # Safety
/// `i` valid; `out` points to `cap` ops.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_item_draw(
    i: *const m::ItemDrawIn,
    out: *mut m::MenuOp,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        match rd(i) {
            Some(i) => m::item_draw(i, items_mut(out, cap)),
            None => 0,
        }
    }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_item_submenu_pos(
    kind: i32,
    parent_global: *const Vector3f,
    item_pos: *const Vector3f,
    item_size_y: f32,
    parent_w: f32,
    child_w: f32,
    virtual_x: f32,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(g), Some(ip)) = (rd(parent_global), rd(item_pos)) {
            wr(
                out,
                m::item_submenu_pos(kind, *g, *ip, item_size_y, parent_w, child_w, virtual_x),
            );
        }
    }
}

/// # Safety
/// `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_item_init_size(
    separator: bool,
    separator_height: f32,
    v_padding: f32,
    font_y: f32,
    menu_item_width: f32,
    out: *mut Vector2f,
) {
    // SAFETY: caller contract.
    unsafe {
        wr(
            out,
            m::item_init_size(
                separator,
                separator_height,
                v_padding,
                font_y,
                menu_item_width,
            ),
        );
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_item_update(
    timer: f32,
    dt: f32,
    open_time: f32,
    mouse_over: bool,
    kind: i32,
    highlighted_is_self: bool,
    is_parent: bool,
    menu_open: bool,
) -> m::ItemUpdate {
    m::item_update(
        timer,
        dt,
        open_time,
        mouse_over,
        kind,
        highlighted_is_self,
        is_parent,
        menu_open,
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_item_mouse_up(
    separator: bool,
    highlighted_is_self: bool,
    is_parent: bool,
) -> m::ItemUp {
    m::item_mouse_up(separator, highlighted_is_self, is_parent)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_item_mouse_move_highlights(
    visible: bool,
    draw_mouse: bool,
    attention_blocks: bool,
    mouse_over: bool,
) -> bool {
    m::item_mouse_move_highlights(visible, draw_mouse, attention_blocks, mouse_over)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_item_mouse_leave(
    attention_blocks: bool,
    menu_open: bool,
) -> m::ItemLeave {
    m::item_mouse_leave(attention_blocks, menu_open)
}

/// # Safety
/// Text pointers valid for their lengths; `out` for `cap` units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_join_shortcut(
    cur: *const u32,
    cur_len: usize,
    add: *const u32,
    add_len: usize,
    out: *mut u32,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        put(
            &m::join_shortcut(items(cur, cur_len), items(add, add_len)),
            out,
            cap,
        )
    }
}

// ---------------------------------------------------------------- context menu

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_ctx_item_width(size_x: f32, left: f32, right: f32) -> f32 {
    m::ctx_item_width(size_x, left, right)
}

/// # Safety
/// `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_ctx_item_pos(
    size_y: f32,
    up_h: f32,
    left_w: f32,
    num_items: usize,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, m::ctx_item_pos(size_y, up_h, left_w, num_items)) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_ctx_separation(prev_sep: bool, new_sep: bool, item_sep: f32) -> f32 {
    m::ctx_separation(prev_sep, new_sep, item_sep)
}

/// # Safety
/// `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_ctx_empty_size(
    size_x: f32,
    up_h: f32,
    down_h: f32,
    out: *mut Vector2f,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, m::ctx_empty_size(size_x, up_h, down_h)) }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_ctx_grown_size(
    size: *const Vector2f,
    item_h: f32,
    separation: f32,
    out: *mut Vector2f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(s) = rd(size) {
            wr(out, m::ctx_grown_size(*s, item_h, separation));
        }
    }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_ctx_update_width(
    met: *const m::CtxMetrics,
    item_text_w: f32,
    current_item_width: f32,
    size_y: f32,
    out: *mut m::CtxWidth,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(c) = rd(met) {
            wr(
                out,
                m::ctx_update_width(c, item_text_w, current_item_width, size_y),
            );
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_ctx_hide_plan(submenu_open: bool, has_parent_menu: bool) -> m::CtxHide {
    m::ctx_hide_plan(submenu_open, has_parent_menu)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_ctx_update(
    timer: f32,
    dt: f32,
    hide_time: f32,
    visible: bool,
    parent_item_not_highlighted: bool,
) -> m::CtxUpdate {
    m::ctx_update(timer, dt, hide_time, visible, parent_item_not_highlighted)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_ctx_lost_focus_hides(
    focus_null: bool,
    connected_to_focus: bool,
    focus_connected_to_menu: bool,
) -> bool {
    m::ctx_lost_focus_hides(focus_null, connected_to_focus, focus_connected_to_menu)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_ctx_key_hides(key: i32) -> bool {
    m::ctx_key_hides(key)
}

// ---------------------------------------------------------------- main menu

/// # Safety
/// `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_main_item_pos(
    v_padding: f32,
    has_prev: bool,
    prev_x: f32,
    prev_w: f32,
    item_separation: f32,
    out: *mut Vector3f,
) {
    let prev = has_prev.then_some((prev_x, prev_w));
    // SAFETY: caller contract.
    unsafe { wr(out, m::main_item_pos(v_padding, prev, item_separation)) }
}

/// # Safety
/// `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_main_item_size(
    text_len: f32,
    text_padding: f32,
    font_y: f32,
    v_padding: f32,
    out: *mut m::MainItemSize,
) {
    // SAFETY: caller contract.
    unsafe {
        wr(
            out,
            m::main_item_size(text_len, text_padding, font_y, v_padding),
        );
    }
}

/// # Safety
/// `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_main_init(
    parent_kind: i32,
    virtual_x: f32,
    parent_w: f32,
    font_y: f32,
    out: *mut m::MainInit,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, m::main_init(parent_kind, virtual_x, parent_w, font_y)) }
}

// ---------------------------------------------------------------- popup base

/// # Safety
/// `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_popup_init(
    attention: usize,
    focus: usize,
    default_focus: usize,
    out: *mut p::PopUpState,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, p::PopUpState::new(attention, focus, default_focus)) }
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_popup_set_next_focus(st: *mut p::PopUpState, widget: usize) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { rw(st) } {
        s.set_next_focus(widget);
    }
}

/// # Safety
/// `st` valid; `pre` and `post` point to 3 ids each.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_popup_after_callback(
    st: *mut p::PopUpState,
    pre: *const usize,
    post: *const usize,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(s), [a, b, c], [d, e, f]) = (rw(st), items(pre, 3), items(post, 3)) {
            s.after_callback([*a, *b, *c], [*d, *e, *f]);
        }
    }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_popup_pos(
    virtual_size: *const Vector2f,
    size: *const Vector2f,
    popup_z: f32,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(v), Some(s)) = (rd(virtual_size), rd(size)) {
            wr(out, p::popup_pos(*v, *s, popup_z));
        }
    }
}

// ---------------------------------------------------------------- message box

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_msgbox_layout(
    label_len: f32,
    text_len: f32,
    font_y: f32,
    virtual_size: *const Vector2f,
    two_buttons: bool,
    out: *mut p::MsgBoxLayout,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(v) = rd(virtual_size) {
            wr(
                out,
                p::msgbox_layout(label_len, text_len, font_y, *v, two_buttons),
            );
        }
    }
}

/// Returns whether the press is handled; `use_second` says the second button is the pressed one.
///
/// # Safety
/// `use_second` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_msgbox_gamepad(
    val: i32,
    has_button1: bool,
    use_second: *mut bool,
) -> bool {
    match p::msgbox_gamepad(val, has_button1) {
        Some(second) => {
            // SAFETY: caller contract.
            unsafe { wr(use_second, second) };
            true
        }
        None => false,
    }
}

// ---------------------------------------------------------------- colour picker

/// # Safety
/// `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_color_layout(out: *mut p::ColorLayout) {
    // SAFETY: caller contract.
    unsafe { wr(out, p::color_layout()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_color_slider_value(c: f32) -> i32 {
    p::color_slider_value(c)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_color_from_slider(v: i32) -> f32 {
    p::color_from_slider(v)
}

/// Index of the first set hit, or -1.
///
/// # Safety
/// `hits` points to 4 bools.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_color_component(hits: *const bool) -> i32 {
    // SAFETY: caller contract.
    match unsafe { items(hits, 4) } {
        [a, b, c, d] => p::color_component(&[*a, *b, *c, *d]).map_or(-1, |i| i as i32),
        _ => -1,
    }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_color_changed(a: *const Color, b: *const Color) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match (rd(a), rd(b)) {
            (Some(a), Some(b)) => p::color_changed(*a, *b),
            _ => false,
        }
    }
}

// ---------------------------------------------------------------- file picker

/// # Safety
/// Text pointers valid for their lengths; `out` for `cap` units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_fp_start_filename(
    file: *const u32,
    file_len: usize,
    path: *const u32,
    path_len: usize,
    out: *mut u32,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        put(
            &p::fp_start_filename(items(file, file_len), items(path, path_len)),
            out,
            cap,
        )
    }
}

/// # Safety
/// `out` valid for `cap` units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_fp_caption(kind: i32, out: *mut u32, cap: usize) -> usize {
    // SAFETY: caller contract.
    unsafe { put(&p::fp_caption(kind), out, cap) }
}

/// # Safety
/// `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_fp_layout(font_y: f32, out: *mut p::FpLayout) {
    // SAFETY: caller contract.
    unsafe { wr(out, p::fp_layout(font_y)) }
}

/// # Safety
/// `name` valid for `name_len`; `filters` and `lens` point to `count` entries, each text valid for
/// its length; `out` for `cap` units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_fp_filter_text(
    name: *const u32,
    name_len: usize,
    filters: *const *const u32,
    lens: *const usize,
    count: usize,
    out: *mut u32,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        let list: Vec<&[u32]> = items(filters, count)
            .iter()
            .zip(items(lens, count))
            .map(|(f, n)| items(*f, *n))
            .collect();
        put(&p::fp_filter_text(items(name, name_len), &list), out, cap)
    }
}

/// # Safety
/// `out` valid for `cap` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_fp_dir_entries(
    has_personal: bool,
    has_game: bool,
    path_len: usize,
    out: *mut i32,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        put(
            &p::fp_dir_entries(has_personal, has_game, path_len),
            out,
            cap,
        )
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_fp_dir_select(
    selection: i32,
    num_items: i32,
    path_len: usize,
) -> p::DirSelect {
    p::fp_dir_select(selection, num_items, path_len)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_fp_accept(kind: i32, name_empty: bool, exists: bool) -> i32 {
    p::fp_accept(kind, name_empty, exists)
}

/// Writes the extension to append and returns its length; `append` says whether to append.
///
/// # Safety
/// Text pointers valid for their lengths; `out` for `cap` units; `append` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_fp_save_extension(
    full: *const u32,
    full_len: usize,
    has_filter: bool,
    filter: *const u32,
    filter_len: usize,
    out: *mut u32,
    cap: usize,
    append: *mut bool,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        let f = has_filter.then(|| items(filter, filter_len));
        match p::fp_save_extension(items(full, full_len), f) {
            Some(ext) => {
                wr(append, true);
                put(&ext, out, cap)
            }
            None => {
                wr(append, false);
                0
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_fp_overwrite_confirmed(kind: i32, val: i32) -> bool {
    p::fp_overwrite_confirmed(kind, val)
}

// ---------------------------------------------------------------- UI keyboard

/// # Safety
/// `out` valid for `cap` keys.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_kb_keys(out: *mut p::KbKey, cap: usize) -> usize {
    // SAFETY: caller contract.
    unsafe { put(&p::kb_keys(), out, cap) }
}

/// # Safety
/// `sizes`, `pos` and `row` point to `n` entries; `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_kb_layout(
    sizes: *const Vector2f,
    n: usize,
    pos: *mut Vector3f,
    row: *mut i32,
    out: *mut p::KbLayout,
) {
    // SAFETY: caller contract.
    unsafe {
        wr(
            out,
            p::kb_layout(items(sizes, n), items_mut(pos, n), items_mut(row, n)),
        );
    }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_kb_window_pos(
    target_global: *const Vector3f,
    target_size: *const Vector2f,
    window_w: f32,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(g), Some(s)) = (rd(target_global), rd(target_size)) {
            wr(out, p::kb_window_pos(*g, *s, window_w));
        }
    }
}

/// # Safety
/// `row_len` points to `rows` lengths; `out` to `cap` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w3_kb_nav(
    row_len: *const usize,
    rows: usize,
    out: *mut p::KbNav,
    cap: usize,
) {
    // SAFETY: caller contract.
    unsafe { p::kb_nav(items(row_len, rows), items_mut(out, cap)) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_kb_ui_press(val: i32) -> i32 {
    p::kb_ui_press(val)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w3_kb_ui_release(val: i32) -> bool {
    p::kb_ui_release(val)
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<m::MenuOp>() == 40);
    assert!(size_of::<m::ItemUpdate>() == 8);
    assert!(size_of::<m::ItemUp>() == 3 && size_of::<m::ItemLeave>() == 3);
    assert!(size_of::<m::CtxMetrics>() == 32 && size_of::<m::CtxWidth>() == 52);
    assert!(size_of::<m::CtxHide>() == 2 && size_of::<m::CtxUpdate>() == 8);
    assert!(size_of::<m::MainItemSize>() == 20 && size_of::<m::MainInit>() == 32);
    assert!(size_of::<p::PopUpState>() == 32 && size_of::<p::MsgBoxLayout>() == 64);
    assert!(size_of::<p::ColorLayout>() == 228 && size_of::<p::FpLayout>() == 156);
    assert!(size_of::<p::DirSelect>() == 16 && size_of::<p::KbKey>() == 28);
    assert!(size_of::<p::KbLayout>() == 16 && size_of::<p::KbNav>() == 16);
};
