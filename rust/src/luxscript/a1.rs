//! A1: the Amnesia script API (LuxScriptHandler's registered functions) is registered from Rust;
//! the bodies are in `fns`. Engine objects still in C++ are reached through one host op table
//! (`LuxScriptHostA1.cpp`).

use std::ffi::{CString, c_char, c_void};
use std::sync::RwLock;

/// Flat argument or result slot; ints, floats and bools live in the low bits of `u`, strings in `p`/`n`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Arg {
    pub u: u64,
    pub p: *const c_void,
    pub n: usize,
}

impl Default for Arg {
    fn default() -> Self {
        Self {
            u: 0,
            p: std::ptr::null(),
            n: 0,
        }
    }
}

impl Arg {
    pub fn i(v: i32) -> Self {
        Self {
            u: u64::from(v.cast_unsigned()),
            ..Self::default()
        }
    }

    pub fn f(v: f32) -> Self {
        Self {
            u: u64::from(v.to_bits()),
            ..Self::default()
        }
    }

    pub fn b(v: bool) -> Self {
        Self {
            u: u64::from(v),
            ..Self::default()
        }
    }

    pub fn bytes(b: &[u8]) -> Self {
        Self {
            p: b.as_ptr().cast(),
            n: b.len(),
            ..Self::default()
        }
    }

    pub fn ptr(p: *const c_void) -> Self {
        Self {
            p,
            ..Self::default()
        }
    }

    pub fn as_i(&self) -> i32 {
        (self.u as u32).cast_signed()
    }

    pub fn as_f(&self) -> f32 {
        f32::from_bits(self.u as u32)
    }

    pub fn as_b(&self) -> bool {
        self.u & 0xFF != 0
    }
}

pub type HostFn = unsafe extern "C" fn(op: u32, args: *const Arg, n: usize, ret: *mut Arg);

static HOST: RwLock<Option<HostFn>> = RwLock::new(None);

pub fn set_host(h: Option<HostFn>) {
    if let Ok(mut g) = HOST.write() {
        *g = h;
    }
}

/// Engine-object host op (`fns::ops`); not traced, the script function that issued it is.
pub fn host(op: u32, args: &[Arg]) -> Arg {
    let h = HOST.read().ok().and_then(|g| *g);
    let mut ret = Arg::default();
    if let Some(h) = h {
        // SAFETY: `args` and `ret` outlive the call; the lock is released so the host may call back.
        unsafe { h(op, args.as_ptr(), args.len(), &raw mut ret) };
    }
    ret
}

/// Bytes of the engine script string `p`.
///
/// # Safety
/// `p` is a `string&` passed by the engine for the duration of the call.
pub unsafe fn raw_bytes<'a>(p: *const c_void) -> &'a [u8] {
    #[cfg(not(test))]
    {
        // SAFETY: caller contract.
        unsafe { crate::script::engine::script_str(p) }
    }
    #[cfg(test)]
    {
        if p.is_null() {
            return &[];
        }
        // SAFETY: tests pass `Vec<u8>` pointers.
        unsafe { &*p.cast::<Vec<u8>>() }
    }
}

/// # Safety
/// As [`raw_bytes`].
pub unsafe fn str_arg(p: *const c_void) -> Arg {
    // SAFETY: caller contract.
    Arg::bytes(unsafe { raw_bytes(p) })
}

pub type AddFn = unsafe extern "C" fn(ctx: *mut c_void, decl: *const c_char, f: *const c_void);

/// Registers every function in the order of the original `InitScriptFunctions`; returns the count.
///
/// # Safety
/// `add` accepts `ctx` and the NUL-terminated declaration.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a1_register(add: AddFn, ctx: *mut c_void) -> usize {
    let t = super::a1_table::table();
    for &(decl, _, f) in &t {
        if let Ok(d) = CString::new(decl) {
            // SAFETY: caller contract; `f` is an `extern "C"` function matching the declaration.
            unsafe { add(ctx, d.as_ptr(), f) };
        }
    }
    t.len()
}

/// # Safety
/// `h` stays valid until replaced.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a1b_set_host(h: Option<HostFn>) {
    set_host(h);
}
