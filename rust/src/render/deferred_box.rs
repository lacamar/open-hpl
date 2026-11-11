//! `cRendererDeferred::RenderLights_BoxWeighted`.

use std::hint::black_box;

use super::deferred::{DlOp, Op, P3Op, Plan, State, st, state};
use super::deferred_dir::{BoxIn, BoxLightIn, box_order, v5, vec3};
use super::deferred_fx::{Item, Seq, o, ops, p4, x};
use super::deferred_light::{linear_light_color, underwater_fog_color};
use super::deferred_pass::{blend, pshape};
use crate::math::{Matrixf, Vector3f, matrix_mul_vec, v3};

pub mod bslot {
    pub const RESOLVE_PROG: i32 = 6;
    pub const BUFFER_FB: i32 = 0;
    pub const WEIGHT_TEX: i32 = 0;
}

fn mul4(a: [f32; 4], b: [f32; 4]) -> [f32; 4] {
    [a[0] * b[0], a[1] * b[1], a[2] * b[2], a[3] * b[3]]
}

fn scale4(a: [f32; 4], s: f32) -> [f32; 4] {
    [a[0] * s, a[1] * s, a[2] * s, a[3] * s]
}

fn box_light_draw(i: &BoxIn, view_up: [f32; 3], l: &BoxLightIn, out: &mut Vec<Item>) {
    let hdr = i.hdr != 0;
    let lblend = l.blend_func.clamp(0, 2);
    let prog = lblend * 2 + i32::from(l.spherical != 0);
    if i.prog_ok & (1 << (prog & 31)) == 0 {
        return;
    }
    out.push(o(p4(x::LOG, 1, l.idx)));
    out.push(o(state(
        State::Blend,
        match lblend {
            blend::REPLACE => st::BLEND_PREMUL_ALPHA,
            blend::ADD => st::BLEND_MODULATE_SOURCE,
            _ => st::BLEND_ADD,
        },
    )));
    out.push(o(Op::Program { prog }));
    let mut diffuse = scale4(linear_light_color(l.diffuse, hdr), l.brightness * l.fade);
    if i.underwater != 0 && l.spherical == 0 {
        let fog = underwater_fog_color(i.fog_color);
        diffuse[0] *= fog[0];
        diffuse[1] *= fog[1];
        diffuse[2] *= fog[2];
    }
    let (sky, ground) = (l.ambient_sky, l.ambient_ground);
    let center = [
        l.pos[0] + l.probe_offset[0] - i.origin[0],
        l.pos[1] + l.probe_offset[1] - i.origin[1],
        l.pos[2] + l.probe_offset[2] - i.origin[2],
    ];
    out.extend(ops([
        Op::Float {
            prog,
            id: v5::AF_NEG_FAR_PLANE,
            v: -i.frustum_far,
        },
        Op::ScreenPositionVars { prog },
        Op::InvViewMatrix {
            prog,
            id: v5::A_MTX_INV_VIEW,
        },
        Op::Vec3 {
            prog,
            id: v5::AV_BOX_CENTER,
            v: center,
        },
        Op::Vec3 {
            prog,
            id: v5::AV_BOX_EXTENT,
            v: [l.size[0] * 0.5, l.size[1] * 0.5, l.size[2] * 0.5],
        },
        Op::Float {
            prog,
            id: v5::AF_FALLOFF,
            v: l.falloff,
        },
        Op::Vec3 {
            prog,
            id: v5::AV_VIEW_SPACE_UP,
            v: view_up,
        },
    ]));
    if i.instance_inputs != 0 {
        instance_inputs(prog, l, diffuse, out, hdr, i.frustum_far);
    } else {
        out.extend(ops([
            Op::Vec3 {
                prog,
                id: v5::AV_AMBIENT_COLOR_SKY,
                v: [
                    sky[0] * diffuse[0],
                    sky[1] * diffuse[1],
                    sky[2] * diffuse[2],
                ],
            },
            Op::Vec3 {
                prog,
                id: v5::AV_AMBIENT_COLOR_GROUND,
                v: [
                    ground[0] * diffuse[0],
                    ground[1] * diffuse[1],
                    ground[2] * diffuse[2],
                ],
            },
            Op::Float {
                prog,
                id: v5::AF_BEVEL,
                v: l.bevel,
            },
            Op::Float {
                prog,
                id: v5::AF_WEIGHT,
                v: l.weight,
            },
        ]));
        for (k, band) in l.bands.iter().enumerate() {
            out.push(o(Op::Vec3 {
                prog,
                id: v5::AV_BAND0 + k as i32,
                v: *band,
            }));
        }
    }
    out.extend(ops([
        Op::Dl(DlOp::SetModelView(l.idx)),
        Op::Dl(DlOp::DrawCurrent),
    ]));
}

