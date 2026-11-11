//! `cRendererDeferred` part D: fog, depth of field, night vision and the HPL3 SSAO upsample.
//!
//! Same plan pattern as `deferred`. Plans here are written as sequences whose `Ask` items carry the
//! continuation that consumes the shim's answer; `P4` ops (`x` ids) are the renderer calls the shim
//! performs on its own objects.

use std::collections::VecDeque;
use std::hint::black_box;

use super::deferred::{Feed, NONE, Op, Out, P4, Plan, Query, Source, State, c_max, st, state};
use crate::math::Matrixf;

/// `P4` ids.
pub mod x {
    pub const TEX_GBUFFER: i32 = 0;
    pub const RED_FROM_ALPHA: i32 = 1;
    pub const FILTER_SAVE: i32 = 2;
    pub const FILTER_NEAREST: i32 = 3;
    pub const FILTER_RESTORE: i32 = 4;
    pub const DRAW_ACCUM_QUAD: i32 = 5;
    pub const PASS_BEGIN: i32 = 6;
    pub const PASS_END: i32 = 7;
    pub const QUAD_PLAIN: i32 = 8;
    pub const MATRIX_NULL: i32 = 9;
    pub const CULL_ACTIVE: i32 = 10;
    pub const GEN_FOG_PROGRAM: i32 = 11;
    pub const ENSURE_FOG_NOISE: i32 = 12;
    pub const INT_ASK: i32 = 13;
    pub const FOG_AREA_MATRIX: i32 = 14;
    pub const SETUP_FOG_DATA: i32 = 15;
    pub const CLEAR_FOG_DATA: i32 = 16;
    pub const ASK_FOGS: i32 = 17;
    pub const LOG: i32 = 18;
    pub const DIR_SHADOW_CREATE: i32 = 19;
    pub const ASK_DIR_SHADOW_SIZE: i32 = 20;
    pub const POLYGON_OFFSET_ACTIVE: i32 = 21;
    pub const DEPTH_CLAMP_ACTIVE: i32 = 22;
    pub const POLYGON_OFFSET: i32 = 23;
    pub const OCCLUSION_PLANES: i32 = 24;
    pub const ALPHA_LIMIT_DEFAULT: i32 = 25;
    pub const DIR_SHADOW_FB: i32 = 26;
    pub const DIR_FRUSTUM: i32 = 27;
    pub const DIR_CASTERS: i32 = 28;
    pub const DIR_SET_FB: i32 = 29;
    pub const DIR_RENDER: i32 = 30;
    pub const TEX_BUFFER: i32 = 31;
}

pub mod pass {
    pub const DEPTH_OF_FIELD: i32 = 0;
    pub const FULLSCREEN_FOG: i32 = 1;
    pub const FOG: i32 = 2;
}

/// `kVar_*` ids of `RendererDeferred.cpp` used by the part D plans.
pub mod v4 {
    // generated from the shim's #defines
    pub const AV_FOG_SETTINGS: i32 = 95;
    pub const AV_SECOND_FOG_SETTINGS: i32 = 96;
    pub const AV_CAMERA_POS: i32 = 97;
    pub const AL_FOG_TYPE: i32 = 98;
    pub const AV_SECOND_FOG_COLOR: i32 = 74;
    pub const AV_SECOND_FOG_START_AND_LENGTH: i32 = 75;
    pub const AF_SECOND_FALLOFF_EXP: i32 = 76;
    pub const AF_NEG_FAR_PLANE: i32 = 3;
    pub const AV_FOG_START_AND_LENGTH: i32 = 14;
    pub const AV_FOG_COLOR: i32 = 15;
    pub const AF_FALLOFF_EXP: i32 = 20;
    pub const A_MTX_NOISE: i32 = 70;
    pub const AF_INV_NOISE_SCALE: i32 = 71;
    pub const AF_NOISE_STRENGTH: i32 = 72;
    pub const AV_SKYBOX_COLOR: i32 = 73;
    pub const A_MTX_SKYBOX: i32 = 99;
    pub const AF_SKYBOX_MIP_NUM: i32 = 100;
    pub const AV_RAY_CAST_START: i32 = 16;
    pub const A_MTX_BOX_INV_VIEW_MODEL_ROTATION: i32 = 17;
    pub const AV_NEG_PLANE_DIST_NEG: i32 = 18;
    pub const AV_NEG_PLANE_DIST_POS: i32 = 19;
    pub const AF_FAR_PLANE: i32 = 10;
    pub const AV_LIGHT_COLOR: i32 = 1;
    pub const AF_FALLOFF_POW: i32 = 26;
    pub const AF_MAX_DISTANCE: i32 = 101;
    pub const AF_SIZE_DIV: i32 = 67;
    pub const AF_POWER: i32 = 68;
    pub const AV_FOCUS_START_END: i32 = 53;
    pub const AV_OFFSET_MUL: i32 = 54;
}

