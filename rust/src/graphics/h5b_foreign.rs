//! `ForeignGfx`: a `Gfx` over a C++ `iLowLevelGraphics` (the retained Vulkan backend, or any other
//! implementation) behind one call entry. Arguments travel as 64-bit slots; resources are the ids the
//! facade hands out. The facade is `HPL2/core/sources/graphics/RustGfxFacade.cpp`.

use core::ffi::c_void;

use super::h5b_gfx::*;
use super::h5b_imm::Batch;
use super::h5b_ops as op;

/// `ctx`, operation, argument slots, count, eight output slots; returns the primary result.
pub type CallFn = unsafe extern "C" fn(*mut c_void, u32, *const u64, usize, *mut u64) -> u64;

/// One bitmap level, as the facade reads it.
#[repr(C)]
pub struct FfiLevel {
    pub data: *const u8,
    pub len: usize,
}

#[repr(C)]
pub struct FfiBmp {
    pub size: V3l,
    pub format: i32,
    pub images: u32,
    pub mips: u32,
    pub levels: *const FfiLevel,
}

const NONE: u64 = u32::MAX as u64;

#[derive(Default)]
struct A(Vec<u64>);

impl A {
    fn i(mut self, x: i32) -> Self {
        self.0.push(x as i64 as u64);
        self
    }
    fn u(mut self, x: u32) -> Self {
        self.0.push(u64::from(x));
        self
    }
    fn f(mut self, x: f32) -> Self {
        self.0.push(u64::from(x.to_bits()));
        self
    }
    fn b(mut self, x: bool) -> Self {
        self.0.push(u64::from(x));
        self
    }
    fn id(mut self, x: Option<u32>) -> Self {
        self.0.push(x.map_or(NONE, u64::from));
        self
    }
    fn p<T>(mut self, x: *const T) -> Self {
        self.0.push(x as usize as u64);
        self
    }
    fn s(self, x: &str) -> Self {
        self.sl(x.as_bytes())
    }
    fn sl<T>(mut self, x: &[T]) -> Self {
        self.0.push(x.as_ptr() as usize as u64);
        self.0.push(x.len() as u64);
        self
    }
    fn v2(self, v: V2f) -> Self {
        self.f(v[0]).f(v[1])
    }
    fn v3(self, v: V3f) -> Self {
        self.f(v[0]).f(v[1]).f(v[2])
    }
    fn c4(self, v: Col) -> Self {
        self.f(v[0]).f(v[1]).f(v[2]).f(v[3])
    }
    fn v2i(self, v: V2l) -> Self {
        self.i(v[0]).i(v[1])
    }
    fn v3i(self, v: V3l) -> Self {
        self.i(v[0]).i(v[1]).i(v[2])
    }
}

pub struct ForeignGfx {
    ctx: *mut c_void,
    call: CallFn,
    batch: Batch,
}

impl ForeignGfx {
    /// # Safety
    /// `call(ctx, ..)` must be the facade entry for a live `iLowLevelGraphics`, valid while `self` lives.
    pub unsafe fn new(ctx: *mut c_void, call: CallFn) -> Self {
        Self {
            ctx,
            call,
            batch: Batch::new(),
        }
    }

    fn c8(&mut self, o: u32, a: &A) -> (u64, [u64; 8]) {
        let mut out = [0u64; 8];
        // SAFETY: the contract of `new`; the slots point at data alive for the call.
        let r = unsafe { (self.call)(self.ctx, o, a.0.as_ptr(), a.0.len(), out.as_mut_ptr()) };
        (r, out)
    }

    fn c(&mut self, o: u32, a: A) -> u64 {
        self.c8(o, &a).0
    }

    fn ci(&mut self, o: u32, a: A) -> i32 {
        self.c(o, a) as i32
    }

    fn cb(&mut self, o: u32, a: A) -> bool {
        self.c(o, a) & 1 != 0
    }

    fn cf(&mut self, o: u32, a: A) -> f32 {
        f32::from_bits(self.c(o, a) as u32)
    }

    fn text(&mut self, o: u32, a: A) -> String {
        let (p, out) = self.c8(o, &a);
        let len = out.first().copied().unwrap_or(0) as usize;
        if p == 0 {
            return String::new();
        }
        // SAFETY: the facade returns a pointer/length of a string it keeps until the next call.
        let bytes = unsafe { core::slice::from_raw_parts(p as usize as *const u8, len) };
        String::from_utf8_lossy(bytes).into_owned()
    }

    fn with_bmps<R>(
        &mut self,
        bs: &[Bmp],
        f: impl FnOnce(&mut Self, *const FfiBmp, usize) -> R,
    ) -> R {
        let levels: Vec<Vec<FfiLevel>> = bs
            .iter()
            .map(|b| {
                b.images
                    .iter()
                    .flat_map(|img| {
                        img.iter().map(|m| FfiLevel {
                            data: m.as_ptr(),
                            len: m.len(),
                        })
                    })
                    .collect()
            })
            .collect();
        let ffi: Vec<FfiBmp> = bs
            .iter()
            .zip(&levels)
            .map(|(b, l)| FfiBmp {
                size: b.size,
                format: b.format,
                images: b.images.len() as u32,
                mips: b.images.first().map_or(0, Vec::len) as u32,
                levels: l.as_ptr(),
            })
            .collect();
        f(self, ffi.as_ptr(), ffi.len())
    }

    fn data_args(a: A, d: Option<&[u8]>) -> A {
        match d {
            Some(d) => a.b(true).sl(d),
            None => a.b(false).p::<u8>(core::ptr::null()).u(0),
        }
    }
}

