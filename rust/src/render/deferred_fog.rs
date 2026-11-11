//! `cRendererDeferred` fog passes: `SetupFogProgram`, `RenderFullScreenFog` and `RenderFog`.

use std::collections::VecDeque;
use std::hint::black_box;

use super::deferred::{DlOp, Feed, NONE, Op, P3Op, Plan, State, c_max, log2, st, state};
use super::deferred_fx::cmath::{matrix_inverse, matrix_mul, matrix_mul_vec, plane_dist};
use super::deferred_fx::{
    Item, Seq, ask, fog_depth, fog_render_color, mat, o, ops, p4, pass, pass_begin, pass_end, v4, x,
};
use super::deferred_light::underwater_fog_color;
use super::deferred_pass::pshape;
use crate::math::{Matrixf, Planef, Vector3f, matrix_rotate, matrix_translate, sin, transpose};
use crate::types::Vector2l;

/// `eFeature_FogArea_*`.
pub mod fogf {
    pub const OUTSIDE_BOX: u32 = 1 << 0;
    pub const BACKSIDE: u32 = 1 << 1;
    pub const UNDERWATER: u32 = 1 << 2;
    pub const NOISE: u32 = 1 << 3;
    pub const SKYBOX: u32 = 1 << 4;
    pub const SECONDARY: u32 = 1 << 5;
    pub const FOG_AREA: u32 = 1 << 6;
    pub const LIGHTEN: u32 = 1 << 7;
    pub const SEPARATE_SKYBOX: u32 = 1 << 8;
}

pub mod fslot {
    pub const PROG: i32 = 0;
    pub const NOISE_TEX: i32 = 0;
    pub const SKY_TEX: i32 = 1;
    pub const REFRACTION_TEX: i32 = 2;
}

/// World state `SetupFogProgram` and the fog passes read.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct FogWorld {
    pub hdr: i32,
    pub depth_in_normal_alpha: i32,
    pub linear_depth_idx: i32,
    pub fog_active: i32,
    pub fog_underwater: i32,
    pub area_num: i32,
    pub frustum_far: f32,
    pub cam_y: f32,
    pub time: f32,
    pub rt_w: i32,
    pub rt_h: i32,
    pub sec_active: i32,
    pub sec_start: f32,
    pub sec_end: f32,
    pub sec_falloff: f32,
    pub sec_horizon: f32,
    pub sec_density: f32,
    pub sec_height_density: f32,
    pub sec_color: [f32; 4],
    pub sec_brightness: f32,
    pub sky_tex: i32,
    pub sky_active: i32,
    pub sky_separate: i32,
    pub sky_color: [f32; 4],
    pub sky_brightness: f32,
    pub sky_rot: [f32; 3],
    pub sky_w: i32,
    pub sky_h: i32,
    pub view: Matrixf,
    pub inv_view: Matrixf,
}

/// The world's fog or one fog area, whichever `SetupFogProgram` was given.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct FogParams {
    pub is_area: i32,
    pub start: f32,
    pub end: f32,
    pub falloff: f32,
    pub color: [f32; 4],
    pub brightness: f32,
    pub noise_strength: f32,
    pub noise_size: f32,
    pub turb: [f32; 3],
    pub skybox_wanted: i32,
    pub lighten: i32,
    pub exp: i32,
    pub height: i32,
    pub density: f32,
    pub height_density: f32,
    pub height_horizon: f32,
    pub bv_min_y: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct FogAreaIn {
    pub inside_near: i32,
    pub backside_inside: i32,
    pub backside_outside: i32,
    pub underwater: i32,
    pub params: FogParams,
    pub model: Matrixf,
}

type Then = Box<dyn FnOnce(bool) -> Vec<Item>>;

fn turbulence_term(b: f32, t: f32, c: [f32; 3]) -> f32 {
    let s1 = sin(1.13f32.mul_add(t, 2.5f32.mul_add(b, c[0])));
    let s2 = sin(1.17f32.mul_add(t, b + c[1]));
    let s3 = sin(0.5f32.mul_add(b, c[2]) + t);
    0.25f32.mul_add(s2, 0.125 * s1) + s3
}

/// `FogNoiseTurbulence`.
fn fog_noise_turbulence(turb: [f32; 3], size: f32, time: f32) -> Vector3f {
    let t = time * 0.0625;
    let a = 4.0 * t;
    let b = a + 1.0;
    let d1 = turbulence_term(b, t, [13.0, 17.0, 103.0]);
    let b2 = b + d1;
    let d2 = turbulence_term(b2, t, [103.0, 13.0, 113.0]);
    let m = c_max(1.0, size * 0.25);
    Vector3f {
        x: d1 * turb[0] * m,
        y: a * turb[1] * m,
        z: d2 * turb[2] * m,
    }
}

