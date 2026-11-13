//! `GlGfx`: `Gfx` over the Rust GL context (`backend::gl_ctx`). State, matrices, clears,
//! immediate-mode drawing and the vertex batch run in Rust against the context the C++
//! `cLowLevelGraphicsSDL` shell holds. Everything that touches C++-held state or objects (resources,
//! frame-buffer binding, scissor and gamma, caps) goes to the `inner` device, the facade over that same
//! shell, whose textures, vertex buffers, programs and frame buffers already hold `gl_res` handles.

use super::h5b_gfx::*;
use super::h5b_imm::Batch;
use crate::backend::gl_ctx::ctx::{Ctx, Sources};

pub struct GlGfx<F: Gfx> {
    ctx: *mut Ctx,
    inner: F,
    batch: Batch,
}

impl<F: Gfx> GlGfx<F> {
    /// # Safety
    /// `ctx` outlives `self` and is only used from the render thread.
    pub unsafe fn new(ctx: *mut Ctx, inner: F) -> Self {
        Self {
            ctx,
            inner,
            batch: Batch::new(),
        }
    }

    pub fn inner(&self) -> &F {
        &self.inner
    }

    fn c(&mut self) -> Option<&mut Ctx> {
        // SAFETY: `new` contract; the borrow ends before any call back into C++.
        unsafe { self.ctx.as_mut() }
    }

    fn flush(&mut self, quads: bool, flags: u32, auto_clear: bool) {
        let stride = (super::h5b_imm::BATCH_STRIDE * 4) as i32;
        let (count, idx) = (self.batch.indices.len() as i32, self.batch.indices.as_ptr());
        let tex = self.batch.texcoord_ptrs();
        let src = Sources {
            element: core::ptr::null(),
            batch_vertex: self.batch.vertex_ptr(),
            batch_texcoord: tex.as_ptr(),
        };
        if let Some(c) = self.c() {
            // SAFETY: the pointers come from the batch arrays, alive for the call.
            unsafe { c.flush_batch(quads, flags, stride, count, idx.cast(), &src) };
        }
        if auto_clear {
            self.batch.clear_all();
        }
    }
}

macro_rules! ctx {
    ($s:ident, $c:ident => $e:expr) => {
        if let Some($c) = $s.c() {
            $e
        }
    };
}

macro_rules! fwd {
    ($($vis:vis fn $n:ident(&mut self $(, $a:ident: $t:ty)*) $(-> $r:ty)?;)*) => {
        $($vis fn $n(&mut self $(, $a: $t)*) $(-> $r)? { self.inner.$n($($a),*) })*
    };
}

impl<F: Gfx> Imm for GlGfx<F> {
    fn im_begin(&mut self, mode: u32) {
        ctx!(self, c => c.begin(mode));
    }
    fn im_end(&mut self) {
        ctx!(self, c => c.end());
    }
    fn im_vertex3(&mut self, x: f32, y: f32, z: f32) {
        ctx!(self, c => c.vertex3f(x, y, z));
    }
    fn im_color4(&mut self, r: f32, g: f32, b: f32, a: f32) {
        ctx!(self, c => c.color4f(r, g, b, a));
    }
    fn im_texcoord2(&mut self, s: f32, t: f32) {
        ctx!(self, c => c.texcoord2f(s, t));
    }
    fn im_texcoord3(&mut self, s: f32, t: f32, r: f32) {
        ctx!(self, c => c.texcoord3f(s, t, r));
    }
    fn im_multitexcoord2(&mut self, unit: u32, s: f32, t: f32) {
        ctx!(self, c => c.multitexcoord2f(unit, s, t));
    }
    fn im_multitexcoord3(&mut self, unit: u32, s: f32, t: f32, r: f32) {
        ctx!(self, c => c.multitexcoord3f(unit, s, t, r));
    }
    fn im_clear_texture0(&mut self) {
        self.inner.set_texture(0, None);
    }
    fn im_blend_off(&mut self) {
        ctx!(self, c => c.set_blend_active(false));
    }
}

impl<F: Gfx> Draw for GlGfx<F> {
    fn batch(&mut self) -> &mut Batch {
        &mut self.batch
    }

