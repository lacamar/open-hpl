//! `TraceGfx`: a `Gfx` that does no GPU work and writes the text trace of
//! `cRecordingLowLevelGraphics` (HPL2/tests/trace), byte for byte.

use std::collections::BTreeMap;
use std::fmt::Write as _;

use super::h5b_gfx::*;
use super::h5b_imm::Batch;
use crate::graphics::formats;

/// `cRecordingConfig`.
#[derive(Clone, Debug)]
pub struct TraceCfg {
    pub seed: u64,
    pub screen: V2l,
    pub occlusion_table: i32,
    pub framebuffer_compile_ok: bool,
    pub caps: BTreeMap<i32, i32>,
    pub default_cap: i32,
}

impl Default for TraceCfg {
    fn default() -> Self {
        let caps = [(3, 16), (4, 8), (5, 6), (7, 16), (13, 5), (21, 5)]
            .into_iter()
            .collect();
        Self {
            seed: 1,
            screen: [1280, 720],
            occlusion_table: 64,
            framebuffer_compile_ok: true,
            caps,
            default_cap: 1,
        }
    }
}

fn splitmix(state: &mut u64) -> u64 {
    *state = state.wrapping_add(0x9e37_79b9_7f4a_7c15);
    let mut z = *state;
    z = (z ^ (z >> 30)).wrapping_mul(0xbf58_476d_1ce4_e5b9);
    z = (z ^ (z >> 27)).wrapping_mul(0x94d0_49bb_1331_11eb);
    z ^ (z >> 31)
}

pub fn fnv(data: &[u8]) -> u64 {
    data.iter().fold(1_469_598_103_934_665_603u64, |h, &b| {
        (h ^ u64::from(b)).wrapping_mul(1_099_511_628_211)
    })
}

pub fn hash_tag(tag: &str, data: &[u8]) -> String {
    format!("{tag}{{n={},h={:016x}}}", data.len(), fnv(data))
}

fn f32_bytes(v: &[f32]) -> Vec<u8> {
    v.iter().flat_map(|x| x.to_ne_bytes()).collect()
}

fn u32_bytes(v: &[u32]) -> Vec<u8> {
    v.iter().flat_map(|x| x.to_ne_bytes()).collect()
}

fn i32_bytes(v: &[i32]) -> Vec<u8> {
    v.iter().flat_map(|x| x.to_ne_bytes()).collect()
}

/// `cTraceWriter::Put` for the argument types the recorder uses.
pub mod put {
    pub fn f(x: f32) -> String {
        format!("f:{:08x}", x.to_bits())
    }
    pub fn i(x: i32) -> String {
        x.to_string()
    }
    pub fn u(x: u32) -> String {
        format!("{x}u")
    }
    pub fn b(x: bool) -> String {
        x.to_string()
    }
    pub fn s(x: &str) -> String {
        format!("\"{x}\"")
    }
    pub fn w(x: &str) -> String {
        let mut o = String::from("w\"");
        for c in x.chars() {
            let c = c as u32;
            if (32..127).contains(&c) {
                o.push(char::from_u32(c).unwrap_or('?'));
            } else {
                o.push_str(&format!("\\u{c}"));
            }
        }
        o.push('"');
        o
    }
    pub fn cstr(x: &str) -> String {
        super::hash_tag("cstr", x.as_bytes())
    }
    fn list(v: &[f32]) -> String {
        v.iter().map(|&x| f(x)).collect::<Vec<_>>().join(",")
    }
    pub fn v2(v: [f32; 2]) -> String {
        format!("v2{{{}}}", list(&v))
    }
    pub fn v2i(v: [i32; 2]) -> String {
        format!("v2i{{{},{}}}", v[0], v[1])
    }
    pub fn v3(v: [f32; 3]) -> String {
        format!("v3{{{}}}", list(&v))
    }
    pub fn v3i(v: [i32; 3]) -> String {
        format!("v3i{{{},{},{}}}", v[0], v[1], v[2])
    }
    pub fn c(v: [f32; 4]) -> String {
        format!("c{{{}}}", list(&v))
    }
    pub fn pl(v: [f32; 4]) -> String {
        format!("pl{{{}}}", list(&v))
    }
    pub fn r(v: [f32; 4]) -> String {
        format!("r{{{}}}", list(&v))
    }
    pub fn m(v: &[f32; 16]) -> String {
        format!("m{{{}}}", list(v))
    }
    pub fn vtx(v: &super::Vtx) -> String {
        format!(
            "vtx{{{},{},{},{},{}}}",
            v3(v.pos),
            v3(v.tex),
            v3(v.tan),
            v3(v.norm),
            c(v.col)
        )
    }
    pub fn vtxvec(v: &[super::Vtx]) -> String {
        let body: Vec<_> = v.iter().map(vtx).collect();
        format!("vtxvec[{}]{{{}}}", v.len(), body.join(","))
    }
    pub fn v3vec(v: &[[f32; 3]]) -> String {
        let body: Vec<_> = v.iter().map(|&x| v3(x)).collect();
        format!("v3vec[{}]{{{}}}", v.len(), body.join(","))
    }
    pub fn ints(v: &[i32]) -> String {
        let body: Vec<_> = v.iter().map(|x| x.to_string()).collect();
        format!("ints[{}]{{{}}}", v.len(), body.join(","))
    }
}

use put::{b, c, f, i, s, u, v2i, v3i, w};

#[derive(Default)]
struct Arr {
    format: i32,
    num: i32,
    var: i32,
    f: Vec<f32>,
    i: Vec<i32>,
    b: Vec<u8>,
}

impl Arr {
    fn size(&self) -> usize {
        let n = match self.format {
            0 => self.f.len(),
            1 => self.i.len(),
            _ => self.b.len(),
        };
        usize::try_from(self.num)
            .ok()
            .filter(|&d| d > 0)
            .map_or(0, |d| n.checked_div(d).unwrap_or(0))
    }
}

#[derive(Default)]
struct Vb {
    arrays: BTreeMap<i32, Arr>,
    indices: Vec<u32>,
    draw: i32,
    rvtx: i32,
    ridx: i32,
}

#[derive(Default)]
struct Tex {
    size: V3l,
    frames: i32,
    time: f32,
}

#[derive(Default)]
struct Prog {
    shaders: [Option<ShaderId>; 2],
    vars: BTreeMap<String, i32>,
}

pub struct TraceGfx {
    pub cfg: TraceCfg,
    out: String,
    counters: BTreeMap<&'static str, u32>,
    occlusion: Vec<u32>,
    fetches: i32,
    oq_last: BTreeMap<u32, u32>,
    screen: V2l,
    current_fb: Option<FbId>,
    clip: [Plane; 8],
    gamma: f32,
    ms: i32,
    fullscreen: bool,
    format: i32,
    color_output: String,
    texs: BTreeMap<u32, Tex>,
    vbs: BTreeMap<u32, Vb>,
    progs: BTreeMap<u32, Prog>,
    fb_size: BTreeMap<u32, V2l>,
    batch: Batch,
}

