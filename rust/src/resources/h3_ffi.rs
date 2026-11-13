//! C ABI of the H3 manager flows, mirrored in `HPL2/core/include/resources/RustResourcesH3.h`.

use super::h3_ops::{Flow, LOG_CREATE_AND_DELETE, Ops, Str};
use super::manager::Manager;
use crate::string_ffi::{Sink, emit, units};
use std::ffi::c_void;
use std::sync::atomic::Ordering;

/// # Safety
/// `m` and `ops` are live for the call.
unsafe fn flow<'a>(m: *mut Manager, ops: *const Ops) -> Flow<'a> {
    // SAFETY: caller contract.
    unsafe { Flow::from_raw(m, &*ops) }
}

/// # Safety
/// `p` points to `n` valid `Str` whose pointers are valid for their lengths.
unsafe fn byte_list<'a>(p: *const Str, n: usize) -> Vec<&'a [u8]> {
    // SAFETY: caller contract.
    unsafe { units(p, n) }
        .iter()
        // SAFETY: caller contract.
        .map(|b| unsafe { units(b.p, b.n) })
        .collect()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h3_log_create_get() -> bool {
    LOG_CREATE_AND_DELETE.load(Ordering::Relaxed)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h3_log_create_set(on: bool) {
    LOG_CREATE_AND_DELETE.store(on, Ordering::Relaxed);
}

macro_rules! ffi {
    (fn $name:ident($m:ident, $o:ident $(, $a:ident: $t:ty)*) $(-> $r:ty)? $body:block) => {
        /// # Safety
        /// `m` and `ops` live; pointer arguments valid for their lengths.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name($m: *mut Manager, $o: *const Ops $(, $a: $t)*) $(-> $r)? {
            // SAFETY: caller contract.
            unsafe { $body }
        }
    };
}

ffi!(fn ohpl_h3f_begin(m, o, name: *const u8, n: usize) {
    flow(m, o).begin(units(name, n));
});

ffi!(fn ohpl_h3f_end(m, o) {
    flow(m, o).end();
});

ffi!(fn ohpl_h3f_add(m, o, res: *mut c_void, log: bool, to_set: bool) {
    flow(m, o).add(res, log, to_set);
});

ffi!(fn ohpl_h3f_inc_user(m, o, res: *mut c_void) {
    flow(m, o).inc_user(res);
});

ffi!(fn ohpl_h3f_tabs(m, o, sink: Sink<u8>, ctx: *mut c_void) {
    emit(sink, ctx, &flow(m, o).tabs());
});

ffi!(fn ohpl_h3f_find_loaded(m, o, name: *const u8, n: usize, count: *mut i32, sink: Sink<u32>, ctx: *mut c_void) -> *mut c_void {
    let mut f = flow(m, o);
    if !count.is_null() {
        f.seed = *count;
    }
    let lk = f.find_loaded(units(name, n), !count.is_null());
    if !count.is_null() {
        *count = lk.count;
    }
    emit(sink, ctx, &lk.path);
    lk.res
});

ffi!(fn ohpl_h3f_destroy_all(m, o) {
    flow(m, o).destroy_all();
});

ffi!(fn ohpl_h3f_destroy_unused(m, o, keep: i64) {
    flow(m, o).destroy_unused(keep);
});

ffi!(fn ohpl_h3f_shutdown(m, o, which: i32) {
    flow(m, o).shutdown(which);
});

ffi!(fn ohpl_h3f_destroy_standard(m, o, res: *mut c_void) {
    flow(m, o).destroy_standard(res);
});

ffi!(fn ohpl_h3f_model(m, o, mesh: bool, name: *const u8, n: usize, exts: *const Str, ne: usize, flag: i32) -> *mut c_void {
    flow(m, o).model(mesh, units(name, n), &byte_list(exts, ne), flag)
});

ffi!(fn ohpl_h3f_ent_file(m, o, name: *const u8, n: usize) -> *mut c_void {
    flow(m, o).ent_file(units(name, n))
});

ffi!(fn ohpl_h3f_script(m, o, name: *const u8, n: usize, msgs: *mut c_void) -> *mut c_void {
    flow(m, o).script(units(name, n), msgs)
});

ffi!(fn ohpl_h3f_font(m, o, name: *const u8, n: usize, size: i32, first: i32, last: i32) -> *mut c_void {
    flow(m, o).font(units(name, n), size, first, last)
});

ffi!(fn ohpl_h3f_material(m, o, name: *const u8, n: usize) -> *mut c_void {
    flow(m, o).material(units(name, n))
});

ffi!(fn ohpl_h3f_material_custom(m, o, res: *mut c_void) {
    flow(m, o).material_custom(res);
});

ffi!(fn ohpl_h3f_material_physics_name(m, o, name: *const u8, n: usize, sink: Sink<u8>, ctx: *mut c_void) {
    emit(sink, ctx, &flow(m, o).material_physics_name(units(name, n)));
});

ffi!(fn ohpl_h3f_sound_entity(m, o, name: *const u8, n: usize, has_resolver: bool) -> *mut c_void {
    flow(m, o).sound_entity(units(name, n), has_resolver)
});

ffi!(fn ohpl_h3f_sound_entity_add_custom(m, o, name: *const u8, n: usize, data: *mut c_void) {
    flow(m, o).sound_entity_add_custom(units(name, n), data);
});

ffi!(fn ohpl_h3f_sound_entity_preload(m, o, name: *const u8, n: usize, has_resolver: bool) {
    flow(m, o).sound_entity_preload(units(name, n), has_resolver);
});

ffi!(fn ohpl_h3f_video(m, o, name: *const u8, n: usize) -> *mut c_void {
    flow(m, o).video(units(name, n))
});

ffi!(fn ohpl_h3f_video_destroy(m, o, res: *mut c_void) {
    flow(m, o).video_destroy(res);
});

ffi!(fn ohpl_h3f_video_update(m, o, dt_bits: i64) {
    flow(m, o).video_update(dt_bits);
});

ffi!(fn ohpl_h3f_sound(m, o, name: *const u8, n: usize, stream: bool, looped: bool, formats: *const Str, nf: usize) -> *mut c_void {
    flow(m, o).sound(units(name, n), stream, looped, &byte_list(formats, nf))
});

ffi!(fn ohpl_h3f_sound_destroy(m, o, res: *mut c_void) {
    flow(m, o).sound_destroy(res);
});

ffi!(fn ohpl_h3f_sound_destroy_all(m, o) {
    flow(m, o).sound_destroy_all();
});

ffi!(fn ohpl_h3f_particle_by_type(m, o, name: *const u8, n: usize, ty: *const u8, tn: usize, x: f32, y: f32, z: f32) -> *mut c_void {
    flow(m, o).particle_by_type(units(name, n), units(ty, tn), [x, y, z])
});

ffi!(fn ohpl_h3f_particle_by_element(m, o, name: *const u8, n: usize, dn: *const u8, dl: usize, element: *mut c_void, x: f32, y: f32, z: f32) -> *mut c_void {
    flow(m, o).particle_by_element(units(name, n), units(dn, dl), element, [x, y, z])
});

ffi!(fn ohpl_h3f_particle_add_data(m, o, res: *mut c_void) {
    flow(m, o).add(res, true, true);
});

ffi!(fn ohpl_h3f_particle_preload(m, o, name: *const u8, n: usize) {
    flow(m, o).particle_preload(units(name, n));
});

ffi!(fn ohpl_h3f_particle_destroy(m, o, res: *mut c_void) {
    flow(m, o).particle_destroy(res);
});

ffi!(fn ohpl_h3f_gpu_shader(m, o, name: *const u8, n: usize, ty: i32, vars: *mut c_void, has_transpile: bool, fragment: i32) -> *mut c_void {
    flow(m, o).gpu_shader(units(name, n), ty, vars, has_transpile, fragment)
});

ffi!(fn ohpl_h3f_gpu_destroy(m, o, res: *mut c_void) {
    flow(m, o).gpu_destroy(res);
});

ffi!(fn ohpl_h3f_texture_simple(m, o, name: *const u8, n: usize, mip: bool, usage: i32, ty: i32, level: i32, exts: *const Str, ne: usize) -> *mut c_void {
    flow(m, o).texture_simple(units(name, n), mip, usage, ty, level, &byte_list(exts, ne))
});

ffi!(fn ohpl_h3f_texture_anim(m, o, name: *const u8, n: usize, mip: bool, ty: i32, usage: i32, level: i32, exts: *const Str, ne: usize) -> *mut c_void {
    flow(m, o).texture_anim(units(name, n), mip, ty, usage, level, &byte_list(exts, ne))
});

ffi!(fn ohpl_h3f_texture_cube(m, o, name: *const u8, n: usize, mip: bool, usage: i32, level: i32, cube_type: i32, exts: *const Str, ne: usize) -> *mut c_void {
    flow(m, o).texture_cube(units(name, n), mip, usage, level, cube_type, &byte_list(exts, ne))
});

ffi!(fn ohpl_h3f_texture_destroy(m, o, res: *mut c_void) {
    flow(m, o).texture_destroy(res);
});

ffi!(fn ohpl_h3f_texture_update(m, o, dt_bits: i64) {
    flow(m, o).texture_update(dt_bits);
});

ffi!(fn ohpl_h3f_mem_usage(m, o) -> i32 {
    let _ = o;
    (*m).mem_usage
});

ffi!(fn ohpl_h3f_image_init(m, o, w: i32, h: i32) {
    let _ = o;
    (*m).frames.size = (w, h);
});

ffi!(fn ohpl_h3f_image_create_in_frame(m, o, name: *const u8, n: usize, handle: i32, exts: *const Str, ne: usize) -> *mut c_void {
    flow(m, o).image_create_in_frame(units(name, n), handle, &byte_list(exts, ne))
});

ffi!(fn ohpl_h3f_image_from_bitmap(m, o, bmp: *mut c_void, handle: i32) -> *mut c_void {
    flow(m, o).image_from_bitmap(bmp, handle)
});

ffi!(fn ohpl_h3f_image_custom_frame(m, o, tex: *mut c_void) -> *mut c_void {
    flow(m, o).image_custom_frame(tex)
});

ffi!(fn ohpl_h3f_image_frame_texture(m, o, handle: i32) -> *mut c_void {
    flow(m, o).image_frame_texture(handle)
});

ffi!(fn ohpl_h3f_image_create_frame(m, o, w: i32, h: i32) -> i32 {
    flow(m, o).image_create_frame(w, h)
});

ffi!(fn ohpl_h3f_image_set_frame_locked(m, o, handle: i32, locked: bool) {
    flow(m, o).image_set_frame_locked(handle, locked);
});

ffi!(fn ohpl_h3f_image_flush_all(m, o) -> i32 {
    flow(m, o).image_flush_all()
});

ffi!(fn ohpl_h3f_image_reorganize_all(m, o) {
    flow(m, o).image_reorganize_all();
});

ffi!(fn ohpl_h3f_image_destroy(m, o, res: *mut c_void) {
    flow(m, o).image_destroy(res);
});

ffi!(fn ohpl_h3f_for_each(m, o, what: i32, arg: i64) {
    flow(m, o).for_each(what, arg);
});

ffi!(fn ohpl_h3f_material_blank(m, o, on: bool) {
    let _ = o;
    (*m).mat_blank = on;
});
