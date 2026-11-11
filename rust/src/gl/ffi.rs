//! C ABI for the GL backend, declared in `HPL2/core/include/impl/RustGl.h`.
//!
//! Pointers are non-null and valid for the call unless a function says it accepts null;
//! outputs do not alias inputs.

use std::ffi::{c_char, c_void};

use super::fbo::{self, Attach, DrawPlan, DsPlan};
use super::glsl::{self, AsId, Params};
use super::state::{self, GlState, TexPlan, Viewport};
use super::tables;
use super::texture::{self, Clock, CreateIn, CreatePlan, MipPlan, Props, Upload};
use super::vbuf::{self, Elem, MAX_OPS, Op};
use crate::string_ffi::{Sink, emit, units};
use crate::types::{Matrixf, Vector2l, Vector3f, Vector3l};

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

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_srgb(f: u32) -> u32 {
    tables::srgb(f)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_texture_binding(target: u32) -> u32 {
    tables::texture_binding(target)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_uniform_components(t: u32) -> i32 {
    tables::uniform_components(t)
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

/// # Safety
/// `s` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_state_new(s: *mut GlState) {
    // SAFETY: caller contract.
    unsafe { s.write(GlState::new()) }
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
/// `s` points to an initialised state, `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_state_set_texture(
    s: *mut GlState,
    unit: u32,
    new_target: u32,
    has_texture: bool,
    multitexture: bool,
    out: *mut TexPlan,
) {
    // SAFETY: caller contract.
    unsafe {
        out.write((*s).set_texture(unit, new_target, has_texture, multitexture));
    }
}

/// # Safety
/// `targets` has `n` items, `out` has `cap` writable items.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_draw_targets(
    targets: *const i32,
    n: usize,
    out: *mut u32,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    unsafe { state::draw_targets(units(targets, n), units_mut(out, cap)) }
}

/// # Safety
/// `out` has four writable ints.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_scissor_rect(
    target_h: i32,
    pos: Vector2l,
    size: Vector2l,
    out: *mut [i32; 4],
) {
    // SAFETY: caller contract.
    unsafe { out.write(state::scissor_rect(target_h, pos, size)) }
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_viewport(
    target: Vector2l,
    pos: Vector2l,
    size: Vector2l,
    out: *mut Viewport,
) {
    // SAFETY: caller contract.
    unsafe { out.write(state::viewport(target, pos, size)) }
}

/// # Safety
/// `out` has six writable ints.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_copy_to_texture(
    fb_pos: Vector2l,
    fb_size: Vector2l,
    fb_total: Vector2l,
    pos: Vector2l,
    size: Vector2l,
    tex_offset: Vector2l,
    tex_height: i32,
    out: *mut [i32; 6],
) {
    // SAFETY: caller contract.
    unsafe {
        out.write(state::copy_to_texture(
            fb_pos, fb_size, fb_total, pos, size, tex_offset, tex_height,
        ));
    }
}

/// # Safety
/// `out` has four writable ints.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_read_rect(
    fb_pos: Vector2l,
    fb_size: Vector2l,
    fb_total: Vector2l,
    pos: Vector2l,
    size: Vector2l,
    out: *mut [i32; 4],
) {
    // SAFETY: caller contract.
    unsafe { out.write(state::read_rect(fb_pos, fb_size, fb_total, pos, size)) }
}

/// # Safety
/// `elems` has `n` items; `out` has `MAX_OPS` writable ops.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_vb_bind_vbo(elems: *const Elem, n: usize, out: *mut Op) -> usize {
    // SAFETY: caller contract.
    unsafe { vbuf::bind_vbo(units(elems, n), units_mut(out, MAX_OPS)) }
}

/// # Safety
/// As `ohpl_gl_vb_bind_vbo`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_vb_bind_array(
    elems: *const Elem,
    n: usize,
    out: *mut Op,
) -> usize {
    // SAFETY: caller contract.
    unsafe { vbuf::bind_array(units(elems, n), units_mut(out, MAX_OPS)) }
}

/// # Safety
/// As `ohpl_gl_vb_bind_vbo`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_vb_unbind(elems: *const Elem, n: usize, out: *mut Op) -> usize {
    // SAFETY: caller contract.
    unsafe { vbuf::unbind(units(elems, n), units_mut(out, MAX_OPS)) }
}

