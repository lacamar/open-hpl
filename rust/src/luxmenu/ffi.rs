//! C ABI of the GA7a menu layer, mirrored in `amnesia/src/game/RustLuxMenu.h`.

use super::keyconfig::{self as kc, Sub};
use super::options::{self as opt, Mode, Range};
use super::profile::{self as pf, Date};
use std::ffi::c_void;

type Ctx = *mut c_void;
type Sink32 = unsafe extern "C" fn(Ctx, *const u32, usize);
type SetColor = unsafe extern "C" fn(Ctx, usize, i32, i32);

#[repr(C)]
pub struct Str {
    p: *const u8,
    n: usize,
}

#[repr(C)]
pub struct Op {
    kind: i32,
    a: i32,
    b: i32,
}

fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the caller passes a live `n`-byte buffer.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn units<'a>(p: *const u32, n: usize) -> &'a [u32] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the caller passes a live `n`-unit buffer.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn subs<'a>(p: *const Sub, n: usize) -> &'a [Sub] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the caller passes `n` live sub-actions.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn subs_mut<'a>(p: *mut Sub, n: usize) -> &'a mut [Sub] {
    if n == 0 || p.is_null() {
        return &mut [];
    }
    // SAFETY: the caller passes `n` writable sub-actions that nothing else touches for the call.
    unsafe { std::slice::from_raw_parts_mut(p, n) }
}

fn wide(s: Sink32, c: Ctx, v: &[u32]) {
    // SAFETY: `s` accepts `c`.
    unsafe { s(c, v.as_ptr(), v.len()) }
}

fn put<T>(p: *mut T, v: T) {
    if !p.is_null() {
        // SAFETY: non-null out pointers are writable.
        unsafe { p.write(v) }
    }
}

// ---- profile --------------------------------------------------------------------------------

