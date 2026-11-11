//! `cRendererDeferred` part E (G6e): the per-object passes (Z, G-buffer, decals, illumination,
//! translucent with its refraction and world reflection set-up), the sky box and the debug content
//! passes.
//!
//! Same plan pattern as `deferred_fx`: `Seq` items whose `Ask` continuations consume the shim's
//! answers. Objects are named by `(list, index)` into the current render list; the shim performs
//! the material and renderable virtual calls (`y` ids of `P4`).

use super::deferred::{
    NONE, Op, P4, Pass, Plan, State, c_max, cdiv, comp, fixed, pass_ops, st, state, tan,
};
use super::deferred_fx::{Item, Seq, ask, cmath, o, p4, p4f, pass_begin, pass_end, x};
use super::deferred_lightlist::FrustumIn;
use crate::math::{Matrixf, Planef, Rect2l, cross3, dot3, mul3, sub3};
use crate::types::{Vector2l, Vector3f};

unsafe extern "C" {
    fn ohpl_math_get_normalized_clip_rect_from_bv(
        bv_min: *const Vector3f,
        bv_max: *const Vector3f,
        view: *const Matrixf,
        proj: *const Matrixf,
        perspective: bool,
        fov: f32,
        near: f32,
        aspect: f32,
        tan_half_fov: f32,
        dest_min: *mut Vector3f,
        dest_max: *mut Vector3f,
    ) -> bool;
    fn ohpl_math_get_clip_rect_from_normalized_min_max(
        min: *const Vector3f,
        max: *const Vector3f,
        screen: *const Vector2l,
        out: *mut Rect2l,
    );
    fn ohpl_math_matrix_plane_mirror(p: *const Planef, out: *mut Matrixf);
    fn ohpl_math_projection_matrix_oblique_near_clip_plane(
        m: *const Matrixf,
        p: *const Planef,
        out: *mut Matrixf,
    );
    fn ohpl_math_transform_plane(m: *const Matrixf, p: *const Planef, out: *mut Planef);
}

/// `P4` ids of the object passes.
pub mod y {
    pub const OBJ_PROGRAM: i32 = 100;
    pub const OBJ_TEXTURES: i32 = 101;
    pub const OBJ_MATRIX_PTR: i32 = 102;
    pub const OBJ_VBUF: i32 = 103;
    pub const OBJ_DRAW_MAT: i32 = 104;
    pub const OBJ_Z: i32 = 105;
    pub const OBJ_TEX_UNIT: i32 = 106;
    pub const ASK_OBJ_BLEND: i32 = 107;
    pub const SET_ALPHA_LIMIT: i32 = 108;
    pub const SKY_MATRIX: i32 = 109;
    pub const SKY_TEXTURE: i32 = 110;
    pub const SKY_VBUF: i32 = 111;
    pub const QUAD2: i32 = 112;
    pub const TEX_REFLECTION: i32 = 113;
    pub const SET_WORLD_FOG: i32 = 114;
    pub const ASK_WORLD_FOG: i32 = 115;
    pub const ASK_COMPILED_FOG: i32 = 116;
    pub const OBJ_COMPILE: i32 = 117;
    pub const ASK_TRANS_INFO: i32 = 118;
    pub const ASK_FOG_VIS: i32 = 119;
    pub const SET_TEMP_FOG: i32 = 120;
    pub const ASK_UPDATE_GRAPHICS: i32 = 121;
    pub const ASK_RETRIEVE_QUERY: i32 = 122;
    pub const OBJ_MODEL_MATRIX: i32 = 123;
    pub const SET_MATRIX_CTX: i32 = 124;
    pub const ASK_PLANE_VISIBLE: i32 = 125;
    pub const WAIT_RETRIEVE_QUERIES: i32 = 126;
    pub const ASK_CLIP_RECT_OBJ: i32 = 127;
    pub const ASK_BV_MIN_MAX: i32 = 128;
    pub const FLAT_PROJECTION_SIZE: i32 = 129;
    pub const QUAD_CLIP: i32 = 130;
    pub const COPY_REFRACTION_RECT: i32 = 131;
    pub const COPY_REFRACTION_FULL: i32 = 132;
    pub const SET_DOF: i32 = 133;
    pub const ASK_REFL_INFO: i32 = 134;
    pub const ASK_PLANE_BV: i32 = 135;
    pub const ASK_REFL_CLEARED: i32 = 136;
    pub const REFL_SET_FB: i32 = 137;
    pub const REFL_SET_CLEARED: i32 = 138;
    pub const REFL_LOG: i32 = 139;
    pub const REFL_BEGIN: i32 = 140;
    pub const ASK_REFL_TARGET: i32 = 141;
    pub const ASK_REFL_SURFACE: i32 = 142;
    pub const REFL_FRUSTUM: i32 = 143;
    pub const REFL_RESET_PLANES: i32 = 144;
    pub const REFL_ADD_PLANE: i32 = 145;
    pub const REFL_SCISSOR: i32 = 146;
    pub const REFL_TARGET: i32 = 147;
    pub const REFL_RENDER: i32 = 148;
    pub const REFL_END: i32 = 149;
}

/// `pass_begin` ids of the object passes.
pub mod pass5 {
    pub const EARLY_Z: i32 = 16;
    pub const GBUFFER: i32 = 17;
    pub const GBUFFER_CONTENT: i32 = 18;
    pub const DECALS: i32 = 19;
    pub const ILLUMINATION: i32 = 20;
    pub const SKYBOX: i32 = 21;
    pub const REFLECTION_CONTENT: i32 = 22;
    pub const TRANSLUCENT: i32 = 23;
}

/// `REFL_LOG` kinds.
pub mod rlog {
    pub const SETUP: i32 = 0;
    pub const CLEAR_BEGIN: i32 = 1;
    pub const CLEAR_END: i32 = 2;
    pub const CLIP_LIMITS: i32 = 3;
    pub const SCISSOR: i32 = 4;
}

/// `eRenderListType_*`.
pub mod list {
    pub const Z: i32 = 0;
    pub const DIFFUSE: i32 = 1;
    pub const TRANSLUCENT: i32 = 2;
    pub const DECAL: i32 = 3;
    pub const ILLUMINATION: i32 = 4;
}

/// `eMaterialRenderMode_*`.
pub mod mode {
    pub const DIFFUSE: i32 = 2;
    pub const DIFFUSE_FOG: i32 = 3;
    pub const ILLUMINATION: i32 = 5;
    pub const ILLUMINATION_FOG: i32 = 6;
}