/// `SetupFogProgram`; `then` gets whether a program was generated.
fn setup_fog(w: FogWorld, p: FogParams, flags: u32, underwater: bool, then: Then) -> Vec<Item> {
    let go = move |flags: u32| fog_program(w, p, flags, underwater, then);
    if p.noise_strength > 0.0 {
        vec![ask(p4(x::ENSURE_FOG_NOISE, 0, 0), move |f| {
            go(flags | if f.flag() { fogf::NOISE } else { 0 })
        })]
    } else {
        go(flags)
    }
}

fn fog_program(
    w: FogWorld,
    p: FogParams,
    mut flags: u32,
    underwater: bool,
    then: Then,
) -> Vec<Item> {
    let mut sky = false;
    if underwater {
        flags |= fogf::UNDERWATER;
    } else {
        sky = p.skybox_wanted != 0 && w.sky_tex != 0 && w.sky_active != 0;
        if sky {
            flags |= fogf::SKYBOX;
        }
        if sky && w.sky_separate != 0 {
            flags |= fogf::SEPARATE_SKYBOX;
        }
        if p.is_area != 0 {
            flags |= fogf::FOG_AREA;
        }
        if p.lighten != 0 {
            flags |= fogf::LIGHTEN;
        }
    }
    let secondary = p.is_area == 0 && w.sec_active != 0;
    if secondary {
        flags |= fogf::SECONDARY;
    }
    vec![ask(
        p4(x::GEN_FOG_PROGRAM, fslot::PROG, flags as i32),
        move |f| {
            let ok = f.flag();
            let mut v = vec![o(Op::Program { prog: fslot::PROG })];
            if ok {
                v.extend(fog_variables(&w, &p, flags, underwater, sky, secondary));
            }
            v.extend(then(ok));
            v
        },
    )]
}

fn fog_variables(
    w: &FogWorld,
    p: &FogParams,
    flags: u32,
    underwater: bool,
    sky: bool,
    secondary: bool,
) -> Vec<Item> {
    use fslot::PROG as pr;
    let hdr = w.hdr != 0;
    let area = p.is_area != 0;
    let mut start_len = [p.start, p.end - p.start];
    let mut fog_color = if underwater {
        underwater_fog_color(p.color)
    } else {
        fog_render_color(hdr, p.color, p.brightness)
    };
    let mut falloff = p.falloff;
    let mut settings = [p.start, p.end, p.falloff, 0.0];
    let mut sec_settings = [0.0f32; 4];
    let mut cam = [w.cam_y; 3];
    let exp = p.exp != 0;
    let height = p.height != 0;
    if exp || height {
        settings[2] = p.density * 0.03125;
        settings[3] = p.height_density;
        cam[0] -= p.height_horizon;
    }
    if area && height {
        for c in &mut cam {
            *c -= p.bv_min_y;
        }
    }
    if secondary && !underwater {
        cam[1] -= w.sec_horizon;
        sec_settings = [w.sec_start, w.sec_end, w.sec_falloff, 0.0];
        if exp || height {
            sec_settings[2] = w.sec_density * 32.0;
            sec_settings[3] = w.sec_height_density;
        }
        if w.sec_end > p.end {
            std::mem::swap(&mut settings, &mut sec_settings);
            cam.swap(0, 1);
        }
    }
    let mut v = ops([
        Op::Vec4 {
            prog: pr,
            id: v4::AV_FOG_SETTINGS,
            v: settings,
        },
        Op::Vec4 {
            prog: pr,
            id: v4::AV_SECOND_FOG_SETTINGS,
            v: sec_settings,
        },
        Op::Vec3 {
            prog: pr,
            id: v4::AV_CAMERA_POS,
            v: cam,
        },
        Op::Int {
            prog: pr,
            id: v4::AL_FOG_TYPE,
            v: i32::from(exp) | (i32::from(height) << 1),
        },
    ]);
    if secondary {
        let mut sec_start_len = [w.sec_start, w.sec_end - w.sec_start];
        let mut sec_color = fog_render_color(hdr, w.sec_color, w.sec_brightness);
        let mut sec_falloff = w.sec_falloff;
        if !underwater && w.sec_end > p.end {
            std::mem::swap(&mut start_len, &mut sec_start_len);
            std::mem::swap(&mut fog_color, &mut sec_color);
            std::mem::swap(&mut falloff, &mut sec_falloff);
        }
        v.extend(ops([
            Op::Vec4 {
                prog: pr,
                id: v4::AV_SECOND_FOG_COLOR,
                v: sec_color,
            },
            Op::Vec2 {
                prog: pr,
                id: v4::AV_SECOND_FOG_START_AND_LENGTH,
                v: sec_start_len,
            },
            Op::Float {
                prog: pr,
                id: v4::AF_SECOND_FALLOFF_EXP,
                v: sec_falloff,
            },
        ]));
    }
    v.extend(ops([
        Op::Float {
            prog: pr,
            id: v4::AF_NEG_FAR_PLANE,
            v: -w.frustum_far,
        },
        Op::Vec2 {
            prog: pr,
            id: v4::AV_FOG_START_AND_LENGTH,
            v: start_len,
        },
        Op::Vec4 {
            prog: pr,
            id: v4::AV_FOG_COLOR,
            v: fog_color,
        },
        Op::Float {
            prog: pr,
            id: v4::AF_FALLOFF_EXP,
            v: falloff,
        },
        Op::ScreenPositionVars { prog: pr },
    ]));
    let turb = fog_noise_turbulence(p.turb, p.noise_size, w.time);
    v.push(mat(
        pr,
        v4::A_MTX_NOISE,
        &matrix_mul(&matrix_translate(turb), &w.inv_view),
    ));
    if flags & fogf::NOISE != 0 {
        v.extend(ops([
            Op::Float {
                prog: pr,
                id: v4::AF_INV_NOISE_SCALE,
                v: 1.0 / p.noise_size,
            },
            Op::Float {
                prog: pr,
                id: v4::AF_NOISE_STRENGTH,
                v: p.noise_strength,
            },
            Op::Tex {
                unit: 1,
                tex: fslot::NOISE_TEX,
            },
        ]));
    }
    if sky {
        let rot = Vector3f {
            x: w.sky_rot[0],
            y: w.sky_rot[1],
            z: w.sky_rot[2],
        };
        v.extend(ops([
            Op::Vec4 {
                prog: pr,
                id: v4::AV_SKYBOX_COLOR,
                v: fog_render_color(hdr, w.sky_color, w.sky_brightness),
            },
            Op::Tex {
                unit: 2,
                tex: fslot::SKY_TEX,
            },
        ]));
        v.push(mat(
            pr,
            v4::A_MTX_SKYBOX,
            &transpose(&matrix_rotate(rot, 0)),
        ));
        v.push(o(Op::Float {
            prog: pr,
            id: v4::AF_SKYBOX_MIP_NUM,
            v: log2(w.sky_w.max(w.sky_h) as f32),
        }));
    }
    if flags & fogf::LIGHTEN != 0 {
        v.extend(ops([
            Op::CopyFbToTex {
                tex: fslot::REFRACTION_TEX,
                size: Vector2l {
                    x: w.rt_w,
                    y: w.rt_h,
                },
            },
            Op::Tex {
                unit: 3,
                tex: fslot::REFRACTION_TEX,
            },
            state(State::Blend, st::BLEND_NONE),
        ]));
    }
    v
}