impl Imm for ForeignGfx {
    fn im_begin(&mut self, _: u32) {}
    fn im_end(&mut self) {}
    fn im_vertex3(&mut self, _: f32, _: f32, _: f32) {}
    fn im_color4(&mut self, _: f32, _: f32, _: f32, _: f32) {}
    fn im_texcoord2(&mut self, _: f32, _: f32) {}
    fn im_texcoord3(&mut self, _: f32, _: f32, _: f32) {}
    fn im_multitexcoord2(&mut self, _: u32, _: f32, _: f32) {}
    fn im_multitexcoord3(&mut self, _: u32, _: f32, _: f32, _: f32) {}
    fn im_clear_texture0(&mut self) {}
    fn im_blend_off(&mut self) {}
}

impl Gfx for ForeignGfx {
    fn init(&mut self, a: &InitArgs) -> bool {
        let args = A::default()
            .v2i(a.size)
            .i(a.display)
            .i(a.bpp)
            .i(a.fullscreen)
            .i(a.multisampling)
            .i(a.program_format)
            .s(&a.caption)
            .v2i(a.window_pos);
        self.cb(op::INIT, args)
    }

    fn gpu_program_format(&mut self) -> i32 {
        self.ci(op::GPU_PROGRAM_FORMAT, A::default())
    }

    fn caps(&mut self, kind: i32) -> i32 {
        self.ci(op::CAPS, A::default().i(kind))
    }

    fn renderer_description(&mut self) -> String {
        self.text(op::RENDERER_DESCRIPTION, A::default())
    }

    fn window_handle(&mut self) -> usize {
        self.c(op::WINDOW_HANDLE, A::default()) as usize
    }

    fn set_color_output(&mut self, mode: &str, pw: f32, pn: f32) -> bool {
        self.cb(op::SET_COLOR_OUTPUT, A::default().s(mode).f(pw).f(pn))
    }

    fn color_output(&mut self) -> String {
        self.text(op::COLOR_OUTPUT, A::default())
    }

    fn hdr_headroom(&mut self) -> f32 {
        self.cf(op::HDR_HEADROOM, A::default())
    }

    fn screen_float_stats(&mut self) -> Option<[f32; 3]> {
        let (r, o) = self.c8(op::SCREEN_FLOAT_STATS, &A::default());
        let g = |k: usize| f32::from_bits(o.get(k).copied().unwrap_or(0) as u32);
        (r & 1 != 0).then(|| [g(0), g(1), g(2)])
    }

    fn show_cursor(&mut self, on: bool) {
        self.c(op::SHOW_CURSOR, A::default().b(on));
    }

    fn set_window_grab(&mut self, on: bool) {
        self.c(op::SET_WINDOW_GRAB, A::default().b(on));
    }

    fn set_relative_mouse(&mut self, on: bool) {
        self.c(op::SET_RELATIVE_MOUSE, A::default().b(on));
    }

    fn set_window_caption(&mut self, name: &str) {
        self.c(op::SET_WINDOW_CAPTION, A::default().s(name));
    }

    fn window_mouse_focus(&mut self) -> bool {
        self.cb(op::WINDOW_MOUSE_FOCUS, A::default())
    }

    fn window_input_focus(&mut self) -> bool {
        self.cb(op::WINDOW_INPUT_FOCUS, A::default())
    }

    fn window_is_visible(&mut self) -> bool {
        self.cb(op::WINDOW_IS_VISIBLE, A::default())
    }

    fn multisampling(&mut self) -> i32 {
        self.ci(op::MULTISAMPLING, A::default())
    }

    fn screen_size_float(&mut self) -> V2f {
        let (_, o) = self.c8(op::SCREEN_SIZE_FLOAT, &A::default());
        let g = |k: usize| f32::from_bits(o.get(k).copied().unwrap_or(0) as u32);
        [g(0), g(1)]
    }

    fn screen_size_int(&mut self) -> V2l {
        let (_, o) = self.c8(op::SCREEN_SIZE_INT, &A::default());
        let g = |k: usize| o.get(k).copied().unwrap_or(0) as i32;
        [g(0), g(1)]
    }

    fn check_update_screen_size(&mut self) -> bool {
        self.cb(op::CHECK_UPDATE_SCREEN_SIZE, A::default())
    }

    fn force_window_size(&mut self, w: i32, h: i32) -> bool {
        self.cb(op::FORCE_WINDOW_SIZE, A::default().i(w).i(h))
    }

    fn fullscreen_active(&mut self) -> bool {
        self.cb(op::FULLSCREEN_ACTIVE, A::default())
    }

    fn set_vsync(&mut self, on: bool, adaptive: bool) {
        self.c(op::SET_VSYNC, A::default().b(on).b(adaptive));
    }

    fn set_multisampling_active(&mut self, on: bool) {
        self.c(op::SET_MULTISAMPLING_ACTIVE, A::default().b(on));
    }

    fn set_gamma(&mut self, g: f32) {
        self.c(op::SET_GAMMA, A::default().f(g));
    }

    fn gamma(&mut self) -> f32 {
        self.cf(op::GAMMA, A::default())
    }

    fn create_font(&mut self, name: &str) -> FontId {
        FontId(self.c(op::CREATE_FONT, A::default().s(name)) as u32)
    }

    fn create_program(&mut self, name: &str) -> ProgId {
        ProgId(self.c(op::CREATE_PROGRAM, A::default().s(name)) as u32)
    }

    fn create_shader(&mut self, name: &str, kind: i32) -> ShaderId {
        ShaderId(self.c(op::CREATE_SHADER, A::default().s(name).i(kind)) as u32)
    }