/// `eRenderableType_*` and `eCollision_*` values the translucent pass compares.
pub mod enums {
    pub const RENDERABLE_PARTICLE_EMITTER: u32 = 0;
    pub const RENDERABLE_SUB_MESH: u32 = 1;
    pub const COLLISION_OUTSIDE: i32 = 1;
}

/// Word layout of the `ASK_TRANS_INFO` answer.
pub mod info {
    pub const AFFECTED_BY_DOF: usize = 0;
    pub const HAS_REFRACTION: usize = 1;
    pub const HAS_WORLD_REFLECTION: usize = 2;
    pub const AFFECTED_BY_FOG: usize = 3;
    pub const HAS_TRANSLUCENT_ILLUMINATION: usize = 4;
    pub const USE_REFRACTION_EDGE_CHECK: usize = 5;
    pub const DEPTH_TEST: usize = 6;
    pub const BLEND: usize = 7;
    pub const RENDER_TYPE: usize = 8;
    pub const BV_CENTER: usize = 9;
    pub const BV_RADIUS: usize = 12;
    pub const WORDS: usize = 13;
}

/// An object op: `a` is the render mode or other argument, `d` the texture unit.
fn obj(id: i32, a: i32, list: i32, idx: i32) -> Op {
    Op::P4(P4 {
        id,
        a,
        b: list,
        c: idx,
        ..P4::default()
    })
}

fn obj_unit(id: i32, a: i32, list: i32, idx: i32, unit: i32) -> Op {
    Op::P4(P4 {
        id,
        a,
        b: list,
        c: idx,
        d: unit,
        ..P4::default()
    })
}

fn draw_current() -> Op {
    Op::Dl(super::deferred::DlOp::DrawCurrent)
}

fn alpha_limit(v: f32) -> Op {
    p4f(y::SET_ALPHA_LIMIT, 0, 0, 0, 0, f8([v]))
}

fn f8<const N: usize>(v: [f32; N]) -> [f32; 8] {
    let mut out = [0.0; 8];
    for (d, s) in out.iter_mut().zip(v) {
        *d = s;
    }
    out
}

fn count(n: i32) -> impl Iterator<Item = i32> {
    0..n.max(0)
}

pub fn plan_z(n: i32) -> Plan {
    let mut v = vec![pass_begin(pass5::EARLY_Z)];
    v.extend(pass_ops(Pass::ZBegin));
    v.extend(count(n).map(|i| obj(y::OBJ_Z, 0, list::Z, i)));
    v.push(pass_end(pass5::EARLY_Z));
    fixed(v)
}