    fn set_batch_texture_unit_active(&mut self, unit: u32, active: bool) {
        let stride = (super::h5b_imm::BATCH_STRIDE * 4) as i32;
        let tex = self.batch.texcoord_ptrs();
        let src = Sources {
            element: core::ptr::null(),
            batch_vertex: self.batch.vertex_ptr(),
            batch_texcoord: tex.as_ptr(),
        };
        if let Some(c) = self.c() {
            // SAFETY: the pointers come from the batch arrays, alive for the call.
            unsafe { c.batch_texture_unit(unit, active, stride, &src) };
        }
    }

    fn flush_tri_batch(&mut self, flags: u32, auto_clear: bool) {
        self.flush(false, flags, auto_clear);
    }

    fn flush_quad_batch(&mut self, flags: u32, auto_clear: bool) {
        self.flush(true, flags, auto_clear);
    }
}

impl<F: Gfx> Gfx for GlGfx<F> {
    fwd! {
        fn init(&mut self, a: &InitArgs) -> bool;
        fn gpu_program_format(&mut self) -> i32;
        fn caps(&mut self, kind: i32) -> i32;
        fn renderer_description(&mut self) -> String;
        fn window_handle(&mut self) -> usize;
        fn set_color_output(&mut self, mode: &str, paper_white: f32, peak_nits: f32) -> bool;
        fn color_output(&mut self) -> String;
        fn hdr_headroom(&mut self) -> f32;
        fn screen_float_stats(&mut self) -> Option<[f32; 3]>;
        fn show_cursor(&mut self, on: bool);
        fn set_window_grab(&mut self, on: bool);
        fn set_relative_mouse(&mut self, on: bool);
        fn set_window_caption(&mut self, name: &str);
        fn window_mouse_focus(&mut self) -> bool;
        fn window_input_focus(&mut self) -> bool;
        fn window_is_visible(&mut self) -> bool;
        fn multisampling(&mut self) -> i32;
        fn screen_size_float(&mut self) -> V2f;
        fn screen_size_int(&mut self) -> V2l;
        fn check_update_screen_size(&mut self) -> bool;
        fn force_window_size(&mut self, w: i32, h: i32) -> bool;
        fn fullscreen_active(&mut self) -> bool;
        fn set_vsync(&mut self, on: bool, adaptive: bool);
        fn set_multisampling_active(&mut self, on: bool);
        fn set_gamma(&mut self, g: f32);
        fn gamma(&mut self) -> f32;
        fn create_font(&mut self, name: &str) -> FontId;
        fn create_program(&mut self, name: &str) -> ProgId;
        fn create_shader(&mut self, name: &str, kind: i32) -> ShaderId;
        fn create_texture(&mut self, name: &str, kind: i32, usage: i32) -> TexId;
        fn create_vertex_buffer(&mut self, kind: i32, draw: i32, usage: i32, rvtx: i32, ridx: i32) -> VbId;
        fn create_frame_buffer(&mut self, name: &str) -> FbId;
        fn create_depth_stencil(&mut self, size: V2l, depth_bits: i32, stencil_bits: i32) -> DsbId;
        fn create_occlusion_query(&mut self) -> OqId;
        fn destroy_font(&mut self, id: FontId);
        fn destroy_program(&mut self, id: ProgId);
        fn destroy_shader(&mut self, id: ShaderId);
        fn destroy_texture(&mut self, id: TexId);
        fn destroy_vertex_buffer(&mut self, id: VbId);
        fn destroy_frame_buffer(&mut self, id: FbId);
        fn destroy_depth_stencil(&mut self, id: DsbId);
        fn destroy_occlusion_query(&mut self, id: OqId);
        fn swap_buffers(&mut self);
        fn copy_frame_buffer_to_texture(&mut self, t: TexId, pos: V2l, size: V2l, tex_off: V2l);
        fn copy_frame_buffer_to_bitmap(&mut self, pos: V2l, size: V2l) -> Bmp;
        fn set_current_frame_buffer(&mut self, fb: Option<FbId>, pos: V2l, size: V2l);
        fn current_frame_buffer(&mut self) -> Option<FbId>;
        fn set_frame_buffer_draw_targets(&mut self, targets: &[i32], count: i32);
        fn set_scissor_rect(&mut self, pos: V2l, size: V2l);
        fn set_clip_plane(&mut self, idx: i32, plane: Plane);
        fn clip_plane(&mut self, idx: i32) -> Plane;
        fn set_texture(&mut self, unit: u32, t: Option<TexId>);
        fn tex_create_from_bitmap(&mut self, t: TexId, b: &Bmp) -> bool;
        fn tex_create_anim(&mut self, t: TexId, b: &[Bmp]) -> bool;
        fn tex_create_cube(&mut self, t: TexId, b: &[Bmp]) -> bool;
        fn tex_create_raw(&mut self, t: TexId, size: V3l, fmt: i32, data: Option<&[u8]>) -> bool;
        fn tex_set_raw(&mut self, t: TexId, level: i32, off: V3l, size: V3l, fmt: i32, data: Option<&[u8]>);
        fn tex_update(&mut self, t: TexId, dt: f32);
        fn tex_set_filter(&mut self, t: TexId, f: i32);
        fn tex_set_anisotropy(&mut self, t: TexId, x: f32);
        fn tex_set_wrap_s(&mut self, t: TexId, m: i32);
        fn tex_set_wrap_t(&mut self, t: TexId, m: i32);
        fn tex_set_wrap_r(&mut self, t: TexId, m: i32);
        fn tex_set_wrap_str(&mut self, t: TexId, m: i32);
        fn tex_set_compare_mode(&mut self, t: TexId, m: i32);
        fn tex_set_compare_func(&mut self, t: TexId, f: i32);
        fn tex_set_red_from_alpha(&mut self, t: TexId, on: bool);
        fn tex_auto_mipmaps(&mut self, t: TexId);
        fn tex_has_animation(&mut self, t: TexId) -> bool;
        fn tex_next_frame(&mut self, t: TexId);
        fn tex_prev_frame(&mut self, t: TexId);
        fn tex_time_count(&mut self, t: TexId) -> f32;
        fn tex_set_time_count(&mut self, t: TexId, x: f32);
        fn tex_handle(&mut self, t: TexId) -> i32;
        fn tex_size(&mut self, t: TexId) -> V3l;
        fn tex_pixels_rgba_float(&mut self, t: TexId) -> Option<Vec<f32>>;
        fn vb_create_element_array(&mut self, v: VbId, kind: i32, format: i32, num: i32, var: i32);
        fn vb_add_vec3(&mut self, v: VbId, element: i32, p: V3f);
        fn vb_add_vec4(&mut self, v: VbId, element: i32, p: V3f, w: f32);
        fn vb_add_color(&mut self, v: VbId, element: i32, c: Col);
        fn vb_add_index(&mut self, v: VbId, i: u32);
        fn vb_compile(&mut self, v: VbId, flags: u32) -> bool;
        fn vb_update(&mut self, v: VbId, types: u32, indices: bool);
        fn vb_create_shadow_double(&mut self, v: VbId, update: bool);
        fn vb_transform(&mut self, v: VbId, m: &Mat);
        fn vb_draw(&mut self, v: VbId, draw: i32);
        fn vb_draw_indices(&mut self, v: VbId, idx: &[u32], count: i32, draw: i32);
        fn vb_bind(&mut self, v: VbId);
        fn vb_unbind(&mut self, v: VbId);
        fn vb_create_copy(&mut self, v: VbId, kind: i32, usage: i32, flags: u32) -> VbId;
        fn vb_bounding_volume(&mut self, v: VbId) -> (V3f, V3f);
        fn vb_element_num(&mut self, v: VbId, element: i32) -> i32;
        fn vb_element_format(&mut self, v: VbId, element: i32) -> i32;
        fn vb_element_var(&mut self, v: VbId, element: i32) -> i32;
        fn vb_vertex_num(&mut self, v: VbId) -> i32;
        fn vb_index_num(&mut self, v: VbId) -> i32;
        fn vb_resize_array(&mut self, v: VbId, element: i32, size: i32);
        fn vb_resize_indices(&mut self, v: VbId, size: i32);
        fn shader_from_file(&mut self, s: ShaderId, file: &str, entry: &str, print: bool) -> bool;
        fn shader_from_string(&mut self, s: ShaderId, src: &str, entry: &str, print: bool) -> bool;
        fn prog_set_shader(&mut self, p: ProgId, kind: i32, s: Option<ShaderId>);
        fn prog_link(&mut self, p: ProgId) -> bool;
        fn prog_dump_uniforms(&mut self, p: ProgId) -> String;
        fn prog_bind(&mut self, p: ProgId);
        fn prog_unbind(&mut self, p: ProgId);
        fn prog_set_sampler_to_unit(&mut self, p: ProgId, name: &str, unit: i32) -> bool;
        fn prog_variable_id(&mut self, p: ProgId, name: &str) -> i32;
        fn prog_variable_as_id(&mut self, p: ProgId, name: &str, id: i32) -> bool;
        fn prog_set_int(&mut self, p: ProgId, var: i32, x: i32) -> bool;
        fn prog_set_float(&mut self, p: ProgId, var: i32, x: f32) -> bool;
        fn prog_set_vec2(&mut self, p: ProgId, var: i32, v: V2f) -> bool;
        fn prog_set_vec3(&mut self, p: ProgId, var: i32, v: V3f) -> bool;
        fn prog_set_vec4(&mut self, p: ProgId, var: i32, v: Col) -> bool;
        fn prog_set_float_array(&mut self, p: ProgId, var: i32, x: &[f32]) -> bool;
        fn prog_set_matrix(&mut self, p: ProgId, var: i32, m: &Mat) -> bool;
        fn prog_set_matrix_api(&mut self, p: ProgId, var: i32, kind: i32, op: i32) -> bool;
        fn fb_set_texture_2d(&mut self, f: FbId, idx: i32, t: Option<TexId>, mip: i32);
        fn fb_set_texture_3d(&mut self, f: FbId, idx: i32, t: Option<TexId>, z: i32, mip: i32);
        fn fb_set_texture_cube(&mut self, f: FbId, idx: i32, t: Option<TexId>, face: i32, mip: i32);
        fn fb_set_depth_texture_2d(&mut self, f: FbId, t: Option<TexId>, mip: i32);
        fn fb_set_depth_texture_cube(&mut self, f: FbId, t: Option<TexId>, face: i32, mip: i32);
        fn fb_set_depth_stencil(&mut self, f: FbId, d: Option<DsbId>);
        fn fb_compile_and_validate(&mut self, f: FbId) -> bool;
        fn fb_post_bind_update(&mut self, f: FbId);
        fn oq_begin(&mut self, q: OqId);
        fn oq_end(&mut self, q: OqId);
        fn oq_fetch_results(&mut self, q: OqId) -> bool;
        fn oq_sample_count(&mut self, q: OqId) -> u32;
        fn font_from_font_file(&mut self, f: FontId, file: &str, size: i32, first: u16, last: u16) -> bool;
        fn font_from_bitmap_file(&mut self, f: FontId, file: &str) -> bool;
    }

