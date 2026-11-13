//! A1: the Amnesia script API (LuxScriptHandler's registered functions) registered and entered from Rust.
//! Each function marshals its script arguments into [`Arg`]s and calls the host op table, which is
//! implemented by the C++ game objects (`LuxScriptA1Host.cpp`) until those are ported.

use super::a1_gen;
use std::ffi::{CString, c_char, c_void};
use std::sync::RwLock;

pub const OP_WARNING: u32 = 0xFFFF;

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

#[cfg_attr(not(test), allow(dead_code))] // read by the golden tests
pub struct Func {
    pub decl: &'static str,
    pub name: &'static str,
    pub op: u32,
    pub kinds: &'static str,
    pub ret: char,
    pub f: *const c_void,
}

pub type HostFn = unsafe extern "C" fn(op: u32, args: *const Arg, n: usize, ret: *mut Arg);

static HOST: RwLock<Option<HostFn>> = RwLock::new(None);

pub fn set_host(h: Option<HostFn>) {
    if let Ok(mut g) = HOST.write() {
        *g = h;
    }
}

pub fn call(op: u32, args: &[Arg]) -> Arg {
    let host = HOST.read().ok().and_then(|g| *g);
    let mut ret = Arg::default();
    if let Some(h) = host {
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

pub fn nul_cut(b: &[u8]) -> &[u8] {
    b.split(|&c| c == 0).next().unwrap_or(b)
}

/// `printf("%f", double(x))`.
pub fn fmt_f(x: f32) -> String {
    let d = f64::from(x);
    if d.is_nan() {
        return if d.is_sign_negative() { "-nan" } else { "nan" }.into();
    }
    if d.is_infinite() {
        return if d < 0.0 { "-inf" } else { "inf" }.into();
    }
    format!("{d:.6}")
}

pub fn warn(msg: &[u8]) {
    call(OP_WARNING, &[Arg::bytes(msg)]);
}

pub type AddFn = unsafe extern "C" fn(ctx: *mut c_void, decl: *const c_char, f: *const c_void);

/// Registers every function in the order of `cLuxScriptHandler::InitScriptFunctions`; returns the count.
///
/// # Safety
/// `add` accepts `ctx` and the NUL-terminated declaration.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a1_register(add: AddFn, ctx: *mut c_void) -> usize {
    let t = a1_gen::table();
    for f in &t {
        if let Ok(d) = CString::new(f.decl) {
            // SAFETY: caller contract; `f.f` is an `extern "C"` function matching the declaration.
            unsafe { add(ctx, d.as_ptr(), f.f) };
        }
    }
    t.len()
}

/// # Safety
/// `h` stays valid until replaced.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a1_set_host(h: Option<HostFn>) {
    set_host(h);
}
