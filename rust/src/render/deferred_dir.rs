//! `cRendererDeferred` directional light (`RenderLights_Directional`, `RenderDirectionalShadowMap`)
//! and the weighted box light pass (`RenderLights_BoxWeighted`).

use std::cmp::Ordering;
use std::hint::black_box;

use super::deferred::{DlOp, NONE, Op, P3Op, Plan, State, c_max, c_min, cdiv, st, state};
use super::deferred_fx::cmath::{matrix_mul, matrix_mul_vec};
use super::deferred_fx::{Item, Seq, ask, mat, o, ops, p4, p4f, x};
use super::deferred_light::{feature, linear_light_color, underwater_fog_color};
use super::deferred_pass::pshape;
use crate::math::{
    Matrixf, Vector3f, cross3, div3, dot3, len3, matrix_mul_3x3, normalize3,
    orthographic_projection, sub3, tan, v3,
};
use crate::types::Vector2f;

unsafe extern "C" {
    fn powf(x: f32, y: f32) -> f32;
}

fn pow(x: f32, y: f32) -> f32 {
    // SAFETY: pure libm function on plain floats.
    unsafe { powf(x, y) }
}

/// `a + b` with gcc's operand order for NaNs: the first NaN operand wins, as in the hardware add.
fn add_first(a: f32, b: f32) -> f32 {
    if a.is_nan() { a } else { a + b }
}

/// `cMath::Interpolate`.
fn interpolate(a: f32, b: f32, t: f32) -> f32 {
    a.mul_add(1.0 - t, b * t)
}

/// `kVar_*` ids used here.
pub mod v5 {
    pub const AV_LIGHT_COLOR: i32 = 1;
    pub const AF_NEG_FAR_PLANE: i32 = 3;
    pub const AV_SHADOW_MAP_OFFSET_MUL: i32 = 8;
    pub const AF_FAR_PLANE: i32 = 10;
    pub const AV_FOG_COLOR: i32 = 15;
    pub const A_MTX_INV_VIEW: i32 = 32;
    pub const AV_AMBIENT_COLOR_SKY: i32 = 35;
    pub const AV_AMBIENT_COLOR_GROUND: i32 = 36;
    pub const AV_BOX_CENTER: i32 = 37;
    pub const AV_BOX_EXTENT: i32 = 38;
    pub const AF_FALLOFF: i32 = 39;
    pub const AF_BEVEL: i32 = 40;
    pub const AF_WEIGHT: i32 = 41;
    pub const AV_BAND0: i32 = 43;
    pub const AF_TRANSLUCENCY_SCALE: i32 = 69;
    pub const AV_LIGHT_DIRECTION: i32 = 78;
    pub const A_MTX_LIGHT_VIEW_PROJ0: i32 = 87;
    pub const AV_SPLITS_NEAR: i32 = 91;
    pub const AV_SPLITS_FAR: i32 = 92;
    pub const AV_SPLIT_OFFSET_MUL: i32 = 93;
    pub const AV_VIEW_SPACE_UP: i32 = 94;
    pub const AV_SHADOW_PARAMS: i32 = 110;
    pub const AV_SHADOW_MAP_SIZE: i32 = 111;
    pub const AF_SHADOW_DISTANCE: i32 = 112;
    pub const AL_SLICE_NUM: i32 = 113;
    pub const AF_SLICE_DIV: i32 = 114;
    pub const A_MTX_LIGHT_VIEW_PROJ_SLICE0: i32 = 115;
}

pub mod dslot {
    pub const PROG: i32 = 0;
    pub const SHADOW_TEX: i32 = 0;
    pub const JITTER_TEX: i32 = 1;
}

const SLICES: usize = 4;
const VARIABILITY_STATIC: i32 = 1;
const VARIABILITY_DYNAMIC: i32 = 2;
const FEATURE_DIRECTIONAL: u32 = 1 << 12;

