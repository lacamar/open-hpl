//! C ABI of the H15 physics module and surface data, mirrored in `RustPhysBook.h`.

use super::host::Str;
use super::preg::{self, PhysState};
use super::surf::{self, SurfaceState};
use super::surface::SurfaceParams;
use super::wreg::WorldOp;

/// # Safety
/// `p` points to `n` readable bytes, or `n` is 0.
unsafe fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    // SAFETY: caller contract.
    unsafe { Str { p, n }.bytes() }
}

/// # Safety
/// `state` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15p_new(op: WorldOp, me: usize, state: *mut PhysState) -> u64 {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { state.as_mut() } {
        *s = PhysState::default();
    }
    preg::new(op, me)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15p_free(book: u64) {
    preg::free(book);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15p_create_world(book: u64, add: bool) -> usize {
    preg::create_world(book, add)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15p_destroy_world(book: u64, w: usize) {
    preg::destroy_world(book, w);
}

/// # Safety
/// `name` points to `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15p_create_surface(book: u64, name: *const u8, n: usize) -> usize {
    // SAFETY: caller contract.
    preg::create_surface(book, unsafe { bytes(name, n) })
}

/// # Safety
/// `name` points to `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15p_get_surface(book: u64, name: *const u8, n: usize) -> usize {
    // SAFETY: caller contract.
    preg::get_surface(book, unsafe { bytes(name, n) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15p_surface_len(book: u64) -> usize {
    preg::surface_len(book)
}

/// # Safety
/// `name` and `obj` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15p_surface_at(
    book: u64,
    i: usize,
    name: *mut Str,
    obj: *mut usize,
) -> bool {
    let Some((p, n, h)) = preg::surface_at(book, i) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        if let Some(o) = name.as_mut() {
            *o = Str { p, n };
        }
        if let Some(o) = obj.as_mut() {
            *o = h;
        }
    }
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15p_can_play(book: u64, max: i32) -> bool {
    preg::can_play(book, max)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15p_add_impact(book: u64) {
    preg::add_impact(book);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15p_impact_count(book: u64) -> usize {
    preg::impact_count(book)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15p_update_impacts(book: u64, dt: f32, duration: f32) {
    preg::update_impacts(book, dt, duration);
}

/// # Safety
/// `path` points to `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15p_load_surface(book: u64, path: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    preg::load_surface(book, unsafe { bytes(path, n) })
}

/// # Safety
/// `state` is writable and embedded in the surface that owns the returned record.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15s_new(op: WorldOp, me: usize, state: *mut SurfaceState) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { state.as_mut() } {
        *s = surf::new(op, me);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15s_free(rs: u64) {
    surf::free(rs);
}

/// # Safety
/// `state` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15s_set_f32(state: *mut SurfaceState, id: i32, v: f32) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { state.as_mut() } {
        s.set_f32(id, v);
    }
}

/// # Safety
/// `state` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15s_get_f32(state: *mut SurfaceState, id: i32) -> f32 {
    // SAFETY: caller contract.
    unsafe { state.as_mut() }.map_or(0.0, |s| s.get_f32(id))
}

/// # Safety
/// `state` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15s_set_i32(state: *mut SurfaceState, id: i32, v: i32) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { state.as_mut() } {
        s.set_i32(id, v);
    }
}

/// # Safety
/// `state` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15s_get_i32(state: *const SurfaceState, id: i32) -> i32 {
    // SAFETY: caller contract.
    unsafe { state.as_ref() }.map_or(0, |s| s.get_i32(id))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15s_create_impact(rs: u64, min_speed: f32, hit: bool) -> usize {
    surf::create_impact(rs, min_speed, hit)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15s_impact_at(rs: u64, hit: bool, i: i32) -> usize {
    surf::impact_at(rs, hit, i)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15s_impact_num(rs: u64, hit: bool) -> i32 {
    surf::impact_num(rs, hit)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15s_impact_from_speed(rs: u64, hit: bool, speed: f32) -> usize {
    surf::impact_from_speed(rs, hit, speed)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15s_preload(rs: u64) {
    surf::preload_data(rs);
}

/// # Safety
/// `state` is live; `name` points to `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15s_to_material(
    rs: u64,
    state: *const SurfaceState,
    name: *const u8,
    n: usize,
    world: usize,
) -> usize {
    // SAFETY: caller contract.
    let (Some(st), name) = (unsafe { state.as_ref() }, unsafe { bytes(name, n) }) else {
        return 0;
    };
    surf::to_material(rs, st, name, world)
}

/// # Safety
/// `state` is live; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15s_params(
    state: *const SurfaceState,
    me: usize,
    physics: usize,
    scrape: Str,
    roll: Str,
    out: *mut SurfaceParams,
) {
    // SAFETY: caller contract.
    if let (Some(st), Some(out)) = unsafe { (state.as_ref(), out.as_mut()) } {
        *out = surf::params(st, me, physics, scrape, roll);
    }
}