    fn set_color_write_active(&mut self, r: bool, g: bool, b: bool, a: bool) {
        ctx!(self, c => c.set_color_write(r, g, b, a));
    }
    fn set_depth_write_active(&mut self, on: bool) {
        ctx!(self, c => c.set_depth_write(on));
    }
    fn set_cull_active(&mut self, on: bool) {
        ctx!(self, c => c.set_cull_active(on));
    }
    fn set_cull_mode(&mut self, mode: i32) {
        ctx!(self, c => c.set_cull_mode(mode));
    }
    fn set_depth_test_active(&mut self, on: bool) {
        ctx!(self, c => c.set_depth_test(on));
    }
    fn set_depth_test_func(&mut self, f: i32) {
        ctx!(self, c => c.set_depth_func(f));
    }
    fn set_alpha_test_active(&mut self, on: bool) {
        ctx!(self, c => c.set_alpha_test(on));
    }
    fn set_alpha_test_func(&mut self, f: i32, reference: f32) {
        ctx!(self, c => c.set_alpha_func(f, reference));
    }
    fn set_stencil_active(&mut self, on: bool) {
        ctx!(self, c => c.set_stencil_active(on));
    }
    fn set_stencil_write_mask(&mut self, mask: u32) {
        ctx!(self, c => c.set_stencil_write_mask(mask));
    }
    fn set_stencil(
        &mut self,
        f: i32,
        reference: i32,
        mask: u32,
        fail: i32,
        zfail: i32,
        zpass: i32,
    ) {
        ctx!(self, c => c.set_stencil(f, reference, mask, fail, zfail, zpass));
    }
    fn set_stencil_two_side(
        &mut self,
        front: i32,
        back: i32,
        reference: i32,
        mask: u32,
        ops: [i32; 6],
    ) {
        ctx!(self, c => c.set_stencil_two_side([front, back], reference, mask, ops));
    }
    fn set_scissor_active(&mut self, on: bool) {
        ctx!(self, c => c.set_scissor_active(on));
    }
    fn set_clip_plane_active(&mut self, idx: i32, on: bool) {
        ctx!(self, c => c.set_clip_plane_active(idx, on));
    }
    fn set_color(&mut self, col: Col) {
        ctx!(self, c => c.set_color(col[0], col[1], col[2], col[3]));
    }
    fn set_blend_active(&mut self, on: bool) {
        ctx!(self, c => c.set_blend_active(on));
    }
    fn set_blend_func(&mut self, src: i32, dst: i32) {
        ctx!(self, c => c.set_blend_func(src, dst));
    }
    fn set_blend_func_separate(&mut self, sc: i32, dc: i32, sa: i32, da: i32) {
        ctx!(self, c => c.set_blend_func_separate(sc, dc, sa, da));
    }
    fn set_polygon_offset_active(&mut self, on: bool) {
        ctx!(self, c => c.set_polygon_offset_active(on));
    }
    fn set_depth_clamp_active(&mut self, on: bool) {
        ctx!(self, c => c.set_depth_clamp_active(on));
    }
    fn set_polygon_offset(&mut self, bias: f32, slope: f32) {
        ctx!(self, c => c.set_polygon_offset(bias, slope));
    }
    fn push_matrix(&mut self, kind: i32) {
        ctx!(self, c => c.push_matrix(kind));
    }
    fn pop_matrix(&mut self, kind: i32) {
        ctx!(self, c => c.pop_matrix(kind));
    }
    fn set_identity_matrix(&mut self, kind: i32) {
        ctx!(self, c => c.set_identity_matrix(kind));
    }
    fn set_matrix(&mut self, kind: i32, m: &Mat) {
        ctx!(self, c => c.set_matrix(kind, m));
    }
    fn set_ortho_size(&mut self, size: V2f, min: f32, max: f32) {
        ctx!(self, c => c.set_ortho(0.0, size[0], size[1], 0.0, min, max));
    }
    fn set_ortho_box(&mut self, min: V3f, max: V3f) {
        ctx!(self, c => c.set_ortho(min[0], max[0], max[1], min[1], min[2], max[2]));
    }
    fn set_active_texture_unit(&mut self, unit: u32) {
        ctx!(self, c => c.set_active_texture_unit(unit));
    }
    fn set_texture_env(&mut self, param: i32, val: i32) {
        ctx!(self, c => c.set_texture_env(param, val));
    }
    fn set_texture_constant_color(&mut self, col: Col) {
        ctx!(self, c => c.set_texture_constant_color(col));
    }
    fn set_clear_color(&mut self, col: Col) {
        ctx!(self, c => c.set_clear_color(col[0], col[1], col[2], col[3]));
    }
    fn set_clear_depth(&mut self, d: f32) {
        ctx!(self, c => c.set_clear_depth(d));
    }
    fn set_clear_stencil(&mut self, v: i32) {
        ctx!(self, c => c.set_clear_stencil(v));
    }
    fn clear_frame_buffer(&mut self, flags: u32) {
        ctx!(self, c => c.clear(flags));
    }
    fn flush_rendering(&mut self) {
        ctx!(self, c => c.flush());
    }
    fn wait_and_finish_rendering(&mut self) {
        ctx!(self, c => c.finish());
    }
}
