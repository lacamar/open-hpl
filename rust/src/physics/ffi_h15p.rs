use super::host::Str;
use super::preg::{self, PhysState};
use super::surf::{self, SurfaceState};
use super::surface::SurfaceParams;
use super::wreg::WorldOp;

unsafe fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    unsafe { Str { p, n }.bytes() }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15p_new(op: WorldOp, me: usize, state: *mut *mut PhysState) -> u64 {
    let (book, s) = preg::new(op, me);
    if let Some(o) = unsafe { state.as_mut() } {
        *o = s;
    }
    book
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15p_free(book: u64) {
    preg::free(book);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15p_surface_len(book: u64) -> usize {
    preg::surface_len(book)
}

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
pub extern "C" fn ohpl_h15s_new(op: WorldOp, me: usize) -> *mut SurfaceState {
    surf::new(op, me)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15s_free(state: *const SurfaceState) {
    if let Some(s) = unsafe { state.as_ref() } {
        surf::free(s.rs);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15s_to_material(
    rs: u64,
    state: *const SurfaceState,
    name: *const u8,
    n: usize,
    world: usize,
) -> usize {
    let (Some(st), name) = (unsafe { state.as_ref() }, unsafe { bytes(name, n) }) else {
        return 0;
    };
    surf::to_material(rs, st, name, world)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15s_params(
    state: *const SurfaceState,
    me: usize,
    physics: usize,
    scrape: Str,
    roll: Str,
    out: *mut SurfaceParams,
) {
    if let (Some(st), Some(out)) = unsafe { (state.as_ref(), out.as_mut()) } {
        *out = surf::params(st, me, physics, scrape, roll);
    }
}
