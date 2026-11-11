//! Script math natives (`SomaScriptMath.cpp`). Every `fma` marks a site gcc contracts on aarch64.

use crate::math::{dot3, fast_random_float, fma, rand_rectf, v3};
use crate::types::Vector3f;

unsafe extern "C" {
    fn sinf(x: f32) -> f32;
    fn cosf(x: f32) -> f32;
    fn roundf(x: f32) -> f32;
    fn floorf(x: f32) -> f32;
}

/// Single-instruction float ops with the operand order gcc emitted: which NaN an operation returns
/// when both inputs are NaN depends on it, and LLVM is free to commute `fmul`/`fadd`/`fma`.
#[cfg(target_arch = "aarch64")]
mod hw {
    use std::arch::asm;

    macro_rules! binop {
        ($name:ident, $ins:literal) => {
            #[inline(always)]
            pub(super) fn $name(n: f32, m: f32) -> f32 {
                let r: f32;
                // SAFETY: a register-only float instruction without memory access.
                unsafe {
                    asm!(concat!($ins, " {r:s}, {n:s}, {m:s}"), r = lateout(vreg) r, n = in(vreg) n, m = in(vreg) m,
                        options(pure, nomem, nostack));
                }
                r
            }
        };
    }
    macro_rules! ternop {
        ($name:ident, $ins:literal) => {
            #[inline(always)]
            pub(super) fn $name(n: f32, m: f32, a: f32) -> f32 {
                let r: f32;
                // SAFETY: a register-only float instruction without memory access.
                unsafe {
                    asm!(concat!($ins, " {r:s}, {n:s}, {m:s}, {a:s}"), r = lateout(vreg) r, n = in(vreg) n, m = in(vreg) m,
                        a = in(vreg) a, options(pure, nomem, nostack));
                }
                r
            }
        };
    }
    binop!(mul, "fmul");
    binop!(add, "fadd");
    binop!(sub, "fsub");
    ternop!(madd, "fmadd");
    ternop!(msub, "fmsub");
}

#[cfg(not(target_arch = "aarch64"))]
mod hw {
    pub(super) fn mul(n: f32, m: f32) -> f32 {
        n * m
    }
    pub(super) fn add(n: f32, m: f32) -> f32 {
        n + m
    }
    pub(super) fn sub(n: f32, m: f32) -> f32 {
        n - m
    }
    pub(super) fn madd(n: f32, m: f32, a: f32) -> f32 {
        n.mul_add(m, a)
    }
    pub(super) fn msub(n: f32, m: f32, a: f32) -> f32 {
        (-n).mul_add(m, a)
    }
}

const PI_F: f32 = std::f32::consts::PI;
const TWO_PI_F: f32 = std::f32::consts::TAU;

pub(crate) fn sin(x: f32) -> f32 {
    // SAFETY: pure libm function.
    unsafe { sinf(x) }
}

pub(crate) fn cos(x: f32) -> f32 {
    // SAFETY: pure libm function.
    unsafe { cosf(x) }
}

pub(crate) fn round(x: f32) -> f32 {
    // SAFETY: pure libm function.
    unsafe { roundf(x) }
}

pub(crate) fn floor(x: f32) -> f32 {
    // SAFETY: pure libm function.
    unsafe { floorf(x) }
}

pub(crate) fn sigmoid_curve(x: f32) -> f32 {
    hw::mul(hw::mul(x, x), hw::msub(x, 2.0, 3.0))
}

pub(crate) fn interpolate_sigmoid(a: f32, b: f32, t: f32) -> f32 {
    let x = hw::mul(hw::mul(hw::sub(b, a), t), t);
    hw::madd(x, hw::msub(t, 2.0, 3.0), a)
}

pub(crate) fn interpolate_linear(a: f32, b: f32, t: f32) -> f32 {
    hw::madd(t, hw::sub(b, a), a)
}

pub(crate) fn interpolate_cosine(a: f32, b: f32, t: f32) -> f32 {
    let f = hw::mul(hw::sub(1.0, cos(hw::mul(t, PI_F))), 0.5);
    hw::madd(a, hw::sub(1.0, f), hw::mul(b, f))
}

pub(crate) fn interpolate_cubic(x0: f32, x1: f32, x2: f32, x3: f32, t: f32) -> f32 {
    let a0 = hw::add(hw::sub(hw::sub(x3, x2), x0), x1);
    let a1 = hw::sub(hw::sub(x0, x1), a0);
    let a2 = hw::sub(x2, x0);
    hw::madd(t, hw::madd(t, hw::madd(t, a0, a1), a2), x1)
}

