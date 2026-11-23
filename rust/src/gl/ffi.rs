//! C ABI for the GL backend, declared in `HPL2/core/include/impl/RustGl.h`.
//!
//! Pointers are non-null and valid for the call unless a function says it accepts null;
//! outputs do not alias inputs.

use std::ffi::{c_char, c_void};

use super::fbo::{self, Attach, DrawPlan};
use super::glsl::{self, Params};
use super::state::{GlState, TexPlan, Viewport};
use super::tables;
use super::texture::{self, Clock, CreateIn, CreatePlan, MipPlan, Props, Upload};
use super::vbuf::{self, Elem, MAX_OPS, Op};
use crate::string_ffi::{Sink, emit, units};

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<GlState>() == 96 && size_of::<Op>() == 28 && size_of::<Elem>() == 16);
    assert!(size_of::<TexPlan>() == 16 && size_of::<Viewport>() == 40);
    assert!(size_of::<CreateIn>() == 44 && size_of::<CreatePlan>() == 36);
    assert!(size_of::<Upload>() == 44 && size_of::<MipPlan>() == 24);
    assert!(size_of::<Props>() == 20 && size_of::<Clock>() == 8);
    assert!(size_of::<Attach>() == 32 && size_of::<DrawPlan>() == 28);
};

macro_rules! enum_tables {
    ($($name:ident => $f:path;)*) => {$(
        #[unsafe(no_mangle)]
        pub extern "C" fn $name(v: i32) -> u32 { $f(v) }
    )*};
}

enum_tables! {
    ohpl_gl_vertex_array => tables::vertex_array;
    ohpl_gl_vertex_type => tables::vertex_type;
    ohpl_gl_draw_type_mode => tables::draw_mode;
    ohpl_gl_usage => tables::usage;
    ohpl_gl_wrap => tables::wrap;
    ohpl_gl_pixel_format => tables::pixel_format;
    ohpl_gl_pixel_internal_format => tables::pixel_internal_format;
    ohpl_gl_compression_format => tables::compression_format;
    ohpl_gl_texture_target => tables::texture_target;
    ohpl_gl_blend => tables::blend;
    ohpl_gl_texture_param => tables::texture_param;
    ohpl_gl_texture_op => tables::texture_op;
    ohpl_gl_texture_source => tables::texture_source;
    ohpl_gl_compare_mode => tables::compare_mode;
    ohpl_gl_compare_func => tables::compare_func;
    ohpl_gl_texture_func => tables::texture_func;
    ohpl_gl_compare => tables::compare;
    ohpl_gl_stencil_op => tables::stencil_op;
    ohpl_gl_shader_type => tables::shader_type;
    ohpl_gl_depth_renderbuffer => tables::depth_renderbuffer;
    ohpl_gl_stencil_renderbuffer => tables::stencil_renderbuffer;
}

/// # Safety
/// `p` points to `n` writable items, or `n` is 0.
unsafe fn units_mut<'a, T>(p: *mut T, n: usize) -> &'a mut [T] {
    if n == 0 {
        &mut []
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts_mut(p, n) }
    }
}

macro_rules! state_fns {
    ($($name:ident($($arg:ident: $t:ty),*) -> $ret:ty => $body:expr;)*) => {$(
        /// # Safety
        /// `s` points to an initialised state.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(s: *mut GlState $(, $arg: $t)*) -> $ret {
            // SAFETY: caller contract.
            let s = unsafe { &mut *s };
            ($body)(s $(, $arg)*)
        }
    )*};
}

