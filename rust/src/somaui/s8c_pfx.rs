//! `cSomaPostEffect` and its fear tentacle and blur subclasses: effect objects, parameters, GPU
//! programs, render targets, pass sequencing and the `cPostEffect_*` natives. The C++ shell is an
//! `iPostEffect` that forwards `RenderEffect` and `Reset`; every engine call goes through [`Gpu`].

use super::ffi_s8b::Io;
use super::postfx::{self, BlurPlan, LevelSize, PARAMS, TentacleFades};
use super::s8d::hex;
use std::cell::RefCell;
use std::collections::HashMap;

/// Engine object pointer; 0 is none.
pub type Ptr = usize;

pub trait Gpu {
    fn call(&self, op: u32, io: &mut Io);
}

/// Host op numbers, in the order of `eOhplS8cOp` in `RustSomaS8b.h`.
pub const OPS: [&str; 62] = [
    "NewShell",
    "DeleteShell",
    "IsActive",
    "SetActive",
    "SetDisabled",
    "IsDisabled",
    "CompAdd",
    "CompRemove",
    "CompNum",
    "CompGet",
    "ViewportComp",
    "FrameTime",
    "TrailAlpha",
    "ProgramFromShaders",
    "ShaderNew",
    "ShaderSampler",
    "ProgramNew",
    "ProgramShader",
    "ProgramLink",
    "UniF",
    "UniV2",
    "UniV3",
    "UniV4",
    "UniArr",
    "UniMat",
    "SetFlat",
    "SetBlend",
    "SetChannel",
    "SetRange",
    "SetTexture",
    "SetProgram",
    "SetFinalFb",
    "SetFb",
    "DrawQuad",
    "FbResult",
    "TexSize",
    "TexBilinear",
    "ScreenSize",
    "CreateTarget",
    "DestroyTex",
    "DestroyFb",
    "CompSetFb",
    "ClearWhite",
    "SetDepthTest",
    "SetDepthWrite",
    "SetCull",
    "SetMatrixNull",
    "SetVb",
    "DrawCurrent",
    "MatCreate",
    "MatDestroy",
    "MatTex",
    "XmlTentacle",
    "EntNew",
    "EntAnim",
    "EntPlay",
    "EntAnimIndex",
    "EntBaseSpeed",
    "EntState",
    "EntUpdate",
    "EntVb",
    "EntDelete",
];

pub const OP_BASE: u32 = 32;
pub const OP_DUMP_ON: u32 = OP_BASE + OPS.len() as u32;
pub const OP_LOG: u32 = OP_DUMP_ON + 1;
pub const OP_LOG_END: u32 = OP_DUMP_ON + 2;

fn op(name: &str) -> u32 {
    OPS.iter()
        .position(|n| *n == name)
        .map_or(0, |i| OP_BASE + i as u32)
}

fn put(dst: &mut [f32], src: &[f32]) {
    for (d, s) in dst.iter_mut().zip(src) {
        *d = *s;
    }
}

fn group_of(groups: &[Ptr], i: usize) -> Ptr {
    i.checked_rem(groups.len())
        .and_then(|k| groups.get(k))
        .copied()
        .unwrap_or(0)
}

const CHROMA: usize = 0;
const RADIAL: usize = 1;
const TRAIL: usize = 2;
const FADE: usize = 3;
const DISTORT: usize = 4;
const TENT: usize = 6;
const BLUR: usize = 7;

/// Effect type name, GLSL fragment shader stem.
const TYPES: [(&str, Option<&str>); 8] = [
    ("ChromaticAberration", Some("chromatic_aberration")),
    ("RadialBlur", Some("radial_blur")),
    ("ImageTrail", Some("image_trail")),
    ("ImageFadeFX", Some("image_fade_fx")),
    ("VideoDistortion", Some("video_distortion")),
    ("FXAA", None),
    ("FearTentacles", None),
    ("Blur", None),
];

fn defaults(ty: usize) -> [f32; PARAMS] {
    let mut p = [0.0; PARAMS];
    match ty {
        CHROMA => p[1] = std::f32::consts::PI / 3.0,
        RADIAL => p[0] = 0.06,
        TRAIL => p[0] = 0.3,
        FADE => p[0] = 1.0,
        DISTORT => {
            p[0] = 1.0;
            p[2] = 0.15;
            p[3] = 1.0;
        }
        _ => {}
    }
    p
}

/// Typed wrappers of the host ops.
pub struct Gx<'a>(pub &'a dyn Gpu);

