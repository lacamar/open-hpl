//! `trait Gfx`: the `iLowLevelGraphics` family (device, textures, vertex buffers, programs, frame
//! buffers, queries, fonts) as one Rust interface. Resources are ids; enum arguments carry the raw
//! HPL2 enum values so every implementation sees the C++ call unchanged.

use super::h5b_imm::{self, Batch};

pub type V2f = [f32; 2];
pub type V3f = [f32; 3];
pub type V2l = [i32; 2];
pub type V3l = [i32; 3];
pub type Col = [f32; 4];
pub type Plane = [f32; 4];
pub type Rect = [f32; 4];
pub type Mat = [f32; 16];

/// `cVertex`.
#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct Vtx {
    pub pos: V3f,
    pub tex: V3f,
    pub tan: V3f,
    pub norm: V3f,
    pub col: Col,
}

macro_rules! ids {
    ($($n:ident),*) => {$(
        /// Resource id: the creation order within its kind.
        #[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
        pub struct $n(pub u32);
    )*};
}
ids!(TexId, VbId, ProgId, ShaderId, FbId, DsbId, OqId, FontId);

/// `cBitmap` as the graphics layer sees it: `images[image][mip]` are the raw level bytes.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Bmp {
    pub size: V3l,
    pub format: i32,
    pub images: Vec<Vec<Vec<u8>>>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct InitArgs {
    pub size: V2l,
    pub display: i32,
    pub bpp: i32,
    pub fullscreen: i32,
    pub multisampling: i32,
    pub program_format: i32,
    pub caption: String,
    pub window_pos: V2l,
}

/// Immediate-mode primitive modes (`IM_*`): the GL enum values the C++ passes.
pub mod prim {
    pub const LINES: u32 = 0x1;
    pub const LINE_STRIP: u32 = 0x3;
    pub const TRIANGLES: u32 = 0x4;
    pub const QUADS: u32 = 0x7;
    pub const TEXTURE0: u32 = 0x84C0;
}

/// The immediate-mode primitives the draw helpers are written against.
pub trait Imm {
    fn im_begin(&mut self, mode: u32);
    fn im_end(&mut self);
    fn im_vertex3(&mut self, x: f32, y: f32, z: f32);
    fn im_color4(&mut self, r: f32, g: f32, b: f32, a: f32);
    fn im_texcoord2(&mut self, s: f32, t: f32);
    fn im_texcoord3(&mut self, s: f32, t: f32, r: f32);
    fn im_multitexcoord2(&mut self, unit: u32, s: f32, t: f32);
    fn im_multitexcoord3(&mut self, unit: u32, s: f32, t: f32, r: f32);
    /// `SetTexture(0, NULL)` of the coloured sphere.
    fn im_clear_texture0(&mut self);
    /// `SetBlendActive(false)` of the coloured sphere.
    fn im_blend_off(&mut self);
}

pub trait Gfx: Imm {
    fn batch(&mut self) -> &mut Batch;

    // ---- device
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

    // ---- creation
    fn create_font(&mut self, name: &str) -> FontId;
    fn create_program(&mut self, name: &str) -> ProgId;
    fn create_shader(&mut self, name: &str, kind: i32) -> ShaderId;
    fn create_texture(&mut self, name: &str, kind: i32, usage: i32) -> TexId;
    fn create_vertex_buffer(
        &mut self,
        kind: i32,
        draw: i32,
        usage: i32,
        rvtx: i32,
        ridx: i32,
    ) -> VbId;
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

    // ---- frame buffer
    fn clear_frame_buffer(&mut self, flags: u32);
    fn set_clear_color(&mut self, c: Col);
    fn set_clear_depth(&mut self, d: f32);
    fn set_clear_stencil(&mut self, v: i32);
    fn flush_rendering(&mut self);
    fn wait_and_finish_rendering(&mut self);
    fn swap_buffers(&mut self);
    fn copy_frame_buffer_to_texture(&mut self, t: TexId, pos: V2l, size: V2l, tex_off: V2l);
    fn copy_frame_buffer_to_bitmap(&mut self, pos: V2l, size: V2l) -> Bmp;
    fn set_current_frame_buffer(&mut self, fb: Option<FbId>, pos: V2l, size: V2l);
    fn current_frame_buffer(&mut self) -> Option<FbId>;
    fn set_frame_buffer_draw_targets(&mut self, targets: &[i32], count: i32);

