//! Vector helpers in gcc's contraction order and `cPidController<cVector3f>`.

use super::hw;
use crate::math::{fma, v3};
use crate::types::{Matrixf, Vector3f};

pub const PID_N: usize = 20;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Pid3 {
    pub p: f32,
    pub i: f32,
    pub d: f32,
    pub n: i32,
    pub error_num: i32,
    pub last_num: i32,
    pub errors: [Vector3f; PID_N],
    pub steps: [f32; PID_N],
    pub integral: Vector3f,
    pub derivative: Vector3f,
}

impl Pid3 {
    pub fn init(&mut self, n: i32, p: f32, i: f32, d: f32) {
        *self = Pid3::default();
        self.n = n.clamp(0, PID_N as i32);
        self.p = p;
        self.i = i;
        self.d = d;
        self.last_num = -1;
    }

    /// Errors stay; only the time steps are cleared, like the C++ `Reset`.
    pub fn reset(&mut self) {
        self.error_num = 0;
        self.last_num = -1;
        self.steps = [0.0; PID_N];
        self.integral = Vector3f::default();
        self.derivative = Vector3f::default();
    }

    /// C++ indexes `mvErrors` unchecked; with no buffer the output is 0 and the index wraps.
    pub fn output(&mut self, e: Vector3f, dt: f32) -> Vector3f {
        let n = usize::try_from(self.n).unwrap_or(0).min(PID_N);
        if n == 0 {
            return Vector3f::default();
        }
        let mut idx = usize::try_from(self.error_num).unwrap_or(0);
        if idx >= n {
            idx = 0;
        }
        if let Some(s) = self.errors.get_mut(idx) {
            *s = e;
        }
        if let Some(s) = self.steps.get_mut(idx) {
            *s = dt;
        }
        let mut integral = Vector3f::default();
        for (er, st) in self.errors.iter().zip(&self.steps).take(n) {
            integral = v3(
                fmad(er.x, *st, integral.x),
                fmad(*st, er.y, integral.y),
                fmad(*st, er.z, integral.z),
            );
        }
        self.integral = integral;
        self.derivative = Vector3f::default();
        if let Ok(last) = usize::try_from(self.last_num) {
            let prev = self.errors.get(last).copied().unwrap_or_default();
            self.derivative = v3(
                (e.x - prev.x) / dt,
                (e.y - prev.y) / dt,
                (e.z - prev.z) / dt,
            );
        }
        self.last_num = i32::try_from(idx).unwrap_or(0);
        let mut next = idx + 1;
        if next >= n {
            next = 0;
        }
        self.error_num = i32::try_from(next).unwrap_or(0);
        let (p, i, d) = (self.p, self.i, self.d);
        let (it, dv) = (self.integral, self.derivative);
        v3(
            fmad(d, dv.x, fmad(e.x, p, hw::mul(i, it.x))),
            fmad(d, dv.y, fmad(p, e.y, hw::mul(i, it.y))),
            fmad(d, dv.z, fmad(p, e.z, hw::mul(i, it.z))),
        )
    }
}

/// `a + n * m` fused with `n` the first source operand; when both multiplicands are NaN the
/// first one wins, so the operand order is part of the result.
#[cfg(target_arch = "aarch64")]
#[inline(always)]
pub fn fmad(n: f32, m: f32, a: f32) -> f32 {
    use std::arch::asm;
    let r: f32;
    // SAFETY: a register-only float instruction without memory access.
    unsafe {
        asm!("fmadd {r:s}, {n:s}, {m:s}, {a:s}", r = lateout(vreg) r, n = in(vreg) n, m = in(vreg) m,
            a = in(vreg) a, options(pure, nomem, nostack));
    }
    r
}

#[cfg(not(target_arch = "aarch64"))]
pub fn fmad(n: f32, m: f32, a: f32) -> f32 {
    n.mul_add(m, a)
}

unsafe extern "C" {
    fn ohpl_math_vector3_cross(a: *const Vector3f, b: *const Vector3f, out: *mut Vector3f);
    fn ohpl_math_matrix_mul_vec(a: *const Matrixf, b: *const Vector3f, out: *mut Vector3f);
    fn ohpl_math_vector3_dot(a: *const Vector3f, b: *const Vector3f) -> f32;
    fn ohpl_math_vector3_max_length(v: *const Vector3f, m: f32, out: *mut Vector3f);
}

