//! C ABI of the post effects: the shell forwards `RenderEffect` and `Reset`, engine calls go through
//! the host table of `ffi_s8b`.

use super::ffi_s8b::{Io, host_call};
use super::s8c_pfx::{self as pfx, Gpu, Gx};
use crate::somascript::s2_as::Engine;
use std::ffi::c_void;

struct CGpu;

impl Gpu for CGpu {
    fn call(&self, op: u32, io: &mut Io) {
        host_call(op, io);
    }
}

static GPU: CGpu = CGpu;

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8c_render(shell: usize, tex: usize, fb: usize) -> usize {
    pfx::render(&Gx(&GPU), shell, tex, fb)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8c_reset(shell: usize) {
    pfx::reset(shell);
}

/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8c_register(engine: *mut c_void) {
    pfx::register(Engine(engine), &GPU);
}