    fn create_texture(&mut self, name: &str, kind: i32, usage: i32) -> TexId {
        TexId(self.c(op::CREATE_TEXTURE, A::default().s(name).i(kind).i(usage)) as u32)
    }

    fn create_vertex_buffer(
        &mut self,
        kind: i32,
        draw: i32,
        usage: i32,
        rvtx: i32,
        ridx: i32,
    ) -> VbId {
        VbId(self.c(
            op::CREATE_VERTEX_BUFFER,
            A::default().i(kind).i(draw).i(usage).i(rvtx).i(ridx),
        ) as u32)
    }

    fn create_frame_buffer(&mut self, name: &str) -> FbId {
        FbId(self.c(op::CREATE_FRAME_BUFFER, A::default().s(name)) as u32)
    }

    fn create_depth_stencil(&mut self, size: V2l, d: i32, s: i32) -> DsbId {
        DsbId(self.c(op::CREATE_DEPTH_STENCIL, A::default().v2i(size).i(d).i(s)) as u32)
    }

    fn create_occlusion_query(&mut self) -> OqId {
        OqId(self.c(op::CREATE_OCCLUSION_QUERY, A::default()) as u32)
    }

    fn destroy_font(&mut self, id: FontId) {
        self.c(op::DESTROY_FONT, A::default().u(id.0));
    }

    fn destroy_program(&mut self, id: ProgId) {
        self.c(op::DESTROY_PROGRAM, A::default().u(id.0));
    }

    fn destroy_shader(&mut self, id: ShaderId) {
        self.c(op::DESTROY_SHADER, A::default().u(id.0));
    }

    fn destroy_texture(&mut self, id: TexId) {
        self.c(op::DESTROY_TEXTURE, A::default().u(id.0));
    }

    fn destroy_vertex_buffer(&mut self, id: VbId) {
        self.c(op::DESTROY_VERTEX_BUFFER, A::default().u(id.0));
    }

    fn destroy_frame_buffer(&mut self, id: FbId) {
        self.c(op::DESTROY_FRAME_BUFFER, A::default().u(id.0));
    }

    fn destroy_depth_stencil(&mut self, id: DsbId) {
        self.c(op::DESTROY_DEPTH_STENCIL, A::default().u(id.0));
    }

    fn destroy_occlusion_query(&mut self, id: OqId) {
        self.c(op::DESTROY_OCCLUSION_QUERY, A::default().u(id.0));
    }

    fn clear_frame_buffer(&mut self, flags: u32) {
        self.c(op::CLEAR_FRAME_BUFFER, A::default().u(flags));
    }

    fn set_clear_color(&mut self, c: Col) {
        self.c(op::SET_CLEAR_COLOR, A::default().c4(c));
    }

    fn set_clear_depth(&mut self, d: f32) {
        self.c(op::SET_CLEAR_DEPTH, A::default().f(d));
    }

    fn set_clear_stencil(&mut self, v: i32) {
        self.c(op::SET_CLEAR_STENCIL, A::default().i(v));
    }

    fn flush_rendering(&mut self) {
        self.c(op::FLUSH_RENDERING, A::default());
    }

    fn wait_and_finish_rendering(&mut self) {
        self.c(op::WAIT_AND_FINISH_RENDERING, A::default());
    }

    fn swap_buffers(&mut self) {
        self.c(op::SWAP_BUFFERS, A::default());
    }

    fn copy_frame_buffer_to_texture(&mut self, t: TexId, pos: V2l, size: V2l, off: V2l) {
        self.c(
            op::COPY_FRAME_BUFFER_TO_TEXTURE,
            A::default().u(t.0).v2i(pos).v2i(size).v2i(off),
        );
    }

    fn copy_frame_buffer_to_bitmap(&mut self, pos: V2l, size: V2l) -> Bmp {
        let (p, o) = self.c8(
            op::COPY_FRAME_BUFFER_TO_BITMAP,
            &A::default().v2i(pos).v2i(size),
        );
        let g = |k: usize| o.get(k).copied().unwrap_or(0);
        let len = g(4) as usize;
        let data = if p == 0 {
            Vec::new()
        } else {
            // SAFETY: the facade keeps `len` bytes at `p` until its next call.
            unsafe { core::slice::from_raw_parts(p as usize as *const u8, len) }.to_vec()
        };
        Bmp {
            size: [g(0) as i32, g(1) as i32, g(2) as i32],
            format: g(3) as i32,
            images: vec![vec![data]],
        }
    }

    fn set_current_frame_buffer(&mut self, fb: Option<FbId>, pos: V2l, size: V2l) {
        self.c(
            op::SET_CURRENT_FRAME_BUFFER,
            A::default().id(fb.map(|x| x.0)).v2i(pos).v2i(size),
        );
    }

    fn current_frame_buffer(&mut self) -> Option<FbId> {
        let r = self.c(op::CURRENT_FRAME_BUFFER, A::default());
        (r != NONE).then_some(FbId(r as u32))
    }

    fn set_frame_buffer_draw_targets(&mut self, targets: &[i32], count: i32) {
        self.c(
            op::SET_FRAME_BUFFER_DRAW_TARGETS,
            A::default().sl(targets).i(count),
        );
    }

    fn set_color_write_active(&mut self, r: bool, g: bool, b: bool, a: bool) {
        self.c(op::SET_COLOR_WRITE_ACTIVE, A::default().b(r).b(g).b(b).b(a));
    }

    fn set_depth_write_active(&mut self, on: bool) {
        self.c(op::SET_DEPTH_WRITE_ACTIVE, A::default().b(on));
    }

    fn set_cull_active(&mut self, on: bool) {
        self.c(op::SET_CULL_ACTIVE, A::default().b(on));
    }

