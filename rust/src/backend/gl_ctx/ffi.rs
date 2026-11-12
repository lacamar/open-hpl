//! C ABI of the GL context, declared in `HPL2/core/include/impl/RustGlCtx.h`.
//!
//! `c` is the pointer `ohpl_glctx_new` returned and is never null. Nothing here calls back into
//! C++ except the log hook registered with `ohpl_glctx_set_host_log`.

use core::ffi::{c_char, c_void};

use super::ctx::{self, Ctx, Sources, WindowCfg};
use super::oq::{self, Query};
use crate::types::Vector2l;

macro_rules! ctx_fns {
    ($($name:ident($($arg:ident: $t:ty),*) $(-> $ret:ty)? => |$c:ident| $body:expr;)*) => {$(
        /// # Safety
        /// `c` comes from `ohpl_glctx_new`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(c: *mut Ctx $(, $arg: $t)*) $(-> $ret)? {
            // SAFETY: caller contract; the context is only used from the render thread.
            let $c = unsafe { &mut *c };
            $body
        }
    )*};
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glctx_set_host_log(f: ctx::HostLog) {
    ctx::set_host_log(f);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_glctx_new() -> *mut Ctx {
    Box::into_raw(Box::new(Ctx::new()))
}

/// # Safety
/// `c` comes from `ohpl_glctx_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glctx_free(c: *mut Ctx) {
    if !c.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(c) });
    }
}

/// # Safety
/// `c` comes from `ohpl_glctx_new`; `cfg` is valid with a NUL-terminated caption.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glctx_create_window(c: *mut Ctx, cfg: *mut WindowCfg) -> bool {
    // SAFETY: caller contract.
    unsafe { (*c).create_window(&mut *cfg) }
}

/// Copies the `GLCAPS` line into `out` (NUL-terminated, truncated to `cap`); returns its full length.
///
/// # Safety
/// `c` comes from `ohpl_glctx_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glctx_caps_line(c: *mut Ctx, out: *mut c_char, cap: usize) -> usize {
    // SAFETY: caller contract.
    let line = unsafe { &*c }.caps.line();
    let bytes = line.as_bytes();
    let n = bytes.len().min(cap.saturating_sub(1));
    // SAFETY: `out` has room for `cap` bytes.
    unsafe {
        core::ptr::copy_nonoverlapping(bytes.as_ptr(), out.cast::<u8>(), n);
        out.add(n).write(0);
    }
    bytes.len()
}

