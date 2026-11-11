//! C ABI for the widget logic, declared in `gui/RustWidget.h`.
//!
//! Pointers are valid, aligned and non-null for the call unless noted; a null input makes the call
//! a no-op that returns the zero value. Op buffers are `cap` entries long and the call returns the
//! number written.

use super::widget::{self, GFX_SLOTS, GfxMetrics, Op, Ops, ParentPos};
use super::widgets_base as wb;
use crate::types::{Vector2f, Vector3f};

type Metrics = [GfxMetrics; GFX_SLOTS];

/// # Safety
/// `p` is null or valid for reads.
unsafe fn rd<'a, T>(p: *const T) -> Option<&'a T> {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }
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
/// `ops` is null or points to `cap` writable ops; `m` is null or points to `GFX_SLOTS` metrics.
unsafe fn with_ops(
    ops: *mut Op,
    cap: usize,
    m: *const GfxMetrics,
    f: impl FnOnce(&mut Ops, &Metrics),
) -> usize {
    if ops.is_null() {
        return 0;
    }
    // SAFETY: caller contract.
    let (buf, m) = unsafe {
        (
            std::slice::from_raw_parts_mut(ops, cap),
            m.cast::<Metrics>().as_ref(),
        )
    };
    let Some(m) = m else {
        return 0;
    };
    let mut o = Ops::new(buf);
    f(&mut o, m);
    o.len()
}

// ---------------------------------------------------------------- iWidget

/// # Safety
/// All pointers readable or writable as named.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_global_position(
    parent: *const ParentPos,
    affected_by_scroll: bool,
    local: *const Vector3f,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(p), Some(l)) = (rd(parent), rd(local)) {
            wr(out, widget::global_position(p, affected_by_scroll, *l));
        }
    }
}

/// `parent` is null for a root widget.
///
/// # Safety
/// As `ohpl_wd_global_position`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_global_to_local(
    pos: *const Vector3f,
    parent: *const ParentPos,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(p) = rd(pos) {
            wr(out, widget::global_to_local(*p, rd(parent)));
        }
    }
}

/// # Safety
/// As `ohpl_wd_global_position`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_center_in_set(
    set: *const Vector2f,
    size: *const Vector2f,
    global_z: f32,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(s), Some(z)) = (rd(set), rd(size)) {
            wr(out, widget::center_in_set(*s, *z, global_z));
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_wd_point_stage(parent_ok: bool, only_clipped: bool, clips: bool) -> i32 {
    widget::point_stage(parent_ok, only_clipped, clips) as i32
}

/// # Safety
/// Pointers readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_point_in_box(
    point: *const Vector2f,
    global: *const Vector3f,
    size: *const Vector2f,
) -> bool {
    // SAFETY: caller contract.
    match unsafe { (rd(point), rd(global), rd(size)) } {
        (Some(p), Some(g), Some(s)) => widget::point_in_box(*p, *g, *s),
        _ => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_wd_ui_arrow_slot(arrow: i32) -> i32 {
    widget::ui_arrow_slot(arrow)
}

/// `local + global` when `add`, else `local - global` (child re-parenting).
///
/// # Safety
/// Pointers readable or writable as named.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_rebase(
    local: *const Vector3f,
    global: *const Vector3f,
    add: bool,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(l), Some(g)) = (rd(local), rd(global)) {
            wr(
                out,
                if add {
                    widget::add3(*l, *g)
                } else {
                    crate::math::sub3(*l, *g)
                },
            );
        }
    }
}

/// # Safety
/// Pointers readable or writable as named.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_rel_to_mouse(
    global: *const Vector3f,
    mouse: *const Vector2f,
    out: *mut Vector2f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(g), Some(m)) = (rd(global), rd(mouse)) {
            wr(out, widget::rel_to_mouse(*g, *m));
        }
    }
}

