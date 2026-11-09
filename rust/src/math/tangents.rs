//! Per-vertex tangent generation.

use super::{Vector3f, cross3, div3, dot3, fma, normalize3, sub3, v3};

/// `Max(float, float)` as `cMath` defines it: the second operand wins on NaN.
fn fmax(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

/// # Safety
/// `dest` has `vertex_count * 4` writable floats; `indices` has `index_count` entries; `vertices`,
/// `tex` (stride 3) and `normals` (stride 3) cover every referenced index and `vertex_count`.
pub(crate) unsafe fn create_tri_tangent_vectors(
    dest: *mut f32,
    indices: *const u32,
    index_count: i32,
    vertices: *const f32,
    stride: i32,
    tex: *const f32,
    normals: *const f32,
    vertex_count: i32,
) {
    // SAFETY (all raw reads below): caller contract; offsets use the same int math as the C++.
    let at = |base: *const f32, idx: i32, stride: i32| -> Vector3f {
        unsafe {
            let p = base.offset(idx.wrapping_mul(stride) as isize);
            v3(*p, *p.add(1), *p.add(2))
        }
    };
    let equal = |base: *const f32, a: i32, b: i32, stride: i32| -> bool {
        unsafe {
            let pa = base.offset(a.wrapping_mul(stride) as isize);
            let pb = base.offset(b.wrapping_mul(stride) as isize);
            *pa == *pb && *pa.add(1) == *pb.add(1) && *pa.add(2) == *pb.add(2)
        }
    };

    let n = vertex_count.max(0) as usize;
    let mut t1 = vec![Vector3f::default(); n];
    let mut t2 = vec![Vector3f::default(); n];
    let fma_add = |v: &mut Vector3f, d: Vector3f, r: f32| {
        v.x = fma(d.x, r, v.x);
        v.y = fma(d.y, r, v.y);
        v.z = fma(d.z, r, v.z);
    };
    let add = |v: &mut Vector3f, d: Vector3f| {
        v.x += d.x;
        v.y += d.y;
        v.z += d.z;
    };

    let mut tri = 0i32;
    while tri < index_count {
        // SAFETY: caller contract.
        let (i1, i2, i3) = unsafe {
            (
                *indices.offset(tri as isize) as i32,
                *indices.offset(tri as isize + 1) as i32,
                *indices.offset(tri as isize + 2) as i32,
            )
        };
        tri = tri.wrapping_add(3);
        let p1 = at(vertices, i1, stride);
        let p12 = sub3(at(vertices, i2, stride), p1);
        let p13 = sub3(at(vertices, i3, stride), p1);
        let x1 = at(tex, i1, 3);
        let x12 = sub3(at(tex, i2, 3), x1);
        let x13 = sub3(at(tex, i3, 3), x1);

        let det = fma(x12.x, x13.y, -(x12.y * x13.x));
        if det.abs() < 1e-12f32 {
            continue;
        }
        let r = 1.0f32 / det;
        // gcc vectorises x/y (second product fused) and leaves z scalar (first fused), and fuses
        // `* r` into each accumulate.
        let s_dir = v3(
            fma(-x12.y, p13.x, x13.y * p12.x),
            fma(-x12.y, p13.y, x13.y * p12.y),
            fma(x13.y, p12.z, -(x12.y * p13.z)),
        );
        let t_dir = v3(
            fma(-x13.x, p12.x, x12.x * p13.x),
            fma(-x13.x, p12.y, x12.x * p13.y),
            fma(x12.x, p13.z, -(x13.x * p12.z)),
        );
        for i in [i1, i2, i3] {
            fma_add(&mut t1[i as usize], s_dir, r);
        }
        for i in [i1, i2, i3] {
            fma_add(&mut t2[i as usize], t_dir, r);
        }
    }

    let max_cos = -1.0f32;
    for i in 0..vertex_count {
        for j in (i + 1)..vertex_count {
            if equal(vertices, i, j, stride) && equal(normals, i, j, 3) {
                let (a1, a2) = (t1[i as usize], t2[i as usize]);
                let (b1, b2) = (t1[j as usize], t2[j as usize]);
                if dot3(a1, b1) >= max_cos {
                    add(&mut t1[j as usize], a1);
                    add(&mut t1[i as usize], b1);
                }
                if dot3(a2, b2) >= max_cos {
                    add(&mut t2[j as usize], a2);
                    add(&mut t2[i as usize], b2);
                }
            }
        }
    }

    for i in 0..vertex_count {
        let normal = at(normals, i, 3);
        let norm = |v: Vector3f| div3(v, fmax(1.0, fmax(v.x.abs(), fmax(v.y.abs(), v.z.abs()))));
        let tan1 = norm(t1[i as usize]);
        let tan2 = norm(t2[i as usize]);
        let d = dot3(normal, tan1);
        let mut tan = v3(
            fma(-normal.x, d, tan1.x),
            fma(-normal.y, d, tan1.y),
            fma(-normal.z, d, tan1.z),
        );
        if !(normalize3(&mut tan) >= 1e-6f32) {
            let axis = if normal.x.abs() < 0.9f32 {
                v3(1.0, 0.0, 0.0)
            } else {
                v3(0.0, 1.0, 0.0)
            };
            tan = cross3(normal, axis);
            normalize3(&mut tan);
        }
        let w = if dot3(cross3(normal, tan1), tan2) < 0.0 {
            -1.0
        } else {
            1.0
        };
        // SAFETY: caller contract.
        unsafe {
            let p = dest.offset(i.wrapping_mul(4) as isize);
            *p = tan.x;
            *p.add(1) = tan.y;
            *p.add(2) = tan.z;
            *p.add(3) = w;
        }
    }
}
