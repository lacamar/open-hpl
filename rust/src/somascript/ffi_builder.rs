//! C ABI for `builder.rs`.

use super::builder::{self, Builder};
use crate::string_ffi::{Sink, emit, units};

use std::ffi::c_void;

pub type SectionSink = unsafe extern "C" fn(*mut c_void, *const u8, usize, *const u8, usize);

/// # Safety
/// Pointers valid for their lengths. Free with `ohpl_gs6_builder_free`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_builder_new(
    game_dir: *const u8,
    n: usize,
    game: *const u8,
    m: usize,
) -> *mut Builder {
    // SAFETY: caller contract.
    let b = unsafe { Builder::new(units(game_dir, n), units(game, m)) };
    Box::into_raw(Box::new(b))
}

/// # Safety
/// `b` came from `ohpl_gs6_builder_new` (or is null) and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_builder_free(b: *mut Builder) {
    if !b.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(b) });
    }
}

/// Emits the resolved path (empty if none).
///
/// # Safety
/// `b` is live; pointers valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_builder_resolve(
    b: *const Builder,
    include: *const u8,
    n: usize,
    from: *const u8,
    m: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let r = unsafe { (*b).resolve(units(include, n), units(from, m)) };
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &r) }
}

/// Emits each section, then the first missing include (`missing`, possibly empty) and the compat
/// patch warnings (`warning`). Returns false if the entry file cannot be opened.
///
/// # Safety
/// `b` is live; pointers valid for their lengths; sinks accept `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_builder_collect(
    b: *const Builder,
    entry: *const u8,
    n: usize,
    section: SectionSink,
    missing: Sink<u8>,
    warning: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let Some(c) = (unsafe { (*b).collect(units(entry, n)) }) else {
        return false;
    };
    for (name, code) in &c.sections {
        // SAFETY: caller contract.
        unsafe { section(ctx, name.as_ptr(), name.len(), code.as_ptr(), code.len()) }
    }
    // SAFETY: caller contract.
    unsafe { emit(missing, ctx, &c.missing) };
    for w in &c.warnings {
        // SAFETY: caller contract.
        unsafe { emit(warning, ctx, w) }
    }
    true
}

/// # Safety
/// Pointer valid for its length.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_is_nosave(key: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    builder::is_nosave(unsafe { units(key, n) })
}

/// Key of the bytecode cache entry for these sections; `sections` is `count` (name, code) pairs.
///
/// # Safety
/// Arrays hold `count` valid (pointer, length) pairs; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_cache_key(
    exe_size: u64,
    mtime_sec: i64,
    mtime_nsec: i64,
    names: *const *const u8,
    name_lens: *const usize,
    codes: *const *const u8,
    code_lens: *const usize,
    count: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let (names, name_lens, codes, code_lens) = unsafe {
        (
            units(names, count),
            units(name_lens, count),
            units(codes, count),
            units(code_lens, count),
        )
    };
    let sections: Vec<(Vec<u8>, Vec<u8>)> = (0..count)
        .map(|i| {
            let at = |p: &[*const u8], l: &[usize]| match (p.get(i), l.get(i)) {
                // SAFETY: caller contract.
                (Some(&p), Some(&l)) => unsafe { units(p, l) }.to_vec(),
                _ => Vec::new(),
            };
            (at(names, name_lens), at(codes, code_lens))
        })
        .collect();
    let k = builder::cache_key(exe_size, mtime_sec, mtime_nsec, &sections);
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, k.as_bytes()) }
}

/// # Safety
/// Pointer valid for its length; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_cache_file_name(
    entry: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let k = builder::cache_file_name(unsafe { units(entry, n) });
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, k.as_bytes()) }
}
