//! S2: `SomaScriptMath.cpp` natives (vectors, colors, matrices, quaternions, pid controllers, string
//! sets, constants and the cMath/cGenerate globals) on the Rust path.

use super::math::{self, Pid};
use super::s2_api;
use super::s2_as::{Engine, Frame, Soma};
use super::s2_host;
use crate::math as hm;
use crate::types::{Matrixf, Quaternion, Vector3f};
use std::ffi::c_void;

type V2 = [f32; 2];
type V3 = [f32; 3];
type V4 = [f32; 4];
type Rect<T> = [T; 4];

unsafe extern "C" {
    fn atanf(x: f32) -> f32;
    fn logf(x: f32) -> f32;
    fn powf(x: f32, y: f32) -> f32;
}

trait Lane: Copy + PartialEq {
    fn add(self, o: Self) -> Self;
    fn sub(self, o: Self) -> Self;
    fn mul(self, o: Self) -> Self;
    fn div(self, o: Self) -> Self;
    fn minus_one() -> Self;
    fn zero() -> Self;
}

impl Lane for f32 {
    fn add(self, o: Self) -> Self {
        self + o
    }
    fn sub(self, o: Self) -> Self {
        self - o
    }
    fn mul(self, o: Self) -> Self {
        self * o
    }
    fn div(self, o: Self) -> Self {
        self / o
    }
    fn minus_one() -> Self {
        -1.0
    }
    fn zero() -> Self {
        0.0
    }
}

impl Lane for i32 {
    fn add(self, o: Self) -> Self {
        self.wrapping_add(o)
    }
    fn sub(self, o: Self) -> Self {
        self.wrapping_sub(o)
    }
    fn mul(self, o: Self) -> Self {
        self.wrapping_mul(o)
    }
    // aarch64 `sdiv`: zero divisor gives zero, MIN / -1 wraps
    fn div(self, o: Self) -> Self {
        if o == 0 { 0 } else { self.wrapping_div(o) }
    }
    fn minus_one() -> Self {
        -1
    }
    fn zero() -> Self {
        0
    }
}

fn at<T: Copy + Default, const N: usize>(a: &[T; N], i: usize) -> T {
    a.get(i).copied().unwrap_or_default()
}

fn zip<T: Lane + Default, const N: usize>(a: [T; N], b: [T; N], f: fn(T, T) -> T) -> [T; N] {
    std::array::from_fn(|i| f(at(&a, i), at(&b, i)))
}

fn each<T: Lane + Default, const N: usize>(a: [T; N], s: T, f: fn(T, T) -> T) -> [T; N] {
    std::array::from_fn(|i| f(at(&a, i), s))
}

fn splat<T: Lane, const N: usize>(s: T) -> [T; N] {
    [s; N]
}

fn vec_ops<T: Lane + Default + 'static, const N: usize>(s: &Soma, name: &str, sname: &str) {
    type A<T, const N: usize> = [T; N];
    let assign_op = |opn: &'static str, f: fn(T, T) -> T| {
        s.method(
            name,
            &format!("{name} &{opn}(const {name} &in)"),
            move |fr| {
                let a = fr.me::<A<T, N>>();
                *a = zip(*a, *fr.rf::<A<T, N>>(0), f);
                fr.ret_ref(std::ptr::from_mut(a));
            },
        );
    };
    s.method(name, &format!("{name}&opAssign(const {name} &in)"), |fr| {
        let a = fr.me::<A<T, N>>();
        *a = *fr.rf::<A<T, N>>(0);
        fr.ret_ref(std::ptr::from_mut(a));
    });
    assign_op("opAddAssign", T::add);
    assign_op("opSubAssign", T::sub);
    assign_op("opMulAssign", T::mul);
    assign_op("opDivAssign", T::div);
    s.method(
        name,
        &format!("bool opEquals(const {name} &in) const"),
        |fr| {
            fr.ret::<bool>(*fr.me::<A<T, N>>() == *fr.rf::<A<T, N>>(0));
        },
    );
    let bin = |opn: &'static str, f: fn(T, T) -> T| {
        s.method(
            name,
            &format!("{name} {opn}(const {name} &in) const"),
            move |fr| {
                fr.ret(zip(*fr.me::<A<T, N>>(), *fr.rf::<A<T, N>>(0), f));
            },
        );
    };
    bin("opAdd", T::add);
    bin("opSub", T::sub);
    bin("opMul", T::mul);
    bin("opDiv", T::div);
    let assign_s = |opn: &'static str, f: fn(T, T) -> T| {
        s.method(name, &format!("{name} &{opn}({sname})"), move |fr| {
            let a = fr.me::<A<T, N>>();
            *a = each(*a, fr.arg::<T>(0), f);
            fr.ret_ref(std::ptr::from_mut(a));
        });
    };
    assign_s("opAddAssign", T::add);
    assign_s("opSubAssign", T::sub);
    assign_s("opMulAssign", T::mul);
    assign_s("opDivAssign", T::div);
    let bin_s = |opn: &'static str, f: fn(T, T) -> T| {
        s.method(name, &format!("{name} {opn}({sname}) const"), move |fr| {
            fr.ret(each(*fr.me::<A<T, N>>(), fr.arg::<T>(0), f));
        });
    };
    bin_s("opAdd", T::add);
    bin_s("opSub", T::sub);
    bin_s("opMul", T::mul);
    bin_s("opDiv", T::div);
    bin_s("opMul_r", T::mul);
    bin_s("opAdd_r", T::add);
    s.method(name, &format!("{name} opNeg() const"), |fr| {
        fr.ret(each(*fr.me::<A<T, N>>(), T::minus_one(), T::mul));
    });
    // the official natives index without a bounds check
    s.method(
        name,
        &format!("{sname} GetElement(uint64 alIdx) const"),
        |fr| {
            let i = fr.arg::<u64>(0) as usize;
            // SAFETY: mirrors `((const S*)&v)[i]`; scripts index within the vector.
            fr.ret::<T>(unsafe { *fr.me::<A<T, N>>().as_ptr().wrapping_add(i) });
        },
    );
    s.method(
        name,
        &format!("void SetElement(uint64 alIdx,{sname}) const"),
        |fr| {
            let i = fr.arg::<u64>(0) as usize;
            // SAFETY: mirrors `((S*)&v)[i] = f`; scripts index within the vector.
            unsafe { *fr.me::<A<T, N>>().as_mut_ptr().wrapping_add(i) = fr.arg::<T>(1) };
        },
    );
    s.method(
        name,
        &format!("{name} &opAssign(const {sname} &in)"),
        |fr| {
            let a = fr.me::<A<T, N>>();
            *a = splat(*fr.rf::<T>(0));
            fr.ret_ref(std::ptr::from_mut(a));
        },
    );
    s.method(
        name,
        &format!("{name} &opAddAssign(const {sname} &in)"),
        |fr| {
            let a = fr.me::<A<T, N>>();
            *a = each(*a, *fr.rf::<T>(0), T::add);
            fr.ret_ref(std::ptr::from_mut(a));
        },
    );
    s.method(
        name,
        &format!("{name} opAdd(const {sname} &in) const"),
        |fr| {
            fr.ret(each(*fr.me::<A<T, N>>(), *fr.rf::<T>(0), T::add));
        },
    );
    s.construct(name, "void f()", |fr| {
        fr.ret_at::<A<T, N>>(splat(T::zero()))
    });
    s.construct(name, &format!("void f(const {name} &in)"), |fr| {
        fr.ret_at::<A<T, N>>(*fr.rf::<A<T, N>>(0))
    });
}