    // ---- state
    fn set_color_write_active(&mut self, r: bool, g: bool, b: bool, a: bool);
    fn set_depth_write_active(&mut self, on: bool);
    fn set_cull_active(&mut self, on: bool);
    fn set_cull_mode(&mut self, mode: i32);
    fn set_depth_test_active(&mut self, on: bool);
    fn set_depth_test_func(&mut self, f: i32);
    fn set_alpha_test_active(&mut self, on: bool);
    fn set_alpha_test_func(&mut self, f: i32, reference: f32);
    fn set_stencil_active(&mut self, on: bool);
    fn set_stencil_write_mask(&mut self, mask: u32);
    fn set_stencil(&mut self, f: i32, reference: i32, mask: u32, fail: i32, zfail: i32, zpass: i32);
    /// `ops` = front fail/zfail/zpass, back fail/zfail/zpass.
    fn set_stencil_two_side(
        &mut self,
        front: i32,
        back: i32,
        reference: i32,
        mask: u32,
        ops: [i32; 6],
    );
    fn set_scissor_active(&mut self, on: bool);
    fn set_scissor_rect(&mut self, pos: V2l, size: V2l);
    fn set_clip_plane(&mut self, idx: i32, plane: Plane);
    fn clip_plane(&mut self, idx: i32) -> Plane;
    fn set_clip_plane_active(&mut self, idx: i32, on: bool);
    fn set_color(&mut self, c: Col);
    fn set_blend_active(&mut self, on: bool);
    fn set_blend_func(&mut self, src: i32, dst: i32);
    fn set_blend_func_separate(&mut self, sc: i32, dc: i32, sa: i32, da: i32);
    fn set_polygon_offset_active(&mut self, on: bool);
    fn set_depth_clamp_active(&mut self, on: bool);
    fn set_polygon_offset(&mut self, bias: f32, slope: f32);

    // ---- matrices and texture units
    fn push_matrix(&mut self, kind: i32);
    fn pop_matrix(&mut self, kind: i32);
    fn set_identity_matrix(&mut self, kind: i32);
    fn set_matrix(&mut self, kind: i32, m: &Mat);
    fn set_ortho_size(&mut self, size: V2f, min: f32, max: f32);
    fn set_ortho_box(&mut self, min: V3f, max: V3f);
    fn set_texture(&mut self, unit: u32, t: Option<TexId>);
    fn set_active_texture_unit(&mut self, unit: u32);
    fn set_texture_env(&mut self, param: i32, val: i32);
    fn set_texture_constant_color(&mut self, c: Col);

    // ---- textures
    fn tex_create_from_bitmap(&mut self, t: TexId, b: &Bmp) -> bool;
    fn tex_create_anim(&mut self, t: TexId, b: &[Bmp]) -> bool;
    fn tex_create_cube(&mut self, t: TexId, b: &[Bmp]) -> bool;
    fn tex_create_raw(&mut self, t: TexId, size: V3l, fmt: i32, data: Option<&[u8]>) -> bool;
    fn tex_set_raw(
        &mut self,
        t: TexId,
        level: i32,
        off: V3l,
        size: V3l,
        fmt: i32,
        data: Option<&[u8]>,
    );
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

    // ---- vertex buffers
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

    // ---- shaders and programs
    fn shader_from_file(&mut self, s: ShaderId, file: &str, entry: &str, print: bool) -> bool;
    fn shader_from_string(&mut self, s: ShaderId, src: &str, entry: &str, print: bool) -> bool;
    /// `iGpuProgram::SetShader`: base-class state, not a traced call.
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

    // ---- frame buffer objects
    fn fb_set_texture_2d(&mut self, f: FbId, idx: i32, t: Option<TexId>, mip: i32);
    fn fb_set_texture_3d(&mut self, f: FbId, idx: i32, t: Option<TexId>, z: i32, mip: i32);
    fn fb_set_texture_cube(&mut self, f: FbId, idx: i32, t: Option<TexId>, face: i32, mip: i32);
    fn fb_set_depth_texture_2d(&mut self, f: FbId, t: Option<TexId>, mip: i32);
    fn fb_set_depth_texture_cube(&mut self, f: FbId, t: Option<TexId>, face: i32, mip: i32);
    fn fb_set_depth_stencil(&mut self, f: FbId, d: Option<DsbId>);
    fn fb_compile_and_validate(&mut self, f: FbId) -> bool;
    fn fb_post_bind_update(&mut self, f: FbId);