/// # Safety
/// `dates` holds `n` dates and `out` has room for `n` indices.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga7a_pf_sort(dates: *const Date, n: usize, out: *mut u32) {
    let d: &[Date] = if n == 0 || dates.is_null() {
        &[]
    } else {
        // SAFETY: per the contract.
        unsafe { std::slice::from_raw_parts(dates, n) }
    };
    for (i, v) in pf::sort_order(d).into_iter().enumerate() {
        // SAFETY: `out` has room for `n` indices.
        unsafe { out.add(i).write(v) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_pf_trim(p: *const u32, n: usize, start: *mut usize, end: *mut usize) {
    let (s, e) = pf::trim(units(p, n));
    put(start, s);
    put(end, e);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_pf_illegal(len: *mut usize) -> *const u32 {
    put(len, pf::ILLEGAL_CHARS.len());
    pf::ILLEGAL_CHARS.as_ptr()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_pf_name_max() -> i32 {
    pf::NAME_MAX
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_pf_select(num: i32, count: usize) -> i32 {
    pf::select_target(num, count).map_or(-1, |n| n as i32)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_pf_delete_plan(
    selected: i32,
    in_use: i32,
    config_differs: i32,
    new_selection: *mut i32,
) -> i32 {
    let p = pf::delete_plan(selected, in_use != 0, config_differs != 0);
    put(new_selection, p.new_selection);
    i32::from(p.reset_profile) | (i32::from(p.remove_config) << 1)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_pf_activate(item_num: i32) -> i32 {
    i32::from(pf::activate(item_num) == pf::Activate::CreateWindow)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_pf_ui_press(
    secondary: i32,
    list_locked: i32,
    name_visible: i32,
) -> i32 {
    i32::from(pf::window_ui_press(
        secondary != 0,
        list_locked != 0,
        name_visible != 0,
    ))
}

// ---- key config -----------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_key_pos(s: *const Sub, n: usize, key: i32) -> i32 {
    kc::key_pos(subs(s, n), key)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_mouse_pos(s: *const Sub, n: usize, button: i32) -> i32 {
    kc::mouse_pos(subs(s, n), button)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_pad_button_pos(s: *const Sub, n: usize, button: i32) -> i32 {
    kc::pad_button_pos(subs(s, n), button)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_pad_hat_pos(s: *const Sub, n: usize, hat: i32, state: i32) -> i32 {
    kc::pad_hat_pos(subs(s, n), hat, state)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_pad_axis_pos(s: *const Sub, n: usize, axis: i32, range: i32) -> i32 {
    kc::pad_axis_pos(subs(s, n), axis, range)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_has_sub_actions(s: *const Sub, n: usize) -> i32 {
    i32::from(kc::has_sub_actions(subs(s, n)))
}

/// Writes the positions of `a` shared with `b` to `out` (room for 2); returns how many.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_sharing(
    a: *const Sub,
    na: usize,
    b: *const Sub,
    nb: usize,
    out: *mut i32,
) -> usize {
    let v = kc::sharing(subs(a, na), subs(b, nb));
    for (i, p) in v.iter().take(kc::POS_NUM).enumerate() {
        // SAFETY: `out` has room for 2 positions.
        unsafe { out.add(i).write(*p) };
    }
    v.len().min(kc::POS_NUM)
}

/// What: 0 key, 1 mouse button, 2 pad button, 3 pad hat (id, state), 4 pad axis (id, range).
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_set(s: *mut Sub, n: usize, pos: i32, what: i32, a: i32, b: i32) {
    let bind = match what {
        0 => kc::Bind::Key(a),
        1 => kc::Bind::Mouse(a),
        2 => kc::Bind::PadButton(a),
        3 => kc::Bind::PadHat(a, b),
        _ => kc::Bind::PadAxis(a, b),
    };
    kc::set(subs_mut(s, n), pos, bind);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_from_string(p: *const u8, n: usize, value: i32, out: *mut Sub) {
    put(out, kc::from_string(bytes(p, n), value));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_label(
    s: *const Sub,
    sink: unsafe extern "C" fn(Ctx, *const u8, usize),
    ctx: Ctx,
) {
    // SAFETY: `s` is a live sub-action.
    let Some(sub) = (unsafe { s.as_ref() }) else {
        return;
    };
    let l = kc::label(sub);
    // SAFETY: `sink` accepts `ctx`.
    unsafe { sink(ctx, l.as_ptr(), l.len()) }
}

/// # Safety
/// `names` and `values` hold `n` entries; `s` holds `ns` writable sub-actions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga7a_kc_fill_default(
    names: *const Str,
    values: *const i32,
    n: usize,
    s: *mut Sub,
    ns: usize,
    has_mac_key: i32,
    mac_key: i32,
) {
    let mut inputs = Vec::new();
    for i in 0..n {
        // SAFETY: per the contract.
        let (name, value) = unsafe { (&*names.add(i), *values.add(i)) };
        inputs.push((bytes(name.p, name.n), value));
    }
    let mac = (has_mac_key != 0).then_some(mac_key);
    kc::fill_default(&inputs, subs_mut(s, ns), mac);
}

/// # Safety
/// `names` holds `n` entries; `s` holds `ns` writable sub-actions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga7a_kc_fill_initial(
    names: *const Str,
    n: usize,
    s: *mut Sub,
    ns: usize,
) {
    let mut v = Vec::new();
    for i in 0..n {
        // SAFETY: per the contract.
        let name = unsafe { &*names.add(i) };
        v.push(bytes(name.p, name.n).to_vec());
    }
    kc::fill_initial(&v, subs_mut(s, ns));
}

/// Engine calls for the bindings, -1 when unchanged. Kinds: 1 key, 2 mouse, 3 pad button,
/// 4 pad hat (id, state), 5 pad axis (id, range).
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_update(
    s: *const Sub,
    n: usize,
    changed: i32,
    out: *mut Op,
    cap: usize,
) -> i32 {
    let Some(ops) = kc::update_ops(subs(s, n), changed != 0) else {
        return -1;
    };
    for (i, op) in ops.iter().take(cap).enumerate() {
        let (kind, a, b) = match *op {
            kc::Op::Key(k) => (1, k, 0),
            kc::Op::Mouse(k) => (2, k, 0),
            kc::Op::PadButton(k) => (3, k, 0),
            kc::Op::PadHat(h, st) => (4, h, st),
            kc::Op::PadAxis(ax, r) => (5, ax, r),
        };
        // SAFETY: `out` has room for `cap` ops.
        unsafe { out.add(i).write(Op { kind, a, b }) };
    }
    ops.len().min(cap) as i32
}

/// `set(ctx, entry, pos, red)` runs at the original `SetColor` points; returns "invalid inputs".
///
/// # Safety
/// `s` holds two sub-actions per entry, `counts` holds `ncat` entries and `set` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga7a_kc_check(
    s: *const Sub,
    nsubs: usize,
    counts: *const usize,
    ncat: usize,
    set: SetColor,
    ctx: Ctx,
) -> i32 {
    let c: &[usize] = if ncat == 0 || counts.is_null() {
        &[]
    } else {
        // SAFETY: per the contract.
        unsafe { std::slice::from_raw_parts(counts, ncat) }
    };
    let invalid = kc::check_inputs(subs(s, nsubs), c, |e, p, red| {
        // SAFETY: `set` accepts `ctx`.
        unsafe { set(ctx, e, p, i32::from(red)) }
    });
    i32::from(invalid)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_binds_pad(p: *const u8, n: usize) -> i32 {
    i32::from(kc::binds_pad(bytes(p, n)))
}

/// 0 nothing, 1 save, 2 cancel.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_key_press(waiting: i32, key: i32, escape: i32) -> i32 {
    match kc::key_press(waiting != 0, key, escape) {
        kc::KeyAct::None => 0,
        kc::KeyAct::Save => 1,
        kc::KeyAct::Cancel => 2,
    }
}

/// 0 eat (return true), 1 pass (return false), 2 cancel, 3 save and raise the flag.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_pad_press(
    waiting: i32,
    value: f32,
    is_button: i32,
    id: i32,
    cancel_button: i32,
) -> i32 {
    match kc::pad_press(waiting != 0, value, is_button != 0, id, cancel_button) {
        kc::PadAct::Eat => 0,
        kc::PadAct::Pass => 1,
        kc::PadAct::Cancel => 2,
        kc::PadAct::Save => 3,
    }
}

fn press(p: kc::Press) -> i32 {
    i32::from(p.handled) | (i32::from(p.clear_flag) << 1) | (i32::from(p.start_waiting) << 2)
}

/// Bit 0 handled, bit 1 clear the flag, bit 2 start waiting.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_ui_button(is_primary: i32, flag: i32, nobody_waiting: i32) -> i32 {
    press(kc::ui_button(
        is_primary != 0,
        flag != 0,
        nobody_waiting != 0,
    ))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_ui_arrow(flag: i32) -> i32 {
    press(kc::ui_arrow(flag != 0))
}

/// 0 nothing, 1 start waiting, 2 save the mouse button.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_click(nobody_waiting: i32, is_left: i32) -> i32 {
    match kc::click(nobody_waiting != 0, is_left != 0) {
        kc::Click::Nothing => 0,
        kc::Click::StartWaiting => 1,
        kc::Click::SaveMouse => 2,
    }
}

/// 0 ignore, 1 push, 2 pop.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_set_waiting(has_new: i32, has_current: i32) -> i32 {
    match kc::set_waiting(has_new != 0, has_current != 0) {
        kc::Waiting::Ignore => 0,
        kc::Waiting::Push => 1,
        kc::Waiting::Pop => 2,
    }
}

/// Bit 0 fade out, bit 1 fade restore.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_tip_flags(tip_empty: i32) -> i32 {
    let (o, r) = kc::tip_flags(tip_empty != 0);
    i32::from(o) | (i32::from(r) << 1)
}

/// Fills `alphas` (room for 2) with the alpha of each colour write; flags bit 0 out, bit 1
/// restore go in and come out through `flags`. Returns the number of writes.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_tip_fade(
    alpha: f32,
    flags: *mut i32,
    dt: f32,
    alphas: *mut f32,
) -> usize {
    let f = if flags.is_null() {
        0
    } else {
        // SAFETY: non-null `flags` is readable.
        unsafe { flags.read() }
    };
    let (nf, sets) = kc::tip_fade(
        alpha,
        kc::Fade {
            out: f & 1 != 0,
            restore: f & 2 != 0,
        },
        dt,
    );
    put(flags, i32::from(nf.out) | (i32::from(nf.restore) << 1));
    for (i, a) in sets.iter().enumerate() {
        // SAFETY: `alphas` has room for 2 alphas.
        unsafe { alphas.add(i).write(*a) };
    }
    sets.len()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_category_active(selected: i32, category: i32) -> i32 {
    i32::from(kc::category_active(selected, category))
}

/// New category to select, -1 when the press is not handled.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_category_button(
    was_waiting: i32,
    button: i32,
    prev: i32,
    next: i32,
    category: i32,
    items: i32,
) -> i32 {
    kc::category_button(was_waiting != 0, button, prev, next, category, items).unwrap_or(-1)
}

/// 0 nothing, 1 clear the cancelled flag, 2 go to the options.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_exit(popup_active: i32, cancelled: i32) -> i32 {
    match kc::exit_pressed(popup_active != 0, cancelled != 0) {
        kc::Exit::Nothing => 0,
        kc::Exit::ClearCancelled => 1,
        kc::Exit::ToOptions => 2,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_kc_ui_cancel(was_waiting: i32, is_secondary: i32) -> i32 {
    i32::from(kc::ui_cancel(was_waiting != 0, is_secondary != 0))
}

// ---- options --------------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_aniso_index(x: f32) -> i32 {
    opt::aniso_index(x)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_aniso_from_index(i: i32) -> f32 {
    opt::aniso_from_index(i)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_ssao_index(samples: i32) -> i32 {
    opt::ssao_index(samples)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_ssao_from_index(i: i32) -> i32 {
    opt::ssao_from_index(i)
}

/// Kinds: 0 gamma, 1 mouse, 2 gamepad look, 3 volume, 4 HDR white, 5 HDR peak.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_range(kind: i32, min: *mut f32, step: *mut f32, max: *mut f32) {
    let r = match kind {
        0 => Range::Gamma,
        1 => Range::Mouse,
        2 => Range::Gamepad,
        3 => Range::Volume,
        4 => Range::PaperWhite,
        _ => Range::Peak,
    };
    let (lo, st, hi) = opt::range(r);
    put(min, lo);
    put(step, st);
    put(max, hi);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_slider_setup(
    min: f32,
    max: f32,
    step: f32,
    max_value: *mut i32,
    bar_size: *mut i32,
) {
    let (m, b) = opt::slider_setup(min, max, step);
    put(max_value, m);
    put(bar_size, b);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_slider_to_int(
    value: f32,
    min: f32,
    max: f32,
    slider_max: i32,
) -> i32 {
    opt::slider_to_int(value, min, max, slider_max)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_slider_to_value(
    value: i32,
    slider_max: i32,
    min: f32,
    max: f32,
) -> f32 {
    opt::slider_to_value(value, slider_max, min, max)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_slider_label(
    value: f32,
    min: f32,
    max: f32,
    sink: Sink32,
    ctx: Ctx,
) {
    wide(sink, ctx, &opt::slider_label(value, min, max));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_int_label(value: f32, sink: Sink32, ctx: Ctx) {
    wide(sink, ctx, &opt::int_label(value));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_renderer_name(i: i32) -> *const u8 {
    static NAMES: [&[u8]; 2] = [b"OpenGL\0", b"Vulkan\0"];
    NAMES
        .get(usize::from(i == 1))
        .map_or(std::ptr::null(), |n| n.as_ptr())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_renderer_index(p: *const u8, n: usize) -> i32 {
    opt::renderer_index(bytes(p, n))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_color_name(i: i32) -> *const u8 {
    static NAMES: [&[u8]; 4] = [b"SDR\0", b"HDR10\0", b"scRGB\0", b"P3\0"];
    let i = usize::try_from(i.clamp(0, 3)).unwrap_or(0);
    NAMES.get(i).map_or(std::ptr::null(), |n| n.as_ptr())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_color_index(p: *const u8, n: usize) -> i32 {
    opt::color_index(bytes(p, n))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_gui_scale_index(scale: i32, items: i32) -> i32 {
    opt::gui_scale_index(scale, items)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_flip_index(i: i32, items: i32) -> i32 {
    opt::flip_index(i, items)
}

/// # Safety
/// `files` holds `n` wide strings.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga7a_op_language_index(
    lang: *const u8,
    lang_len: usize,
    files: *const WStr,
    n: usize,
) -> i32 {
    let mut v: Vec<&[u32]> = Vec::new();
    for i in 0..n {
        // SAFETY: per the contract.
        let f = unsafe { &*files.add(i) };
        v.push(units(f.p, f.n));
    }
    opt::language_index(bytes(lang, lang_len), &v)
}

#[repr(C)]
pub struct WStr {
    p: *const u32,
    n: usize,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_sound_device_index(
    id: i32,
    ids: *const i32,
    n: usize,
    items: i32,
) -> i32 {
    let v: &[i32] = if n == 0 || ids.is_null() {
        &[]
    } else {
        // SAFETY: the caller passes `n` ids.
        unsafe { std::slice::from_raw_parts(ids, n) }
    };
    opt::sound_device_index(id, v, items)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_multi_display(m: *const Mode, n: usize) -> i32 {
    i32::from(opt::multi_display(modes(m, n)))
}

fn modes<'a>(m: *const Mode, n: usize) -> &'a [Mode] {
    if n == 0 || m.is_null() {
        return &[];
    }
    // SAFETY: the caller passes `n` modes.
    unsafe { std::slice::from_raw_parts(m, n) }
}

/// Per surviving mode, writes its source index and refresh rate (room for `n` each); returns
/// the count.
///
/// # Safety
/// `m` holds `n` modes; `src` and `refresh` have room for `n` values.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga7a_op_dedup_modes(
    m: *const Mode,
    n: usize,
    src: *mut u32,
    refresh: *mut i32,
) -> usize {
    let v = opt::dedup_modes(modes(m, n));
    for (k, (s, r)) in v.iter().enumerate() {
        // SAFETY: at most `n` survivors.
        unsafe {
            src.add(k).write(*s as u32);
            refresh.add(k).write(*r);
        }
    }
    v.len()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_current_mode(
    m: *const Mode,
    n: usize,
    display: i32,
    w: i32,
    h: i32,
) -> i32 {
    opt::current_mode(modes(m, n), display, w, h)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_resolution_label(
    w: i32,
    h: i32,
    custom: i32,
    sink: Sink32,
    ctx: Ctx,
) {
    wide(sink, ctx, &opt::resolution_label(w, h, custom != 0));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_needs_restart(
    setting_initial: i32,
    has_data: i32,
    data_needs: i32,
) -> i32 {
    i32::from(opt::needs_restart(
        setting_initial != 0,
        has_data != 0,
        data_needs != 0,
    ))
}

/// 0 keep the key config values, 1 from current, 2 dump then initial, 3 from initial.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_activate(
    key_config_open: i32,
    setting_language: i32,
    initial_set: i32,
) -> i32 {
    match opt::activate(
        key_config_open != 0,
        setting_language != 0,
        initial_set != 0,
    ) {
        opt::Activate::KeepKeyConfig => 0,
        opt::Activate::FromCurrent => 1,
        opt::Activate::DumpThenInitial => 2,
        opt::Activate::FromInitial => 3,
    }
}

/// Bit 0 show basic, bit 1 show advanced, bit 2 label index, bit 3 toggle button first.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_toggle_gfx(basic_visible: i32) -> i32 {
    let t = opt::toggle_gfx(basic_visible != 0);
    i32::from(t.show_basic)
        | (i32::from(t.show_advanced) << 1)
        | ((t.label as i32) << 2)
        | (i32::from(t.toggle_first) << 3)
}

/// Flags bit 0 restart, bit 1 text reset. Returns the action: 0 none, 1 fade in, 2 fade out;
/// bit 8 set when the scroll offset resets, bit 9 when fade in clamps alpha to 1.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_op_tip_step(
    flags: *mut i32,
    alpha: f32,
    dt: f32,
    delta: *mut f32,
) -> i32 {
    let f = if flags.is_null() {
        0
    } else {
        // SAFETY: non-null `flags` is readable.
        unsafe { flags.read() }
    };
    let s = opt::tip_step(
        opt::Tip {
            restart: f & 1 != 0,
            text_reset: f & 2 != 0,
        },
        alpha,
        dt,
    );
    put(
        flags,
        i32::from(s.tip.restart) | (i32::from(s.tip.text_reset) << 1),
    );
    let (act, d, clamp) = match s.act {
        opt::TipAct::None => (0, 0.0, false),
        opt::TipAct::FadeIn { delta, clamp } => (1, delta, clamp),
        opt::TipAct::FadeOut { delta } => (2, delta, false),
    };
    put(delta, d);
    act | (i32::from(s.reset_scroll) << 8) | (i32::from(clamp) << 9)
}
