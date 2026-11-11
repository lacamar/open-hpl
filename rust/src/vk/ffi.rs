//! C ABI for the Vulkan backend, declared in `HPL2/core/include/impl/RustVk.h`.
//!
//! Pointers are non-null and valid for the stated lengths unless a function says it accepts null;
//! outputs do not alias inputs.

use std::ffi::{c_char, c_void};

use super::glsl_rewrite;
use super::tables::{self, Format, MipIn, MipPlan, Ring, SamplerKey, Uniform, Viewport};
use crate::string_ffi::units;

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<Format>() == 28 && size_of::<MipIn>() == 40 && size_of::<MipPlan>() == 36);
    assert!(size_of::<Ring>() == 32 && size_of::<Viewport>() == 32 && size_of::<Uniform>() == 24);
    assert!(size_of::<SamplerKey>() == 44);
};

macro_rules! enum_tables {
    ($($name:ident => $f:path;)*) => {$(
        #[unsafe(no_mangle)]
        pub extern "C" fn $name(v: i32) -> u32 { $f(v) }
    )*};
}

enum_tables! {
    ohpl_vk_stencil_op => tables::stencil_op;
    ohpl_vk_blend_factor => tables::blend_factor;
    ohpl_vk_wrap => tables::wrap;
}

/// # Safety
/// `p` points to `n` writable units, or `n` is 0.
unsafe fn mut_units<'a, T>(p: *mut T, n: usize) -> &'a mut [T] {
    if n == 0 {
        &mut []
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts_mut(p, n) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_prim(draw_type: i32) -> i32 {
    tables::prim(draw_type)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_format(pixel_format: i32, srgb: bool, depth24: u32, out: &mut Format) {
    *out = tables::vk_format(pixel_format, srgb, depth24);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_unorm_of_srgb(f: u32) -> u32 {
    tables::unorm_of_srgb(f)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_format_size(f: u32) -> usize {
    tables::format_size(f)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_float_to_half(f: f32) -> u16 {
    tables::float_to_half(f)
}

/// Source and destination byte counts of a pixel conversion; false where C++ `resize` threw.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_convert_len(
    f: &Format,
    pixels: usize,
    src_len: &mut usize,
    dst_len: &mut usize,
) -> bool {
    match (
        tables::convert_src_len(f, pixels),
        tables::convert_dst_len(f, pixels),
    ) {
        (Some(s), Some(d)) => {
            *src_len = s;
            *dst_len = d;
            true
        }
        _ => false,
    }
}

/// # Safety
/// `src` and `dst` are valid for `src_len` and `dst_len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vk_convert_pixels(
    f: &Format,
    src: *const u8,
    src_len: usize,
    pixels: usize,
    dst: *mut u8,
    dst_len: usize,
) {
    // SAFETY: caller contract.
    let (src, dst) = unsafe { (units(src, src_len), mut_units(dst, dst_len)) };
    tables::convert_pixels(f, src, pixels, dst);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_full_mip_count(x: i32, y: i32, z: i32) -> i32 {
    tables::full_mip_count(x, y, z)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_halve_size(x: i32, y: i32, out: &mut [i32; 2]) {
    let (nx, ny) = tables::halve_size(x, y);
    *out = [nx, ny];
}

/// # Safety
/// `src` is valid for `src_len` bytes and `dst` for `dst_len`, the new size times `bpp`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vk_halve_image(
    src: *const u8,
    src_len: usize,
    x: i32,
    y: i32,
    bpp: usize,
    dst: *mut u8,
    dst_len: usize,
) {
    // SAFETY: caller contract.
    let (src, dst) = unsafe { (units(src, src_len), mut_units(dst, dst_len)) };
    tables::halve_image(src, (x, y), bpp, dst);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_mip_plan(i: &MipIn, out: &mut MipPlan) {
    *out = tables::mip_plan(i);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_converted_len(draw_type: i32, count: i32) -> usize {
    tables::converted_len(draw_type, count)
}

/// # Safety
/// `idx` is valid for `n` indices and `out` for `cap`, which is `ohpl_vk_converted_len`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vk_convert_indices(
    draw_type: i32,
    idx: *const u32,
    n: usize,
    out: *mut u32,
    cap: usize,
) -> bool {
    // SAFETY: caller contract.
    let (idx, out) = unsafe { (units(idx, n), mut_units(out, cap)) };
    tables::convert_indices(draw_type, idx, out)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_sampler_key(k: &SamplerKey) -> u64 {
    tables::sampler_key(k)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_ring_alloc(
    used: usize,
    size: usize,
    alloc: usize,
    align: usize,
    out: &mut Ring,
) {
    *out = tables::ring_alloc(used, size, alloc, align);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_viewport(
    total: &[i32; 2],
    pos: &[i32; 2],
    size: &[i32; 2],
    scissor: bool,
    scissor_pos: &[i32; 2],
    scissor_size: &[i32; 2],
    out: &mut Viewport,
) {
    *out = tables::viewport(
        *total,
        *pos,
        *size,
        scissor.then_some((*scissor_pos, *scissor_size)),
    );
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_gl_ortho(
    l: f32,
    r: f32,
    b: f32,
    t: f32,
    n: f32,
    f: f32,
    out: &mut [f32; 16],
) {
    *out = tables::gl_ortho(l, r, b, t, n, f);
}

/// # Safety
/// `dst` is valid for `cols * 4` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vk_write_matrix(dst: *mut f32, m: &[f32; 16], cols: i32) {
    let n = usize::try_from(cols).unwrap_or(0).saturating_mul(4);
    // SAFETY: caller contract.
    tables::write_matrix(unsafe { mut_units(dst, n) }, m, cols);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vk_reflect_uniform(
    offset: i32,
    gl_type: i32,
    size: i32,
    array_stride: i32,
    out: &mut Uniform,
) {
    *out = tables::reflect_uniform(offset, gl_type, size, array_stride);
}

/// # Safety
/// `name` is valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vk_split_uniform_name(
    name: *const c_char,
    n: usize,
    element: &mut i32,
) -> usize {
    // SAFETY: caller contract.
    let (len, el) = tables::split_uniform_name(unsafe { units(name.cast::<u8>(), n) });
    *element = el;
    len
}

/// # Safety
/// `data` is valid for `data_len` bytes and `src` for `count * comps` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vk_write_uniform(
    data: *mut u8,
    data_len: usize,
    u: &Uniform,
    first: i32,
    comps: i32,
    count: i32,
    src: *const f32,
) -> bool {
    let n = usize::try_from(comps)
        .unwrap_or(0)
        .saturating_mul(usize::try_from(count).unwrap_or(0));
    // SAFETY: caller contract.
    let (data, src) = unsafe { (mut_units(data, data_len), units(src, n)) };
    tables::write_uniform(data, u, first, comps, count, src)
}

/// # Safety
/// `data` is valid for `data_len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vk_write_int(
    data: *mut u8,
    data_len: usize,
    u: &Uniform,
    element: i32,
    x: i32,
) -> bool {
    // SAFETY: caller contract.
    tables::write_int(unsafe { mut_units(data, data_len) }, u, element, x)
}

/// # Safety
/// `data` is valid for `data_len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vk_write_mat4(
    data: *mut u8,
    data_len: usize,
    u: &Uniform,
    element: i32,
    m: &[f32; 16],
) -> bool {
    // SAFETY: caller contract.
    tables::write_mat4(unsafe { mut_units(data, data_len) }, u, element, m)
}

/// # Safety
/// `p` is valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vk_unpack_pixel(
    format: u32,
    p: *const u8,
    n: usize,
    out: &mut [f32; 4],
) {
    // SAFETY: caller contract.
    *out = tables::unpack_pixel(format, unsafe { units(p, n) });
}

/// Receiver of one rewritten program; pointers are valid for the callback only.
#[repr(C)]
pub struct GlslSink {
    pub ctx: *mut c_void,
    pub error: unsafe extern "C" fn(*mut c_void, *const c_char, usize),
    pub program:
        unsafe extern "C" fn(*mut c_void, *const c_char, usize, *const c_char, usize, u32, i32),
    pub sampler: unsafe extern "C" fn(*mut c_void, *const c_char, usize, *const c_char, usize, i32),
    pub init: unsafe extern "C" fn(*mut c_void, *const c_char, usize, *const f32, usize),
}

/// # Safety
/// Both sources are valid for their lengths; `sink` holds valid callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vk_glsl_to_vulkan(
    vtx: *const c_char,
    vtx_len: usize,
    frag: *const c_char,
    frag_len: usize,
    sink: &GlslSink,
) -> bool {
    // SAFETY: caller contract.
    let (v, f) = unsafe {
        (
            units(vtx.cast::<u8>(), vtx_len),
            units(frag.cast::<u8>(), frag_len),
        )
    };
    match glsl_rewrite::glsl_to_vulkan(v, f) {
        Err(e) => {
            // SAFETY: callbacks valid per contract.
            unsafe { (sink.error)(sink.ctx, e.as_ptr().cast(), e.len()) };
            false
        }
        Ok(p) => {
            // SAFETY: callbacks valid per contract.
            unsafe {
                for s in &p.samplers {
                    (sink.sampler)(
                        sink.ctx,
                        s.name.as_ptr().cast(),
                        s.name.len(),
                        s.ty.as_ptr().cast(),
                        s.ty.len(),
                        s.binding,
                    );
                }
                for i in &p.inits {
                    (sink.init)(
                        sink.ctx,
                        i.name.as_ptr().cast(),
                        i.name.len(),
                        i.values.as_ptr(),
                        i.values.len(),
                    );
                }
                (sink.program)(
                    sink.ctx,
                    p.vertex.as_ptr().cast(),
                    p.vertex.len(),
                    p.fragment.as_ptr().cast(),
                    p.fragment.len(),
                    p.attrib_mask,
                    p.frag_outputs,
                );
            }
            true
        }
    }
}