/// `iWidget::DrawBordersAndCorners`.
///
/// # Safety
/// See the module contract; `m` has `GFX_SLOTS` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_borders(
    ops: *mut Op,
    cap: usize,
    has_bg: bool,
    outside: bool,
    pos: *const Vector3f,
    size: *const Vector2f,
    m: *const GfxMetrics,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        let (Some(p), Some(s)) = (rd(pos), rd(size)) else {
            return 0;
        };
        with_ops(ops, cap, m, |o, m| {
            widget::borders_and_corners(o, has_bg, outside, *p, *s, m)
        })
    }
}

// ---------------------------------------------------------------- window

/// # Safety
/// See the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_window_draw(
    ops: *mut Op,
    cap: usize,
    d: *const widget::WindowDraw,
    m: *const GfxMetrics,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        let Some(d) = rd(d) else {
            return 0;
        };
        with_ops(ops, cap, m, |o, m| widget::window_draw(o, d, m))
    }
}

/// # Safety
/// Pointers readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_window_mouse_down(
    look: *const widget::WindowLook,
    global: *const Vector3f,
    mouse: *const Vector2f,
    val: i32,
) -> bool {
    // SAFETY: caller contract.
    match unsafe { (rd(look), rd(global), rd(mouse)) } {
        (Some(l), Some(g), Some(m)) => widget::window_mouse_down(l, *g, *m, val),
        _ => false,
    }
}

/// # Safety
/// Pointers readable or writable as named.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_window_drag_pos(
    rel: *const Vector3f,
    mouse: *const Vector2f,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(r), Some(m)) = (rd(rel), rd(mouse)) {
            wr(out, widget::window_drag_pos(*r, *m));
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_wd_window_key_closes(
    focused: bool,
    close_on_escape: bool,
    key: i32,
    modifier: i32,
) -> bool {
    widget::window_key_closes(focused, close_on_escape, key, modifier)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_wd_window_focus_lost(
    no_focused_widget: bool,
    connected_down: bool,
    connected_up: bool,
) -> bool {
    widget::window_focus_lost(no_focused_widget, connected_down, connected_up)
}

/// # Safety
/// Pointers readable or writable as named.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_window_menu_pos(
    look: *const widget::WindowLook,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(l) = rd(look) {
            wr(out, widget::window_menu_pos(l));
        }
    }
}

/// # Safety
/// `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_window_button_pos(
    width: f32,
    button_size: f32,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, widget::window_button_pos(width, button_size)) }
}

// ---------------------------------------------------------------- frame

/// # Safety
/// Pointers readable or writable as named.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_frame_slider_layout(
    size: *const Vector2f,
    h_visible: bool,
    v_visible: bool,
    out: *mut widget::SliderLayout,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(s) = rd(size) {
            wr(out, widget::frame_slider_layout(*s, h_visible, v_visible));
        }
    }
}

/// # Safety
/// `children` has `n` elements.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_frame_scroll_plan(
    input: *const widget::ScrollIn,
    children: *const widget::ChildBox,
    n: usize,
    out: *mut widget::ScrollPlan,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(i) = rd(input) {
            wr(out, widget::frame_scroll_plan(i, items(children, n)));
        }
    }
}

/// Returns whether the vertical slider must be set to `*out`.
///
/// # Safety
/// `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_frame_wheel(
    val: i32,
    value: i32,
    button_add: i32,
    out: *mut i32,
) -> bool {
    match widget::frame_wheel(val, value, button_add) {
        Some(v) => {
            // SAFETY: caller contract.
            unsafe { wr(out, v) };
            true
        }
        None => false,
    }
}

/// # Safety
/// See the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_frame_draw(
    ops: *mut Op,
    cap: usize,
    d: *const widget::FrameDraw,
    m: *const GfxMetrics,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        let Some(d) = rd(d) else {
            return 0;
        };
        with_ops(ops, cap, m, |o, m| widget::frame_draw(o, d, m))
    }
}

/// # Safety
/// See the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_frame_draw_background(
    ops: *mut Op,
    cap: usize,
    d: *const widget::FrameDraw,
    m: *const GfxMetrics,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        let Some(d) = rd(d) else {
            return 0;
        };
        with_ops(ops, cap, m, |o, _| widget::frame_draw_background(o, d))
    }
}