impl Default for TraceGfx {
    fn default() -> Self {
        Self::new(TraceCfg::default())
    }
}

impl TraceGfx {
    pub fn new(cfg: TraceCfg) -> Self {
        let mut state = cfg.seed;
        let n = if cfg.occlusion_table > 0 {
            cfg.occlusion_table
        } else {
            1
        };
        let occlusion = (0..n)
            .map(|_| {
                let r = splitmix(&mut state);
                if r & 3 == 0 {
                    0
                } else {
                    (r >> 8).checked_rem(1000).unwrap_or(0) as u32 + 1
                }
            })
            .collect();
        Self {
            screen: cfg.screen,
            cfg,
            out: String::new(),
            counters: BTreeMap::new(),
            occlusion,
            fetches: 0,
            oq_last: BTreeMap::new(),
            current_fb: None,
            clip: [[0.0; 4]; 8],
            gamma: 1.0,
            ms: 0,
            fullscreen: false,
            format: 2,
            color_output: "sdr".into(),
            texs: BTreeMap::new(),
            vbs: BTreeMap::new(),
            progs: BTreeMap::new(),
            fb_size: BTreeMap::new(),
            batch: Batch::new(),
        }
    }

    pub fn text(&self) -> &str {
        &self.out
    }

    fn label(kind: &str, id: u32) -> String {
        format!("{kind}#{id}")
    }

    fn opt(kind: &str, id: Option<u32>) -> String {
        id.map_or_else(|| "null".into(), |n| Self::label(kind, n))
    }

    fn next(&mut self, kind: &'static str) -> u32 {
        let n = self.counters.entry(kind).or_insert(0);
        let id = *n;
        *n += 1;
        id
    }

    fn call(&mut self, name: &str, args: &[String]) {
        let _ = writeln!(self.out, "{name}({})", args.join(", "));
    }

    fn callr(&mut self, name: &str, ret: &str, args: &[String]) {
        let _ = writeln!(self.out, "{name}({}) -> {ret}", args.join(", "));
    }

    fn bmp_text(bm: &Bmp) -> String {
        let mips = bm.images.first().map_or(0, Vec::len);
        let mut t = format!(
            "{}{}{}{}",
            put::v3i(bm.size),
            i(bm.format),
            i(bm.images.len() as i32),
            i(mips as i32)
        );
        for img in &bm.images {
            for m in img {
                t.push_str(&hash_tag("img", m));
            }
        }
        t
    }

    fn raw_text(data: Option<&[u8]>) -> String {
        data.map_or_else(|| hash_tag("cstr", b"null"), |d| hash_tag("raw", d))
    }
}

impl Imm for TraceGfx {
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

macro_rules! rec {
    ($self:ident, $name:literal $(, $a:expr)*) => { $self.call($name, &[$($a),*]) };
}

macro_rules! tex_rec {
    ($self:ident, $t:expr, $name:literal $(, $a:expr)*) => {
        $self.call(&format!("tex#{}.{}", $t.0, $name), &[$($a),*])
    };
}

macro_rules! vb_rec {
    ($self:ident, $v:expr, $name:literal $(, $a:expr)*) => {
        $self.call(&format!("vb#{}.{}", $v.0, $name), &[$($a),*])
    };
}

macro_rules! prog_rec {
    ($self:ident, $p:expr, $name:literal $(, $a:expr)*) => {
        $self.call(&format!("prog#{}.{}", $p.0, $name), &[$($a),*])
    };
}

impl Gfx for TraceGfx {
    fn init(&mut self, a: &InitArgs) -> bool {
        rec!(
            self,
            "Init",
            i(a.size[0]),
            i(a.size[1]),
            i(a.display),
            i(a.bpp),
            i(a.fullscreen),
            i(a.multisampling),
            i(a.program_format),
            s(&a.caption),
            v2i(a.window_pos)
        );
        self.screen = a.size;
        self.ms = a.multisampling;
        self.fullscreen = a.fullscreen != 0;
        self.format = a.program_format;
        true
    }

    fn gpu_program_format(&mut self) -> i32 {
        let v = self.format;
        self.callr("GetGpuProgramFormat", &i(v), &[]);
        v
    }

    fn caps(&mut self, kind: i32) -> i32 {
        let v = self
            .cfg
            .caps
            .get(&kind)
            .copied()
            .unwrap_or(self.cfg.default_cap);
        self.callr("GetCaps", &i(v), &[i(kind)]);
        v
    }

    fn renderer_description(&mut self) -> String {
        self.callr("GetRendererDescription", &s("recording"), &[]);
        "recording".into()
    }

    fn window_handle(&mut self) -> usize {
        rec!(self, "GetWindowHandle");
        0
    }

    fn set_color_output(&mut self, mode: &str, pw: f32, pn: f32) -> bool {
        self.callr("SetColorOutput", "true", &[s(mode), f(pw), f(pn)]);
        self.color_output = mode.into();
        true
    }

    fn color_output(&mut self) -> String {
        let v = self.color_output.clone();
        self.callr("GetColorOutput", &s(&v), &[]);
        v
    }

    fn hdr_headroom(&mut self) -> f32 {
        self.callr("GetHdrHeadroom", &f(1.0), &[]);
        1.0
    }

    fn screen_float_stats(&mut self) -> Option<[f32; 3]> {
        self.callr("GetScreenFloatStats", "true", &[f(1.0), f(0.5), f(0.0)]);
        Some([1.0, 0.5, 0.0])
    }

    fn show_cursor(&mut self, on: bool) {
        rec!(self, "ShowCursor", b(on));
    }

    fn set_window_grab(&mut self, on: bool) {
        rec!(self, "SetWindowGrab", b(on));
    }

    fn set_relative_mouse(&mut self, on: bool) {
        rec!(self, "SetRelativeMouse", b(on));
    }

    fn set_window_caption(&mut self, name: &str) {
        rec!(self, "SetWindowCaption", s(name));
    }

    fn window_mouse_focus(&mut self) -> bool {
        self.callr("GetWindowMouseFocus", "true", &[]);
        true
    }

    fn window_input_focus(&mut self) -> bool {
        self.callr("GetWindowInputFocus", "true", &[]);
        true
    }

    fn window_is_visible(&mut self) -> bool {
        self.callr("GetWindowIsVisible", "true", &[]);
        true
    }

    fn multisampling(&mut self) -> i32 {
        let v = self.ms;
        self.callr("GetMultisampling", &i(v), &[]);
        v
    }

    fn screen_size_float(&mut self) -> V2f {
        let v = [self.screen[0] as f32, self.screen[1] as f32];
        self.callr("GetScreenSizeFloat", &put::v2(v), &[]);
        v
    }

    fn screen_size_int(&mut self) -> V2l {
        let v = self.screen;
        self.callr("GetScreenSizeInt", &v2i(v), &[]);
        v
    }

