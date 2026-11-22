//! C ABI for the GX classes, declared in `graphics/RustGx.h`. Every pointer is the one the
//! matching `_new` returned.

use super::gx_decal::Decal;
use crate::types::{Color, Vector2l, Vector3f};
use std::ffi::c_void;

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gx_decal_new(ll: *mut c_void, res: *mut c_void) -> *mut Decal {
    Box::into_raw(Box::new(Decal::new(ll, res)))
}

/// # Safety
/// `d` came from `ohpl_gx_decal_new` and is freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gx_decal_free(d: *mut Decal) {
    if !d.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(d) });
    }
}

macro_rules! ffi {
    ($(fn $name:ident($d:ident $(, $a:ident: $t:ty)*) $(-> $r:ty)? $body:block)*) => {$(
        /// # Safety
        /// `d` came from `ohpl_gx_decal_new`; pointer arguments are valid for the call.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name($d: *mut Decal $(, $a: $t)*) $(-> $r)? {
            // SAFETY: caller contract.
            let $d = unsafe { &mut *$d };
            #[allow(unused_unsafe)]
            unsafe { $body }
        }
    )*};
}

ffi! {
fn ohpl_gx_decal_add_submesh(d, sub: *mut c_void) -> bool { d.add_submesh(sub) }
fn ohpl_gx_decal_add_affected(d, sub: *mut c_void) { d.add_affected(sub) }
fn ohpl_gx_decal_clear_meshes(d) { d.clear_meshes() }
fn ohpl_gx_decal_set_max_tris(d, n: i32) { d.set_max_tris(n) }
fn ohpl_gx_decal_set_position(d, v: *const Vector3f) { d.set_position(*v) }
fn ohpl_gx_decal_set_up(d, v: *const Vector3f, basis: bool) { d.set_up(*v, basis) }
fn ohpl_gx_decal_set_right(d, v: *const Vector3f, basis: bool) { d.set_right(*v, basis) }
fn ohpl_gx_decal_set_forward(d, v: *const Vector3f, basis: bool) { d.set_forward(*v, basis) }
fn ohpl_gx_decal_set_offset(d, o: f32) { d.set_offset(o) }
fn ohpl_gx_decal_set_size(d, v: *const Vector3f) { d.set_size(*v) }
fn ohpl_gx_decal_set_material(d, name: *const u8, len: usize) {
    let bytes = if name.is_null() { &[][..] } else { std::slice::from_raw_parts(name, len) };
    d.set_material(&String::from_utf8_lossy(bytes))
}
fn ohpl_gx_decal_set_color(d, c: *const Color) { d.set_color(*c) }
fn ohpl_gx_decal_set_sub_div(d, v: *const Vector2l) { d.set_sub_div(*v) }
fn ohpl_gx_decal_set_current_sub_div(d, i: i32) { d.set_current_sub_div(i) }
fn ohpl_gx_decal_is_updated(d) -> bool { d.is_updated() }
fn ohpl_gx_decal_compile(d) -> bool { d.compile() }
fn ohpl_gx_decal_can_create(d) -> bool { d.can_create() }
fn ohpl_gx_decal_create_mesh(d) -> *mut c_void { d.create_mesh() }
fn ohpl_gx_decal_material(d) -> *mut c_void { d.material() }
}