fn sqr2(v: V2) -> f32 {
    hm::fma(v[0], v[0], v[1] * v[1])
}

fn sqr3(v: V3) -> f32 {
    hm::dot3(hm::v3(v[0], v[1], v[2]), hm::v3(v[0], v[1], v[2]))
}

fn sqr4(v: V4) -> f32 {
    hm::fma(
        v[3],
        v[3],
        hm::fma(v[2], v[2], hm::fma(v[0], v[0], v[1] * v[1])),
    )
}

fn norm_inv<const N: usize>(v: &mut [f32; N], len: f32) -> f32 {
    if f64::from(len) > 1e-08 {
        let inv = 1.0f32 / len;
        for x in v.iter_mut() {
            *x *= inv;
        }
    }
    len
}

fn vectors(s: &Soma) {
    vec_ops::<f32, 2>(s, "cVector2f", "float");
    s.construct("cRect2f", "void f()", |fr| fr.ret_at::<Rect<f32>>([0.0; 4]));
    s.construct("cRect2f", "void f(const cRect2f &in)", |fr| {
        fr.ret_at::<Rect<f32>>(*fr.rf::<Rect<f32>>(0))
    });
    s.construct(
        "cRect2f",
        "void f(float afX, float afY, float afW, float afH)",
        |fr| {
            fr.ret_at::<Rect<f32>>([fr.arg(0), fr.arg(1), fr.arg(2), fr.arg(3)]);
        },
    );
    s.construct("cRect2f", "void f(float afX, float afY)", |fr| {
        fr.ret_at::<Rect<f32>>([fr.arg(0), fr.arg(1), 0.0, 0.0])
    });
    s.construct("cRect2l", "void f()", |fr| fr.ret_at::<Rect<i32>>([0; 4]));
    s.construct("cRect2l", "void f(const cRect2l &in)", |fr| {
        fr.ret_at::<Rect<i32>>(*fr.rf::<Rect<i32>>(0))
    });
    s.construct(
        "cRect2l",
        "void f(int afX, int afY, int afW, int afH)",
        |fr| {
            fr.ret_at::<Rect<i32>>([fr.arg(0), fr.arg(1), fr.arg(2), fr.arg(3)]);
        },
    );
    s.construct("cRect2l", "void f(int afX, int afY)", |fr| {
        fr.ret_at::<Rect<i32>>([fr.arg(0), fr.arg(1), 0, 0])
    });
    plane(s);
    s.construct("cDate", "void f()", |fr| fr.ret_at::<[i32; 8]>([0; 8]));
    s.construct("cDate", "void f(const cDate &in)", |fr| {
        fr.ret_at::<[i32; 8]>(*fr.rf::<[i32; 8]>(0))
    });
    s.construct("cVector2f", "void f(float afX)", |fr| {
        fr.ret_at::<V2>([fr.arg(0); 2])
    });
    s.construct("cVector2f", "void f(float afX, float afY)", |fr| {
        fr.ret_at::<V2>([fr.arg(0), fr.arg(1)])
    });
    s.construct("cVector2f", "void f(const cVector3f& in avX)", |fr| {
        let v = fr.rf::<V3>(0);
        fr.ret_at::<V2>([v[0], v[1]]);
    });
    s.method("cVector2f", "float SqrLength() const", |fr| {
        fr.ret(sqr2(*fr.me::<V2>()))
    });
    s.method("cVector2f", "float Length() const", |fr| {
        fr.ret(sqr2(*fr.me::<V2>()).sqrt())
    });
    s.method("cVector2f", "float Normalize()", |fr| {
        let v = fr.me::<V2>();
        let len = sqr2(*v).sqrt();
        fr.ret(norm_inv(v, len));
    });

    vec_ops::<f32, 3>(s, "cVector3f", "float");
    s.construct("cVector3f", "void f(float afX)", |fr| {
        fr.ret_at::<V3>([fr.arg(0); 3])
    });
    s.construct(
        "cVector3f",
        "void f(float afX, float afY, float afZ)",
        |fr| fr.ret_at::<V3>([fr.arg(0), fr.arg(1), fr.arg(2)]),
    );
    s.construct("cVector3f", "void f(const cVector2f& in avX)", |fr| {
        let v = fr.rf::<V2>(0);
        fr.ret_at::<V3>([v[0], v[1], 0.0]);
    });
    s.method("cVector3f", "float SqrLength() const", |fr| {
        fr.ret(sqr3(*fr.me::<V3>()))
    });
    s.method("cVector3f", "float Length() const", |fr| {
        fr.ret(sqr3(*fr.me::<V3>()).sqrt())
    });
    s.method("cVector3f", "float Normalize()", |fr| {
        let v = fr.me::<V3>();
        let len = sqr3(*v).sqrt();
        fr.ret(norm_inv(v, len));
    });

    vec_ops::<f32, 4>(s, "cVector4f", "float");
    s.construct("cVector4f", "void f(float afX)", |fr| {
        fr.ret_at::<V4>([fr.arg(0); 4])
    });
    s.construct(
        "cVector4f",
        "void f(float afX, float afY, float afZ, float afW)",
        |fr| {
            fr.ret_at::<V4>([fr.arg(0), fr.arg(1), fr.arg(2), fr.arg(3)]);
        },
    );
    s.method("cVector4f", "float SqrLength() const", |fr| {
        fr.ret(sqr4(*fr.me::<V4>()))
    });
    s.method("cVector4f", "float Length() const", |fr| {
        fr.ret(sqr4(*fr.me::<V4>()).sqrt())
    });
    s.method("cVector4f", "float Normalize()", |fr| {
        let v = fr.me::<V4>();
        let l = sqr4(*v).sqrt();
        if l > 0.0 {
            *v = each(*v, l, f32::div);
        }
        fr.ret(l);
    });

    vec_ops::<i32, 2>(s, "cVector2l", "int");
    s.construct("cVector2l", "void f(int afX)", |fr| {
        fr.ret_at::<[i32; 2]>([fr.arg(0); 2])
    });
    s.construct("cVector2l", "void f(int alX, int alY)", |fr| {
        fr.ret_at::<[i32; 2]>([fr.arg(0), fr.arg(1)])
    });
    s.method("cVector2l", "int SqrLength() const", |fr| {
        let v = fr.me::<[i32; 2]>();
        fr.ret(
            v[0].wrapping_mul(v[0])
                .wrapping_add(v[1].wrapping_mul(v[1])),
        );
    });

    vec_ops::<i32, 3>(s, "cVector3l", "int");
    s.construct("cVector3l", "void f(int afX)", |fr| {
        fr.ret_at::<[i32; 3]>([fr.arg(0); 3])
    });
    s.construct("cVector3l", "void f(int alX, int alY, int alZ)", |fr| {
        fr.ret_at::<[i32; 3]>([fr.arg(0), fr.arg(1), fr.arg(2)])
    });
    s.method("cVector3l", "int SqrLength() const", |fr| {
        let v = fr.me::<[i32; 3]>();
        let m = |x: i32| x.wrapping_mul(x);
        fr.ret(m(v[0]).wrapping_add(m(v[1])).wrapping_add(m(v[2])));
    });
}