    fn check_update_screen_size(&mut self) -> bool {
        self.callr("CheckAndUpdateScreenSize", "false", &[]);
        false
    }

    fn force_window_size(&mut self, w_: i32, h: i32) -> bool {
        rec!(self, "ForceWindowSize", i(w_), i(h));
        self.screen = [w_, h];
        true
    }

    fn fullscreen_active(&mut self) -> bool {
        let v = self.fullscreen;
        self.callr("GetFullscreenModeActive", &b(v), &[]);
        v
    }

    fn set_vsync(&mut self, on: bool, adaptive: bool) {
        rec!(self, "SetVsyncActive", b(on), b(adaptive));
    }

    fn set_multisampling_active(&mut self, on: bool) {
        rec!(self, "SetMultisamplingActive", b(on));
    }

    fn set_gamma(&mut self, g: f32) {
        rec!(self, "SetGammaCorrection", f(g));
        self.gamma = g;
    }

    fn gamma(&mut self) -> f32 {
        let v = self.gamma;
        self.callr("GetGammaCorrection", &f(v), &[]);
        v
    }

    fn create_font(&mut self, name: &str) -> FontId {
        let n = self.next("font");
        self.callr("CreateFontData", &Self::label("font", n), &[s(name)]);
        FontId(n)
    }

    fn create_program(&mut self, name: &str) -> ProgId {
        let n = self.next("prog");
        self.progs.insert(n, Prog::default());
        self.callr("CreateGpuProgram", &Self::label("prog", n), &[s(name)]);
        ProgId(n)
    }

    fn create_shader(&mut self, name: &str, kind: i32) -> ShaderId {
        // The recorder's shader constructor asks the device for its program format first.
        self.gpu_program_format();
        let n = self.next("shader");
        self.callr(
            "CreateGpuShader",
            &Self::label("shader", n),
            &[s(name), i(kind)],
        );
        ShaderId(n)
    }

    fn create_texture(&mut self, name: &str, kind: i32, usage: i32) -> TexId {
        let n = self.next("tex");
        self.texs.insert(
            n,
            Tex {
                frames: 1,
                ..Tex::default()
            },
        );
        self.callr(
            "CreateTexture",
            &Self::label("tex", n),
            &[s(name), i(kind), i(usage)],
        );
        TexId(n)
    }

    fn create_vertex_buffer(
        &mut self,
        kind: i32,
        draw: i32,
        usage: i32,
        rvtx: i32,
        ridx: i32,
    ) -> VbId {
        let n = self.next("vb");
        self.vbs.insert(
            n,
            Vb {
                draw,
                rvtx,
                ridx,
                ..Vb::default()
            },
        );
        self.callr(
            "CreateVertexBuffer",
            &Self::label("vb", n),
            &[i(kind), i(draw), i(usage), i(rvtx), i(ridx)],
        );
        VbId(n)
    }

    fn create_frame_buffer(&mut self, name: &str) -> FbId {
        let n = self.next("fb");
        self.callr("CreateFrameBuffer", &Self::label("fb", n), &[s(name)]);
        FbId(n)
    }

    fn create_depth_stencil(&mut self, size: V2l, d: i32, st: i32) -> DsbId {
        let n = self.next("dsb");
        self.callr(
            "CreateDepthStencilBuffer",
            &Self::label("dsb", n),
            &[v2i(size), i(d), i(st)],
        );
        DsbId(n)
    }

    fn create_occlusion_query(&mut self) -> OqId {
        let n = self.next("oq");
        self.callr("CreateOcclusionQuery", &Self::label("oq", n), &[]);
        OqId(n)
    }

    fn destroy_font(&mut self, id: FontId) {
        self.call(&format!("font#{}.~", id.0), &[]);
    }

    fn destroy_program(&mut self, id: ProgId) {
        self.call(&format!("prog#{}.~", id.0), &[]);
        self.progs.remove(&id.0);
    }

    fn destroy_shader(&mut self, id: ShaderId) {
        self.call(&format!("shader#{}.~", id.0), &[]);
    }

    fn destroy_texture(&mut self, id: TexId) {
        self.call(&format!("tex#{}.~", id.0), &[]);
        self.texs.remove(&id.0);
    }

    fn destroy_vertex_buffer(&mut self, id: VbId) {
        self.call(&format!("vb#{}.~", id.0), &[]);
        self.vbs.remove(&id.0);
    }

    fn destroy_frame_buffer(&mut self, id: FbId) {
        self.call(&format!("fb#{}.~", id.0), &[]);
        self.fb_size.remove(&id.0);
    }

    fn destroy_depth_stencil(&mut self, id: DsbId) {
        self.call(&format!("dsb#{}.~", id.0), &[]);
    }

    fn destroy_occlusion_query(&mut self, id: OqId) {
        self.call(&format!("oq#{}.~", id.0), &[]);
    }

    fn clear_frame_buffer(&mut self, flags: u32) {
        rec!(self, "ClearFrameBuffer", u(flags));
    }

    fn set_clear_color(&mut self, col: Col) {
        rec!(self, "SetClearColor", c(col));
    }

    fn set_clear_depth(&mut self, d: f32) {
        rec!(self, "SetClearDepth", f(d));
    }

    fn set_clear_stencil(&mut self, v: i32) {
        rec!(self, "SetClearStencil", i(v));
    }

    fn flush_rendering(&mut self) {
        rec!(self, "FlushRendering");
    }

    fn wait_and_finish_rendering(&mut self) {
        rec!(self, "WaitAndFinishRendering");
    }

    fn swap_buffers(&mut self) {
        rec!(self, "SwapBuffers");
    }

    fn copy_frame_buffer_to_texture(&mut self, t: TexId, pos: V2l, size: V2l, off: V2l) {
        rec!(
            self,
            "CopyFrameBufferToTexure",
            Self::label("tex", t.0),
            v2i(pos),
            v2i(size),
            v2i(off)
        );
    }

    fn copy_frame_buffer_to_bitmap(&mut self, pos: V2l, size: V2l) -> Bmp {
        rec!(self, "CopyFrameBufferToBitmap", v2i(pos), v2i(size));
        let sz = if size[0] < 0 { self.screen } else { size };
        let px = (sz[0].max(0) as usize) * (sz[1].max(0) as usize);
        let data = [0u8, 0, 0, 255].repeat(px);
        Bmp {
            size: [sz[0], sz[1], 1],
            format: formats::RGBA,
            images: vec![vec![data]],
        }
    }

    fn set_current_frame_buffer(&mut self, fb: Option<FbId>, pos: V2l, size: V2l) {
        rec!(
            self,
            "SetCurrentFrameBuffer",
            Self::opt("fb", fb.map(|x| x.0)),
            v2i(pos),
            v2i(size)
        );
        self.current_fb = fb;
    }

