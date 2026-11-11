//! C ABI of the ImGui core, mirrored in `soma/src/game/RustSomaUi.h`.

use super::imgui::{
    self, Actions, Fade, Focus, Layout, MouseIn, NavEntry, Prev, State, WidgetIn, WidgetOut,
};
use super::imgui_widgets::{self as widgets, ButtonFlags, ButtonOut, MultiIn, SliderIn};
use super::imgui_window::{self, WindowIn, WindowOut};

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<State>() == 56 && offset_of!(State, def_int) == 44);
    assert!(size_of::<Fade>() == 48);
    assert!(size_of::<Layout>() == 48);
    assert!(size_of::<Prev>() == 28);
    assert!(size_of::<NavEntry>() == 32);
};

/// The three action arrays of a UI.
#[repr(C)]
pub struct OhplGs7ImActions {
    pub down: *mut [bool; 10],
    pub triggered: *mut [bool; 10],
    pub used: *mut [bool; 10],
}

/// # Safety
/// The pointers are live, distinct and writable.
unsafe fn actions<'a>(a: &OhplGs7ImActions) -> Actions<'a> {
    // SAFETY: caller contract.
    unsafe {
        Actions {
            down: &mut *a.down,
            triggered: &mut *a.triggered,
            used: &mut *a.used,
        }
    }
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_default_or_current_int(s: *mut State, def: i32) -> i32 {
    // SAFETY: caller contract.
    imgui::default_or_current_int(unsafe { &mut *s }, def)
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_default_or_current_float(s: *mut State, def: f32) -> f32 {
    // SAFETY: caller contract.
    imgui::default_or_current_float(unsafe { &mut *s }, def)
}

/// Returns whether the fade has a time, then `x` is the eased input.
///
/// # Safety
/// `f` and `x` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_fade_advance(f: *mut Fade, dt: f32, x: *mut f32) -> bool {
    // SAFETY: caller contract.
    unsafe {
        imgui::fade_advance(&mut *f, dt).is_some_and(|v| {
            *x = v;
            true
        })
    }
}

/// # Safety
/// `f` and `s` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_fade_apply(f: *const Fade, t: f32, s: *mut State) {
    // SAFETY: caller contract.
    unsafe { imgui::fade_apply(&*f, t, &mut *s) }
}

/// # Safety
/// `existing` is null or live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_fade_blocked(existing: *const Fade, replace: bool) -> bool {
    // SAFETY: caller contract.
    imgui::fade_blocked(unsafe { existing.as_ref() }, replace)
}

/// # Safety
/// `s` is live; `goal` has 4 floats; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_fade_make(
    s: *const State,
    ty: i32,
    goal: *const f32,
    time: f32,
    easing: i32,
    out: *mut Fade,
) {
    // SAFETY: caller contract.
    unsafe { *out = imgui::fade_make(&*s, ty, goal.cast::<[f32; 4]>().read(), time, easing) }
}

#[repr(C)]
pub struct OhplGs7ImMouseIn {
    pub screen: [f32; 2],
    pub virtual_size: [f32; 2],
    pub virtual_offset: [f32; 2],
    pub is_3d: bool,
    pub cursor_3d: [f32; 2],
}

#[repr(C)]
pub struct OhplGs7ImMouseOut {
    pub pos: [f32; 2],
    pub rel: [f32; 2],
    pub cursor_3d: [f32; 2],
}

/// # Safety
/// `i` and `out` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_mouse_map(
    i: *const OhplGs7ImMouseIn,
    px: i32,
    py: i32,
    rx: i32,
    ry: i32,
    out: *mut OhplGs7ImMouseOut,
) {
    // SAFETY: caller contract.
    unsafe {
        let i = &*i;
        let (pos, rel, cursor_3d) = imgui::mouse_map(
            &MouseIn {
                screen: i.screen,
                virtual_size: i.virtual_size,
                virtual_offset: i.virtual_offset,
                is_3d: i.is_3d,
                cursor_3d: i.cursor_3d,
            },
            [px, py],
            [rx, ry],
        );
        *out = OhplGs7ImMouseOut {
            pos,
            rel,
            cursor_3d,
        };
    }
}

/// # Safety
/// Vectors have the stated float counts and are writable where mutated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_layout(
    pos: *mut [f32; 3],
    size: *mut [f32; 2],
    default_size: *const f32,
    origin: *const f32,
    do_align: bool,
    align_mode: i32,
) {
    // SAFETY: caller contract.
    unsafe {
        imgui::layout(
            &mut *pos,
            &mut *size,
            default_size.cast::<[f32; 2]>().read(),
            origin.cast::<[f32; 3]>().read(),
            do_align,
            align_mode,
        );
    }
}

/// # Safety
/// Vectors have the stated float counts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_align(
    pos: *const f32,
    size: *const f32,
    mode: i32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let r = imgui::align(
            pos.cast::<[f32; 3]>().read(),
            size.cast::<[f32; 2]>().read(),
            mode,
        );
        out.cast::<[f32; 3]>().write(r);
    }
}

/// # Safety
/// `l` is live; vectors have the stated float counts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_advance(l: *mut Layout, pos: *const f32, size: *const f32) {
    // SAFETY: caller contract.
    unsafe {
        imgui::advance(
            &mut *l,
            pos.cast::<[f32; 3]>().read(),
            size.cast::<[f32; 2]>().read(),
        );
    }
}

