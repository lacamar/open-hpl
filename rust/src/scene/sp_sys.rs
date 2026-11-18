//! SP: state of `cParticleSystem` and `cParticleSystemData`. The C++ classes are shells holding one
//! pointer. Emitters are `Pe` boxes (sp_emit.rs) paired with their C++ shell; emitter data stay
//! C++ objects, tracked here as opaque pointers.

use super::h12b_psys::{PsHost, apply_all, set_visible, update_logic};
use super::sp_emit::{Pe, apply};
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
    emitters: Vec<*mut Pe>,
    data_name: Vec<u8>,
    shell: *mut c_void,
}

impl Sys {
    pub fn pubs(&self) -> &SpPub {
        &self.p
    }

    fn new(data: *mut c_void, shell: *mut c_void) -> Self {
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
            shell,
        }
    }
}

pub fn hpl3_color() -> bool {
    HPL3_COLOR.load(Ordering::Relaxed)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_sp_new(data: *mut c_void, shell: *mut c_void) -> *mut Sys {
    Box::into_raw(Box::new(Sys::new(data, shell)))
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
/// `s` valid, `e` from `ohpl_spe_new` and owned by its C++ shell.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sp_add_emitter(s: *mut Sys, e: *mut Pe) {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    // SAFETY: caller contract.
    unsafe { &mut *e }.attach(s, s.shell);
    s.emitters.push(e);
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
        // SAFETY: emitters stay live until their shells are deleted with the system.
        .map_or(std::ptr::null_mut(), |&e| unsafe { &*e }.ctx())
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
    hpl3_color()
}

fn pe_at(c: *mut c_void, i: i32) -> *mut Pe {
    // SAFETY: `c` is the `Sys` of the `PsHost` built below; indices come from its own count.
    unsafe { &*c.cast::<Sys>() }
        .emitters
        .get(usize::try_from(i).unwrap_or(usize::MAX))
        .copied()
        .unwrap_or(std::ptr::null_mut())
}

unsafe extern "C" fn h_count(c: *mut c_void) -> i32 {
    // SAFETY: see `pe_at`.
    unsafe { &*c.cast::<Sys>() }.emitters.len() as i32
}

unsafe extern "C" fn h_warm_time(c: *mut c_void, i: i32) -> f32 {
    // SAFETY: see `pe_at`.
    unsafe { &*pe_at(c, i) }.warm_time()
}

unsafe extern "C" fn h_warm_steps(c: *mut c_void, i: i32) -> f32 {
    // SAFETY: see `pe_at`.
    unsafe { &*pe_at(c, i) }.warm_steps()
}

unsafe extern "C" fn h_update(c: *mut c_void, i: i32, dt: f32) {
    // SAFETY: see `pe_at`.
    unsafe { super::sp_emit::ohpl_spe_update_logic(pe_at(c, i), dt) };
}

unsafe extern "C" fn h_apply(c: *mut c_void, i: i32, op: i32, arg: bool) {
    // SAFETY: see `pe_at`.
    unsafe { apply(pe_at(c, i), op, arg) };
}

unsafe extern "C" fn h_query(c: *mut c_void, i: i32, q: i32) -> bool {
    // SAFETY: see `pe_at`.
    let pe = unsafe { &*pe_at(c, i) };
    if q == 0 { pe.is_dead() } else { pe.is_dying() }
}

fn ps_host(s: *mut Sys) -> PsHost {
    PsHost {
        ctx: s.cast(),
        emitter_count: h_count,
        warm_up_time: h_warm_time,
        warm_up_steps: h_warm_steps,
        update: h_update,
        apply: h_apply,
        query: h_query,
    }
}

/// `cParticleSystem::UpdateLogic`.
///
/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sp_update_logic(s: *mut Sys, active: bool, dt: f32) {
    // SAFETY: caller contract; `first` is copied out so host calls never see a live reference.
    let mut first = unsafe { (*s).p.first_update };
    update_logic(&ps_host(s), &mut first, active, dt);
    // SAFETY: caller contract.
    unsafe { (*s).p.first_update = first };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sp_set_visible(s: *mut Sys, visible: bool) {
    // SAFETY: as `ohpl_sp_update_logic`.
    let mut v = unsafe { (*s).p.is_visible };
    set_visible(&ps_host(s), &mut v, visible);
    // SAFETY: caller contract.
    unsafe { (*s).p.is_visible = v };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sp_apply_all(s: *mut Sys, op: i32, arg: bool) {
    apply_all(&ps_host(s), op, arg);
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sp_all(s: *mut Sys, query: i32) -> bool {
    ps_host(s).all(query)
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
        let s = ohpl_sp_new(std::ptr::null_mut(), std::ptr::null_mut());
        // SAFETY: `s` is fresh.
        unsafe {
            assert!((*s).p.remove_when_dead && (*s).p.is_visible && (*s).p.first_update);
            assert_eq!((*s).p.max_fade_end, 110.0);
            assert!(ohpl_sp_emitter(s, 0).is_null());
            assert_eq!(ohpl_sp_emitter_count(s), 0);
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
