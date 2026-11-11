//! `cRendererDeferred` part B: SSAO, edge smoothing and the light program/shadow/sort decisions.
//!
//! Same plan pattern as `deferred`: each function becomes an op list the shim replays. Slot ids
//! (`slot`) and `kVar_*` ids (`var`) are mirrored in `RustRendererDeferred.h`.

use super::deferred::{
    DlOp, Feed, Op, Out, Plan, Source, State, c_max, fixed_with_arrays, st, state,
};
use crate::math::{Matrixf, matrix_mul, matrix_mul_3x3};
use crate::types::{Vector2l, Vector3f};

pub mod slot {
    pub mod fb {
        pub const LINEAR_DEPTH: i32 = 0;
        pub const SSAO: i32 = 1;
        pub const SSAO_BLUR: i32 = 2;
        pub const EDGE_LINEAR_DEPTH: i32 = 3;
        pub const COUNT: usize = 4;
    }

    pub mod tex {
        pub const GBUF_DEPTH: i32 = 0;
        pub const GBUF_NORMAL: i32 = 1;
        pub const LINEAR_DEPTH: i32 = 2;
        pub const SSAO_SCATTER: i32 = 3;
        pub const SSAO: i32 = 4;
        pub const SSAO_BLUR: i32 = 5;
        pub const EDGE_LINEAR: i32 = 6;
        pub const EDGE_TEMP: i32 = 7;
        pub const FALLOFF: i32 = 8;
        pub const GOBO: i32 = 9;
        pub const SHADOW: i32 = 10;
        pub const JITTER: i32 = 11;
        pub const SPOT_FALLOFF: i32 = 12;
        pub const GBUFFER0: i32 = 13;
        pub const GBUFFER_MAX: i32 = 5;
        pub const COUNT: usize = 18;
    }

    pub mod prog {
        pub const SSAO_UNPACK: i32 = 0;
        pub const SSAO_RENDER: i32 = 1;
        pub const SSAO_BLUR0: i32 = 2;
        pub const SSAO_BLUR1: i32 = 3;
        pub const EDGE_UNPACK: i32 = 4;
        pub const EDGE_RENDER: i32 = 5;
        pub const DEPTH_ONLY: i32 = 6;
        pub const LIGHT: i32 = 7;
        pub const COUNT: usize = 8;
    }
}

/// `kVar_*` ids of `RendererDeferred.cpp` used by this module.
pub mod var {
    pub const AV_LIGHT_POS: i32 = 0;
    pub const AV_LIGHT_COLOR: i32 = 1;
    pub const AF_INV_LIGHT_RADIUS: i32 = 2;
    pub const AF_NEG_FAR_PLANE: i32 = 3;
    pub const AF_ONE_MINUS_COS_HALF_SPOT_FOV: i32 = 4;
    pub const AV_LIGHT_FORWARD: i32 = 5;
    pub const A_MTX_SPOT_VIEW_PROJ: i32 = 6;
    pub const A_MTX_INV_VIEW_ROTATION: i32 = 7;
    pub const AV_SHADOW_MAP_OFFSET_MUL: i32 = 8;
    pub const AF_NEG_INV_FAR_PLANE: i32 = 9;
    pub const AF_FAR_PLANE: i32 = 10;
    pub const AF_SCATTER_LENGTH_MUL: i32 = 11;
    pub const AV_SCREEN_SIZE: i32 = 12;
    pub const AV_SCATTER_LENGTH_LIMITS: i32 = 13;
    pub const AV_FOG_START_AND_LENGTH: i32 = 14;
    pub const AV_FOG_COLOR: i32 = 15;
    pub const AF_FALLOFF_EXP: i32 = 20;
    pub const AF_DEPTH_DIFF_MUL: i32 = 21;
    pub const AF_SKIP_EDGE_LIMIT: i32 = 22;
    pub const A_MTX_LIGHT_VIEW_PROJ: i32 = 25;
    pub const AF_FALLOFF_POW: i32 = 26;
    pub const AF_SPOT_FALLOFF_POW: i32 = 27;
    pub const A_MTX_INV_VIEW: i32 = 32;
    pub const AV_MASK_CENTER: i32 = 33;
    pub const AV_MASK_EXTENT: i32 = 34;
    pub const AF_LIGHT_SOURCE_RADIUS: i32 = 42;
    pub const AV_LIGHT_UP: i32 = 43;
    pub const AV_LIGHT_RIGHT: i32 = 44;
    pub const AF_SPOT_NEAR_CLIP: i32 = 52;
    pub const AF_TRANSLUCENCY_SCALE: i32 = 69;
    pub const AF_FOG_FALLOFF_EXP: i32 = 77;
    pub const AV_LIGHT_POS_SCALE: i32 = 94;
    pub const AV_SHADOW_PARAMS: i32 = 110;
    pub const AV_SHADOW_MAP_SIZE: i32 = 111;
}

