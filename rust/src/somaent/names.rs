//! Name and callback strings of `cSomaLuxEntity`; the C++ shell holds one box and indexes it by `eSeStr`.

use std::collections::BTreeMap;

pub const COUNT: usize = 14;

#[derive(Default)]
pub struct Names {
    s: [Vec<u8>; COUNT],
    vars: BTreeMap<Vec<u8>, Vec<u8>>,
}

impl Names {
    pub(crate) fn name(&self) -> &[u8] {
        &self.s[0]
    }
    #[cfg(test)]
    pub(crate) fn set_name(&mut self, n: &[u8]) {
        self.s[0] = n.to_vec();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_se_names_new() -> *mut Names {
    Box::into_raw(Box::default())
}

/// # Safety
/// `p` came from `ohpl_se_names_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_names_free(p: *mut Names) {
    if !p.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(p) });
    }
}

/// # Safety
/// `p` is a live box from `ohpl_se_names_new`; `len` receives the byte count.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_names_get(
    p: *const Names,
    i: usize,
    len: *mut usize,
) -> *const u8 {
    // SAFETY: caller contract.
    let (n, len) = unsafe { (&*p, &mut *len) };
    let s = n.s.get(i).map_or(&[][..], Vec::as_slice);
    *len = s.len();
    s.as_ptr()
}

/// # Safety
/// `p` is a live box from `ohpl_se_names_new`; `s` points at `len` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_names_set(p: *mut Names, i: usize, s: *const u8, len: usize) {
    // SAFETY: caller contract.
    let (n, s) = unsafe { (&mut *p, std::slice::from_raw_parts(s, len)) };
    if let Some(d) = n.s.get_mut(i) {
        d.clear();
        d.extend_from_slice(s);
    }
}

/// # Safety
/// `p` is live; `k` points at `kl` bytes. With `insert` an absent key becomes empty (C++ `operator[]`).
/// Returns null when absent and not inserted; `len` receives the value size.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_var_get(
    p: *mut Names,
    k: *const u8,
    kl: usize,
    insert: bool,
    len: *mut usize,
) -> *const u8 {
    // SAFETY: caller contract.
    let (n, k, len) = unsafe { (&mut *p, std::slice::from_raw_parts(k, kl), &mut *len) };
    let v = if insert {
        Some(n.vars.entry(k.to_vec()).or_default())
    } else {
        n.vars.get_mut(k)
    };
    match v {
        Some(v) => {
            *len = v.len();
            v.as_ptr()
        }
        None => std::ptr::null(),
    }
}

/// # Safety
/// `p` is live; `k` and `v` point at `kl` and `vl` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_var_set(
    p: *mut Names,
    k: *const u8,
    kl: usize,
    v: *const u8,
    vl: usize,
) {
    // SAFETY: caller contract.
    let (n, k, v) = unsafe {
        (
            &mut *p,
            std::slice::from_raw_parts(k, kl),
            std::slice::from_raw_parts(v, vl),
        )
    };
    n.vars.insert(k.to_vec(), v.to_vec());
}

/// # Safety
/// `p` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_var_clear(p: *mut Names) {
    // SAFETY: caller contract.
    unsafe { &mut *p }.vars.clear();
}

/// Entry `i` in key order; null past the end.
///
/// # Safety
/// `p` is live; `kl`, `k`, `vl` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_var_at(
    p: *const Names,
    i: usize,
    k: *mut *const u8,
    kl: *mut usize,
    vl: *mut usize,
) -> *const u8 {
    // SAFETY: caller contract.
    let (n, k, kl, vl) = unsafe { (&*p, &mut *k, &mut *kl, &mut *vl) };
    match n.vars.iter().nth(i) {
        Some((key, v)) => {
            (*k, *kl, *vl) = (key.as_ptr(), key.len(), v.len());
            v.as_ptr()
        }
        None => std::ptr::null(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn set_get_round_trip() {
        let p = ohpl_se_names_new();
        let mut len = 0;
        // SAFETY: p is live; buffers outlive the calls.
        unsafe {
            ohpl_se_names_set(p, 3, b"abc".as_ptr(), 3);
            let g = ohpl_se_names_get(p, 3, &mut len);
            assert_eq!(std::slice::from_raw_parts(g, len), b"abc");
            ohpl_se_names_get(p, 0, &mut len);
            assert_eq!(len, 0);
            ohpl_se_var_set(p, b"b".as_ptr(), 1, b"2".as_ptr(), 1);
            assert!(ohpl_se_var_get(p, b"a".as_ptr(), 1, false, &mut len).is_null());
            assert!(!ohpl_se_var_get(p, b"a".as_ptr(), 1, true, &mut len).is_null() || len == 0);
            let (mut k, mut kl) = (std::ptr::null(), 0);
            ohpl_se_var_at(p, 0, &mut k, &mut kl, &mut len);
            assert_eq!(std::slice::from_raw_parts(k, kl), b"a");
            assert!(ohpl_se_var_at(p, 2, &mut k, &mut kl, &mut len).is_null());
            ohpl_se_var_clear(p);
            assert!(ohpl_se_var_at(p, 0, &mut k, &mut kl, &mut len).is_null());
            ohpl_se_names_free(p);
        }
    }
}
