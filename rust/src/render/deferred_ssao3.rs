//! `cRendererDeferred::RenderHpl3SSAO`.

use super::deferred::{NONE, Op, Plan, State, c_clamp, st, state};
use super::deferred_fx::cmath::{matrix_inverse, matrix_mul};
use super::deferred_fx::{Item, Seq, fog_depth, mat, o, ops, p4, pass_begin, pass_end, x};
use crate::math::{IDENTITY, Matrixf, len3, tan, v3};

unsafe extern "C" {
    fn fmodf(x: f32, y: f32) -> f32;
}

/// Slots: frame buffers 0..2 are the SSAO buffers, mip buffers follow; textures 0..2 are the SSAO
/// textures, 3 the mip texture; programs as `prog`.
pub mod slot {
    pub const MIP_FB0: i32 = 3;
    pub const MIP_TEX: i32 = 3;

    pub mod prog {
        pub const RENDER: i32 = 0;
        pub const DOWNSAMPLE: i32 = 1;
        pub const BLUR: i32 = 2;
        pub const TEMPORAL: i32 = 3;
    }
}

pub mod var {
    pub const AV_INV_SCREEN_SIZE: i32 = 24;
    pub const AV_UV_TO_VIEW0: i32 = 55;
    pub const AV_UV_TO_VIEW1: i32 = 56;
    pub const AF_T: i32 = 57;
    pub const AF_STEP_SIZE_MAX: i32 = 58;
    pub const AF_FAR_PLANE: i32 = 10;
    pub const AF_RADIUS: i32 = 59;
    pub const AF_SCREEN_SIZE_DIV: i32 = 60;
    pub const AF_LOD_SCALE: i32 = 61;
    pub const AF_DEPTH_DIFFERENCE: i32 = 62;
    pub const AV_DIRECTION: i32 = 63;
    pub const A_MTX_TEMPORAL_VIEW: i32 = 64;
    pub const A_MTX_TEMPORAL_PROJECTION: i32 = 65;
    pub const AV_SCREEN_SIZE: i32 = 12;
    pub const AF_TEMPORAL_BLUR_AMOUNT: i32 = 66;
}

pub const PASS_SSAO: i32 = 3;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Hpl3RenderIn {
    pub programs_ok: i32,
    pub is_reflection: i32,
    pub ssao_active: i32,
    pub size_div: i32,
    pub fov: f32,
    pub aspect: f32,
    pub far_plane: f32,
    pub tex_w: i32,
    pub tex_h: i32,
    pub mip_w: i32,
    pub mip_h: i32,
    pub screen_fx: f32,
    pub screen_fy: f32,
    pub depth_in_normal_alpha: i32,
    pub linear_idx: i32,
    pub mip_buffers: i32,
    pub time: f32,
    pub first_frame: i32,
    pub frame_time: f32,
    pub prev_view: Matrixf,
    pub view: Matrixf,
    pub proj: Matrixf,
}

/// The renderer members the pass writes.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Hpl3State {
    pub time: f32,
    pub first_frame: i32,
    pub rendered: i32,
    pub prev_view: Matrixf,
}

const BLUR_PASSES: [[usize; 3]; 4] = [[0, 1, 0], [1, 0, 1], [1, 0, 0], [0, 2, 1]];

fn fb3(fb: i32) -> Item {
    o(p4(x::SET_FB3, fb, 0))
}

fn quad() -> Item {
    o(Op::Quad {
        min: [0.0, 0.0],
        max: [1.0, 1.0],
    })
}

fn blur_pass(v: &mut Vec<Item>, k: usize, dirs: &[[f32; 2]; 2]) {
    use slot::prog::BLUR;
    let p = BLUR_PASSES.get(k).copied().unwrap_or([0; 3]);
    v.push(fb3(p[1] as i32));
    v.push(o(Op::Vec2 {
        prog: BLUR,
        id: var::AV_DIRECTION,
        v: dirs.get(p[2]).copied().unwrap_or([0.0; 2]),
    }));
    v.push(o(Op::Tex {
        unit: 0,
        tex: p[0] as i32,
    }));
    v.push(quad());
}