impl Gx<'_> {
    fn go(&self, op: u32, io: &mut Io) {
        self.0.call(op, io);
    }

    fn ptr(&self, op: u32, p: Ptr) -> Io {
        let mut io = Io::new();
        io.p[0] = p as u64;
        self.go(op, &mut io);
        io
    }

    fn named(&self, op: u32, p: Ptr, name: &str, f: &[f32]) {
        let mut io = Io::new();
        io.p[0] = p as u64;
        io.s = name.as_ptr();
        io.n = name.len();
        put(&mut io.f, f);
        self.go(op, &mut io);
    }

    fn int(&self, op: u32, p: Ptr, i: i32) -> Io {
        let mut io = Io::new();
        io.p[0] = p as u64;
        io.i[0] = i;
        self.go(op, &mut io);
        io
    }

    fn text(&self, op: u32, text: &[u8], i: [i32; 2]) -> Io {
        let mut io = Io::new();
        io.s = text.as_ptr();
        io.n = text.len();
        io.i[..2].copy_from_slice(&i);
        self.go(op, &mut io);
        io
    }

    fn dump(&self) -> bool {
        let mut io = Io::new();
        self.go(OP_DUMP_ON, &mut io);
        io.r != 0
    }

    fn log(&self, line: &str, mask: bool) {
        self.text(OP_LOG, line.as_bytes(), [i32::from(mask), 0]);
    }

    fn log_end(&self) {
        self.go(OP_LOG_END, &mut Io::new());
    }

    fn new_shell(&self) -> Ptr {
        let mut io = Io::new();
        self.go(op("NewShell"), &mut io);
        io.r as Ptr
    }

    fn is_active(&self, shell: Ptr) -> bool {
        self.ptr(op("IsActive"), shell).r != 0
    }

    fn is_disabled(&self, shell: Ptr) -> bool {
        self.ptr(op("IsDisabled"), shell).r != 0
    }

    fn comp_num(&self, comp: Ptr) -> i32 {
        self.ptr(op("CompNum"), comp).r as i32
    }

    fn comp_get(&self, comp: Ptr, i: i32) -> Ptr {
        self.int(op("CompGet"), comp, i).r as Ptr
    }

    fn viewport_comp(&self) -> Ptr {
        let mut io = Io::new();
        self.go(op("ViewportComp"), &mut io);
        io.r as Ptr
    }

    fn comp_remove(&self, comp: Ptr, shell: Ptr) {
        let mut io = Io::new();
        io.p = [comp as u64, shell as u64];
        self.go(op("CompRemove"), &mut io);
    }

    fn frame_time(&self) -> f32 {
        let mut io = Io::new();
        self.go(op("FrameTime"), &mut io);
        io.f[0]
    }

    fn program_from_shaders(&self, name: &str, vtx: &str, frag: &str, vars: &[&str]) -> Ptr {
        let mut s = format!("{name}\n{vtx}\n{frag}");
        for v in vars {
            s.push('\n');
            s.push_str(v);
        }
        self.text(op("ProgramFromShaders"), s.as_bytes(), [0, 0]).r as Ptr
    }

    fn uni(&self, prog: Ptr, name: &str, v: &[f32]) {
        let o = match v.len() {
            1 => op("UniF"),
            2 => op("UniV2"),
            3 => op("UniV3"),
            4 => op("UniV4"),
            _ => return,
        };
        self.named(o, prog, name, v);
    }

    fn uni_arr(&self, prog: Ptr, name: &str, v: &[f32]) {
        let mut io = Io::new();
        io.p[0] = prog as u64;
        io.s = name.as_ptr();
        io.n = name.len();
        io.i[0] = v.len() as i32;
        put(&mut io.f, v);
        self.go(op("UniArr"), &mut io);
    }

    fn uni_mat(&self, prog: Ptr, name: &str, m: &[f32; 16]) {
        self.named(op("UniMat"), prog, name, m);
    }

    fn flat(&self) {
        self.go(op("SetFlat"), &mut Io::new());
    }

    /// 0 none, 1 MulX2.
    fn blend(&self, mode: i32) {
        self.int(op("SetBlend"), 0, mode);
    }

    fn channel_rgba(&self) {
        self.go(op("SetChannel"), &mut Io::new());
    }

    fn range(&self, n: i32) {
        self.int(op("SetRange"), 0, n);
    }

    fn tex(&self, i: i32, t: Ptr) {
        self.int(op("SetTexture"), t, i);
    }

    fn program(&self, p: Ptr) {
        self.ptr(op("SetProgram"), p);
    }

    fn final_fb(&self, fb: Ptr) {
        self.ptr(op("SetFinalFb"), fb);
    }

    fn set_fb(&self, fb: Ptr, tex: Ptr) {
        let mut io = Io::new();
        io.p = [fb as u64, tex as u64];
        self.go(op("SetFb"), &mut io);
    }

    fn quad(&self, tex: Ptr, flip_y: bool) {
        self.int(op("DrawQuad"), tex, i32::from(flip_y));
    }

    fn fb_result(&self, fb: Ptr) -> Ptr {
        self.ptr(op("FbResult"), fb).r as Ptr
    }

    fn tex_size(&self, t: Ptr) -> ([i32; 2], [f32; 2]) {
        let io = self.ptr(op("TexSize"), t);
        ([io.i[0], io.i[1]], [io.f[0], io.f[1]])
    }

    fn bilinear(&self, t: Ptr) {
        self.ptr(op("TexBilinear"), t);
    }

    fn screen_size(&self) -> ([i32; 2], [f32; 2]) {
        let mut io = Io::new();
        self.go(op("ScreenSize"), &mut io);
        ([io.i[0], io.i[1]], [io.f[0], io.f[1]])
    }

    /// Render target texture and its frame buffer.
    fn create_target(&self, name: &str, w: i32, h: i32) -> (Ptr, Ptr) {
        let mut io = Io::new();
        io.s = name.as_ptr();
        io.n = name.len();
        io.i[..2].copy_from_slice(&[w, h]);
        self.go(op("CreateTarget"), &mut io);
        (io.r as Ptr, io.p[0] as Ptr)
    }

    fn destroy_tex(&self, t: Ptr) {
        self.ptr(op("DestroyTex"), t);
    }

    fn destroy_fb(&self, fb: Ptr) {
        self.ptr(op("DestroyFb"), fb);
    }

    fn mat_create(&self, file: &[u8]) -> Ptr {
        self.text(op("MatCreate"), file, [0, 0]).r as Ptr
    }

    fn mat_texture(&self, mat: Ptr, diffuse: bool) -> Ptr {
        self.int(op("MatTex"), mat, i32::from(!diffuse)).r as Ptr
    }
}

#[derive(Default)]
struct Level {
    w: i32,
    h: i32,
    div: i32,
    tex_a: Ptr,
    tex_b: Ptr,
    fb_a: Ptr,
    fb_b: Ptr,
}

struct Anim {
    name: Vec<u8>,
    idx: i32,
    speed: f32,
    weight: f32,
}

#[derive(Default)]
struct Tent {
    fades: TentacleFades,
    anims: Vec<Anim>,
    mats: Vec<Ptr>,
    groups: Vec<Ptr>,
    tentacles: Vec<([f32; 3], [f32; 3])>,
    time: f32,
    target: Ptr,
    buffer: Ptr,
    size: [i32; 2],
}

struct Effect {
    ty: usize,
    params: [f32; PARAMS],
    textures: [Ptr; 3],
    clear: bool,
    t: f32,
    tent: Tent,
    levels: Vec<Level>,
}

#[derive(Default)]
struct Progs {
    plain: [Ptr; 8],
    tried: [bool; 8],
    tent: Ptr,
    tent_merge: Ptr,
    tent_tried: bool,
    blur: [Ptr; 4],
    blur_tried: bool,
}

thread_local! {
    static FX: RefCell<HashMap<Ptr, Effect>> = RefCell::new(HashMap::new());
    static PROGS: RefCell<Progs> = RefCell::new(Progs::default());
}

fn with_fx<R>(shell: Ptr, f: impl FnOnce(&mut Effect) -> R) -> Option<R> {
    FX.with(|m| m.try_borrow_mut().ok()?.get_mut(&shell).map(f))
}

#[cfg(test)]
fn forget_programs() {
    let _ = with_progs(|p| *p = Progs::default());
}

fn with_progs<R>(f: impl FnOnce(&mut Progs) -> R) -> Option<R> {
    PROGS.with(|p| p.try_borrow_mut().ok().map(|mut p| f(&mut p)))
}

fn exists(shell: Ptr) -> bool {
    with_fx(shell, |_| ()).is_some()
}