    fn current_frame_buffer(&mut self) -> Option<FbId> {
        let v = self.current_fb;
        self.callr(
            "GetCurrentFrameBuffer",
            &Self::opt("fb", v.map(|x| x.0)),
            &[],
        );
        v
    }

    fn set_frame_buffer_draw_targets(&mut self, targets: &[i32], count: i32) {
        let n = usize::try_from(count).unwrap_or(0);
        rec!(
            self,
            "SetFrameBufferDrawTargets",
            put::ints(targets.get(..n).unwrap_or(targets)),
            i(count)
        );
    }

    fn set_color_write_active(&mut self, r: bool, g: bool, bl: bool, a: bool) {
        rec!(self, "SetColorWriteActive", b(r), b(g), b(bl), b(a));
    }

    fn set_depth_write_active(&mut self, on: bool) {
        rec!(self, "SetDepthWriteActive", b(on));
    }

    fn set_cull_active(&mut self, on: bool) {
        rec!(self, "SetCullActive", b(on));
    }

    fn set_cull_mode(&mut self, mode: i32) {
        rec!(self, "SetCullMode", i(mode));
    }

    fn set_depth_test_active(&mut self, on: bool) {
        rec!(self, "SetDepthTestActive", b(on));
    }

    fn set_depth_test_func(&mut self, fun: i32) {
        rec!(self, "SetDepthTestFunc", i(fun));
    }

    fn set_alpha_test_active(&mut self, on: bool) {
        rec!(self, "SetAlphaTestActive", b(on));
    }

    fn set_alpha_test_func(&mut self, fun: i32, r: f32) {
        rec!(self, "SetAlphaTestFunc", i(fun), f(r));
    }

    fn set_stencil_active(&mut self, on: bool) {
        rec!(self, "SetStencilActive", b(on));
    }

    fn set_stencil_write_mask(&mut self, mask: u32) {
        rec!(self, "SetStencilWriteMask", u(mask));
    }

    fn set_stencil(&mut self, fun: i32, r: i32, mask: u32, fail: i32, zfail: i32, zpass: i32) {
        rec!(
            self,
            "SetStencil",
            i(fun),
            i(r),
            u(mask),
            i(fail),
            i(zfail),
            i(zpass)
        );
    }

    fn set_stencil_two_side(&mut self, front: i32, back: i32, r: i32, mask: u32, ops: [i32; 6]) {
        let mut a = vec![i(front), i(back), i(r), u(mask)];
        a.extend(ops.iter().map(|&o| i(o)));
        self.call("SetStencilTwoSide", &a);
    }

    fn set_scissor_active(&mut self, on: bool) {
        rec!(self, "SetScissorActive", b(on));
    }

    fn set_scissor_rect(&mut self, pos: V2l, size: V2l) {
        rec!(self, "SetScissorRect", v2i(pos), v2i(size));
    }

    fn set_clip_plane(&mut self, idx: i32, plane: Plane) {
        rec!(self, "SetClipPlane", i(idx), put::pl(plane));
        if let Some(p) = usize::try_from(idx).ok().and_then(|n| self.clip.get_mut(n)) {
            *p = plane;
        }
    }

    fn clip_plane(&mut self, idx: i32) -> Plane {
        let p = usize::try_from(idx)
            .ok()
            .and_then(|n| self.clip.get(n))
            .copied()
            .unwrap_or([0.0; 4]);
        self.callr("GetClipPlane", &put::pl(p), &[i(idx)]);
        p
    }

    fn set_clip_plane_active(&mut self, idx: i32, on: bool) {
        rec!(self, "SetClipPlaneActive", i(idx), b(on));
    }

    fn set_color(&mut self, col: Col) {
        rec!(self, "SetColor", c(col));
    }

    fn set_blend_active(&mut self, on: bool) {
        rec!(self, "SetBlendActive", b(on));
    }

    fn set_blend_func(&mut self, src: i32, dst: i32) {
        rec!(self, "SetBlendFunc", i(src), i(dst));
    }

    fn set_blend_func_separate(&mut self, sc: i32, dc: i32, sa: i32, da: i32) {
        rec!(self, "SetBlendFuncSeparate", i(sc), i(dc), i(sa), i(da));
    }

    fn set_polygon_offset_active(&mut self, on: bool) {
        rec!(self, "SetPolygonOffsetActive", b(on));
    }

    fn set_depth_clamp_active(&mut self, on: bool) {
        rec!(self, "SetDepthClampActive", b(on));
    }

    fn set_polygon_offset(&mut self, bias: f32, slope: f32) {
        rec!(self, "SetPolygonOffset", f(bias), f(slope));
    }

    fn push_matrix(&mut self, kind: i32) {
        rec!(self, "PushMatrix", i(kind));
    }

    fn pop_matrix(&mut self, kind: i32) {
        rec!(self, "PopMatrix", i(kind));
    }

    fn set_identity_matrix(&mut self, kind: i32) {
        rec!(self, "SetIdentityMatrix", i(kind));
    }

    fn set_matrix(&mut self, kind: i32, m: &Mat) {
        rec!(self, "SetMatrix", i(kind), put::m(m));
    }

    fn set_ortho_size(&mut self, size: V2f, min: f32, max: f32) {
        rec!(self, "SetOrthoProjection", put::v2(size), f(min), f(max));
    }

    fn set_ortho_box(&mut self, min: V3f, max: V3f) {
        rec!(self, "SetOrthoProjection", put::v3(min), put::v3(max));
    }

    fn set_texture(&mut self, unit: u32, t: Option<TexId>) {
        rec!(
            self,
            "SetTexture",
            u(unit),
            Self::opt("tex", t.map(|x| x.0))
        );
    }

    fn set_active_texture_unit(&mut self, unit: u32) {
        rec!(self, "SetActiveTextureUnit", u(unit));
    }

    fn set_texture_env(&mut self, param: i32, val: i32) {
        rec!(self, "SetTextureEnv", i(param), i(val));
    }

    fn set_texture_constant_color(&mut self, col: Col) {
        rec!(self, "SetTextureConstantColor", c(col));
    }

    fn tex_create_from_bitmap(&mut self, t: TexId, bm: &Bmp) -> bool {
        tex_rec!(self, t, "CreateFromBitmap", s(&Self::bmp_text(bm)));
        if let Some(x) = self.texs.get_mut(&t.0) {
            x.size = bm.size;
        }
        true
    }

    fn tex_create_anim(&mut self, t: TexId, bms: &[Bmp]) -> bool {
        tex_rec!(self, t, "CreateAnimFromBitmapVec", i(bms.len() as i32));
        for bm in bms {
            tex_rec!(self, t, "  frame", s(&Self::bmp_text(bm)));
        }
        if let (Some(x), Some(first)) = (self.texs.get_mut(&t.0), bms.first()) {
            x.size = first.size;
        }
        if let Some(x) = self.texs.get_mut(&t.0) {
            x.frames = bms.len() as i32;
        }
        true
    }