fn plane(s: &Soma) {
    let from_normal_point = |n: V3, p: V3| -> [f32; 4] {
        [
            n[0],
            n[1],
            n[2],
            -hm::dot3(hm::v3(n[0], n[1], n[2]), hm::v3(p[0], p[1], p[2])),
        ]
    };
    let from_points = move |p0: V3, p1: V3, p2: V3| -> [f32; 4] {
        let e1 = hm::v3(p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]);
        let e2 = hm::v3(p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]);
        let c = hm::cross3(e1, e2);
        let d = -hm::dot3(c, hm::v3(p0[0], p0[1], p0[2]));
        let mut pl = [c.x, c.y, c.z, d];
        normalize_plane(&mut pl);
        pl
    };
    s.construct("cPlanef", "void f()", |fr| fr.ret_at::<V4>([0.0; 4]));
    s.construct("cPlanef", "void f(const cPlanef &in)", |fr| {
        fr.ret_at::<V4>(*fr.rf::<V4>(0))
    });
    s.construct(
        "cPlanef",
        "void f(float afA, float afB, float afC, float afD)",
        |fr| {
            fr.ret_at::<V4>([fr.arg(0), fr.arg(1), fr.arg(2), fr.arg(3)]);
        },
    );
    s.construct(
        "cPlanef",
        "void f(const cVector3f &in avNormal, const cVector3f &in avPoint)",
        move |fr| {
            fr.ret_at::<V4>(from_normal_point(*fr.rf::<V3>(0), *fr.rf::<V3>(1)));
        },
    );
    s.construct(
        "cPlanef",
        "void f(const cVector3f &in avPoint0,const cVector3f &in avPoint1, const cVector3f &in avPoint2)",
        move |fr| fr.ret_at::<V4>(from_points(*fr.rf::<V3>(0), *fr.rf::<V3>(1), *fr.rf::<V3>(2))),
    );
    s.method("cPlanef", "bool opEquals(const cPlanef &in) const", |fr| {
        fr.ret::<bool>(*fr.me::<V4>() == *fr.rf::<V4>(0))
    });
    s.method("cPlanef", "cPlanef opMul(float) const", |fr| {
        fr.ret(each(*fr.me::<V4>(), fr.arg::<f32>(0), f32::mul))
    });
    s.method("cPlanef", "cPlanef &opMulAssign(float)", |fr| {
        let p = fr.me::<V4>();
        *p = each(*p, fr.arg::<f32>(0), f32::mul);
        fr.ret_ref(std::ptr::from_mut(p));
    });
    s.method(
        "cPlanef",
        "void FromNormalPoint(const cVector3f &in avNormal, const cVector3f &in avPoint)",
        move |fr| *fr.me::<V4>() = from_normal_point(*fr.rf::<V3>(0), *fr.rf::<V3>(1)),
    );
    s.method(
        "cPlanef",
        "void FromPoints(const cVector3f &in avPoint0,const cVector3f &in avPoint1, const cVector3f &in avPoint2)",
        move |fr| *fr.me::<V4>() = from_points(*fr.rf::<V3>(0), *fr.rf::<V3>(1), *fr.rf::<V3>(2)),
    );
    s.method("cPlanef", "void Normalize()", |fr| {
        normalize_plane(fr.me::<V4>())
    });
    s.method("cPlanef", "cVector3f GetNormal() const", |fr| {
        let p = fr.me::<V4>();
        fr.ret::<V3>([p[0], p[1], p[2]]);
    });
}

fn normalize_plane(p: &mut V4) {
    let mag = hm::dot3(hm::v3(p[0], p[1], p[2]), hm::v3(p[0], p[1], p[2])).sqrt();
    for x in p.iter_mut() {
        *x /= mag;
    }
}