    fn set_cull_mode(&mut self, mode: i32) {
        self.c(op::SET_CULL_MODE, A::default().i(mode));
    }

    fn set_depth_test_active(&mut self, on: bool) {
        self.c(op::SET_DEPTH_TEST_ACTIVE, A::default().b(on));
    }

    fn set_depth_test_func(&mut self, f: i32) {
        self.c(op::SET_DEPTH_TEST_FUNC, A::default().i(f));
    }

    fn set_alpha_test_active(&mut self, on: bool) {
        self.c(op::SET_ALPHA_TEST_ACTIVE, A::default().b(on));
    }

    fn set_alpha_test_func(&mut self, f: i32, r: f32) {
        self.c(op::SET_ALPHA_TEST_FUNC, A::default().i(f).f(r));
    }

    fn set_stencil_active(&mut self, on: bool) {
        self.c(op::SET_STENCIL_ACTIVE, A::default().b(on));
    }

    fn set_stencil_write_mask(&mut self, mask: u32) {
        self.c(op::SET_STENCIL_WRITE_MASK, A::default().u(mask));
    }

    fn set_stencil(&mut self, f: i32, r: i32, mask: u32, fail: i32, zfail: i32, zpass: i32) {
        self.c(
            op::SET_STENCIL,
            A::default().i(f).i(r).u(mask).i(fail).i(zfail).i(zpass),
        );
    }

    fn set_stencil_two_side(&mut self, front: i32, back: i32, r: i32, mask: u32, ops: [i32; 6]) {
        self.c(
            op::SET_STENCIL_TWO_SIDE,
            A::default().i(front).i(back).i(r).u(mask).sl(&ops),
        );
    }

    fn set_scissor_active(&mut self, on: bool) {
        self.c(op::SET_SCISSOR_ACTIVE, A::default().b(on));
    }

    fn set_scissor_rect(&mut self, pos: V2l, size: V2l) {
        self.c(op::SET_SCISSOR_RECT, A::default().v2i(pos).v2i(size));
    }

    fn set_clip_plane(&mut self, idx: i32, p: Plane) {
        self.c(op::SET_CLIP_PLANE, A::default().i(idx).c4(p));
    }

    fn clip_plane(&mut self, idx: i32) -> Plane {
        let (_, o) = self.c8(op::CLIP_PLANE, &A::default().i(idx));
        let g = |k: usize| f32::from_bits(o.get(k).copied().unwrap_or(0) as u32);
        [g(0), g(1), g(2), g(3)]
    }

    fn set_clip_plane_active(&mut self, idx: i32, on: bool) {
        self.c(op::SET_CLIP_PLANE_ACTIVE, A::default().i(idx).b(on));
    }

    fn set_color(&mut self, c: Col) {
        self.c(op::SET_COLOR, A::default().c4(c));
    }

    fn set_blend_active(&mut self, on: bool) {
        self.c(op::SET_BLEND_ACTIVE, A::default().b(on));
    }

    fn set_blend_func(&mut self, s: i32, d: i32) {
        self.c(op::SET_BLEND_FUNC, A::default().i(s).i(d));
    }

    fn set_blend_func_separate(&mut self, sc: i32, dc: i32, sa: i32, da: i32) {
        self.c(
            op::SET_BLEND_FUNC_SEPARATE,
            A::default().i(sc).i(dc).i(sa).i(da),
        );
    }

    fn set_polygon_offset_active(&mut self, on: bool) {
        self.c(op::SET_POLYGON_OFFSET_ACTIVE, A::default().b(on));
    }

    fn set_depth_clamp_active(&mut self, on: bool) {
        self.c(op::SET_DEPTH_CLAMP_ACTIVE, A::default().b(on));
    }

    fn set_polygon_offset(&mut self, bias: f32, slope: f32) {
        self.c(op::SET_POLYGON_OFFSET, A::default().f(bias).f(slope));
    }

    fn push_matrix(&mut self, kind: i32) {
        self.c(op::PUSH_MATRIX, A::default().i(kind));
    }

    fn pop_matrix(&mut self, kind: i32) {
        self.c(op::POP_MATRIX, A::default().i(kind));
    }

    fn set_identity_matrix(&mut self, kind: i32) {
        self.c(op::SET_IDENTITY_MATRIX, A::default().i(kind));
    }

    fn set_matrix(&mut self, kind: i32, m: &Mat) {
        self.c(op::SET_MATRIX, A::default().i(kind).p(m.as_ptr()));
    }

    fn set_ortho_size(&mut self, size: V2f, min: f32, max: f32) {
        self.c(op::SET_ORTHO_SIZE, A::default().v2(size).f(min).f(max));
    }

    fn set_ortho_box(&mut self, min: V3f, max: V3f) {
        self.c(op::SET_ORTHO_BOX, A::default().v3(min).v3(max));
    }

    fn set_texture(&mut self, unit: u32, t: Option<TexId>) {
        self.c(op::SET_TEXTURE, A::default().u(unit).id(t.map(|x| x.0)));
    }

    fn set_active_texture_unit(&mut self, unit: u32) {
        self.c(op::SET_ACTIVE_TEXTURE_UNIT, A::default().u(unit));
    }

    fn set_texture_env(&mut self, param: i32, val: i32) {
        self.c(op::SET_TEXTURE_ENV, A::default().i(param).i(val));
    }

    fn set_texture_constant_color(&mut self, c: Col) {
        self.c(op::SET_TEXTURE_CONSTANT_COLOR, A::default().c4(c));
    }

