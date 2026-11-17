//! The reference-counted `string` of the Amnesia scripts: a byte buffer behind a reference count.

use std::cell::{Cell, UnsafeCell};
use std::ffi::{c_char, c_int};

unsafe extern "C" {
    fn snprintf(buf: *mut c_char, n: usize, fmt: *const c_char, ...) -> c_int;
}

/// What the engine and the natives hold as `string@`.
pub struct ScriptString {
    rc: Cell<i32>,
    buf: UnsafeCell<Vec<u8>>,
}

impl ScriptString {
    /// A new string with one reference, owned by the caller.
    pub fn new_raw(b: &[u8]) -> *mut ScriptString {
        Box::into_raw(Box::new(ScriptString {
            rc: Cell::new(1),
            buf: UnsafeCell::new(b.to_vec()),
        }))
    }

    pub fn add_ref(&self) {
        self.rc.set(self.rc.get().wrapping_add(1));
    }

    pub fn bytes(&self) -> &[u8] {
        // SAFETY: the buffer is only touched through `with`, which runs no script code, so no
        // mutable access is live while a native reads.
        unsafe { (*self.buf.get()).as_slice() }
    }

    fn with<R>(&self, f: impl FnOnce(&mut Vec<u8>) -> R) -> R {
        // SAFETY: as `bytes`; `f` is plain buffer code.
        f(unsafe { &mut *self.buf.get() })
    }

    pub fn assign(&self, b: &[u8]) {
        let b = b.to_vec();
        self.with(|v| *v = b);
    }

    pub fn append(&self, b: &[u8]) {
        let b = b.to_vec();
        self.with(|v| v.extend_from_slice(&b));
    }

    pub fn resize(&self, n: usize) {
        self.with(|v| v.resize(n, 0));
    }

    /// Address of byte `i` for `opIndex`, or null past the end.
    pub fn byte_addr(&self, i: usize) -> *mut u8 {
        self.with(|v| {
            v.get_mut(i)
                .map_or(std::ptr::null_mut(), std::ptr::from_mut)
        })
    }

    pub fn concat(a: &[u8], b: &[u8]) -> *mut ScriptString {
        Self::new_raw(&[a, b].concat())
    }
}

/// Drops one reference.
///
/// # Safety
/// `s` is null or a live string the caller owns a reference to.
pub unsafe fn release(s: *const ScriptString) {
    // SAFETY: caller contract.
    let Some(r) = (unsafe { s.as_ref() }) else {
        return;
    };
    r.rc.set(r.rc.get().wrapping_sub(1));
    if r.rc.get() == 0 {
        // SAFETY: last reference; created by `new_raw`.
        drop(unsafe { Box::from_raw(s.cast_mut()) });
    }
}

/// `std::string` ordering: bytes, then length.
pub fn cmp(a: &[u8], b: &[u8]) -> i32 {
    match a.cmp(b) {
        std::cmp::Ordering::Less => -1,
        std::cmp::Ordering::Equal => 0,
        std::cmp::Ordering::Greater => 1,
    }
}

/// `sprintf("%g")` of a double, as the add-on printed floats and doubles.
pub fn fmt_g(v: f64) -> Vec<u8> {
    let mut buf = [0u8; 100];
    // SAFETY: the format takes one double; the buffer bound is passed.
    let n = unsafe { snprintf(buf.as_mut_ptr().cast(), buf.len(), c"%g".as_ptr(), v) };
    let n = usize::try_from(n).unwrap_or(0).min(buf.len() - 1);
    buf.get(..n).unwrap_or_default().to_vec()
}

pub fn fmt_bool(b: bool) -> Vec<u8> {
    if b {
        b"true".to_vec()
    } else {
        b"false".to_vec()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn formats_like_printf() {
        assert_eq!(fmt_g(0.5), b"0.5");
        assert_eq!(fmt_g(1e10), b"1e+10");
        assert_eq!(fmt_g(f64::from(1.25f32)), b"1.25");
        assert_eq!(fmt_g(100000.0), b"100000");
        assert_eq!(fmt_g(1234567.0), b"1.23457e+06");
        assert_eq!(fmt_bool(true), b"true");
    }

    #[test]
    fn compares_like_std_string() {
        assert_eq!(cmp(b"a", b"b"), -1);
        assert_eq!(cmp(b"b", b"a"), 1);
        assert_eq!(cmp(b"ab", b"a"), 1);
        assert_eq!(cmp(b"", b""), 0);
        assert_eq!(cmp(b"\xff", b"a"), 1);
    }

    #[test]
    fn owns_and_releases() {
        let s = ScriptString::new_raw(b"ab");
        // SAFETY: fresh string.
        unsafe {
            (*s).add_ref();
            (*s).append(b"cd");
            assert_eq!((*s).bytes(), b"abcd");
            assert!(!(*s).byte_addr(3).is_null());
            assert!((*s).byte_addr(4).is_null());
            release(s);
            assert_eq!((*s).bytes(), b"abcd");
            release(s);
        }
    }
}