pub fn create(g: &Gx, ty: usize) -> Ptr {
    let shell = g.new_shell();
    let fx = Effect {
        ty,
        params: defaults(ty),
        textures: [0; 3],
        clear: true,
        t: 0.0,
        tent: Tent::default(),
        levels: Vec::new(),
    };
    FX.with(|m| {
        if let Ok(mut m) = m.try_borrow_mut() {
            m.insert(shell, fx);
        }
    });
    shell
}

/// The shell's `Reset` and `OnSetActive(false)`.
pub fn reset(shell: Ptr) {
    let _ = with_fx(shell, |e| e.clear = true);
}

/// `cGraphics_DestroyPostEffect`.
pub fn destroy(g: &Gx, shell: Ptr) {
    if !exists(shell) {
        return;
    }
    g.comp_remove(g.viewport_comp(), shell);
    let fx = FX.with(|m| m.try_borrow_mut().ok().and_then(|mut m| m.remove(&shell)));
    if let Some(mut fx) = fx {
        clear_entities(g, &mut fx.tent);
        for m in fx.tent.mats.drain(..) {
            if m != 0 {
                g.ptr(op("MatDestroy"), m);
            }
        }
        destroy_target(g, &mut fx.tent);
        destroy_levels(g, &mut fx.levels);
    }
    g.ptr(op("DeleteShell"), shell);
}

/// `Set`: store the values, switch the effect on or off by its key parameter.
fn set(g: &Gx, shell: Ptr, vals: &[f32]) {
    let Some((ty, params)) = with_fx(shell, |e| {
        for (d, s) in e.params.iter_mut().zip(vals) {
            *d = *s;
        }
        (e.ty, e.params)
    }) else {
        return;
    };
    let (active, clear) = postfx::set_activity(ty, &params, g.is_active(shell));
    if clear {
        reset(shell);
    }
    g.int(op("SetActive"), shell, i32::from(active));
}

fn render_plain(g: &Gx, fx: &mut Effect, tex_in: Ptr, fb_out: Ptr) -> Ptr {
    if fx.ty == TRAIL {
        let alpha = postfx::trail_alpha(fx.params[0], g.frame_time(), fx.clear);
        g.log(&format!("trail={}", hex(alpha)), false);
        let mut io = Io::new();
        io.f[0] = alpha;
        g.go(op("TrailAlpha"), &mut io);
        fx.clear = false;
        return tex_in;
    }
    let Some((name, Some(shader))) = TYPES.get(fx.ty).copied() else {
        return tex_in;
    };
    let prog = with_progs(|p| {
        let (tried, slot) = (p.tried.get_mut(fx.ty)?, p.plain.get_mut(fx.ty)?);
        if !*tried {
            *tried = true;
            *slot = g.program_from_shaders(
                &format!("Soma{name}"),
                "posteffect_quad_vtx.glsl",
                &format!("posteffect_{shader}_frag.glsl"),
                &["UseUv"],
            );
        }
        Some(*slot)
    })
    .flatten()
    .unwrap_or(0);
    if prog == 0 {
        return tex_in;
    }
    let p = fx.params;
    let (_, tex) = g.tex_size(tex_in);
    let frame_time = g.frame_time();
    g.flat();
    g.blend(0);
    g.channel_rgba();
    g.range(1);
    g.tex(0, tex_in);
    g.final_fb(fb_out);
    g.program(prog);
    if fx.ty != RADIAL {
        g.bilinear(tex_in);
    }
    match fx.ty {
        CHROMA => {
            let (_, screen) = g.screen_size();
            let c = postfx::chromatic(&p, screen[1], tex);
            g.uni(prog, "avOffsetA", &c.offset_a);
            g.uni(prog, "avOffsetB", &c.offset_b);
            g.uni(prog, "avOffsetC", &c.offset_c);
            g.uni(prog, "avColorA", &c.color_a);
            g.uni(prog, "avColorB", &c.color_b);
            g.uni(prog, "avColorC", &c.color_c);
        }
        RADIAL => {
            g.uni(prog, "afSize", &[p[0]]);
            g.uni(prog, "afBlurStartDist", &[p[2]]);
            g.uni(prog, "afAlpha", &[p[1]]);
        }
        FADE => {
            fx.t += frame_time;
            g.uni(prog, "avScreenSize", &tex);
            g.uni(prog, "afAmount", &[p[0]]);
            g.uni(prog, "afT", &[fx.t]);
            for (i, t) in fx.textures.iter().enumerate() {
                g.tex(i as i32 + 1, *t);
            }
        }
        DISTORT => {
            g.uni(prog, "avScreenSize", &tex);
            g.uni(prog, "afAmount", &[p[0]]);
            g.uni(prog, "afSeed", &[p[1]]);
            g.uni(prog, "afLineDensity", &[p[2]]);
            g.uni(prog, "afOffsetMul", &[p[3]]);
            g.uni(prog, "avScreenOffset", &[p[4], p[5]]);
            g.uni(prog, "avScreenBendAmount", &[p[6], p[7]]);
        }
        _ => {}
    }
    g.quad(tex_in, true);
    g.program(0);
    g.blend(0);
    g.range(0);
    g.fb_result(fb_out)
}

const TENT_VTX: &str = r#"#version 120
uniform mat4 a_mtxModel;
uniform vec3 avPos;
uniform float afTentacleScale, afIntensity, afTime, afMaterialAlpha, afInstance;
uniform sampler2D aDispA, aDispB;
void main()
{
	vec2 uv = gl_MultiTexCoord0.xy;
	vec4 d = texture2DLod(aDispA, uv, 0.0);
	if (afMaterialAlpha > 0.00001) d = mix(d, texture2DLod(aDispB, uv, 0.0), afMaterialAlpha);
	vec3 p = gl_Vertex.xyz;
	p.xy += d.xy * afIntensity * 0.2 * sin(afTime + d.b * 2.0 + afInstance);
	gl_Position = vec4((a_mtxModel * vec4(p * afTentacleScale, 0.0)).xyz + avPos, 1.0);
	gl_TexCoord[0].xy = uv;
}"#;

const TENT_FRAG: &str = r#"#version 120
uniform sampler2D aTexA, aTexB;
uniform float afIntensity, afAlpha, afMaterialAlpha;
void main()
{
	vec4 c = texture2D(aTexA, gl_TexCoord[0].xy);
	if (afMaterialAlpha > 0.00001) c = mix(c, texture2D(aTexB, gl_TexCoord[0].xy), afMaterialAlpha);
	gl_FragColor = mix(vec4(0.5, 0.5, 0.5, 0.0), c, c.a * afIntensity * afAlpha);
}"#;

const SHADER_VERTEX: i32 = 0;
const SHADER_FRAGMENT: i32 = 1;

