//! S2: C ABI of the Rust script natives (`soma/src/game/RustSomaScriptS2.h`).

use super::s2_api::{self, Api};
use super::s2_as::{self, Engine, Func};
use super::s2_host;
use super::{s1_gen, s2_globals, s2_math, s2_strings};
use crate::string_ffi::{Sink, emit, units};
use std::ffi::{CStr, c_char, c_int, c_void};

pub(crate) const STEP_LUX: u32 = 0;
pub(crate) const STEP_GLOBAL_ENGINE: u32 = 1;
pub(crate) const STEP_CLASSES: u32 = 2;

fn step(e: Engine, n: u32) {
    if let Some(h) = s2_host::ops() {
        // SAFETY: the host registers on the engine it is handed.
        unsafe { (h.register_step)(e.0, n) };
    }
}

/// `RegisterSomaScriptNatives`.
///
/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_register_natives(engine: *mut c_void) {
    let e = Engine(engine);
    s2_math::register(e);
    s2_strings::register_natives(e);
    step(e, STEP_LUX);
    s2_globals::register_before_engine(e);
    step(e, STEP_GLOBAL_ENGINE);
    s2_globals::register_after_engine(e);
    step(e, STEP_CLASSES);
    s1_gen::register(e);
}

/// `RegisterSomaScriptStrings` and `RegisterSomaScriptArrayExtras` are separate in the C++ engine setup.
///
/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_register_strings(engine: *mut c_void) {
    s2_strings::register_types(Engine(engine));
}

/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_register_array_extras(engine: *mut c_void) {
    s2_strings::register_array_extras(Engine(engine));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s2_api_new() -> *mut Api {
    Box::into_raw(Box::default())
}

/// # Safety
/// `api` came from `ohpl_s2_api_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_api_free(api: *mut Api) {
    s2_api::clear_active(api);
    if !api.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(api) });
    }
}

/// # Safety
/// `api` is live; `text` is valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_api_load(api: *mut Api, text: *const u8, n: usize) {
    // SAFETY: caller contract.
    if let Some(a) = unsafe { api.as_mut() } {
        a.load(unsafe { units(text, n) });
    }
}

/// # Safety
/// Both are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_api_rebase(api: *mut Api, base: *const Api) {
    // SAFETY: caller contract.
    if let (Some(a), Some(b)) = unsafe { (api.as_mut(), base.as_ref()) } {
        a.rebase_enums(b);
    }
}

/// # Safety
/// `api` and `engine` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_api_register_types(api: *mut Api, engine: *mut c_void) {
    // SAFETY: caller contract.
    if let Some(a) = unsafe { api.as_mut() } {
        a.register_types(Engine(engine));
    }
}

/// # Safety
/// `api` and `engine` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_api_register(api: *mut Api, engine: *mut c_void) -> c_int {
    // SAFETY: caller contract.
    match unsafe { api.as_mut() } {
        Some(a) => c_int::try_from(a.register(Engine(engine))).unwrap_or(c_int::MAX),
        None => 0,
    }
}

pub const LIST_ERRORS: u32 = 0;
pub const LIST_WARNINGS: u32 = 1;
pub const LIST_REMAPS: u32 = 2;

/// # Safety
/// `api` is live; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_api_list(
    api: *const Api,
    which: u32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let Some(a) = (unsafe { api.as_ref() }) else {
        return;
    };
    let l = match which {
        LIST_ERRORS => a.errors(),
        LIST_WARNINGS => a.warnings(),
        LIST_REMAPS => a.remaps(),
        _ => &[],
    };
    for s in l {
        // SAFETY: caller contract.
        unsafe { emit(sink, ctx, s) };
    }
}

/// `cSomaScriptApi::NativeDecl`; a null `ty` is a global.
///
/// # Safety
/// Pointers valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_native_decl(
    ty: *const u8,
    tn: usize,
    decl: *const u8,
    dn: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let t = (!ty.is_null()).then(|| unsafe { units(ty, tn) });
    let r = s2_api::active_native_decl(t, unsafe { units(decl, dn) });
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &r) };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s2_is_dummy(p: *mut c_void) -> bool {
    s2_api::is_dummy(p)
}

/// # Safety
/// `g` is the running generic frame.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_stub_call(g: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { s2_api::stub_call(g) }
}

/// # Safety
/// `g` is the running generic frame.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_release_handle_args(g: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { s2_api::release_handle_args(g) }
}

/// # Safety
/// `engine` is live; `ty` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_dummy_of(engine: *mut c_void, ty: *const c_char) -> *mut c_void {
    // SAFETY: caller contract.
    s2_api::dummy_of(Engine(engine), unsafe { CStr::from_ptr(ty) }.to_bytes())
}

/// # Safety
/// `ty` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_new_struct(ty: *const c_char, owned: bool) -> *mut c_void {
    // SAFETY: caller contract.
    let t = unsafe { CStr::from_ptr(ty) }.to_bytes();
    if owned {
        s2_api::new_owned_script_struct(t)
    } else {
        s2_api::new_script_struct(t)
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s2_is_stub(func: *mut c_void) -> bool {
    !func.is_null() && s2_api::is_stub(Func(func))
}

/// # Safety
/// `ty` is valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_set_indirect(ty: *const u8, n: usize, pointer_offset: i32) {
    // SAFETY: caller contract.
    s2_api::set_indirect_props(unsafe { units(ty, n) }, pointer_offset);
}

/// # Safety
/// Pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_indirect_offset(
    ty: *const u8,
    n: usize,
    name: *const u8,
    m: usize,
) -> i32 {
    // SAFETY: caller contract.
    s2_api::indirect_prop_offset(unsafe { units(ty, n) }, unsafe { units(name, m) })
}

/// # Safety
/// `ty` is valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_new_prop_block(ty: *const u8, n: usize) -> *mut c_void {
    // SAFETY: caller contract.
    s2_api::new_prop_block(unsafe { units(ty, n) })
}

/// # Safety
/// `ty` is valid for `n` bytes; `block` came from `ohpl_s2_new_prop_block`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_free_prop_block(ty: *const u8, n: usize, block: *mut c_void) {
    // SAFETY: caller contract.
    s2_api::free_prop_block(unsafe { units(ty, n) }, block);
}

pub type CountSink = unsafe extern "C" fn(*mut c_void, *const u8, usize, i32);

/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_stub_counts(sink: CountSink, ctx: *mut c_void) {
    for (k, v) in s2_api::stub_counts() {
        // SAFETY: caller contract.
        unsafe { sink(ctx, k.as_ptr(), k.len(), v) };
    }
}

/// # Safety
/// `engine` is live; `path` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_dump_decls(engine: *mut c_void, path: *const c_char) -> c_int {
    // SAFETY: caller contract.
    unsafe { s2_as::ohpl_as2_dump_decls(engine, path) }
}

/// `ConfigureSomaScriptEngine`: scripts use `'x'` integer literals, unqualified `&` references and
/// property accessors, and members exist before the constructor body.
///
/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_configure_engine(engine: *mut c_void) {
    let e = Engine(engine);
    e.set_property(5, 1);
    e.set_property(1, 1);
    e.set_property(14, 3);
    e.set_property(38, 0);
}

/// # Safety
/// `engine` is live; `path` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_call_corpus(
    engine: *mut c_void,
    seed: u32,
    calls: u32,
    path: *const c_char,
) -> c_int {
    // SAFETY: caller contract.
    unsafe { s2_as::ohpl_as2_call_corpus(engine, seed, calls, path) }
}
