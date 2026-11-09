//! Golden values captured from the original gcc/aarch64 C++ build, compared bit-for-bit.

use super::geom::*;
use super::matrix::*;
use super::scalar::*;
use super::tangents::create_tri_tangent_vectors;
use std::f32::consts::TAU;

use super::{Matrixf, Quaternion, Rect2l, Vector3f, v3};

fn bits(m: &Matrixf) -> [u32; 16] {
    let mut out = [0u32; 16];
    for (o, v) in out.iter_mut().zip(m.m.iter().flatten()) {
        *o = v.to_bits();
    }
    out
}

fn v3_bits(v: Vector3f) -> [u32; 3] {
    [v.x.to_bits(), v.y.to_bits(), v.z.to_bits()]
}

fn mat(v: [f32; 16]) -> Matrixf {
    Matrixf {
        m: [
            [v[0], v[1], v[2], v[3]],
            [v[4], v[5], v[6], v[7]],
            [v[8], v[9], v[10], v[11]],
            [v[12], v[13], v[14], v[15]],
        ],
    }
}

fn sample() -> (Matrixf, Matrixf) {
    let a = mat([
        0.3, -1.7, 2.9, 0.5, 1.1, 0.6, -0.4, 7.0, 2.2, 0.9, 1.3, -3.1, 0.0, 0.0, 0.0, 1.0,
    ]);
    (a, matrix_rotate(v3(0.4, -1.3, 2.1), 2))
}

#[test]
fn matrix_ops_match_cpp() {
    let (a, r) = sample();
    assert_eq!(
        bits(&matrix_inverse(&a)),
        [
            0x3eb03b5e, 0x3fba47bc, 0xbea3dd61, 0xc1359f8b, 0xbf328cfd, 0xbfe77f64, 0x3f7fd86f,
            0x4180dae8, 0xbdcc0eda, 0xbf9af9d3, 0x3f1e7442, 0x412719fb, 0x00000000, 0x00000000,
            0x80000000, 0x3f800000
        ]
    );
    assert_eq!(
        bits(&matrix_mul(&a, &r)),
        [
            0x3ff5397f, 0x3fd73443, 0x400d9161, 0x3f000000, 0x3dd6ed3b, 0xbfa7986c, 0x3d8b2b69,
            0x40e00000, 0x3ff9489f, 0xbfd4a8cd, 0x3f631668, 0xc0466666, 0, 0, 0, 0x3f800000
        ]
    );
    assert_eq!(
        bits(&r),
        [
            0x3e4162eb, 0xbf4b899c, 0x3f138cdd, 0, 0x3ed736be, 0xbeee13b1, 0xbf47773e, 0,
            0x3f6332ee, 0x3ec761d7, 0x3e7c4bc5, 0, 0, 0, 0, 0x3f800000
        ]
    );
    assert_eq!(
        bits(&perspective_projection(0.05, 100.0, 1.2, 1.7, false)),
        [
            0x3f5c1d3b, 0, 0, 0, 0, 0x3fbb18d9, 0, 0, 0, 0, 0xbf8020c9, 0xbdcce707, 0, 0,
            0xbf800000, 0
        ]
    );
    assert_eq!(
        v3_bits(matrix_mul_vec(&a, v3(1.5, -2.5, 0.7))),
        [0x40e75c29, 0x40dbd70a, 0xbf91eb84]
    );
    assert_eq!(
        v3_bits(matrix_euler_angle_distance(&a, &r)),
        [0x3f820479, 0xbf8239f5, 0xbed8764d]
    );
}

#[test]
fn quaternion_slerp_matches_cpp() {
    let (_, r) = sample();
    let q0 = matrix_to_quaternion(&matrix_rotate(v3(0.2, 0.3, 0.4), 0));
    let q1 = matrix_to_quaternion(&r);
    let s: Quaternion = quat_slerp(0.37, &q0, &q1, true);
    assert_eq!(
        [
            s.v.x.to_bits(),
            s.v.y.to_bits(),
            s.v.z.to_bits(),
            s.w.to_bits()
        ],
        [0x3e975943, 0x3d42d4c0, 0x3ec48e46, 0x3f5f9ccf]
    );
}

#[test]
#[allow(clippy::approx_constant)]
fn scalar_ops_match_cpp() {
    assert_eq!(wrap(-7.3, 0.0, TAU).to_bits(), 0x40a8861c);
    assert_eq!(round_float_to_decimals(3.14159, 2).to_bits(), 0x4048f5c2);
    assert_eq!(angle_distance(0.2, 6.0, TAU).to_bits(), 0xbef76410);
    assert_eq!(get_flag_bit(12), 1 << 12);
    assert!(is_pow2(4096) && !is_pow2(3));
    assert_eq!(log2_to_int(1024), 10);
}