// ---------------------------------------------------------------- button

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_wd_button_repeat_period(freq: f32) -> f32 {
    wb::button_repeat_period(freq)
}

/// # Safety
/// Pointers readable or writable as named.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_button_mouse_down(
    s: *const wb::ButtonState,
    enabled: bool,
    val: i32,
    out: *mut wb::ButtonAction,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(s) = rd(s) {
            wr(out, wb::button_mouse_down(*s, enabled, val));
        }
    }
}

/// # Safety
/// Pointers readable or writable as named.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_button_mouse_up(
    s: *const wb::ButtonState,
    enabled: bool,
    val: i32,
    mouse_over: bool,
    out: *mut wb::ButtonAction,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(s) = rd(s) {
            wr(out, wb::button_mouse_up(*s, enabled, val, mouse_over));
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_wd_button_release(toggleable: bool, pressed: bool) -> bool {
    wb::button_release(toggleable, pressed)
}

/// # Safety
/// Pointers readable or writable as named.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_button_update(
    s: *const wb::ButtonState,
    dt: f32,
    out: *mut wb::ButtonAction,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(s) = rd(s) {
            wr(out, wb::button_update(*s, dt));
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_wd_ui_primary(val: i32) -> bool {
    wb::ui_primary(val)
}

/// # Safety
/// See the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_button_draw(
    ops: *mut Op,
    cap: usize,
    d: *const wb::ButtonDraw,
    m: *const GfxMetrics,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        let Some(d) = rd(d) else {
            return 0;
        };
        with_ops(ops, cap, m, |o, m| wb::button_draw(o, d, m))
    }
}

// ---------------------------------------------------------------- check box

/// # Safety
/// Pointers readable or writable as named.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_check_layout(
    box_width: f32,
    font_height: f32,
    label_pos_x: f32,
    label_size: *const Vector2f,
    out: *mut wb::CheckLayout,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(l) = rd(label_size) {
            wr(
                out,
                wb::check_layout(box_width, font_height, label_pos_x, *l),
            );
        }
    }
}

/// # Safety
/// `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_check_label_pos(
    box_width: f32,
    box_height: f32,
    font_height: f32,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, wb::check_label_pos(box_width, box_height, font_height)) }
}

/// # Safety
/// Pointers readable or writable as named.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_check_box_pos(
    global: *const Vector3f,
    box_height: f32,
    active_height: f32,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(g) = rd(global) {
            wr(out, wb::check_box_pos(*g, box_height, active_height));
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_wd_check_mouse_down(val: i32) -> bool {
    wb::check_mouse_down(val)
}

/// Returns the callback's result; `forward` receives whether to forward the message.
///
/// # Safety
/// `forward` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_check_label_forward(
    val: i32,
    enabled: bool,
    forward: *mut bool,
) -> bool {
    let (ret, fwd) = wb::check_label_forward(val, enabled);
    // SAFETY: caller contract.
    unsafe { wr(forward, fwd) };
    ret
}

// ---------------------------------------------------------------- label

/// # Safety
/// Pointers readable or writable as named.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_label_update(
    s: *const wb::LabelScroll,
    dt: f32,
    speed_mul: f32,
    height: f32,
    rows_height: f32,
    out: *mut wb::LabelScroll,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(s) = rd(s) {
            wr(
                out,
                wb::label_update(*s, dt, speed_mul, height, rows_height),
            );
        }
    }
}

/// Returns the op count; `rows_height_set` tells whether `*rows_height` was written (wrapped labels).
///
/// # Safety
/// `rows` has `n_rows` elements; other pointers per the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_label_draw_text(
    ops: *mut Op,
    cap: usize,
    d: *const wb::LabelDraw,
    rows: *const i32,
    n_rows: usize,
    rows_height: *mut f32,
    rows_height_set: *mut bool,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        let Some(d) = rd(d) else {
            return 0;
        };
        if ops.is_null() {
            return 0;
        }
        let mut o = Ops::new(std::slice::from_raw_parts_mut(ops, cap));
        let h = wb::label_draw_text(&mut o, d, items(rows, n_rows));
        wr(rows_height_set, h.is_some());
        if let Some(h) = h {
            wr(rows_height, h);
        }
        o.len()
    }
}

