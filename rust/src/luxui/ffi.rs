//! C ABI of the GA8 UI layer, mirrored in `amnesia/src/game/RustLuxUi.h`. Functions that produce
//! events write them to a caller array and return the count; the strings inside stay valid until
//! the next call on the same thread, so the caller copies them before running any event.

use super::event::{Ev, Out, Str, flush};
use super::inventory::{self as inv, InvState};
use super::journal::{self as jrn, Pages};
use super::message::{self as msg, MessageState};
use super::{commentary, hint, itemtype, keyentry};
use std::cell::Cell;
use std::ffi::c_void;

type Ctx = *mut c_void;
type Sink = unsafe extern "C" fn(Ctx, *const u8, usize);
type Sink32 = unsafe extern "C" fn(Ctx, *const u32, usize);
type Replace = unsafe extern "C" fn(Ctx, *const u32, usize, i32, Sink32, Ctx);

thread_local! {
    static SCRATCH: Cell<Option<Out>> = const { Cell::new(None) };
}

fn run(out: *mut Ev, cap: usize, f: impl FnOnce(&mut Out)) -> usize {
    let mut o = SCRATCH.with(Cell::take).unwrap_or_default();
    o.clear();
    f(&mut o);
    // SAFETY: the caller passes `cap` writable events.
    let n = unsafe { flush(&o, out, cap) };
    SCRATCH.with(|c| c.set(Some(o)));
    n
}

fn bytes<'a>(s: Str) -> &'a [u8] {
    if s.n == 0 || s.p.is_null() {
        return &[];
    }
    // SAFETY: the caller passes a live `n`-byte buffer.
    unsafe { std::slice::from_raw_parts(s.p, s.n) }
}

fn pb<'a>(p: *const u8, n: usize) -> &'a [u8] {
    bytes(Str { p, n })
}

fn units<'a>(p: *const u32, n: usize) -> &'a [u32] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the caller passes a live `n`-unit buffer.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn sink(s: Sink, c: Ctx, v: &[u8]) {
    // SAFETY: `s` accepts `c`.
    unsafe { s(c, v.as_ptr(), v.len()) }
}

fn vec3(p: *const f32) -> [f32; 3] {
    // SAFETY: the caller passes 3 floats.
    unsafe { [*p, *p.add(1), *p.add(2)] }
}

fn vec2(p: *const f32) -> [f32; 2] {
    // SAFETY: the caller passes 2 floats.
    unsafe { [*p, *p.add(1)] }
}

fn put(p: *mut f32, v: &[f32]) {
    for (i, x) in v.iter().enumerate() {
        // SAFETY: the caller passes `v.len()` writable floats.
        unsafe { p.add(i).write(*x) };
    }
}

macro_rules! state {
    ($p:expr) => {
        // SAFETY: the caller passes a live state or null.
        match unsafe { $p.as_mut() } {
            Some(s) => s,
            None => return,
        }
    };
    ($p:expr, $d:expr) => {
        // SAFETY: the caller passes a live state or null.
        match unsafe { $p.as_mut() } {
            Some(s) => s,
            None => return $d,
        }
    };
}

// ---- key entry ------------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_key_verify(c: u8) -> i32 {
    i32::from(keyentry::verify_char(c))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_key_route(keyboard: i32, paste: i32, tab: i32) -> i32 {
    match keyentry::route(keyboard != 0, paste != 0, tab != 0) {
        keyentry::Route::Base => 0,
        keyentry::Route::Key => 1,
        keyentry::Route::Paste => 2,
    }
}

/// # Safety
/// The out pointers are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_key_action(
    composed: i32,
    len: i32,
    pos: i32,
    mark: i32,
    ascii: u8,
    out_pos: *mut i32,
    out_epos: *mut i32,
    out_ch: *mut u8,
) -> i32 {
    match keyentry::key_action(composed != 0, len, pos, mark, ascii) {
        keyentry::KeyAct::Base => 0,
        keyentry::KeyAct::Swallow => 1,
        keyentry::KeyAct::Insert { pos, epos, ch } => {
            // SAFETY: writable per the contract.
            unsafe {
                out_pos.write(pos);
                out_epos.write(epos);
                out_ch.write(ch);
            }
            2
        }
    }
}

/// # Safety
/// `p` is `n` readable bytes, `out` is `cap` writable bytes (17 suffice).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_key_paste(
    p: *const u8,
    n: usize,
    out: *mut u8,
    cap: usize,
) -> usize {
    let v = keyentry::paste_filter(pb(p, n));
    let k = v.len().min(cap);
    for (i, c) in v.iter().take(k).enumerate() {
        // SAFETY: i < cap.
        unsafe { out.add(i).write(*c) };
    }
    k
}

// ---- item types -----------------------------------------------------------------------------