/// Everything `RenderLights_Directional` and `RenderDirectionalShadowMap` read.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DirIn {
    pub active: i32,
    pub brightness: f32,
    pub diffuse: [f32; 4],
    pub cast_specular: i32,
    pub cast_diffuse: i32,
    pub translucency: f32,
    pub underwater: i32,
    pub cast_shadows: i32,
    pub render_shadows: i32,
    pub hdr: i32,
    pub ambient_sky: [f32; 4],
    pub ambient_ground: [f32; 4],
    pub direction: [f32; 3],
    pub shadow_blur: f32,
    pub frustum_far: f32,
    pub frustum_near: f32,
    pub fov: f32,
    pub aspect: f32,
    pub view: Matrixf,
    pub inv_view: Matrixf,
    pub fog_active: i32,
    pub fog_color: [f32; 4],
    pub fog_end: f32,
    pub shadow_distance: f32,
    pub slice_log_term: f32,
    pub caster_distance: f32,
    pub bias_mul: f32,
    pub slope_bias_mul: f32,
    pub casters_affected: i32,
    pub map_bias: f32,
    pub map_slope_bias: f32,
    pub shadow_exists: i32,
    pub shadow_res: i32,
    pub depth_clamp: i32,
    pub sun_caster_cull: i32,
    pub jitter_tex: i32,
    pub far_left: f32,
    pub far_bottom: f32,
    pub far_right: f32,
    pub far_top: f32,
    pub far_plane: f32,
    pub gbuffer_textures: i32,
}

struct Shadow {
    mtx: [Matrixf; SLICES],
    near: [f32; SLICES],
    far: [f32; SLICES],
    offset_mul: [f32; SLICES],
    slice_mtx: [Matrixf; SLICES],
    distance: f32,
    depth0: f32,
}

pub(super) fn vec3(v: Vector3f) -> [f32; 3] {
    [v.x, v.y, v.z]
}

/// `cRendererDeferred::RenderLights_Directional`.
pub fn plan_directional(i: DirIn) -> Plan {
    if i.active == 0 || i.brightness <= 0.0 {
        return Seq::plan(Vec::new());
    }
    let mut flags = FEATURE_DIRECTIONAL;
    if i.diffuse[3] > 0.0 && i.cast_specular != 0 {
        flags |= feature::SPECULAR;
    }
    if i.translucency > 0.0 {
        flags |= feature::TRANSLUCENCY;
    }
    if i.underwater != 0 {
        flags |= feature::UNDERWATER_FOG;
    }
    if i.cast_shadows != 0 && i.render_shadows != 0 {
        flags |= feature::SHADOW_MAP;
        let mut v = Vec::new();
        if i.shadow_exists == 0 {
            let size = match i.shadow_res {
                2 => 2048,
                1 => 1024,
                _ => 512,
            };
            v.push(o(p4(x::DIR_SHADOW_CREATE, size, 0)));
        }
        v.push(ask(p4(x::ASK_DIR_SHADOW_SIZE, 0, 0), move |f| {
            let size = f.vec3();
            shadow_map(&i, size, flags)
        }));
        return Seq::plan(v);
    }
    Seq::plan(program(&i, flags, None))
}

