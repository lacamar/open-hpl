//! C ABI of the journal shell (`amnesia/src/game/LuxJournal.h`): lifecycle, content calls, save-data access and the
//! widget callback entry. Notes and diaries are arena handle bits (0 = null) and are never dereferenced in C++.

use super::event::Str;
use super::jrn::{self, Entry, Quest};
use std::ffi::c_void;

type Ctx = *mut c_void;
type Sink = unsafe extern "C" fn(Ctx, *const u8, usize);

fn b<'a>(s: Str) -> &'a [u8] {
    if s.n == 0 || s.p.is_null() {
        return &[];
    }
    // SAFETY: the caller passes a live `n`-byte buffer.
    unsafe { std::slice::from_raw_parts(s.p, s.n) }
}

fn sink(s: Sink, c: Ctx, v: &[u8]) {
    // SAFETY: `s` accepts `c`.
    unsafe { s(c, v.as_ptr(), v.len()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_new() {
    jrn::create();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_free() {
    jrn::destroy();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_load_fonts() {
    jrn::load_fonts();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_reset() {
    jrn::reset();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_update(dt: f32) {
    if !super::dump::c_on() {
        jrn::update(dt);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_on_enter() {
    jrn::on_enter();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_on_leave() {
    jrn::on_leave();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_on_draw(frame_time: f32) {
    super::dump::c_tick();
    if super::dump::c_on() {
        jrn::update(super::dump::DT);
    }
    jrn::on_draw(frame_time);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_get_set() -> usize {
    jrn::get_set()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_exit_pressed(instant: i32) {
    jrn::exit_pressed(instant != 0);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_exit() {
    jrn::exit();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_set_force_instant_exit(on: i32) {
    jrn::set_force_instant_exit(on != 0);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_change_state(state: i32) {
    jrn::change_state(state);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_open_last_read() {
    jrn::open_last_read();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_open_note_idx(i: i32, narration: i32) {
    jrn::open_note_idx(i, narration != 0);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_open_diary_idx(cat: i32, i: i32, narration: i32) {
    jrn::open_diary_idx(cat, i, narration != 0);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_add_note(entry: Str, image: Str) {
    jrn::add_note(b(entry), b(image));
}

/// Returns the diary number inside its container.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_add_diary(entry: Str, image: Str) -> i32 {
    jrn::add_diary(b(entry), b(image)).map_or(0, |d| d.1)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_add_quest(name: Str, entry: Str) -> i32 {
    i32::from(jrn::add_quest(b(name), b(entry)))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_disable_quest(name: Str) -> i32 {
    i32::from(jrn::disable_quest(b(name)))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_quest_state(name: Str) -> i32 {
    jrn::quest_state(b(name))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_cb(token: usize, w: usize, lval: i32, _fval: f32) -> i32 {
    i32::from(jrn::dispatch(token, w, lval))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_last_read(cat: *mut i32, entry: *mut i32, ty: *mut i32) {
    let (c, e, t) = jrn::last_read();
    for (p, v) in [(cat, c), (entry, e), (ty, t)] {
        if !p.is_null() {
            // SAFETY: the caller passes writable ints.
            unsafe { *p = v };
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_note_count() -> usize {
    jrn::note_count()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_note_field(n: usize, f: i32, s: Sink, c: Ctx) {
    sink(s, c, &jrn::note_field(n, f));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_cont_count() -> usize {
    jrn::cont_count()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_cont_type(n: usize, s: Sink, c: Ctx) {
    sink(s, c, &jrn::cont_type(n));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_diary_count(cont: usize) -> usize {
    jrn::diary_count(cont)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_diary_field(cont: usize, n: usize, f: i32, s: Sink, c: Ctx) {
    sink(s, c, &jrn::diary_field(cont, n, f));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_quest_count() -> usize {
    jrn::quest_count()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_quest_field(n: usize, f: i32, s: Sink, c: Ctx) {
    sink(s, c, &jrn::quest_field(n, f));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_quest_active(n: usize) -> i32 {
    i32::from(jrn::quest_active(n))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_load_begin(cat: i32, entry: i32, ty: i32) {
    jrn::load_begin(cat, entry, ty);
}

fn entry(name: Str, text: Str, icon: Str, image: Str) -> Entry {
    Entry {
        name: b(name).to_vec(),
        text: b(text).to_vec(),
        icon: b(icon).to_vec(),
        image: b(image).to_vec(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_load_note(name: Str, text: Str, icon: Str, image: Str) {
    jrn::load_note(entry(name, text, icon, image));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_load_container(ty: Str) {
    jrn::load_container(b(ty).to_vec());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_load_diary(name: Str, text: Str, icon: Str, image: Str) {
    jrn::load_diary(entry(name, text, icon, image));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_jrn_load_quest(active: i32, name: Str, name_entry: Str, text_entry: Str) {
    jrn::load_quest(Quest {
        active: active != 0,
        name: b(name).to_vec(),
        name_entry: b(name_entry).to_vec(),
        text_entry: b(text_entry).to_vec(),
    });
}