fn tent_shader(g: &Gx, ty: i32, name: &str, src: &str) -> Ptr {
    let mut io = Io::new();
    let text = format!("{name}\u{1}{src}");
    io.s = text.as_ptr();
    io.n = text.len();
    io.i[0] = ty;
    g.go(op("ShaderNew"), &mut io);
    io.r as Ptr
}

fn sampler(g: &Gx, shader: Ptr, name: &str, unit: i32) {
    let mut io = Io::new();
    io.p[0] = shader as u64;
    io.s = name.as_ptr();
    io.n = name.len();
    io.i[0] = unit;
    g.go(op("ShaderSampler"), &mut io);
}

fn tent_programs(g: &Gx) -> Option<(Ptr, Ptr)> {
    with_progs(|p| {
        if !p.tent_tried {
            p.tent_tried = true;
            let vtx = tent_shader(g, SHADER_VERTEX, "SomaFearTentacles_vtx", TENT_VTX);
            let frag = if vtx != 0 {
                tent_shader(g, SHADER_FRAGMENT, "SomaFearTentacles_frag", TENT_FRAG)
            } else {
                0
            };
            if vtx != 0 && frag != 0 {
                sampler(g, frag, "aTexA", 0);
                sampler(g, frag, "aTexB", 1);
                sampler(g, vtx, "aDispA", 2);
                sampler(g, vtx, "aDispB", 3);
                let mut io = Io::new();
                let name = "SomaFearTentacles";
                io.s = name.as_ptr();
                io.n = name.len();
                g.go(op("ProgramNew"), &mut io);
                let prog = io.r as Ptr;
                for (shader, ty) in [(vtx, SHADER_VERTEX), (frag, SHADER_FRAGMENT)] {
                    let mut io = Io::new();
                    io.p = [prog as u64, shader as u64];
                    io.i[0] = ty;
                    g.go(op("ProgramShader"), &mut io);
                }
                if g.ptr(op("ProgramLink"), prog).r != 0 {
                    p.tent = prog;
                }
            }
            p.tent_merge = g.program_from_shaders(
                "SomaFearTentaclesMerge",
                "posteffect_quad_vtx.glsl",
                "posteffect_fear_tentacles_merge_frag.glsl",
                &["UseUv"],
            );
        }
        (p.tent, p.tent_merge)
    })
    .filter(|(a, b)| *a != 0 && *b != 0)
}

fn destroy_target(g: &Gx, t: &mut Tent) {
    if t.buffer != 0 {
        g.destroy_fb(t.buffer);
    }
    if t.target != 0 {
        g.destroy_tex(t.target);
    }
    t.buffer = 0;
    t.target = 0;
}

fn clear_entities(g: &Gx, t: &mut Tent) {
    for e in t.groups.drain(..) {
        g.ptr(op("EntDelete"), e);
    }
    t.tentacles.clear();
    t.anims.clear();
    t.fades.clear();
}

fn render_tentacles(g: &Gx, fx: &mut Effect, tex_in: Ptr, fb_out: Ptr) -> Ptr {
    let t = &mut fx.tent;
    let mat_at = |t: &Tent, i: i32| usize::try_from(i).ok().and_then(|i| t.mats.get(i).copied());
    let mat_a = mat_at(t, t.fades.mat).unwrap_or(0);
    if t.tentacles.is_empty() || t.anims.is_empty() || mat_a == 0 {
        return tex_in;
    }
    let Some((prog, merge)) = tent_programs(g) else {
        return tex_in;
    };
    let dt = g.frame_time();
    t.time = postfx::tentacle_time(t.time, dt, t.anims.first().map_or(0.0, |a| a.speed));
    t.fades.advance(dt);
    let mat_a = mat_at(t, t.fades.mat).unwrap_or(0);
    let (second, alpha) = t.fades.blend();
    let mat_b = second.and_then(|i| mat_at(t, i)).unwrap_or(0);
    if mat_a == 0 {
        return tex_in;
    }
    let mat_b = if mat_b == 0 { mat_a } else { mat_b };

    for &ent in &t.groups {
        for a in &t.anims {
            let mut io = Io::new();
            io.p[0] = ent as u64;
            io.i[0] = a.idx;
            io.f[..2].copy_from_slice(&[a.speed, a.weight]);
            g.go(op("EntState"), &mut io);
        }
        let mut io = Io::new();
        io.p[0] = ent as u64;
        io.f[0] = dt;
        g.go(op("EntUpdate"), &mut io);
    }

    let (px, _) = g.tex_size(tex_in);
    let size = postfx::tentacle_target(px[0], px[1]);
    if t.target == 0 || t.size != size {
        destroy_target(g, t);
        (t.target, t.buffer) = g.create_target("SomaFearTentacles", size[0], size[1]);
        t.size = size;
    }

    let p = fx.params;
    g.log(&format!("tentacle target={}x{}", size[0], size[1]), false);
    g.ptr(op("CompSetFb"), t.buffer);
    g.go(op("ClearWhite"), &mut Io::new());
    g.int(op("SetDepthTest"), 0, 0);
    g.int(op("SetDepthWrite"), 0, 0);
    g.int(op("SetCull"), 0, 0);
    g.blend(1);
    g.channel_rgba();
    g.tex(0, g.mat_texture(mat_a, true));
    g.tex(1, g.mat_texture(mat_b, true));
    g.tex(2, g.mat_texture(mat_a, false));
    g.tex(3, g.mat_texture(mat_b, false));
    g.range(4);
    g.go(op("SetMatrixNull"), &mut Io::new());
    g.program(prog);
    g.uni(prog, "afIntensity", &[p[0]]);
    g.uni(prog, "afTentacleScale", &[p[1]]);
    g.uni(prog, "afAlpha", &[p[5]]);
    g.uni(prog, "afTime", &[t.time]);
    g.uni(prog, "afMaterialAlpha", &[alpha]);
    for (i, (pos, rot)) in t.tentacles.iter().enumerate() {
        // ponytail: displaced after CPU skinning, tentacles of a group share a pose; GPU dual-quat instancing if it shows
        let model = postfx::tentacle_model(size, crate::math::v3(rot[0], rot[1], rot[2]));
        let mut flat = [0.0f32; 16];
        put(
            &mut flat,
            &model.m.iter().flatten().copied().collect::<Vec<f32>>(),
        );
        g.uni_mat(prog, "a_mtxModel", &flat);
        let v = postfx::tentacle_pos(crate::math::v3(pos[0], pos[1], pos[2]), p[2]);
        g.uni(prog, "avPos", &[v.x, v.y, v.z]);
        g.uni(prog, "afInstance", &[i as f32]);
        let ent = group_of(&t.groups, i);
        let vb = g.ptr(op("EntVb"), ent).r as Ptr;
        g.ptr(op("SetVb"), vb);
        g.go(op("DrawCurrent"), &mut Io::new());
    }
    g.ptr(op("SetVb"), 0);
    g.int(op("SetCull"), 0, 1);

    g.flat();
    g.blend(0);
    g.range(2);
    g.tex(0, tex_in);
    g.tex(1, t.target);
    g.final_fb(fb_out);
    g.program(merge);
    g.uni(merge, "afIntensity", &[p[4]]);
    g.uni(merge, "afAlpha", &[p[5]]);
    g.uni(merge, "avTentacleUvMul", &[1.0, 1.0]);
    g.uni(merge, "afAspect", &[postfx::tentacle_aspect(size)]);
    g.uni(merge, "afCircleRadius", &[p[6]]);
    g.uni(merge, "afCircleAlpha", &[p[7]]);
    g.uni(merge, "afCircleSoftness", &[p[8]]);
    g.quad(tex_in, true);
    g.program(0);
    g.range(0);
    g.fb_result(fb_out)
}

