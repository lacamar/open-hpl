//! Single-instruction float ops in gcc's operand order: which NaN an operation returns when both
//! inputs are NaN depends on it, and LLVM is free to commute `fadd` and `fmul`.

#[cfg(target_arch = "aarch64")]
mod imp {
    use std::arch::asm;

    macro_rules! binop {
        ($name:ident, $ins:literal) => {
            #[inline(always)]
            pub fn $name(n: f32, m: f32) -> f32 {
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
    binop!(add, "fadd");
    binop!(mul, "fmul");
    binop!(sub, "fsub");

    /// `fmsub`: `a - n * m`.
    #[inline(always)]
    pub fn msub(n: f32, m: f32, a: f32) -> f32 {
        let r: f32;
        // SAFETY: a register-only float instruction without memory access.
        unsafe {
            asm!("fmsub {r:s}, {n:s}, {m:s}, {a:s}", r = lateout(vreg) r, n = in(vreg) n, m = in(vreg) m,
                a = in(vreg) a, options(pure, nomem, nostack));
        }
        r
    }
}

#[cfg(not(target_arch = "aarch64"))]
mod imp {
    pub fn add(n: f32, m: f32) -> f32 {
        n + m
    }
    pub fn mul(n: f32, m: f32) -> f32 {
        n * m
    }
    pub fn sub(n: f32, m: f32) -> f32 {
        n - m
    }
    pub fn msub(n: f32, m: f32, a: f32) -> f32 {
        (-n).mul_add(m, a)
    }
}

/// `n + m` with `n` the first source operand.
pub fn add(n: f32, m: f32) -> f32 {
    imp::add(n, m)
}

/// `n * m` with `n` the first source operand.
pub fn mul(n: f32, m: f32) -> f32 {
    imp::mul(n, m)
}

/// `n - m` with `n` the first source operand.
pub fn sub(n: f32, m: f32) -> f32 {
    imp::sub(n, m)
}

/// `a - n * m` fused, negating `n` first so a NaN `n` comes back with its sign flipped.
pub fn msub(n: f32, m: f32, a: f32) -> f32 {
    imp::msub(n, m, a)
}

unsafe extern "C" {
    fn sincosf(x: f32, s: *mut f32, c: *mut f32);
}

/// libm `sincosf`, which gcc emits when one angle feeds both `sinf` and `cosf`; its NaN results
/// can differ from the separate calls.
pub fn sincos(x: f32) -> (f32, f32) {
    let (mut s, mut c) = (0.0f32, 0.0f32);
    // SAFETY: pure libm function writing two floats we own.
    unsafe { sincosf(x, &mut s, &mut c) };
    (s, c)
}
