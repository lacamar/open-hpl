//! C ABI for the render plumbing, declared in `graphics/RustRender.h`.
//!
//! Unless noted, pointers are non-null, valid and aligned for the call; a null makes the call a
//! no-op returning the zero value.

use std::ffi::c_void;
use std::slice;

use super::combo::ComboCache;
use super::funcs::{Gpu, Log, State};
use super::list::{DecalKey, IllumKey, SolidKey, TransKey};
use crate::types::{Vector2f, Vector2l, Vector3f};

/// # Safety
/// `p` is null or points at `n` valid elements.
unsafe fn items<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: by the caller's contract.
        unsafe { slice::from_raw_parts(p, n) }
    }
}

#[repr(C)]
pub struct CompileInC {
    pub flags: u32,
    pub origin: Vector3f,
    pub solid: *const SolidKey,
    pub solid_len: usize,
    pub trans: *mut TransKey,
    pub trans_len: usize,
    pub decal: *const DecalKey,
    pub decal_len: usize,
    pub illum: *const IllumKey,
    pub illum_len: usize,
}

/// Each array has room for the matching `*_len` indices.
#[repr(C)]
pub struct CompileOutC {
    pub z: *mut u32,
    pub diffuse: *mut u32,
    pub trans: *mut u32,
    pub decal: *mut u32,
    pub illum: *mut u32,
}

macro_rules! backend {
    ($($name:ident($($a:ident: $t:ty),*) $(-> $r:ty)?;)*) => {
        /// `iLowLevelGraphics` behind C callbacks; `ctx` is passed back as the first argument.
        #[repr(C)]
        pub struct Backend {
            pub ctx: *mut c_void,
            $(pub $name: unsafe extern "C" fn(*mut c_void $(, $t)*) $(-> $r)?,)*
        }
    };
}

backend! {
    depth_test(on: bool);
    depth_write(on: bool);
    depth_func(func: u32);
    cull_active(on: bool);
    cull_mode(mode: u32);
    stencil_active(on: bool);
    scissor_active(on: bool);
    scissor_rect(pos: Vector2l, size: Vector2l);
    color_write(r: bool, g: bool, b: bool, a: bool);
    alpha_test_active(on: bool);
    alpha_test_greater_or_equal(limit: f32);
    blend_active(on: bool);
    blend_func(src: u32, dst: u32);
    program_bind(program: usize);
    program_unbind(program: usize);
    texture(unit: i32, texture: usize);
    vertex_buffer_unbind(vb: usize);
    vertex_buffer_bind(vb: usize);
    set_projection(matrix: usize);
    set_model_matrix(matrix: usize);
    set_frame_buffer(fb: usize, use_pos_size: bool, pos: Vector2l, size: Vector2l);
    frame_buffer_size(fb: usize, out: *mut Vector2l);
    clear_frame_buffer(flags: u32);
    log(event: i32, a: i64, b: i64, c: i64, d: i64, f: f32);
}

struct CGpu<'a>(&'a Backend);

