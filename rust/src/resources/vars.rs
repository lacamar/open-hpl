//! `cResourceVarsObject` (H2 pass 2): the name/value map and its typed accessors.

use crate::string;
use crate::string_ffi::{Sink, emit, units};
use std::collections::BTreeMap;
use std::ffi::c_void;

#[derive(Default, Clone)]
pub struct Vars(BTreeMap<Vec<u8>, Vec<u8>>);

fn terminated(v: &[u8]) -> Vec<u8> {
    let mut t = v.to_vec();
    t.push(0);
    t
}

/// `"%f : %f"` with the colons turned into spaces, as `AddVarVector2f/3f/Color` store them.
fn joined(parts: &[f32]) -> Vec<u8> {
    let shown: Vec<Vec<u8>> = parts
        .iter()
        .map(|&x| string::float_to_string(x, 6, false))
        .collect();
    let text = shown.join(b" : ".as_slice());
    string::replace_char_to(&text, b":", b" ")
}

impl Vars {
    pub fn clear(&mut self) {
        self.0.clear();
    }

    pub fn set(&mut self, name: &[u8], value: &[u8]) {
        self.0.insert(name.to_vec(), value.to_vec());
    }

    /// `std::map::insert`: an existing name keeps its value.
    pub fn insert(&mut self, name: &[u8], value: &[u8]) {
        self.0
            .entry(name.to_vec())
            .or_insert_with(|| value.to_vec());
    }

    pub fn get(&self, name: &[u8]) -> Option<&[u8]> {
        self.0.get(name).map(Vec::as_slice)
    }

    pub fn add_bool(&mut self, name: &[u8], v: bool) {
        self.set(name, if v { b"true" } else { b"false" });
    }

    pub fn add_int(&mut self, name: &[u8], v: i32) {
        self.set(name, &string::int_to_string(v, 0));
    }

    pub fn add_float(&mut self, name: &[u8], v: f32) {
        self.set(name, &string::float_to_string(v, -1, true));
    }

    pub fn add_floats(&mut self, name: &[u8], v: &[f32]) {
        self.set(name, &joined(v));
    }

    pub fn get_bool(&self, name: &[u8], default: bool) -> bool {
        self.get(name).map_or(default, string::to_bool)
    }

    pub fn get_int(&self, name: &[u8], default: i32) -> i32 {
        self.get(name)
            .map_or(default, |v| string::token_to_int(&terminated(v)))
    }

    pub fn get_float(&self, name: &[u8], default: f32) -> f32 {
        self.get(name)
            .map_or(default, |v| string::token_to_float(&terminated(v)))
    }

    /// True when the variable holds exactly `out.len()` numbers; `out` is untouched otherwise.
    pub fn get_floats(&self, name: &[u8], out: &mut [f32]) -> bool {
        self.get(name).is_some_and(|v| string::floats_exact(v, out))
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h2_vars_new() -> *mut Vars {
    Box::into_raw(Box::default())
}

/// # Safety
/// `v` comes from `ohpl_h2_vars_new` or `ohpl_h2_vars_clone` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_vars_free(v: *mut Vars) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(v) });
}

/// # Safety
/// `v` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_vars_clone(v: *const Vars) -> *mut Vars {
    // SAFETY: caller contract.
    Box::into_raw(Box::new(unsafe { &*v }.clone()))
}

/// # Safety
/// `v` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_vars_clear(v: *mut Vars) {
    // SAFETY: caller contract.
    unsafe { &mut *v }.clear();
}

/// # Safety
/// `v` live; pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_vars_set(
    v: *mut Vars,
    n: *const u8,
    nn: usize,
    val: *const u8,
    vn: usize,
) {
    // SAFETY: caller contract.
    unsafe { (&mut *v).set(units(n, nn), units(val, vn)) }
}

/// # Safety
/// `v` live; pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_vars_insert(
    v: *mut Vars,
    n: *const u8,
    nn: usize,
    val: *const u8,
    vn: usize,
) {
    // SAFETY: caller contract.
    unsafe { (&mut *v).insert(units(n, nn), units(val, vn)) }
}

/// # Safety
/// `v` live; pointers valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_vars_get(
    v: *const Vars,
    n: *const u8,
    nn: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        (&*v).get(units(n, nn)).is_some_and(|s| {
            emit(sink, ctx, s);
            true
        })
    }
}

/// # Safety
/// `v` live; `n` valid for `nn` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_vars_add_bool(v: *mut Vars, n: *const u8, nn: usize, x: bool) {
    // SAFETY: caller contract.
    unsafe { (&mut *v).add_bool(units(n, nn), x) }
}

/// # Safety
/// `v` live; `n` valid for `nn` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_vars_add_int(v: *mut Vars, n: *const u8, nn: usize, x: i32) {
    // SAFETY: caller contract.
    unsafe { (&mut *v).add_int(units(n, nn), x) }
}

/// # Safety
/// `v` live; `n` valid for `nn` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_vars_add_float(v: *mut Vars, n: *const u8, nn: usize, x: f32) {
    // SAFETY: caller contract.
    unsafe { (&mut *v).add_float(units(n, nn), x) }
}

/// # Safety
/// `v` live; `n` valid for `nn` bytes; `x` valid for `count` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_vars_add_floats(
    v: *mut Vars,
    n: *const u8,
    nn: usize,
    x: *const f32,
    count: usize,
) {
    // SAFETY: caller contract.
    unsafe { (&mut *v).add_floats(units(n, nn), units(x, count)) }
}

/// # Safety
/// `v` live; `n` valid for `nn` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_vars_get_bool(
    v: *const Vars,
    n: *const u8,
    nn: usize,
    d: bool,
) -> bool {
    // SAFETY: caller contract.
    unsafe { (&*v).get_bool(units(n, nn), d) }
}

/// # Safety
/// `v` live; `n` valid for `nn` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_vars_get_int(
    v: *const Vars,
    n: *const u8,
    nn: usize,
    d: i32,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { (&*v).get_int(units(n, nn), d) }
}

/// # Safety
/// `v` live; `n` valid for `nn` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_vars_get_float(
    v: *const Vars,
    n: *const u8,
    nn: usize,
    d: f32,
) -> f32 {
    // SAFETY: caller contract.
    unsafe { (&*v).get_float(units(n, nn), d) }
}

/// True and `out` filled when the variable holds exactly `count` numbers.
///
/// # Safety
/// `v` live; `n` valid for `nn` bytes; `out` valid for `count` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_vars_get_floats(
    v: *const Vars,
    n: *const u8,
    nn: usize,
    out: *mut f32,
    count: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let out = std::slice::from_raw_parts_mut(out, count);
        (&*v).get_floats(units(n, nn), out)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn typed_round_trip() {
        let mut v = Vars::default();
        v.add_bool(b"b", true);
        v.add_int(b"i", -7);
        v.add_float(b"f", 1.5);
        v.add_floats(b"v", &[1.0, 2.5]);
        v.set(b"s", b"x y");
        assert!(v.get_bool(b"b", false));
        assert_eq!(v.get_int(b"i", 0), -7);
        assert_eq!(v.get_float(b"f", 0.0), 1.5);
        assert_eq!(v.get(b"v"), Some(&b"1.000000   2.500000"[..]));
        let mut out = [0.0; 2];
        assert!(v.get_floats(b"v", &mut out));
        assert_eq!(out, [1.0, 2.5]);
        assert!(!v.get_floats(b"s", &mut [0.0; 3]));
        assert_eq!(v.get_int(b"missing", 9), 9);
        assert!(!v.get_bool(b"i", true));
    }
}