/// `RenderHpl3SSAO`; the plan's `res.h3` holds the members to store back.
pub fn plan_hpl3_ssao(i: &Hpl3RenderIn) -> Plan {
    let mut state_out = Hpl3State {
        time: i.time,
        first_frame: i.first_frame,
        rendered: 0,
        prev_view: i.prev_view,
    };
    if i.programs_ok == 0 || i.is_reflection != 0 || i.ssao_active == 0 {
        let mut p = Seq::plan(Vec::new());
        p.res.h3 = state_out;
        return p;
    }
    use slot::prog::{BLUR, DOWNSAMPLE, RENDER, TEMPORAL};
    let din = i.depth_in_normal_alpha != 0;
    let far = i.far_plane;
    let div = i.size_div as f32;
    let tan_half = tan(i.fov * 0.5);
    let t = tan_half * far;
    let w = i.aspect * t;
    let uv0 = [2.0 * w, 2.0 * t, 0.0];
    let uv1 = [-w, -t, -far];
    let size = [i.tex_w as f32, i.tex_h as f32];
    let mut v = ops([
        pass_begin(PASS_SSAO),
        state(State::Channel, st::CHANNEL_RGBA),
        state(State::Alpha, st::ALPHA_SOLID),
        state(State::Blend, st::BLEND_NONE),
        state(State::DepthTest, 0),
        state(State::DepthWrite, 0),
        Op::TexRangeNull { first: 0 },
        Op::FlatProjection,
        Op::Program { prog: DOWNSAMPLE },
        Op::Vec2 {
            prog: DOWNSAMPLE,
            id: var::AV_INV_SCREEN_SIZE,
            v: [1.0 / i.screen_fx, 1.0 / i.screen_fy],
        },
    ]);
    v.extend(ops(fog_depth(din, i.linear_idx, true, 0)));
    for k in 0..i.mip_buffers {
        v.push(fb3(slot::MIP_FB0 + k));
        v.push(quad());
    }
    v.extend(ops(fog_depth(din, i.linear_idx, false, 0)));

    // SAFETY: pure libm function on plain floats.
    state_out.time = unsafe { fmodf(i.time + i.frame_time, 256.0) };
    let mip = [i.mip_w as f32, i.mip_h as f32];
    let lod = 0.4 * (mip[1].mul_add(mip[1], mip[0] * mip[0]) + 1.0).sqrt() as i32 as f32;
    v.push(fb3(0));
    v.extend(ops([
        Op::Program { prog: RENDER },
        Op::Vec3 {
            prog: RENDER,
            id: var::AV_UV_TO_VIEW0,
            v: uv0,
        },
        Op::Vec3 {
            prog: RENDER,
            id: var::AV_UV_TO_VIEW1,
            v: uv1,
        },
        Op::Float {
            prog: RENDER,
            id: var::AF_T,
            v: state_out.time * 1.941_611,
        },
        Op::Float {
            prog: RENDER,
            id: var::AF_STEP_SIZE_MAX,
            v: 0.5 / tan_half,
        },
        Op::Float {
            prog: RENDER,
            id: var::AF_FAR_PLANE,
            v: far,
        },
        Op::Float {
            prog: RENDER,
            id: var::AF_RADIUS,
            v: 1.5,
        },
        Op::Float {
            prog: RENDER,
            id: var::AF_SCREEN_SIZE_DIV,
            v: div,
        },
        Op::Float {
            prog: RENDER,
            id: var::AF_LOD_SCALE,
            v: lod,
        },
        p4(x::TEX_BUFFER, 0, 1),
        Op::Tex {
            unit: 1,
            tex: slot::MIP_TEX,
        },
    ]));
    v.push(quad());
    v.push(o(Op::Tex { unit: 1, tex: NONE }));

    v.extend(ops([
        Op::Program { prog: BLUR },
        Op::Float {
            prog: BLUR,
            id: var::AF_FAR_PLANE,
            v: far,
        },
        Op::Float {
            prog: BLUR,
            id: var::AF_DEPTH_DIFFERENCE,
            v: 0.0625,
        },
    ]));
    let dirs = [[0.0, 1.0 / size[1]], [1.0 / size[0], 0.0]];
    for k in 0..2 {
        blur_pass(&mut v, k, &dirs);
    }

    let temporal = if i.first_frame != 0 {
        IDENTITY
    } else {
        matrix_mul(&i.prev_view, &matrix_inverse(&i.view))
    };
    let mut amount = c_clamp(i.frame_time, 1.0 / 60.0, 1.0 / 15.0);
    let moved = len3(v3(temporal.m[0][3], temporal.m[1][3], temporal.m[2][3]));
    if moved > 0.1 {
        amount += (moved - 0.1) / 10.0;
    }
    if i.first_frame != 0 {
        amount += 2.0;
    }
    state_out.first_frame = 0;
    state_out.prev_view = i.view;

    v.push(fb3(1));
    v.extend(ops([
        Op::Program { prog: TEMPORAL },
        Op::Float {
            prog: TEMPORAL,
            id: var::AF_FAR_PLANE,
            v: far,
        },
        Op::Vec3 {
            prog: TEMPORAL,
            id: var::AV_UV_TO_VIEW0,
            v: uv0,
        },
        Op::Vec3 {
            prog: TEMPORAL,
            id: var::AV_UV_TO_VIEW1,
            v: uv1,
        },
    ]));
    v.push(mat(TEMPORAL, var::A_MTX_TEMPORAL_VIEW, &temporal));
    v.push(mat(TEMPORAL, var::A_MTX_TEMPORAL_PROJECTION, &i.proj));
    v.extend(ops([
        Op::Vec2 {
            prog: TEMPORAL,
            id: var::AV_SCREEN_SIZE,
            v: size,
        },
        Op::Float {
            prog: TEMPORAL,
            id: var::AF_TEMPORAL_BLUR_AMOUNT,
            v: c_clamp(amount, 0.0, 1.0),
        },
        Op::Tex { unit: 0, tex: 0 },
        Op::Tex { unit: 1, tex: 2 },
    ]));
    v.push(quad());
    v.push(o(Op::Tex { unit: 1, tex: NONE }));
    v.push(o(Op::Program { prog: BLUR }));
    for k in 2..4 {
        blur_pass(&mut v, k, &dirs);
    }
    v.extend(ops([
        Op::Tex { unit: 0, tex: NONE },
        Op::Program { prog: NONE },
        Op::NormalFrustumProjection,
        pass_end(PASS_SSAO),
    ]));
    state_out.rendered = 1;
    let mut p = Seq::plan(v);
    p.res.h3 = state_out;
    p
}