impl Gpu for CGpu<'_> {
    fn depth_test(&mut self, on: bool) {
        // SAFETY: `Backend` callbacks are valid for the call by the FFI contract.
        unsafe { (self.0.depth_test)(self.0.ctx, on) }
    }
    fn depth_write(&mut self, on: bool) {
        // SAFETY: as above.
        unsafe { (self.0.depth_write)(self.0.ctx, on) }
    }
    fn depth_func(&mut self, func: u32) {
        // SAFETY: as above.
        unsafe { (self.0.depth_func)(self.0.ctx, func) }
    }
    fn cull_active(&mut self, on: bool) {
        // SAFETY: as above.
        unsafe { (self.0.cull_active)(self.0.ctx, on) }
    }
    fn cull_mode(&mut self, mode: u32) {
        // SAFETY: as above.
        unsafe { (self.0.cull_mode)(self.0.ctx, mode) }
    }
    fn stencil_active(&mut self, on: bool) {
        // SAFETY: as above.
        unsafe { (self.0.stencil_active)(self.0.ctx, on) }
    }
    fn scissor_active(&mut self, on: bool) {
        // SAFETY: as above.
        unsafe { (self.0.scissor_active)(self.0.ctx, on) }
    }
    fn scissor_rect(&mut self, pos: Vector2l, size: Vector2l) {
        // SAFETY: as above.
        unsafe { (self.0.scissor_rect)(self.0.ctx, pos, size) }
    }
    fn color_write(&mut self, r: bool, g: bool, b: bool, a: bool) {
        // SAFETY: as above.
        unsafe { (self.0.color_write)(self.0.ctx, r, g, b, a) }
    }
    fn alpha_test_active(&mut self, on: bool) {
        // SAFETY: as above.
        unsafe { (self.0.alpha_test_active)(self.0.ctx, on) }
    }
    fn alpha_test_greater_or_equal(&mut self, limit: f32) {
        // SAFETY: as above.
        unsafe { (self.0.alpha_test_greater_or_equal)(self.0.ctx, limit) }
    }
    fn blend_active(&mut self, on: bool) {
        // SAFETY: as above.
        unsafe { (self.0.blend_active)(self.0.ctx, on) }
    }
    fn blend_func(&mut self, src: u32, dst: u32) {
        // SAFETY: as above.
        unsafe { (self.0.blend_func)(self.0.ctx, src, dst) }
    }
    fn program_bind(&mut self, program: usize) {
        // SAFETY: as above.
        unsafe { (self.0.program_bind)(self.0.ctx, program) }
    }
    fn program_unbind(&mut self, program: usize) {
        // SAFETY: as above.
        unsafe { (self.0.program_unbind)(self.0.ctx, program) }
    }
    fn texture(&mut self, unit: i32, texture: usize) {
        // SAFETY: as above.
        unsafe { (self.0.texture)(self.0.ctx, unit, texture) }
    }
    fn vertex_buffer_unbind(&mut self, vb: usize) {
        // SAFETY: as above.
        unsafe { (self.0.vertex_buffer_unbind)(self.0.ctx, vb) }
    }
    fn vertex_buffer_bind(&mut self, vb: usize) {
        // SAFETY: as above.
        unsafe { (self.0.vertex_buffer_bind)(self.0.ctx, vb) }
    }
    fn set_projection(&mut self, matrix: usize) {
        // SAFETY: as above.
        unsafe { (self.0.set_projection)(self.0.ctx, matrix) }
    }
    fn set_model_matrix(&mut self, matrix: usize) {
        // SAFETY: as above.
        unsafe { (self.0.set_model_matrix)(self.0.ctx, matrix) }
    }
    fn set_frame_buffer(&mut self, fb: usize, use_pos_size: bool, pos: Vector2l, size: Vector2l) {
        // SAFETY: as above.
        unsafe { (self.0.set_frame_buffer)(self.0.ctx, fb, use_pos_size, pos, size) }
    }
    fn frame_buffer_size(&mut self, fb: usize) -> Vector2l {
        let mut out = Vector2l::default();
        // SAFETY: as above; `out` is a live local.
        unsafe { (self.0.frame_buffer_size)(self.0.ctx, fb, &raw mut out) };
        out
    }
    fn clear_frame_buffer(&mut self, flags: u32) {
        // SAFETY: as above.
        unsafe { (self.0.clear_frame_buffer)(self.0.ctx, flags) }
    }
    fn log(&mut self, event: Log, a: i64, b: i64, c: i64, d: i64, f: f32) {
        // SAFETY: as above.
        unsafe { (self.0.log)(self.0.ctx, event as i32, a, b, c, d, f) }
    }
}

macro_rules! rf {
    ($(fn $name:ident($($a:ident: $t:ty),*) $(-> $r:ty)? => |$s:ident, $g:ident| $body:expr;)*) => {$(
        /// # Safety
        /// `st` is the state of one live `iRenderFunctions`, `be` its callbacks.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(st: *mut State, be: *const Backend $(, $a: $t)*) $(-> $r)? {
            // SAFETY: the caller's contract.
            let (Some($s), Some(be)) = (unsafe { st.as_mut() }, unsafe { be.as_ref() }) else {
                return Default::default();
            };
            let mut $g = CGpu(be);
            $body
        }
    )*};
}