ctx_fns! {
    ohpl_glctx_load() => |c| c.load();
    ohpl_glctx_debug_from_env() => |c| c.debug_from_env();
    ohpl_glctx_window() -> *mut c_void => |c| c.window();
    ohpl_glctx_show_cursor(on: bool) => |c| c.show_cursor(on);
    ohpl_glctx_window_grab(on: bool, headless: bool) => |c| c.window_grab(on, headless);
    ohpl_glctx_relative_mouse(on: bool, headless: bool) => |c| c.relative_mouse(on, headless);
    ohpl_glctx_set_caption(caption: *const c_char) => |c| {
        // SAFETY: caller contract: NUL-terminated.
        unsafe { c.set_caption(caption) }
    };
    ohpl_glctx_mouse_focus() -> bool => |c| c.mouse_focus();
    ohpl_glctx_input_focus() -> bool => |c| c.input_focus();
    ohpl_glctx_is_visible() -> bool => |c| c.is_visible();
    ohpl_glctx_set_vsync(on: bool, adaptive: bool) => |c| c.set_vsync(on, adaptive);
    ohpl_glctx_force_window_size(w: i32, h: i32) -> bool => |c| c.force_window_size(w, h);
    ohpl_glctx_check_screen(out: *mut Vector2l) -> bool => |c| match c.check_screen() {
        Some(v) => {
            // SAFETY: `out` is writable.
            unsafe { out.write(v) };
            true
        }
        None => false,
    };
    ohpl_glctx_setup_gl() => |c| c.setup_gl();
    ohpl_glctx_get_string(name: u32) -> *const u8 => |c| c.get_string(name);
    ohpl_glctx_get_caps(kind: i32, force_sm34_off: bool) -> i32 => |c| c.get_caps(kind, force_sm34_off);
    ohpl_glctx_set_multisampling_active(on: bool) => |c| c.set_multisampling_active(on);

    ohpl_glctx_clear(flags: u32) => |c| c.clear(flags);
    ohpl_glctx_set_clear_color(r: f32, g: f32, b: f32, a: f32) => |c| c.set_clear_color(r, g, b, a);
    ohpl_glctx_set_clear_depth(d: f32) => |c| c.set_clear_depth(d);
    ohpl_glctx_set_clear_stencil(v: i32) => |c| c.set_clear_stencil(v);
    ohpl_glctx_copy_to_texture(pos: Vector2l, size: Vector2l, tex_off: Vector2l, tex_type: i32, tex_height: i32)
        => |c| c.copy_to_texture(pos, size, tex_off, tex_type, tex_height);
    ohpl_glctx_read_rect(pos: Vector2l, size: Vector2l, out: *mut [i32; 4]) => |c| {
        // SAFETY: `out` has four writable ints.
        unsafe { out.write(c.read_rect(pos, size)) }
    };
    ohpl_glctx_read_pixels(rect: *const [i32; 4], format: i32, data: *mut c_void) => |c| {
        // SAFETY: `rect` is readable and `data` sized for the rectangle.
        unsafe { c.read_pixels(*rect, format, data) }
    };
    ohpl_glctx_bind_framebuffer(handle: u32) => |c| c.bind_framebuffer(handle);
    ohpl_glctx_set_viewport(target: Vector2l, pos: Vector2l, size: Vector2l) => |c| c.set_viewport(target, pos, size);
    ohpl_glctx_set_draw_targets(targets: *const i32, count: i32) => |c| {
        let n = usize::try_from(count).unwrap_or(0);
        let slice = if n == 0 || targets.is_null() {
            &[]
        } else {
            // SAFETY: `targets` has `count` items.
            unsafe { core::slice::from_raw_parts(targets, n) }
        };
        c.set_draw_targets(slice, count);
    };
    ohpl_glctx_finish() => |c| c.finish();
    ohpl_glctx_flush() => |c| c.flush();
    ohpl_glctx_swap(gamma: f32) => |c| c.swap(gamma);

    ohpl_glctx_set_color_write(r: bool, g: bool, b: bool, a: bool) => |c| c.set_color_write(r, g, b, a);
    ohpl_glctx_set_depth_write(on: bool) => |c| c.set_depth_write(on);
    ohpl_glctx_set_depth_test(on: bool) => |c| c.set_depth_test(on);
    ohpl_glctx_set_depth_func(f: i32) => |c| c.set_depth_func(f);
    ohpl_glctx_set_alpha_test(on: bool) => |c| c.set_alpha_test(on);
    ohpl_glctx_set_alpha_func(f: i32, reference: f32) => |c| c.set_alpha_func(f, reference);
    ohpl_glctx_set_stencil_active(on: bool) => |c| c.set_stencil_active(on);
    ohpl_glctx_set_stencil_write_mask(mask: u32) => |c| c.set_stencil_write_mask(mask);
    ohpl_glctx_set_stencil(func: i32, reference: i32, mask: u32, fail: i32, zfail: i32, zpass: i32)
        => |c| c.set_stencil(func, reference, mask, fail, zfail, zpass);
    ohpl_glctx_set_stencil_two_side(front_func: i32, back_func: i32, reference: i32, mask: u32, ops: *const [i32; 6])
        => |c| {
            // SAFETY: `ops` has six readable ints.
            c.set_stencil_two_side([front_func, back_func], reference, mask, unsafe { *ops })
        };
    ohpl_glctx_set_cull_active(on: bool) => |c| c.set_cull_active(on);
    ohpl_glctx_set_cull_mode(mode: i32) => |c| c.set_cull_mode(mode);
    ohpl_glctx_set_scissor_active(on: bool) => |c| c.set_scissor_active(on);
    ohpl_glctx_set_scissor_rect(target_h: i32, pos: Vector2l, size: Vector2l) => |c| c.set_scissor_rect(target_h, pos, size);
    ohpl_glctx_set_clip_plane(idx: i32, a: f32, b: f32, cc: f32, d: f32) => |c| c.set_clip_plane(idx, [a, b, cc, d]);
    ohpl_glctx_set_clip_plane_active(idx: i32, on: bool) => |c| c.set_clip_plane_active(idx, on);
    ohpl_glctx_set_blend_active(on: bool) => |c| c.set_blend_active(on);
    ohpl_glctx_set_blend_func(src: i32, dst: i32) => |c| c.set_blend_func(src, dst);
    ohpl_glctx_set_blend_func_separate(sc: i32, dc: i32, sa: i32, da: i32) => |c| c.set_blend_func_separate(sc, dc, sa, da);
    ohpl_glctx_set_depth_clamp_active(on: bool) => |c| c.set_depth_clamp_active(on);
    ohpl_glctx_set_polygon_offset_active(on: bool) => |c| c.set_polygon_offset_active(on);
    ohpl_glctx_set_polygon_offset(bias: f32, slope: f32) => |c| c.set_polygon_offset(bias, slope);

    ohpl_glctx_push_matrix(kind: i32) => |c| c.push_matrix(kind);
    ohpl_glctx_pop_matrix(kind: i32) => |c| c.pop_matrix(kind);
    ohpl_glctx_set_matrix(kind: i32, m: *const [f32; 16]) => |c| {
        // SAFETY: `m` has sixteen readable floats.
        c.set_matrix(kind, unsafe { &*m })
    };
    ohpl_glctx_set_identity_matrix(kind: i32) => |c| c.set_identity_matrix(kind);
    ohpl_glctx_set_ortho(l: f32, r: f32, b: f32, t: f32, n: f32, f: f32) => |c| c.set_ortho(l, r, b, t, n, f);

    ohpl_glctx_set_texture(unit: u32, tex_type: i32, has_texture: bool, handle: u32)
        => |c| c.set_texture(unit, tex_type, has_texture, handle);
    ohpl_glctx_set_active_texture_unit(unit: u32) => |c| c.set_active_texture_unit(unit);
    ohpl_glctx_set_texture_env(param: i32, val: i32) => |c| c.set_texture_env(param, val);
    ohpl_glctx_set_texture_constant_color(r: f32, g: f32, b: f32, a: f32) => |c| c.set_texture_constant_color([r, g, b, a]);
    ohpl_glctx_set_color(r: f32, g: f32, b: f32, a: f32) => |c| c.set_color(r, g, b, a);

    ohpl_glctx_begin(mode: u32) => |c| c.begin(mode);
    ohpl_glctx_end() => |c| c.end();
    ohpl_glctx_vertex3f(x: f32, y: f32, z: f32) => |c| c.vertex3f(x, y, z);
    ohpl_glctx_color4f(r: f32, g: f32, b: f32, a: f32) => |c| c.color4f(r, g, b, a);
    ohpl_glctx_texcoord2f(s: f32, t: f32) => |c| c.texcoord2f(s, t);
    ohpl_glctx_texcoord3f(s: f32, t: f32, r: f32) => |c| c.texcoord3f(s, t, r);
    ohpl_glctx_multitexcoord2f(unit: u32, s: f32, t: f32) => |c| c.multitexcoord2f(unit, s, t);
    ohpl_glctx_multitexcoord3f(unit: u32, s: f32, t: f32, r: f32) => |c| c.multitexcoord3f(unit, s, t, r);

    ohpl_glctx_batch_texture_unit(unit: u32, active: bool, stride_bytes: i32, vertex: *const f32, texcoords: *const *mut f32)
        => |c| {
            let src = Sources { element: core::ptr::null(), batch_vertex: vertex, batch_texcoord: texcoords };
            // SAFETY: caller contract: the batch arrays outlive the call.
            unsafe { c.batch_texture_unit(unit, active, stride_bytes, &src) }
        };
    ohpl_glctx_flush_batch(quads: bool, flags: u32, stride_bytes: i32, index_count: i32, indices: *const c_void,
        vertex: *const f32, texcoords: *const *mut f32)
        => |c| {
            let src = Sources { element: core::ptr::null(), batch_vertex: vertex, batch_texcoord: texcoords };
            // SAFETY: caller contract: the batch arrays outlive the call.
            unsafe { c.flush_batch(quads, flags, stride_bytes, index_count, indices, &src) }
        };
}