    fn tex_create_from_bitmap(&mut self, t: TexId, b: &Bmp) -> bool {
        self.with_bmps(core::slice::from_ref(b), |s, p, n| {
            s.cb(
                op::TEX_CREATE_FROM_BITMAP,
                A::default().u(t.0).p(p).u(n as u32),
            )
        })
    }

    fn tex_create_anim(&mut self, t: TexId, b: &[Bmp]) -> bool {
        self.with_bmps(b, |s, p, n| {
            s.cb(op::TEX_CREATE_ANIM, A::default().u(t.0).p(p).u(n as u32))
        })
    }

    fn tex_create_cube(&mut self, t: TexId, b: &[Bmp]) -> bool {
        self.with_bmps(b, |s, p, n| {
            s.cb(op::TEX_CREATE_CUBE, A::default().u(t.0).p(p).u(n as u32))
        })
    }

    fn tex_create_raw(&mut self, t: TexId, size: V3l, fmt: i32, data: Option<&[u8]>) -> bool {
        let a = Self::data_args(A::default().u(t.0).v3i(size).i(fmt), data);
        self.cb(op::TEX_CREATE_RAW, a)
    }

    fn tex_set_raw(
        &mut self,
        t: TexId,
        level: i32,
        off: V3l,
        size: V3l,
        fmt: i32,
        data: Option<&[u8]>,
    ) {
        let a = Self::data_args(A::default().u(t.0).i(level).v3i(off).v3i(size).i(fmt), data);
        self.c(op::TEX_SET_RAW, a);
    }

    fn tex_update(&mut self, t: TexId, dt: f32) {
        self.c(op::TEX_UPDATE, A::default().u(t.0).f(dt));
    }

    fn tex_set_filter(&mut self, t: TexId, f: i32) {
        self.c(op::TEX_SET_FILTER, A::default().u(t.0).i(f));
    }

    fn tex_set_anisotropy(&mut self, t: TexId, x: f32) {
        self.c(op::TEX_SET_ANISOTROPY, A::default().u(t.0).f(x));
    }

    fn tex_set_wrap_s(&mut self, t: TexId, m: i32) {
        self.c(op::TEX_SET_WRAP_S, A::default().u(t.0).i(m));
    }

    fn tex_set_wrap_t(&mut self, t: TexId, m: i32) {
        self.c(op::TEX_SET_WRAP_T, A::default().u(t.0).i(m));
    }

    fn tex_set_wrap_r(&mut self, t: TexId, m: i32) {
        self.c(op::TEX_SET_WRAP_R, A::default().u(t.0).i(m));
    }

    fn tex_set_wrap_str(&mut self, t: TexId, m: i32) {
        self.c(op::TEX_SET_WRAP_STR, A::default().u(t.0).i(m));
    }

    fn tex_set_compare_mode(&mut self, t: TexId, m: i32) {
        self.c(op::TEX_SET_COMPARE_MODE, A::default().u(t.0).i(m));
    }

    fn tex_set_compare_func(&mut self, t: TexId, f: i32) {
        self.c(op::TEX_SET_COMPARE_FUNC, A::default().u(t.0).i(f));
    }

    fn tex_set_red_from_alpha(&mut self, t: TexId, on: bool) {
        self.c(op::TEX_SET_RED_FROM_ALPHA, A::default().u(t.0).b(on));
    }

    fn tex_auto_mipmaps(&mut self, t: TexId) {
        self.c(op::TEX_AUTO_MIPMAPS, A::default().u(t.0));
    }

    fn tex_has_animation(&mut self, t: TexId) -> bool {
        self.cb(op::TEX_HAS_ANIMATION, A::default().u(t.0))
    }

    fn tex_next_frame(&mut self, t: TexId) {
        self.c(op::TEX_NEXT_FRAME, A::default().u(t.0));
    }

    fn tex_prev_frame(&mut self, t: TexId) {
        self.c(op::TEX_PREV_FRAME, A::default().u(t.0));
    }

    fn tex_time_count(&mut self, t: TexId) -> f32 {
        self.cf(op::TEX_TIME_COUNT, A::default().u(t.0))
    }

    fn tex_set_time_count(&mut self, t: TexId, x: f32) {
        self.c(op::TEX_SET_TIME_COUNT, A::default().u(t.0).f(x));
    }

    fn tex_handle(&mut self, t: TexId) -> i32 {
        self.ci(op::TEX_HANDLE, A::default().u(t.0))
    }

    fn tex_size(&mut self, t: TexId) -> V3l {
        let (_, o) = self.c8(op::TEX_SIZE, &A::default().u(t.0));
        let g = |k: usize| o.get(k).copied().unwrap_or(0) as i32;
        [g(0), g(1), g(2)]
    }

    fn tex_pixels_rgba_float(&mut self, t: TexId) -> Option<Vec<f32>> {
        let (p, o) = self.c8(op::TEX_PIXELS_RGBA_FLOAT, &A::default().u(t.0));
        if p == 0 {
            return None;
        }
        let n = o.first().copied().unwrap_or(0) as usize;
        // SAFETY: the facade keeps `n` floats at `p` until its next call.
        Some(unsafe { core::slice::from_raw_parts(p as usize as *const f32, n) }.to_vec())
    }

    fn vb_create_element_array(&mut self, v: VbId, kind: i32, format: i32, num: i32, var: i32) {
        self.c(
            op::VB_CREATE_ELEMENT_ARRAY,
            A::default().u(v.0).i(kind).i(format).i(num).i(var),
        );
    }

    fn vb_add_vec3(&mut self, v: VbId, element: i32, p: V3f) {
        self.c(op::VB_ADD_VEC3, A::default().u(v.0).i(element).v3(p));
    }

