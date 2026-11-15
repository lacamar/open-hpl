//! C ABI of the GA8 UI layer, mirrored in `amnesia/src/game/RustLuxUi.h`. Functions that produce
//! events write them to a caller array and return the count; the strings inside stay valid until
//! the next call on the same thread, so the caller copies them before running any event.

use super::event::{Ev, Str};
use super::journal::{self as jrn, Pages};
use super::{commentary, keyentry};
use std::ffi::c_void;

type Ctx = *mut c_void;
type Sink = unsafe extern "C" fn(Ctx, *const u8, usize);

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
    assert!(size_of::<Ev>() == 88);
};

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_jrn_is_open(state: i32) -> i32 {
    i32::from(jrn::is_open_state(state))
}