/// # Safety
/// Pointers readable or writable as named.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_label_background(
    global: *const Vector3f,
    size: *const Vector2f,
    out_pos: *mut Vector3f,
    out_size: *mut Vector2f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(g), Some(s)) = (rd(global), rd(size)) {
            let (p, z) = wb::label_background(*g, *s);
            wr(out_pos, p);
            wr(out_size, z);
        }
    }
}

// ---------------------------------------------------------------- image, dummy, group

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_wd_image_mouse(val: i32) -> bool {
    wb::image_mouse(val)
}

/// # Safety
/// `children` has `n` elements; `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_dummy_enclose(
    children: *const wb::DummyChild,
    n: usize,
    out: *mut Vector2f,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, wb::dummy_enclose(items(children, n))) }
}

/// # Safety
/// `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_group_header_pos(
    header_offset: f32,
    font_height: f32,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, wb::group_header_pos(header_offset, font_height)) }
}

/// # Safety
/// See the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_group_draw(
    ops: *mut Op,
    cap: usize,
    d: *const wb::GroupDraw,
    m: *const GfxMetrics,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        let Some(d) = rd(d) else {
            return 0;
        };
        with_ops(ops, cap, m, |o, m| wb::group_draw(o, d, m))
    }
}

// ---------------------------------------------------------------- file browser

/// `eFileBrowserFileType` of a lowercase extension, or -1 for none.
///
/// # Safety
/// `ext` has `len` wide chars.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_file_type(ext: *const u32, len: usize) -> i32 {
    // SAFETY: caller contract.
    wb::file_type_of_extension(unsafe { items(ext, len) }).unwrap_or(-1)
}

/// Returns the scaled size; `unit` receives the index into bytes/KB/MB/GB.
///
/// # Safety
/// `unit` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_human_size(size: u64, unit: *mut usize) -> u64 {
    let (v, u) = wb::human_readable_size(size);
    // SAFETY: caller contract.
    unsafe { wr(unit, u) };
    v
}

/// # Safety
/// `out` has `cap` writable chars.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_drive_letters(mask: u64, out: *mut u32, cap: usize) -> usize {
    if out.is_null() {
        return 0;
    }
    // SAFETY: caller contract.
    wb::drive_letters(mask, unsafe { std::slice::from_raw_parts_mut(out, cap) })
}

/// # Safety
/// `path` has `len` wide chars.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_navigate_target(path: *const u32, len: usize) -> i32 {
    // SAFETY: caller contract.
    wb::navigate_target(unsafe { items(path, len) }) as i32
}

/// # Safety
/// `piece` has `len` wide chars.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_path_piece(piece: *const u32, len: usize, depth: usize) -> i32 {
    // SAFETY: caller contract.
    wb::path_piece(unsafe { items(piece, len) }, depth) as i32
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<GfxMetrics>() == 16 && size_of::<Op>() == 64);
    assert!(size_of::<ParentPos>() == 36 && size_of::<widget::WindowLook>() == 40);
    assert!(size_of::<widget::WindowDraw>() == 96 && size_of::<widget::SliderLayout>() == 40);
    assert!(size_of::<widget::ChildBox>() == 24 && size_of::<widget::ScrollIn>() == 60);
    assert!(size_of::<widget::SliderPlan>() == 24 && size_of::<widget::ScrollPlan>() == 60);
    assert!(size_of::<widget::FrameDraw>() == 44 && size_of::<wb::ButtonState>() == 16);
    assert!(size_of::<wb::ButtonAction>() == 20 && size_of::<wb::ButtonDraw>() == 80);
    assert!(size_of::<wb::CheckLayout>() == 16 && size_of::<wb::LabelScroll>() == 12);
    assert!(size_of::<wb::LabelDraw>() == 84 && size_of::<wb::DummyChild>() == 20);
    assert!(size_of::<wb::GroupDraw>() == 44);
};
