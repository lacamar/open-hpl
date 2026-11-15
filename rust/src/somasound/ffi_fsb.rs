//! C ABI of the FSB glue (`soma/src/game/SomaFsb.h`).

use super::extract;
use crate::string_ffi::units;
use std::collections::BTreeMap;
use std::ffi::c_void;

type Pair = unsafe extern "C" fn(*mut c_void, *const u8, usize, *const u8, usize);

#[repr(C)]
pub struct OhplS7Str {
    pub p: *const u8,
    pub n: usize,
}

/// # Safety
/// `game` and `sub` are readable for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_cache_dir(
    game: *const u8,
    gn: usize,
    sub: *const u8,
    sn: usize,
    sink: unsafe extern "C" fn(*mut c_void, *const u8, usize),
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let d = extract::cache_dir(unsafe { units(game, gn) }, unsafe { units(sub, sn) });
    // SAFETY: caller contract.
    unsafe { sink(ctx, d.as_ptr(), d.len()) };
}

/// # Safety
/// Every pointer is readable for its length; `samples` holds `count` strings; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_extract_samples(
    bank: *const u8,
    bn: usize,
    cache: *const u8,
    cn: usize,
    prefix: *const u8,
    pn: usize,
    samples: *const OhplS7Str,
    count: usize,
    sink: Pair,
    ctx: *mut c_void,
) {
    let list: Vec<Vec<u8>> = (0..count)
        // SAFETY: caller contract.
        .map(|i| unsafe {
            let s = &*samples.add(i);
            units(s.p, s.n).to_vec()
        })
        .collect();
    let mut out = BTreeMap::new();
    // SAFETY: caller contract.
    let (bank, cache, prefix) = unsafe { (units(bank, bn), units(cache, cn), units(prefix, pn)) };
    extract::extract_samples(bank, cache, prefix, &list, &mut out);
    for (k, v) in &out {
        // SAFETY: caller contract.
        unsafe { sink(ctx, k.as_ptr(), k.len(), v.as_ptr(), v.len()) };
    }
}