rf! {
    fn ohpl_rf_exit() => |s, g| s.exit(&mut g);
    fn ohpl_rf_flat_projection_set(null_matrix: usize) => |s, g| s.flat_projection_set(&mut g, null_matrix);
    fn ohpl_rf_set_projection_matrix(null_matrix: usize, matrix: usize) => |s, g| {
        s.set_projection_matrix(&mut g, null_matrix, matrix)
    };
    fn ohpl_rf_set_depth_test(on: bool) -> bool => |s, g| s.set_depth_test(&mut g, on);
    fn ohpl_rf_set_depth_write(on: bool) -> bool => |s, g| s.set_depth_write(&mut g, on);
    fn ohpl_rf_set_depth_test_func(func: u32) -> bool => |s, g| s.set_depth_test_func(&mut g, func);
    fn ohpl_rf_set_cull_active(on: bool) -> bool => |s, g| s.set_cull_active(&mut g, on);
    fn ohpl_rf_set_cull_mode(mode: u32, check_inverted: bool) -> bool => |s, g| {
        s.set_cull_mode(&mut g, mode, check_inverted)
    };
    fn ohpl_rf_set_stencil_active(on: bool) -> bool => |s, g| s.set_stencil_active(&mut g, on);
    fn ohpl_rf_set_scissor_active(on: bool, target_pos: Vector2l) -> bool => |s, g| {
        s.set_scissor_active(&mut g, on, target_pos)
    };
    fn ohpl_rf_set_scissor_rect(pos: Vector2l, size: Vector2l, auto_enable: bool, target_pos: Vector2l) -> bool => |s, g| {
        s.set_scissor_rect(&mut g, pos, size, auto_enable, target_pos)
    };
    fn ohpl_rf_set_channel_mode(mode: u32) -> bool => |s, g| s.set_channel_mode(&mut g, mode);
    fn ohpl_rf_set_alpha_mode(mode: u32) -> bool => |s, g| s.set_alpha_mode(&mut g, mode);
    fn ohpl_rf_set_alpha_limit(limit: f32) -> bool => |s, g| s.set_alpha_limit(&mut g, limit);
    fn ohpl_rf_set_blend_mode(mode: u32) -> bool => |s, g| s.set_blend_mode(&mut g, mode);
    fn ohpl_rf_set_program(program: usize) -> bool => |s, g| s.set_program(&mut g, program);
    fn ohpl_rf_set_texture(unit: i32, texture: usize) => |s, g| s.set_texture(&mut g, unit, texture);
    fn ohpl_rf_set_texture_range(texture: usize, first: i32, last: i32) => |s, g| {
        s.set_texture_range(&mut g, texture, first, last)
    };
    fn ohpl_rf_set_vertex_buffer(vb: usize) => |s, g| s.set_vertex_buffer(&mut g, vb);
    fn ohpl_rf_set_matrix(matrix: usize) => |s, g| s.set_matrix(&mut g, matrix);
    fn ohpl_rf_set_invert_cull_mode(on: bool) => |s, g| s.set_invert_cull(&mut g, on);
    fn ohpl_rf_set_frame_buffer(fb: usize, use_pos_size: bool, use_global_scissor: bool, target_pos: Vector2l) => |s, g| {
        s.set_frame_buffer(&mut g, fb, use_pos_size, use_global_scissor, target_pos)
    };
    fn ohpl_rf_clear_frame_buffer(flags: u32, use_pos_size: bool, target_pos: Vector2l) => |s, g| {
        s.clear_frame_buffer(&mut g, flags, use_pos_size, target_pos)
    };
}

/// `InitAndResetRenderFunctions` state reset.
///
/// # Safety
/// `st` is the state of one live `iRenderFunctions`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rf_init(
    st: *mut State,
    null_matrix: usize,
    log: bool,
    use_global_scissor: bool,
    global_pos: Vector2l,
    global_size: Vector2l,
    screen: Vector2l,
    screen_float: Vector2f,
    frame_buffer_size: Vector2l,
    target_size: Vector2l,
) {
    // SAFETY: the caller's contract.
    if let Some(s) = unsafe { st.as_mut() } {
        s.init(
            null_matrix,
            log,
            use_global_scissor,
            global_pos,
            global_size,
            screen,
            screen_float,
            frame_buffer_size,
            target_size,
        );
    }
}

/// Plain model-view change made by the C++ (`SetModelViewMatrix`, flat projections).
///
/// # Safety
/// `st` is the state of one live `iRenderFunctions`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rf_model_view_set(st: *mut State, null_matrix: usize) {
    // SAFETY: the caller's contract.
    if let Some(s) = unsafe { st.as_mut() } {
        s.matrix = null_matrix;
    }
}

/// Opaque combo cache, one per `cProgramComboManager`.
pub struct ComboHandle(ComboCache);

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_combo_new(modes: usize) -> *mut ComboHandle {
    Box::into_raw(Box::new(ComboHandle(ComboCache::new(modes))))
}

/// # Safety
/// `h` is live or null.
unsafe fn cache<'a>(h: *mut ComboHandle) -> Option<&'a mut ComboCache> {
    // SAFETY: the caller's contract.
    unsafe { h.as_mut() }.map(|h| &mut h.0)
}

/// Returns the combination count.
///
/// # Safety
/// `h` is live; `stages` points at `n` values.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_combo_setup(
    h: *mut ComboHandle,
    mode: i32,
    stages: *const u32,
    n: usize,
) -> i32 {
    // SAFETY: the caller's contract.
    let (Some(c), stages) = (unsafe { cache(h) }, unsafe { items(stages, n) }) else {
        return 0;
    };
    usize::try_from(mode).map_or(0, |m| c.setup(m, stages))
}
