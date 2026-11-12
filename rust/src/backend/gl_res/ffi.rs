//! C ABI of the GL resource shells, declared in `HPL2/core/include/impl/RustGlRes.h`.
//!
//! Handles are arena ids from `ohpl_gl*_new`; 0 is never valid and every call on a stale or zero
//! handle does nothing. Pointer arguments are valid for the call; nothing here calls into C++.

use core::ffi::{c_char, c_void};

use super::fbo::{DSBS, Dsb, FBS, Fb};
use super::prog::{self, PROGRAMS, Prog, SHADERS, Shader};
use super::tex::{Face, Io, Res, TEXS, Tex};
use super::vb::{self, Data, VBS, Vb};
use crate::gl::fbo::Attach;
use crate::types::{Vector2l, Vector3l};

fn name_bytes<'a>(p: *const c_char, n: usize) -> &'a [u8] {
    if p.is_null() {
        return &[];
    }
    // SAFETY: caller passes `n` readable bytes.
    unsafe { core::slice::from_raw_parts(p.cast(), n) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gltex_new() -> u64 {
    TEXS.insert(Tex::default())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gltex_free(h: u64) {
    drop(TEXS.remove(h));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gltex_generate_handles(h: u64, n: i32) {
    TEXS.with(h, |t| t.generate_handles(n));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gltex_handle_count(h: u64) -> usize {
    TEXS.with(h, |t| t.handle_count()).unwrap_or(0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gltex_contains_data(h: u64) -> bool {
    TEXS.with(h, |t| t.contains_data()).unwrap_or(false)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gltex_handle(h: u64, count: f32) -> u32 {
    TEXS.with(h, |t| t.current(count)).unwrap_or(0)
}

/// `CreateTexture` for `n` faces (1 for a plain texture) into the handle at `idx`.
///
/// # Safety
/// `io`, `faces` (`n` entries with their level arrays) and `res` are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gltex_create_faces(
    h: u64,
    io: *mut Io,
    idx: i32,
    faces: *const Face,
    n: i32,
    res: *mut Res,
) -> bool {
    TEXS.with(h, |t| {
        let gl_handle = usize::try_from(idx)
            .ok()
            .and_then(|i| t.handle(i))
            .unwrap_or(0);
        for i in 0..n {
            // SAFETY: caller contract.
            let ok = unsafe {
                t.create_texture(
                    &mut *io,
                    gl_handle,
                    &*faces.add(i as usize),
                    i,
                    true,
                    &mut *res,
                )
            };
            if !ok {
                return false;
            }
        }
        true
    })
    .unwrap_or(false)
}

/// # Safety
/// `io` is valid; `data` holds the texture bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gltex_create_raw(
    h: u64,
    io: *mut Io,
    size: Vector3l,
    format: i32,
    data: *const c_void,
) -> bool {
    // SAFETY: caller contract.
    TEXS.with(h, |t| t.create_raw(unsafe { &mut *io }, size, format, data))
        .unwrap_or(false)
}

/// # Safety
/// `io` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gltex_create_raw_end(_h: u64, io: *const Io) {
    // SAFETY: caller contract.
    let target = crate::gl::tables::texture_target(unsafe { (*io).tex_type });
    gl!(glDisable(target));
}

/// # Safety
/// `io` is valid; `data` holds the bytes of the region.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gltex_set_raw_data(
    h: u64,
    io: *const Io,
    level: i32,
    offset: Vector3l,
    size: Vector3l,
    format: i32,
    data: *const c_void,
) {
    // SAFETY: caller contract.
    TEXS.with(h, |t| unsafe {
        t.set_raw_data(&*io, level, offset, size, format, data)
    });
}

/// # Safety
/// `io` is valid; `out` holds `size.x * size.y * 4` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gltex_read_pixels(
    h: u64,
    io: *const Io,
    count: f32,
    depth: bool,
    out: *mut f32,
) -> bool {
    // SAFETY: caller contract.
    TEXS.with(h, |t| unsafe { t.read_pixels(&*io, count, depth, out) })
        .unwrap_or(false)
}

/// # Safety
/// `io` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gltex_set_filter(h: u64, io: *mut Io, filter: i32) {
    // SAFETY: caller contract.
    TEXS.with(h, |t| t.set_filter(unsafe { &mut *io }, filter));
}

/// # Safety
/// `io` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gltex_apply_aniso(h: u64, io: *const Io, x: f32) {
    // SAFETY: caller contract.
    TEXS.with(h, |t| t.apply_aniso(unsafe { &*io }, x));
}

/// # Safety
/// `io` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gltex_set_wrap(h: u64, io: *mut Io, mask: u32, mode: i32) {
    // SAFETY: caller contract.
    TEXS.with(h, |t| t.set_wrap(unsafe { &mut *io }, mask, mode));
}