/// Occlusion query: `q` is the `OhplGlOq` the C++ shell holds.
///
/// # Safety
/// `q` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glq_create(q: *mut Query) {
    let value = oq::global().map_or(Query::default(), Query::create);
    // SAFETY: caller contract.
    unsafe { q.write(value) }
}

/// # Safety
/// `q` was initialised by `ohpl_glq_create`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glq_destroy(q: *const Query) {
    if let Some(gl) = oq::global() {
        // SAFETY: caller contract.
        unsafe { &*q }.destroy(gl);
    }
}

/// # Safety
/// `q` was initialised by `ohpl_glq_create`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glq_begin(q: *mut Query) {
    if let Some(gl) = oq::global() {
        // SAFETY: caller contract.
        unsafe { &mut *q }.begin(gl);
    }
}

/// # Safety
/// `q` was initialised by `ohpl_glq_create`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glq_end(q: *const Query) {
    if let Some(gl) = oq::global() {
        // SAFETY: caller contract.
        unsafe { &*q }.end(gl);
    }
}

/// # Safety
/// `q` was initialised by `ohpl_glq_create`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_glq_fetch(q: *mut Query) -> bool {
    match oq::global() {
        // SAFETY: caller contract.
        Some(gl) => unsafe { &mut *q }.fetch(gl),
        None => true,
    }
}

const _: () = {
    use core::mem::size_of;
    assert!(size_of::<Query>() == 12 && size_of::<WindowCfg>() == 56);
};