    fn tex_create_cube(&mut self, t: TexId, bms: &[Bmp]) -> bool {
        tex_rec!(self, t, "CreateCubeFromBitmapVec", i(bms.len() as i32));
        for bm in bms {
            tex_rec!(self, t, "  face", s(&Self::bmp_text(bm)));
        }
        if let (Some(x), Some(first)) = (self.texs.get_mut(&t.0), bms.first()) {
            x.size = first.size;
        }
        true
    }

    fn tex_create_raw(&mut self, t: TexId, size: V3l, fmt: i32, data: Option<&[u8]>) -> bool {
        tex_rec!(
            self,
            t,
            "CreateFromRawData",
            v3i(size),
            i(fmt),
            s(&Self::raw_text(data))
        );
        if let Some(x) = self.texs.get_mut(&t.0) {
            x.size = size;
        }
        true
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
        tex_rec!(
            self,
            t,
            "SetRawData",
            i(level),
            v3i(off),
            v3i(size),
            i(fmt),
            s(&Self::raw_text(data))
        );
    }

    fn tex_update(&mut self, t: TexId, dt: f32) {
        tex_rec!(self, t, "Update", f(dt));
    }

    fn tex_set_filter(&mut self, t: TexId, fl: i32) {
        tex_rec!(self, t, "SetFilter", i(fl));
    }

    fn tex_set_anisotropy(&mut self, t: TexId, x: f32) {
        tex_rec!(self, t, "SetAnisotropyDegree", f(x));
    }

    fn tex_set_wrap_s(&mut self, t: TexId, m: i32) {
        tex_rec!(self, t, "SetWrapS", i(m));
    }

    fn tex_set_wrap_t(&mut self, t: TexId, m: i32) {
        tex_rec!(self, t, "SetWrapT", i(m));
    }

    fn tex_set_wrap_r(&mut self, t: TexId, m: i32) {
        tex_rec!(self, t, "SetWrapR", i(m));
    }

    fn tex_set_wrap_str(&mut self, t: TexId, m: i32) {
        tex_rec!(self, t, "SetWrapSTR", i(m));
    }

    fn tex_set_compare_mode(&mut self, t: TexId, m: i32) {
        tex_rec!(self, t, "SetCompareMode", i(m));
    }

    fn tex_set_compare_func(&mut self, t: TexId, fun: i32) {
        tex_rec!(self, t, "SetCompareFunc", i(fun));
    }

    fn tex_set_red_from_alpha(&mut self, t: TexId, on: bool) {
        tex_rec!(self, t, "SetRedFromAlpha", b(on));
    }

    fn tex_auto_mipmaps(&mut self, t: TexId) {
        tex_rec!(self, t, "AutoGenerateMipmaps");
    }

    fn tex_has_animation(&mut self, t: TexId) -> bool {
        self.texs.get(&t.0).is_some_and(|x| x.frames > 1)
    }

    fn tex_next_frame(&mut self, t: TexId) {
        tex_rec!(self, t, "NextFrame");
    }

    fn tex_prev_frame(&mut self, t: TexId) {
        tex_rec!(self, t, "PrevFrame");
    }

    fn tex_time_count(&mut self, t: TexId) -> f32 {
        self.texs.get(&t.0).map_or(0.0, |x| x.time)
    }

    fn tex_set_time_count(&mut self, t: TexId, x: f32) {
        tex_rec!(self, t, "SetTimeCount", f(x));
        if let Some(e) = self.texs.get_mut(&t.0) {
            e.time = x;
        }
    }

    fn tex_handle(&mut self, t: TexId) -> i32 {
        t.0 as i32 + 1
    }

    fn tex_size(&mut self, t: TexId) -> V3l {
        self.texs.get(&t.0).map_or([0; 3], |x| x.size)
    }

    fn tex_pixels_rgba_float(&mut self, t: TexId) -> Option<Vec<f32>> {
        tex_rec!(self, t, "GetRawPixelsRGBAFloat");
        let sz = self.tex_size(t);
        let n = (sz[0].max(0) as usize) * (sz[1].max(0) as usize) * 4;
        Some(vec![0.0; n])
    }

    fn vb_create_element_array(&mut self, v: VbId, kind: i32, format: i32, num: i32, var: i32) {
        vb_rec!(
            self,
            v,
            "CreateElementArray",
            i(kind),
            i(format),
            i(num),
            i(var)
        );
        if let Some(vb) = self.vbs.get_mut(&v.0) {
            let a = vb.arrays.entry(kind).or_default();
            a.format = format;
            a.num = num;
            a.var = var;
        }
    }

    fn vb_add_vec3(&mut self, v: VbId, element: i32, p: V3f) {
        vb_rec!(self, v, "AddVertexVec3f", i(element), put::v3(p));
        if let Some(a) = self
            .vbs
            .get_mut(&v.0)
            .map(|vb| vb.arrays.entry(element).or_default())
        {
            a.f.extend_from_slice(&p);
            for _ in 3..a.num {
                a.f.push(0.0);
            }
        }
    }

    fn vb_add_vec4(&mut self, v: VbId, element: i32, p: V3f, w_: f32) {
        vb_rec!(self, v, "AddVertexVec4f", i(element), put::v3(p), f(w_));
        if let Some(a) = self
            .vbs
            .get_mut(&v.0)
            .map(|vb| vb.arrays.entry(element).or_default())
        {
            a.f.extend_from_slice(&p);
            a.f.push(w_);
        }
    }

    fn vb_add_color(&mut self, v: VbId, element: i32, col: Col) {
        vb_rec!(self, v, "AddVertexColor", i(element), c(col));
        if let Some(a) = self
            .vbs
            .get_mut(&v.0)
            .map(|vb| vb.arrays.entry(element).or_default())
        {
            for x in col {
                if a.format == 2 {
                    a.b.push(x.mul_add(255.0, 0.5) as u8);
                } else {
                    a.f.push(x);
                }
            }
        }
    }

    fn vb_add_index(&mut self, v: VbId, idx: u32) {
        vb_rec!(self, v, "AddIndex", u(idx));
        if let Some(vb) = self.vbs.get_mut(&v.0) {
            vb.indices.push(idx);
        }
    }

    fn vb_compile(&mut self, v: VbId, flags: u32) -> bool {
        let mut t = String::new();
        if let Some(vb) = self.vbs.get(&v.0) {
            for (k, a) in &vb.arrays {
                t.push_str(&k.to_string());
                t.push_str(&hash_tag("f", &f32_bytes(&a.f)));
                t.push_str(&hash_tag("i", &i32_bytes(&a.i)));
                t.push_str(&hash_tag("b", &a.b));
            }
            t.push_str(&hash_tag("idx", &u32_bytes(&vb.indices)));
        }
        vb_rec!(self, v, "Compile", u(flags), s(&t));
        true
    }