#[repr(C)]
pub struct ItInfo {
    pub name: Str,
    pub has_count: i32,
    pub max_count: i32,
    pub show_pickup: i32,
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_it_info(t: i32, out: *mut ItInfo) -> i32 {
    let Some(i) = itemtype::type_info(t) else {
        return 0;
    };
    // SAFETY: writable per the contract.
    unsafe {
        out.write(ItInfo {
            name: Str::of(i.name),
            has_count: i32::from(i.has_count),
            max_count: i.max_count,
            show_pickup: i32::from(i.show_pickup),
        });
    }
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_it_from_string(p: *const u8, n: usize) -> i32 {
    itemtype::from_string(pb(p, n)).unwrap_or(-1)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_it_has_max(t: i32) -> i32 {
    i32::from(itemtype::has_max_amount(t))
}

#[repr(C)]
pub struct ItItem {
    pub name: Str,
    pub sval: Str,
    pub extra: Str,
    pub amount: f32,
}

#[repr(C)]
pub struct ItPlayer {
    pub coins: i32,
    pub health: f32,
    pub sanity: f32,
    pub lamp_oil: f32,
    pub has_lantern: i32,
    pub lantern_active: i32,
    pub use_item_state: i32,
}

fn item(i: &ItItem) -> itemtype::Item<'_> {
    itemtype::Item {
        name: bytes(i.name),
        sval: bytes(i.sval),
        extra: bytes(i.extra),
        amount: i.amount,
    }
}

fn player(p: &ItPlayer) -> itemtype::Player {
    itemtype::Player {
        coins: p.coins,
        health: p.health,
        sanity: p.sanity,
        lamp_oil: p.lamp_oil,
        has_lantern: p.has_lantern != 0,
        lantern_active: p.lantern_active != 0,
        use_item_state: p.use_item_state != 0,
    }
}

/// # Safety
/// Pointers are valid; `consumed` is writable; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_it_before_add(
    t: i32,
    it: *const ItItem,
    p: *const ItPlayer,
    consumed: *mut i32,
    out: *mut Ev,
    cap: usize,
) -> usize {
    // SAFETY: valid per the contract.
    let (Some(it), Some(p)) = (unsafe { it.as_ref() }, unsafe { p.as_ref() }) else {
        return 0;
    };
    let mut c = false;
    let n = run(out, cap, |o| {
        c = itemtype::before_add(t, &item(it), &player(p), o)
    });
    // SAFETY: writable per the contract.
    unsafe { consumed.write(i32::from(c)) };
    n
}

/// # Safety
/// Pointers are valid; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_it_note_before_add(
    null: i32,
    it: *const ItItem,
    out: *mut Ev,
    cap: usize,
) -> usize {
    // SAFETY: valid per the contract.
    let Some(it) = (unsafe { it.as_ref() }) else {
        return 0;
    };
    run(out, cap, |o| {
        itemtype::note_before_add(null != 0, &item(it), o);
    })
}

/// # Safety
/// Pointers are valid; `go` is writable; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_it_diary_before_add(
    null: i32,
    idx: i32,
    it: *const ItItem,
    go: *mut i32,
    out: *mut Ev,
    cap: usize,
) -> usize {
    // SAFETY: valid per the contract.
    let Some(it) = (unsafe { it.as_ref() }) else {
        return 0;
    };
    let mut g = false;
    let n = run(out, cap, |o| {
        g = itemtype::diary_before_add(null != 0, idx, &item(it), o)
    });
    // SAFETY: writable per the contract.
    unsafe { go.write(i32::from(g)) };
    n
}

/// # Safety
/// `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_it_diary_after(show: i32, out: *mut Ev, cap: usize) -> usize {
    run(out, cap, |o| itemtype::diary_after_script(show != 0, o))
}