/// `1 << n`: the shift count wraps like the aarch64 `lsl` the C++ compiles to.
pub(crate) fn get_bit(n: i32) -> i32 {
    1i32.wrapping_shl(n as u32)
}

pub(crate) fn set_bit_flag(flags: i32, bit: i32, set: bool) -> i32 {
    if set { flags | bit } else { flags & !bit }
}

pub(crate) fn get_bit_flag(flags: i32, bit: i32) -> bool {
    flags & bit != 0
}

pub(crate) fn vector2_dot(a: [f32; 2], b: [f32; 2]) -> f32 {
    fma(a[0], b[0], a[1] * b[1])
}

pub(crate) fn vector2_rotate(v: [f32; 2], a: f32) -> [f32; 2] {
    let (c, s) = (cos(a), sin(a));
    [fma(c, v[0], -(s * v[1])), fma(c, v[1], s * v[0])]
}

/// `std::max(a, std::max(b, c))`.
pub(crate) fn vector3_max_element(v: [f32; 3]) -> f32 {
    let max = |a: f32, b: f32| if a < b { b } else { a };
    max(v[0], max(v[1], v[2]))
}

fn catmull_rom_one(p0: f32, p1: f32, p2: f32, p3: f32, t: f32) -> f32 {
    let t2 = t * t;
    let t3 = t2 * t;
    let a = fma(p2 - p0, t, p1 * 2.0);
    let b = fma(fma(-p1, 5.0, p0 * 2.0) + p2 * 4.0 - p3, t2, a);
    let c = (fma(-p0, 1.0, p1 * 3.0) - p2 * 3.0) + p3;
    fma(c, t3, b) * 0.5
}

pub(crate) fn catmull_rom<const N: usize>(
    p0: [f32; N],
    p1: [f32; N],
    p2: [f32; N],
    p3: [f32; N],
    t: f32,
) -> [f32; N] {
    std::array::from_fn(|i| {
        let g = |p: &[f32; N]| p.get(i).copied().unwrap_or(0.0);
        catmull_rom_one(g(&p0), g(&p1), g(&p2), g(&p3), t)
    })
}

pub(crate) fn sphere_surface_point(seed: [f32; 2], radius: f32) -> [f32; 3] {
    let z = fma(seed[0], 2.0, -1.0);
    let a = seed[1] * TWO_PI_F;
    let w = fma(-z, z, 1.0).sqrt();
    [z * radius, sin(a) * w * radius, cos(a) * w * radius]
}

pub(crate) fn random_sphere_surface_point(radius: f32) -> [f32; 3] {
    let z = fma(rand_rectf(0.0, 1.0), 2.0, -1.0);
    let a = rand_rectf(0.0, 1.0) * TWO_PI_F;
    let w = fma(-z, z, 1.0).sqrt();
    [z * radius, sin(a) * w * radius, cos(a) * w * radius]
}

pub(crate) fn random_circle_edge_point(radius: f32) -> [f32; 2] {
    let a = rand_rectf(0.0, TWO_PI_F);
    [cos(a) * radius, sin(a) * radius]
}

pub(crate) fn project_on_plane(n: [f32; 3], v: [f32; 3]) -> [f32; 3] {
    let vv = v3(v[0], v[1], v[2]);
    let nn = v3(n[0], n[1], n[2]);
    let d = dot3(vv, nn);
    [
        fma(-n[0], d, v[0]),
        fma(-n[1], d, v[1]),
        fma(-n[2], d, v[2]),
    ]
}

/// The engine's `GetNoisePerlin1D` wrapper calls the cosine noise.
pub(crate) fn noise_perlin_1d(x: f32) -> f32 {
    let fl = floor(x);
    let f = (1.0 - cos((x - fl) * PI_F)) * 0.5;
    let i = fl as i32;
    fma(
        fast_random_float(i),
        1.0 - f,
        fast_random_float(i.wrapping_add(1)) * f,
    )
}

/// `RGBToHex`: `%02X%02X%02X` of the channels scaled to bytes.
pub(crate) fn rgb_to_hex(c: [f32; 3]) -> [u8; 6] {
    const HEX: &[u8; 16] = b"0123456789ABCDEF";
    let byte = |v: f32| fma(v, 255.0, 0.5) as i32 as u8;
    let mut out = [0u8; 6];
    for (k, &v) in c.iter().enumerate() {
        let b = byte(v);
        if let Some(d) = out.get_mut(k * 2) {
            *d = HEX.get(usize::from(b >> 4)).copied().unwrap_or(b'0');
        }
        if let Some(d) = out.get_mut(k * 2 + 1) {
            *d = HEX.get(usize::from(b & 15)).copied().unwrap_or(b'0');
        }
    }
    out
}