fn destroy_levels(g: &Gx, levels: &mut Vec<Level>) {
    for (i, l) in levels.iter().enumerate() {
        for fb in [l.fb_a, l.fb_b] {
            if fb != 0 {
                g.destroy_fb(fb);
            }
        }
        if l.tex_b != 0 {
            g.destroy_tex(l.tex_b);
        }
        if l.tex_a != 0 && i > 0 {
            g.destroy_tex(l.tex_a);
        }
    }
    levels.clear();
}

fn blur_programs(g: &Gx) -> Option<[Ptr; 4]> {
    with_progs(|p| {
        if !p.blur_tried {
            p.blur_tried = true;
            let names = [
                "SomaBlur",
                "SomaBlurPlain",
                "SomaBlurCombineMip",
                "SomaBlurCombine",
            ];
            for (i, name) in names.iter().enumerate() {
                let mut vars = vec!["UseUv"];
                if i == postfx::PROG_BLUR as usize || i == postfx::PROG_BLUR_COMBINE as usize {
                    vars.push("Blur");
                    vars.push("kTotalBlurSamples=7");
                }
                if i >= postfx::PROG_COMBINE_MIP as usize {
                    vars.push("CombineMip");
                }
                if let Some(slot) = p.blur.get_mut(i) {
                    *slot = g.program_from_shaders(
                        name,
                        "posteffect_quad_vtx.glsl",
                        "posteffect_blur_frag.glsl",
                        &vars,
                    );
                }
            }
        }
        p.blur
    })
    .filter(|p| p.iter().all(|x| *x != 0))
}

fn create_levels(g: &Gx, levels: &mut Vec<Level>, screen: [i32; 2]) {
    destroy_levels(g, levels);
    for (i, s) in postfx::blur_levels(screen).into_iter().enumerate() {
        let mut l = Level {
            w: s.w,
            h: s.h,
            div: s.div,
            ..Level::default()
        };
        if i > 0 {
            (l.tex_a, l.fb_a) = g.create_target("SomaBlur", l.w, l.h);
        }
        (l.tex_b, l.fb_b) = g.create_target("SomaBlur", l.w, l.h);
        levels.push(l);
    }
}

/// Binds a blur program, with the weights and offsets of `weights` when given.
fn blur_program(
    g: &Gx,
    progs: &[Ptr; 4],
    which: i32,
    w: Option<&postfx::BlurWeights>,
    vertical: bool,
) -> Ptr {
    let p = progs.get(which as usize).copied().unwrap_or(0);
    g.program(p);
    if let Some(w) = w {
        g.uni_arr(p, "avWeights", &w.weights);
        let offs = if vertical { &w.off_v } else { &w.off_h };
        for (i, v) in offs.iter().enumerate() {
            g.uni(p, &format!("avOffsets[{i}]"), v);
        }
    }
    p
}

fn blur_pass(g: &Gx, fb: Ptr, tex: Ptr) {
    g.set_fb(fb, tex);
    g.tex(0, tex);
    g.quad(tex, false);
}

fn render_blur(g: &Gx, fx: &mut Effect, tex_in: Ptr, fb_out: Ptr) -> Ptr {
    if fx.params[0] <= 0.0 {
        return tex_in;
    }
    let Some(progs) = blur_programs(g) else {
        return tex_in;
    };
    let (screen, _) = g.screen_size();
    if fx
        .levels
        .first()
        .is_none_or(|l| l.w != screen[0] || l.h != screen[1])
    {
        create_levels(g, &mut fx.levels, screen);
    }
    if let Some(l) = fx.levels.first_mut() {
        l.tex_a = tex_in;
    }
    g.flat();
    g.blend(0);
    g.channel_rgba();
    g.range(1);
    let sizes: Vec<LevelSize> = fx
        .levels
        .iter()
        .map(|l| LevelSize {
            w: l.w,
            h: l.h,
            div: l.div,
        })
        .collect();
    let plan: BlurPlan = postfx::blur_plan(fx.params[0], screen[1], &sizes);
    let mut result = tex_in;
    for op_ in plan.ops.iter().take(plan.n_ops) {
        let level = |i: i32| usize::try_from(i).ok().and_then(|i| fx.levels.get(i));
        let Some(l) = level(op_.level) else {
            continue;
        };
        match op_.kind {
            postfx::OP_DOWNSAMPLE => {
                blur_program(g, &progs, postfx::PROG_PLAIN, None, false);
                let src = level(op_.level - 1).map_or(0, |p| p.tex_a);
                blur_pass(g, l.fb_a, src);
            }
            postfx::OP_BLUR => {
                let w = plan
                    .weights
                    .get(usize::try_from(op_.slot).unwrap_or(usize::MAX));
                blur_program(g, &progs, postfx::PROG_BLUR, w, op_.vertical);
                if op_.vertical {
                    blur_pass(g, l.fb_a, l.tex_b);
                } else {
                    blur_pass(g, l.fb_b, l.tex_a);
                }
            }
            _ => {
                let w = usize::try_from(op_.slot)
                    .ok()
                    .and_then(|s| plan.weights.get(s));
                let p = blur_program(g, &progs, op_.program, w, op_.vertical);
                if op_.program == postfx::PROG_COMBINE_MIP
                    || op_.program == postfx::PROG_BLUR_COMBINE
                {
                    g.uni(p, "afAlpha", &[op_.alpha]);
                }
                let lt = level(op_.tex0_level);
                let tex = lt.map_or(0, |lt| if op_.tex0_b { lt.tex_b } else { lt.tex_a });
                if let Some(t1) = level(op_.tex1_level) {
                    g.range(2);
                    g.tex(1, t1.tex_a);
                }
                g.tex(0, tex);
                g.final_fb(fb_out);
                g.quad(tex, true);
                result = g.fb_result(fb_out);
            }
        }
    }
    g.program(0);
    g.range(0);
    if let Some(l) = fx.levels.first_mut() {
        l.tex_a = 0;
    }
    result
}

