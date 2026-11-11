//! Fused ops in gcc's operand order. `fmsub` negates its first multiplicand before the NaN rules
//! apply, so a NaN there comes back with its sign flipped; LLVM would move the negation onto the
//! other multiplicand and keep the sign.

#[cfg(target_arch = "aarch64")]
#[inline(always)]
pub fn msub(n: f32, m: f32, a: f32) -> f32 {
    use std::arch::asm;
    let r: f32;
    // SAFETY: a register-only float instruction without memory access.
    unsafe {
        asm!("fmsub {r:s}, {n:s}, {m:s}, {a:s}", r = lateout(vreg) r, n = in(vreg) n, m = in(vreg) m,
            a = in(vreg) a, options(pure, nomem, nostack));
    }
    r
}

/// `a - n * m` fused.
#[cfg(not(target_arch = "aarch64"))]
pub fn msub(n: f32, m: f32, a: f32) -> f32 {
    (-n).mul_add(m, a)
}

/// `n + m` with `n` the first source operand.
#[cfg(target_arch = "aarch64")]
#[inline(always)]
pub fn add(n: f32, m: f32) -> f32 {
    use std::arch::asm;
    let r: f32;
    // SAFETY: a register-only float instruction without memory access.
    unsafe {
        asm!("fadd {r:s}, {n:s}, {m:s}", r = lateout(vreg) r, n = in(vreg) n, m = in(vreg) m,
            options(pure, nomem, nostack));
    }
    r
}

#[cfg(not(target_arch = "aarch64"))]
pub fn add(n: f32, m: f32) -> f32 {
    n + m
}

/// `n * m` with `n` the first source operand.
#[cfg(target_arch = "aarch64")]
#[inline(always)]
pub fn mul(n: f32, m: f32) -> f32 {
    use std::arch::asm;
    let r: f32;
    // SAFETY: a register-only float instruction without memory access.
    unsafe {
        asm!("fmul {r:s}, {n:s}, {m:s}", r = lateout(vreg) r, n = in(vreg) n, m = in(vreg) m,
            options(pure, nomem, nostack));
    }
    r
}

#[cfg(not(target_arch = "aarch64"))]
pub fn mul(n: f32, m: f32) -> f32 {
    n * m
}