/// # Safety
/// Pointers are valid; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_it_on_use(
    t: i32,
    it: *const ItItem,
    p: *const ItPlayer,
    out: *mut Ev,
    cap: usize,
) -> usize {
    // SAFETY: valid per the contract.
    let (Some(it), Some(p)) = (unsafe { it.as_ref() }, unsafe { p.as_ref() }) else {
        return 0;
    };
    run(out, cap, |o| itemtype::on_use(t, &item(it), &player(p), o))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_it_lantern_key(active: i32) -> Str {
    Str::of(itemtype::lantern_name_key(active != 0))
}

// ---- hints ----------------------------------------------------------------------------------

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_hint_init(
    s: *mut hint::HintState,
    y: f32,
    fx: f32,
    fy: f32,
    mul: f32,
) {
    let s = state!(s);
    hint::init(s, y, [fx, fy], mul);
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_hint_reset(s: *mut hint::HintState) {
    hint::reset(state!(s));
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_hint_set_active(s: *mut hint::HintState, on: i32) -> i32 {
    i32::from(hint::set_active(state!(s, 0), on != 0))
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_hint_update(s: *mut hint::HintState, dt: f32) {
    hint::update(state!(s), dt);
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_hint_can_add(
    s: *const hint::HintState,
    blocked: i32,
    given: i32,
) -> i32 {
    // SAFETY: valid per the contract.
    match unsafe { s.as_ref() } {
        Some(s) => i32::from(hint::can_add(s, blocked != 0, given != 0)),
        None => 0,
    }
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_hint_start(s: *mut hint::HintState, time: f32, duration: f32) {
    hint::start(state!(s), time, duration);
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_hint_drawable(s: *const hint::HintState, ptest: i32) -> i32 {
    // SAFETY: valid per the contract.
    match unsafe { s.as_ref() } {
        Some(s) => i32::from(hint::drawable(s, ptest != 0)),
        None => 0,
    }
}

/// # Safety
/// `s` is valid; `pos` is 7 floats (position, size marker, colour).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_hint_label(s: *const hint::HintState, out: *mut f32) {
    // SAFETY: valid per the contract.
    let Some(s) = (unsafe { s.as_ref() }) else {
        return;
    };
    let mut o = Out::default();
    hint::draw_label(s, &mut o);
    if let Some(e) = o.evs.first() {
        put(out, &e.v[..7]);
    }
}

/// # Safety
/// `s` is valid; `out` is 4 floats: x, y, z, centred.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_hint_row(
    s: *const hint::HintState,
    rows: usize,
    i: usize,
    out: *mut f32,
) {
    // SAFETY: valid per the contract.
    let Some(s) = (unsafe { s.as_ref() }) else {
        return;
    };
    put(out, &hint::row_pos(s, rows, i));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_hint_wrap_width() -> f32 {
    hint::WRAP_WIDTH
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_hint_wrap_line() -> f32 {
    hint::WRAP_LINE_HEIGHT
}

/// # Safety
/// `text` is `n` units; `replace` accepts `rctx`; `sink` accepts `sctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_strip_icons(
    text: *const u32,
    n: usize,
    replace: Replace,
    rctx: Ctx,
    sink: Sink32,
    sctx: Ctx,
) {
    unsafe extern "C" fn collect(ctx: Ctx, p: *const u32, n: usize) {
        // SAFETY: `ctx` is the `Vec` handed to `replace` below.
        let out = unsafe { &mut *ctx.cast::<Vec<u32>>() };
        out.extend_from_slice(units(p, n));
    }
    let r = hint::strip_icons(units(text, n), &mut |cmd, pos| {
        let mut out: Vec<u32> = Vec::new();
        // SAFETY: `replace` accepts `rctx`; `collect` accepts the `Vec`.
        unsafe {
            replace(
                rctx,
                cmd.as_ptr(),
                cmd.len(),
                pos,
                collect,
                std::ptr::from_mut(&mut out).cast(),
            )
        };
        out
    });
    // SAFETY: `sink` accepts `sctx`.
    unsafe { sink(sctx, r.as_ptr(), r.len()) };
}

// ---- messages -------------------------------------------------------------------------------

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_msg_reset(s: *mut MessageState) {
    msg::reset(state!(s));
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_msg_start_quest(s: *mut MessageState) {
    msg::start_quest_added(state!(s));
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_msg_set_message(s: *mut MessageState, time: f32, duration: f32) {
    msg::set_message(state!(s), time, duration);
}

/// # Safety
/// `s` is valid; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_msg_start_pause(
    s: *mut MessageState,
    yes_no: i32,
    out: *mut Ev,
    cap: usize,
) -> usize {
    let s = state!(s, 0);
    run(out, cap, |o| msg::start_pause_message(s, yes_no != 0, o))
}

/// # Safety
/// `s` is valid; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_msg_set_pause(
    s: *mut MessageState,
    on: i32,
    out: *mut Ev,
    cap: usize,
) -> usize {
    let s = state!(s, 0);
    run(out, cap, |o| msg::set_pause_active(s, on != 0, o))
}

/// # Safety
/// `s` is valid; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_msg_do_action(
    s: *mut MessageState,
    action: i32,
    pressed: i32,
    has_callback: i32,
    out: *mut Ev,
    cap: usize,
) -> usize {
    let s = state!(s, 0);
    run(out, cap, |o| {
        msg::do_action(s, action, pressed != 0, has_callback != 0, o)
    })
}

/// # Safety
/// `s` is valid; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_msg_update(
    s: *mut MessageState,
    dt: f32,
    out: *mut Ev,
    cap: usize,
) -> usize {
    let s = state!(s, 0);
    run(out, cap, |o| msg::update(s, dt, o))
}

/// # Safety
/// `s` is valid; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_msg_draw_message(
    s: *const MessageState,
    rows: usize,
    out: *mut Ev,
    cap: usize,
) -> usize {
    // SAFETY: valid per the contract.
    let Some(s) = (unsafe { s.as_ref() }) else {
        return 0;
    };
    run(out, cap, |o| msg::draw_message(s, rows, o))
}

/// # Safety
/// `s` is valid; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_msg_draw_pause(
    s: *const MessageState,
    rows: usize,
    out: *mut Ev,
    cap: usize,
) -> usize {
    // SAFETY: valid per the contract.
    let Some(s) = (unsafe { s.as_ref() }) else {
        return 0;
    };
    run(out, cap, |o| msg::draw_pause(s, rows, o))
}

/// # Safety
/// `s` is valid; `icon` is 2 floats or null; `center` 2 and `start` 3 floats; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_msg_draw_quest(
    s: *const MessageState,
    icon: *const f32,
    center: *const f32,
    start: *const f32,
    out: *mut Ev,
    cap: usize,
) -> usize {
    // SAFETY: valid per the contract.
    let Some(s) = (unsafe { s.as_ref() }) else {
        return 0;
    };
    let icon = if icon.is_null() {
        None
    } else {
        Some(vec2(icon))
    };
    run(out, cap, |o| {
        msg::draw_quest(s, icon, vec2(center), vec3(start), o)
    })
}

// ---- commentary icon ------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_comm_spin(y: f32, playing: i32, dt: f32) -> f32 {
    commentary::spin(y, playing != 0, dt)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_comm_set_playing(current: i32, next: i32) -> i32 {
    commentary::set_playing(current != 0, next != 0)
}

/// # Safety
/// Buffers are live; `s` accepts `c`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_comm_focus_text(
    talker: *const u8,
    tn: usize,
    topic: *const u8,
    pn: usize,
    s: Sink,
    c: Ctx,
) {
    sink(s, c, &commentary::focus_text(pb(talker, tn), pb(topic, pn)));
}

/// # Safety
/// Buffers are live; `s` accepts `c`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_comm_file(file: *const u8, n: usize, s: Sink, c: Ctx) {
    sink(s, c, &commentary::comment_file(pb(file, n)));
}