/// `cSomaPostEffect::RenderEffect`.
pub fn render(g: &Gx, shell: Ptr, tex_in: Ptr, fb_out: Ptr) -> Ptr {
    let Some(mut fx) = FX.with(|m| m.try_borrow_mut().ok().and_then(|mut m| m.remove(&shell)))
    else {
        return tex_in;
    };
    if g.dump() {
        let mask = fx.ty == DISTORT;
        let params: String = fx
            .params
            .iter()
            .map(|p| format!("{},", if mask { "rnd".to_string() } else { hex(*p) }))
            .collect();
        let name = TYPES.get(fx.ty).map_or("", |t| t.0);
        g.log(
            &format!(
                "fx={name} dt={} clear={} p={params}",
                hex(g.frame_time()),
                i32::from(fx.clear)
            ),
            mask,
        );
    }
    let out = match fx.ty {
        TENT => render_tentacles(g, &mut fx, tex_in, fb_out),
        BLUR => render_blur(g, &mut fx, tex_in, fb_out),
        _ => render_plain(g, &mut fx, tex_in, fb_out),
    };
    g.log_end();
    FX.with(|m| {
        if let Ok(mut m) = m.try_borrow_mut() {
            m.insert(shell, fx);
        }
    });
    out
}

pub fn register(e: crate::somascript::s2_as::Engine, gpu: &'static dyn Gpu) {
    natives::register(e, gpu);
}

mod natives {
    use super::{
        BLUR, Gpu, Gx, Ptr, TENT, TYPES, clear_entities, create, destroy, exists, group_of, op,
        set, with_fx,
    };
    use crate::somascript::s2_api::is_dummy;
    use crate::somascript::s2_as::{Engine, Frame, Soma};
    use std::ffi::c_void;

    fn handle(f: &Frame, i: usize) -> Ptr {
        let p = f.ptr_arg(i);
        if is_dummy(p) { 0 } else { p as Ptr }
    }

    fn ret_handle(f: &Frame, p: Ptr) {
        f.ret_ref(p as *mut c_void);
    }

    fn me(f: &Frame) -> Ptr {
        f.this as Ptr
    }

    fn params<const N: usize>(shell: Ptr) -> Option<[f32; N]> {
        with_fx(shell, |e| {
            std::array::from_fn(|i| e.params.get(i).copied().unwrap_or(0.0))
        })
    }

    fn out_params(f: &Frame, shell: Ptr, n: usize) {
        if let Some(p) = params::<9>(shell) {
            for (i, v) in p.iter().take(n).enumerate() {
                *f.rm::<f32>(i) = *v;
            }
        }
    }

    fn tent<R>(shell: Ptr, f: impl FnOnce(&mut super::Tent) -> R) -> Option<R> {
        with_fx(shell, |e| f(&mut e.tent))
    }