fn shadow_map(i: &DirIn, size: [f32; 3], flags: u32) -> Vec<Item> {
    let slice_res = cdiv(size[0] as i32, 2);
    let mut v = Vec::new();
    let (near, mut far) = (i.frustum_near, i.frustum_far);
    if i.fog_active != 0 && f64::from(i.fog_color[3]) >= 0.9 {
        far = i.fog_end;
    }
    let dist = i.shadow_distance;
    if dist != -1.0 {
        far = c_min(far, dist);
    }
    let mut sh = Shadow {
        mtx: [Matrixf::default(); SLICES],
        near: [0.0; SLICES],
        far: [0.0; SLICES],
        offset_mul: [0.0; SLICES],
        slice_mtx: [Matrixf::default(); SLICES],
        distance: if dist == -1.0 { far } else { dist },
        depth0: 0.0,
    };
    let mut split = [0.0f32; SLICES + 1];
    for (k, s) in split.iter_mut().enumerate() {
        let t = k as f32 / SLICES as f32;
        *s = interpolate(
            (far - near).mul_add(t, near),
            near * pow(far / near, t),
            i.slice_log_term,
        );
    }
    let d = i.direction;
    let mut back = v3(black_box(-d[0]), black_box(-d[1]), black_box(-d[2]));
    normalize3(&mut back);
    let up0 = if back.y.abs() > 0.99 {
        v3(1.0, 0.0, 0.0)
    } else {
        v3(0.0, 1.0, 0.0)
    };
    let mut right = cross3(up0, back);
    normalize3(&mut right);
    let up = cross3(back, right);
    let tan_half_fov = tan(i.fov * 0.5);
    let caster = i.caster_distance;

    v.extend(ops([
        state(State::DepthFunc, st::FUNC_LESS_OR_EQUAL),
        state(State::DepthTest, 1),
        state(State::DepthWrite, 1),
        state(State::Blend, st::BLEND_NONE),
        state(State::Alpha, st::ALPHA_SOLID),
        p4(x::ALPHA_LIMIT_DEFAULT, 0, 0),
        state(State::Channel, st::CHANNEL_NONE),
        Op::TexRangeNull { first: 0 },
        p4(x::OCCLUSION_PLANES, 0, 0),
        p4(x::POLYGON_OFFSET_ACTIVE, 1, 0),
    ]));
    if i.depth_clamp != 0 {
        v.push(o(p4(x::DEPTH_CLAMP_ACTIVE, 1, 0)));
    }
    v.extend(ops([
        p4(x::CULL_ACTIVE, 0, 0),
        p4f(
            x::POLYGON_OFFSET,
            0,
            0,
            0,
            0,
            [
                i.map_bias * i.bias_mul,
                i.map_slope_bias * i.slope_bias_mul,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
            ],
        ),
        p4(x::DIR_SHADOW_FB, 0, 0),
        Op::ClearDepthValue { v: 1.0 },
        Op::ClearFb {
            flags: super::deferred::clear::DEPTH,
            use_pos: false,
        },
    ]));

    for k in 0..SLICES {
        let (lo, hi) = (
            split.get(k).copied().unwrap_or(0.0),
            split.get(k + 1).copied().unwrap_or(0.0),
        );
        let mut corners = [Vector3f::default(); 8];
        let mut center = Vector3f::default();
        for (j, c) in corners.iter_mut().enumerate() {
            let z = split.get(k + (j >> 2)).copied().unwrap_or(0.0);
            let h = z * tan_half_fov;
            let sx = if j & 1 != 0 { 1.0 } else { -1.0 };
            let sy = if j & 2 != 0 { 1.0 } else { -1.0 };
            *c = matrix_mul_vec(&i.inv_view, v3(black_box(sx * h) * i.aspect, sy * h, -z));
            let e = div3(*c, 8.0);
            center = v3(center.x + e.x, center.y + e.y, center.z + e.z);
        }
        let mut radius = 0.0f32;
        for c in &corners {
            radius = c_max(radius, len3(sub3(center, *c)));
        }
        radius = (radius * 16.0).ceil() / 16.0;

        let mut vc = v3(dot3(right, center), dot3(up, center), dot3(back, center));
        let texel = 2.0 * radius / slice_res as f32;
        vc.x = (vc.x / texel).floor() * texel;
        vc.y = (vc.y / texel).floor() * texel;
        vc.z += radius + caster;
        let view = Matrixf {
            m: [
                [right.x, right.y, right.z, -vc.x],
                [up.x, up.y, up.z, -vc.y],
                [back.x, back.y, back.z, -vc.z],
                [0.0, 0.0, 0.0, 1.0],
            ],
        };
        let depth = add_first(2.0 * radius, caster);
        let proj = orthographic_projection(
            0.0,
            depth,
            Vector2f {
                x: 2.0 * radius,
                y: 2.0 * radius,
            },
        );
        let eye = v3(
            back.x.mul_add(vc.z, right.x.mul_add(vc.x, up.x * vc.y)),
            back.y.mul_add(vc.z, right.y.mul_add(vc.x, up.y * vc.y)),
            back.z.mul_add(vc.z, right.z.mul_add(vc.x, up.z * vc.y)),
        );
        let mut data: Vec<f32> = proj.m.iter().flatten().copied().collect();
        data.extend(view.m.iter().flatten().copied());
        let slice = k as i32;
        v.push(Item::Arr(
            data,
            Box::new(move |arr| {
                p4f(
                    x::DIR_FRUSTUM,
                    slice,
                    arr,
                    0,
                    0,
                    [
                        depth,
                        2.0 * radius,
                        2.0 * radius,
                        eye.x,
                        eye.y,
                        eye.z,
                        0.0,
                        0.0,
                    ],
                )
            }),
        ));
        let bias = if i.sun_caster_cull != 0 {
            4.0 * texel
        } else {
            0.0
        };
        let mut which = 0;
        if i.casters_affected & VARIABILITY_DYNAMIC != 0 {
            which |= 1;
        }
        if i.casters_affected & VARIABILITY_STATIC != 0 {
            which |= 2;
        }
        let slice_i = k as i32;
        v.extend(ops([
            p4f(
                x::DIR_CASTERS,
                slice_i,
                which,
                i32::from(i.sun_caster_cull != 0),
                0,
                [bias, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
            ),
            p4f(
                x::DIR_SET_FB,
                (slice_i & 1) * slice_res,
                (1 - (slice_i >> 1)) * slice_res,
                slice_res,
                0,
                [0.0; 8],
            ),
            p4(x::DIR_RENDER, slice_i, 0),
        ]));

        let atlas = Matrixf {
            m: [
                [0.25, 0.0, 0.0, 0.25 + 0.5 * (slice_i & 1) as f32],
                [0.0, 0.25, 0.0, 0.25 + 0.5 * (slice_i >> 1) as f32],
                [0.0, 0.0, 0.5, 0.5],
                [0.0, 0.0, 0.0, 1.0],
            ],
        };
        let pv = matrix_mul(&proj, &view);
        let slot = sh.mtx.get_mut(k);
        if let Some(m) = slot {
            *m = matrix_mul(&matrix_mul(&atlas, &pv), &i.inv_view);
        }
        let bias_m = Matrixf {
            m: [
                [0.5, 0.0, 0.0, 0.5],
                [0.0, 0.5, 0.0, 0.5],
                [0.0, 0.0, 0.5, 0.5],
                [0.0, 0.0, 0.0, 1.0],
            ],
        };
        if let Some(m) = sh.slice_mtx.get_mut(k) {
            *m = matrix_mul(&matrix_mul(&bias_m, &pv), &i.inv_view);
        }
        if k == 0 {
            sh.depth0 = depth;
        }
        if let Some(n) = sh.near.get_mut(k) {
            *n = -lo;
        }
        if let Some(f) = sh.far.get_mut(k) {
            *f = if k == SLICES - 1 { -1e6 } else { -hi };
        }
        if let Some(m) = sh.offset_mul.get_mut(k) {
            *m = (split.get(1).copied().unwrap_or(0.0) - split.first().copied().unwrap_or(0.0))
                / (hi - lo);
        }
    }

    v.extend(ops([
        Op::Tex { unit: 0, tex: NONE },
        p4(x::OCCLUSION_PLANES, 1, 0),
        p4(x::POLYGON_OFFSET_ACTIVE, 0, 0),
    ]));
    if i.depth_clamp != 0 {
        v.push(o(p4(x::DEPTH_CLAMP_ACTIVE, 0, 0)));
    }
    v.extend(ops([
        p4(x::CULL_ACTIVE, 1, 0),
        Op::NormalFrustumProjection,
        Op::SetAccumBuffer,
    ]));
    v.extend((0..i.gbuffer_textures).map(|k| o(p4(x::TEX_BUFFER, k, k))));
    v.extend(ops([
        state(State::DepthFunc, st::FUNC_GREATER_OR_EQUAL),
        state(State::DepthWrite, 0),
        state(State::Channel, st::CHANNEL_RGBA),
        state(State::Blend, st::BLEND_ADD),
        state(State::Alpha, st::ALPHA_SOLID),
    ]));
    v.extend(program(i, flags, Some((sh, size))));
    v
}

/// The program generation and uniforms of `RenderLights_Directional`.
fn program(i: &DirIn, flags: u32, shadow: Option<(Shadow, [f32; 3])>) -> Vec<Item> {
    let i = *i;
    vec![ask(
        Op::GenProgram {
            slot: dslot::PROG,
            mode: 0,
            flags,
        },
        move |f| {
            if !f.flag() {
                return Vec::new();
            }
            directional_draw(&i, flags, shadow)
        },
    )]
}

fn directional_draw(i: &DirIn, flags: u32, shadow: Option<(Shadow, [f32; 3])>) -> Vec<Item> {
    use dslot::PROG as pr;
    let hdr = i.hdr != 0;
    let b = i.brightness;
    let mut lc = linear_light_color(i.diffuse, hdr);
    if i.cast_specular == 0 {
        lc[3] = 0.0;
    }
    if i.cast_diffuse == 0 {
        lc[3] = -lc[3];
    }
    lc[0] *= b;
    lc[1] *= b;
    lc[2] *= b;
    let scale = |c: [f32; 4]| {
        let l = linear_light_color(c, hdr);
        [l[0] * b, l[1] * b, l[2] * b]
    };
    let dir = v3(
        black_box(-i.direction[0]),
        black_box(-i.direction[1]),
        black_box(-i.direction[2]),
    );
    let mut v = ops([
        p4(x::LOG, 0, 0),
        Op::Program { prog: pr },
        Op::Float {
            prog: pr,
            id: v5::AF_NEG_FAR_PLANE,
            v: -i.frustum_far,
        },
        Op::ScreenPositionVars { prog: pr },
        Op::Vec4 {
            prog: pr,
            id: v5::AV_LIGHT_COLOR,
            v: lc,
        },
        Op::Vec3 {
            prog: pr,
            id: v5::AV_AMBIENT_COLOR_SKY,
            v: scale(i.ambient_sky),
        },
        Op::Vec3 {
            prog: pr,
            id: v5::AV_AMBIENT_COLOR_GROUND,
            v: scale(i.ambient_ground),
        },
        Op::Vec3 {
            prog: pr,
            id: v5::AV_LIGHT_DIRECTION,
            v: vec3(matrix_mul_3x3(&i.view, dir)),
        },
        Op::Vec3 {
            prog: pr,
            id: v5::AV_VIEW_SPACE_UP,
            v: vec3(matrix_mul_3x3(&i.view, v3(0.0, 1.0, 0.0))),
        },
        Op::Float {
            prog: pr,
            id: v5::AF_TRANSLUCENCY_SCALE,
            v: i.translucency * i.translucency * 0.5,
        },
    ]);
    if i.underwater != 0 {
        v.push(o(Op::Vec4 {
            prog: pr,
            id: v5::AV_FOG_COLOR,
            v: underwater_fog_color(i.fog_color),
        }));
    }
    if let Some((sh, size)) = shadow.filter(|_| flags & feature::SHADOW_MAP != 0) {
        v.extend(shadow_uniforms(i, sh, size));
        return v;
    }
    v.extend(draw_tail(i));
    v
}

fn shadow_uniforms(i: &DirIn, sh: Shadow, size: [f32; 3]) -> Vec<Item> {
    let i = *i;
    use dslot::PROG as pr;
    let mut v = Vec::new();
    for (k, m) in sh.mtx.iter().enumerate() {
        v.push(mat(pr, v5::A_MTX_LIGHT_VIEW_PROJ0 + k as i32, m));
    }
    let (w, h) = (size[0], size[1]);
    let depth0 = sh.depth0;
    let (jitter, blur) = (i.jitter_tex != 0, i.shadow_blur);
    let tail = move |sh: Shadow| {
        let mut t = ops([
            Op::Vec4 {
                prog: pr,
                id: v5::AV_SPLITS_NEAR,
                v: sh.near,
            },
            Op::Vec4 {
                prog: pr,
                id: v5::AV_SPLITS_FAR,
                v: sh.far,
            },
            Op::Vec4 {
                prog: pr,
                id: v5::AV_SPLIT_OFFSET_MUL,
                v: sh.offset_mul,
            },
            Op::Vec2 {
                prog: pr,
                id: v5::AV_SHADOW_MAP_OFFSET_MUL,
                v: [blur / w, blur / w],
            },
            Op::Tex {
                unit: 6,
                tex: dslot::SHADOW_TEX,
            },
        ]);
        if jitter {
            t.push(o(Op::Tex {
                unit: 7,
                tex: dslot::JITTER_TEX,
            }));
        }
        t.extend(draw_tail(&i));
        t
    };
    v.push(ask(
        p4f(x::INT_ASK, pr, v5::AL_SLICE_NUM, 4, 0, [0.0; 8]),
        move |f| {
            let mut r = Vec::new();
            if f.flag() {
                for (k, m) in sh.slice_mtx.iter().enumerate() {
                    r.push(mat(pr, v5::A_MTX_LIGHT_VIEW_PROJ_SLICE0 + k as i32, m));
                }
                r.extend(ops([
                    Op::Float {
                        prog: pr,
                        id: v5::AF_SLICE_DIV,
                        v: 0.25,
                    },
                    Op::Float {
                        prog: pr,
                        id: v5::AF_SHADOW_DISTANCE,
                        v: sh.distance,
                    },
                    Op::Vec4 {
                        prog: pr,
                        id: v5::AV_SHADOW_PARAMS,
                        v: [0.0, depth0, 24.0 * depth0, 0.0],
                    },
                    Op::Vec2 {
                        prog: pr,
                        id: v5::AV_SHADOW_MAP_SIZE,
                        v: [w, h],
                    },
                ]));
            }
            r.extend(tail(sh));
            r
        },
    ));
    v
}

fn draw_tail(i: &DirIn) -> Vec<Item> {
    ops([
        Op::P3(P3Op::StencilActive(false)),
        state(State::DepthTest, 0),
        Op::P3(P3Op::Cull(true)),
        Op::FlatProjectionMinMax {
            min: [i.far_left, i.far_bottom, black_box(-i.far_plane) * 1.5],
            max: [i.far_right, i.far_top, i.far_plane * 1.5],
        },
        Op::Dl(DlOp::SetShape(pshape::FULLSCREEN_QUAD)),
        Op::Dl(DlOp::DrawCurrent),
        Op::NormalFrustumProjection,
        Op::P3(P3Op::Cull(false)),
        state(State::DepthTest, 1),
    ])
}

/// One weighted box light; `idx` is its slot in the temp light array.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct BoxLightIn {
    pub id: u64,
    pub idx: i32,
    pub blend_func: i32,
    pub prio: i32,
    pub spherical: i32,
    pub diffuse: [f32; 4],
    pub brightness: f32,
    pub fade: f32,
    pub ambient_sky: [f32; 4],
    pub ambient_ground: [f32; 4],
    pub pos: [f32; 3],
    pub probe_offset: [f32; 3],
    pub size: [f32; 3],
    pub falloff: f32,
    pub bevel: f32,
    pub weight: f32,
    pub bands: [[f32; 3]; 9],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct BoxIn {
    pub buffer_ok: i32,
    pub resolve_ok: i32,
    pub is_reflection: i32,
    pub prog_ok: i32,
    pub instance_inputs: i32,
    pub underwater: i32,
    pub hdr: i32,
    pub frustum_far: f32,
    pub origin: [f32; 3],
    pub view: Matrixf,
    pub fog_color: [f32; 4],
    pub gbuffer_textures: i32,
}

fn box_rank(b: i32) -> i32 {
    match b.clamp(0, 2) {
        0 => 0,
        1 => 2,
        _ => 1,
    }
}

pub(super) fn box_order(a: &BoxLightIn, b: &BoxLightIn) -> Ordering {
    let (la, lb) = (box_rank(a.blend_func), box_rank(b.blend_func));
    if la != lb {
        return la.cmp(&lb);
    }
    if a.prio != b.prio {
        return a.prio.cmp(&b.prio);
    }
    a.id.cmp(&b.id)
}