pub mod feature {
    pub const LIGHT_SHAPES: u32 = 1 << 0;
    pub const SPECULAR: u32 = 1 << 2;
    pub const SPOT_LIGHT: u32 = 1 << 3;
    pub const GOBO: u32 = 1 << 4;
    pub const DIVIDE_IN_FRAG: u32 = 1 << 5;
    pub const SHADOW_MAP: u32 = 1 << 6;
    pub const BOX_MASK: u32 = 1 << 7;
    pub const GOBO_SPECULAR: u32 = 1 << 8;
    pub const GOBO_TYPE_SPECULAR: u32 = 1 << 9;
    pub const TRANSLUCENCY: u32 = 1 << 10;
    pub const UNDERWATER_FOG: u32 = 1 << 11;
}

pub mod kind {
    pub const POINT: i32 = 0;
    pub const SPOT: i32 = 1;
    pub const BOX: i32 = 2;
}

pub mod res {
    pub const LOW: i32 = 0;
    pub const MEDIUM: i32 = 1;
    pub const HIGH: i32 = 2;
}

/// `eDeferredLightList_*`.
pub mod list {
    pub const STENCIL_BACK_SCREEN_QUAD: usize = 0;
    pub const STENCIL_FRONT_RENDER_BACK: usize = 1;
    pub const RENDER_BACK: usize = 2;
    pub const BATCHES: usize = 3;
    pub const BOX_STENCIL_FRONT_RENDER_BACK: usize = 4;
    pub const BOX_RENDER_BACK: usize = 5;
    pub const COUNT: usize = 6;
}

/// `SetVertexBuffer` targets of `SetShape`: a sphere quality, the spot pyramid, or none.
pub mod shape {
    pub const NONE: i32 = -1;
    pub const SPHERE_MEDIUM: i32 = 1;
    pub const PYRAMID: i32 = 3;
}

pub const MAX_GBUFFER_TEXTURES: i32 = slot::tex::GBUFFER_MAX;

pub(super) const RADIUS_MUL_MEDIUM: f32 = 1.12;
pub(super) const RADIUS_MUL_LOW: f32 = 1.2;

/// An `iLight` as the renderer reads it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct LightIn {
    pub kind: i32,
    pub radius: f32,
    pub brightness: f32,
    pub falloff_pow: f32,
    pub spot_falloff_pow: f32,
    pub translucency: f32,
    pub diffuse: [f32; 4],
    pub cast_specular: bool,
    pub cast_diffuse: bool,
    pub cast_shadows: bool,
    pub has_gobo: bool,
    pub gobo_specular: bool,
    pub has_mask_box: bool,
    pub shadow_res: i32,
    pub shadow_blur: f32,
    pub mask_center: Vector3f,
    pub mask_size: Vector3f,
    pub world: Matrixf,
    pub world_pos: Vector3f,
    pub bv_min: Vector3f,
    pub bv_max: Vector3f,
    pub bv_center: Vector3f,
    pub tan_half_fov: f32,
    pub aspect: f32,
    pub cos_half_fov: f32,
    pub near_clip: f32,
    pub view_proj: Matrixf,
    pub box_size: Vector3f,
    pub box_prio: i32,
    pub falloff_map: u64,
    pub gobo: u64,
    pub spot_falloff: u64,
    pub id: u64,
}