    // ---- occlusion queries and fonts
    fn oq_begin(&mut self, q: OqId);
    fn oq_end(&mut self, q: OqId);
    fn oq_fetch_results(&mut self, q: OqId) -> bool;
    fn oq_sample_count(&mut self, q: OqId) -> u32;
    fn font_from_font_file(
        &mut self,
        f: FontId,
        file: &str,
        size: i32,
        first: u16,
        last: u16,
    ) -> bool;
    fn font_from_bitmap_file(&mut self, f: FontId, file: &str) -> bool;

    // ---- immediate drawing (`LowLevelGraphicsImmediate.inl`)
    fn draw_triangle(&mut self, v: &[Vtx]) {
        h5b_imm::draw_triangle(self, v);
    }
    fn draw_quad_pos(&mut self, pos: V3f, size: V2f, col: Col) {
        h5b_imm::draw_quad_pos(self, pos, size, col);
    }
    fn draw_quad_uv(&mut self, pos: V3f, size: V2f, min: V2f, max: V2f, col: Col) {
        h5b_imm::draw_quad_uv(self, pos, size, min, max, col);
    }
    /// `uv` = min0, max0, min1, max1.
    fn draw_quad_uv2(&mut self, pos: V3f, size: V2f, uv: [V2f; 4], col: Col) {
        h5b_imm::draw_quad_uv2(self, pos, size, uv, col);
    }
    fn draw_quad_vtx(&mut self, v: &[Vtx]) {
        h5b_imm::draw_quad_vtx(self, v);
    }
    fn draw_quad_vtx_col(&mut self, v: &[Vtx], col: Col) {
        h5b_imm::draw_quad_vtx_col(self, v, col);
    }
    fn draw_quad_vtx_z(&mut self, v: &[Vtx], z: f32) {
        h5b_imm::draw_quad_vtx_z(self, v, z);
    }
    fn draw_quad_vtx_z_col(&mut self, v: &[Vtx], z: f32, col: Col) {
        h5b_imm::draw_quad_vtx_z_col(self, v, z, col);
    }
    fn draw_quad_multitex(&mut self, v: &[Vtx], extra: &[V3f]) {
        h5b_imm::draw_quad_multitex(self, v, extra);
    }
    fn draw_line(&mut self, b: V3f, e: V3f, col: Col) {
        h5b_imm::draw_line(self, b, e, col);
    }
    fn draw_line_cols(&mut self, b: V3f, bc: Col, e: V3f, ec: Col) {
        h5b_imm::draw_line_cols(self, b, bc, e, ec);
    }
    fn draw_box_min_max(&mut self, min: V3f, max: V3f, col: Col) {
        h5b_imm::draw_box_min_max(self, min, max, col);
    }
    fn draw_sphere(&mut self, pos: V3f, radius: f32, col: Col) {
        h5b_imm::draw_sphere(self, pos, radius, col);
    }
    fn draw_sphere_axes(&mut self, pos: V3f, radius: f32, cols: [Col; 3]) {
        h5b_imm::draw_sphere_axes(self, pos, radius, cols);
    }
    fn draw_line_quad_rect(&mut self, r: Rect, z: f32, col: Col) {
        h5b_imm::draw_line_quad_rect(self, r, z, col);
    }
    fn draw_line_quad_pos(&mut self, pos: V3f, size: V2f, col: Col) {
        h5b_imm::draw_line_quad_pos(self, pos, size, col);
    }

    // ---- vertex batching
    fn add_vertex_to_batch(&mut self, v: &Vtx) {
        self.batch().add_vertex(v);
    }
    fn add_vertex_to_batch_translated(&mut self, v: &Vtx, t: V3f) {
        self.batch().add_vertex_translated(v, t);
    }
    /// The C++ body is empty.
    fn add_vertex_to_batch_matrix(&mut self, _v: &Vtx, _m: &Mat) {}
    fn add_vertex_to_batch_size2d(&mut self, v: &Vtx, t: V3f, col: Option<Col>, w: f32, h: f32) {
        self.batch().add_vertex_size2d(v, t, col, w, h);
    }
    fn add_vertex_to_batch_raw(&mut self, pos: V3f, col: Col, tex: V3f) {
        self.batch().add_vertex_raw(pos, col, tex);
    }
    fn add_index_to_batch(&mut self, i: i32) {
        self.batch().add_index(i);
    }
    fn add_texcoord_to_batch(&mut self, unit: u32, c: V3f) {
        self.batch().add_texcoord(unit, c);
    }
    fn set_batch_texture_unit_active(&mut self, unit: u32, active: bool);
    fn flush_tri_batch(&mut self, flags: u32, auto_clear: bool);
    fn flush_quad_batch(&mut self, flags: u32, auto_clear: bool);
    fn clear_batch(&mut self) {
        self.batch().clear();
    }
}
