//! SP: state of `cParticleSystem` and `cParticleSystemData`. The C++ classes are shells holding one
//! pointer; the emitters and emitter data stay C++ objects, tracked here as opaque pointers.

use crate::types::{Color, Vector3f};
use std::ffi::c_void;
use std::sync::atomic::{AtomicBool, Ordering};

static HPL3_COLOR: AtomicBool = AtomicBool::new(false);

/// Read by C++ through `OhplSpPub`; must stay first in `Sys`.
#[repr(C)]
pub struct SpPub {
    pub color: Color,
    pub light_color: Color,
    pub brightness: f32,
    pub light_brightness: f32,
    pub min_fade_start: f32,
    pub min_fade_end: f32,
    pub max_fade_start: f32,
    pub max_fade_end: f32,
    pub data_size: Vector3f,
    pub light: *mut c_void,
    pub data: *mut c_void,
    pub manager: *mut c_void,
    pub remove_when_dead: bool,
    pub is_visible: bool,
    pub fade_at_distance: bool,
    pub first_update: bool,
}

#[repr(C)]
pub struct Sys {
    p: SpPub,
    emitters: Vec<*mut c_void>,
    data_name: Vec<u8>,
}

impl Sys {
    fn new(data: *mut c_void) -> Self {
        let white = Color {
            r: 1.0,
            g: 1.0,
            b: 1.0,
            a: 1.0,
        };
        Self {
            p: SpPub {
                color: white,
                light_color: white,
                brightness: 1.0,
                light_brightness: 1.0,
                min_fade_start: 2.0,
                min_fade_end: 1.0,
                max_fade_start: 100.0,
                max_fade_end: 110.0,
                data_size: Vector3f::default(),
                light: std::ptr::null_mut(),
                data,
                manager: std::ptr::null_mut(),
                remove_when_dead: true,
                is_visible: true,
                fade_at_distance: false,
                first_update: true,
            },
            emitters: Vec::new(),
            data_name: Vec::new(),
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_sp_new(data: *mut c_void) -> *mut Sys {
    Box::into_raw(Box::new(Sys::new(data)))
}

/// # Safety
/// `s` from `ohpl_sp_new` (or null), not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sp_free(s: *mut Sys) {
    if !s.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(s) });
    }
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sp_add_emitter(s: *mut Sys, e: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { &mut *s }.emitters.push(e);
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sp_emitter_count(s: *const Sys) -> i32 {
    // SAFETY: caller contract.
    unsafe { &*s }.emitters.len() as i32
}

/// Out of range indices return null (C++ indexed a vector unchecked).
///
/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sp_emitter(s: *const Sys, i: i32) -> *mut c_void {
    // SAFETY: caller contract.
    let v = &unsafe { &*s }.emitters;
    usize::try_from(i)
        .ok()
        .and_then(|i| v.get(i))
        .copied()
        .unwrap_or(std::ptr::null_mut())
}

/// # Safety
/// `s` valid, `p` readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sp_set_data_name(s: *mut Sys, p: *const u8, n: usize) {
    // SAFETY: caller contract.
    unsafe { &mut *s }.data_name = unsafe { std::slice::from_raw_parts(p, n) }.to_vec();
}

/// # Safety
/// `s` valid, `n` writable; the returned bytes live until the next `ohpl_sp_set_data_name`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sp_data_name(s: *const Sys, n: *mut usize) -> *const u8 {
    // SAFETY: caller contract.
    let v = &unsafe { &*s }.data_name;
    unsafe { *n = v.len() };
    v.as_ptr()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_sp_set_hpl3_color(on: bool) {
    HPL3_COLOR.store(on, Ordering::Relaxed);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_sp_hpl3_color() -> bool {
    HPL3_COLOR.load(Ordering::Relaxed)
}

pub struct SpData {
    emitters: Vec<*mut c_void>,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_spd_new() -> *mut SpData {
    Box::into_raw(Box::new(SpData {
        emitters: Vec::new(),
    }))
}

/// # Safety
/// `d` from `ohpl_spd_new` (or null), not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spd_free(d: *mut SpData) {
    if !d.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(d) });
    }
}

/// # Safety
/// `d` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spd_add(d: *mut SpData, e: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { &mut *d }.emitters.push(e);
}

/// # Safety
/// `d` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spd_count(d: *const SpData) -> u32 {
    // SAFETY: caller contract.
    unsafe { &*d }.emitters.len() as u32
}

/// Out of range indices return null.
///
/// # Safety
/// `d` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spd_get(d: *const SpData, i: i32) -> *mut c_void {
    // SAFETY: caller contract.
    let v = &unsafe { &*d }.emitters;
    usize::try_from(i)
        .ok()
        .and_then(|i| v.get(i))
        .copied()
        .unwrap_or(std::ptr::null_mut())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn defaults_and_emitters() {
        let s = ohpl_sp_new(std::ptr::null_mut());
        // SAFETY: `s` is fresh.
        unsafe {
            assert!((*s).p.remove_when_dead && (*s).p.is_visible && (*s).p.first_update);
            assert_eq!((*s).p.max_fade_end, 110.0);
            assert!(ohpl_sp_emitter(s, 0).is_null());
            ohpl_sp_add_emitter(s, 8 as *mut c_void);
            assert_eq!(ohpl_sp_emitter_count(s), 1);
            assert_eq!(ohpl_sp_emitter(s, 0) as usize, 8);
            assert!(ohpl_sp_emitter(s, -1).is_null());
            ohpl_sp_set_data_name(s, b"fire".as_ptr(), 4);
            let mut n = 0;
            let p = ohpl_sp_data_name(s, &mut n);
            assert_eq!(std::slice::from_raw_parts(p, n), b"fire");
            ohpl_sp_free(s);
        }
    }

    #[test]
    fn pub_layout() {
        assert_eq!(std::mem::size_of::<SpPub>(), 104);
        assert_eq!(std::mem::offset_of!(Sys, p), 0);
    }

    #[test]
    fn data_list() {
        let d = ohpl_spd_new();
        // SAFETY: `d` is fresh.
        unsafe {
            ohpl_spd_add(d, 4 as *mut c_void);
            assert_eq!(ohpl_spd_count(d), 1);
            assert_eq!(ohpl_spd_get(d, 0) as usize, 4);
            assert!(ohpl_spd_get(d, 1).is_null());
            ohpl_spd_free(d);
        }
    }
}
