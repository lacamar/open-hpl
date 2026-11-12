//! C ABI of the sound handler state, mirrored in `HPL2/core/include/sound/RustSound.h`.

use super::entry::EntryState;
use super::handler::{self, EntryHandle, Handler, Shells};
use std::ffi::c_void;

#[repr(C)]
pub struct HandlerShellOps {
    pub update: unsafe extern "C" fn(*mut c_void, *mut c_void, f32) -> bool,
    pub delete: unsafe extern "C" fn(*mut c_void, *mut c_void),
}

struct CShells<'a> {
    ops: &'a HandlerShellOps,
    ctx: *mut c_void,
}

impl Shells for CShells<'_> {
    fn update(&mut self, shell: *mut c_void, dt: f32) -> bool {
        // SAFETY: `ohpl_sndh_update` contract: `ctx` is valid for the table.
        unsafe { (self.ops.update)(self.ctx, shell, dt) }
    }
    fn delete(&mut self, shell: *mut c_void) {
        // SAFETY: as above.
        unsafe { (self.ops.delete)(self.ctx, shell) }
    }
}

fn entry(bits: u64) -> Option<EntryHandle> {
    EntryHandle::from_bits(bits)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_sndh_new() -> *mut Handler {
    Box::into_raw(Box::default())
}

/// # Safety
/// `h` comes from `ohpl_sndh_new`, holds no entries and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_free(h: *mut Handler) {
    if !h.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(h) });
    }
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_set_silent(h: *mut Handler, silent: bool) {
    // SAFETY: caller contract.
    unsafe { &mut *h }.set_silent(silent);
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_silent(h: *const Handler) -> bool {
    // SAFETY: caller contract.
    unsafe { &*h }.silent()
}

/// # Safety
/// `h` is live; `volume` and `speed` point at 32 floats each.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_set_globals(
    h: *mut Handler,
    volume: *const [f32; 32],
    speed: *const [f32; 32],
) {
    // SAFETY: caller contract.
    unsafe { &mut *h }.set_globals(unsafe { &*volume }, unsafe { &*speed });
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_global_volume(h: *const Handler, entry_type: u32) -> f32 {
    // SAFETY: caller contract.
    unsafe { &*h }.global_volume(entry_type)
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_global_speed(h: *const Handler, entry_type: u32) -> f32 {
    // SAFETY: caller contract.
    unsafe { &*h }.global_speed(entry_type)
}

/// Creates an entry record (id = current counter) and returns its handle bits, 0 on failure.
/// `*state` receives the Rust-owned state, valid until `ohpl_sndh_entry_free`.
///
/// # Safety
/// `h` is live; `name` is readable for `len`; `state` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_entry_new(
    h: *mut Handler,
    shell: *mut c_void,
    name: *const u8,
    len: usize,
    volume: f32,
    entry_type: u32,
    is_3d: bool,
    stream: bool,
    state: *mut *mut EntryState,
) -> u64 {
    // SAFETY: caller contract.
    let (h, name) = unsafe { (&mut *h, std::slice::from_raw_parts(name, len)) };
    let st = EntryState::new(volume, entry_type, is_3d, stream);
    let Some(e) = h.new_entry(shell, name, st) else {
        return 0;
    };
    // SAFETY: caller contract.
    unsafe { *state = h.state_ptr(e) };
    e.to_bits()
}

/// # Safety
/// `h` is live; `bits` came from `ohpl_sndh_entry_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_entry_id(h: *const Handler, bits: u64) -> i32 {
    // SAFETY: caller contract.
    entry(bits).map_or(-1, |e| unsafe { &*h }.entry_id(e))
}

/// # Safety
/// `h` is live; `bits` came from `ohpl_sndh_entry_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_push_back(h: *mut Handler, bits: u64) {
    if let Some(e) = entry(bits) {
        // SAFETY: caller contract.
        unsafe { &mut *h }.push_back(e);
    }
}

/// # Safety
/// `h` is live; `bits` came from `ohpl_sndh_entry_new`; the state pointer dies with the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_entry_free(h: *mut Handler, bits: u64) {
    if let Some(e) = entry(bits) {
        // SAFETY: caller contract.
        unsafe { &mut *h }.free_entry(e);
    }
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_count(h: *const Handler) -> usize {
    // SAFETY: caller contract.
    unsafe { &*h }.len()
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_shell_at(h: *const Handler, i: usize) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { &*h }.shell_at(i)
}

/// # Safety
/// `h` is live; `name` is readable for `len`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_find(
    h: *const Handler,
    name: *const u8,
    len: usize,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { &*h }.find(unsafe { std::slice::from_raw_parts(name, len) })
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_is_valid(
    h: *const Handler,
    shell: *mut c_void,
    id: i32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { &*h }.is_valid(shell, id)
}

/// # Safety
/// `h` is live; `ops`/`ctx` are a valid pair. The shell callbacks may re-enter `h`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_update(
    h: *mut Handler,
    dt: f32,
    ops: *const HandlerShellOps,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let mut shells = CShells {
        ops: unsafe { &*ops },
        ctx,
    };
    // SAFETY: caller contract; no reference to `h` is live here.
    unsafe { handler::update_all(h, dt, &mut shells) };
}

/// # Safety
/// `h` is live; `f` accepts `ctx` and may re-enter `h`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_for_each(
    h: *mut Handler,
    mask: u32,
    f: unsafe extern "C" fn(*mut c_void, *mut c_void),
    ctx: *mut c_void,
) {
    // SAFETY: caller contract; the closure only forwards to `f`.
    unsafe {
        handler::for_each(h, mask, &mut |shell| {
            f(ctx, shell);
        })
    };
}