    fn vb_update(&mut self, v: VbId, types: u32, indices: bool) {
        let mut t = String::new();
        if let Some(vb) = self.vbs.get(&v.0) {
            for (k, a) in &vb.arrays {
                if formats::vertex_element_flag(*k) & types != 0 {
                    t.push_str(&k.to_string());
                    t.push_str(&hash_tag("f", &f32_bytes(&a.f)));
                    t.push_str(&hash_tag("b", &a.b));
                }
            }
            if indices {
                t.push_str(&hash_tag("idx", &u32_bytes(&vb.indices)));
            }
        }
        vb_rec!(self, v, "UpdateData", u(types), b(indices), s(&t));
    }

    fn vb_create_shadow_double(&mut self, v: VbId, update: bool) {
        vb_rec!(self, v, "CreateShadowDouble", b(update));
    }

    fn vb_transform(&mut self, v: VbId, m: &Mat) {
        vb_rec!(self, v, "Transform", put::m(m));
        let Some(a) = self.vbs.get_mut(&v.0).and_then(|vb| vb.arrays.get_mut(&1)) else {
            return;
        };
        if a.num < 3 {
            return;
        }
        let step = a.num as usize;
        let row = |r: usize, p: [f32; 3]| {
            let o = r * 4;
            let at = |k: usize| m.get(o + k).copied().unwrap_or(0.0);
            at(2).mul_add(p[2], at(0).mul_add(p[0], at(1) * p[1])) + at(3)
        };
        for ch in a.f.chunks_exact_mut(step) {
            if let [x, y, z, ..] = ch {
                let p = [*x, *y, *z];
                *x = row(0, p);
                *y = row(1, p);
                *z = row(2, p);
            }
        }
    }

    fn vb_draw(&mut self, v: VbId, draw: i32) {
        vb_rec!(self, v, "Draw", i(draw));
    }

    fn vb_draw_indices(&mut self, v: VbId, idx: &[u32], count: i32, draw: i32) {
        let n = usize::try_from(count).unwrap_or(0).min(idx.len());
        let t = hash_tag("idx", &u32_bytes(idx.get(..n).unwrap_or(&[])));
        vb_rec!(self, v, "DrawIndices", s(&t), i(count), i(draw));
    }

    fn vb_bind(&mut self, v: VbId) {
        vb_rec!(self, v, "Bind");
    }

    fn vb_unbind(&mut self, v: VbId) {
        vb_rec!(self, v, "UnBind");
    }

    fn vb_create_copy(&mut self, v: VbId, kind: i32, usage: i32, flags: u32) -> VbId {
        let n = self.next("vb");
        let mut copy = Vb::default();
        if let Some(src) = self.vbs.get(&v.0) {
            copy.draw = src.draw;
            copy.rvtx = src.rvtx;
            copy.ridx = src.ridx;
            for (k, a) in &src.arrays {
                if formats::vertex_element_flag(*k) & flags != 0 {
                    copy.arrays.insert(
                        *k,
                        Arr {
                            format: a.format,
                            num: a.num,
                            var: a.var,
                            f: a.f.clone(),
                            i: a.i.clone(),
                            b: a.b.clone(),
                        },
                    );
                }
            }
            copy.indices = src.indices.clone();
        }
        self.vbs.insert(n, copy);
        vb_rec!(
            self,
            v,
            "CreateCopy",
            i(kind),
            i(usage),
            u(flags),
            Self::label("vb", n)
        );
        VbId(n)
    }

    fn vb_bounding_volume(&mut self, v: VbId) -> (V3f, V3f) {
        vb_rec!(self, v, "CreateBoundingVolume");
        let Some(a) = self.vbs.get(&v.0).and_then(|vb| vb.arrays.get(&1)) else {
            return ([0.0; 3], [0.0; 3]);
        };
        if a.num < 3 || a.f.is_empty() {
            return ([0.0; 3], [0.0; 3]);
        }
        let step = a.num as usize;
        let (mut mn, mut mx) = ([f32::MAX; 3], [f32::MIN; 3]);
        let mut first = true;
        for ch in a.f.chunks_exact(step) {
            if let [x, y, z, ..] = ch {
                let p = [*x, *y, *z];
                if first {
                    (mn, mx, first) = (p, p, false);
                }
                mn = [mn[0].min(p[0]), mn[1].min(p[1]), mn[2].min(p[2])];
                mx = [mx[0].max(p[0]), mx[1].max(p[1]), mx[2].max(p[2])];
            }
        }
        (mn, mx)
    }

    fn vb_element_num(&mut self, v: VbId, element: i32) -> i32 {
        self.vbs
            .get(&v.0)
            .and_then(|vb| vb.arrays.get(&element))
            .map_or(0, |a| a.num)
    }

    fn vb_element_format(&mut self, v: VbId, element: i32) -> i32 {
        self.vbs
            .get(&v.0)
            .and_then(|vb| vb.arrays.get(&element))
            .map_or(0, |a| a.format)
    }

    fn vb_element_var(&mut self, v: VbId, element: i32) -> i32 {
        self.vbs
            .get(&v.0)
            .and_then(|vb| vb.arrays.get(&element))
            .map_or(0, |a| a.var)
    }

    fn vb_vertex_num(&mut self, v: VbId) -> i32 {
        let Some(vb) = self.vbs.get(&v.0) else {
            return 0;
        };
        let a = vb.arrays.get(&1).or_else(|| vb.arrays.values().next());
        a.map_or(0, |a| a.size() as i32)
    }

    fn vb_index_num(&mut self, v: VbId) -> i32 {
        self.vbs.get(&v.0).map_or(0, |vb| vb.indices.len() as i32)
    }

    fn vb_resize_array(&mut self, v: VbId, element: i32, size: i32) {
        vb_rec!(self, v, "ResizeArray", i(element), i(size));
        if let Some(a) = self
            .vbs
            .get_mut(&v.0)
            .map(|vb| vb.arrays.entry(element).or_default())
        {
            let n = usize::try_from(size).unwrap_or(0);
            a.f.resize(if a.format == 0 { n } else { 0 }, 0.0);
            a.i.resize(if a.format == 1 { n } else { 0 }, 0);
            a.b.resize(if a.format == 2 { n } else { 0 }, 0);
        }
    }

    fn vb_resize_indices(&mut self, v: VbId, size: i32) {
        vb_rec!(self, v, "ResizeIndices", i(size));
        if let Some(vb) = self.vbs.get_mut(&v.0) {
            vb.indices.resize(usize::try_from(size).unwrap_or(0), 0);
        }
    }

    fn shader_from_file(&mut self, sh: ShaderId, file: &str, entry: &str, print: bool) -> bool {
        self.callr(
            &format!("shader#{}.CreateFromFile", sh.0),
            "true",
            &[w(file), s(entry), b(print)],
        );
        true
    }

    fn shader_from_string(&mut self, sh: ShaderId, src: &str, entry: &str, print: bool) -> bool {
        self.callr(
            &format!("shader#{}.CreateFromString", sh.0),
            "true",
            &[put::cstr(src), s(entry), b(print)],
        );
        true
    }

