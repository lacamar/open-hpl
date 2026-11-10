//! Light probe of a translucent object: HPL3 `cRendererDeferred::CalculateSphericalHarmoincs`,
//! rows r,g,b over (normal.xyz, 1).
//!
//! Float expressions fuse multiply-adds as the gcc build does.

// NaN weights must skip a light exactly as `!(w > 0)` does in the C++.
#![allow(clippy::neg_cmp_op_on_partial_ord)]

use super::common::{fmax, fmin, pow};
use super::types::{Host, LightInfo, light_kind};
use crate::math::{clamp, fma, len3, sub3, v3};
use crate::types::{Matrixf, Vector2f, Vector3f};

fn smoothstep(min: f32, max: f32, x: f32) -> f32 {
    let t = clamp((x - min) / (max - min), 0.0, 1.0);
    t * t * fma(-2.0, t, 3.0)
}

fn volume(s: Vector3f) -> f32 {
    s.x * s.y * s.z
}

fn lerp(a: f32, b: f32, t: f32) -> f32 {
    fma(a, 1.0 - t, b * t)
}

fn aabb_intersection(min1: Vector3f, max1: Vector3f, min2: Vector3f, max2: Vector3f) -> bool {
    !(max1.x < min2.x
        || max1.y < min2.y
        || max1.z < min2.z
        || max2.x < min1.x
        || max2.y < min1.y
        || max2.z < min1.z)
}

fn vmax(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(fmax(a.x, b.x), fmax(a.y, b.y), fmax(a.z, b.z))
}

fn vmin(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(fmin(a.x, b.x), fmin(a.y, b.y), fmin(a.z, b.z))
}

fn add3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x + b.x, a.y + b.y, a.z + b.z)
}

fn div3v(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x / b.x, a.y / b.y, a.z / b.z)
}

fn axis(v: Vector3f, i: usize) -> f32 {
    match i {
        0 => v.x,
        1 => v.y,
        _ => v.z,
    }
}

type Acc = [[f32; 4]; 3];

fn add_box(
    acc: &mut Acc,
    total: &mut f32,
    bmin: Vector3f,
    bmax: Vector3f,
    inv_volume: f32,
    b: &LightInfo,
) {
    let (lmin, lmax) = (b.min, b.max);
    if !aabb_intersection(bmin, bmax, lmin, lmax) {
        return;
    }
    let imin = vmax(bmin, lmin);
    let imax = vmin(bmax, lmax);
    let w = fmin(0.5f32 * volume(sub3(lmax, lmin)), volume(sub3(imax, imin))) * inv_volume;
    if w <= 0.0 {
        return;
    }
    let d = div3v(sub3(add3(lmin, lmax), add3(imin, imax)), sub3(lmax, lmin));
    let d = v3(d.x.abs(), d.y.abs(), d.z.abs());
    let e = v3(
        fmax(0.0, 1.0 - d.x),
        fmax(0.0, 1.0 - d.y),
        fmax(0.0, 1.0 - d.z),
    );
    // HPL3 sums y twice, in its box light shader too
    let fbox = (1.0f32 / (1.0f32 / e.x + 1.0f32 / e.y + 1.0f32 / e.y)).sqrt();
    let cutoff = fmin(fmin(e.x, e.y), e.z);
    let sphere = smoothstep(lerp(3.0, 1.0, b.bevel).sqrt(), 0.0, len3(d));
    let falloff = smoothstep(0.0, 1.0, fbox);
    let f = falloff * falloff * smoothstep(0.0, 0.125, cutoff) * sphere * sphere;
    let w2 = w * b.weight * pow(f, b.falloff_pow + 1e-5f32);
    if !(w2 > 0.0) {
        return;
    }

    let col = [
        b.diffuse.r * b.diffuse.r,
        b.diffuse.g * b.diffuse.g,
        b.diffuse.b * b.diffuse.b,
    ];
    for (c, (row, col)) in acc.iter_mut().zip(col).enumerate() {
        let fc = col * b.brightness;
        let mut r = [0.0, 0.0, 0.0, fc * 0.2820948f32];
        if b.use_sh {
            for (k, slot) in r.iter_mut().enumerate() {
                let band = b
                    .bands
                    .get((k + 1) & 3)
                    .copied()
                    .unwrap_or(v3(0.0, 0.0, 0.0));
                *slot = fc * axis(band, c);
            }
        }
        for (a, r) in row.iter_mut().zip(r) {
            if b.blend_func == super::types::box_blend::REPLACE {
                *a = lerp(*a, r * w2 * w2, w2);
            } else if b.blend_func == super::types::box_blend::ADD {
                *a = fma(r * w2, *total, *a);
            } else {
                *a = fma(r * w2, f, *a);
            }
        }
    }
    if b.blend_func == super::types::box_blend::REPLACE {
        *total = lerp(*total, w2, w2);
    } else if b.blend_func != super::types::box_blend::ADD {
        *total = fma(f, b.weight, *total);
    }
}