fn depth_items(w: &FogWorld, bind: bool) -> Vec<Item> {
    ops(fog_depth(
        w.depth_in_normal_alpha != 0,
        w.linear_depth_idx,
        bind,
        0,
    ))
}

fn blend_for(pass_idx: i32) -> Item {
    o(state(
        State::Blend,
        if pass_idx == 0 {
            st::BLEND_MUL
        } else {
            st::BLEND_ALPHA
        },
    ))
}

/// `cRendererDeferred::RenderFullScreenFog`.
pub fn plan_full_screen_fog(w: FogWorld, p: FogParams) -> Plan {
    if w.fog_active == 0 {
        return Seq::plan(Vec::new());
    }
    let mut v = ops([
        pass_begin(pass::FULLSCREEN_FOG),
        state(State::DepthTest, 0),
        state(State::DepthWrite, 0),
        state(State::Channel, st::CHANNEL_RGBA),
        state(State::Alpha, st::ALPHA_SOLID),
    ]);
    v.extend(depth_items(&w, true));
    v.extend(ops([
        Op::TexRangeNull { first: 1 },
        p4(x::MATRIX_NULL, 0, 0),
        Op::FlatProjection,
    ]));
    v.extend(full_screen_pass(w, p, i32::from(w.fog_underwater == 0)));
    Seq::plan(v)
}

fn full_screen_pass(w: FogWorld, p: FogParams, pass_idx: i32) -> Vec<Item> {
    if pass_idx >= 2 {
        let mut v = ops([Op::NormalFrustumProjection, state(State::DepthTest, 1)]);
        v.extend(depth_items(&w, false));
        v.extend(ops([
            Op::TexRangeNull { first: 1 },
            pass_end(pass::FULLSCREEN_FOG),
        ]));
        return v;
    }
    let mut v = vec![blend_for(pass_idx)];
    v.extend(setup_fog(
        w,
        p,
        0,
        pass_idx == 0,
        Box::new(move |ok| {
            let mut r = Vec::new();
            if ok {
                r.push(o(p4(x::QUAD_PLAIN, 0, 0)));
            }
            r.extend(full_screen_pass(w, p, pass_idx + 1));
            r
        }),
    ));
    v
}

