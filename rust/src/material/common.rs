//! Helpers shared by the material types.

use super::types::{ForceFieldInfo, RenderView, Uniforms};
use crate::math::{IDENTITY, matrix_mul, v3};
use crate::types::{Color, Matrixf, Vector3f};

unsafe extern "C" {
    fn fmodf(x: f32, y: f32) -> f32;
    fn powf(x: f32, y: f32) -> f32;
}

pub(crate) fn fmod(x: f32, y: f32) -> f32 {
    // SAFETY: pure libm function on plain floats.
    unsafe { fmodf(x, y) }
}

pub(crate) fn pow(x: f32, y: f32) -> f32 {
    // SAFETY: pure libm function on plain floats.
    unsafe { powf(x, y) }
}

/// `cMath::Max(float, float)`
pub(crate) fn fmax(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

/// `cMath::Min(float, float)`
pub(crate) fn fmin(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

pub(crate) const fn color(r: f32, g: f32, b: f32, a: f32) -> Color {
    Color { r, g, b, a }
}

pub(crate) fn color_scale(c: Color, s: f32) -> Color {
    color(c.r * s, c.g * s, c.b * s, c.a * s)
}

pub(crate) fn color_mul(c: Color, o: Color) -> Color {
    color(c.r * o.r, c.g * o.g, c.b * o.b, c.a * o.a)
}

pub(crate) fn matrix_translate(t: Vector3f) -> Matrixf {
    Matrixf {
        m: [
            [1.0, 0.0, 0.0, t.x],
            [0.0, 1.0, 0.0, t.y],
            [0.0, 0.0, 1.0, t.z],
            [0.0, 0.0, 0.0, 1.0],
        ],
    }
}

/// `m.GetTranspose().GetRotation()`
pub(crate) fn inv_view_rotation(m: &Matrixf) -> Matrixf {
    let e = |r: usize, c: usize| {
        m.m.get(c)
            .and_then(|row| row.get(r))
            .copied()
            .unwrap_or(0.0)
    };
    Matrixf {
        m: [
            [e(0, 0), e(0, 1), e(0, 2), 0.0],
            [e(1, 0), e(1, 1), e(1, 2), 0.0],
            [e(2, 0), e(2, 1), e(2, 2), 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    }
}

/// `cRendererDeferred::GetFogRenderColor`
pub(crate) fn fog_render_color(c: Color, brightness: f32, hdr: bool) -> Color {
    if hdr {
        color(
            c.r * c.r * brightness,
            c.g * c.g * brightness,
            c.b * c.b * brightness,
            c.a,
        )
    } else {
        c
    }
}

pub(crate) fn model_or_identity(has: bool, m: &Matrixf) -> Matrixf {
    if has { *m } else { IDENTITY }
}

pub(crate) fn zero3() -> Vector3f {
    v3(0.0, 0.0, 0.0)
}

/// `iMaterialType::SetForceFieldVars`
pub(crate) fn force_field_vars(
    out: &mut Uniforms,
    first: i32,
    fields: &[ForceFieldInfo],
    force_mul: f32,
    max_force: f32,
) {
    let mut start = [0.0f32; 4];
    let mut length = [1.0f32; 4];
    let mut force = [0.0f32; 4];
    let mut t = [0.0f32; 4];
    for (i, id) in (first..first + 4).enumerate() {
        let mut pos = zero3();
        if let (Some(f), Some(s), Some(l), Some(fo), Some(tt)) = (
            fields.get(i),
            start.get_mut(i),
            length.get_mut(i),
            force.get_mut(i),
            t.get_mut(i),
        ) {
            pos = f.pos;
            *s = f.falloff_start;
            *l = f.radius - f.falloff_start;
            *fo = f.force * force_mul;
            *tt = f.t;
        }
        out.vec3(id, pos);
    }
    let [s0, s1, s2, s3] = start;
    let [l0, l1, l2, l3] = length;
    let [f0, f1, f2, f3] = force;
    let [t0, t1, t2, t3] = t;
    out.vec4(first + 4, s0, s1, s2, s3);
    out.vec4(first + 5, l0, l1, l2, l3);
    out.vec4(first + 6, f0, f1, f2, f3);
    out.vec4(first + 7, t0, t1, t2, t3);
    out.float(first + 8, max_force);
}

/// `GetCurrentFrustum()->GetViewMatrix() * model` as `cMath::MatrixMul`.
pub(crate) fn view_mul(r: &RenderView, model: &Matrixf) -> Matrixf {
    matrix_mul(&r.view, model)
}

/// Variable names `iMaterialType::AddForceFieldVariableIds` registers, in id order.
pub(crate) const FORCE_FIELD_NAMES: [&str; 9] = [
    "avForceFieldPos0",
    "avForceFieldPos1",
    "avForceFieldPos2",
    "avForceFieldPos3",
    "avForceFieldFadeStarts",
    "avForceFieldFadeLengths",
    "avForceFieldForces",
    "avForceFieldTs",
    "afForceFieldMaxForce",
];