/// The `cMath` calls the original makes out of line; inlining them here with constant arguments
/// folds NaN signs differently.
pub(super) mod cmath {
    use crate::math::{Matrixf, Planef, Vector3f};

    #[inline(never)]
    pub fn matrix_mul(a: &Matrixf, b: &Matrixf) -> Matrixf {
        crate::math::matrix_mul(a, b)
    }

    #[inline(never)]
    pub fn matrix_inverse(a: &Matrixf) -> Matrixf {
        crate::math::matrix_inverse(a)
    }

    #[inline(never)]
    pub fn matrix_mul_vec(a: &Matrixf, v: Vector3f) -> Vector3f {
        crate::math::matrix_mul_vec(a, v)
    }

    #[inline(never)]
    pub fn plane_dist(p: &Planef, v: Vector3f) -> f32 {
        crate::math::plane_dist(p, v)
    }
}

type Cont = Box<dyn FnOnce(Feed) -> Vec<Item>>;

pub(super) enum Item {
    Op(Op),
    Arr(Vec<f32>, Box<dyn FnOnce(i32) -> Op>),
    Ask(Op, Cont),
}

pub(super) struct Seq {
    items: VecDeque<Item>,
    pending: Option<Cont>,
}

impl Seq {
    pub(super) fn plan(items: Vec<Item>) -> Plan {
        Plan::new(Self {
            items: items.into(),
            pending: None,
        })
    }
}

impl Source for Seq {
    fn refill(&mut self, out: &mut Out<'_>, feed: Option<Feed>) -> bool {
        if let Some(c) = self.pending.take() {
            for it in c(feed.unwrap_or(Feed::Bool(false))).into_iter().rev() {
                self.items.push_front(it);
            }
        }
        while let Some(it) = self.items.pop_front() {
            match it {
                Item::Op(op) => out.push(op),
                Item::Arr(v, f) => {
                    let (idx, _) = out.array(v);
                    out.push(f(idx));
                }
                Item::Ask(op, c) => {
                    out.push(op);
                    self.pending = Some(c);
                    return true;
                }
            }
        }
        false
    }
}

pub(super) fn o(op: Op) -> Item {
    Item::Op(op)
}

pub(super) fn ops(v: impl IntoIterator<Item = Op>) -> Vec<Item> {
    v.into_iter().map(Item::Op).collect()
}

pub(super) fn p4(id: i32, a: i32, b: i32) -> Op {
    Op::P4(P4 {
        id,
        a,
        b,
        ..P4::default()
    })
}

pub(super) fn p4f(id: i32, a: i32, b: i32, c: i32, d: i32, f: [f32; 8]) -> Op {
    Op::P4(P4 { id, a, b, c, d, f })
}

pub(super) fn ask(op: Op, c: impl FnOnce(Feed) -> Vec<Item> + 'static) -> Item {
    Item::Ask(op, Box::new(c))
}

pub(super) fn mat(prog: i32, id: i32, m: &Matrixf) -> Item {
    Item::Arr(
        m.m.iter().flatten().copied().collect(),
        Box::new(move |arr| Op::Matrix { prog, id, arr }),
    )
}

fn s(which: State, val: i32) -> Op {
    state(which, val)
}

fn float(prog: i32, id: i32, v: f32) -> Op {
    Op::Float { prog, id, v }
}

fn vec4(prog: i32, id: i32, v: [f32; 4]) -> Op {
    Op::Vec4 { prog, id, v }
}

fn tex(unit: i32, tex: i32) -> Op {
    Op::Tex { unit, tex }
}

fn quad(min: [f32; 2], max: [f32; 2]) -> Op {
    Op::Quad { min, max }
}

pub(super) fn pass_begin(id: i32) -> Op {
    p4(x::PASS_BEGIN, id, 0)
}

pub(super) fn pass_end(id: i32) -> Op {
    p4(x::PASS_END, id, 0)
}

/// `GetFogRenderColor`.
pub(super) fn fog_render_color(hdr: bool, c: [f32; 4], brightness: f32) -> [f32; 4] {
    if hdr {
        [
            c[0] * c[0] * brightness,
            c[1] * c[1] * brightness,
            c[2] * c[2] * brightness,
            c[3],
        ]
    } else {
        c
    }
}

/// `SetFogDepthTexture`.
pub(super) fn fog_depth(
    depth_in_normal_alpha: bool,
    linear_idx: i32,
    bind: bool,
    unit: i32,
) -> Vec<Op> {
    if !depth_in_normal_alpha {
        return if bind {
            vec![p4(x::TEX_GBUFFER, unit, linear_idx)]
        } else {
            Vec::new()
        };
    }
    let mut v = vec![p4(x::RED_FROM_ALPHA, i32::from(bind), 0)];
    if bind {
        v.push(p4(x::TEX_GBUFFER, unit, 1));
    }
    v
}