/// What `SetupLightsAndRenderQueries` decides for one light (`cDeferredLight`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LightOut {
    pub render: Matrixf,
    pub transform: Matrixf,
    pub clip: crate::math::Rect2l,
    pub area: i32,
    pub inside_near_plane: bool,
    pub cast_shadows: bool,
    pub shadow_res: i32,
}

#[derive(Clone, Debug, Default)]
pub struct LightResults {
    pub lights: Vec<LightOut>,
    pub inv_view: Matrixf,
    pub lists: [Vec<u32>; list::COUNT],
    pub rendered: i32,
}

/// Far plane geometry of `SetupRenderVariables`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FarIn {
    pub plane: f32,
    pub top: f32,
    pub bottom: f32,
    pub right: f32,
    pub left: f32,
}

impl FarIn {
    fn quad_pos(&self) -> [f32; 3] {
        [self.left, self.bottom, -self.plane]
    }

    fn quad_size(&self) -> [f32; 2] {
        [self.right * 2.0, self.top * 2.0]
    }
}

fn f2(x: i32, y: i32) -> [f32; 2] {
    [x as f32, y as f32]
}

/// Render states and flat projection both fullscreen passes start with.
fn quad_pass_setup(ops: &mut Vec<Op>, f: &FarIn) {
    ops.extend([
        state(State::Channel, st::CHANNEL_RGBA),
        Op::TexRangeNull { first: 1 },
        state(State::Alpha, st::ALPHA_SOLID),
        state(State::Blend, st::BLEND_NONE),
        state(State::DepthWrite, 0),
        state(State::DepthTest, 0),
        Op::FlatProjectionMinMax {
            min: [f.left, f.bottom, -f.plane * 1.5],
            max: [f.right, f.top, f.plane * 1.5],
        },
    ]);
}

fn quad(f: &FarIn, uv_max: [f32; 2]) -> Op {
    Op::QuadAt {
        pos: f.quad_pos(),
        size: f.quad_size(),
        min: [0.0, 0.0],
        max: uv_max,
    }
}

fn float(prog: i32, id: i32, v: f32) -> Op {
    Op::Float { prog, id, v }
}