    pub fn register(e: Engine, gpu: &'static dyn Gpu) {
        let s = Soma(e);
        let g = move || Gx(gpu);
        let comp = "cPostEffectComposite";
        s.plain(
            "cViewport",
            "cPostEffectComposite@ GetPostEffectComposite()",
            move |f| ret_handle(f, g().viewport_comp()),
        );
        s.method(
            comp,
            "void AddPostEffect(iPostEffect@ apPostEffect, int alPrio)",
            move |f| {
                let p = handle(f, 0);
                if exists(p) {
                    let mut io = super::Io::new();
                    io.p = [me(f) as u64, p as u64];
                    io.i[0] = f.arg(1);
                    g().0.call(op("CompAdd"), &mut io);
                }
            },
        );
        s.method(
            comp,
            "void RemovePostEffect(iPostEffect@ apPostEffect)",
            move |f| {
                let p = handle(f, 0);
                if exists(p) {
                    g().comp_remove(me(f), p);
                }
            },
        );
        s.method(comp, "iPostEffect@ GetPostEffect(int alIdx)", move |f| {
            let (c, i): (Ptr, i32) = (me(f), f.arg(0));
            let g = g();
            ret_handle(
                f,
                if i >= 0 && i < g.comp_num(c) {
                    g.comp_get(c, i)
                } else {
                    0
                },
            );
        });
        s.method(
            comp,
            "iPostEffect@ GetPostEffectFromType(const tString&in asType)",
            move |f| {
                let (c, want) = (me(f), f.str_arg(0));
                let g = g();
                let found = (0..g.comp_num(c)).map(|i| g.comp_get(c, i)).find(|p| {
                    with_fx(*p, |e| {
                        TYPES.get(e.ty).is_some_and(|t| t.0.as_bytes() == want)
                    }) == Some(true)
                });
                ret_handle(f, found.unwrap_or(0));
            },
        );
        s.func(
            "void cGraphics_DestroyPostEffect(iPostEffect@ apPostEffect)",
            move |f| {
                destroy(&g(), handle(f, 0));
            },
        );

        for ty in [
            "iPostEffect",
            "cPostEffect_ChromaticAberration",
            "cPostEffect_FXAA",
            "cPostEffect_ImageFadeFX",
            "cPostEffect_ImageTrail",
            "cPostEffect_RadialBlur",
            "cPostEffect_VideoDistortion",
            "cPostEffect_FearTentacles",
            "cPostEffect_Blur",
        ] {
            if e.type_by_name(ty.as_bytes()).is_none() {
                continue;
            }
            s.method(ty, "void SetDisabled(bool abX)", move |f| {
                if exists(me(f)) {
                    g().int(op("SetDisabled"), me(f), i32::from(f.arg::<bool>(0)));
                }
            });
            s.method(ty, "bool IsDisabled()", move |f| {
                f.ret(exists(me(f)) && g().is_disabled(me(f)));
            });
            s.method(ty, "void SetActive(bool abX)", move |f| {
                if exists(me(f)) {
                    g().int(op("SetActive"), me(f), i32::from(f.arg::<bool>(0)));
                }
            });
            s.method(ty, "bool IsActive()", move |f| {
                f.ret(exists(me(f)) && g().is_active(me(f)));
            });
            if ty != "iPostEffect" {
                s.method(ty, "void Reset()", |f| super::reset(me(f)));
            }
        }

        let make = move |f: &Frame, ty: usize| ret_handle(f, create(&g(), ty));
        s.func(
            "cPostEffect_ChromaticAberration@ cGraphics_CreatePostEffect_ChromaticAberration()",
            move |f| make(f, 0),
        );
        s.func(
            "cPostEffect_FXAA@ cGraphics_CreatePostEffect_FXAA()",
            move |f| make(f, 5),
        );
        s.func(
            "cPostEffect_ImageFadeFX@ cGraphics_CreatePostEffect_ImageFadeFX()",
            move |f| make(f, 3),
        );
        s.func(
            "cPostEffect_ImageTrail@ cGraphics_CreatePostEffect_ImageTrail()",
            move |f| make(f, 2),
        );
        s.func(
            "cPostEffect_RadialBlur@ cGraphics_CreatePostEffect_RadialBlur()",
            move |f| make(f, 1),
        );

        let ca = "cPostEffect_ChromaticAberration";
        s.method(
            ca,
            "void SetParams(float afAmount, float afRotation, float afHue, const cVector2f&in avOffset)",
            move |f| {
                let o = *f.rf::<[f32; 2]>(3);
                set(&g(), me(f), &[f.arg(0), f.arg(1), f.arg(2), o[0], o[1]]);
            },
        );
        s.method(
            ca,
            "void GetParams(float &out afAmount, float &out afRotation, float &out afHue, cVector2f &out avOffset)",
            |f| {
                if let Some(p) = params::<5>(me(f)) {
                    *f.rm::<f32>(0) = p[0];
                    *f.rm::<f32>(1) = p[1];
                    *f.rm::<f32>(2) = p[2];
                    *f.rm::<[f32; 2]>(3) = [p[3], p[4]];
                }
            },
        );
        let fade = "cPostEffect_ImageFadeFX";
        s.method(
            fade,
            "void SetParams(float afAmount, iTexture@ apFadeTexture, iTexture@ apColorTexture, iTexture@ apOffsetTexture)",
            move |f| {
                set(&g(), me(f), &[f.arg(0)]);
                let t = [handle(f, 1), handle(f, 2), handle(f, 3)];
                let _ = with_fx(me(f), |e| e.textures = t);
            },
        );
        s.method(fade, "void GetParams(float &out afAmount)", |f| {
            out_params(f, me(f), 1)
        });
        let trail = "cPostEffect_ImageTrail";
        s.method(trail, "void SetParams(float afAmount)", move |f| {
            set(&g(), me(f), &[f.arg(0)]);
        });
        s.method(trail, "void GetParams(float &out afAmount)", |f| {
            out_params(f, me(f), 1)
        });
        let radial = "cPostEffect_RadialBlur";
        s.method(
            radial,
            "void SetParams(float afSize, float afAlpha, float afBlurStartDist)",
            move |f| set(&g(), me(f), &[f.arg(0), f.arg(1), f.arg(2)]),
        );
        s.method(
            radial,
            "void GetParams(float &out afSize, float &out afAlpha, float &out afBlurStartDist)",
            |f| out_params(f, me(f), 3),
        );

        if e.type_by_name(b"cPostEffect_FearTentacles").is_some() {
            let t = "cPostEffect_FearTentacles";
            s.func(
                "cPostEffect_FearTentacles@ cGraphics_CreatePostEffect_FearTentacles()",
                move |f| make(f, TENT),
            );
            s.method(
                t,
                "void SetParams(float afIntensity, float afTentacleScale, float afSpreadDist, float afRefraction, float afVingette, float afAlpha, float afCircleRadius, float afCircleAlpha, float afCircleSoftness)",
                |f| {
                    let v: [f32; 9] = std::array::from_fn(|i| f.arg(i));
                    let _ = with_fx(me(f), |e| e.params = v);
                },
            );
            s.method(
                t,
                "void GetParams(float&out afIntensity, float&out afTentacleScale, float&out afSpreadDist, float&out afRefraction, float&out afVingette, float&out afAlpha, float&out afCircleRadius, float&out afCircleAlpha, float&out afCircleSoftness)",
                |f| out_params(f, me(f), 9),
            );
            s.method(
                t,
                "void SetupTentacleEntity(const tString&in asEntFile, int alNum, int alAnimGroupNum)",
                move |f| setup_entity(&g(), me(f), f.str_arg(0), f.arg(1), f.arg(2)),
            );
            s.method(
                t,
                "void SetupTentacleMaterial(int alMatIndex, const tString&in asMat)",
                move |f| setup_material(&g(), me(f), f.arg(0), f.str_arg(1)),
            );
            s.method(t, "int GetTentacleEntityNum()", |f| {
                f.ret(tent(me(f), |t| t.tentacles.len() as i32).unwrap_or(0));
            });
            s.method(
                t,
                "void SetTentacleTransform(int alIndex, const cVector3f&in avPos, const cVector3f&in avRot)",
                |f| {
                    let (i, pos, rot): (i32, [f32; 3], [f32; 3]) = (f.arg(0), *f.rf(1), *f.rf(2));
                    let _ = tent(me(f), |t| {
                        if let Some(x) = usize::try_from(i).ok().and_then(|i| t.tentacles.get_mut(i)) {
                            *x = (pos, rot);
                        }
                    });
                },
            );
            s.method(
                t,
                "void SetupTentacleAnimation(const tString&in asAnim, float afSpeedMin, float afSpeedMax)",
                move |f| setup_animation(&g(), me(f), f.str_arg(0), f.arg(1), f.arg(2)),
            );
            s.method(
                t,
                "void SetTentacleAnimationSpeed(const tString&in asAnim, float afSpeed)",
                |f| {
                    let (n, v): (&[u8], f32) = (f.str_arg(0), f.arg(1));
                    let _ = tent(me(f), |t| {
                        if let Some(a) = t.anims.iter_mut().find(|a| a.name == n) {
                            a.speed = v;
                        }
                    });
                },
            );
            s.method(
                t,
                "void SetTentacleAnimationWeight(const tString&in asAnim, float afWeight)",
                |f| {
                    let (n, v): (&[u8], f32) = (f.str_arg(0), f.arg(1));
                    let _ = tent(me(f), |t| {
                        if let Some(a) = t.anims.iter_mut().find(|a| a.name == n) {
                            a.weight = v;
                        }
                    });
                },
            );
            s.method(
                t,
                "void SetTentacleMaterial(int alMatIndex, float afFadeTime)",
                |f| {
                    let (i, time): (i32, f32) = (f.arg(0), f.arg(1));
                    let _ = tent(me(f), |t| {
                        t.fades
                            .set_material(t.tentacles.len(), t.mats.len(), i, time)
                    });
                },
            );
            s.method(t, "cMeshEntity@ GetTentacleEntity(int alIndex)", |f| {
                let i: i32 = f.arg(0);
                let p = tent(me(f), |t| {
                    let ok = usize::try_from(i).ok().filter(|i| *i < t.tentacles.len());
                    ok.map(|i| group_of(&t.groups, i))
                })
                .flatten()
                .unwrap_or(0);
                ret_handle(f, p);
            });
        }
        if e.type_by_name(b"cPostEffect_Blur").is_some() {
            s.func(
                "cPostEffect_Blur@ cGraphics_CreatePostEffect_Blur()",
                move |f| make(f, BLUR),
            );
            s.method(
                "cPostEffect_Blur",
                "void SetParams(float afAmount)",
                move |f| {
                    set(&g(), me(f), &[f.arg(0)]);
                },
            );
            s.method(
                "cPostEffect_Blur",
                "void GetParams(float &out afAmount)",
                |f| {
                    out_params(f, me(f), 1);
                },
            );
        }
        // not in Bunker
        if e.type_by_name(b"cPostEffect_VideoDistortion").is_none() {
            return;
        }
        let vd = "cPostEffect_VideoDistortion";
        s.func(
            "cPostEffect_VideoDistortion@ cGraphics_CreatePostEffect_VideoDistortion()",
            move |f| make(f, 4),
        );
        s.method(
            vd,
            "void SetParams(float afAmount, float afRandomSeed, float afLineDensity, float afOffsetMul, const cVector2f&in avScreenOffset, const cVector2f&in avScreenBendAmount)",
            move |f| {
                let (o, b) = (*f.rf::<[f32; 2]>(4), *f.rf::<[f32; 2]>(5));
                set(&g(), me(f), &[f.arg(0), f.arg(1), f.arg(2), f.arg(3), o[0], o[1], b[0], b[1]]);
            },
        );
        s.method(
            vd,
            "void GetParams(float &out afAmount, float&out afRandomSeed, float&out afLineDensity, float&out afOffsetMul, cVector2f&out avScreenOffset, cVector2f&out avScreenBendAmount)",
            |f| {
                if let Some(p) = params::<8>(me(f)) {
                    for (i, v) in p.iter().take(4).enumerate() {
                        *f.rm::<f32>(i) = *v;
                    }
                    *f.rm::<[f32; 2]>(4) = [p[4], p[5]];
                    *f.rm::<[f32; 2]>(5) = [p[6], p[7]];
                }
            },
        );
    }