    fn vb_add_vec4(&mut self, v: VbId, element: i32, p: V3f, w: f32) {
        self.c(op::VB_ADD_VEC4, A::default().u(v.0).i(element).v3(p).f(w));
    }

    fn vb_add_color(&mut self, v: VbId, element: i32, c: Col) {
        self.c(op::VB_ADD_COLOR, A::default().u(v.0).i(element).c4(c));
    }

    fn vb_add_index(&mut self, v: VbId, i: u32) {
        self.c(op::VB_ADD_INDEX, A::default().u(v.0).u(i));
    }

    fn vb_compile(&mut self, v: VbId, flags: u32) -> bool {
        self.cb(op::VB_COMPILE, A::default().u(v.0).u(flags))
    }

    fn vb_update(&mut self, v: VbId, types: u32, indices: bool) {
        self.c(op::VB_UPDATE, A::default().u(v.0).u(types).b(indices));
    }

    fn vb_create_shadow_double(&mut self, v: VbId, update: bool) {
        self.c(op::VB_CREATE_SHADOW_DOUBLE, A::default().u(v.0).b(update));
    }

    fn vb_transform(&mut self, v: VbId, m: &Mat) {
        self.c(op::VB_TRANSFORM, A::default().u(v.0).p(m.as_ptr()));
    }

    fn vb_draw(&mut self, v: VbId, draw: i32) {
        self.c(op::VB_DRAW, A::default().u(v.0).i(draw));
    }

    fn vb_draw_indices(&mut self, v: VbId, idx: &[u32], count: i32, draw: i32) {
        self.c(
            op::VB_DRAW_INDICES,
            A::default().u(v.0).sl(idx).i(count).i(draw),
        );
    }

    fn vb_bind(&mut self, v: VbId) {
        self.c(op::VB_BIND, A::default().u(v.0));
    }

    fn vb_unbind(&mut self, v: VbId) {
        self.c(op::VB_UNBIND, A::default().u(v.0));
    }

    fn vb_create_copy(&mut self, v: VbId, kind: i32, usage: i32, flags: u32) -> VbId {
        VbId(self.c(
            op::VB_CREATE_COPY,
            A::default().u(v.0).i(kind).i(usage).u(flags),
        ) as u32)
    }

    fn vb_bounding_volume(&mut self, v: VbId) -> (V3f, V3f) {
        let (_, o) = self.c8(op::VB_BOUNDING_VOLUME, &A::default().u(v.0));
        let g = |k: usize| f32::from_bits(o.get(k).copied().unwrap_or(0) as u32);
        ([g(0), g(1), g(2)], [g(3), g(4), g(5)])
    }

    fn vb_element_num(&mut self, v: VbId, e: i32) -> i32 {
        self.ci(op::VB_ELEMENT_NUM, A::default().u(v.0).i(e))
    }

    fn vb_element_format(&mut self, v: VbId, e: i32) -> i32 {
        self.ci(op::VB_ELEMENT_FORMAT, A::default().u(v.0).i(e))
    }

    fn vb_element_var(&mut self, v: VbId, e: i32) -> i32 {
        self.ci(op::VB_ELEMENT_VAR, A::default().u(v.0).i(e))
    }

    fn vb_vertex_num(&mut self, v: VbId) -> i32 {
        self.ci(op::VB_VERTEX_NUM, A::default().u(v.0))
    }

    fn vb_index_num(&mut self, v: VbId) -> i32 {
        self.ci(op::VB_INDEX_NUM, A::default().u(v.0))
    }

    fn vb_resize_array(&mut self, v: VbId, e: i32, size: i32) {
        self.c(op::VB_RESIZE_ARRAY, A::default().u(v.0).i(e).i(size));
    }

    fn vb_resize_indices(&mut self, v: VbId, size: i32) {
        self.c(op::VB_RESIZE_INDICES, A::default().u(v.0).i(size));
    }

    fn shader_from_file(&mut self, sh: ShaderId, file: &str, entry: &str, print: bool) -> bool {
        self.cb(
            op::SHADER_FROM_FILE,
            A::default().u(sh.0).s(file).s(entry).b(print),
        )
    }

    fn shader_from_string(&mut self, sh: ShaderId, src: &str, entry: &str, print: bool) -> bool {
        self.cb(
            op::SHADER_FROM_STRING,
            A::default().u(sh.0).s(src).s(entry).b(print),
        )
    }

    fn prog_set_shader(&mut self, p: ProgId, kind: i32, s: Option<ShaderId>) {
        self.c(
            op::PROG_SET_SHADER,
            A::default().u(p.0).i(kind).id(s.map(|x| x.0)),
        );
    }

    fn prog_link(&mut self, p: ProgId) -> bool {
        self.cb(op::PROG_LINK, A::default().u(p.0))
    }

    fn prog_dump_uniforms(&mut self, p: ProgId) -> String {
        self.text(op::PROG_DUMP_UNIFORMS, A::default().u(p.0))
    }

    fn prog_bind(&mut self, p: ProgId) {
        self.c(op::PROG_BIND, A::default().u(p.0));
    }

    fn prog_unbind(&mut self, p: ProgId) {
        self.c(op::PROG_UNBIND, A::default().u(p.0));
    }

    fn prog_set_sampler_to_unit(&mut self, p: ProgId, name: &str, unit: i32) -> bool {
        self.cb(
            op::PROG_SET_SAMPLER_TO_UNIT,
            A::default().u(p.0).s(name).i(unit),
        )
    }

    fn prog_variable_id(&mut self, p: ProgId, name: &str) -> i32 {
        self.ci(op::PROG_VARIABLE_ID, A::default().u(p.0).s(name))
    }