/// # Safety
/// `out` has `MAX_OPS` writable ops.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_batch_states(flags: u32, out: *mut Op) -> usize {
    // SAFETY: caller contract.
    unsafe { vbuf::batch_states(flags, units_mut(out, MAX_OPS)) }
}

/// # Safety
/// `out` has `MAX_OPS` writable ops.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_batch_arrays(stride: i32, out: *mut Op) -> usize {
    // SAFETY: caller contract.
    unsafe { vbuf::batch_arrays(stride, units_mut(out, MAX_OPS)) }
}

/// # Safety
/// `out` has `MAX_OPS` writable ops.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_batch_texture_unit(
    unit: u32,
    active: bool,
    stride: i32,
    out: *mut Op,
) -> usize {
    // SAFETY: caller contract.
    unsafe { vbuf::batch_texture_unit(unit, active, stride, units_mut(out, MAX_OPS)) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_vb_draw_mode(own: i32, requested: i32) -> u32 {
    vbuf::draw_mode(own, requested)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_vb_draw_count(element_num: i32, index_num: i32) -> i32 {
    vbuf::draw_count(element_num, index_num)
}

/// # Safety
/// `out` has four writable floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_vb_vec3_components(
    element_num: i32,
    x: f32,
    y: f32,
    z: f32,
    out: *mut [f32; 4],
) -> usize {
    // SAFETY: caller contract.
    unsafe { vbuf::vec3_components(element_num, Vector3f { x, y, z }, &mut *out) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_vb_color_components(format: i32) -> usize {
    vbuf::color_components(format)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_vb_copy_flags(requested: u32, present: u32) -> u32 {
    vbuf::copy_flags(requested, present)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_vb_vertex_num(size: i32, element_num: i32, shadow: bool) -> i32 {
    vbuf::vertex_num(size, element_num, shadow)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_vb_bounding_check(flags: u32, format: i32) -> i32 {
    vbuf::bounding_check(flags, format)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_vb_shadow_double_extra(size: usize) -> usize {
    vbuf::shadow_double_extra(size)
}

/// # Safety
/// `data` has `total` writable 32-bit words (4-byte aligned); `size` is the original length.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_vb_shadow_double_fill(
    format: i32,
    data: *mut u32,
    total: usize,
    size: usize,
) {
    if format == vbuf::FORMAT_BYTE {
        return;
    }
    // SAFETY: caller contract.
    unsafe { vbuf::shadow_double_fill(format, units_mut(data, total), size) }
}

/// # Safety
/// `m` is a 4x4 matrix; each array pointer is null or has its length in floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_vb_transform(
    m: *const Matrixf,
    pos: *mut f32,
    pos_len: usize,
    normal: *mut f32,
    normal_len: usize,
    tangent: *mut f32,
    tangent_len: usize,
    vtx_num: i32,
    stride: i32,
    shadow: bool,
) -> u32 {
    // SAFETY: caller contract.
    unsafe {
        let opt = |p: *mut f32, n| (!p.is_null()).then(|| units_mut(p, n));
        vbuf::transform(
            &*m,
            units_mut(pos, pos_len),
            opt(normal, normal_len),
            opt(tangent, tangent_len),
            vtx_num,
            stride,
            shadow,
        )
    }
}

/// # Safety
/// `i` and `out` are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_tex_plan_create(i: *const CreateIn, out: *mut CreatePlan) {
    // SAFETY: caller contract.
    unsafe { out.write(texture::plan_create(&*i)) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_tex_level_size(size: Vector3l, count: i32) -> Vector3l {
    texture::level_size(size, count)
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_tex_plan_upload(
    tex_type: i32,
    srgb: bool,
    format: i32,
    level: i32,
    data_size: i32,
    size: Vector3l,
    face: i32,
    out: *mut Upload,
) {
    // SAFETY: caller contract.
    unsafe {
        out.write(texture::plan_upload(
            tex_type, srgb, format, level, data_size, size, face,
        ));
    }
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_tex_plan_mips(
    tex_type: i32,
    usage: i32,
    fbo_ext: bool,
    srgb: bool,
    format: i32,
    size: Vector3l,
    face: i32,
    out: *mut MipPlan,
) {
    // SAFETY: caller contract.
    unsafe {
        out.write(texture::plan_mips(
            tex_type, usage, fbo_ext, srgb, format, size, face,
        ));
    }
}

/// # Safety
/// `wrap` has three ints; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_tex_plan_props(
    tex_type: i32,
    use_mips: bool,
    filter: i32,
    wrap: *const [i32; 3],
    out: *mut Props,
) {
    // SAFETY: caller contract.
    unsafe {
        out.write(texture::plan_props(tex_type, use_mips, filter, *wrap));
    }
}

/// # Safety
/// `out` has two writable words.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_tex_filter_params(
    filter: i32,
    use_mips: bool,
    tex_type: i32,
    out: *mut [u32; 2],
) {
    // SAFETY: caller contract.
    unsafe { out.write(texture::filter_params(filter, use_mips, tex_type)) }
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

/// Handle index, or -1 for handle 0.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_tex_handle_slot(has_data: bool, handles: usize, count: f32) -> i64 {
    texture::handle_slot(has_data, handles, count).map_or(-1, |i| i as i64)
}

/// # Safety
/// `bytes` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_tex_raw_size(
    size: Vector3l,
    format: i32,
    bytes: *mut i32,
) -> Vector3l {
    let (s, b) = texture::raw_size(size, format);
    // SAFETY: caller contract.
    unsafe { bytes.write(b) };
    s
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_params_new() -> *mut Params {
    Box::into_raw(Box::default())
}

/// # Safety
/// `p` came from `ohpl_gl_params_new` and is not used again.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_params_free(p: *mut Params) {
    if !p.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(p) });
    }
}

/// # Safety
/// `p` is valid; `name` has `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_params_find(
    p: *const Params,
    name: *const c_char,
    n: usize,
) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        (*p).find(units(name.cast::<u8>(), n))
            .map_or(-1, |i| i32::try_from(i).unwrap_or(i32::MAX))
    }
}