/// `SetFogDepthTexture` as a plan.
pub fn plan_fog_depth(depth_in_normal_alpha: bool, linear_idx: i32, bind: bool, unit: i32) -> Plan {
    Seq::plan(ops(fog_depth(
        depth_in_normal_alpha,
        linear_idx,
        bind,
        unit,
    )))
}

pub mod nslot {
    pub const NV_PROG: i32 = 0;
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct NightIn {
    pub program_ok: i32,
    pub visible: i32,
    pub color: [f32; 4],
    pub brightness: f32,
    pub falloff: f32,
    pub radius_max: f32,
    pub far_plane: f32,
    pub depth_in_normal_alpha: i32,
    pub linear_depth_idx: i32,
}

/// `cRendererDeferred::RenderNightVision`.
pub fn plan_night_vision(i: &NightIn) -> Plan {
    if i.program_ok == 0 || i.visible == 0 {
        return Seq::plan(Vec::new());
    }
    let (c, b) = (i.color, i.brightness);
    let p = nslot::NV_PROG;
    let mut v = vec![
        s(State::DepthTest, 0),
        s(State::DepthWrite, 0),
        Op::P3(super::deferred::P3Op::StencilActive(false)),
        s(State::Channel, st::CHANNEL_RGBA),
        s(State::Blend, st::BLEND_ADD),
        Op::FlatProjection,
        Op::SetAccumBuffer,
        Op::Program { prog: p },
        float(p, v4::AF_FAR_PLANE, i.far_plane),
        vec4(
            p,
            v4::AV_LIGHT_COLOR,
            [c[0] * c[0] * b, c[1] * c[1] * b, c[2] * c[2] * b, c[3]],
        ),
        float(p, v4::AF_FALLOFF_POW, i.falloff),
        float(p, v4::AF_MAX_DISTANCE, i.radius_max),
        Op::ScreenPositionVars { prog: p },
    ];
    v.extend((0..3).map(|k| p4(x::TEX_GBUFFER, k, k)));
    v.extend(fog_depth(
        i.depth_in_normal_alpha != 0,
        i.linear_depth_idx,
        true,
        3,
    ));
    v.push(quad([0.0, 0.0], [1.0, 1.0]));
    v.extend(fog_depth(
        i.depth_in_normal_alpha != 0,
        i.linear_depth_idx,
        false,
        3,
    ));
    v.extend([
        Op::TexRangeNull { first: 0 },
        Op::Program { prog: NONE },
        Op::NormalFrustumProjection,
        s(State::DepthTest, 1),
    ]);
    Seq::plan(ops(v))
}

pub mod hslot {
    pub const UPSAMPLE_PROG: i32 = 0;
    pub const SSAO_TEX: i32 = 0;
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Hpl3In {
    pub rendered: i32,
    pub far_plane: f32,
    pub size_div: i32,
    pub tex_w: i32,
    pub tex_h: i32,
    pub depth_in_normal_alpha: i32,
    pub linear_depth_idx: i32,
}

/// `cRendererDeferred::ApplyHpl3SSAO`. A null upsample program was dereferenced by the original;
/// its variable writes are skipped here.
pub fn plan_apply_hpl3_ssao(i: &Hpl3In) -> Plan {
    if i.rendered == 0 {
        return Seq::plan(Vec::new());
    }
    let p = hslot::UPSAMPLE_PROG;
    let din = i.depth_in_normal_alpha != 0;
    let half = [0.5 / i.tex_w as f32, 0.5 / i.tex_h as f32];
    let mut v = vec![
        s(State::DepthTest, 0),
        s(State::DepthWrite, 0),
        Op::P3(super::deferred::P3Op::StencilActive(false)),
        s(State::Channel, st::CHANNEL_RGBA),
        s(State::Blend, st::BLEND_MUL),
        Op::FlatProjection,
        Op::SetAccumBuffer,
        Op::Program { prog: p },
        float(p, v4::AF_FAR_PLANE, i.far_plane),
        float(p, v4::AF_SIZE_DIV, 1.0 / i.size_div as f32),
        float(p, v4::AF_POWER, 8.0),
        tex(0, hslot::SSAO_TEX),
    ];
    v.extend(fog_depth(din, i.linear_depth_idx, true, 1));
    v.push(quad(
        [black_box(-half[0]), black_box(-half[1])],
        [1.0 - half[0], 1.0 - half[1]],
    ));
    v.extend(fog_depth(din, i.linear_depth_idx, false, 1));
    v.extend([
        tex(0, NONE),
        tex(1, NONE),
        Op::Program { prog: NONE },
        s(State::Blend, st::BLEND_ADD),
        Op::NormalFrustumProjection,
        s(State::DepthTest, 1),
    ]);
    Seq::plan(ops(v))
}

pub mod dslot {
    pub const FOCUS_PROG: i32 = 0;
    pub const BLUR_PROG: i32 = 1;
    pub const ACCUM_TEX: i32 = 0;
    pub const GAUSS_TEX: i32 = 1;
    pub const TEX_A: i32 = 2;
    pub const TEX_B: i32 = 3;
    pub const FB_A: i32 = 0;
    pub const FB_B: i32 = 1;
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DofIn {
    pub screen_w: i32,
    pub screen_h: i32,
    pub screen_fx: f32,
    pub screen_fy: f32,
    pub focus_start: f32,
    pub focus_end: f32,
    pub falloff: f32,
    pub near_plane: f32,
    pub far_plane: f32,
    pub depth_in_normal_alpha: i32,
    pub linear_depth_idx: i32,
}

/// `cRendererDeferred::RenderDepthOfField`.
pub fn plan_depth_of_field(i: DofIn) -> Plan {
    Seq::plan(vec![ask(Op::Ask(Query::DepthOfField), move |f| {
        if !f.flag() {
            return Vec::new();
        }
        dof_body(&i)
    })])
}

fn dof_body(i: &DofIn) -> Vec<Item> {
    use dslot::*;
    let din = i.depth_in_normal_alpha != 0;
    let (start, end) = (i.focus_start, i.focus_end);
    let falloff_len = (end - start) * 0.5 / c_max(1e-15, i.falloff);
    let near = i.near_plane;
    let size = crate::types::Vector2l {
        x: i.screen_w,
        y: i.screen_h,
    };
    let mut v = vec![
        pass_begin(pass::DEPTH_OF_FIELD),
        s(State::DepthTest, 0),
        s(State::DepthWrite, 0),
        s(State::Blend, st::BLEND_NONE),
        s(State::Alpha, st::ALPHA_SOLID),
        s(State::Channel, st::CHANNEL_RGBA),
        Op::FlatProjection,
        Op::AcquireTemp {
            fb: FB_A,
            tex: TEX_A,
            size,
            fmt: super::deferred::pf::RGBA16,
            idx: 6,
        },
        Op::AcquireTemp {
            fb: FB_B,
            tex: TEX_B,
            size,
            fmt: super::deferred::pf::RGBA16,
            idx: 7,
        },
        Op::SetFb {
            fb: FB_A,
            use_pos: false,
        },
        Op::Program { prog: FOCUS_PROG },
        vec4(
            FOCUS_PROG,
            v4::AV_FOCUS_START_END,
            [
                0.5f32.mul_add(start, near),
                near + start,
                end,
                end + falloff_len,
            ],
        ),
        float(FOCUS_PROG, v4::AF_FAR_PLANE, i.far_plane),
        p4(x::FILTER_SAVE, ACCUM_TEX, 0),
        p4(x::FILTER_NEAREST, ACCUM_TEX, 0),
        tex(0, ACCUM_TEX),
    ]
    .into_iter()
    .collect::<Vec<_>>();
    v.extend(fog_depth(din, i.linear_depth_idx, true, 1));
    v.push(quad([0.0, 0.0], [1.0, 1.0]));
    v.extend(fog_depth(din, i.linear_depth_idx, false, 1));
    v.extend([
        p4(x::FILTER_RESTORE, ACCUM_TEX, 0),
        Op::SetFb {
            fb: FB_B,
            use_pos: false,
        },
        Op::Program { prog: BLUR_PROG },
        Op::Vec2 {
            prog: BLUR_PROG,
            id: v4::AV_OFFSET_MUL,
            v: [1.0 / i.screen_fx, 0.0],
        },
        p4(x::FILTER_NEAREST, TEX_A, 0),
        tex(0, TEX_A),
        tex(1, GAUSS_TEX),
        quad([0.0, 0.0], [1.0, 1.0]),
        Op::SetAccumBuffer,
        Op::Vec2 {
            prog: BLUR_PROG,
            id: v4::AV_OFFSET_MUL,
            v: [0.0, -1.0 / i.screen_fy],
        },
        p4(x::FILTER_NEAREST, TEX_B, 0),
        tex(0, TEX_B),
        p4(x::DRAW_ACCUM_QUAD, 0, 0),
        tex(0, NONE),
        tex(1, NONE),
        Op::Program { prog: NONE },
        Op::NormalFrustumProjection,
        pass_end(pass::DEPTH_OF_FIELD),
    ]);
    ops(v)
}