/// # Safety
/// Vectors have the stated float counts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_mouse_over(
    use_input: bool,
    has_focus: bool,
    mouse: *const f32,
    pos: *const f32,
    size: *const f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        imgui::mouse_over(
            use_input,
            has_focus,
            mouse.cast::<[f32; 2]>().read(),
            pos.cast::<[f32; 3]>().read(),
            size.cast::<[f32; 2]>().read(),
        )
    }
}

/// # Safety
/// All pointers are live; the action arrays are distinct.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_widget_base(
    id: u64,
    i: *const WidgetIn,
    f: *mut Focus,
    a: *const OhplGs7ImActions,
    st: *mut State,
    prev: *mut Prev,
    out: *mut WidgetOut,
) {
    // SAFETY: caller contract.
    unsafe {
        *out = imgui::widget_base(id, &*i, &mut *f, &mut actions(&*a), &mut *st, &mut *prev);
    }
}

/// # Safety
/// `a` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_became_pressed(
    a: *const OhplGs7ImActions,
    keys: bool,
    mouse: bool,
) -> bool {
    // SAFETY: caller contract.
    unsafe { imgui::became_pressed(&mut actions(&*a), keys, mouse) }
}

/// # Safety
/// `a` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_action_triggered(
    a: *const OhplGs7ImActions,
    action: i32,
    check_used: bool,
) -> bool {
    // SAFETY: caller contract.
    unsafe { actions(&*a).triggered(action, check_used) }
}

/// # Safety
/// `nav` has `n` entries; `f` and `a` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_update_ui_movement(
    nav: *const NavEntry,
    n: usize,
    f: *mut Focus,
    a: *const OhplGs7ImActions,
) {
    // SAFETY: caller contract.
    unsafe {
        let nav: &[NavEntry] = if n == 0 {
            &[]
        } else {
            std::slice::from_raw_parts(nav, n)
        };
        imgui::update_ui_movement(nav, &mut *f, &mut actions(&*a));
    }
}

/// # Safety
/// All pointers are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_button(
    st: *mut State,
    a: *const OhplGs7ImActions,
    over: bool,
    mouse_over: bool,
    mode: i32,
    dt: f32,
    flags: *const ButtonFlags,
    out: *mut ButtonOut,
) {
    // SAFETY: caller contract.
    unsafe {
        *out = widgets::button(
            &mut *st,
            &mut actions(&*a),
            over,
            mouse_over,
            mode,
            dt,
            *flags,
        );
    }
}

/// True when the mouse focus is to be locked.
///
/// # Safety
/// All pointers are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_slider(
    st: *mut State,
    a: *const OhplGs7ImActions,
    i: *const SliderIn,
) -> bool {
    // SAFETY: caller contract.
    unsafe { widgets::slider(&mut *st, &mut actions(&*a), &*i) }
}

/// # Safety
/// Vectors have the stated float counts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_slider_button_pos(
    value: f32,
    min: f32,
    max: f32,
    pos: *const f32,
    size: *const f32,
    button: *const f32,
    vertical: bool,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let r = widgets::slider_button_pos(
            value,
            [min, max],
            pos.cast::<[f32; 3]>().read(),
            size.cast::<[f32; 2]>().read(),
            button.cast::<[f32; 2]>().read(),
            vertical,
        );
        out.cast::<[f32; 3]>().write(r);
    }
}

/// # Safety
/// `st` and `a` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_checkbox(
    st: *mut State,
    a: *const OhplGs7ImActions,
    over: bool,
) -> bool {
    // SAFETY: caller contract.
    unsafe { widgets::checkbox(&mut *st, &mut actions(&*a), over) }
}

/// # Safety
/// All pointers are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_multi_select(
    st: *mut State,
    a: *const OhplGs7ImActions,
    i: *const MultiIn,
) {
    // SAFETY: caller contract.
    unsafe { widgets::multi_select(&mut *st, &mut actions(&*a), &*i) }
}

/// # Safety
/// Vectors have the stated float counts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_text_origin(
    pos: *const f32,
    size: *const f32,
    font: *const f32,
    align: i32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let r = widgets::text_origin(
            pos.cast::<[f32; 3]>().read(),
            size.cast::<[f32; 2]>().read(),
            font.cast::<[f32; 2]>().read(),
            align,
        );
        out.cast::<[f32; 3]>().write(r);
    }
}

/// # Safety
/// Colours have 4 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_color_mul(a: *const f32, b: *const f32, out: *mut f32) {
    // SAFETY: caller contract.
    unsafe {
        let r = widgets::color_mul(a.cast::<[f32; 4]>().read(), b.cast::<[f32; 4]>().read());
        out.cast::<[f32; 4]>().write(r);
    }
}

/// # Safety
/// Vectors have the stated float counts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_gauge_fill(
    pos: *const f32,
    size: *const f32,
    pad: *const f32,
    fill: f32,
    vertical: bool,
    out_pos: *mut f32,
    out_size: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let (p, s) = widgets::gauge_fill(
            pos.cast::<[f32; 3]>().read(),
            size.cast::<[f32; 2]>().read(),
            pad.cast::<[f32; 2]>().read(),
            fill,
            vertical,
        );
        out_pos.cast::<[f32; 3]>().write(p);
        out_size.cast::<[f32; 2]>().write(s);
    }
}

/// # Safety
/// `i` and `out` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_im_window(i: *const WindowIn, out: *mut WindowOut) {
    // SAFETY: caller contract.
    unsafe { *out = imgui_window::window(&*i) }
}