// ---- inventory ------------------------------------------------------------------------------

/// # Safety
/// `s` is valid; `bubbles` is 9 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_init(
    s: *mut InvState,
    fade_in: f32,
    fade_out: f32,
    bubbles: *const f32,
) {
    let s = state!(s);
    let b = [
        vec3(bubbles),
        vec3(bubbles.wrapping_add(3)),
        vec3(bubbles.wrapping_add(6)),
    ];
    inv::init(s, fade_in, fade_out, b);
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_reset(s: *mut InvState) {
    inv::reset(state!(s));
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_reset_session(s: *mut InvState) {
    inv::reset_session(state!(s));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_should_drop(left: i32, primary: i32) -> i32 {
    i32::from(inv::should_drop_picked(left != 0, primary != 0))
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_update_alpha(s: *mut InvState, dt: f32) -> i32 {
    inv::update_alpha(state!(s, 0), dt)
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_update_texts(s: *mut InvState, dt: f32) {
    inv::update_texts(state!(s), dt);
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_exit_pressed(s: *mut InvState) -> i32 {
    inv::exit_pressed(state!(s, 0))
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_open_journal(s: *mut InvState) -> i32 {
    inv::open_journal(state!(s, 0))
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_pauses_voices(s: *const InvState) -> i32 {
    // SAFETY: valid per the contract.
    i32::from(unsafe { s.as_ref() }.is_some_and(inv::pauses_voices_on_enter))
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_leave(s: *mut InvState) -> i32 {
    i32::from(inv::leave(state!(s, 0)))
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_set_desc(
    s: *mut InvState,
    header_empty: i32,
    normal_empty: i32,
) -> i32 {
    i32::from(inv::set_desc_text(
        state!(s, 0),
        header_empty != 0,
        normal_empty != 0,
    ))
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_set_message(s: *mut InvState, life: f32, duration: f32) {
    inv::set_message(state!(s), life, duration);
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_set_picked(s: *mut InvState, t: f64) {
    inv::set_picked(state!(s), t);
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_note_current(s: *mut InvState, t: f64) {
    inv::note_current_widget(state!(s), t);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_status_index(x: f32) -> i32 {
    inv::status_index(x)
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_glow_alpha(s: *const InvState, idx: usize) -> f32 {
    // SAFETY: valid per the contract.
    unsafe { s.as_ref() }.map_or(0.0, |s| inv::status_glow_alpha(s, idx))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_add_plan(
    ty: i32,
    sub: *const u8,
    subn: usize,
    existing: i32,
    count: i32,
) -> i32 {
    inv::add_plan(&inv::AddIn {
        ty,
        subtype: pb(sub, subn),
        existing: existing != 0,
        existing_count: count,
    })
}

/// # Safety
/// `cur` is `n` bytes; `s` accepts `c`; `unlock` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_tinderbox(
    cur: *const u8,
    n: usize,
    unlock: *mut i32,
    s: Sink,
    c: Ctx,
) {
    let (v, u) = inv::tinderbox_count(pb(cur, n));
    // SAFETY: writable per the contract.
    unsafe { unlock.write(i32::from(u)) };
    sink(s, c, &v);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_still_alive(p: *const u8, n: usize) -> i32 {
    i32::from(inv::is_still_alive_item(pb(p, n)))
}

/// # Safety
/// `p` is `n` bytes; the sinks accept their contexts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_item_strings(
    sub: *const u8,
    sn: usize,
    image: *const u8,
    inn: usize,
    s: Sink,
    c0: Ctx,
    c1: Ctx,
    c2: Ctx,
) {
    sink(s, c0, &inv::game_name_entry(pb(sub, sn)));
    sink(s, c1, &inv::game_desc_entry(pb(sub, sn)));
    sink(s, c2, &inv::full_image_name(pb(image, inn)));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_remove_one(has_count: i32, after: i32) -> i32 {
    i32::from(inv::remove_one(has_count != 0, after))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_combine_matches(a: Str, b: Str, x: Str, y: Str) -> i32 {
    i32::from(inv::combine_matches(bytes(a), bytes(b), bytes(x), bytes(y)))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_special_combine(a: i32, b: i32) -> i32 {
    inv::special_combine(a, b)
}

/// # Safety
/// `s` accepts `c`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_script_cmd(f: Str, a: Str, b: Str, s: Sink, c: Ctx) {
    sink(s, c, &inv::run_script_command(bytes(f), bytes(a), bytes(b)));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_slot_up(item: i32, picked: i32, same: i32) -> i32 {
    inv::slot_mouse_up(item != 0, picked != 0, same != 0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_slot_enter(item: i32, message: i32, picked: i32, same: i32) -> i32 {
    inv::slot_mouse_enter(item != 0, message != 0, picked != 0, same != 0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_slot_leave(item: i32, current_none: i32) -> i32 {
    i32::from(inv::slot_mouse_leave_clears(item != 0, current_none != 0))
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_slot_release(
    s: *const InvState,
    button: i32,
    same: i32,
    t: f64,
) -> i32 {
    // SAFETY: valid per the contract.
    unsafe { s.as_ref() }.map_or(0, |s| inv::slot_ui_release(s, button, same != 0, t))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_slot_glow(glow: f32, hovered: i32, dt: f32) -> f32 {
    inv::slot_glow_update(glow, hovered != 0, dt)
}

/// # Safety
/// Vector pointers are 3/2 floats; `out` is 2 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_pick_offset(
    mouse_mode: i32,
    gpos: *const f32,
    size: *const f32,
    image: *const f32,
    mouse: *const f32,
    out: *mut f32,
) {
    let r = if mouse_mode != 0 {
        inv::pick_offset_mouse(vec3(gpos), vec2(size), vec2(image), vec2(mouse))
    } else {
        inv::pick_offset_button(vec3(gpos), vec2(size), vec2(image))
    };
    put(out, &r);
}

#[repr(C)]
pub struct SlotDraw {
    pub pos: [f32; 3],
    pub glow: i32,
    pub glow_alpha: f32,
    pub glow_pos: [f32; 3],
    pub count_pos: [f32; 3],
}

/// # Safety
/// Pointers are valid; vectors are 3/2 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_slot_draw(
    s: *const InvState,
    gpos: *const f32,
    size: *const f32,
    image: *const f32,
    glow: f32,
    pulse: *const f32,
    count_offset: *const f32,
    out: *mut SlotDraw,
) {
    // SAFETY: valid per the contract.
    let Some(s) = (unsafe { s.as_ref() }) else {
        return;
    };
    let d = inv::slot_draw(
        s,
        vec3(gpos),
        vec2(size),
        vec2(image),
        glow,
        vec3(pulse),
        vec3(count_offset),
    );
    // SAFETY: writable per the contract.
    unsafe {
        out.write(SlotDraw {
            pos: d.pos,
            glow: i32::from(d.glow),
            glow_alpha: d.glow_alpha,
            glow_pos: d.glow_pos,
            count_pos: d.count_pos,
        });
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_nav(button: i32, user: i32) -> i32 {
    inv::ui_button_nav(button, user)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_mouse_over(
    fader: i32,
    message: i32,
    current: i32,
    active: i32,
    same: i32,
) -> i32 {
    inv::mouse_over(fader, message != 0, current != 0, active != 0, same != 0)
}

/// # Safety
/// Corner sizes are 2 floats each; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_draw_frame(
    pos: *const f32,
    size: *const f32,
    c0: *const f32,
    c1: *const f32,
    c3: *const f32,
    alpha: f32,
    out: *mut Ev,
    cap: usize,
) -> usize {
    run(out, cap, |o| {
        inv::draw_frame(
            vec3(pos),
            vec2(size),
            vec2(c0),
            vec2(c1),
            vec2(c3),
            alpha,
            o,
        );
    })
}

#[repr(C)]
pub struct FrameIn {
    pub widget_pos: [f32; 3],
    pub widget_size: [f32; 2],
    pub hpad: [f32; 2],
    pub vpad: [f32; 2],
    pub offset: [f32; 3],
    pub size: [f32; 2],
    pub c0: [f32; 2],
    pub c1: [f32; 2],
    pub c3: [f32; 2],
    pub alpha: f32,
}

/// # Safety
/// `f` is valid; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_frame_around(
    f: *const FrameIn,
    out: *mut Ev,
    cap: usize,
) -> usize {
    // SAFETY: valid per the contract.
    let Some(f) = (unsafe { f.as_ref() }) else {
        return 0;
    };
    let f = inv::FrameIn {
        widget_pos: f.widget_pos,
        widget_size: f.widget_size,
        hpad: f.hpad,
        vpad: f.vpad,
        offset: f.offset,
        size: f.size,
        c0: f.c0,
        c1: f.c1,
        c3: f.c3,
        alpha: f.alpha,
    };
    run(out, cap, |o| inv::draw_frame_around(&f, o))
}

#[repr(C)]
pub struct GridIn {
    pub widget_size: [f32; 2],
    pub h_ext: [f32; 2],
    pub v_ext: [f32; 2],
    pub separation: [f32; 3],
    pub slot_size: [f32; 2],
    pub start: [f32; 3],
    pub frame_hpad: [f32; 2],
    pub frame_vpad: [f32; 2],
    pub frame_offset: [f32; 3],
    pub grid_offset: [f32; 3],
    pub color: [f32; 4],
    pub rows: i32,
    pub cols: i32,
    pub alpha: f32,
}

/// # Safety
/// `g` is valid; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_grid(g: *const GridIn, out: *mut Ev, cap: usize) -> usize {
    // SAFETY: valid per the contract.
    let Some(g) = (unsafe { g.as_ref() }) else {
        return 0;
    };
    let g = inv::GridIn {
        widget_size: g.widget_size,
        h_ext: g.h_ext,
        v_ext: g.v_ext,
        separation: g.separation,
        slot_size: g.slot_size,
        start: g.start,
        frame_hpad: g.frame_hpad,
        frame_vpad: g.frame_vpad,
        frame_offset: g.frame_offset,
        grid_offset: g.grid_offset,
        color: g.color,
        rows: g.rows,
        cols: g.cols,
        alpha: g.alpha,
    };
    run(out, cap, |o| inv::draw_grid(&g, o))
}

/// # Safety
/// Vectors are 3/2/4 floats; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_desc_rows(
    center: *const f32,
    width: f32,
    font: *const f32,
    rows: usize,
    color: *const f32,
    alpha: f32,
    out: *mut Ev,
    cap: usize,
) -> usize {
    let c = [
        // SAFETY: 4 floats per the contract.
        unsafe { *color },
        unsafe { *color.add(1) },
        unsafe { *color.add(2) },
        unsafe { *color.add(3) },
    ];
    run(out, cap, |o| {
        inv::draw_desc_rows(vec3(center), width, vec2(font), rows, c, alpha, o)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_wrap_line(font_h: f32) -> f32 {
    inv::wrap_line_height([0.0, font_h])
}

/// # Safety
/// Vectors are 2/3 floats; `out` is 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_picked_pos(
    offset: *const f32,
    mouse_down: i32,
    mouse: *const f32,
    wpos: *const f32,
    wsize: *const f32,
    out: *mut f32,
) {
    put(
        out,
        &inv::picked_pos(
            vec2(offset),
            mouse_down != 0,
            vec2(mouse),
            vec3(wpos),
            vec2(wsize),
        ),
    );
}

/// # Safety
/// `center` is 3 floats; `pos` 3 and `size` 2 are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_message_bar(
    start_x: f32,
    width: f32,
    center: *const f32,
    font_h: f32,
    pos: *mut f32,
    size: *mut f32,
) {
    let (p, s) = inv::message_bar(start_x, width, vec3(center), font_h);
    put(pos, &p);
    put(size, &s);
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_message_alpha(s: *const InvState) -> f32 {
    // SAFETY: valid per the contract.
    unsafe { s.as_ref() }.map_or(0.0, inv::message_alpha)
}

/// # Safety
/// Vectors are 3/2 floats; `out` is 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_text_frame_pos(
    header: *const f32,
    frame_x: f32,
    c0: *const f32,
    hpad_x: f32,
    vpad_y: f32,
    offset: *const f32,
    out: *mut f32,
) {
    put(
        out,
        &inv::text_frame_pos(
            vec3(header),
            frame_x,
            vec2(c0),
            hpad_x,
            vpad_y,
            vec3(offset),
        ),
    );
}

/// # Safety
/// `out` is 2 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_text_frame_default(width: f32, out: *mut f32) {
    put(out, &inv::item_text_frame_default(width));
}

/// # Safety
/// Vectors are 3/2 floats; `pos` 3, `sz` 2, `col` 4 are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_element_at_center(
    center: *const f32,
    image: *const f32,
    size: *const f32,
    alpha: f32,
    mul: i32,
    pos: *mut f32,
    sz: *mut f32,
    col: *mut f32,
) {
    let (p, s, c) = inv::element_at_center(vec3(center), vec2(image), vec2(size), alpha, mul != 0);
    put(pos, &p);
    put(sz, &s);
    put(col, &c);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_inv_oil_height(oil: f32, h: f32) -> f32 {
    inv::oil_remaining_height(oil, h)
}

/// # Safety
/// `s` valid; vectors 3/2 floats; `bubbles` is 3 sizes of 2 floats; `out` is `cap` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_draw_oil(
    s: *const InvState,
    center: *const f32,
    wpos: *const f32,
    wsize: *const f32,
    oil: f32,
    bubbles: *const f32,
    out: *mut Ev,
    cap: usize,
) -> usize {
    // SAFETY: valid per the contract.
    let Some(s) = (unsafe { s.as_ref() }) else {
        return 0;
    };
    let sizes = [
        vec2(bubbles),
        vec2(bubbles.wrapping_add(2)),
        vec2(bubbles.wrapping_add(4)),
    ];
    let d = inv::OilDraw {
        center: vec3(center),
        widget_pos: vec3(wpos),
        widget_size: vec2(wsize),
        lamp_oil: oil,
        bubble_sizes: &sizes,
    };
    run(out, cap, |o| inv::draw_oil(s, &d, o))
}

/// # Safety
/// `s` valid; vectors 3/2 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_oil_update(
    s: *mut InvState,
    wpos: *const f32,
    wsize: *const f32,
    oil: f32,
    dt: f32,
) -> i32 {
    let s = state!(s, 1);
    i32::from(inv::oil_update(s, vec3(wpos), vec2(wsize), oil, dt))
}

/// # Safety
/// Vectors are 3/2 floats; `out` is 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_inv_status_pos(
    center: *const f32,
    image: *const f32,
    out: *mut f32,
) {
    put(out, &inv::status_widget_pos(vec3(center), vec2(image)));
}

// ---- journal --------------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_pages_new() -> *mut Pages {
    Box::into_raw(Box::default())
}

/// # Safety
/// `p` came from `ohpl_ga8_jrn_pages_new` and is freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_pages_free(p: *mut Pages) {
    if !p.is_null() {
        // SAFETY: allocated by `pages_new`.
        drop(unsafe { Box::from_raw(p) });
    }
}

/// `rows` are `n` wrapped rows given as `(pointer, length)` pairs.
fn rows_in(p: *const *const u32, lens: *const usize, n: usize) -> Vec<Vec<u32>> {
    (0..n)
        .map(|i| {
            // SAFETY: the caller passes `n` pointers and lengths.
            let (r, l) = unsafe { (*p.add(i), *lens.add(i)) };
            units(r, l).to_vec()
        })
        .collect()
}

/// # Safety
/// `pg` is valid; `rows`/`lens` are `n` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_load_text(
    pg: *mut Pages,
    rows: *const *const u32,
    lens: *const usize,
    n: usize,
    max_rows: i32,
) {
    let pg = state!(pg);
    pg.load_text(&rows_in(rows, lens, n), max_rows);
}

/// # Safety
/// `pg` is valid; `rows`/`lens` are `n` entries; `s` accepts `c` (called once per bad line with
/// the line text, `arg_count` is written through `cnt` before each call).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_load_narration(
    pg: *mut Pages,
    rows: *const *const u32,
    lens: *const usize,
    n: usize,
    cnt: *mut usize,
    s: Sink,
    c: Ctx,
) {
    let pg = state!(pg);
    for (row, args) in pg.load_narration(&rows_in(rows, lens, n)) {
        // SAFETY: writable per the contract.
        unsafe { cnt.write(args) };
        sink(s, c, &row);
    }
}

/// # Safety
/// `pg` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_page_count(pg: *const Pages) -> usize {
    // SAFETY: valid per the contract.
    unsafe { pg.as_ref() }.map_or(0, |p| p.pages.len())
}

/// # Safety
/// `pg` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_row_count(pg: *const Pages, page: usize) -> usize {
    // SAFETY: valid per the contract.
    unsafe { pg.as_ref() }
        .and_then(|p| p.pages.get(page))
        .map_or(0, |p| p.rows.len())
}

/// # Safety
/// `pg` is valid; `n` is writable. The row stays valid until the next load.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_row(
    pg: *const Pages,
    page: usize,
    row: usize,
    n: *mut usize,
) -> *const u32 {
    // SAFETY: valid per the contract.
    let r = unsafe { pg.as_ref() }
        .and_then(|p| p.pages.get(page))
        .and_then(|p| p.rows.get(row));
    // SAFETY: writable per the contract.
    unsafe { n.write(r.map_or(0, Vec::len)) };
    r.map_or(std::ptr::null(), |r| r.as_ptr())
}

/// # Safety
/// `pg` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_voice(pg: *const Pages, page: usize) -> Str {
    // SAFETY: valid per the contract.
    unsafe { pg.as_ref() }
        .and_then(|p| p.pages.get(page))
        .map_or(Str::NONE, |p| Str::of(&p.voice))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_background(state: i32) -> Str {
    Str::of(jrn::background(state))
}

/// # Safety
/// `alpha` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_update_alpha(
    alpha: *mut f32,
    active: i32,
    from_inv: i32,
    dt: f32,
) -> i32 {
    // SAFETY: writable per the contract.
    match unsafe { alpha.as_mut() } {
        Some(a) => jrn::update_alpha(a, active != 0, from_inv != 0, dt),
        None => 0,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_state_alpha(alpha: f32, current: i32, dt: f32) -> f32 {
    jrn::state_alpha(alpha, current != 0, dt)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_hover_alpha(alpha: f32, hovered: i32, dt: f32) -> f32 {
    jrn::hover_alpha(alpha, hovered != 0, dt)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_narration_done(state: i32, active: i32, valid: i32) -> i32 {
    i32::from(jrn::narration_done(state, active != 0, valid != 0))
}

/// # Safety
/// `arg` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_exit_pressed(
    active: i32,
    instant: i32,
    force: i32,
    state: i32,
    page: i32,
    pages: usize,
    arg: *mut i32,
) -> i32 {
    let (a, v) = jrn::exit_pressed(active != 0, instant != 0, force != 0, state, page, pages);
    // SAFETY: writable per the contract.
    unsafe { arg.write(v) };
    a
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_list_index(state: i32) -> i32 {
    jrn::list_index(state)
}

/// # Safety
/// `s` accepts the contexts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_entries(
    kind: i32,
    name: Str,
    num: i32,
    s: Sink,
    c0: Ctx,
    c1: Ctx,
) {
    let (a, b) = match kind {
        0 => jrn::note_entries(bytes(name)),
        1 => jrn::diary_entries(bytes(name), num),
        _ => jrn::quest_entries(bytes(name)),
    };
    sink(s, c0, &a);
    sink(s, c1, &b);
}

/// # Safety
/// `s` accepts the contexts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_image_files(image: Str, s: Sink, c0: Ctx, c1: Ctx) {
    let (a, b) = jrn::image_files(bytes(image));
    sink(s, c0, &a);
    sink(s, c1, &b);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_archivist(game: Str, notes: usize) -> i32 {
    jrn::archivist(bytes(game), notes)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_disable_quest(found: i32, active: i32) -> i32 {
    jrn::disable_quest(found != 0, active != 0)
}

/// # Safety
/// `items` is `n` identities.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_find_index(
    items: *const usize,
    n: usize,
    target: usize,
) -> i32 {
    let v = if n == 0 || items.is_null() {
        &[]
    } else {
        // SAFETY: `n` readable entries per the contract.
        unsafe { std::slice::from_raw_parts(items, n) }
    };
    jrn::find_index(v, target)
}

/// # Safety
/// `counts` is `nc` sizes and `items` their sum of identities; `cat`/`entry` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_find_diary(
    counts: *const usize,
    nc: usize,
    items: *const usize,
    ni: usize,
    target: usize,
    cat: *mut i32,
    entry: *mut i32,
) {
    let sl = |p: *const usize, n: usize| -> &[usize] {
        if n == 0 || p.is_null() {
            &[]
        } else {
            // SAFETY: `n` readable entries per the contract.
            unsafe { std::slice::from_raw_parts(p, n) }
        }
    };
    let (c, e) = jrn::find_diary(sl(counts, nc), sl(items, ni), target);
    // SAFETY: writable per the contract.
    unsafe {
        cat.write(c);
        entry.write(e);
    }
}

/// # Safety
/// `counts` is `nc` sizes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_last_read(
    kind: i32,
    cat: i32,
    entry: i32,
    notes: usize,
    counts: *const usize,
    nc: usize,
) -> i32 {
    let v = if nc == 0 || counts.is_null() {
        &[]
    } else {
        // SAFETY: `nc` readable entries per the contract.
        unsafe { std::slice::from_raw_parts(counts, nc) }
    };
    jrn::last_read(kind, cat, entry, notes, v)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_note_arrows(page: i32, pages: usize, back: *mut i32, fwd: *mut i32) {
    let (b, f) = jrn::note_arrows(page, pages);
    // SAFETY: writable per the contract.
    unsafe {
        back.write(i32::from(b));
        fwd.write(i32::from(f));
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_narrated_plays(page: i32, pages: usize) -> i32 {
    i32::from(jrn::narrated_page_plays(page, pages))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_page_start_y(rows: usize, dist: f32) -> f32 {
    jrn::text_page_start_y(rows, dist)
}

/// # Safety
/// `pos` is 3 and `back_x` 1 writable floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_nav_positions(
    width: f32,
    start_y: f32,
    max_rows: i32,
    dist: f32,
    pos: *mut f32,
    back_x: *mut f32,
) {
    let (p, b) = jrn::nav_positions(width, start_y, max_rows, dist);
    put(pos, &p);
    put(back_x, &[b]);
}

/// # Safety
/// `page` and `y` are `entries` writable entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_layout_list(
    entries: usize,
    page: *mut usize,
    y: *mut f32,
) -> usize {
    let (v, pages) = jrn::layout_list(entries);
    for (i, p) in v.iter().enumerate() {
        // SAFETY: i < entries.
        unsafe {
            page.add(i).write(p.page);
            y.add(i).write(p.y);
        }
    }
    pages
}

/// # Safety
/// `rows` is `n` row counts; `page` and `y` are `n` writable entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_layout_quests(
    rows: *const usize,
    n: usize,
    page: *mut usize,
    y: *mut f32,
) -> usize {
    let r = if n == 0 || rows.is_null() {
        &[]
    } else {
        // SAFETY: `n` readable entries per the contract.
        unsafe { std::slice::from_raw_parts(rows, n) }
    };
    let (v, pages) = jrn::layout_quests(r);
    for (i, p) in v.iter().enumerate() {
        // SAFETY: i < n.
        unsafe {
            page.add(i).write(p.page);
            y.add(i).write(p.y);
        }
    }
    pages
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_start_page(pages: usize) -> i32 {
    jrn::start_page(pages)
}

/// # Safety
/// The out pointers are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_list_page(
    page: i32,
    pages: usize,
    state: i32,
    no_pages: *mut i32,
    back: *mut i32,
    fwd: *mut i32,
    default_return: *mut i32,
) {
    let r = jrn::set_list_page(&jrn::ListPageIn { page, pages, state });
    // SAFETY: writable per the contract.
    unsafe {
        no_pages.write(i32::from(r.no_pages));
        back.write(i32::from(r.back));
        fwd.write(i32::from(r.forward));
        default_return.write(i32::from(r.default_is_return));
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_main_click(user: i32) -> i32 {
    jrn::main_click(user)
}

/// # Safety
/// `kind` and `delta` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_arrow_click(
    user: i32,
    kind: *mut i32,
    delta: *mut i32,
) -> i32 {
    match jrn::arrow_click(user) {
        Some((k, d)) => {
            // SAFETY: writable per the contract.
            unsafe {
                kind.write(k);
                delta.write(d);
            }
            1
        }
        None => 0,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_ui_button(kind: i32, button: i32, back: i32, fwd: i32) -> i32 {
    jrn::ui_button(kind, button, back != 0, fwd != 0).unwrap_or(-1)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_ui_arrow(
    kind: i32,
    arrow: i32,
    back: i32,
    fwd: i32,
    down_ok: i32,
) -> i32 {
    jrn::ui_arrow(kind, arrow, back != 0, fwd != 0, down_ok != 0).unwrap_or(-1)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_main_ui_button(button: i32) -> i32 {
    jrn::main_ui_button(button)
}

/// # Safety
/// `out` is 3 writable floats: t, x_add, alpha.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_hover_effect(
    pulse: f32,
    color_alpha: f32,
    effect_alpha: f32,
    out: *mut f32,
) {
    let e = jrn::hover_effect(pulse, color_alpha, effect_alpha);
    put(out, &[e.t, e.x_add, e.alpha]);
}

/// # Safety
/// `gpos` is 3 floats; `out` is 3 writable floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_effect_pos(
    centered: i32,
    gpos: *const f32,
    size_x: f32,
    x_add: f32,
    out: *mut f32,
) {
    let p = if centered != 0 {
        jrn::centered_effect_pos(vec3(gpos), size_x, x_add)
    } else {
        jrn::left_effect_pos(vec3(gpos), size_x)
    };
    put(out, &p);
}

/// # Safety
/// `font` is 2 floats; `out` is 2 writable floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_jrn_effect_size(font: *const f32, x_add: f32, out: *mut f32) {
    put(out, &jrn::effect_size(vec2(font), x_add));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_fade_alpha(alpha: f32) -> f32 {
    jrn::fade_alpha(alpha)
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<InvState>() == 128);
    assert!(size_of::<hint::HintState>() == 52);
    assert!(size_of::<MessageState>() == 60);
    assert!(size_of::<Ev>() == 88);
    assert!(size_of::<SlotDraw>() == 44);
};

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_msg_wrap_width(pause: i32) -> f32 {
    if pause != 0 {
        msg::PAUSE_WRAP_WIDTH
    } else {
        msg::MESSAGE_WRAP_WIDTH
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_msg_line_height(font_h: f32) -> f32 {
    font_h + 2.0
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_is_open(state: i32) -> i32 {
    i32::from(jrn::is_open_state(state))
}