    pub(super) fn setup_entity(g: &Gx, shell: Ptr, file: &[u8], num: i32, groups: i32) {
        let Some(()) = tent(shell, |t| clear_entities(g, t)) else {
            return;
        };
        let def = g.text(op("XmlTentacle"), file, [0, 0]).text();
        let mut lines = def.split(|b| *b == b'\n');
        let head = lines.next().unwrap_or_default();
        let has_mesh = head.first() == Some(&b'1');
        let mesh = head.get(2..).unwrap_or_default().to_vec();
        let anims: Vec<(Vec<u8>, Vec<u8>, f32)> = lines
            .filter_map(|l| {
                let mut p = l.split(|b| *b == b'\t');
                let (name, file, speed) = (p.next()?, p.next()?, p.next()?);
                let bits = u32::from_str_radix(std::str::from_utf8(speed).ok()?, 16).ok()?;
                Some((name.to_vec(), file.to_vec(), f32::from_bits(bits)))
            })
            .collect();
        let groups = num.min(groups).max(1);
        let mut made = Vec::new();
        for _ in 0..if has_mesh { groups } else { 0 } {
            let ent = g.text(op("EntNew"), &mesh, [0, 0]).r as Ptr;
            if ent == 0 {
                break;
            }
            for (name, file, speed) in &anims {
                let text = [file.as_slice(), b"\x01", name.as_slice()].concat();
                let mut io = super::Io::new();
                io.p[0] = ent as u64;
                io.s = text.as_ptr();
                io.n = text.len();
                io.f[0] = *speed;
                g.0.call(op("EntAnim"), &mut io);
            }
            let mut io = super::Io::new();
            io.p[0] = ent as u64;
            io.i[..3].copy_from_slice(&[0, 1, 1]);
            g.0.call(op("EntPlay"), &mut io);
            made.push(ent);
        }
        let _ = tent(shell, |t| {
            if !made.is_empty() {
                t.tentacles = vec![([0.0; 3], [0.0; 3]); usize::try_from(num).unwrap_or(0)];
            }
            t.groups = made;
        });
    }

    fn setup_material(g: &Gx, shell: Ptr, idx: i32, file: &[u8]) {
        if !(0..=64).contains(&idx) {
            return;
        }
        let idx = idx as usize;
        let old = tent(shell, |t| {
            if idx >= t.mats.len() {
                t.mats.resize(idx + 1, 0);
            }
            t.mats.get(idx).copied().unwrap_or(0)
        });
        let Some(old) = old else {
            return;
        };
        if old != 0 {
            g.ptr(op("MatDestroy"), old);
        }
        let m = g.mat_create(file);
        let _ = tent(shell, |t| {
            if let Some(slot) = t.mats.get_mut(idx) {
                *slot = m;
            }
        });
    }

    fn setup_animation(g: &Gx, shell: Ptr, name: &[u8], min: f32, max: f32) {
        let Some((first, groups)) = tent(shell, |t| {
            let known = t.anims.iter().any(|a| a.name == name);
            let first = t.groups.first().copied().filter(|_| !known)?;
            Some((first, t.groups.clone()))
        })
        .flatten() else {
            return;
        };
        let mut io = super::Io::new();
        io.p[0] = first as u64;
        io.s = name.as_ptr();
        io.n = name.len();
        g.0.call(op("EntAnimIndex"), &mut io);
        let idx = io.i[0];
        if idx < 0 {
            return;
        }
        let _ = tent(shell, |t| {
            t.anims.push(super::Anim {
                name: name.to_vec(),
                idx,
                speed: 1.0,
                weight: 1.0,
            });
        });
        let speeds = super::postfx::tentacle_speeds(groups.len(), min, max);
        for (ent, speed) in groups.iter().zip(speeds) {
            let mut io = super::Io::new();
            io.p[0] = *ent as u64;
            io.i[..3].copy_from_slice(&[idx, 1, 0]);
            g.0.call(op("EntPlay"), &mut io);
            let mut io = super::Io::new();
            io.p[0] = *ent as u64;
            io.i[0] = idx;
            io.f[0] = speed;
            g.0.call(op("EntBaseSpeed"), &mut io);
        }
    }
}

#[cfg(test)]
#[path = "s8c_pfx_tests.rs"]
mod tests;

#[cfg(test)]
#[path = "s8c_replay_tests.rs"]
mod replay_tests;