state_fns! {
    ohpl_gl_state_setup() -> () => |s: &mut GlState| s.setup();
    ohpl_gl_state_color_write(r: bool, g: bool, b: bool, a: bool) -> bool
        => |s: &mut GlState, r, g, b, a| s.set_color_write(r, g, b, a);
    ohpl_gl_state_depth_write(on: bool) -> bool => |s: &mut GlState, on| s.set_depth_write(on);
    ohpl_gl_state_depth_test(on: bool) -> bool => |s: &mut GlState, on| s.set_depth_test(on);
    ohpl_gl_state_depth_func(f: i32) -> bool => |s: &mut GlState, f| s.set_depth_func(f);
    ohpl_gl_state_alpha_test(on: bool) -> bool => |s: &mut GlState, on| s.set_alpha_test(on);
    ohpl_gl_state_alpha_func(f: i32, r: f32) -> bool => |s: &mut GlState, f, r| s.set_alpha_func(f, r);
    ohpl_gl_state_scissor(on: bool) -> bool => |s: &mut GlState, on| s.set_scissor(on);
    ohpl_gl_state_blend(on: bool) -> bool => |s: &mut GlState, on| s.set_blend(on);
    ohpl_gl_state_active_unit(unit: u32) -> bool => |s: &mut GlState, u| s.set_active_unit(u);
    ohpl_gl_state_cull_active(on: bool) -> () => |s: &mut GlState, on| s.cull_active = on;
    ohpl_gl_state_cull_mode(mode: i32) -> () => |s: &mut GlState, m| s.cull_mode = m;
}

/// # Safety
/// `elems` has `n` items; `out` has `MAX_OPS` writable ops.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_vb_bind_vbo(elems: *const Elem, n: usize, out: *mut Op) -> usize {
    // SAFETY: caller contract.
    unsafe { vbuf::bind_vbo(units(elems, n), units_mut(out, MAX_OPS)) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_tex_aniso_fits(max: i32, degree: f32) -> bool {
    texture::aniso_fits(max, degree)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_tex_aniso_apply(
    has_data: bool,
    cap: bool,
    max: i32,
    x: f32,
    current: f32,
) -> bool {
    texture::aniso_apply(has_data, cap, max, x, current)
}

/// # Safety
/// `c` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_clock_update(
    c: *mut Clock,
    handles: usize,
    step: f32,
    frame_time: f32,
    mode: i32,
) {
    // SAFETY: caller contract.
    unsafe { (*c).update(handles, step, frame_time, mode) }
}

/// # Safety
/// `c` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_clock_next(c: *mut Clock, handles: usize, mode: i32) {
    // SAFETY: caller contract.
    unsafe { (*c).next_frame(handles, mode) }
}

/// # Safety
/// `c` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_clock_prev(c: *mut Clock, handles: usize, mode: i32) {
    // SAFETY: caller contract.
    unsafe { (*c).prev_frame(handles, mode) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_params_new() -> *mut Params {
    Box::into_raw(Box::default())
}

/// # Safety
/// `p` is valid; `name` has `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_params_as_id_finish(
    p: *mut Params,
    name: *const c_char,
    n: usize,
    id: i32,
    location: i32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { (*p).as_id_finish(units(name.cast::<u8>(), n), id, location) }
}

type LineSink = unsafe extern "C" fn(*mut c_void, i32, *const c_char, usize);

/// Calls `each(ctx, row, line, len)` for every numbered line of a NUL-terminated shader.
///
/// # Safety
/// `code` is NUL-terminated; `each` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_shader_code_lines(
    code: *const c_char,
    each: LineSink,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let bytes = unsafe { std::ffi::CStr::from_ptr(code) }.to_bytes();
    glsl::code_lines(bytes, |row, line| {
        // SAFETY: `each` accepts `ctx` and the line slice.
        unsafe { each(ctx, row, line.as_ptr().cast(), line.len()) }
    });
}

/// # Safety
/// `name` has `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_shader_dump_name(
    count: i32,
    name: *const c_char,
    n: usize,
    vertex: bool,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        let out = glsl::dump_name(count, units(name.cast::<u8>(), n), vertex);
        emit(sink, ctx, &out);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_fbo_entry_accepts(entry: i32, tex_type: i32) -> bool {
    fbo::entry_accepts(entry, tex_type)
}