/// # Safety
/// `p` is valid; `name` has `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_params_add(
    p: *mut Params,
    name: *const c_char,
    n: usize,
    location: i32,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { (*p).add(units(name.cast::<u8>(), n), location) }
}

/// Writes the location of `id`; false when `id` is outside the cache.
///
/// # Safety
/// `p` and `out` are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_params_location(p: *const Params, id: i32, out: *mut i32) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match (*p).location(id) {
            Some(l) => {
                out.write(l);
                true
            }
            None => false,
        }
    }
}

/// 0 false, 1 true, 2 look the uniform up and call `ohpl_gl_params_as_id_finish`.
///
/// # Safety
/// `p` is valid; `name` has `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_params_as_id_begin(
    p: *const Params,
    name: *const c_char,
    n: usize,
    id: i32,
) -> i32 {
    // SAFETY: caller contract.
    match unsafe { (*p).as_id_begin(units(name.cast::<u8>(), n), id) } {
        AsId::Done(false) => 0,
        AsId::Done(true) => 1,
        AsId::Lookup => 2,
    }
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

/// # Safety
/// `current` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_prog_bind(current: *mut i32, handle: u32) -> bool {
    // SAFETY: caller contract.
    unsafe { glsl::bind(&mut *current, handle) }
}

/// # Safety
/// `current` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_prog_unbind(current: *mut i32) -> bool {
    // SAFETY: caller contract.
    unsafe { glsl::unbind(&mut *current) }
}

/// # Safety
/// `current` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_prog_forget(current: *mut i32, handle: u32) {
    // SAFETY: caller contract.
    unsafe { glsl::forget(&mut *current, handle) }
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

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_fbo_plan_depth_stencil(
    depth_bits: i32,
    stencil_bits: i32,
    packed_ext: bool,
    out: *mut DsPlan,
) {
    // SAFETY: caller contract.
    unsafe {
        out.write(fbo::plan_depth_stencil(
            depth_bits,
            stencil_bits,
            packed_ext,
        ))
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_fbo_entry_accepts(entry: i32, tex_type: i32) -> bool {
    fbo::entry_accepts(entry, tex_type)
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_fbo_plan_attach(
    attachment: u32,
    index: i32,
    tex_type: i32,
    mip: i32,
    extra: i32,
    tex_size: Vector2l,
    out: *mut Attach,
) {
    // SAFETY: caller contract.
    unsafe {
        out.write(fbo::plan_attach(
            attachment, index, tex_type, mip, extra, tex_size,
        ));
    }
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gl_fbo_plan_draw_buffers(occupied: u32, out: *mut DrawPlan) {
    // SAFETY: caller contract.
    unsafe { out.write(fbo::plan_draw_buffers(occupied)) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gl_fbo_status_class(status: u32) -> u32 {
    fbo::status_class(status)
}