/// # Safety
/// `io` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gltex_set_compare(h: u64, io: *mut Io, func: bool, value: i32) {
    // SAFETY: caller contract.
    TEXS.with(h, |t| t.set_compare(unsafe { &mut *io }, func, value));
}

/// # Safety
/// `io` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gltex_set_red_from_alpha(h: u64, io: *const Io, on: bool) {
    // SAFETY: caller contract.
    TEXS.with(h, |t| t.set_red_from_alpha(unsafe { &*io }, on));
}

/// # Safety
/// `io` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gltex_auto_mipmaps(h: u64, io: *const Io) {
    // SAFETY: caller contract.
    TEXS.with(h, |t| t.auto_generate_mipmaps(unsafe { &*io }));
}

/// 0 when the texture has no data, 1 for a normal texture, 2 for a rectangle.
///
/// # Safety
/// `io` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gltex_props_begin(h: u64, io: *mut Io, idx: i32) -> i32 {
    TEXS.with(h, |t| {
        let gl_handle = usize::try_from(idx)
            .ok()
            .and_then(|i| t.handle(i))
            .unwrap_or(0);
        // SAFETY: caller contract.
        match t.props_begin(unsafe { &mut *io }, gl_handle) {
            None => 0,
            Some(false) => 1,
            Some(true) => 2,
        }
    })
    .unwrap_or(0)
}

/// # Safety
/// `io` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gltex_props_aniso(h: u64, io: *const Io) {
    // SAFETY: caller contract.
    TEXS.with(h, |t| t.props_aniso(unsafe { &*io }));
}