    fn prog_set_shader(&mut self, p: ProgId, kind: i32, sh: Option<ShaderId>) {
        if let Some(slot) = self
            .progs
            .get_mut(&p.0)
            .and_then(|x| x.shaders.get_mut(usize::try_from(kind).unwrap_or(2)))
        {
            *slot = sh;
        }
    }

    fn prog_link(&mut self, p: ProgId) -> bool {
        let sh = self.progs.get(&p.0).map(|x| x.shaders).unwrap_or_default();
        prog_rec!(
            self,
            p,
            "Link",
            Self::opt("shader", sh[0].map(|x| x.0)),
            Self::opt("shader", sh[1].map(|x| x.0))
        );
        true
    }

    fn prog_dump_uniforms(&mut self, p: ProgId) -> String {
        prog_rec!(self, p, "DumpUniforms");
        String::new()
    }

    fn prog_bind(&mut self, p: ProgId) {
        prog_rec!(self, p, "Bind");
    }

    fn prog_unbind(&mut self, p: ProgId) {
        prog_rec!(self, p, "UnBind");
    }

    fn prog_set_sampler_to_unit(&mut self, p: ProgId, name: &str, unit: i32) -> bool {
        prog_rec!(self, p, "SetSamplerToUnit", s(name), i(unit));
        true
    }

    fn prog_variable_id(&mut self, p: ProgId, name: &str) -> i32 {
        let vars = self.progs.entry(p.0).or_default();
        let next = vars.vars.len() as i32;
        let id = *vars.vars.entry(name.to_string()).or_insert(next);
        self.callr(&format!("prog#{}.GetVariableId", p.0), &i(id), &[s(name)]);
        id
    }

    fn prog_variable_as_id(&mut self, p: ProgId, name: &str, id: i32) -> bool {
        prog_rec!(self, p, "GetVariableAsId", s(name), i(id));
        self.progs
            .entry(p.0)
            .or_default()
            .vars
            .insert(name.to_string(), id);
        true
    }

    fn prog_set_int(&mut self, p: ProgId, var: i32, x: i32) -> bool {
        prog_rec!(self, p, "SetInt", i(var), i(x));
        true
    }

    fn prog_set_float(&mut self, p: ProgId, var: i32, x: f32) -> bool {
        prog_rec!(self, p, "SetFloat", i(var), f(x));
        true
    }

    fn prog_set_vec2(&mut self, p: ProgId, var: i32, x: V2f) -> bool {
        prog_rec!(self, p, "SetVec2f", i(var), f(x[0]), f(x[1]));
        true
    }

    fn prog_set_vec3(&mut self, p: ProgId, var: i32, x: V3f) -> bool {
        prog_rec!(self, p, "SetVec3f", i(var), f(x[0]), f(x[1]), f(x[2]));
        true
    }

    fn prog_set_vec4(&mut self, p: ProgId, var: i32, x: Col) -> bool {
        prog_rec!(
            self,
            p,
            "SetVec4f",
            i(var),
            f(x[0]),
            f(x[1]),
            f(x[2]),
            f(x[3])
        );
        true
    }

    fn prog_set_float_array(&mut self, p: ProgId, var: i32, x: &[f32]) -> bool {
        let t: String = x.iter().map(|&v| f(v)).collect();
        prog_rec!(self, p, "SetFloatArray", i(var), i(x.len() as i32), s(&t));
        true
    }

    fn prog_set_matrix(&mut self, p: ProgId, var: i32, m: &Mat) -> bool {
        prog_rec!(self, p, "SetMatrixf", i(var), put::m(m));
        true
    }

    fn prog_set_matrix_api(&mut self, p: ProgId, var: i32, kind: i32, op: i32) -> bool {
        prog_rec!(self, p, "SetMatrixfApi", i(var), i(kind), i(op));
        true
    }

    fn fb_set_texture_2d(&mut self, fb: FbId, idx: i32, t: Option<TexId>, mip: i32) {
        self.fb_rec(
            fb,
            "SetTexture2D",
            vec![i(idx), Self::opt("tex", t.map(|x| x.0)), i(mip)],
        );
        self.fb_adopt_size(fb, t);
    }

    fn fb_set_texture_3d(&mut self, fb: FbId, idx: i32, t: Option<TexId>, z: i32, mip: i32) {
        self.fb_rec(
            fb,
            "SetTexture3D",
            vec![i(idx), Self::opt("tex", t.map(|x| x.0)), i(z), i(mip)],
        );
        self.fb_adopt_size(fb, t);
    }

    fn fb_set_texture_cube(&mut self, fb: FbId, idx: i32, t: Option<TexId>, face: i32, mip: i32) {
        self.fb_rec(
            fb,
            "SetTextureCubeMap",
            vec![i(idx), Self::opt("tex", t.map(|x| x.0)), i(face), i(mip)],
        );
        self.fb_adopt_size(fb, t);
    }

    fn fb_set_depth_texture_2d(&mut self, fb: FbId, t: Option<TexId>, mip: i32) {
        self.fb_rec(
            fb,
            "SetDepthTexture2D",
            vec![Self::opt("tex", t.map(|x| x.0)), i(mip)],
        );
    }

    fn fb_set_depth_texture_cube(&mut self, fb: FbId, t: Option<TexId>, face: i32, mip: i32) {
        self.fb_rec(
            fb,
            "SetDepthTextureCubeMap",
            vec![Self::opt("tex", t.map(|x| x.0)), i(face), i(mip)],
        );
    }

    fn fb_set_depth_stencil(&mut self, fb: FbId, d: Option<DsbId>) {
        self.fb_rec(
            fb,
            "SetDepthStencilBuffer",
            vec![Self::opt("dsb", d.map(|x| x.0))],
        );
    }

    fn fb_compile_and_validate(&mut self, fb: FbId) -> bool {
        let ok = self.cfg.framebuffer_compile_ok;
        self.callr(&format!("fb#{}.CompileAndValidate", fb.0), &b(ok), &[]);
        ok
    }

    fn fb_post_bind_update(&mut self, fb: FbId) {
        self.fb_rec(fb, "PostBindUpdate", vec![]);
    }

    fn oq_begin(&mut self, q: OqId) {
        self.call(&format!("oq#{}.Begin", q.0), &[]);
    }

    fn oq_end(&mut self, q: OqId) {
        self.call(&format!("oq#{}.End", q.0), &[]);
    }

    fn oq_fetch_results(&mut self, q: OqId) -> bool {
        let n = self.occlusion.len();
        let sample = self
            .occlusion
            .get(
                usize::try_from(self.fetches)
                    .unwrap_or(0)
                    .checked_rem(n)
                    .unwrap_or(0),
            )
            .copied()
            .unwrap_or(0);
        self.fetches += 1;
        self.oq_last.insert(q.0, sample);
        self.callr(&format!("oq#{}.FetchResults", q.0), "true", &[u(sample)]);
        true
    }

