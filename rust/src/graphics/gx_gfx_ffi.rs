//! C ABI for `gx_graphics.rs`, declared in `graphics/RustGx.h`.

use super::gx_gfx_host::text;
use super::gx_graphics::{self, Graphics, InitArgs};
use crate::types::Vector2l;
use std::ffi::{c_char, c_void};
use std::ptr::null_mut;

/// # Safety
/// `p` is null or holds `len` bytes.
unsafe fn str_of<'a>(p: *const c_char, len: usize) -> &'a str {
    // SAFETY: caller contract.
    unsafe { text(p, len) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gx_gfx_new(me: *mut c_void, ll: *mut c_void) -> *mut Graphics {
    Box::into_raw(Box::new(Graphics::new(me, ll)))
}

/// # Safety
/// `g` came from `ohpl_gx_gfx_new` and is freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gx_gfx_free(g: *mut Graphics) {
    if !g.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(g) });
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gx_gfx_temp_texture_type() -> i32 {
    gx_graphics::temp_texture_type()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gx_gfx_set_temp_texture_type(t: i32) {
    gx_graphics::set_temp_texture_type(t);
}

macro_rules! ffi {
    ($(fn $name:ident($g:ident $(, $a:ident: $t:ty)*) $(-> $r:ty)? $body:block)*) => {$(
        /// # Safety
        /// `g` came from `ohpl_gx_gfx_new`; pointer arguments are valid for the call.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name($g: *mut Graphics $(, $a: $t)*) $(-> $r)? {
            // SAFETY: caller contract.
            let $g = unsafe { &mut *$g };
            #[allow(unused_unsafe)]
            unsafe { $body }
        }
    )*};
}

ffi! {
fn ohpl_gx_gfx_init(
    g,
    width: i32,
    height: i32,
    display: i32,
    bpp: i32,
    fullscreen: i32,
    multisampling: i32,
    gpu_format: i32,
    caption: *const c_char,
    caption_len: usize,
    pos: *const Vector2l,
    res: *mut c_void,
    flags: u32
) -> bool {
    let args = InitArgs {
        width,
        height,
        display,
        bpp,
        fullscreen,
        multisampling,
        gpu_format,
        caption: str_of(caption, caption_len),
        pos: *pos,
        flags,
    };
    g.init(&args, res)
}
fn ohpl_gx_gfx_update(g, dt: f32) { g.update(dt) }
fn ohpl_gx_gfx_renderer(g, kind: i32) -> *mut c_void { g.renderer(kind) }
fn ohpl_gx_gfx_reload_renderer_data(g) { g.reload_renderer_data() }
fn ohpl_gx_gfx_create_frame_buffer(g, n: *const c_char, len: usize) -> *mut c_void {
    g.create_frame_buffer(str_of(n, len))
}
fn ohpl_gx_gfx_destroy_frame_buffer(g, fb: *mut c_void) { g.destroy_frame_buffer(fb) }
fn ohpl_gx_gfx_temp_frame_buffer(g, size: *const Vector2l, format: i32, index: i32) -> *mut c_void {
    g.temp_frame_buffer(*size, format, index)
}
fn ohpl_gx_gfx_create_depth_stencil(
    g,
    size: *const Vector2l,
    depth: i32,
    stencil: i32,
    look: bool
) -> *mut c_void {
    g.create_depth_stencil(*size, depth, stencil, look)
}
fn ohpl_gx_gfx_find_depth_stencil(g, size: *const Vector2l, depth: i32, stencil: i32) -> *mut c_void {
    g.find_depth_stencil(*size, depth, stencil)
}
fn ohpl_gx_gfx_destroy_depth_stencil(g, b: *mut c_void) { g.destroy_depth_stencil(b) }
fn ohpl_gx_gfx_create_texture(g, n: *const c_char, len: usize, kind: i32, usage: i32) -> *mut c_void {
    g.create_texture(str_of(n, len), kind, usage)
}
fn ohpl_gx_gfx_destroy_texture(g, t: *mut c_void) { g.destroy_texture(t) }
fn ohpl_gx_gfx_create_composite(g) -> *mut c_void { g.create_composite() }
fn ohpl_gx_gfx_destroy_composite(g, c: *mut c_void) { g.destroy_composite(c) }
fn ohpl_gx_gfx_add_post_effect_type(g, t: *mut c_void) { g.add_post_effect_type(t) }
fn ohpl_gx_gfx_create_post_effect(g, params: *mut c_void) -> *mut c_void { g.create_post_effect(params) }
fn ohpl_gx_gfx_destroy_post_effect(g, pe: *mut c_void) { g.destroy_post_effect(pe) }
fn ohpl_gx_gfx_create_gpu_program(g, n: *const c_char, len: usize) -> *mut c_void {
    g.create_gpu_program(str_of(n, len))
}
fn ohpl_gx_gfx_create_gpu_program_from_shaders(
    g,
    n: *const c_char,
    n_len: usize,
    vtx: *const c_char,
    vtx_len: usize,
    frag: *const c_char,
    frag_len: usize,
    vars: *mut c_void
) -> *mut c_void {
    g.create_gpu_program_from_shaders(
        str_of(n, n_len),
        str_of(vtx, vtx_len),
        str_of(frag, frag_len),
        vars,
    )
}
fn ohpl_gx_gfx_destroy_gpu_program(g, p: *mut c_void) { g.destroy_gpu_program(p) }
fn ohpl_gx_gfx_add_material_type(g, t: *mut c_void, n: *const c_char, len: usize) {
    g.add_material_type(t, str_of(n, len))
}
fn ohpl_gx_gfx_material_type(g, n: *const c_char, len: usize) -> *mut c_void {
    g.material_type(str_of(n, len))
}
fn ohpl_gx_gfx_material_type_name(g, idx: usize, len: *mut usize) -> *const c_char {
    match g.material_type_names().nth(idx) {
        Some(s) => {
            *len = s.len();
            s.as_ptr().cast()
        }
        None => {
            *len = 0;
            null_mut::<c_char>().cast_const()
        }
    }
}
fn ohpl_gx_gfx_reload_materials(g) { g.reload_materials() }
fn ohpl_gx_gfx_mesh_creator(g) -> *mut c_void { g.mesh_creator() }
fn ohpl_gx_gfx_texture_creator(g) -> *mut c_void { g.texture_creator() }
fn ohpl_gx_gfx_decal_creator(g) -> *mut c_void { g.decal_creator() }
fn ohpl_gx_gfx_screen_is_setup(g) -> bool { g.screen_is_setup() }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gx_texgen_scatter(
    tex: *mut c_void,
    size: i32,
    samples: i32,
    sort: bool,
    three_d: bool,
) {
    gx_graphics::scatter_texture(tex, size, samples, sort, three_d);
}