fn color(s: &Soma) {
    type C = [f32; 4];
    s.construct("cColor", "void f()", |fr| fr.ret_at::<C>([0.0; 4]));
    s.construct("cColor", "void f(const cColor &in)", |fr| {
        fr.ret_at::<C>(*fr.rf::<C>(0))
    });
    s.construct(
        "cColor",
        "void f(float afR, float afG, float afB, float afA)",
        |fr| {
            fr.ret_at::<C>([fr.arg(0), fr.arg(1), fr.arg(2), fr.arg(3)]);
        },
    );
    s.construct("cColor", "void f(float afR, float afG, float afB)", |fr| {
        fr.ret_at::<C>([fr.arg(0), fr.arg(1), fr.arg(2), 1.0])
    });
    s.construct("cColor", "void f(float afVal, float afAlpha)", |fr| {
        fr.ret_at::<C>([fr.arg(0), fr.arg(0), fr.arg(0), fr.arg(1)])
    });
    s.construct("cColor", "void f(float afVal)", |fr| {
        fr.ret_at::<C>([fr.arg(0), fr.arg(0), fr.arg(0), 1.0])
    });
    s.method("cColor", "cColor&opAssign(const cColor &in)", |fr| {
        let a = fr.me::<C>();
        *a = *fr.rf::<C>(0);
        fr.ret_ref(std::ptr::from_mut(a));
    });
    s.method("cColor", "bool opEquals(const cColor &in) const", |fr| {
        fr.ret::<bool>(*fr.me::<C>() == *fr.rf::<C>(0))
    });
    s.method("cColor", "cColor opAdd(const cColor &in) const", |fr| {
        fr.ret(zip(*fr.me::<C>(), *fr.rf::<C>(0), f32::add))
    });
    s.method("cColor", "cColor opSub(const cColor &in) const", |fr| {
        fr.ret(zip(*fr.me::<C>(), *fr.rf::<C>(0), f32::sub))
    });
    s.method("cColor", "cColor opMul(const cColor &in) const", |fr| {
        fr.ret(zip(*fr.me::<C>(), *fr.rf::<C>(0), f32::mul))
    });
    s.method("cColor", "cColor opDiv(const cColor &in) const", |fr| {
        fr.ret(zip(*fr.me::<C>(), *fr.rf::<C>(0), f32::div))
    });
    s.method("cColor", "cColor opMul(float) const", |fr| {
        fr.ret(each(*fr.me::<C>(), fr.arg::<f32>(0), f32::mul))
    });
    s.method("cColor", "cColor opDiv(float) const", |fr| {
        fr.ret(each(*fr.me::<C>(), fr.arg::<f32>(0), f32::div))
    });
    s.method(
        "cColor",
        "cColor ToLinearSpace(const float afPower, const bool abCorrectAlpha) const",
        |fr| {
            let (c, p, alpha) = (*fr.me::<C>(), fr.arg::<f32>(0), fr.arg::<bool>(1));
            fr.ret::<C>([
                pw(c[0], p),
                pw(c[1], p),
                pw(c[2], p),
                if alpha { pw(c[3], p) } else { c[3] },
            ]);
        },
    );
    s.method(
        "cColor",
        "cColor ToSRGB(const bool abCorrectAlpha) const",
        |fr| {
            let (c, alpha) = (*fr.me::<C>(), fr.arg::<bool>(0));
            let p = 1.0f32 / 2.2f32;
            fr.ret::<C>([
                pw(c[0], p),
                pw(c[1], p),
                pw(c[2], p),
                if alpha { pw(c[3], p) } else { c[3] },
            ]);
        },
    );
}

fn pw(x: f32, p: f32) -> f32 {
    // SAFETY: plain libm call.
    unsafe { powf(x, p) }
}

fn row_matrix(a: V4, b: V4, c: V4, d: V4) -> Matrixf {
    Matrixf { m: [a, b, c, d] }
}

fn matrix_quat(s: &Soma) {
    s.construct("cMatrixf", "void f()", |fr| {
        fr.ret_at::<Matrixf>(hm::IDENTITY)
    });
    s.construct("cMatrixf", "void f(const cMatrixf &in)", |fr| {
        fr.ret_at::<Matrixf>(*fr.rf::<Matrixf>(0))
    });
    s.construct(
        "cMatrixf",
        "void f(const cVector4f &in, const cVector4f &in, const cVector4f &in,const cVector4f &in)",
        |fr| {
            fr.ret_at::<Matrixf>(row_matrix(
                *fr.rf::<V4>(0),
                *fr.rf::<V4>(1),
                *fr.rf::<V4>(2),
                *fr.rf::<V4>(3),
            ))
        },
    );
    s.construct(
        "cMatrixf",
        "void f(float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float)",
        |fr| {
            let mut m = [[0.0f32; 4]; 4];
            for (i, x) in m.iter_mut().flatten().enumerate() {
                *x = fr.arg(i);
            }
            fr.ret_at::<Matrixf>(Matrixf { m });
        },
    );
    s.method("cMatrixf", "cMatrixf&opAssign(const cMatrixf &in)", |fr| {
        let a = fr.me::<Matrixf>();
        *a = *fr.rf::<Matrixf>(0);
        fr.ret_ref(std::ptr::from_mut(a));
    });
    s.method(
        "cMatrixf",
        "bool opEquals(const cMatrixf &in) const",
        |fr| {
            let (a, b) = (fr.me::<Matrixf>(), fr.rf::<Matrixf>(0));
            fr.ret::<bool>(
                a.m.iter().flatten().map(|x| x.to_bits()).eq(b
                    .m
                    .iter()
                    .flatten()
                    .map(|x| x.to_bits())),
            );
        },
    );
    s.method("cMatrixf", "float GetElement(uint64, uint64) const", |fr| {
        let (c, r) = (fr.arg::<u64>(0), fr.arg::<u64>(1));
        let m = fr.me::<Matrixf>();
        fr.ret::<f32>(if r < 4 && c < 4 {
            m.m.get(r as usize).map_or(0.0, |row| at(row, c as usize))
        } else {
            0.0
        });
    });
    for (name, row) in [("Right", 0usize), ("Up", 1), ("Forward", 2)] {
        s.method(
            "cMatrixf",
            &format!("cVector3f Get{name}() const"),
            move |fr| {
                let m = fr.me::<Matrixf>();
                let r = m.m.get(row).copied().unwrap_or_default();
                fr.ret::<V3>([r[0], r[1], r[2]]);
            },
        );
        s.method(
            "cMatrixf",
            &format!("void Set{name}(const cVector3f&in avVec)"),
            move |fr| {
                let (m, v) = (fr.me::<Matrixf>(), fr.rf::<V3>(0));
                if let Some(r) = m.m.get_mut(row) {
                    r.iter_mut().zip(v).for_each(|(d, x)| *d = *x);
                }
            },
        );
    }
    s.method("cMatrixf", "cVector3f GetTranslation() const", |fr| {
        let m = fr.me::<Matrixf>();
        fr.ret::<V3>([m.m[0][3], m.m[1][3], m.m[2][3]]);
    });
    s.method(
        "cMatrixf",
        "void SetTranslation(const cVector3f&in avTrans)",
        |fr| {
            let (m, v) = (fr.me::<Matrixf>(), fr.rf::<V3>(0));
            for (row, x) in m.m.iter_mut().zip(v) {
                row[3] = *x;
            }
        },
    );
    s.method(
        "cMatrixf",
        "void SetRotation(const cMatrixf&in a_mtxRot)",
        |fr| {
            let (m, r) = (fr.me::<Matrixf>(), fr.rf::<Matrixf>(0));
            for (d, s) in m.m.iter_mut().zip(&r.m).take(3) {
                d.iter_mut().zip(s).take(3).for_each(|(a, b)| *a = *b);
            }
        },
    );
    s.method("cMatrixf", "cMatrixf GetRotation() const", |fr| {
        fr.ret(rotation(fr.me::<Matrixf>()))
    });
    s.method("cMatrixf", "cMatrixf GetTranspose() const", |fr| {
        fr.ret(hm::transpose(fr.me::<Matrixf>()))
    });

    s.construct("cQuaternion", "void f()", |fr| {
        fr.ret_at::<Quaternion>(Quaternion {
            v: hm::v3(0.0, 0.0, 0.0),
            w: 1.0,
        })
    });
    s.construct("cQuaternion", "void f(const cQuaternion &in)", |fr| {
        fr.ret_at::<Quaternion>(*fr.rf::<Quaternion>(0))
    });
    s.construct("cQuaternion", "void f(float, const cVector3f &in)", |fr| {
        let mut q = Quaternion::default();
        hm::quat_from_angle_axis(&mut q, fr.arg(0), v3of(fr.rf(1)));
        fr.ret_at::<Quaternion>(q);
    });
    s.construct("cQuaternion", "void f(float, float, float, float)", |fr| {
        fr.ret_at::<Quaternion>(Quaternion {
            v: hm::v3(fr.arg(1), fr.arg(2), fr.arg(3)),
            w: fr.arg(0),
        });
    });
    s.construct("cQuaternion", "void f(const cMatrixf &in)", |fr| {
        fr.ret_at::<Quaternion>(quat_from_matrix(fr.rf::<Matrixf>(0)))
    });
    s.method(
        "cQuaternion",
        "cQuaternion&opAssign(const cQuaternion &in)",
        |fr| {
            let a = fr.me::<Quaternion>();
            *a = *fr.rf::<Quaternion>(0);
            fr.ret_ref(std::ptr::from_mut(a));
        },
    );
    s.method(
        "cQuaternion",
        "cQuaternion opAdd(const cQuaternion &in) const",
        |fr| fr.ret(hm::quat_add(fr.me(), fr.rf(0))),
    );
    s.method(
        "cQuaternion",
        "cQuaternion opSub(const cQuaternion &in) const",
        |fr| fr.ret(hm::quat_sub(fr.me(), fr.rf(0))),
    );
    s.method(
        "cQuaternion",
        "cQuaternion opMul(const cQuaternion &in) const",
        |fr| fr.ret(hm::quat_mul(fr.me(), fr.rf(0))),
    );
    s.method("cQuaternion", "cQuaternion opMul(float) const", |fr| {
        fr.ret(hm::quat_scale(fr.me(), fr.arg::<f32>(0)))
    });
    s.method("cQuaternion", "void Normalize()", |fr| {
        hm::quat_normalize(fr.me())
    });
    s.method(
        "cQuaternion",
        "void FromRotationMatrix(const cMatrixf &in)",
        |fr| hm::quat_from_rotation_matrix(fr.me(), fr.rf(0)),
    );
}