fn add_light(
    acc: &mut Acc,
    host: &mut dyn Host,
    i: usize,
    bmin: Vector3f,
    bmax: Vector3f,
    inv_volume: f32,
    l: &LightInfo,
) {
    let (lmin, lmax) = (l.min, l.max);
    if !aabb_intersection(bmin, bmax, lmin, lmax) {
        return;
    }
    let imin = vmax(bmin, lmin);
    let imax = vmin(bmax, lmax);

    let radius = l.radius;
    let half = 0.5f32;
    let mid = v3(
        (imin.x + imax.x) * half,
        (imin.y + imax.y) * half,
        (imin.z + imax.z) * half,
    );
    let dir = sub3(l.pos, mid);
    let dist = len3(dir);
    let inv = fmax(dist, 1e-6f32);
    let dir = v3(dir.x / inv, dir.y / inv, dir.z / inv);
    let mut att = fmax(0.0, 1.0 - dist / radius);
    if l.falloff_pow != 0.5 {
        att = pow(att, 2.0f32 * l.falloff_pow);
    }
    if att == 0.0 {
        return;
    }
    // r^2, not the sphere volume: as in HPL3
    let mut w = fmin(
        0.5f32 * 4.1887903f32 * radius * radius,
        volume(sub3(imax, imin)),
    ) * inv_volume
        * att;

    if l.kind == light_kind::SPOT {
        let Some((rmin, rmax)) = host.spot_clip(i) else {
            return;
        };
        let area = (rmax.x - rmin.x) * (rmax.y - rmin.y);
        let rmin = v3(fmax(rmin.x, -1.0), fmax(rmin.y, -1.0), 0.0);
        let rmax = v3(fmin(rmax.x, 1.0), fmin(rmax.y, 1.0), 0.0);
        w *= (rmax.x - rmin.x) * (rmax.y - rmin.y) / area;
        let c = Vector2f {
            x: (rmin.x + rmax.x) * 0.5f32,
            y: (rmin.y + rmax.y) * 0.5f32,
        };
        let center = fma(c.x, c.x, c.y * c.y).sqrt();
        let near = l.near_clip;
        let t = (dist - near) / (radius - near);
        w *=
            clamp(t * 8.0, 0.0, 1.0) * clamp((1.0 - t) * 128.0, 0.0, 1.0) * fmax(0.0, 1.0 - center);
    }
    if !(w > 0.0) {
        return;
    }

    let col = [
        l.diffuse.r * l.diffuse.r,
        l.diffuse.g * l.diffuse.g,
        l.diffuse.b * l.diffuse.b,
    ];
    for (row, col) in acc.iter_mut().zip(col) {
        let fc = w * l.brightness * col;
        let [a0, a1, a2, a3] = row;
        *a0 = fma(dir.x * 0.0575824f32, fc, *a0);
        *a1 = fma(dir.y * 0.0575824f32, fc, *a1);
        *a2 = fma(dir.z * 0.0575824f32, fc, *a2);
        *a3 = fma(0.0705237f32, fc, *a3);
    }
}

/// The matrix `px_mtxLightProbe` gets, from the lights of the current render list.
pub fn calc_light_probe(host: &mut dyn Host) -> Matrixf {
    let bv = host.object_bv();
    let (bmin, bmax) = (bv.min, bv.max);
    let inv_volume = 1.0f32 / fmax(volume(sub3(bmax, bmin)), 1e-6f32);

    let lights: Vec<(usize, LightInfo)> = (0..host.light_count())
        .filter_map(|i| host.light(i, true).map(|l| (i, l)))
        .collect();
    let mut boxes: Vec<&LightInfo> = lights
        .iter()
        .map(|(_, l)| l)
        .filter(|l| l.kind == light_kind::BOX)
        .collect();
    boxes.sort_by_key(|b| b.blend_func);

    let mut acc: Acc = [[0.0; 4]; 3];
    let mut total = 1.5259022e-05f32;
    for b in boxes {
        add_box(&mut acc, &mut total, bmin, bmax, inv_volume, b);
    }
    for v in acc.iter_mut().flatten() {
        *v /= total;
    }

    for (i, l) in &lights {
        if l.kind != light_kind::BOX {
            add_light(&mut acc, host, *i, bmin, bmax, inv_volume, l);
        }
    }

    let mut m = Matrixf { m: [[0.0; 4]; 4] };
    for (row, a) in m.m.iter_mut().zip(acc) {
        *row = a;
    }
    m
}
