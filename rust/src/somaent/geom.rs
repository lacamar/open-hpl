//! Oriented boxes of entity bodies and areas, and the attachment matrix maths.

use crate::math::{
    cross3, dot3, fma, len3, matrix_inverse, matrix_mul, matrix_mul_vec, matrix_unit_vectors, sin,
    v3,
};
use crate::types::{Matrixf, Vector3f};

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Obb {
    pub center: Vector3f,
    pub axis: [Vector3f; 3],
    pub half: Vector3f,
}

const _: () = assert!(std::mem::size_of::<Obb>() == 60);

pub(crate) fn get(v: Vector3f, i: usize) -> f32 {
    [v.x, v.y, v.z].get(i).copied().unwrap_or(0.0)
}

pub(crate) fn set(v: &mut Vector3f, i: usize, x: f32) {
    match i {
        0 => v.x = x,
        1 => v.y = x,
        _ => v.z = x,
    }
}

fn col(m: &Matrixf, i: usize) -> Vector3f {
    let c = |r: usize| {
        m.m.get(r)
            .and_then(|row| row.get(i))
            .copied()
            .unwrap_or(0.0)
    };
    v3(c(0), c(1), c(2))
}

pub(crate) fn translation(m: &Matrixf) -> Vector3f {
    col(m, 3)
}

pub(crate) fn set_translation(m: &mut Matrixf, t: Vector3f) {
    m.m[0][3] = t.x;
    m.m[1][3] = t.y;
    m.m[2][3] = t.z;
}

pub(crate) fn rotation(m: &Matrixf) -> Matrixf {
    let mut r = *m;
    r.m[0][3] = 0.0;
    r.m[1][3] = 0.0;
    r.m[2][3] = 0.0;
    r.m[3] = [0.0, 0.0, 0.0, 1.0];
    r
}

fn mid(min: Vector3f, max: Vector3f) -> Vector3f {
    v3(
        (min.x + max.x) * 0.5,
        (min.y + max.y) * 0.5,
        (min.z + max.z) * 0.5,
    )
}

pub fn aabb_to_obb(min: Vector3f, max: Vector3f) -> Obb {
    Obb {
        center: mid(min, max),
        half: v3(
            (max.x - min.x) * 0.5,
            (max.y - min.y) * 0.5,
            (max.z - min.z) * 0.5,
        ),
        axis: [v3(1.0, 0.0, 0.0), v3(0.0, 1.0, 0.0), v3(0.0, 0.0, 1.0)],
    }
}

pub fn matrix_obb(m: &Matrixf, min: Vector3f, max: Vector3f) -> Obb {
    let mut b = Obb {
        center: matrix_mul_vec(m, mid(min, max)),
        ..Obb::default()
    };
    for i in 0..3 {
        let c = col(m, i);
        let len = len3(c);
        let axis = if len > 0.0 {
            v3(c.x / len, c.y / len, c.z / len)
        } else {
            v3(
                f32::from(u8::from(i == 0)),
                f32::from(u8::from(i == 1)),
                f32::from(u8::from(i == 2)),
            )
        };
        if let Some(slot) = b.axis.get_mut(i) {
            *slot = axis;
        }
        set(
            &mut b.half,
            i,
            (get(max, i) - get(min, i)) * 0.5 * if len > 0.0 { len } else { 1.0 },
        );
    }
    b
}

#[expect(
    clippy::neg_cmp_op_on_partial_ord,
    reason = "a NaN distance keeps the point inside, as the C++ `> half` early-out"
)]
pub fn point_in_obb(b: &Obb, p: Vector3f) -> bool {
    let d = v3(p.x - b.center.x, p.y - b.center.y, p.z - b.center.z);
    b.axis
        .iter()
        .enumerate()
        .all(|(i, a)| !(dot3(d, *a).abs() > get(b.half, i)))
}

pub fn obb_overlap(a: &Obb, b: &Obb) -> bool {
    let t = v3(
        b.center.x - a.center.x,
        b.center.y - a.center.y,
        b.center.z - a.center.z,
    );
    let separated = |axis: Vector3f| {
        if dot3(axis, axis) < 1e-8f32 {
            return false;
        }
        let (mut ra, mut rb) = (0.0f32, 0.0f32);
        for i in 0..3 {
            if let (Some(aa), Some(ba)) = (a.axis.get(i), b.axis.get(i)) {
                ra = fma(get(a.half, i), dot3(*aa, axis).abs(), ra);
                rb = fma(get(b.half, i), dot3(*ba, axis).abs(), rb);
            }
        }
        dot3(t, axis).abs() > ra + rb
    };
    if a.axis.iter().chain(b.axis.iter()).any(|&ax| separated(ax)) {
        return false;
    }
    !a.axis
        .iter()
        .any(|&x| b.axis.iter().any(|&y| separated(cross3(x, y))))
}