/// `cRendererDeferred::RenderFog`.
pub fn plan_fog(w: FogWorld) -> Plan {
    if w.area_num == 0 {
        return Seq::plan(ops([p4(x::CLEAR_FOG_DATA, 0, 0)]));
    }
    let mut v = ops([
        pass_begin(pass::FOG),
        p4(x::SETUP_FOG_DATA, 0, 0),
        state(State::DepthTest, 1),
        state(State::DepthWrite, 0),
        state(State::Channel, st::CHANNEL_RGBA),
        state(State::Alpha, st::ALPHA_SOLID),
    ]);
    v.extend(depth_items(&w, true));
    v.push(o(Op::TexRangeNull { first: 1 }));
    v.push(ask(p4(x::ASK_FOGS, 0, 0), move |f| {
        let areas = match f {
            Feed::Fogs(a) => a,
            _ => Vec::new(),
        };
        fog_areas(w, areas.into(), 0)
    }));
    Seq::plan(v)
}

fn fog_areas(w: FogWorld, mut areas: VecDeque<FogAreaIn>, idx: i32) -> Vec<Item> {
    let Some(a) = areas.pop_front() else {
        let mut v = ops([Op::P3(P3Op::Cull(true)), state(State::DepthTest, 1)]);
        v.extend(depth_items(&w, false));
        v.extend(ops([Op::TexRangeNull { first: 1 }, pass_end(pass::FOG)]));
        return v;
    };
    let mut flags = 0;
    if a.inside_near != 0 {
        if a.backside_inside != 0 {
            flags |= fogf::BACKSIDE;
        }
    } else {
        if a.backside_outside != 0 {
            flags |= fogf::BACKSIDE;
        }
        flags |= fogf::OUTSIDE_BOX;
    }
    fog_area_pass(w, a, areas, idx, flags, i32::from(a.underwater == 0))
}

fn fog_area_pass(
    w: FogWorld,
    a: FogAreaIn,
    rest: VecDeque<FogAreaIn>,
    idx: i32,
    flags: u32,
    pass_idx: i32,
) -> Vec<Item> {
    if pass_idx >= 2 {
        return fog_areas(w, rest, idx + 1);
    }
    let mut v = vec![blend_for(pass_idx)];
    v.extend(setup_fog(
        w,
        a.params,
        flags,
        pass_idx == 0,
        Box::new(move |ok| {
            let mut r = Vec::new();
            if ok {
                r.extend(fog_area_draw(&w, &a, idx));
            }
            r.extend(fog_area_pass(w, a, rest, idx, flags, pass_idx + 1));
            r
        }),
    ));
    v
}

fn fog_area_draw(w: &FogWorld, a: &FogAreaIn, idx: i32) -> Vec<Item> {
    use fslot::PROG as pr;
    let inside = a.inside_near != 0;
    let mut v = Vec::new();
    if !inside {
        let inv = matrix_inverse(&matrix_mul(&w.view, &a.model));
        let start = matrix_mul_vec(&inv, Vector3f::default());
        let d = |pa, pb, pc| {
            plane_dist(
                &Planef {
                    a: pa,
                    b: pb,
                    c: pc,
                    d: 0.5,
                },
                start,
            )
        };
        let neg = [d(-1.0, 0.0, 0.0), d(0.0, -1.0, 0.0), d(0.0, 0.0, -1.0)];
        let pos = [d(1.0, 0.0, 0.0), d(0.0, 1.0, 0.0), d(0.0, 0.0, 1.0)];
        let mut rot = inv;
        for row in &mut rot.m {
            row[3] = 0.0;
        }
        rot.m[3] = [0.0, 0.0, 0.0, 1.0];
        v.push(o(Op::Vec3 {
            prog: pr,
            id: v4::AV_RAY_CAST_START,
            v: [start.x, start.y, start.z],
        }));
        v.push(mat(pr, v4::A_MTX_BOX_INV_VIEW_MODEL_ROTATION, &rot));
        v.extend(ops([
            Op::Vec3 {
                prog: pr,
                id: v4::AV_NEG_PLANE_DIST_NEG,
                v: neg.map(|c| black_box(-c)),
            },
            Op::Vec3 {
                prog: pr,
                id: v4::AV_NEG_PLANE_DIST_POS,
                v: pos.map(|c| black_box(-c)),
            },
        ]));
    }
    v.extend(ops([
        Op::P3(P3Op::Cull(!inside)),
        state(State::DepthTest, i32::from(!inside)),
        p4(x::FOG_AREA_MATRIX, idx, 0),
        Op::Dl(DlOp::SetShape(pshape::BOX)),
        Op::Dl(DlOp::DrawCurrent),
    ]));
    v
}

#[allow(dead_code)]
const _: i32 = NONE;