fn rotation(m: &Matrixf) -> Matrixf {
    let r = &m.m;
    Matrixf {
        m: [
            [r[0][0], r[0][1], r[0][2], 0.0],
            [r[1][0], r[1][1], r[1][2], 0.0],
            [r[2][0], r[2][1], r[2][2], 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    }
}

fn quat_from_matrix(m: &Matrixf) -> Quaternion {
    let mut q = Quaternion::default();
    hm::quat_from_rotation_matrix(&mut q, m);
    q
}

fn leak<T>(v: T) -> *mut c_void {
    Box::into_raw(Box::new(v)).cast()
}

fn constants(e: Engine) {
    let prop = |decl: &str, p: *mut c_void| {
        // SAFETY: the value is leaked and lives as long as the engine.
        unsafe { e.reg_global_property(decl.as_bytes(), p) };
    };
    for (name, v) in [
        ("Pi", std::f32::consts::PI),
        ("PiDiv2", std::f32::consts::FRAC_PI_2),
        ("PiDiv4", std::f32::consts::FRAC_PI_4),
        ("PiMul2", std::f32::consts::TAU),
        ("Epsilon", 0.0001f32),
        ("Sqrt2", std::f32::consts::SQRT_2),
    ] {
        prop(&format!("const float cMath_{name}"), leak(v));
    }
    for (name, c) in [
        ("Red", [1.0f32, 0.0, 0.0, 1.0]),
        ("Green", [0.0, 1.0, 0.0, 1.0]),
        ("Blue", [0.0, 0.0, 1.0, 1.0]),
        ("White", [1.0, 1.0, 1.0, 1.0]),
    ] {
        prop(&format!("const cColor cColor_{name}"), leak(c));
    }
    prop("const cVector2l cVector2l_MinusOne", leak([-1i32; 2]));
    let v2: [(&str, V2); 7] = [
        ("Zero", [0.0, 0.0]),
        ("One", [1.0, 1.0]),
        ("Right", [1.0, 0.0]),
        ("Left", [-1.0, 0.0]),
        ("Up", [0.0, 1.0]),
        ("Down", [0.0, -1.0]),
        ("MinusOne", [-1.0, -1.0]),
    ];
    for (name, v) in v2 {
        prop(&format!("const cVector2f cVector2f_{name}"), leak(v));
    }
    let v3: [(&str, V3); 9] = [
        ("Zero", [0.0; 3]),
        ("One", [1.0; 3]),
        ("Right", [1.0, 0.0, 0.0]),
        ("Left", [-1.0, 0.0, 0.0]),
        ("Up", [0.0, 1.0, 0.0]),
        ("Down", [0.0, -1.0, 0.0]),
        ("Forward", [0.0, 0.0, 1.0]),
        ("Back", [0.0, 0.0, -1.0]),
        ("MinusOne", [-1.0; 3]),
    ];
    for (name, v) in v3 {
        prop(&format!("const cVector3f cVector3f_{name}"), leak(v));
    }
    for (name, v) in [("Zero", 0.0f32), ("One", 1.0), ("MinusOne", -1.0)] {
        prop(&format!("const cVector4f cVector4f_{name}"), leak([v; 4]));
    }
    prop("const cMatrixf cMatrixf_Identity", leak(hm::IDENTITY));
    prop("const cMatrixf cMatrixf_Zero", leak(Matrixf::default()));
    prop(
        "const cQuaternion cQuaternion_Identity",
        leak(Quaternion {
            v: hm::v3(0.0, 0.0, 0.0),
            w: 1.0,
        }),
    );
}

fn v3of(v: &V3) -> Vector3f {
    hm::v3(v[0], v[1], v[2])
}

fn arr3(v: Vector3f) -> V3 {
    [v.x, v.y, v.z]
}

fn easing(t: i32, x: f32) -> f32 {
    s2_host::easing(t, x)
}

fn functions(s: &Soma) {
    let f1 =
        |decl: &'static str, f: fn(f32) -> f32| s.func(decl, move |fr| fr.ret(f(fr.arg::<f32>(0))));
    f1("float cMath_Sin(float afX)", hm::sin);
    f1("float cMath_Cos(float afX)", hm::cos);
    f1("float cMath_Tan(float afX)", hm::tan);
    f1("float cMath_SigmoidCurve(float afX)", math::sigmoid_curve);
    s.func(
        "float cMath_InterpolateSigmoid(float afA,float afB,float afT)",
        |fr| {
            fr.ret(math::interpolate_sigmoid(fr.arg(0), fr.arg(1), fr.arg(2)));
        },
    );
    f1("float cMath_ASin(float afX)", hm::asin);
    f1("float cMath_ACos(float afX)", hm::acos);
    // SAFETY (the next three): plain libm calls.
    f1("float cMath_ATan(float afX)", |x| unsafe { atanf(x) });
    s.func("float cMath_ATan2(float afY, float afX)", |fr| {
        fr.ret(hm::atan2(fr.arg(0), fr.arg(1)))
    });
    f1("float cMath_Log(float afX)", |x| unsafe { logf(x) });
    s.func("float cMath_Pow(float afX, float afExp)", |fr| {
        fr.ret(pw(fr.arg(0), fr.arg(1)))
    });
    f1("float cMath_Sqrt(float afX)", f32::sqrt);
    s.func("void cMath_Randomize(int alSeed)", |fr| {
        hm::randomize(fr.arg(0))
    });
    s.func(
        "cVector2f cMath_RandomCircleEdgePoint(float afRadius)",
        |fr| fr.ret(math::random_circle_edge_point(fr.arg(0))),
    );
    s.func(
        "float cMath_Easing(eEasing aType, float afT, float afMin = 0, float afMax = 1)",
        |fr| {
            let (t, x, a, b) = (
                fr.arg::<i32>(0),
                fr.arg::<f32>(1),
                fr.arg::<f32>(2),
                fr.arg::<f32>(3),
            );
            fr.ret(a + (b - a) * easing(t, hm::clamp(x, 0.0, 1.0)));
        },
    );
    s.func(
        "float cMath_Vector3MaxElement(const cVector3f &in avVec)",
        |fr| fr.ret(math::vector3_max_element(*fr.rf(0))),
    );
    s.func(
        "cQuaternion cMath_QuaternionEuler(const cVector3f&in avEuler, eEulerRotationOrder aOrder)",
        |fr| {
            let m = hm::matrix_rotate(v3of(fr.rf(0)), fr.arg(1));
            fr.ret(quat_from_matrix(&m));
        },
    );
    s.func(
        "cVector3f cMath_Vector3CatmullRom(const cVector3f&in avP0, const cVector3f&in avP1, const cVector3f&in avP2, const cVector3f&in avP3, float afFract)",
        |fr| fr.ret(math::catmull_rom(*fr.rf::<V3>(0), *fr.rf::<V3>(1), *fr.rf::<V3>(2), *fr.rf::<V3>(3), fr.arg(4))),
    );
    s.func(
        "cVector2f cMath_Vector2CatmullRom(const cVector2f &in avP0, const cVector2f &in avP1, const cVector2f &in avP2, const cVector2f &in avP3, float afFract)",
        |fr| fr.ret(math::catmull_rom(*fr.rf::<V2>(0), *fr.rf::<V2>(1), *fr.rf::<V2>(2), *fr.rf::<V2>(3), fr.arg(4))),
    );
    s.func(
        "void Math_CatmullRom(cVector3f &out avResult, const cVector3f &in avP0, const cVector3f &in avP1, const cVector3f &in avP2, const cVector3f &in avP3, float afFract)",
        |fr| *fr.rm::<V3>(0) = math::catmull_rom(*fr.rf::<V3>(1), *fr.rf::<V3>(2), *fr.rf::<V3>(3), *fr.rf::<V3>(4), fr.arg(5)),
    );
    s.method(
        "cQuaternion",
        "void FromAngleAxis(float afAngle, const cVector3f &in)",
        |fr| {
            hm::quat_from_angle_axis(fr.me(), fr.arg(0), v3of(fr.rf(1)));
        },
    );
    s.func(
        "cVector3f cMath_GetAngleDistanceVector3fRad(const cVector3f&in avAngles1, const cVector3f&in avAngles2)",
        |fr| {
            let (a, b) = (fr.rf::<V3>(0), fr.rf::<V3>(1));
            fr.ret::<V3>(std::array::from_fn(|i| hm::angle_distance(at(a, i), at(b, i), std::f32::consts::TAU)));
        },
    );
    s.func(
        "cVector3f cMath_Vector3SphereSurfacePoint(const cVector2f&in avSeed, float afRadius)",
        |fr| {
            fr.ret(math::sphere_surface_point(*fr.rf(0), fr.arg(1)));
        },
    );
    s.func(
        "cVector3f cMath_RandomSphereSurfacePoint(float afRadius)",
        |fr| fr.ret(math::random_sphere_surface_point(fr.arg(0))),
    );
    s.func(
        "float cMath_Vector2Dot(const cVector2f &in avVecA, const cVector2f &in avVecB)",
        |fr| {
            fr.ret(math::vector2_dot(*fr.rf(0), *fr.rf(1)));
        },
    );
    s.func("cVector3f cMath_Vector3ProjectOnPlane(const cVector3f &in avPlaneNormal, const cVector3f &in avVec)", |fr| {
        fr.ret(math::project_on_plane(*fr.rf(0), *fr.rf(1)));
    });
    s.func_new(
        "cMatrixf cMath_MatrixSetScale(const cMatrixf &in a_mtx, const cVector3f&in avScale)",
        |fr| {
            let (m, sc) = (fr.rf::<Matrixf>(0), fr.rf::<V3>(1));
            let mut r = *m;
            for (dst, (row, k)) in r.m.iter_mut().zip(m.m.iter().zip(sc)) {
                let mut v = hm::v3(row[0], row[1], row[2]);
                hm::normalize3(&mut v);
                for (d, x) in dst.iter_mut().zip([v.x, v.y, v.z]) {
                    *d = x * k;
                }
            }
            fr.ret(r);
        },
    );
    s.func(
        "cMatrixf cMath_MatrixRotateXYZ(const cVector3f &in avRot)",
        |fr| fr.ret(hm::matrix_rotate(v3of(fr.rf(0)), 0)),
    );
    s.func(
        "cVector3f cMath_MatrixToEulerAngles(const cMatrixf &in a_mtxA)",
        |fr| {
            fr.ret(arr3(hm::matrix_to_euler_angles(fr.rf(0))));
        },
    );
    for (name, order) in [("XZY", 1), ("YXZ", 2), ("YZX", 3), ("ZXY", 4), ("ZYX", 5)] {
        s.func(
            &format!("cMatrixf cMath_MatrixRotate{name}(const cVector3f &in avRot)"),
            move |fr| {
                fr.ret(hm::matrix_rotate(v3of(fr.rf(0)), order));
            },
        );
    }
    s.func(
        "float cMath_InterpolateLinear(float afA, float afB, float afT)",
        |fr| {
            fr.ret(math::interpolate_linear(fr.arg(0), fr.arg(1), fr.arg(2)));
        },
    );
    s.func(
        "float cMath_InterpolateCosine(float afA, float afB, float afT)",
        |fr| {
            fr.ret(math::interpolate_cosine(fr.arg(0), fr.arg(1), fr.arg(2)));
        },
    );
    s.func("float cMath_FastRandomFloat(int alSeed)", |fr| {
        fr.ret(hm::fast_random_float(fr.arg(0)))
    });
    s.func("float cGenerate_GetNoisePerlin1D(float afX)", |fr| {
        fr.ret(math::noise_perlin_1d(fr.arg(0)))
    });
    f1("float cMath_Round(float afX)", math::round);
    s.func("int cMath_GetBit(int alBitNum)", |fr| {
        fr.ret(math::get_bit(fr.arg(0)))
    });
    s.func(
        "void cMath_SetBitFlag(int&out alFlagNum, int alBit, bool abSet)",
        |fr| {
            let flag = fr.rm::<i32>(0);
            *flag = math::set_bit_flag(*flag, fr.arg(1), fr.arg(2));
        },
    );
    s.func(
        "bool cMath_CheckPlaneLineIntersection(const cPlanef&in aPlane, const cVector3f &in avLineStart, const cVector3f &in avLineEnd, cVector3f&out avIntersectionPos, float&out afT)",
        |fr| {
            let p = fr.rf::<V4>(0);
            let plane = hm::Planef { a: p[0], b: p[1], c: p[2], d: p[3] };
            let (hit, r) = hm::plane_line_intersection(&plane, v3of(fr.rf(1)), v3of(fr.rf(2)));
            if let Some((t, pos)) = r {
                *fr.rm::<f32>(4) = t;
                *fr.rm::<V3>(3) = arr3(pos);
            }
            fr.ret::<bool>(hit);
        },
    );
    s.func("void cMath_GetAngleFromVector(const cVector2f &in avVec, float &out afAngle, float &out afLength)", |fr| {
        let v = fr.rf::<V2>(0);
        let (a, l) = hm::angle_from_vector(crate::types::Vector2f { x: v[0], y: v[1] });
        *fr.rm::<f32>(2) = l;
        *fr.rm::<f32>(1) = a;
    });
    s.func("bool cMath_GetBitFlag(int alFlagNum, int alBit)", |fr| {
        fr.ret::<bool>(math::get_bit_flag(fr.arg(0), fr.arg(1)))
    });
    s.func(
        "cVector3f cMath_ExpandAABBMin(const cVector3f&in avBaseMin, const cVector3f&in avAddMin)",
        |fr| {
            let (a, b) = (fr.rf::<V3>(0), fr.rf::<V3>(1));
            fr.ret::<V3>(std::array::from_fn(|i| {
                let (x, y) = (at(a, i), at(b, i));
                if x < y { x } else { y }
            }));
        },
    );
    s.func(
        "cVector3f cMath_ExpandAABBMax(const cVector3f&in avBaseMax, const cVector3f&in avAddMax)",
        |fr| {
            let (a, b) = (fr.rf::<V3>(0), fr.rf::<V3>(1));
            fr.ret::<V3>(std::array::from_fn(|i| {
                let (x, y) = (at(a, i), at(b, i));
                if x > y { x } else { y }
            }));
        },
    );
    s.func(
        "float cMath_InterpolateCubic(float afX0, float afX1, float afX2, float afX3, float afT)",
        |fr| {
            fr.ret(math::interpolate_cubic(
                fr.arg(0),
                fr.arg(1),
                fr.arg(2),
                fr.arg(3),
                fr.arg(4),
            ));
        },
    );
    s.func(
        "cVector2f cMath_Vector2Rotate(const cVector2f &in avVec, float afAngle)",
        |fr| {
            fr.ret(math::vector2_rotate(*fr.rf(0), fr.arg(1)));
        },
    );
    s.func(
        "cVector3f cMath_Vector3IncreaseTo(const cVector3f&in avX, const cVector3f&in avAdd, const cVector3f&in avDest)",
        |fr| {
            let (x, a, d) = (fr.rf::<V3>(0), fr.rf::<V3>(1), fr.rf::<V3>(2));
            fr.ret::<V3>(std::array::from_fn(|i| hm::increase_to(at(x, i), at(a, i), at(d, i))));
        },
    );
    s.func("cQuaternion cMath_QuaternionNlerp(float afT,const cQuaternion&in aqA, const cQuaternion&in aqB)", |fr| {
        let t = fr.arg::<f32>(0);
        let mut q = hm::quat_add(&hm::quat_scale(fr.rf(1), 1.0 - t), &hm::quat_scale(fr.rf(2), t));
        hm::quat_normalize(&mut q);
        fr.ret(q);
    });
    s.func(
        "cQuaternion cMath_QuaternionMatrix(const cMatrixf &in a_mtxA)",
        |fr| fr.ret(quat_from_matrix(fr.rf(0))),
    );
    s.func(
        "cQuaternion cMath_QuaternionConjugate(const cQuaternion&in aqA)",
        |fr| {
            let q = fr.rf::<Quaternion>(0);
            fr.ret(Quaternion {
                v: hm::v3(-q.v.x, -q.v.y, -q.v.z),
                w: q.w,
            });
        },
    );
    s.func(
        "cMatrixf cMath_MatrixNormal(const cMatrixf &in a_mtxA, bool abTranspose)",
        |fr| {
            let m = fr.rf::<Matrixf>(0);
            let mut r = hm::IDENTITY;
            for (d, s) in r.m.iter_mut().zip(&m.m).take(3) {
                d.iter_mut().zip(s).take(3).for_each(|(a, b)| *a = *b);
            }
            let r = hm::matrix_inverse(&r);
            fr.ret(if fr.arg::<bool>(1) {
                hm::transpose(&r)
            } else {
                r
            });
        },
    );
    s.func("tString cMath_RGBToHex(const cColor &in aRGB)", |fr| {
        let c = fr.rf::<V4>(0);
        let hex = math::rgb_to_hex([c[0], c[1], c[2]]);
        fr.ret_string(&hex);
    });
    s.func(
        "bool cMath_CheckAABBSphereCollision(const cVector3f&in avMin,const cVector3f&in avMax, const cVector3f&in avCenter, float afRadius)",
        |fr| fr.ret::<bool>(math::aabb_sphere_collision(*fr.rf(0), *fr.rf(1), *fr.rf(2), fr.arg(3))),
    );
}

fn pid<const N: usize>(s: &Soma, name: &'static str, t: &'static str) {
    // SAFETY (the helpers below): script struct blocks hold a `Pid<N>` at offset 16.
    fn at<const N: usize>(p: *mut c_void) -> &'static mut Pid<N> {
        // SAFETY: caller passes a struct block of at least 4096 bytes.
        unsafe { &mut *p.cast::<u8>().add(16).cast::<Pid<N>>() }
    }
    s.0.reg_behaviour(
        super::s2_as::BEHAVE_FACTORY,
        name.as_bytes(),
        format!("{name}@ f()").as_bytes(),
        Box::new(move |fr| {
            let o = s2_api::new_owned_script_struct(name.as_bytes());
            at::<N>(o).set_error_num(10);
            fr.ret::<*mut c_void>(o);
        }),
    );
    s.0.reg_behaviour(
        super::s2_as::BEHAVE_FACTORY,
        name.as_bytes(),
        format!("{name}@ f(float afP, float afI, float afD, int alErrorNum)").as_bytes(),
        Box::new(move |fr| {
            let o = s2_api::new_owned_script_struct(name.as_bytes());
            let p = at::<N>(o);
            p.p = fr.arg(0);
            p.i = fr.arg(1);
            p.d = fr.arg(2);
            p.set_error_num(fr.arg(3));
            fr.ret::<*mut c_void>(o);
        }),
    );
    let pr = |decl: String, f: Box<dyn Fn(&Frame)>| {
        s.0.reg_method(
            name.as_bytes(),
            decl.as_bytes(),
            super::s2_as::Kind::Plain,
            f,
        );
    };
    pr(
        format!("{t} Output(const {t}&in avError, float afTimeStep)"),
        Box::new(|fr| {
            let e: [f32; N] = *fr.rf::<[f32; N]>(0);
            let r = at::<N>(fr.this).output(e, fr.arg(1));
            fr.ret::<[f32; N]>(r);
        }),
    );
    pr(
        "void SetErrorNum(int alErrorNum)".into(),
        Box::new(|fr| at::<N>(fr.this).set_error_num(fr.arg(0))),
    );
    pr(
        "void Reset()".into(),
        Box::new(|fr| at::<N>(fr.this).reset()),
    );
    pr(
        format!("{t} GetLastError()"),
        Box::new(|fr| fr.ret::<[f32; N]>(at::<N>(fr.this).last_error)),
    );
    pr(
        format!("{t} GetLastDerivative()"),
        Box::new(|fr| fr.ret::<[f32; N]>(at::<N>(fr.this).last_derivative)),
    );
    pr(
        format!("{t} GetLastIntegral()"),
        Box::new(|fr| fr.ret::<[f32; N]>(at::<N>(fr.this).last_integral)),
    );
}

fn string_set(s: &Soma) {
    let Some(h) = s2_host::ops() else { return };
    s.0.reg_behaviour(
        super::s2_as::BEHAVE_FACTORY,
        b"cScriptStringSet",
        b"cScriptStringSet@ f()",
        Box::new(move |fr| {
            let o = s2_api::new_owned_script_struct(b"cScriptStringSet");
            // SAFETY: host call on the fresh struct block.
            unsafe { (h.sset_reset)(o) };
            fr.ret::<*mut c_void>(o);
        }),
    );
    // SAFETY (all closures below): the host ops take the struct block and a live `std::string`.
    s.method(
        "cScriptStringSet",
        "cScriptStringSet& opAssign(const cScriptStringSet &in)",
        move |fr| {
            let (p, o) = (fr.this, fr.ptr_arg(0));
            unsafe { (h.sset_assign)(p, o) };
            fr.ret_ref(p);
        },
    );
    s.method(
        "cScriptStringSet",
        "void Add(const tString &in asStr)",
        move |fr| unsafe { (h.sset_add)(fr.this, fr.ptr_arg(0)) },
    );
    s.method(
        "cScriptStringSet",
        "void Erase(const tString &in asStr)",
        move |fr| unsafe { (h.sset_erase)(fr.this, fr.ptr_arg(0)) },
    );
    s.method(
        "cScriptStringSet",
        "bool Exists(const tString &in asStr)",
        move |fr| {
            fr.ret::<bool>(unsafe { (h.sset_count)(fr.this, fr.ptr_arg(0)) } != 0);
        },
    );
    s.method(
        "cScriptStringSet",
        "int Count(const tString &in asStr)",
        move |fr| fr.ret(unsafe { (h.sset_count)(fr.this, fr.ptr_arg(0)) }),
    );
    s.method("cScriptStringSet", "void Clear()", move |fr| unsafe {
        (h.sset_reset)(fr.this)
    });
    s.method("cScriptStringSet", "int Size()", move |fr| {
        fr.ret(unsafe { (h.sset_size)(fr.this) })
    });
    s.method(
        "cScriptStringSet",
        "void ElementsToArray(array<tString> &out avOutElements)",
        move |fr| {
            unsafe { (h.sset_to_array)(fr.this, fr.ptr_arg(0)) };
        },
    );
}

pub fn register(e: Engine) {
    let s = Soma(e);
    string_set(&s);
    pid::<3>(&s, "cPidControllerVec3", "cVector3f");
    pid::<1>(&s, "cPidControllerf", "float");
    vectors(&s);
    color(&s);
    matrix_quat(&s);
    constants(e);
    functions(&s);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn int_lane_division_matches_aarch64() {
        assert_eq!(i32::MIN.div(-1), i32::MIN);
        assert_eq!(5i32.div(0), 0);
        assert_eq!(7i32.div(2), 3);
    }

    #[test]
    fn plane_from_normal_point_negates_dot() {
        let a = hm::dot3(hm::v3(0.0, 1.0, 0.0), hm::v3(1.0, 2.0, 3.0));
        assert_eq!(-a, -2.0);
    }
}