    fn prog_variable_as_id(&mut self, p: ProgId, name: &str, id: i32) -> bool {
        self.cb(op::PROG_VARIABLE_AS_ID, A::default().u(p.0).s(name).i(id))
    }

    fn prog_set_int(&mut self, p: ProgId, var: i32, x: i32) -> bool {
        self.cb(op::PROG_SET_INT, A::default().u(p.0).i(var).i(x))
    }

    fn prog_set_float(&mut self, p: ProgId, var: i32, x: f32) -> bool {
        self.cb(op::PROG_SET_FLOAT, A::default().u(p.0).i(var).f(x))
    }

    fn prog_set_vec2(&mut self, p: ProgId, var: i32, v: V2f) -> bool {
        self.cb(op::PROG_SET_VEC2, A::default().u(p.0).i(var).v2(v))
    }

    fn prog_set_vec3(&mut self, p: ProgId, var: i32, v: V3f) -> bool {
        self.cb(op::PROG_SET_VEC3, A::default().u(p.0).i(var).v3(v))
    }

    fn prog_set_vec4(&mut self, p: ProgId, var: i32, v: Col) -> bool {
        self.cb(op::PROG_SET_VEC4, A::default().u(p.0).i(var).c4(v))
    }

    fn prog_set_float_array(&mut self, p: ProgId, var: i32, x: &[f32]) -> bool {
        self.cb(op::PROG_SET_FLOAT_ARRAY, A::default().u(p.0).i(var).sl(x))
    }

    fn prog_set_matrix(&mut self, p: ProgId, var: i32, m: &Mat) -> bool {
        self.cb(
            op::PROG_SET_MATRIX,
            A::default().u(p.0).i(var).p(m.as_ptr()),
        )
    }

    fn prog_set_matrix_api(&mut self, p: ProgId, var: i32, kind: i32, o: i32) -> bool {
        self.cb(
            op::PROG_SET_MATRIX_API,
            A::default().u(p.0).i(var).i(kind).i(o),
        )
    }

    fn fb_set_texture_2d(&mut self, f: FbId, idx: i32, t: Option<TexId>, mip: i32) {
        self.c(
            op::FB_SET_TEXTURE_2D,
            A::default().u(f.0).i(idx).id(t.map(|x| x.0)).i(mip),
        );
    }

    fn fb_set_texture_3d(&mut self, f: FbId, idx: i32, t: Option<TexId>, z: i32, mip: i32) {
        self.c(
            op::FB_SET_TEXTURE_3D,
            A::default().u(f.0).i(idx).id(t.map(|x| x.0)).i(z).i(mip),
        );
    }

    fn fb_set_texture_cube(&mut self, f: FbId, idx: i32, t: Option<TexId>, face: i32, mip: i32) {
        self.c(
            op::FB_SET_TEXTURE_CUBE,
            A::default().u(f.0).i(idx).id(t.map(|x| x.0)).i(face).i(mip),
        );
    }

    fn fb_set_depth_texture_2d(&mut self, f: FbId, t: Option<TexId>, mip: i32) {
        self.c(
            op::FB_SET_DEPTH_TEXTURE_2D,
            A::default().u(f.0).id(t.map(|x| x.0)).i(mip),
        );
    }

    fn fb_set_depth_texture_cube(&mut self, f: FbId, t: Option<TexId>, face: i32, mip: i32) {
        self.c(
            op::FB_SET_DEPTH_TEXTURE_CUBE,
            A::default().u(f.0).id(t.map(|x| x.0)).i(face).i(mip),
        );
    }

    fn fb_set_depth_stencil(&mut self, f: FbId, d: Option<DsbId>) {
        self.c(
            op::FB_SET_DEPTH_STENCIL,
            A::default().u(f.0).id(d.map(|x| x.0)),
        );
    }

    fn fb_compile_and_validate(&mut self, f: FbId) -> bool {
        self.cb(op::FB_COMPILE_AND_VALIDATE, A::default().u(f.0))
    }

    fn fb_post_bind_update(&mut self, f: FbId) {
        self.c(op::FB_POST_BIND_UPDATE, A::default().u(f.0));
    }

    fn oq_begin(&mut self, q: OqId) {
        self.c(op::OQ_BEGIN, A::default().u(q.0));
    }

    fn oq_end(&mut self, q: OqId) {
        self.c(op::OQ_END, A::default().u(q.0));
    }

    fn oq_fetch_results(&mut self, q: OqId) -> bool {
        self.cb(op::OQ_FETCH_RESULTS, A::default().u(q.0))
    }

    fn oq_sample_count(&mut self, q: OqId) -> u32 {
        self.c(op::OQ_SAMPLE_COUNT, A::default().u(q.0)) as u32
    }

    fn font_from_font_file(
        &mut self,
        f: FontId,
        file: &str,
        size: i32,
        first: u16,
        last: u16,
    ) -> bool {
        self.cb(
            op::FONT_FROM_FONT_FILE,
            A::default()
                .u(f.0)
                .s(file)
                .i(size)
                .u(u32::from(first))
                .u(u32::from(last)),
        )
    }

    fn font_from_bitmap_file(&mut self, f: FontId, file: &str) -> bool {
        self.cb(op::FONT_FROM_BITMAP_FILE, A::default().u(f.0).s(file))
    }
}

impl Draw for ForeignGfx {
    fn batch(&mut self) -> &mut Batch {
        &mut self.batch
    }

    fn draw_triangle(&mut self, v: &[Vtx]) {
        self.c(op::DRAW_TRIANGLE, A::default().sl(v));
    }

