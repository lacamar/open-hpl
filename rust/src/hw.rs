#[cfg(target_arch = "aarch64")]
mod imp {
    use std::arch::asm;

    macro_rules! binop {
        ($name:ident, $ins:literal) => {
            #[inline(always)]
            pub fn $name(n: f32, m: f32) -> f32 {
                let r: f32;
                unsafe {
                    asm!(concat!($ins, " {r:s}, {n:s}, {m:s}"), r = lateout(vreg) r, n = in(vreg) n, m = in(vreg) m,
                        options(pure, nomem, nostack));
                }
                r
            }
        };
    }
    macro_rules! fused {
        ($name:ident, $ins:literal) => {
            #[inline(always)]
            pub fn $name(n: f32, m: f32, a: f32) -> f32 {
                let r: f32;
                unsafe {
                    asm!(concat!($ins, " {r:s}, {n:s}, {m:s}, {a:s}"), r = lateout(vreg) r, n = in(vreg) n, m = in(vreg) m,
                        a = in(vreg) a, options(pure, nomem, nostack));
                }
                r
            }
        };
    }
    binop!(add, "fadd");
    binop!(mul, "fmul");
    binop!(sub, "fsub");
    binop!(nmul, "fnmul");
    fused!(msub, "fmsub");
    fused!(madd, "fmadd");
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
    pub fn nmul(n: f32, m: f32) -> f32 {
        -(n * m)
    }
    pub fn msub(n: f32, m: f32, a: f32) -> f32 {
        (-n).mul_add(m, a)
    }
    pub fn madd(n: f32, m: f32, a: f32) -> f32 {
        n.mul_add(m, a)
    }
}

pub fn add(n: f32, m: f32) -> f32 {
    imp::add(n, m)
}

pub fn mul(n: f32, m: f32) -> f32 {
    imp::mul(n, m)
}

pub fn sub(n: f32, m: f32) -> f32 {
    imp::sub(n, m)
}

pub fn nmul(n: f32, m: f32) -> f32 {
    imp::nmul(n, m)
}

pub fn msub(n: f32, m: f32, a: f32) -> f32 {
    imp::msub(n, m, a)
}

pub fn madd(n: f32, m: f32, a: f32) -> f32 {
    imp::madd(n, m, a)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fused_ops() {
        assert_eq!(add(1.0, 2.0), 3.0);
        assert_eq!(sub(1.0, 2.0), -1.0);
        assert_eq!(mul(2.0, 3.0), 6.0);
        assert_eq!(madd(2.0, 3.0, 1.0), 7.0);
        assert_eq!(msub(2.0, 3.0, 10.0), 4.0);
        assert_eq!(nmul(2.0, 3.0), -6.0);
    }
}

unsafe extern "C" {
    fn sincosf(x: f32, s: *mut f32, c: *mut f32);
}

pub fn sincos(x: f32) -> (f32, f32) {
    let (mut s, mut c) = (0.0f32, 0.0f32);
    unsafe { sincosf(x, &mut s, &mut c) };
    (s, c)
}
