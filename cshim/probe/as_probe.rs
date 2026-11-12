#![allow(clippy::missing_safety_doc)]
use std::os::raw::{c_char, c_void};

unsafe extern "C" {
    fn probe_str_data(s: *const c_void) -> *const c_char;
    fn probe_str_len(s: *const c_void) -> usize;
    fn probe_str_new(p: *const c_char, n: usize) -> *mut c_void;
    fn probe_report(name: *const c_char, ok: i32);
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct V3 {
    x: f32,
    y: f32,
    z: f32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct I3 {
    a: i32,
    b: i32,
    c: i32,
}
#[repr(C)]
pub struct Obj {
    rc: i32,
    v: i32,
}

#[unsafe(no_mangle)]
pub extern "C" fn rs_add_i(a: i32, b: i32) -> i32 {
    a + b
}
#[unsafe(no_mangle)]
pub extern "C" fn rs_add_f(a: f32, b: f32) -> f32 {
    a + b
}
#[unsafe(no_mangle)]
pub extern "C" fn rs_add_d(a: f64, b: f64) -> f64 {
    a + b
}
#[unsafe(no_mangle)]
pub extern "C" fn rs_not(a: bool) -> bool {
    !a
}
#[unsafe(no_mangle)]
pub extern "C" fn rs_add_i64(a: i64, b: i64) -> i64 {
    a + b
}
#[unsafe(no_mangle)]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn rs_sum9(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32, g: i32, h: i32, i: i32) -> i32 {
    a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h + 9 * i
}
#[unsafe(no_mangle)]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn rs_sum9d(a: f64, b: f64, c: f64, d: f64, e: f64, f: f64, g: f64, h: f64, i: f64) -> f64 {
    a + 2.0 * b + 3.0 * c + 4.0 * d + 5.0 * e + 6.0 * f + 7.0 * g + 8.0 * h + 9.0 * i
}
#[unsafe(no_mangle)]
pub extern "C" fn rs_mixed(a: i32, b: f32, c: f64, d: bool, e: i64) -> f64 {
    a as f64 + b as f64 + c + if d { 100.0 } else { 0.0 } + e as f64
}
#[unsafe(no_mangle)]
pub extern "C" fn rs_v3_add(a: &V3, b: &V3) -> V3 {
    V3 { x: a.x + b.x, y: a.y + b.y, z: a.z + b.z }
}
#[unsafe(no_mangle)]
pub extern "C" fn rs_v3_dot_val(a: V3, b: V3) -> f32 {
    a.x * b.x + a.y * b.y + a.z * b.z
}
#[unsafe(no_mangle)]
pub extern "C" fn rs_i3_inc(a: &mut I3) {
    *a = I3 { a: 100, b: 101, c: 102 };
}
#[unsafe(no_mangle)]
pub extern "C" fn rs_i3_make(a: i32) -> I3 {
    I3 { a, b: a + 1, c: a + 2 }
}
#[unsafe(no_mangle)]
pub extern "C" fn rs_v3_len2(v: &V3) -> f32 {
    v.x * v.x + v.y * v.y + v.z * v.z
}
#[unsafe(no_mangle)]
pub extern "C" fn rs_v3_m_scale(this: &mut V3, k: f32) {
    this.x *= k;
    this.y *= k;
    this.z *= k;
}
#[unsafe(no_mangle)]
pub extern "C" fn rs_v3_m_sum(this: &V3) -> f32 {
    this.x + this.y + this.z
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rs_v3_ctor(p: *mut V3) {
    unsafe { *p = V3 { x: 1.0, y: 2.0, z: 3.0 } }
}
#[unsafe(no_mangle)]
pub extern "C" fn rs_obj_factory() -> *mut Obj {
    Box::into_raw(Box::new(Obj { rc: 1, v: 40 }))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rs_obj_addref(o: *mut Obj) {
    unsafe { (*o).rc += 1 }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rs_obj_release(o: *mut Obj) {
    unsafe {
        (*o).rc -= 1;
        if (*o).rc == 0 {
            drop(Box::from_raw(o));
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rs_obj_get(o: *mut Obj) -> i32 {
    unsafe { (*o).v }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rs_obj_set(o: *mut Obj, v: i32) {
    unsafe { (*o).v = v }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rs_obj_rc(o: *mut Obj) -> i32 {
    unsafe { (*o).rc }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rs_obj_pass(o: *mut Obj) -> i32 {
    unsafe {
        let v = (*o).v + 1;
        rs_obj_release(o);
        v
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rs_obj_ret(o: *mut Obj) -> *mut Obj {
    // the arg's owned reference is transferred to the return value
    o
}
#[unsafe(no_mangle)]
pub extern "C" fn rs_obj_null() -> *mut Obj {
    std::ptr::null_mut()
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rs_str_len(s: *const c_void) -> i32 {
    unsafe { probe_str_len(s) as i32 }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rs_str_first(s: *const c_void) -> i32 {
    unsafe { *probe_str_data(s) as i32 }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rs_str_upper(s: *const c_void) -> *mut c_void {
    unsafe {
        let n = probe_str_len(s);
        let b: Vec<u8> = std::slice::from_raw_parts(probe_str_data(s) as *const u8, n).iter().map(u8::to_ascii_uppercase).collect();
        probe_str_new(b.as_ptr() as *const c_char, n)
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rs_str_inout(s: *mut c_void) -> i32 {
    unsafe { probe_str_len(s) as i32 }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn rs_check(ok: bool, name: *const c_void) {
    unsafe {
        let n = probe_str_len(name);
        let mut v = std::slice::from_raw_parts(probe_str_data(name) as *const u8, n).to_vec();
        v.push(0);
        probe_report(v.as_ptr() as *const c_char, ok as i32);
    }
}
static mut RS_PROP: i32 = 77;
#[unsafe(no_mangle)]
pub extern "C" fn rs_prop_ptr() -> *mut i32 {
    &raw mut RS_PROP
}
