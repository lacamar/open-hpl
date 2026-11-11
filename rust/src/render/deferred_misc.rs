//! `cRendererDeferred` environment particles, lens flare update, multi iris and glare passes.

use super::deferred::{DlOp, GLARE_VERTS, NONE, Op, Plan, Query, State, c_clamp, c_max, st, state};
use super::deferred_fx::cmath::matrix_mul_vec;
use super::deferred_fx::{Item, Seq, ask, ops, p4, p4f, x};
use crate::math::{Matrixf, dot3, len3, sub3, v3};

unsafe extern "C" {
    fn powf(x: f32, y: f32) -> f32;
}

fn pow(x: f32, y: f32) -> f32 {
    // SAFETY: pure libm function on plain floats.
    unsafe { powf(x, y) }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EnvIn {
    pub active: i32,
    pub num: i32,
    pub behind_focus: i32,
    pub focus_start: f32,
    pub focus_end: f32,
    pub falloff: f32,
    pub proj: Matrixf,
}

/// `cRendererDeferred::RenderEnvironmentParticles`; systems are answered by `x::ENV_VISIBLE`.
pub fn plan_environment_particles(i: EnvIn) -> Plan {
    if i.active == 0 || i.num == 0 {
        return Seq::plan(Vec::new());
    }
    Seq::plan(vec![ask(Op::Ask(Query::DepthOfField), move |f| {
        let dof = f.flag();
        if !dof && i.behind_focus == 0 {
            return Vec::new();
        }
        env_body(&i, dof)
    })])
}

fn env_body(i: &EnvIn, dof: bool) -> Vec<Item> {
    let behind = i.behind_focus != 0;
    let mut p = [
        0.0,
        0.0,
        if behind { 0.0 } else { 1.0 },
        if behind { 1.0 } else { -1.0 },
    ];
    if dof {
        let end_z = matrix_mul_vec(&i.proj, v3(0.0, 0.0, -i.focus_end)).z;
        let start_z = matrix_mul_vec(&i.proj, v3(0.0, 0.0, -i.focus_start)).z;
        p[0] = end_z;
        p[1] = 1.0 / ((start_z - end_z) * 0.5 / c_max(i.falloff, 1e-15));
    }
    let mut v = ops([
        state(State::DepthTest, 1),
        state(State::DepthWrite, 0),
        state(State::Channel, st::CHANNEL_RGBA),
        state(State::Alpha, st::ALPHA_SOLID),
        p4(x::MATRIX_NULL, 0, 0),
        state(State::Blend, st::BLEND_ALPHA),
        p4(x::CULL_ACTIVE, 0, 0),
        Op::NormalFrustumProjection,
    ]);
    v.extend(env_system(0, i.num, dof, p));
    v
}

fn env_system(idx: i32, num: i32, dof: bool, p: [f32; 4]) -> Vec<Item> {
    if idx >= num {
        return ops([
            p4(x::CULL_ACTIVE, 1, 0),
            Op::Tex { unit: 0, tex: NONE },
            Op::Program { prog: NONE },
            p4(x::SET_VERTEX_BUFFER_NONE, 0, 0),
        ]);
    }
    vec![ask(p4(x::ENV_VISIBLE, idx, i32::from(dof)), move |f| {
        let a = f.vec3();
        let mut v = Vec::new();
        if a[0] != 0.0 && a[1] != 0.0 {
            v.extend(ops([
                Op::Program { prog: 0 },
                p4f(
                    x::ENV_SETUP_BASE,
                    idx,
                    0,
                    0,
                    0,
                    [p[0], p[1], p[2], p[3], 0.0, 0.0, 0.0, 0.0],
                ),
                Op::Tex { unit: 0, tex: 0 },
                p4(x::ENV_VERTEX_BUFFER, idx, 0),
            ]));
            for j in 0..a[2] as i32 {
                v.extend(ops([
                    p4(x::ENV_ITERATION, idx, j),
                    Op::Dl(DlOp::DrawCurrent),
                ]));
            }
        }
        v.extend(env_system(idx + 1, num, dof, p));
        v
    })]
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FlareUpdateIn {
    pub is_reflection: i32,
    pub count: i32,
    pub frame_time: f32,
    pub frame: i32,
}

/// `cRendererDeferred::UpdateLensFlares`.
pub fn plan_update_lens_flares(i: FlareUpdateIn) -> Plan {
    if i.is_reflection != 0 {
        return Seq::plan(Vec::new());
    }
    let mut v = Vec::new();
    for k in 0..i.count {
        v.push(p4(x::FLARE_RETRIEVE, k, 0));
        v.push(p4f(
            x::FLARE_UPDATE,
            k,
            i.frame,
            0,
            0,
            [i.frame_time, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
        ));
    }
    Seq::plan(ops(v))
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GlareIn {
    pub iris_ok: i32,
    pub glare_ok: i32,
    pub flare_count: i32,
    pub multi_count: i32,
    pub fog_active: i32,
    pub fog_start: f32,
    pub fog_end: f32,
    pub fog_falloff: f32,
    pub fog_alpha: f32,
    pub cam: [f32; 3],
    pub aspect: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GlareFlare {
    pub vis: f32,
    pub any_active: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct IrisFlare {
    pub tex_ok: i32,
    pub blend: i32,
    pub vis: f32,
    pub pos: [f32; 3],
}

/// `cRendererDeferred::RenderMultiIrisAndGlare`; `pos` is the glare mesh's position array (4 floats
/// per vertex), `flares` every lens flare and `iris` the multi iris ones.
pub fn plan_multi_iris_and_glare(
    i: GlareIn,
    flares: Vec<GlareFlare>,
    iris: Vec<IrisFlare>,
    pos: Vec<f32>,
) -> Plan {
    if i.iris_ok == 0 || i.glare_ok == 0 || i.flare_count == 0 {
        return Seq::plan(Vec::new());
    }
    let mut v = ops([
        state(State::DepthTest, 0),
        state(State::DepthWrite, 0),
        p4(x::CULL_ACTIVE, 0, 0),
        state(State::Alpha, st::ALPHA_SOLID),
        state(State::Channel, st::CHANNEL_RGBA),
        p4(x::MATRIX_NULL, 0, 0),
    ]);
    for (k, f) in iris.iter().enumerate() {
        if f.tex_ok == 0 {
            continue;
        }
        let mut vis = f.vis;
        if i.fog_active != 0 {
            let cam = v3(i.cam[0], i.cam[1], i.cam[2]);
            let dist = len3(sub3(v3(f.pos[0], f.pos[1], f.pos[2]), cam));
            let t = c_clamp((dist - i.fog_start) / (i.fog_end - i.fog_start), 0.0, 1.0);
            let fp = pow(t, i.fog_falloff);
            vis *= (1.0 - i.fog_alpha).mul_add(fp, 1.0 - fp);
        }
        if vis <= 0.0 {
            continue;
        }
        v.extend(ops([
            p4(x::BLEND_RAW, f.blend, 0),
            p4(x::FLARE_TEXTURE, k as i32, 0),
            Op::Program { prog: 0 },
            p4f(
                x::FLARE_IRIS_BUFFER,
                k as i32,
                0,
                0,
                0,
                [vis, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
            ),
            Op::Dl(DlOp::DrawCurrent),
        ]));
    }
    let colors = vec![0.0f32; GLARE_VERTS * 4];
    let queue: Vec<(i32, f32)> = flares
        .iter()
        .enumerate()
        .filter(|(_, f)| (f.vis.is_nan() || f.vis > 0.0) && f.any_active != 0)
        .map(|(k, f)| (k as i32, f.vis))
        .collect();
    v.extend(glare_flares(i, queue, 0, 0.0, colors, pos));
    Seq::plan(v)
}

fn glare_flares(
    i: GlareIn,
    queue: Vec<(i32, f32)>,
    at: usize,
    sum: f32,
    colors: Vec<f32>,
    pos: Vec<f32>,
) -> Vec<Item> {
    let Some(&(k, vis)) = queue.get(at) else {
        return glare_finish(sum, colors);
    };
    vec![ask(
        p4f(
            x::ASK_GLARE,
            k,
            0,
            0,
            0,
            [vis, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
        ),
        move |f| {
            let g = f.floats();
            let c = |n: usize| g.get(n).copied().unwrap_or(0.0);
            let mut colors = colors;
            let mut sum = sum;
            let total = c(0) + c(1) + c(2);
            if total.is_nan() || total > 0.0 {
                sum += total;
                let (gx, gy) = (c(4), c(5));
                for (n, col) in colors.chunks_exact_mut(4).enumerate() {
                    let p = |o: usize| pos.get(n * 4 + o).copied().unwrap_or(0.0);
                    let d = v3((p(0) - gx) * i.aspect, (1.0 - p(1)) - gy, p(2));
                    let d2 = dot3(d, d);
                    let w = std::f32::consts::SQRT_2 / (128.0 * d2).mul_add(d2, 1.0);
                    for (ch, slot) in col.iter_mut().enumerate() {
                        *slot = c(ch).mul_add(w, *slot);
                    }
                }
            }
            glare_flares(i, queue, at + 1, sum, colors, pos)
        },
    )]
}

fn glare_finish(sum: f32, colors: Vec<f32>) -> Vec<Item> {
    let mut v = vec![Item::Arr(
        colors,
        Box::new(|arr| p4(x::GLARE_WRITE, arr, 0)),
    )];
    if sum > 1.0 / 255.0 {
        v.extend(ops([
            p4(x::GLARE_UPDATE_DATA, 0, 0),
            state(State::Blend, st::BLEND_ADD),
            Op::Tex { unit: 0, tex: NONE },
            Op::Program { prog: 1 },
            p4(x::SET_GLARE_BUFFER, 0, 0),
            Op::Dl(DlOp::DrawCurrent),
        ]));
    }
    v.extend(ops([state(State::DepthTest, 1), p4(x::CULL_ACTIVE, 1, 0)]));
    v
}