pub fn plan_gbuffer(n: i32) -> Plan {
    let mut v = vec![pass_begin(pass5::GBUFFER)];
    v.extend(pass_ops(Pass::GbufferBegin));
    for i in count(n) {
        v.extend([
            obj(y::OBJ_PROGRAM, mode::DIFFUSE, list::DIFFUSE, i),
            obj(y::OBJ_TEXTURES, mode::DIFFUSE, list::DIFFUSE, i),
            obj(y::OBJ_MATRIX_PTR, 0, list::DIFFUSE, i),
            obj(y::OBJ_VBUF, 0, list::DIFFUSE, i),
            obj(y::OBJ_DRAW_MAT, mode::DIFFUSE, list::DIFFUSE, i),
        ]);
    }
    v.extend(pass_ops(Pass::GbufferEnd));
    v.push(pass_end(pass5::GBUFFER));
    fixed(v)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DecalIn {
    pub count: i32,
    pub depth_in_normal_alpha: bool,
}

pub fn plan_decals(i: DecalIn) -> Plan {
    if i.count <= 0 {
        return fixed(Vec::new());
    }
    let mut items: Vec<Item> = [
        pass_begin(pass5::DECALS),
        Op::SetGBuffer {
            comp: comp::COLOR_AND_DEPTH,
        },
        state(State::DepthTest, 1),
        state(State::DepthWrite, 0),
        alpha_limit(0.01),
        state(State::Alpha, st::ALPHA_TRANS),
        state(
            State::Channel,
            if i.depth_in_normal_alpha {
                st::CHANNEL_RGB
            } else {
                st::CHANNEL_RGBA
            },
        ),
        state(State::DepthFunc, st::FUNC_LESS_OR_EQUAL),
    ]
    .into_iter()
    .map(o)
    .collect();
    for idx in count(i.count) {
        items.push(ask(obj(y::ASK_OBJ_BLEND, 0, list::DECAL, idx), move |f| {
            vec![
                o(p4(x::BLEND_RAW, f.int(), 0)),
                o(obj(y::OBJ_PROGRAM, mode::DIFFUSE, list::DECAL, idx)),
                o(obj(y::OBJ_TEXTURES, mode::DIFFUSE, list::DECAL, idx)),
                o(obj(y::OBJ_MATRIX_PTR, 0, list::DECAL, idx)),
                o(obj(y::OBJ_VBUF, 0, list::DECAL, idx)),
                o(draw_current()),
            ]
        }));
    }
    items.extend([
        o(state(State::Alpha, st::ALPHA_SOLID)),
        o(p4(x::ALPHA_LIMIT_DEFAULT, 0, 0)),
        o(state(State::Channel, st::CHANNEL_RGBA)),
        o(pass_end(pass5::DECALS)),
    ]);
    Seq::plan(items)
}

pub fn plan_illumination(n: i32) -> Plan {
    if n <= 0 {
        return fixed(Vec::new());
    }
    let mut v = vec![
        pass_begin(pass5::ILLUMINATION),
        state(State::DepthTest, 1),
        state(State::DepthWrite, 0),
        state(State::DepthFunc, st::FUNC_EQUAL),
        state(State::Blend, st::BLEND_ADD),
        state(State::Alpha, st::ALPHA_SOLID),
        state(State::Channel, st::CHANNEL_RGBA),
        Op::TexRangeNull { first: 1 },
    ];
    for i in count(n) {
        v.extend([
            obj(y::OBJ_PROGRAM, mode::ILLUMINATION, list::ILLUMINATION, i),
            obj_unit(
                y::OBJ_TEX_UNIT,
                mode::ILLUMINATION,
                list::ILLUMINATION,
                i,
                0,
            ),
            obj(y::OBJ_MATRIX_PTR, 0, list::ILLUMINATION, i),
            obj(y::OBJ_VBUF, 0, list::ILLUMINATION, i),
            obj(y::OBJ_DRAW_MAT, mode::ILLUMINATION, list::ILLUMINATION, i),
        ]);
    }
    v.extend([
        state(State::DepthFunc, st::FUNC_LESS_OR_EQUAL),
        pass_end(pass5::ILLUMINATION),
    ]);
    fixed(v)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SkyIn {
    pub world_ok: bool,
    pub active: bool,
    pub far_plane: f32,
    pub rotation: Vector3f,
    pub origin: Vector3f,
}

/// `a * b` where a NaN `a` survives with its sign: gcc emits `fmul a, b` and aarch64 returns the first
/// quiet NaN, which LLVM's operand swap would lose.
fn first_nan_mul(a: f32, b: f32) -> f32 {
    if a.is_nan() {
        f32::from_bits(a.to_bits() | 0x0040_0000)
    } else {
        a * b
    }
}

/// `sqrtf` as the original's `sqrt(float)` call.
fn sqrt_f(x: f32) -> f32 {
    x.sqrt()
}

pub fn plan_skybox(i: SkyIn) -> Plan {
    if !i.world_ok || !i.active {
        return fixed(Vec::new());
    }
    let side = sqrt_f((i.far_plane * i.far_plane) / 3.0) * 0.95;
    let mut m = cmath::matrix_rotate(i.rotation, 0);
    for row in m.m.iter_mut().take(3) {
        for v in row.iter_mut().take(3) {
            *v = first_nan_mul(*v, side);
        }
    }
    for (r, v) in m.m.iter_mut().zip([i.origin.x, i.origin.y, i.origin.z]) {
        r[3] = v;
    }
    let mut items: Vec<Item> = [
        pass_begin(pass5::SKYBOX),
        Op::SetGBuffer {
            comp: comp::COLOR_AND_DEPTH,
        },
        state(State::DepthTest, 1),
        state(State::DepthWrite, 0),
        state(State::Blend, st::BLEND_NONE),
        state(State::Alpha, st::ALPHA_SOLID),
        state(State::Channel, st::CHANNEL_RGBA),
    ]
    .into_iter()
    .map(o)
    .collect();
    items.push(Item::Arr(
        m.m.iter().flatten().copied().collect(),
        Box::new(|arr| p4(y::SKY_MATRIX, arr, 0)),
    ));
    items.extend(
        [
            Op::Program { prog: NONE },
            p4(y::SKY_TEXTURE, 0, 0),
            Op::TexRangeNull { first: 1 },
            p4(y::SKY_VBUF, 0, 0),
            draw_current(),
            pass_end(pass5::SKYBOX),
        ]
        .into_iter()
        .map(o),
    );
    Seq::plan(items)
}

fn content_quad(pos: [f32; 2], size: [f32; 2], max: [f32; 2]) -> Op {
    p4f(
        y::QUAD2,
        0,
        0,
        0,
        0,
        f8([pos[0], pos[1], size[0], size[1], max[0], max[1]]),
    )
}

fn content_pass_head(id: i32) -> Vec<Op> {
    vec![
        pass_begin(id),
        state(State::DepthTest, 0),
        state(State::DepthWrite, 0),
        state(State::Blend, st::BLEND_NONE),
        state(State::Alpha, st::ALPHA_SOLID),
        state(State::Channel, st::CHANNEL_RGBA),
        Op::SetAccumBuffer,
    ]
}

pub fn plan_gbuffer_content(screen: [f32; 2], num_textures: i32) -> Plan {
    let mut v = content_pass_head(pass5::GBUFFER_CONTENT);
    v.extend([
        Op::ClearFb {
            flags: super::deferred::clear::DEPTH | super::deferred::clear::COLOR,
            use_pos: false,
        },
        Op::FlatProjection,
        Op::Program { prog: NONE },
        Op::TexRangeNull { first: 1 },
    ]);
    let cells = [[0.0, 0.0], [0.5, 0.0], [0.0, 0.5], [0.5, 0.5]];
    for (idx, pos) in cells.iter().enumerate() {
        if idx == 3 && num_textures <= 3 {
            break;
        }
        v.push(p4(x::TEX_BUFFER, 0, idx as i32));
        v.push(content_quad(*pos, [0.5, 0.5], screen));
    }
    v.extend([
        Op::NormalFrustumProjection,
        pass_end(pass5::GBUFFER_CONTENT),
    ]);
    fixed(v)
}

pub fn plan_reflection_content(screen: [f32; 2], div: i32) -> Plan {
    let mut v = content_pass_head(pass5::REFLECTION_CONTENT);
    let d = div as f32;
    v.extend([
        Op::FlatProjection,
        Op::Program { prog: NONE },
        Op::TexRangeNull { first: 1 },
        p4(y::TEX_REFLECTION, 0, 0),
        content_quad([0.0, 0.0], [1.0, 1.0], [screen[0] / d, screen[1] / d]),
        Op::NormalFrustumProjection,
        pass_end(pass5::REFLECTION_CONTENT),
    ]);
    fixed(v)
}

/// The camera a translucent pass renders from.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CameraIn {
    pub f: FrustumIn,
    pub far_plane: f32,
    pub forward: Vector3f,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct TransIn {
    pub count: i32,
    pub debug_skip: i32,
    pub dof_pass: i32,
    pub depth_in_normal_alpha: bool,
    pub hdr: bool,
    pub is_reflection: bool,
    pub fog_active: bool,
    pub underwater: bool,
    pub secondary: bool,
    pub render_world_reflection: bool,
    pub clip_reflection_screen_rect: bool,
    pub settings_log: bool,
    pub occlusion_test_large_lights: bool,
    pub fog_area_num: i32,
    pub dof_start: f32,
    pub dof_end: f32,
    pub dof_falloff: f32,
    pub rt_size: Vector2l,
    pub camera: CameraIn,
}

type Rect = [i32; 4];

struct Tr {
    i: TransIn,
    world_fog: u32,
    refraction_valid: bool,
    drawn: Vec<Rect>,
    half_fov_tan: f32,
}

pub(super) fn rect_intersects(mut a: Rect, mut b: Rect) -> bool {
    for r in [&mut a, &mut b] {
        if r[2] < 0 {
            r[2] = r[2].wrapping_mul(-1);
            r[0] = r[0].wrapping_sub(r[2]);
        }
        if r[3] < 0 {
            r[3] = r[3].wrapping_mul(-1);
            r[1] = r[1].wrapping_sub(r[3]);
        }
    }
    let far = |p: Rect, q: Rect, k: usize| {
        let at = |r: Rect, i: usize| r.get(i).copied().unwrap_or(0);
        at(p, k) > at(q, k).wrapping_add(at(q, k + 2).wrapping_sub(1))
    };
    !(far(a, b, 0) || far(b, a, 0) || far(a, b, 1) || far(b, a, 1))
}

fn word(w: &[u32], i: usize) -> u32 {
    w.get(i).copied().unwrap_or(0)
}

fn flag(w: &[u32], i: usize) -> bool {
    word(w, i) != 0
}

fn fl(w: &[u32], i: usize) -> f32 {
    f32::from_bits(word(w, i))
}

fn rect_of(w: &[u32]) -> Rect {
    [0, 1, 2, 3].map(|i| word(w, i) as i32)
}

/// `GetClipRectFromBV`: the normalized limits, or the whole target when nothing is in front.
fn clip_rect_from_bv(
    min: Vector3f,
    max: Vector3f,
    f: &FrustumIn,
    size: Vector2l,
    tan_half_fov: f32,
) -> Rect {
    let (mut dmin, mut dmax) = (Vector3f::default(), Vector3f::default());
    // SAFETY: exported by crate::math; every pointer is a live local.
    let visible = unsafe {
        ohpl_math_get_normalized_clip_rect_from_bv(
            &min,
            &max,
            &f.view,
            &f.proj,
            f.perspective,
            f.fov,
            f.near,
            f.aspect,
            tan_half_fov,
            &mut dmin,
            &mut dmax,
        )
    };
    if !visible {
        return [0, 0, size.x, size.y];
    }
    let mut r = Rect2l::default();
    // SAFETY: as above.
    unsafe { ohpl_math_get_clip_rect_from_normalized_min_max(&dmin, &dmax, &size, &mut r) };
    [r.x, r.y, r.w, r.h]
}

fn next_obj(tr: Tr, idx: i32) -> Vec<Item> {
    vec![Item::Lazy(Box::new(move || obj_items(tr, idx)))]
}

/// `RenderTranslucent`.
pub fn plan_translucent(i: TransIn) -> Plan {
    if i.count <= 0 {
        return fixed(Vec::new());
    }
    let mut items: Vec<Item> = vec![
        o(pass_begin(pass5::TRANSLUCENT)),
        o(state(State::DepthTest, 1)),
        o(state(State::DepthWrite, 0)),
        o(alpha_limit(0.01)),
        o(state(State::Alpha, st::ALPHA_TRANS)),
    ];
    if i.depth_in_normal_alpha {
        items.push(o(p4(x::RED_FROM_ALPHA, 1, 0)));
    }
    items.push(o(p4(
        y::SET_WORLD_FOG,
        i32::from(i.underwater),
        i32::from(i.secondary),
    )));
    items.push(ask(p4(y::ASK_WORLD_FOG, 0, 0), move |f| {
        let tr = Tr {
            i,
            world_fog: f.int() as u32,
            refraction_valid: false,
            drawn: Vec::new(),
            half_fov_tan: 0.0,
        };
        next_obj(tr, 0)
    }));
    Seq::plan(items)
}

fn finish(i: &TransIn) -> Vec<Item> {
    let mut v = vec![
        o(state(State::Alpha, st::ALPHA_SOLID)),
        o(p4(x::ALPHA_LIMIT_DEFAULT, 0, 0)),
    ];
    if i.depth_in_normal_alpha {
        v.push(o(p4(x::RED_FROM_ALPHA, 0, 0)));
    }
    v.push(o(pass_end(pass5::TRANSLUCENT)));
    v
}

fn obj_items(tr: Tr, idx: i32) -> Vec<Item> {
    if idx >= tr.i.count {
        return finish(&tr.i);
    }
    if idx == tr.i.debug_skip {
        return next_obj(tr, idx + 1);
    }
    vec![ask(p4(y::ASK_COMPILED_FOG, idx, 0), move |f| {
        let mut v = Vec::new();
        if f.int() as u32 != tr.world_fog {
            v.push(o(p4(y::OBJ_COMPILE, idx, 0)));
        }
        v.push(ask(
            p4(y::ASK_TRANS_INFO, idx, i32::from(tr.i.dof_pass != 0)),
            move |f| after_info(tr, idx, f.words()),
        ));
        v
    })]
}

struct DofResult {
    blend: bool,
    params: [f32; 4],
}

/// The depth of field gate of the translucent pass; `None` skips the object.
fn dof_gate(tr: &Tr, w: &[u32]) -> Option<DofResult> {
    let i = &tr.i;
    let mut r = DofResult {
        blend: false,
        params: [0.0; 4],
    };
    if i.dof_pass != 0 && !flag(w, info::AFFECTED_BY_DOF) {
        if i.dof_pass == 1 {
            return None;
        }
    } else if i.dof_pass != 0 {
        let center = Vector3f {
            x: fl(w, info::BV_CENTER),
            y: fl(w, info::BV_CENTER + 1),
            z: fl(w, info::BV_CENTER + 2),
        };
        let radius = fl(w, info::BV_RADIUS);
        let depth = -cmath::matrix_mul_vec(&i.camera.f.view, center).z;
        let (start, end) = (i.dof_start, i.dof_end);
        let behind = depth - radius > end;
        r.blend = !behind && depth + radius > end && !flag(w, info::HAS_REFRACTION);
        if !r.blend && behind != (i.dof_pass == 1) {
            return None;
        }
        let falloff_len = (end - start) * 0.5 / c_max(1e-15, i.dof_falloff);
        r.params = [
            end,
            -1.0 / c_max(1e-6, falloff_len),
            if i.dof_pass == 1 { 0.0 } else { 1.0 },
            if i.dof_pass == 1 { 1.0 } else { -1.0 },
        ];
    }
    Some(r)
}

struct FogAcc {
    alpha: f32,
    color: [f32; 4],
}

fn fog_chain(
    tr: Tr,
    idx: i32,
    w: Vec<u32>,
    dof: DofResult,
    mut render_mode: i32,
    k: i32,
    acc: FogAcc,
) -> Vec<Item> {
    if k >= tr.i.fog_area_num {
        if acc.color[3] > 0.0 {
            render_mode = mode::DIFFUSE_FOG;
        }
        let mut v = vec![o(p4f(
            y::SET_TEMP_FOG,
            0,
            0,
            0,
            0,
            f8([
                acc.alpha,
                acc.color[0],
                acc.color[1],
                acc.color[2],
                acc.color[3],
            ]),
        ))];
        v.extend(update_object(tr, idx, w, dof, render_mode));
        return v;
    }
    vec![ask(p4(y::ASK_FOG_VIS, k, idx), move |f| {
        let a = f.floats();
        let g = |i: usize| a.get(i).copied().unwrap_or(0.0);
        let vis = g(0);
        let mut acc = acc;
        if !tr.i.hdr {
            acc.alpha *= vis;
        } else {
            let col = super::deferred_fx::fog_render_color(true, [g(1), g(2), g(3), g(4)], g(5));
            let fa = 1.0 - vis;
            let prev = acc.color[3] * (1.0 - fa);
            let new = prev + fa;
            if new > 0.0 || new.is_nan() {
                let inv = 1.0 / new;
                let mut c = [0.0f32; 4];
                for (n, (t, fc)) in c.iter_mut().zip(acc.color.iter().zip(col.iter())) {
                    *n = (t * prev + fc * fa) * inv;
                }
                c[3] = new;
                acc.color = c;
            }
        }
        fog_chain(tr, idx, w, dof, render_mode, k + 1, acc)
    })]
}

fn after_info(tr: Tr, idx: i32, w: Vec<u32>) -> Vec<Item> {
    let Some(dof) = dof_gate(&tr, &w) else {
        return next_obj(tr, idx + 1);
    };
    let mut render_mode = if tr.i.fog_active {
        mode::DIFFUSE_FOG
    } else {
        mode::DIFFUSE
    };
    if !flag(&w, info::AFFECTED_BY_FOG) {
        render_mode = mode::DIFFUSE;
    }
    if tr.i.is_reflection && flag(&w, info::HAS_WORLD_REFLECTION) {
        return next_obj(tr, idx + 1);
    }
    if flag(&w, info::AFFECTED_BY_FOG) {
        let acc = FogAcc {
            alpha: 1.0,
            color: [0.0; 4],
        };
        return fog_chain(tr, idx, w, dof, render_mode, 0, acc);
    }
    let mut v = vec![o(p4f(
        y::SET_TEMP_FOG,
        0,
        0,
        0,
        0,
        f8([1.0, 0.0, 0.0, 0.0, 0.0]),
    ))];
    v.extend(update_object(tr, idx, w, dof, render_mode));
    v
}

fn update_object(tr: Tr, idx: i32, w: Vec<u32>, dof: DofResult, render_mode: i32) -> Vec<Item> {
    vec![ask(p4(y::ASK_UPDATE_GRAPHICS, idx, 0), move |f| {
        if !f.flag() {
            return next_obj(tr, idx + 1);
        }
        vec![ask(p4(y::ASK_RETRIEVE_QUERY, idx, 0), move |f| {
            if !f.flag() {
                return next_obj(tr, idx + 1);
            }
            let mut v = vec![o(p4(y::OBJ_MODEL_MATRIX, idx, 0))];
            v.extend(world_reflection(tr, idx, w, dof, render_mode));
            v
        })]
    })]
}

fn world_reflection(tr: Tr, idx: i32, w: Vec<u32>, dof: DofResult, render_mode: i32) -> Vec<Item> {
    if !(flag(&w, info::HAS_WORLD_REFLECTION)
        && word(&w, info::RENDER_TYPE) == enums::RENDERABLE_SUB_MESH)
    {
        return refraction(tr, idx, w, dof, render_mode);
    }
    vec![ask(p4(y::ASK_PLANE_VISIBLE, idx, 0), move |f| {
        if !f.flag() {
            return next_obj(tr, idx + 1);
        }
        let mut v = vec![o(p4(y::WAIT_RETRIEVE_QUERIES, 0, 0))];
        if tr.i.occlusion_test_large_lights {
            v.push(o(Op::Step(super::deferred::Step::RetrieveLightOcclusion)));
        }
        if tr.i.depth_in_normal_alpha {
            v.push(o(p4(x::RED_FROM_ALPHA, 0, 0)));
        }
        v.extend(reflection(&tr.i, idx));
        let after = move || {
            let mut tr = tr;
            let mut v = Vec::new();
            if tr.i.depth_in_normal_alpha {
                v.push(o(p4(x::RED_FROM_ALPHA, 1, 0)));
            }
            tr.refraction_valid = false;
            v.extend(refraction(tr, idx, w, dof, render_mode));
            v
        };
        v.push(Item::Lazy(Box::new(after)));
        v
    })]
}

fn refraction(tr: Tr, idx: i32, w: Vec<u32>, dof: DofResult, render_mode: i32) -> Vec<Item> {
    if !flag(&w, info::HAS_REFRACTION) {
        return draw(tr, idx, w, dof, render_mode);
    }
    vec![ask(p4(y::ASK_PLANE_VISIBLE, idx, 0), move |f| {
        if !f.flag() {
            return next_obj(tr, idx + 1);
        }
        let mut tr = tr;
        if tr.half_fov_tan == 0.0 {
            tr.half_fov_tan = tan(tr.i.camera.f.fov * 0.5);
        }
        let t = tr.half_fov_tan;
        vec![ask(
            p4f(y::ASK_CLIP_RECT_OBJ, idx, 0, 0, 0, f8([0.2, t])),
            move |f| {
                let rect = rect_of(&f.words());
                refraction_copy(tr, idx, w, dof, render_mode, rect)
            },
        )]
    })]
}

fn refraction_copy(
    mut tr: Tr,
    idx: i32,
    w: Vec<u32>,
    dof: DofResult,
    render_mode: i32,
    r: Rect,
) -> Vec<Item> {
    let edge = flag(&w, info::USE_REFRACTION_EDGE_CHECK);
    let mut v: Vec<Item> = Vec::new();
    if edge {
        let [rx, ry, rw, rh] = r;
        let size = tr.i.rt_size;
        v.extend([
            o(p4f(
                y::FLAT_PROJECTION_SIZE,
                0,
                0,
                0,
                0,
                f8([size.x as f32, size.y as f32]),
            )),
            o(state(State::DepthTest, 0)),
            o(Op::Program { prog: NONE }),
            o(state(State::Blend, st::BLEND_NONE)),
            o(state(State::Alpha, st::ALPHA_SOLID)),
            o(state(State::Channel, st::CHANNEL_A)),
            o(Op::TexRangeNull { first: 0 }),
            o(p4f(
                y::QUAD_CLIP,
                0,
                0,
                0,
                0,
                f8([rx as f32, ry as f32, rw as f32, rh as f32]),
            )),
            o(Op::NormalFrustumProjection),
            o(state(State::DepthTest, 1)),
            o(p4(y::SET_MATRIX_CTX, 0, 0)),
            o(obj(y::OBJ_VBUF, 0, list::TRANSLUCENT, idx)),
            o(draw_current()),
            o(state(State::Channel, st::CHANNEL_RGBA)),
            o(state(State::Alpha, st::ALPHA_TRANS)),
        ]);
        if rw > 0 && rh > 0 {
            v.push(o(p4f(y::COPY_REFRACTION_RECT, rx, ry, rw, rh, [0.0; 8])));
        }
        tr.refraction_valid = false;
    } else {
        let size = tr.i.rt_size;
        if !tr.refraction_valid || tr.drawn.iter().any(|d| rect_intersects(*d, r)) {
            v.push(o(p4(y::COPY_REFRACTION_FULL, size.x, size.y)));
            tr.refraction_valid = true;
            tr.drawn.clear();
        }
    }
    v.extend(draw(tr, idx, w, dof, render_mode));
    v
}

fn draw(tr: Tr, idx: i32, w: Vec<u32>, dof: DofResult, render_mode: i32) -> Vec<Item> {
    let tl = list::TRANSLUCENT;
    let refr = flag(&w, info::HAS_REFRACTION);
    let mut v: Vec<Item> = Vec::new();
    if refr {
        v.push(o(state(State::Blend, st::BLEND_NONE)));
    } else {
        v.push(o(p4(x::BLEND_RAW, word(&w, info::BLEND) as i32, 0)));
    }
    v.extend([
        o(state(
            State::DepthTest,
            i32::from(flag(&w, info::DEPTH_TEST)),
        )),
        o(p4f(
            y::SET_DOF,
            i32::from(dof.blend),
            0,
            0,
            0,
            f8(dof.params),
        )),
        o(obj(y::OBJ_PROGRAM, render_mode, tl, idx)),
        o(obj(y::OBJ_TEXTURES, render_mode, tl, idx)),
        o(p4(y::SET_MATRIX_CTX, 0, 0)),
        o(obj(y::OBJ_VBUF, 0, tl, idx)),
        o(obj(y::OBJ_DRAW_MAT, render_mode, tl, idx)),
        o(p4(y::SET_DOF, 0, 0)),
    ]);
    if flag(&w, info::HAS_TRANSLUCENT_ILLUMINATION) && (!dof.blend || tr.i.dof_pass == 2) {
        let m = if render_mode == mode::DIFFUSE {
            mode::ILLUMINATION
        } else {
            mode::ILLUMINATION_FOG
        };
        v.extend([
            o(state(State::Blend, st::BLEND_ADD)),
            o(state(
                State::DepthTest,
                i32::from(flag(&w, info::DEPTH_TEST)),
            )),
            o(obj(y::OBJ_PROGRAM, m, tl, idx)),
            o(obj(y::OBJ_TEXTURES, m, tl, idx)),
            o(p4(y::SET_MATRIX_CTX, 0, 0)),
            o(obj(y::OBJ_VBUF, 0, tl, idx)),
            o(obj(y::OBJ_DRAW_MAT, m, tl, idx)),
        ]);
    }
    let particle = refr && word(&w, info::RENDER_TYPE) == enums::RENDERABLE_PARTICLE_EMITTER;
    if tr.refraction_valid && !particle {
        v.push(ask(p4(y::ASK_BV_MIN_MAX, idx, 0), move |f| {
            let mut tr = tr;
            let a = f.floats();
            let g = |i: usize| a.get(i).copied().unwrap_or(0.0);
            if tr.half_fov_tan == 0.0 {
                tr.half_fov_tan = tan(tr.i.camera.f.fov * 0.5);
            }
            let min = Vector3f {
                x: g(0),
                y: g(1),
                z: g(2),
            };
            let max = Vector3f {
                x: g(3),
                y: g(4),
                z: g(5),
            };
            let r = clip_rect_from_bv(min, max, &tr.i.camera.f, tr.i.rt_size, tr.half_fov_tan);
            tr.drawn.push(r);
            next_obj(tr, idx + 1)
        }));
    } else {
        v.extend(next_obj(tr, idx + 1));
    }
    v
}

fn v3(x: f32, y: f32, z: f32) -> Vector3f {
    Vector3f { x, y, z }
}

fn plane_from_normal_point(n: Vector3f, p: Vector3f) -> Planef {
    Planef {
        a: n.x,
        b: n.y,
        c: n.z,
        d: -dot3(n, p),
    }
}

/// `cPlanef::FromPoints`.
fn plane_from_points(p0: Vector3f, p1: Vector3f, p2: Vector3f) -> Planef {
    let n = cross3(sub3(p1, p0), sub3(p2, p0));
    let d = -dot3(n, p0);
    let mag = dot3(n, n).sqrt();
    Planef {
        a: n.x / mag,
        b: n.y / mag,
        c: n.z / mag,
        d: d / mag,
    }
}

fn add_plane(p: &Planef) -> Item {
    o(p4f(y::REFL_ADD_PLANE, 0, 0, 0, 0, f8([p.a, p.b, p.c, p.d])))
}

fn log_op(settings: bool, kind: i32, f: [f32; 8]) -> Item {
    o(p4f(y::REFL_LOG, i32::from(settings), kind, 0, 0, f))
}

fn fma3(a: Vector3f, s: f32, b: Vector3f) -> Vector3f {
    v3(
        a.x.mul_add(s, b.x),
        a.y.mul_add(s, b.y),
        a.z.mul_add(s, b.z),
    )
}

/// `RenderReflection` for the object at `idx`.
fn reflection(i: &TransIn, idx: i32) -> Vec<Item> {
    let i = *i;
    vec![ask(p4(y::ASK_REFL_INFO, idx, 0), move |f| {
        let a = f.floats();
        let g = |k: usize| a.get(k).copied().unwrap_or(0.0);
        let (max_dist, occlusion_test, one_sided) = (g(0), g(1) != 0.0, g(2) != 0.0);
        if max_dist > 0.0 {
            let cam = &i.camera;
            let point = fma3(mul3(cam.forward, -1.0), max_dist, cam.f.origin);
            let plane = plane_from_normal_point(cam.forward, point);
            vec![ask(
                p4f(
                    y::ASK_PLANE_BV,
                    idx,
                    0,
                    0,
                    0,
                    f8([plane.a, plane.b, plane.c, plane.d]),
                ),
                move |f| {
                    let in_range = f.int() != enums::COLLISION_OUTSIDE;
                    reflection_branch(i, idx, max_dist, occlusion_test, one_sided, in_range)
                },
            )]
        } else {
            reflection_branch(i, idx, max_dist, occlusion_test, one_sided, true)
        }
    })]
}

fn reflection_branch(
    i: TransIn,
    idx: i32,
    max_dist: f32,
    occlusion_test: bool,
    one_sided: bool,
    in_range: bool,
) -> Vec<Item> {
    if i.render_world_reflection && in_range && one_sided {
        sub_mesh_reflection(i, idx, max_dist, occlusion_test)
    } else {
        vec![ask(p4(y::ASK_REFL_CLEARED, 0, 0), move |f| {
            if f.flag() {
                return Vec::new();
            }
            let mut v = Vec::new();
            if i.settings_log {
                v.push(log_op(true, rlog::CLEAR_BEGIN, [0.0; 8]));
            }
            v.extend([
                o(p4(y::REFL_SET_FB, 0, 0)),
                o(Op::ClearFb {
                    flags: super::deferred::clear::COLOR,
                    use_pos: false,
                }),
                o(Op::SetAccumBuffer),
                o(p4(y::REFL_SET_CLEARED, 1, 0)),
            ]);
            if i.settings_log {
                v.push(log_op(true, rlog::CLEAR_END, [0.0; 8]));
            }
            v
        })]
    }
}

fn sub_mesh_reflection(i: TransIn, idx: i32, max_dist: f32, occlusion_test: bool) -> Vec<Item> {
    let mut v = vec![
        log_op(false, rlog::SETUP, [0.0; 8]),
        o(p4(y::REFL_BEGIN, i32::from(occlusion_test), 0)),
    ];
    v.push(ask(p4(y::ASK_REFL_TARGET, 0, 0), move |f| {
        let t = f.words();
        let n = |k: usize| word(&t, k) as i32;
        let (div, screen) = (n(9), Vector2l { x: n(7), y: n(8) });
        let cur_size = Vector2l { x: n(2), y: n(3) };
        let fb = if n(6) != 0 {
            Vector2l { x: n(4), y: n(5) }
        } else {
            screen
        };
        let pos = Vector2l {
            x: cdiv(n(0), div),
            y: cdiv(n(1), div),
        };
        let size = Vector2l {
            x: cdiv(if cur_size.x == -1 { fb.x } else { cur_size.x }, div),
            y: cdiv(if cur_size.y == -1 { fb.y } else { cur_size.y }, div),
        };
        let refl_fb = Vector2l { x: n(10), y: n(11) };
        vec![ask(p4(y::ASK_REFL_SURFACE, idx, 0), move |f| {
            let s = f.floats();
            reflection_setup(i, max_dist, (pos, size, refl_fb), &s)
        })]
    }));
    v
}

fn mat_of(s: &[f32]) -> Matrixf {
    let mut m = Matrixf::default();
    for (row, chunk) in m.m.iter_mut().zip(s.chunks(4)) {
        for (d, v) in row.iter_mut().zip(chunk) {
            *d = *v;
        }
    }
    m
}

fn reflection_setup(
    i: TransIn,
    max_dist: f32,
    rt: (Vector2l, Vector2l, Vector2l),
    s: &[f32],
) -> Vec<Item> {
    let (pos, size, refl_fb) = rt;
    let g = |k: usize| s.get(k).copied().unwrap_or(0.0);
    let world = mat_of(s);
    let on = v3(g(16), g(17), g(18));
    let op = v3(g(19), g(20), g(21));
    let (bv_min, bv_max) = (v3(g(22), g(23), g(24)), v3(g(25), g(26), g(27)));
    let cam = &i.camera;
    let mut surface_normal = cmath::matrix_mul_3x3(&world, on);
    normalize(&mut surface_normal);
    let surface_pos = cmath::matrix_mul_vec(&world, op);
    let plane = plane_from_normal_point(surface_normal, surface_pos);
    let mut mirror = Matrixf::default();
    // SAFETY: exported by crate::math; both pointers are live locals.
    unsafe { ohpl_math_matrix_plane_mirror(&plane, &mut mirror) };
    let refl_view = cmath::matrix_mul(&cam.f.view, &mirror);
    let refl_origin = cmath::matrix_mul_vec(&mirror, cam.f.origin);
    let mut cam_plane = Planef::default();
    // SAFETY: as above.
    unsafe { ohpl_math_transform_plane(&refl_view, &plane, &mut cam_plane) };
    let mut refl_proj = Matrixf::default();
    // SAFETY: as above.
    unsafe {
        ohpl_math_projection_matrix_oblique_near_clip_plane(&cam.f.proj, &cam_plane, &mut refl_proj)
    };
    let mut arr = Vec::with_capacity(48);
    for m in [&refl_proj, &refl_view, &cam.f.proj] {
        arr.extend(m.m.iter().flatten().copied());
    }
    let (far, near, fov, aspect) = (cam.far_plane, cam.f.near, cam.f.fov, cam.f.aspect);
    let frustum = Item::Arr(
        arr,
        Box::new(move |a| {
            p4f(
                y::REFL_FRUSTUM,
                a,
                0,
                0,
                0,
                f8([
                    far,
                    near,
                    fov,
                    aspect,
                    refl_origin.x,
                    refl_origin.y,
                    refl_origin.z,
                ]),
            )
        }),
    );
    let mut v = vec![
        o(p4f(y::REFL_TARGET, pos.x, pos.y, size.x, size.y, [0.0; 8])),
        frustum,
        o(p4(y::REFL_RESET_PLANES, 0, 0)),
    ];
    if max_dist > 0.0 {
        v.push(add_plane(&max_dist_plane(
            &i,
            max_dist,
            surface_normal,
            &plane,
            refl_origin,
        )));
    }
    if i.clip_reflection_screen_rect {
        v.extend(screen_rect_planes(
            &i,
            &refl_view,
            &refl_proj,
            refl_origin,
            (bv_min, bv_max),
            size,
            refl_fb,
        ));
    }
    v.extend([
        o(p4(y::REFL_RENDER, 0, 0)),
        o(p4(y::REFL_END, 0, 0)),
        o(Op::SetAccumBuffer),
        o(state(State::DepthTest, 1)),
        o(state(State::DepthWrite, 0)),
        o(alpha_limit(0.01)),
        o(state(State::Alpha, st::ALPHA_TRANS)),
    ]);
    v
}

/// `cVector3f::Normalize`.
fn normalize(v: &mut Vector3f) {
    let length = dot3(*v, *v).sqrt();
    if f64::from(length) > 1e-08 {
        let inv = 1.0f32 / length;
        v.x *= inv;
        v.y *= inv;
        v.z *= inv;
    }
}

fn max_dist_plane(
    i: &TransIn,
    max_dist: f32,
    surface_normal: Vector3f,
    refl_plane: &Planef,
    refl_origin: Vector3f,
) -> Planef {
    let cam = &i.camera;
    let forward = mul3(cam.forward, -1.0);
    let f_dot_n = dot3(forward, surface_normal);
    if f_dot_n < -0.99999 {
        let clip_normal = mul3(forward, -1.0);
        let clip_point = fma3(forward, max_dist, cam.f.origin);
        return plane_from_normal_point(clip_normal, clip_point);
    }
    let mut cs = Planef::default();
    // SAFETY: exported by crate::math; both pointers are live locals.
    unsafe { ohpl_math_transform_plane(&cam.f.view, refl_plane, &mut cs) };
    let z = -max_dist;
    let mut p1 = v3(0.0, 0.0, z);
    let mut p2 = v3(0.0, 0.0, z);
    let at = |den: f32| (-cs.c).mul_add(-max_dist, -cs.d) / den;
    if cs.b.abs() < 0.0001 {
        p1.x = at(cs.a);
        p2 = p1;
        p2.y += 1.0;
    } else if cs.a.abs() < 0.0001 {
        p1.y = at(cs.b);
        p2 = p1;
        p2.x += 1.0;
    } else {
        p1.x = at(cs.a);
        p2.y = at(cs.b);
    }
    let inv = cmath::matrix_inverse(&cam.f.view);
    let p1 = cmath::matrix_mul_vec(&inv, p1);
    let p2 = cmath::matrix_mul_vec(&inv, p2);
    let mut normal = cross3(sub3(p1, refl_origin), sub3(p2, refl_origin));
    normalize(&mut normal);
    if dot3(surface_normal, normal) < 0.0 {
        normal = mul3(normal, -1.0);
    }
    plane_from_normal_point(normal, p1)
}

fn screen_rect_planes(
    i: &TransIn,
    view: &Matrixf,
    proj: &Matrixf,
    origin: Vector3f,
    bv: (Vector3f, Vector3f),
    size: Vector2l,
    refl_fb: Vector2l,
) -> Vec<Item> {
    let (bv_min, bv_max) = bv;
    let cam = &i.camera;
    let row = |r: usize| {
        let m = view.m.get(r).copied().unwrap_or([0.0; 4]);
        v3(m[0], m[1], m[2])
    };
    let (up, right, forward) = (row(1), row(0), row(2));
    let near = cam.f.near;
    let half_fov_tan = tan(cam.f.fov * 0.5);
    let near_top = half_fov_tan * near;
    let near_right = cam.f.aspect * near_top;
    let (mut min, mut max) = (Vector3f::default(), Vector3f::default());
    // SAFETY: exported by crate::math; every pointer is a live local.
    let visible = unsafe {
        ohpl_math_get_normalized_clip_rect_from_bv(
            &bv_min,
            &bv_max,
            view,
            proj,
            true,
            cam.f.fov,
            near,
            cam.f.aspect,
            half_fov_tan,
            &mut min,
            &mut max,
        )
    };
    let mut v: Vec<Item> = Vec::new();
    if !visible {
        return v;
    }
    if i.settings_log {
        v.push(log_op(
            false,
            rlog::CLIP_LIMITS,
            f8([min.x, min.y, min.z, max.x, max.y, max.z]),
        ));
    }
    // gcc fuses forward * -near into the right plane only; the others share the product
    let back = mul3(forward, -near);
    let near_pos = |dir: Vector3f, k: f32, fused: bool| {
        let p = fma3(dir, k, origin);
        if fused {
            fma3(forward, -near, p)
        } else {
            v3(p.x + back.x, p.y + back.y, p.z + back.z)
        }
    };
    let plus = |a: Vector3f, b: Vector3f| v3(a.x + b.x, a.y + b.y, a.z + b.z);
    let mut needs = false;
    if max.x < 1.0 {
        let n = near_pos(right, max.x * near_right, true);
        v.push(add_plane(&plane_from_points(origin, n, plus(n, up))));
        needs = true;
    }
    if min.x > -1.0 {
        let n = near_pos(right, min.x * near_right, false);
        v.push(add_plane(&plane_from_points(origin, plus(n, up), n)));
        needs = true;
    }
    if max.y < 1.0 {
        let n = near_pos(up, max.y * near_top, false);
        v.push(add_plane(&plane_from_points(origin, plus(n, right), n)));
        needs = true;
    }
    if min.y > -1.0 {
        let n = near_pos(up, min.y * near_top, false);
        v.push(add_plane(&plane_from_points(origin, n, plus(n, right))));
        needs = true;
    }
    if needs {
        let rt = Vector2l {
            x: if size.x < 0 { refl_fb.x } else { size.x },
            y: if size.y < 0 { refl_fb.y } else { size.y },
        };
        let r = clip_rect_from_bv(bv_min, bv_max, &cam.f, rt, half_fov_tan);
        if i.settings_log {
            v.push(log_op(
                false,
                rlog::SCISSOR,
                f8([r[0] as f32, r[1] as f32, r[2] as f32, r[3] as f32]),
            ));
        }
        v.push(o(p4f(y::REFL_SCISSOR, r[0], r[1], r[2], r[3], [0.0; 8])));
    }
    v
}