    fn oq_sample_count(&mut self, q: OqId) -> u32 {
        let v = self.oq_last.get(&q.0).copied().unwrap_or(0);
        self.callr(&format!("oq#{}.GetSampleCount", q.0), &u(v), &[]);
        v
    }

    fn font_from_font_file(
        &mut self,
        fo: FontId,
        file: &str,
        size: i32,
        first: u16,
        last: u16,
    ) -> bool {
        self.callr(
            &format!("font#{}.CreateFromFontFile", fo.0),
            "true",
            &[w(file), i(size), i(i32::from(first)), i(i32::from(last))],
        );
        true
    }

    fn font_from_bitmap_file(&mut self, fo: FontId, file: &str) -> bool {
        self.callr(
            &format!("font#{}.CreateFromBitmapFile", fo.0),
            "true",
            &[w(file)],
        );
        true
    }
}

impl Draw for TraceGfx {
    fn batch(&mut self) -> &mut Batch {
        &mut self.batch
    }

    fn add_vertex_to_batch(&mut self, v: &Vtx) {
        rec!(self, "AddVertexToBatch", put::vtx(v));
    }

    fn add_vertex_to_batch_translated(&mut self, v: &Vtx, t: V3f) {
        rec!(self, "AddVertexToBatch", put::vtx(v), put::v3(t));
    }

    fn add_vertex_to_batch_matrix(&mut self, v: &Vtx, m: &Mat) {
        rec!(self, "AddVertexToBatch", put::vtx(v), put::m(m));
    }

    fn add_vertex_to_batch_size2d(&mut self, v: &Vtx, t: V3f, col: Option<Col>, w_: f32, h: f32) {
        rec!(
            self,
            "AddVertexToBatch_Size2D",
            put::vtx(v),
            put::v3(t),
            c(col.unwrap_or([0.0, 0.0, 0.0, 0.0])),
            f(w_),
            f(h)
        );
    }

    fn add_vertex_to_batch_raw(&mut self, pos: V3f, col: Col, tex: V3f) {
        rec!(
            self,
            "AddVertexToBatch_Raw",
            put::v3(pos),
            c(col),
            put::v3(tex)
        );
    }

    fn add_index_to_batch(&mut self, idx: i32) {
        rec!(self, "AddIndexToBatch", i(idx));
    }

    fn add_texcoord_to_batch(&mut self, unit: u32, co: V3f) {
        rec!(self, "AddTexCoordToBatch", u(unit), put::v3(co));
    }

    fn set_batch_texture_unit_active(&mut self, unit: u32, active: bool) {
        rec!(self, "SetBatchTextureUnitActive", u(unit), b(active));
    }

    fn flush_tri_batch(&mut self, flags: u32, auto_clear: bool) {
        rec!(self, "FlushTriBatch", u(flags), b(auto_clear));
    }

    fn flush_quad_batch(&mut self, flags: u32, auto_clear: bool) {
        rec!(self, "FlushQuadBatch", u(flags), b(auto_clear));
    }

    fn clear_batch(&mut self) {
        rec!(self, "ClearBatch");
    }

    fn draw_triangle(&mut self, v: &[Vtx]) {
        rec!(self, "DrawTriangle", put::vtxvec(v));
    }

    fn draw_quad_pos(&mut self, pos: V3f, size: V2f, col: Col) {
        rec!(self, "DrawQuad", put::v3(pos), put::v2(size), c(col));
    }

    fn draw_quad_uv(&mut self, pos: V3f, size: V2f, min: V2f, max: V2f, col: Col) {
        rec!(
            self,
            "DrawQuad",
            put::v3(pos),
            put::v2(size),
            put::v2(min),
            put::v2(max),
            c(col)
        );
    }

    fn draw_quad_uv2(&mut self, pos: V3f, size: V2f, uv: [V2f; 4], col: Col) {
        rec!(
            self,
            "DrawQuad",
            put::v3(pos),
            put::v2(size),
            put::v2(uv[0]),
            put::v2(uv[1]),
            put::v2(uv[2]),
            put::v2(uv[3]),
            c(col)
        );
    }

    fn draw_quad_vtx(&mut self, v: &[Vtx]) {
        rec!(self, "DrawQuad", put::vtxvec(v));
    }

    fn draw_quad_vtx_col(&mut self, v: &[Vtx], col: Col) {
        rec!(self, "DrawQuad", put::vtxvec(v), c(col));
    }

    fn draw_quad_vtx_z(&mut self, v: &[Vtx], z: f32) {
        rec!(self, "DrawQuad", put::vtxvec(v), f(z));
    }

    fn draw_quad_vtx_z_col(&mut self, v: &[Vtx], z: f32, col: Col) {
        rec!(self, "DrawQuad", put::vtxvec(v), f(z), c(col));
    }

    fn draw_quad_multitex(&mut self, v: &[Vtx], extra: &[V3f]) {
        rec!(self, "DrawQuadMultiTex", put::vtxvec(v), put::v3vec(extra));
    }

    fn draw_line(&mut self, bg: V3f, e: V3f, col: Col) {
        rec!(self, "DrawLine", put::v3(bg), put::v3(e), c(col));
    }

    fn draw_line_cols(&mut self, bg: V3f, bc: Col, e: V3f, ec: Col) {
        rec!(self, "DrawLine", put::v3(bg), c(bc), put::v3(e), c(ec));
    }

    fn draw_box_min_max(&mut self, min: V3f, max: V3f, col: Col) {
        rec!(self, "DrawBoxMinMax", put::v3(min), put::v3(max), c(col));
    }

    fn draw_sphere(&mut self, pos: V3f, radius: f32, col: Col) {
        rec!(self, "DrawSphere", put::v3(pos), f(radius), c(col));
    }

    fn draw_sphere_axes(&mut self, pos: V3f, radius: f32, cols: [Col; 3]) {
        rec!(
            self,
            "DrawSphere",
            put::v3(pos),
            f(radius),
            c(cols[0]),
            c(cols[1]),
            c(cols[2])
        );
    }

    fn draw_line_quad_rect(&mut self, r: Rect, z: f32, col: Col) {
        rec!(self, "DrawLineQuad", put::r(r), f(z), c(col));
    }

    fn draw_line_quad_pos(&mut self, pos: V3f, size: V2f, col: Col) {
        rec!(self, "DrawLineQuad", put::v3(pos), put::v2(size), c(col));
    }
}

impl TraceGfx {
    fn fb_rec(&mut self, fb: FbId, name: &str, args: Vec<String>) {
        self.call(&format!("fb#{}.{name}", fb.0), &args);
    }

    fn fb_adopt_size(&mut self, fb: FbId, t: Option<TexId>) {
        if let Some(sz) = t
            .and_then(|t| self.texs.get(&t.0))
            .map(|x| [x.size[0], x.size[1]])
        {
            self.fb_size.insert(fb.0, sz);
        }
    }
}