#[test]
fn sphere_line_matches_cpp() {
    let (hit, r1, r2) = sphere_line(
        v3(0.5, 0.2, 0.1),
        1.3,
        v3(-3.0, 0.1, 0.0),
        v3(4.0, 0.3, 0.2),
    );
    let (t1, p1) = r1.unwrap();
    let (t2, p2) = r2.unwrap();
    assert!(hit);
    assert_eq!((t1.to_bits(), t2.to_bits()), (0x4099b0fe, 0x400cfb99));
    assert_eq!(v3_bits(p1.unwrap()), [0x3fe643a7, 0x3e72cda4, 0x3e0c673c]);
    assert_eq!(v3_bits(p2.unwrap()), [0xbf4c8751, 0x3e26cbf7, 0x3d80cb21]);
}

#[test]
fn aabb_line_matches_cpp() {
    let mut pos = None;
    let (hit, t) = aabb_line_intersection(
        v3(-1.0, -1.0, -1.0),
        v3(1.0, 1.0, 1.0),
        v3(-3.0, 0.2, 0.1),
        v3(3.0, -0.1, 0.3),
        &mut pos,
    );
    assert!(hit);
    assert_eq!(t.to_bits(), 0x3eaaaaab);
    assert_eq!(v3_bits(pos.unwrap()), [0xbf7fffff, 0x3dcccccc, 0x3e2aaaab]);
}

#[test]
fn orthonormalize_matches_cpp() {
    let [d1, d2, d3] =
        orthonormalize_basis(v3(1.0, 0.2, 0.0), v3(0.3, 1.0, 0.1), v3(0.1, 0.2, 1.0));
    assert_eq!(v3_bits(d1), [0x3f7b0756, 0x3e48d2ab, 0]);
    assert_eq!(v3_bits(d2), [0xbe47a6c3, 0x3f799073, 0x3ddce413]);
    assert_eq!(v3_bits(d3), [0x3cad47f3, 0xbdd899f2, 0x3f7e81b1]);
}

#[test]
fn tangents_match_cpp() {
    let vtx = [
        0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.2, 0.0, 1.0, 0.0f32,
    ];
    let tex = [
        0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0f32,
    ];
    let nor = [
        0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0f32,
    ];
    let idx = [0u32, 1, 2, 0, 2, 3];
    let mut dst = [0.0f32; 16];
    // SAFETY: arrays cover 4 vertices and 6 indices.
    unsafe {
        create_tri_tangent_vectors(
            dst.as_mut_ptr(),
            idx.as_ptr(),
            6,
            vtx.as_ptr(),
            3,
            tex.as_ptr(),
            nor.as_ptr(),
            4,
        );
    }
    for v in dst.chunks(4) {
        assert_eq!(
            v.iter().map(|f| f.to_bits()).collect::<Vec<_>>(),
            [0x3f800000, 0, 0, 0x3f800000]
        );
    }
}

#[test]
fn tangents_skip_out_of_range_triangles() {
    let vtx = [0.0f32, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0];
    let tex = [0.0f32, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0];
    let nor = [0.0f32, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0];
    for idx in [[0u32, 1, 3], [0, 1, u32::MAX], [0, 1, 0x8000_0000]] {
        let mut dst = [7.0f32; 12];
        // SAFETY: arrays cover 3 vertices; bad indices are skipped before any read.
        unsafe {
            create_tri_tangent_vectors(
                dst.as_mut_ptr(),
                idx.as_ptr(),
                3,
                vtx.as_ptr(),
                3,
                tex.as_ptr(),
                nor.as_ptr(),
                3,
            );
        }
        assert!(dst.iter().all(|f| f.is_finite()), "{idx:?}");
    }
}

#[test]
fn rect_fit_wraps_like_cpp() {
    let a = Rect2l {
        x: 0,
        y: 0,
        w: 4,
        h: 4,
    };
    let b = Rect2l {
        x: i32::MAX - 1,
        y: 0,
        w: 8,
        h: 4,
    };
    assert!(!rect_fit_l(&a, &b));
}

#[test]
fn constants_equal_the_c_literals() {
    use super::{PI, PI2, TWO_PI};
    assert_eq!(
        [PI.to_bits(), PI2.to_bits(), TWO_PI.to_bits()],
        [0x40490fdb, 0x3fc90fdb, 0x40c90fdb]
    );
}

#[test]
fn rand_rectl_empty_range_matches_aarch64_c() {
    // Values from g++ -O2 on aarch64: rand()%0 yields rand().
    super::libm::c_srand(7);
    let r = super::libm::c_rand();
    super::libm::c_srand(7);
    assert_eq!(super::scalar::rand_rectl(5, 4), r.wrapping_add(5));
}