pub fn ray_hits_obb(b: &Obb, start: Vector3f, dir: Vector3f, max_dist: f32) -> Option<f32> {
    let rel = v3(
        start.x - b.center.x,
        start.y - b.center.y,
        start.z - b.center.z,
    );
    let (mut t_min, mut t_max) = (0.0f32, max_dist);
    for (i, ax) in b.axis.iter().enumerate() {
        let o = dot3(rel, *ax);
        let d = dot3(dir, *ax);
        let half = get(b.half, i);
        if d.abs() < 1e-6f32 {
            if o.abs() > half {
                return None;
            }
            continue;
        }
        let (mut t1, mut t2) = ((-half - o) / d, (half - o) / d);
        if t1 > t2 {
            std::mem::swap(&mut t1, &mut t2);
        }
        if t_min < t1 {
            t_min = t1;
        }
        if t2 < t_max {
            t_max = t2;
        }
        if t_min > t_max {
            return None;
        }
    }
    Some(t_min)
}

/// `CreateAreaBody`: the body matrix with unit columns, and the size scaled by the column lengths.
pub fn area_body(m: &Matrixf, size: Vector3f) -> (Matrixf, Vector3f) {
    let mut size = size;
    let mut cols = [col(m, 0), col(m, 1), col(m, 2)];
    for (i, c) in cols.iter_mut().enumerate() {
        let len = len3(*c);
        if len > 0.0 {
            *c = v3(c.x / len, c.y / len, c.z / len);
            let v = get(size, i) * len;
            set(&mut size, i, v);
        }
    }
    let [c0, c1, c2] = cols;
    (matrix_unit_vectors(c0, c1, c2, translation(m)), size)
}

pub fn relative_matrix(base: &Matrixf, m: &Matrixf) -> Matrixf {
    let mut mtx = *m;
    let (t, bt) = (translation(m), translation(base));
    set_translation(&mut mtx, v3(t.x - bt.x, t.y - bt.y, t.z - bt.z));
    matrix_mul(&matrix_inverse(&rotation(base)), &mtx)
}

/// `UpdateAttachment` once the parent matrix is known: the matrix to set, if any.
pub fn attachment_step(
    parent: &Matrixf,
    prev: &Matrixf,
    offset: &Matrixf,
    cur: &Matrixf,
    use_rotation: bool,
    locked: bool,
) -> Option<Matrixf> {
    if use_rotation {
        if parent == prev {
            return None;
        }
        return Some(if locked {
            matrix_mul(parent, offset)
        } else {
            matrix_mul(parent, &relative_matrix(prev, cur))
        });
    }
    let (pt, qt) = (translation(parent), translation(prev));
    let delta = v3(pt.x - qt.x, pt.y - qt.y, pt.z - qt.z);
    if delta == v3(0.0, 0.0, 0.0) {
        return None;
    }
    let mut mtx = *cur;
    let t = translation(&mtx);
    set_translation(
        &mut mtx,
        if locked {
            translation(&matrix_mul(parent, offset))
        } else {
            v3(t.x + delta.x, t.y + delta.y, t.z + delta.z)
        },
    );
    Some(mtx)
}

/// Half-extents of the box plus a margin: the liquid area's body query volume.
pub fn liquid_extent(b: &Obb) -> Vector3f {
    let mut ext = v3(0.2, 0.2, 0.2);
    for (i, ax) in b.axis.iter().enumerate() {
        for k in 0..3 {
            let v = fma(get(*ax, k).abs(), get(b.half, i), get(ext, k));
            set(&mut ext, k, v);
        }
    }
    ext
}

pub fn liquid_wave_height(surface_y: f32, amp: f32, freq: f32, time: f32, cx: f32, cz: f32) -> f32 {
    let sum = sin(fma(time, freq, 15.0 * cx)) + sin(fma(time, freq, 15.0 * cz));
    fma(0.3f32 * amp, sum, surface_y)
}

pub fn liquid_body_under(bv_max_y: f32, center_y: f32, ext_y: f32) -> bool {
    bv_max_y <= center_y + ext_y - 0.2f32
}

pub fn liquid_surface(b: &Obb) -> f32 {
    b.center.y + b.half.y
}