fn instance_inputs(
    prog: i32,
    l: &BoxLightIn,
    diffuse: [f32; 4],
    out: &mut Vec<Item>,
    hdr: bool,
    far: f32,
) {
    let sky_col = mul4(diffuse, linear_light_color(l.ambient_sky, hdr));
    let ground_col = mul4(diffuse, linear_light_color(l.ambient_ground, hdr));
    let mul = scale4(
        [
            sky_col[0] + ground_col[0],
            sky_col[1] + ground_col[1],
            sky_col[2] + ground_col[2],
            sky_col[3] + ground_col[3],
        ],
        0.5,
    );
    out.extend(ops([
        Op::Float {
            prog,
            id: v5::AF_FAR_PLANE,
            v: far,
        },
        Op::Float {
            prog,
            id: v5::AF_BEVEL,
            v: 2.0f32.mul_add(black_box(-l.bevel), 3.0).sqrt(),
        },
        Op::Float {
            prog,
            id: v5::AF_WEIGHT,
            v: l.weight * l.fade,
        },
        Op::Vec4 {
            prog,
            id: v5::AV_AMBIENT_COLOR_SKY,
            v: sky_col,
        },
        Op::Vec4 {
            prog,
            id: v5::AV_AMBIENT_COLOR_GROUND,
            v: ground_col,
        },
    ]));
    let b = |k: usize, c: usize| {
        l.bands
            .get(k)
            .and_then(|r| r.get(c))
            .copied()
            .unwrap_or(0.0)
    };
    for c in 0..3usize {
        let m = mul.get(c).copied().unwrap_or(0.0);
        out.push(o(Op::Vec4 {
            prog,
            id: v5::AV_BAND0 + c as i32,
            v: [
                b(3, c) * m,
                b(1, c) * m,
                b(2, c) * m,
                (b(0, c) - b(6, c)) * m,
            ],
        }));
        if c < 2 {
            out.push(o(Op::Vec4 {
                prog,
                id: v5::AV_BAND0 + 3 + c as i32,
                v: [b(4, c) * m, b(5, c) * m, 3.0 * b(6, c) * m, b(7, c) * m],
            }));
        }
    }
    out.extend(ops([
        Op::Vec4 {
            prog,
            id: v5::AV_BAND0 + 5,
            v: [0.0; 4],
        },
        Op::Vec4 {
            prog,
            id: v5::AV_BAND0 + 6,
            v: [
                b(8, 0) * mul[0],
                b(8, 1) * mul[1],
                b(8, 2) * mul[2],
                mul[3].abs(),
            ],
        },
    ]));
}

/// `cRendererDeferred::RenderLights_BoxWeighted` over the box lights of both box lists; the plan's
/// `handled` result is the original's return value.
pub fn plan_box_weighted(i: &BoxIn, lights: &[BoxLightIn]) -> Plan {
    let handled = i.buffer_ok != 0 && i.resolve_ok != 0 && i.is_reflection == 0;
    let mut plan = if !handled || lights.is_empty() {
        Seq::plan(Vec::new())
    } else {
        Seq::plan(box_weighted_items(i, lights))
    };
    plan.res.handled = handled;
    plan
}

fn box_weighted_items(i: &BoxIn, lights: &[BoxLightIn]) -> Vec<Item> {
    let mut sorted = lights.to_vec();
    sorted.sort_by(box_order);
    let mut v = ops([
        Op::SetFb {
            fb: bslot::BUFFER_FB,
            use_pos: true,
        },
        Op::ClearColorValue {
            rgba: [0.0, 0.0, 0.0, 1.5259022e-05],
        },
        Op::ClearFb {
            flags: super::deferred::clear::COLOR,
            use_pos: true,
        },
        Op::ClearColorSettings,
        state(State::DepthTest, 0),
        Op::P3(P3Op::StencilActive(false)),
        state(State::Channel, st::CHANNEL_RGBA),
        Op::P3(P3Op::Cull(false)),
        Op::Dl(DlOp::SetShape(pshape::BOX)),
    ]);
    let view_up = vec3(rotate(&i.view, v3(0.0, 1.0, 0.0)));
    for l in &sorted {
        box_light_draw(i, view_up, l, &mut v);
    }
    v.extend(ops([
        Op::SetAccumBuffer,
        state(State::Blend, st::BLEND_NONE),
        Op::P3(P3Op::Cull(true)),
        Op::FlatProjection,
        Op::Program {
            prog: bslot::RESOLVE_PROG,
        },
        Op::Tex {
            unit: 0,
            tex: bslot::WEIGHT_TEX,
        },
        Op::TexRangeNull { first: 1 },
        p4(x::DRAW_ACCUM_QUAD, 0, 0),
        Op::NormalFrustumProjection,
    ]));
    v.extend((0..i.gbuffer_textures).map(|k| o(p4(x::TEX_BUFFER, k, k))));
    v.extend(ops([
        Op::TexRangeNull {
            first: i.gbuffer_textures,
        },
        state(State::Blend, st::BLEND_ADD),
        Op::P3(P3Op::Cull(false)),
        state(State::DepthTest, 1),
    ]));
    v
}

/// `GetViewMatrix().GetRotation()` times a vector.
fn rotate(view: &Matrixf, v: Vector3f) -> Vector3f {
    let mut r = *view;
    for row in &mut r.m {
        row[3] = 0.0;
    }
    r.m[3] = [0.0, 0.0, 0.0, 1.0];
    matrix_mul_vec(&r, v)
}