/// # Safety
/// `io` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gltex_props_end(h: u64, io: *const Io) {
    // SAFETY: caller contract.
    TEXS.with(h, |t| t.props_end(unsafe { &*io }));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glvb_new(vbo: bool) -> u64 {
    VBS.insert(Vb::new(vbo))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glvb_free(h: u64) {
    drop(VBS.remove(h));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glvb_add_element(h: u64, kind: i32, format: i32, num: i32, flag: u32) {
    VBS.with(h, |v| v.add_element(kind, format, num, flag));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glvb_needs_compile(h: u64, index_count: usize) -> bool {
    VBS.with(h, |v| v.needs_compile(index_count))
        .unwrap_or(false)
}

/// # Safety
/// `data` holds one entry per element, `indices` `index_count` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glvb_compile(
    h: u64,
    usage: i32,
    data: *const Data,
    indices: *const u32,
    index_count: usize,
) {
    // SAFETY: caller contract.
    VBS.with(h, |v| unsafe {
        v.compile(usage, data, indices, index_count)
    });
}

/// # Safety
/// As for `ohpl_glvb_compile`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glvb_update(
    h: u64,
    usage: i32,
    types: u32,
    with_indices: bool,
    data: *const Data,
    indices: *const u32,
    index_count: usize,
) {
    // SAFETY: caller contract.
    VBS.with(h, |v| unsafe {
        v.update(usage, types, with_indices, data, indices, index_count);
    });
}

/// # Safety
/// `ptrs` is null or holds one array pointer per element.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glvb_bind(h: u64, ptrs: *const *const c_void) {
    // SAFETY: caller contract.
    VBS.with(h, |v| unsafe { v.bind(ptrs) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glvb_unbind(h: u64) {
    VBS.with(h, |v| v.unbind());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glvb_draw(
    h: u64,
    own: i32,
    requested: i32,
    element_num: i32,
    index_count: i32,
    indices: *const u32,
) {
    VBS.with(h, |v| {
        v.draw(own, requested, element_num, index_count, indices)
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glvb_draw_indices(
    own: i32,
    requested: i32,
    indices: *const u32,
    count: i32,
) {
    vb::draw_indices(own, requested, indices, count);
}

/// # Safety
/// `id` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glfb_new(id: *mut u32) -> u64 {
    let fb = Fb::new();
    // SAFETY: caller contract.
    unsafe { id.write(fb.id) };
    FBS.insert(fb)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glfb_free(h: u64) {
    drop(FBS.remove(h));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glfb_detach_texture(h: u64, attachment: i32, index: i32) {
    FBS.with(h, |f| f.detach_texture(attachment, index));
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glfb_attach_texture(
    h: u64,
    attachment: i32,
    index: i32,
    tex_type: i32,
    mip: i32,
    extra: i32,
    tex_size: Vector2l,
    tex_handle: u32,
    out: *mut Attach,
) {
    if let Some(at) = FBS.with(h, |f| {
        f.attach_texture(
            attachment, index, tex_type, mip, extra, tex_size, tex_handle,
        )
    }) {
        // SAFETY: caller contract.
        unsafe { out.write(at) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glfb_set_depth_stencil(
    h: u64,
    has_buffer: bool,
    renderbuffer: u32,
    depth_bits: i32,
    stencil_bits: i32,
) {
    FBS.with(h, |f| {
        f.set_depth_stencil(has_buffer.then_some((renderbuffer, depth_bits, stencil_bits)));
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glfb_compile(h: u64, occupied: u32) -> u32 {
    FBS.with(h, |f| f.compile(occupied)).unwrap_or(u32::MAX)
}

#[repr(C)]
pub struct DsbOut {
    pub id: u32,
    pub kind: u32,
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gldsb_new(
    size: Vector2l,
    depth_bits: i32,
    stencil_bits: i32,
    out: *mut DsbOut,
) -> u64 {
    let (d, kind) = Dsb::new(size, depth_bits, stencil_bits);
    // SAFETY: caller contract.
    unsafe { out.write(DsbOut { id: d.id, kind }) };
    DSBS.insert(d)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gldsb_free(h: u64) {
    drop(DSBS.remove(h));
}

/// # Safety
/// `id` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glsh_new(shader_type: i32, id: *mut u32) -> u64 {
    let s = Shader::new(shader_type);
    // SAFETY: `id` is writable.
    unsafe { id.write(s.id) };
    SHADERS.insert(s)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glsh_free(h: u64) {
    drop(SHADERS.remove(h));
}

/// # Safety
/// `source` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glsh_compile(h: u64, source: *const c_char) -> bool {
    SHADERS.with(h, |s| s.compile(source)).unwrap_or(false)
}

/// The failure log kept by the last compile (valid until the next call on `h`).
///
/// # Safety
/// `len` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glsh_log(h: u64, len: *mut usize) -> *const u8 {
    let r = SHADERS.with(h, |s| (s.log().as_ptr(), s.log().len()));
    let (p, n) = r.unwrap_or((core::ptr::null(), 0));
    // SAFETY: caller contract.
    unsafe { len.write(n) };
    p
}

/// `LogShaderInfoLog`: fetches the log from GL; null when it is empty.
///
/// # Safety
/// `len` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glsh_dump_log(h: u64, len: *mut usize) -> *const u8 {
    let r = SHADERS.with(h, |s| s.dump_log().map(|l| (l.as_ptr(), l.len())));
    let (p, n) = r.flatten().unwrap_or((core::ptr::null(), 0));
    // SAFETY: caller contract.
    unsafe { len.write(n) };
    p
}

/// # Safety
/// `id` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glprog_new(id: *mut u32) -> u64 {
    let p = Prog::new();
    // SAFETY: `id` is writable.
    unsafe { id.write(p.id) };
    PROGRAMS.insert(p)
}

/// Detaches the attached shaders (-1 for none) and frees the program.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glprog_free(h: u64, shader0: i64, shader1: i64) {
    PROGRAMS.with(h, |p| {
        for s in [shader0, shader1] {
            if let Ok(s) = u32::try_from(s) {
                p.detach(s);
            }
        }
    });
    drop(PROGRAMS.remove(h));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glprog_attach(h: u64, shader: u32) {
    PROGRAMS.with(h, |p| p.attach(shader));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glprog_link(h: u64) -> bool {
    PROGRAMS.with(h, |p| p.link()).unwrap_or(false)
}

/// # Safety
/// `len` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glprog_log(h: u64, len: *mut usize) -> *const u8 {
    let r = PROGRAMS.with(h, |p| (p.log().as_ptr(), p.log().len()));
    let (p, n) = r.unwrap_or((core::ptr::null(), 0));
    // SAFETY: caller contract.
    unsafe { len.write(n) };
    p
}

/// # Safety
/// `len` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glprog_dump_log(h: u64, len: *mut usize) -> *const u8 {
    let r = PROGRAMS.with(h, |p| p.dump_log().map(|l| (l.as_ptr(), l.len())));
    let (p, n) = r.flatten().unwrap_or((core::ptr::null(), 0));
    // SAFETY: caller contract.
    unsafe { len.write(n) };
    p
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glprog_use(h: u64) {
    PROGRAMS.with(h, |p| p.use_program());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glprog_use_current(h: u64) {
    PROGRAMS.with(h, |p| p.restore_current());
}

/// # Safety
/// `name` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glprog_link_sampler(h: u64, name: *const c_char, unit: i32) {
    PROGRAMS.with(h, |p| p.link_sampler(name, unit));
}

/// # Safety
/// `name` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glprog_set_sampler(h: u64, name: *const c_char, unit: i32) -> bool {
    PROGRAMS
        .with(h, |p| p.set_sampler(name, unit))
        .unwrap_or(false)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glprog_bind(h: u64) {
    if let Some(id) = PROGRAMS.with(h, |p| p.id) {
        prog::bind(id);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glprog_unbind() {
    prog::unbind();
}

/// # Safety
/// `name` holds `n` bytes and is NUL-terminated at `n`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glprog_variable_id(h: u64, name: *const c_char, n: usize) -> i32 {
    PROGRAMS
        .with(h, |p| p.variable_id(name_bytes(name, n), name))
        .unwrap_or(-1)
}

/// # Safety
/// As for `ohpl_glprog_variable_id`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glprog_variable_as_id(
    h: u64,
    name: *const c_char,
    n: usize,
    id: i32,
) -> bool {
    PROGRAMS
        .with(h, |p| p.variable_as_id(name_bytes(name, n), name, id))
        .unwrap_or(false)
}

macro_rules! uniform_fns {
    ($($fname:ident($($a:ident: $t:ty),*) => |$loc:ident| $call:expr;)*) => {$(
        #[unsafe(no_mangle)]
        pub extern "C" fn $fname(h: u64, id: i32 $(, $a: $t)*) -> bool {
            PROGRAMS
                .with(h, |p| p.set_with(id, |$loc| $call))
                .unwrap_or(false)
        }
    )*};
}

uniform_fns! {
    ohpl_glprog_set_int(x: i32) => |l| gl!(glUniform1i(l, x));
    ohpl_glprog_set_float(x: f32) => |l| gl!(glUniform1f(l, x));
    ohpl_glprog_set_vec2(x: f32, y: f32) => |l| gl!(glUniform2f(l, x, y));
    ohpl_glprog_set_vec3(x: f32, y: f32, z: f32) => |l| gl!(glUniform3f(l, x, y, z));
    ohpl_glprog_set_vec4(x: f32, y: f32, z: f32, w: f32) => |l| gl!(glUniform4f(l, x, y, z, w));
}

/// # Safety
/// `values` holds `count` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glprog_set_float_array(
    h: u64,
    id: i32,
    values: *const f32,
    count: i32,
) -> bool {
    PROGRAMS
        .with(h, |p| {
            p.set_with(id, |l| gl!(glUniform1fv(l, count, values)))
        })
        .unwrap_or(false)
}

/// # Safety
/// `m` holds 16 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glprog_set_matrix(h: u64, id: i32, m: *const f32) -> bool {
    PROGRAMS
        .with(h, |p| {
            p.set_with(id, |l| gl!(glUniformMatrix4fv(l, 1, 1, m)))
        })
        .unwrap_or(false)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glprog_uniform_count(h: u64) -> i32 {
    PROGRAMS.with(h, |p| p.uniform_count()).unwrap_or(0)
}

/// # Safety
/// `name` holds 128 bytes, `vals` 16 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glprog_uniform_at(
    h: u64,
    i: u32,
    name: *mut [u8; 128],
    vals: *mut [f32; 16],
) -> i32 {
    // SAFETY: caller contract.
    PROGRAMS
        .with(h, |p| {
            p.uniform_at(i, unsafe { &mut *name }, unsafe { &mut *vals })
        })
        .unwrap_or(0)
}

/// For the shell's bookkeeping of GL names.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glprog_current() -> i32 {
    prog::current()
}

const _: () = {
    assert!(size_of::<Io>() == 84);
    assert!(size_of::<Face>() == 32);
    assert!(size_of::<Data>() == 16);
    assert!(size_of::<Res>() == 4);
};