    fn draw_quad_pos(&mut self, pos: V3f, size: V2f, col: Col) {
        self.c(op::DRAW_QUAD_POS, A::default().v3(pos).v2(size).c4(col));
    }

    fn draw_quad_uv(&mut self, pos: V3f, size: V2f, min: V2f, max: V2f, col: Col) {
        self.c(
            op::DRAW_QUAD_UV,
            A::default().v3(pos).v2(size).v2(min).v2(max).c4(col),
        );
    }

    fn draw_quad_uv2(&mut self, pos: V3f, size: V2f, uv: [V2f; 4], col: Col) {
        let a = uv
            .iter()
            .fold(A::default().v3(pos).v2(size), |a, u| a.v2(*u));
        self.c(op::DRAW_QUAD_UV2, a.c4(col));
    }

    fn draw_quad_vtx(&mut self, v: &[Vtx]) {
        self.c(op::DRAW_QUAD_VTX, A::default().sl(v));
    }

    fn draw_quad_vtx_col(&mut self, v: &[Vtx], col: Col) {
        self.c(op::DRAW_QUAD_VTX_COL, A::default().sl(v).c4(col));
    }

    fn draw_quad_vtx_z(&mut self, v: &[Vtx], z: f32) {
        self.c(op::DRAW_QUAD_VTX_Z, A::default().sl(v).f(z));
    }

    fn draw_quad_vtx_z_col(&mut self, v: &[Vtx], z: f32, col: Col) {
        self.c(op::DRAW_QUAD_VTX_Z_COL, A::default().sl(v).f(z).c4(col));
    }

    fn draw_quad_multitex(&mut self, v: &[Vtx], extra: &[V3f]) {
        self.c(op::DRAW_QUAD_MULTITEX, A::default().sl(v).sl(extra));
    }

    fn draw_line(&mut self, b: V3f, e: V3f, col: Col) {
        self.c(op::DRAW_LINE, A::default().v3(b).v3(e).c4(col));
    }

    fn draw_line_cols(&mut self, b: V3f, bc: Col, e: V3f, ec: Col) {
        self.c(op::DRAW_LINE_COLS, A::default().v3(b).c4(bc).v3(e).c4(ec));
    }

    fn draw_box_min_max(&mut self, min: V3f, max: V3f, col: Col) {
        self.c(op::DRAW_BOX_MIN_MAX, A::default().v3(min).v3(max).c4(col));
    }

    fn draw_sphere(&mut self, pos: V3f, radius: f32, col: Col) {
        self.c(op::DRAW_SPHERE, A::default().v3(pos).f(radius).c4(col));
    }

    fn draw_sphere_axes(&mut self, pos: V3f, radius: f32, cols: [Col; 3]) {
        self.c(
            op::DRAW_SPHERE_AXES,
            A::default()
                .v3(pos)
                .f(radius)
                .c4(cols[0])
                .c4(cols[1])
                .c4(cols[2]),
        );
    }

    fn draw_line_quad_rect(&mut self, r: Rect, z: f32, col: Col) {
        self.c(op::DRAW_LINE_QUAD_RECT, A::default().c4(r).f(z).c4(col));
    }

    fn draw_line_quad_pos(&mut self, pos: V3f, size: V2f, col: Col) {
        self.c(
            op::DRAW_LINE_QUAD_POS,
            A::default().v3(pos).v2(size).c4(col),
        );
    }

    fn add_vertex_to_batch(&mut self, v: &Vtx) {
        self.c(op::ADD_VERTEX_TO_BATCH, A::default().p(v));
    }

    fn add_vertex_to_batch_translated(&mut self, v: &Vtx, t: V3f) {
        self.c(op::ADD_VERTEX_TO_BATCH_TRANSLATED, A::default().p(v).v3(t));
    }

    fn add_vertex_to_batch_matrix(&mut self, v: &Vtx, m: &Mat) {
        self.c(
            op::ADD_VERTEX_TO_BATCH_MATRIX,
            A::default().p(v).p(m.as_ptr()),
        );
    }

    fn add_vertex_to_batch_size2d(&mut self, v: &Vtx, t: V3f, col: Option<Col>, w: f32, h: f32) {
        let c = col.unwrap_or_default();
        self.c(
            op::ADD_VERTEX_TO_BATCH_SIZE2D,
            A::default().p(v).v3(t).b(col.is_some()).c4(c).f(w).f(h),
        );
    }

    fn add_vertex_to_batch_raw(&mut self, pos: V3f, col: Col, tex: V3f) {
        self.c(
            op::ADD_VERTEX_TO_BATCH_RAW,
            A::default().v3(pos).c4(col).v3(tex),
        );
    }

    fn add_index_to_batch(&mut self, i: i32) {
        self.c(op::ADD_INDEX_TO_BATCH, A::default().i(i));
    }

    fn add_texcoord_to_batch(&mut self, unit: u32, c: V3f) {
        self.c(op::ADD_TEXCOORD_TO_BATCH, A::default().u(unit).v3(c));
    }

    fn set_batch_texture_unit_active(&mut self, unit: u32, active: bool) {
        self.c(
            op::SET_BATCH_TEXTURE_UNIT_ACTIVE,
            A::default().u(unit).b(active),
        );
    }

    fn flush_tri_batch(&mut self, flags: u32, auto_clear: bool) {
        self.c(op::FLUSH_TRI_BATCH, A::default().u(flags).b(auto_clear));
    }

    fn flush_quad_batch(&mut self, flags: u32, auto_clear: bool) {
        self.c(op::FLUSH_QUAD_BATCH, A::default().u(flags).b(auto_clear));
    }

    fn clear_batch(&mut self) {
        self.c(op::CLEAR_BATCH, A::default());
    }
}
