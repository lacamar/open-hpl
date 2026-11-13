//! C ABI of `HPL2/core/include/graphics/RustGraphicsH5b.h`: the process-wide `Gfx` the Rust stages
//! draw through, and the scenario entry the differential harness uses.

use core::ffi::c_void;
use std::sync::{Mutex, MutexGuard, PoisonError};

use super::h5b_foreign::{CallFn, ForeignGfx};
use super::h5b_gfx::Gfx;
use super::h5b_gl::GlGfx;
use super::h5b_scn;
use crate::backend::gl_ctx::ctx::Ctx;

struct Slot(Option<Box<dyn Gfx>>);

// SAFETY: the Gfx behind the slot is only touched under the lock, on the render thread's behalf.
unsafe impl Send for Slot {}

static GFX: Mutex<Slot> = Mutex::new(Slot(None));

fn lock() -> MutexGuard<'static, Slot> {
    GFX.lock().unwrap_or_else(PoisonError::into_inner)
}

/// Installs `g` as the process-wide graphics device.
pub fn set_gfx(g: Box<dyn Gfx>) {
    lock().0 = Some(g);
}

pub fn clear_gfx() {
    lock().0 = None;
}

/// Runs `f` on the installed device; `None` before `set_gfx`.
pub fn with_gfx<R>(f: impl FnOnce(&mut dyn Gfx) -> R) -> Option<R> {
    let mut slot = lock();
    let g = slot.0.as_mut()?;
    Some(f(g.as_mut()))
}

/// # Safety
/// `call(ctx, ..)` is the facade entry of a live `iLowLevelGraphics`, valid until `ohpl_h5b_gfx_detach`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5b_gfx_attach_foreign(ctx: *mut c_void, call: CallFn) {
    // SAFETY: caller contract.
    set_gfx(Box::new(unsafe { ForeignGfx::new(ctx, call) }));
}

/// Installs a `GlGfx` over the GL context `gl` (from `ohpl_glctx_new`); resources and the state the
/// C++ shell keeps go through the facade `ctx`.
///
/// # Safety
/// `gl` outlives the attachment; the rest as `ohpl_h5b_gfx_attach_foreign`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5b_gfx_attach_gl(gl: *mut Ctx, ctx: *mut c_void, call: CallFn) {
    // SAFETY: caller contract.
    let inner = unsafe { ForeignGfx::new(ctx, call) };
    // SAFETY: caller contract.
    set_gfx(Box::new(unsafe { GlGfx::new(gl, inner) }));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h5b_gfx_detach() {
    clear_gfx();
}

/// Runs the seeded scenario over the facade and copies the result log into `out`.
///
/// # Safety
/// As `ohpl_h5b_gfx_attach_foreign`; `out` holds `cap` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5b_run_foreign(
    ctx: *mut c_void,
    call: CallFn,
    seed: u64,
    rounds: u32,
    out: *mut u8,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    let mut g = unsafe { ForeignGfx::new(ctx, call) };
    let log = h5b_scn::run(&mut g, seed, rounds);
    let n = log.len().min(cap);
    if !out.is_null() {
        // SAFETY: `out` holds `cap >= n` bytes.
        unsafe { core::ptr::copy_nonoverlapping(log.as_ptr(), out, n) };
    }
    log.len()
}

/// Runs `n` random draw and batch calls over the facade.
///
/// # Safety
/// As `ohpl_h5b_gfx_attach_foreign`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5b_run_draws(ctx: *mut c_void, call: CallFn, seed: u64, n: u32) {
    // SAFETY: caller contract.
    let mut g = unsafe { ForeignGfx::new(ctx, call) };
    h5b_scn::run_draws(&mut g, seed, n);
}