/// `cMath::Vector3Cross` through its exported entry point.
pub fn cross(a: Vector3f, b: Vector3f) -> Vector3f {
    let mut out = Vector3f::default();
    // SAFETY: all pointers refer to live locals; the callee only reads `a`, `b` and writes `out`.
    unsafe { ohpl_math_vector3_cross(&a, &b, &mut out) };
    out
}

/// `cMath::MatrixMul(matrix, vector)` through its exported entry point.
pub fn mat_vec(m: &Matrixf, v: Vector3f) -> Vector3f {
    let mut out = Vector3f::default();
    // SAFETY: all pointers refer to live values; the callee only reads `m`, `v` and writes `out`.
    unsafe { ohpl_math_matrix_mul_vec(m, &v, &mut out) };
    out
}

/// `cMath::Vector3Dot` through its exported entry point.
pub fn dot(a: Vector3f, b: Vector3f) -> f32 {
    // SAFETY: both pointers refer to live locals and are only read.
    unsafe { ohpl_math_vector3_dot(&a, &b) }
}

/// `cMath::Vector3MaxLength` through the exported entry point the C++ calls, so the operand
/// order of its multiplications is the one the original binary has.
pub fn max_length(v: Vector3f, max: f32) -> Vector3f {
    let mut out = Vector3f::default();
    // SAFETY: both pointers refer to live locals; the callee only reads `v` and writes `out`.
    unsafe { ohpl_math_vector3_max_length(&v, max, &mut out) };
    out
}

/// `cMath::Vector3Dist`.
pub fn dist3(a: Vector3f, b: Vector3f) -> f32 {
    let (dx, dy, dz) = (b.x - a.x, b.y - a.y, b.z - a.z);
    fma(dz, dz, fma(dx, dx, dy * dy)).sqrt()
}

/// `c + a * s` per lane, fused.
pub fn madd(a: Vector3f, s: f32, c: Vector3f) -> Vector3f {
    v3(fmad(a.x, s, c.x), fmad(a.y, s, c.y), fmad(a.z, s, c.z))
}

/// `v * s` per lane with `v` the first source operand.
pub fn hmul3(v: Vector3f, s: f32) -> Vector3f {
    v3(hw::mul(v.x, s), hw::mul(v.y, s), hw::mul(v.z, s))
}

/// `cVector3f::Normalize` with the multiplications in gcc operand order.
pub fn normalize(v: &mut Vector3f) -> f32 {
    let length = fma(v.z, v.z, fma(v.x, v.x, v.y * v.y)).sqrt();
    if f64::from(length) > 1e-08 {
        let inv = 1.0f32 / length;
        *v = v3(hw::mul(v.x, inv), hw::mul(v.y, inv), hw::mul(v.z, inv));
    }
    length
}

/// `a - v * d` per lane, fused with `d` as the negated first multiplicand (`fmls`).
pub fn msub3(d: f32, v: Vector3f, a: Vector3f) -> Vector3f {
    v3(
        super::hw::msub(d, v.x, a.x),
        super::hw::msub(d, v.y, a.y),
        super::hw::msub(d, v.z, a.z),
    )
}

/// `-(n * m)` with `n` the first source operand (`fnmul`); the negation flips a NaN's sign.
#[cfg(target_arch = "aarch64")]
#[inline(always)]
pub fn nmul(n: f32, m: f32) -> f32 {
    use std::arch::asm;
    let r: f32;
    // SAFETY: a register-only float instruction without memory access.
    unsafe {
        asm!("fnmul {r:s}, {n:s}, {m:s}", r = lateout(vreg) r, n = in(vreg) n, m = in(vreg) m,
            options(pure, nomem, nostack));
    }
    r
}

#[cfg(not(target_arch = "aarch64"))]
pub fn nmul(n: f32, m: f32) -> f32 {
    -(n * m)
}

pub fn is_zero(a: Vector3f) -> bool {
    a.x == 0.0 && a.y == 0.0 && a.z == 0.0
}