pub(crate) fn aabb_sphere_collision(mn: [f32; 3], mx: [f32; 3], c: [f32; 3], r: f32) -> bool {
    let pick = |i: usize, k: usize, lo: f32, hi: f32| if i & k != 0 { hi } else { lo };
    for i in 0..8usize {
        let p = v3(
            pick(i, 1, mn[0], mx[0]),
            pick(i, 2, mn[1], mx[1]),
            pick(i, 4, mn[2], mx[2]),
        );
        if dist_sqr(p, v3(c[0], c[1], c[2])) <= r * r {
            return true;
        }
    }
    !(mx[0] - c[0] < -r
        || c[0] - mn[0] < -r
        || mx[1] - c[1] < -r
        || c[1] - mn[1] < -r
        || mx[2] - c[2] < -r
        || c[2] - mn[2] < -r)
}

fn dist_sqr(a: Vector3f, b: Vector3f) -> f32 {
    let d = v3(a.x - b.x, a.y - b.y, a.z - b.z);
    dot3(d, d)
}

pub(crate) const MAX_ERRORS: usize = 64;

/// Lanes of the vec3 controller whose products gcc emitted with the scalar operand first.
fn swapped(n: usize, lane: usize) -> bool {
    n == 3 && lane > 0
}

/// `cSomaPid<T>` as it sits in the script struct block (offset 16 of the object).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Pid<const N: usize> {
    pub p: f32,
    pub i: f32,
    pub d: f32,
    pub error_num: i32,
    pub cur: i32,
    pub last_plus_one: i32,
    pub last_error: [f32; N],
    pub last_derivative: [f32; N],
    pub last_integral: [f32; N],
    pub errors: [[f32; N]; MAX_ERRORS],
    pub time_steps: [f32; MAX_ERRORS],
}

impl<const N: usize> Pid<N> {
    pub(crate) fn set_error_num(&mut self, num: i32) {
        self.error_num = num.clamp(1, MAX_ERRORS as i32);
        self.reset();
    }

    pub(crate) fn reset(&mut self) {
        self.cur = 0;
        self.last_plus_one = 0;
        self.errors = [[0.0; N]; MAX_ERRORS];
        self.time_steps = [0.0; MAX_ERRORS];
    }

    /// An index outside the history (a block never initialised by the factory) reads as zero.
    pub(crate) fn output(&mut self, error: [f32; N], dt: f32) -> [f32; N] {
        if self.error_num <= 0 {
            self.set_error_num(10);
        }
        let cur = usize::try_from(self.cur).unwrap_or(MAX_ERRORS);
        if let Some(e) = self.errors.get_mut(cur) {
            *e = error;
        }
        if let Some(t) = self.time_steps.get_mut(cur) {
            *t = dt;
        }
        let count = usize::try_from(self.error_num).unwrap_or(0);
        let mut integral = [0.0f32; N];
        for (e, &t) in self.errors.iter().zip(&self.time_steps).take(count) {
            for (k, (acc, &x)) in integral.iter_mut().zip(e).enumerate() {
                // gcc's operand order: the vec3 y and z lanes put the time step first
                *acc = if swapped(N, k) {
                    hw::madd(t, x, *acc)
                } else {
                    hw::madd(x, t, *acc)
                };
            }
        }
        let mut derivative = [0.0f32; N];
        if self.last_plus_one > 0 && dt > 0.0 {
            let last = usize::try_from(self.last_plus_one - 1).unwrap_or(MAX_ERRORS);
            let a = self.errors.get(cur).copied().unwrap_or([0.0; N]);
            let b = self.errors.get(last).copied().unwrap_or([0.0; N]);
            for k in 0..N {
                let (x, y) = (
                    a.get(k).copied().unwrap_or(0.0),
                    b.get(k).copied().unwrap_or(0.0),
                );
                if let Some(d) = derivative.get_mut(k) {
                    *d = (x - y) / dt;
                }
            }
        }
        self.last_plus_one = self.cur.wrapping_add(1);
        self.cur = self
            .cur
            .wrapping_add(1)
            .checked_rem(self.error_num)
            .unwrap_or_default();
        self.last_error = error;
        self.last_derivative = derivative;
        self.last_integral = integral;
        std::array::from_fn(|k| {
            let g = |a: &[f32; N]| a.get(k).copied().unwrap_or(0.0);
            let (i, e) = (g(&integral), g(&error));
            let scaled = hw::mul(i, self.i);
            let sum = if swapped(N, k) {
                hw::madd(self.p, e, scaled)
            } else {
                hw::madd(e, self.p, scaled)
            };
            hw::madd(g(&derivative), self.d, sum)
        })
    }
}
