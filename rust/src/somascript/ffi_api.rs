//! C ABI for `api.rs`.

use super::api::{self, Enum};
use crate::string_ffi::{Sink, emit, units};

use std::ffi::c_void;

/// (pointer, length) of a byte string owned by the caller.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OhplGs6Str {
    pub ptr: *const u8,
    pub len: usize,
}

impl OhplGs6Str {
    /// # Safety
    /// `ptr` is valid for `len` bytes.
    unsafe fn bytes<'a>(&self) -> &'a [u8] {
        // SAFETY: caller contract.
        unsafe { units(self.ptr, self.len) }
    }
}

/// # Safety
/// `p` holds `n` valid strings.
unsafe fn strs<'a>(p: *const OhplGs6Str, n: usize) -> Vec<&'a [u8]> {
    // SAFETY: caller contract.
    unsafe { units(p, n) }
        .iter()
        // SAFETY: caller contract.
        .map(|s| unsafe { s.bytes() })
        .collect()
}

pub type RecordSink = unsafe extern "C" fn(
    ctx: *mut c_void,
    kind: u32,
    type_idx: usize,
    a: *const u8,
    a_len: usize,
    b: *const u8,
    b_len: usize,
    n: i32,
);

/// Parses `script_api.txt` text into records for C++ to replay.
///
/// # Safety
/// `text` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_api_load(
    text: *const u8,
    n: usize,
    sink: RecordSink,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    api::load(unsafe { units(text, n) }, |r| unsafe {
        sink(
            ctx,
            r.kind,
            r.type_idx,
            r.a.as_ptr(),
            r.a.len(),
            r.b.as_ptr(),
            r.b.len(),
            r.n,
        )
    });
}

/// # Safety
/// `decl` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_decl_name(
    decl: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let r = api::decl_name(unsafe { units(decl, n) });
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &r) }
}

/// Index of the candidate declaration to register instead of `decl`, or -1 to keep `decl`.
///
/// # Safety
/// Arrays valid for their counts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_native_decl_pick(
    cands: *const OhplGs6Str,
    n_cands: usize,
    enums: *const OhplGs6Str,
    n_enums: usize,
    decl: *const u8,
    decl_len: usize,
) -> isize {
    // SAFETY: caller contract.
    let (cands, enums, decl) = unsafe {
        (
            strs(cands, n_cands),
            strs(enums, n_enums),
            units(decl, decl_len),
        )
    };
    let is_enum = |s: &[u8]| enums.contains(&s);
    api::native_decl_pick(&cands, &is_enum, decl).map_or(-1, |i| i as isize)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs6_enums_new() -> *mut Vec<Enum> {
    Box::into_raw(Box::default())
}

/// # Safety
/// `e` came from `ohpl_gs6_enums_new` (or is null) and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_enums_free(e: *mut Vec<Enum>) {
    if !e.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(e) });
    }
}

/// # Safety
/// `e` is live; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_enums_add(e: *mut Vec<Enum>, name: *const u8, n: usize) {
    // SAFETY: caller contract.
    unsafe {
        (*e).push(Enum {
            name: units(name, n).to_vec(),
            values: Vec::new(),
        });
    }
}

/// Appends a value to the last enum added.
///
/// # Safety
/// `e` is live; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_enums_add_value(
    e: *mut Vec<Enum>,
    name: *const u8,
    n: usize,
    value: i32,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(last) = (*e).last_mut() {
            last.values.push((units(name, n).to_vec(), value));
        }
    }
}

/// # Safety
/// Both live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_enums_rebase(e: *mut Vec<Enum>, base: *const Vec<Enum>) {
    // SAFETY: caller contract.
    unsafe { api::rebase_enums(&mut *e, &*base) }
}

/// Values of every enum in order, flattened.
///
/// # Safety
/// `e` is live; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_enums_values(
    e: *const Vec<Enum>,
    sink: Sink<i32>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let v: Vec<i32> = unsafe { &*e }
        .iter()
        .flat_map(|e| e.values.iter().map(|(_, v)| *v))
        .collect();
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &v) }
}

/// # Safety
/// `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_is_lookup_name(name: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    api::is_lookup_name(unsafe { units(name, n) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs6_stub_return_kind(
    is_void: bool,
    inout_ref: bool,
    is_handle: bool,
) -> u32 {
    api::stub_return_kind(is_void, inout_ref, is_handle)
}

/// # Safety
/// Pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_is_script_struct(
    kind: *const u8,
    kind_len: usize,
    name: *const u8,
    name_len: usize,
    has_factory: bool,
) -> bool {
    // SAFETY: caller contract.
    unsafe { api::is_script_struct(units(kind, kind_len), units(name, name_len), has_factory) }
}

/// # Safety
/// Pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_is_copy_ctor(
    params: *const u8,
    params_len: usize,
    name: *const u8,
    name_len: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe { api::is_copy_ctor(units(params, params_len), units(name, name_len)) }
}

/// Accessor declarations for a property: `get` always, `set` unless the property is const.
/// Returns false if the declaration has no name.
///
/// # Safety
/// Pointers valid for their lengths; sinks accept `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_property_accessors(
    decl: *const u8,
    decl_len: usize,
    enums: *const OhplGs6Str,
    n_enums: usize,
    get: Sink<u8>,
    set: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let (decl, enums) = unsafe { (units(decl, decl_len), strs(enums, n_enums)) };
    let is_enum = |s: &[u8]| enums.contains(&s);
    let Some((g, s)) = api::property_accessors(decl, &is_enum) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe { emit(get, ctx, &g) };
    if let Some(s) = s {
        // SAFETY: caller contract.
        unsafe { emit(set, ctx, &s) }
    }
    true
}

/// # Safety
/// `decl` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_prop_name(
    decl: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let r = api::prop_name(unsafe { units(decl, n) });
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &r) }
}