fn tex(unit: i32, tex: i32) -> Op {
    Op::Tex { unit, tex }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SsaoIn {
    pub loaded: bool,
    pub active: bool,
    pub in_box_light: bool,
    pub box_lists_empty: bool,
    pub gbuffer_64bit: bool,
    pub on_color_buffer: bool,
    pub unpack_ok: bool,
    pub render_ok: bool,
    pub blur_ok: [bool; 2],
    pub far: FarIn,
    pub gbuffer_depth_size: Vector2l,
    pub ssao_size: Vector2l,
    pub scatter_mul: f32,
    pub scatter_min: f32,
    pub scatter_max: f32,
    pub depth_diff_mul: f32,
    pub skip_edge: f32,
}

/// `cRendererDeferred::RenderSSAO`; `None` when the pass is skipped.
pub fn plan_ssao(i: &SsaoIn) -> Option<Plan> {
    use slot::{fb, prog, tex as t};
    if !i.loaded || !i.active {
        return None;
    }
    if i.in_box_light && i.box_lists_empty {
        return None;
    }
    let f = &i.far;
    let depth = f2(i.gbuffer_depth_size.x, i.gbuffer_depth_size.y);
    let size = f2(i.ssao_size.x, i.ssao_size.y);
    let mut ops = Vec::new();
    quad_pass_setup(&mut ops, f);

    ops.push(Op::SetFb {
        fb: fb::LINEAR_DEPTH,
        use_pos: false,
    });
    if i.gbuffer_64bit && i.unpack_ok {
        ops.push(float(
            prog::SSAO_UNPACK,
            var::AF_NEG_INV_FAR_PLANE,
            -1.0 / f.plane,
        ));
    }
    ops.extend([
        Op::Program {
            prog: prog::SSAO_UNPACK,
        },
        tex(0, t::GBUF_DEPTH),
        quad(f, depth),
    ]);

    ops.extend([
        Op::SetFb {
            fb: fb::SSAO,
            use_pos: false,
        },
        tex(0, t::LINEAR_DEPTH),
        tex(1, t::SSAO_SCATTER),
    ]);
    if i.render_ok {
        let p = prog::SSAO_RENDER;
        ops.extend([
            float(p, var::AF_FAR_PLANE, f.plane),
            float(p, var::AF_SCATTER_LENGTH_MUL, i.scatter_mul),
            Op::Vec2 {
                prog: p,
                id: var::AV_SCATTER_LENGTH_LIMITS,
                v: [i.scatter_min, i.scatter_max],
            },
            Op::Vec2 {
                prog: p,
                id: var::AV_SCREEN_SIZE,
                v: size,
            },
            float(p, var::AF_DEPTH_DIFF_MUL, i.depth_diff_mul),
            float(p, var::AF_SKIP_EDGE_LIMIT, i.skip_edge),
        ]);
    }
    ops.extend([
        Op::Program {
            prog: prog::SSAO_RENDER,
        },
        quad(f, size),
    ]);

    for (pass, (dst, src, p)) in [
        (fb::SSAO_BLUR, t::SSAO, prog::SSAO_BLUR0),
        (fb::SSAO, t::SSAO_BLUR, prog::SSAO_BLUR1),
    ]
    .into_iter()
    .enumerate()
    {
        ops.extend([
            Op::SetFb {
                fb: dst,
                use_pos: false,
            },
            tex(0, src),
            tex(1, t::LINEAR_DEPTH),
        ]);
        if i.blur_ok.get(pass).copied().unwrap_or(false) {
            ops.push(float(p, var::AF_FAR_PLANE, f.plane));
        }
        ops.extend([Op::Program { prog: p }, quad(f, size)]);
    }

    if i.on_color_buffer {
        ops.extend([
            state(State::Channel, st::CHANNEL_RGB),
            Op::SetGBuffer {
                comp: super::deferred::comp::COLOR,
            },
            Op::FlatProjection,
            Op::Program {
                prog: super::deferred::NONE,
            },
            state(State::Blend, st::BLEND_MUL),
            tex(0, t::SSAO),
            tex(1, super::deferred::NONE),
            Op::Quad {
                min: [0.0, 0.0],
                max: size,
            },
            state(State::Channel, st::CHANNEL_RGBA),
        ]);
    }
    ops.push(Op::NormalFrustumProjection);
    Some(fixed_with_arrays(ops, Vec::new()))
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EdgeIn {
    pub loaded: bool,
    pub use_edge_smooth: bool,
    pub gbuffer_64bit: bool,
    pub render_ok: bool,
    pub far: FarIn,
    pub screen: Vector2l,
    pub screen_f: [f32; 2],
}

/// `cRendererDeferred::RenderEdgeSmooth`; `None` when the pass is skipped.
pub fn plan_edge_smooth(i: &EdgeIn) -> Option<Plan> {
    use slot::{fb, prog, tex as t};
    if !i.loaded || !i.use_edge_smooth {
        return None;
    }
    let f = &i.far;
    let mut ops = Vec::new();
    quad_pass_setup(&mut ops, f);
    ops.push(Op::SetFb {
        fb: fb::EDGE_LINEAR_DEPTH,
        use_pos: false,
    });
    if i.gbuffer_64bit {
        ops.push(float(
            prog::EDGE_UNPACK,
            var::AF_NEG_INV_FAR_PLANE,
            -1.0 / f.plane,
        ));
    }
    ops.extend([
        Op::Program {
            prog: prog::EDGE_UNPACK,
        },
        tex(0, t::GBUF_DEPTH),
        quad(f, i.screen_f),
        Op::SetAccumBuffer,
        Op::CopyFbToTex {
            tex: t::EDGE_TEMP,
            size: i.screen,
        },
        tex(0, t::EDGE_TEMP),
        tex(1, t::EDGE_LINEAR),
        tex(2, t::GBUF_NORMAL),
        Op::Program {
            prog: prog::EDGE_RENDER,
        },
    ]);
    if i.render_ok {
        ops.push(float(prog::EDGE_RENDER, var::AF_FAR_PLANE, f.plane));
    }
    ops.extend([quad(f, i.screen_f), Op::NormalFrustumProjection]);
    Some(fixed_with_arrays(ops, Vec::new()))
}

pub(super) fn linear_light_color(c: [f32; 4], hdr: bool) -> [f32; 4] {
    if hdr {
        [c[0] * c[0], c[1] * c[1], c[2] * c[2], c[3]]
    } else {
        c
    }
}

/// `LightShaderColor`: alpha 0 means no specular, negative alpha no diffuse.
fn light_shader_color(l: &LightIn, hdr: bool) -> [f32; 4] {
    let mut c = linear_light_color(l.diffuse, hdr);
    if !l.cast_specular {
        c[3] = 0.0;
    }
    if !l.cast_diffuse {
        c[3] = -c[3];
    }
    c
}

pub(super) fn underwater_fog_color(c: [f32; 4]) -> [f32; 4] {
    // the squares are reused below, so gcc keeps them unfused
    let inv = 1.0 / (c[0] * c[0] + c[1] * c[1] + c[2] * c[2] + 1e-13);
    [
        c[0] * c[0] * inv,
        c[1] * c[1] * inv,
        c[2] * c[2] * inv,
        c[3],
    ]
}

/// A light plus the `cDeferredLight` and renderer state `SetupLightProgramVariables` reads.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct LightState {
    pub light: LightIn,
    pub view_render: Matrixf,
    pub view_transform: Matrixf,
    pub inv_view: Matrixf,
    pub cast_shadows: bool,
    pub shadow_tex: bool,
    pub shadow_size: Vector2l,
    pub jitter: bool,
    pub hdr: bool,
    pub underwater: bool,
    pub fog_color: [f32; 4],
    pub fog_start: f32,
    pub fog_end: f32,
    pub fog_falloff_exp: f32,
    pub distance_fade: f32,
    pub far_plane: f32,
    pub depth_texture_idx: i32,
    pub program_ok: bool,
}

struct Builder {
    ops: Vec<Op>,
    arrays: Vec<Vec<f32>>,
}

impl Builder {
    fn matrix(&mut self, prog: i32, id: i32, m: &Matrixf) {
        let arr = m.m.iter().flatten().copied().collect::<Vec<_>>();
        self.arrays.push(arr);
        self.ops.push(Op::Matrix {
            prog,
            id,
            arr: (self.arrays.len() - 1) as i32,
        });
    }

    fn vec3(&mut self, prog: i32, id: i32, v: Vector3f) {
        self.ops.push(vec3_op(prog, id, v));
    }
}

fn vec3_op(prog: i32, id: i32, v: Vector3f) -> Op {
    Op::Vec3 {
        prog,
        id,
        v: [v.x, v.y, v.z],
    }
}

fn rotation(m: &Matrixf) -> Matrixf {
    let a = &m.m;
    let r = |i: usize| -> [f32; 4] {
        let row = a.get(i).copied().unwrap_or([0.0; 4]);
        [row[0], row[1], row[2], 0.0]
    };
    Matrixf {
        m: [r(0), r(1), r(2), [0.0, 0.0, 0.0, 1.0]],
    }
}

fn translation(m: &Matrixf) -> Vector3f {
    let a = &m.m;
    let c = |i: usize| a.get(i).map_or(0.0, |r| r[3]);
    Vector3f {
        x: c(0),
        y: c(1),
        z: c(2),
    }
}

fn v3(x: f32, y: f32, z: f32) -> Vector3f {
    Vector3f { x, y, z }
}

/// `cRendererDeferred::SetupLightProgramVariables`.
pub fn plan_light_variables(s: &LightState) -> Plan {
    let l = &s.light;
    let p = slot::prog::LIGHT;
    let mut b = Builder {
        ops: Vec::new(),
        arrays: Vec::new(),
    };
    if !s.program_ok {
        return fixed_with_arrays(b.ops, b.arrays);
    }
    let t = translation(&s.view_render);
    b.vec3(p, var::AV_LIGHT_POS, t);
    b.ops.push(Op::Vec4 {
        prog: p,
        id: var::AV_LIGHT_POS_SCALE,
        v: [t.x, t.y, t.z, 1.0 / l.radius],
    });
    let mut color = light_shader_color(l, s.hdr);
    let brightness = l.brightness * s.distance_fade;
    for c in color.iter_mut().take(3) {
        *c *= brightness;
    }
    b.ops.extend([
        Op::Vec4 {
            prog: p,
            id: var::AV_LIGHT_COLOR,
            v: color,
        },
        float(p, var::AF_INV_LIGHT_RADIUS, 1.0 / l.radius),
        float(p, var::AF_FALLOFF_POW, l.falloff_pow * 2.0),
        float(p, var::AF_SPOT_FALLOFF_POW, l.spot_falloff_pow * 2.0),
        float(
            p,
            var::AF_TRANSLUCENCY_SCALE,
            l.translucency * l.translucency * 0.5,
        ),
    ]);
    if s.underwater {
        b.ops.extend([
            Op::Vec4 {
                prog: p,
                id: var::AV_FOG_COLOR,
                v: underwater_fog_color(s.fog_color),
            },
            Op::Vec2 {
                prog: p,
                id: var::AV_FOG_START_AND_LENGTH,
                v: [s.fog_start, s.fog_end - s.fog_start],
            },
            float(p, var::AF_FOG_FALLOFF_EXP, s.fog_falloff_exp),
        ]);
    }

    if l.kind == kind::POINT {
        if l.has_gobo {
            let m = matrix_mul(&l.world, &s.inv_view);
            b.matrix(p, var::A_MTX_INV_VIEW_ROTATION, &rotation(&m));
            b.ops.push(float(p, var::AF_LIGHT_SOURCE_RADIUS, 0.05));
        }
    } else if l.kind == kind::SPOT {
        let fwd = matrix_mul_3x3(&s.view_transform, v3(0.0, 0.0, 1.0));
        b.vec3(p, var::AV_LIGHT_FORWARD, fwd);
        b.ops.push(float(
            p,
            var::AF_ONE_MINUS_COS_HALF_SPOT_FOV,
            1.0 - l.cos_half_fov,
        ));
        if s.shadow_tex {
            let (w, h) = (s.shadow_size.x as f32, s.shadow_size.y as f32);
            if s.jitter {
                b.ops.push(Op::Vec2 {
                    prog: p,
                    id: var::AV_SHADOW_MAP_OFFSET_MUL,
                    v: [(1.0 / w) * l.shadow_blur, (1.0 / h) * l.shadow_blur],
                });
            }
            let (near, far) = (l.near_clip, l.radius);
            b.ops.extend([
                Op::Vec4 {
                    prog: p,
                    id: var::AV_SHADOW_PARAMS,
                    v: [
                        near,
                        far,
                        (w + 128.0) * 80.0 / 1152.0 * c_max(far - near, 0.0).sqrt() / near,
                        0.0,
                    ],
                },
                Op::Vec2 {
                    prog: p,
                    id: var::AV_SHADOW_MAP_SIZE,
                    v: [w, h],
                },
                Op::Dl(DlOp::ClearShadowTexture),
            ]);
        }
        let m = matrix_mul(&l.view_proj, &s.inv_view);
        b.matrix(p, var::A_MTX_LIGHT_VIEW_PROJ, &m);
        b.ops.push(float(p, var::AF_SPOT_NEAR_CLIP, l.near_clip));
        if l.has_gobo && l.gobo_specular {
            let h = l.tan_half_fov * l.near_clip;
            b.ops
                .push(float(p, var::AF_LIGHT_SOURCE_RADIUS, l.near_clip));
            let up = matrix_mul_3x3(&s.view_transform, v3(0.0, 1.0 / h, 0.0));
            let right = matrix_mul_3x3(&s.view_transform, v3(1.0 / (h * l.aspect), 0.0, 0.0));
            b.vec3(p, var::AV_LIGHT_UP, up);
            b.vec3(p, var::AV_LIGHT_RIGHT, right);
        }
        if l.has_gobo || s.cast_shadows {
            b.matrix(p, var::A_MTX_SPOT_VIEW_PROJ, &m);
        }
    }
    fixed_with_arrays(b.ops, b.arrays)
}

/// `cRendererDeferred::SetupProgramAndTextures`: `extra_flags` are the caller's program features.
pub fn plan_light_program(s: &LightState, extra_flags: u32, mode: i32) -> Plan {
    use feature::*;
    let l = &s.light;
    let mut flags = extra_flags;
    if l.diffuse[3] > 0.0 && l.cast_specular {
        flags |= SPECULAR;
    }
    if l.has_gobo {
        flags |= GOBO;
    }
    if l.has_mask_box {
        flags |= BOX_MASK;
    }
    if l.translucency > 0.0 {
        flags |= TRANSLUCENCY;
    }
    if s.underwater {
        flags |= UNDERWATER_FOG;
    }
    if l.has_gobo && l.gobo_specular {
        flags |= GOBO_SPECULAR | GOBO_TYPE_SPECULAR;
    }
    if l.kind == kind::SPOT {
        flags |= SPOT_LIGHT;
        if flags & LIGHT_SHAPES != 0 {
            flags |= DIVIDE_IN_FRAG;
        }
        if s.cast_shadows && s.shadow_tex {
            flags |= SHADOW_MAP;
        }
    }

    Plan::new(LightProgram {
        s: *s,
        flags,
        mode,
        stage: 0,
    })
}

struct LightProgram {
    s: LightState,
    flags: u32,
    mode: i32,
    stage: u8,
}

impl Source for LightProgram {
    fn refill(&mut self, out: &mut Out<'_>, feed: Option<Feed>) -> bool {
        let (s, l, p) = (&self.s, &self.s.light, slot::prog::LIGHT);
        if self.stage == 0 {
            self.stage = 1;
            out.push(Op::GenProgram {
                slot: p,
                mode: self.mode,
                flags: self.flags,
            });
            return true;
        }
        out.push(Op::Program { prog: p });
        if feed.is_some_and(Feed::flag) {
            out.extend([
                float(p, var::AF_NEG_FAR_PLANE, -s.far_plane),
                float(p, var::AF_FAR_PLANE, s.far_plane),
                Op::ScreenPositionVars { prog: p },
                Op::InvViewMatrix {
                    prog: p,
                    id: var::A_MTX_INV_VIEW,
                },
            ]);
            if l.has_mask_box {
                out.extend([
                    vec3_op(p, var::AV_MASK_CENTER, l.mask_center),
                    vec3_op(p, var::AV_MASK_EXTENT, l.mask_size),
                ]);
            }
        }

        use slot::tex as t;
        if s.depth_texture_idx != 4 {
            out.push(tex(4, t::FALLOFF));
        }
        if l.has_gobo {
            out.push(tex(5, t::GOBO));
        }
        if l.kind == kind::SPOT {
            if s.shadow_tex {
                out.push(tex(6, t::SHADOW));
                if s.jitter {
                    out.push(tex(7, t::JITTER));
                }
            }
            if !l.has_gobo {
                out.push(tex(5, t::SPOT_FALLOFF));
            }
        }
        false
    }
}

/// `cRendererDeferred::RenderLightShadowMap`; the shadow update is answered with a flag.
pub fn plan_light_shadow_map(gbuffer_textures: i32) -> Plan {
    use super::deferred::Geom;
    struct Shadow {
        n: i32,
        stage: u8,
    }
    impl Source for Shadow {
        fn refill(&mut self, out: &mut Out<'_>, feed: Option<Feed>) -> bool {
            if self.stage == 0 {
                self.stage = 1;
                out.extend([
                    state(State::DepthFunc, st::FUNC_LESS_OR_EQUAL),
                    Op::Dl(DlOp::ShadowData),
                    Op::AskGeom {
                        q: Geom::ShadowNeedsUpdate,
                        idx: 0,
                        f: [0.0; 4],
                    },
                ]);
                return true;
            }
            if feed.is_some_and(Feed::flag) {
                out.extend([Op::Dl(DlOp::RenderShadowMap), Op::SetAccumBuffer]);
            }
            for i in 0..self.n.clamp(0, MAX_GBUFFER_TEXTURES) {
                out.push(tex(i, slot::tex::GBUFFER0 + i));
            }
            out.extend([
                state(State::DepthFunc, st::FUNC_GREATER_OR_EQUAL),
                state(State::DepthWrite, 0),
                state(State::Channel, st::CHANNEL_RGBA),
                state(State::Blend, st::BLEND_ADD),
                state(State::Alpha, st::ALPHA_SOLID),
            ]);
            false
        }
    }
    Plan::new(Shadow {
        n: gbuffer_textures,
        stage: 0,
    })
}

/// `cRendererDeferred::GetLightShape`.
pub fn light_shape(kind: i32, quality: i32) -> i32 {
    match kind {
        kind::POINT => quality,
        kind::SPOT => shape::PYRAMID,
        _ => shape::NONE,
    }
}

/// `iRenderer::GetShadowMapResolution`.
pub fn shadow_map_resolution(wanted: i32, max: i32) -> i32 {
    if max == res::HIGH {
        wanted
    } else if max == res::MEDIUM {
        if wanted == res::HIGH {
            res::MEDIUM
        } else {
            wanted
        }
    } else {
        res::LOW
    }
}

/// Fields the light list sort functions compare.
#[derive(Clone, Copy, Debug, Default)]
pub struct SortKey {
    pub kind: i32,
    pub specular: bool,
    pub inside_near: bool,
    pub gobo: u64,
    pub falloff: u64,
    pub spot_falloff: u64,
    pub id: u64,
    pub box_prio: i32,
}

impl SortKey {
    pub fn of(l: &LightIn, inside_near: bool) -> Self {
        Self {
            kind: l.kind,
            specular: l.diffuse[3] > 0.0,
            inside_near,
            gobo: l.gobo,
            falloff: l.falloff_map,
            spot_falloff: l.spot_falloff,
            id: l.id,
            box_prio: l.box_prio,
        }
    }
}

/// `SortFunc_Box`.
pub fn box_less(a: &SortKey, b: &SortKey) -> bool {
    if a.box_prio != b.box_prio {
        return a.box_prio < b.box_prio;
    }
    a.id < b.id
}

/// `SortFunc_Default`.
pub fn default_less(a: &SortKey, b: &SortKey) -> bool {
    if a.kind != b.kind {
        return a.kind < b.kind;
    }
    if a.specular != b.specular {
        return !a.specular & b.specular;
    }
    if a.kind == kind::POINT {
        return !a.inside_near & b.inside_near;
    }
    if a.gobo != b.gobo {
        return a.gobo < b.gobo;
    }
    if a.falloff != b.falloff {
        return a.falloff < b.falloff;
    }
    if a.kind == kind::SPOT && a.spot_falloff != b.spot_falloff {
        return a.spot_falloff < b.spot_falloff;
    }
    a.id < b.id
}

/// The comparison `vLightSortFunctions[list]` applies.
pub fn list_less(list: usize, a: &SortKey, b: &SortKey) -> bool {
    if list == self::list::BOX_STENCIL_FRONT_RENDER_BACK || list == self::list::BOX_RENDER_BACK {
        box_less(a, b)
    } else {
        default_less(a, b)
    }
}
