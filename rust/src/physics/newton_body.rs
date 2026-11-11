//! `cPhysicsBodyNewton` / `cCollideShapeNewton` float logic: HPL3 buoyancy, speed clamp and
//! buoyancy sphere packing. Float expressions mirror the gcc build (fused multiply-adds).

use crate::math::{
    Matrixf, Planef, cross3, div3, fma, len3, matrix_mul_3x3, matrix_mul_vec, mul3, plane_dist,
    sin, sub3, v3,
};
use crate::types::Vector3f;

unsafe extern "C" {
    fn ohpl_math_transform_plane(m: *const Matrixf, p: *const Planef, out: *mut Planef);
    fn ohpl_math_vector3_clamp_to_length(v: *mut Vector3f, max: f32);
}

fn transform_plane(m: &Matrixf, p: &Planef) -> Planef {
    let mut out = Planef::default();
    // SAFETY: all three pointers are valid for the call.
    unsafe { ohpl_math_transform_plane(m, p, &mut out) };
    out
}

fn clamp_to_length(v: &mut Vector3f, max: f32) {
    // SAFETY: `v` is a valid out-pointer.
    unsafe { ohpl_math_vector3_clamp_to_length(v, max) }
}

const PI: f32 = std::f32::consts::PI;
const PI2: f32 = std::f32::consts::FRAC_PI_2;
const ZERO: Vector3f = v3(0.0, 0.0, 0.0);
const SPHERE_VOLUME_FACTOR: f32 = 4.188_790_3;

fn add3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x + b.x, a.y + b.y, a.z + b.z)
}

/// `acc + a * s` with the product fused, as gcc emits for `acc += a * s`.
fn fma3(a: Vector3f, s: f32, acc: Vector3f) -> Vector3f {
    v3(fma(a.x, s, acc.x), fma(a.y, s, acc.y), fma(a.z, s, acc.z))
}

fn fmin(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

fn fmax(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

/// `(vel / sqrtf(|vel|)) * max` when `|vel| > max * max` (sic: the C++ compares the length, not
/// the squared length, against `max²`).
pub fn clamp_speed(vel: Vector3f, max: f32) -> Option<Vector3f> {
    if max > 0.0 {
        let l = len3(vel);
        if l > max * max {
            return Some(mul3(div3(vel, l.sqrt()), max));
        }
    }
    None
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct SubShape {
    pub points: *const Vector3f,
    pub count: usize,
    pub radius: f32,
    pub volume: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct BuoyIn {
    pub mass: f32,
    pub density: f32,
    pub density_mul: f32,
    pub drag_density_mul: f32,
    pub linear_viscosity: f32,
    pub angular_viscosity: f32,
    pub max_linear_speed: f32,
    pub max_angular_speed: f32,
    pub shape_volume: f32,
    pub time_step: f32,
    pub no_prev_vel: bool,
    pub surface: Planef,
    pub matrix: Matrixf,
    /// `cMath::MatrixInverse(matrix)`, still computed by C++.
    pub inverse: Matrixf,
    pub inertia: Matrixf,
    pub mass_centre: Vector3f,
    pub gravity: Vector3f,
    pub linear_velocity: Vector3f,
    pub angular_velocity: Vector3f,
    pub prev_linear_vel: Vector3f,
    pub prev_angular_vel: Vector3f,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct BuoyOut {
    pub force: Vector3f,
    pub torque: Vector3f,
    pub prev_linear_vel: Vector3f,
    pub prev_angular_vel: Vector3f,
    pub drag_density_mul: f32,
    pub no_prev_vel: bool,
    /// False: the drag state was not touched (no drag density).
    pub drag_updated: bool,
}

fn sphere_volume(d: f32, r: f32, point_vol: f32) -> f32 {
    if d < -r {
        point_vol
    } else if d >= 0.0 {
        (2.0 * r + d) * (r - d) * (r - d) * (PI / 3.0)
    } else {
        fma(d * d * (PI / 3.0), fma(3.0, r, d), 0.5 * point_vol)
    }
}

/// Lift from spheres packed in the sub shapes, then drag. `None` when the body gets no force.
pub fn hpl3_buoyancy(b: &BuoyIn, subs: &[SubShape]) -> Option<BuoyOut> {
    let density = b.density * b.density_mul;
    if b.mass == 0.0 || density <= 0.0 {
        return None;
    }
    let surface = transform_plane(&b.inverse, &b.surface);
    let normal = v3(surface.a, surface.b, surface.c);
    let com = b.mass_centre;
    let g_len = len3(b.gravity);
    let mut force = ZERO;
    let mut torque = ZERO;
    let mut volume = 0.0f32;
    for sub in subs {
        if sub.count == 0 || sub.points.is_null() {
            continue;
        }
        // SAFETY: the caller passes `count` valid points per sub shape.
        let points = unsafe { std::slice::from_raw_parts(sub.points, sub.count) };
        let r = sub.radius;
        let point_vol = SPHERE_VOLUME_FACTOR * r * r * r;
        let lift = mul3(normal, density * g_len * sub.volume);
        let point_lift = div3(lift, sub.count as f32);
        let mut sum = 0.0f32;
        for p in points {
            let d = plane_dist(&surface, *p);
            if d > r {
                continue;
            }
            let v = sphere_volume(d, r, point_vol);
            sum += v;
            torque = add3(
                torque,
                cross3(sub3(*p, com), mul3(point_lift, v / point_vol)),
            );
        }
        let frac = sum / (point_vol * sub.count as f32);
        force = fma3(lift, frac, force);
        volume = fma(frac, sub.volume, volume);
    }
    if volume <= 0.0 {
        return None;
    }
    force = matrix_mul_3x3(&b.matrix, force);
    torque = matrix_mul_3x3(&b.matrix, torque);

    let mut out = BuoyOut {
        prev_linear_vel: b.prev_linear_vel,
        prev_angular_vel: b.prev_angular_vel,
        drag_density_mul: b.drag_density_mul,
        no_prev_vel: b.no_prev_vel,
        ..BuoyOut::default()
    };
    let mul = fmin(b.drag_density_mul, b.density_mul);
    let drag = b.density * mul;
    if drag > 0.0 {
        let frac = volume / b.shape_volume;
        let lin = b.linear_velocity;
        let ang = b.angular_velocity;
        let mut vl = ZERO;
        let mut va = ZERO;
        let speed = len3(lin);
        if speed > 0.0 {
            let mut c = if b.max_linear_speed > 0.0 {
                fmin(speed, b.max_linear_speed * mul)
            } else {
                mul * speed
            };
            c = fmin(c, 0.5 * c * c);
            vl = mul3(lin, -c * b.linear_viscosity * drag * frac * b.mass / speed);
        }
        let ang_speed = len3(ang);
        if ang_speed > 0.0 {
            let mut c = if b.max_angular_speed > 0.0 {
                fmin(ang_speed, b.max_angular_speed * mul)
            } else {
                mul * ang_speed
            };
            c = fmin(c, 0.5 * c * c);
            va = mul3(
                matrix_mul_vec(&b.inertia, div3(ang, ang_speed)),
                -drag * c * b.angular_viscosity * frac,
            );
        }
        let mut vw = mul3(lin, -2.0 * mul);
        let mut vwa = mul3(ang, -0.5 * mul);
        if !b.no_prev_vel {
            vw = fma3(sub3(lin, b.prev_linear_vel), -mul * 0.1 / b.time_step, vw);
            vwa = fma3(
                sub3(ang, b.prev_angular_vel),
                -mul * 0.025 / b.time_step,
                vwa,
            );
        }
        clamp_to_length(&mut vw, 50.0);
        clamp_to_length(&mut vwa, 50.0);
        vl = fma3(vw, b.mass * b.linear_viscosity * mul, vl);
        va = fma3(
            matrix_mul_vec(&b.inertia, vwa),
            b.angular_viscosity * mul,
            va,
        );
        force = add3(force, v3(vl.x * 0.25, vl.y, vl.z * 0.25));
        torque = fma3(va, 0.05125, torque);
        out.prev_linear_vel = lin;
        out.prev_angular_vel = ang;
        out.no_prev_vel = false;
        out.drag_density_mul = fma(
            b.drag_density_mul,
            1.0 - b.time_step,
            b.density_mul * b.time_step,
        );
        out.drag_updated = true;
    }
    out.force = force;
    out.torque = torque;
    Some(out)
}

pub const SHAPE_BOX: i32 = 1;
pub const SHAPE_SPHERE: i32 = 2;
pub const SHAPE_CYLINDER: i32 = 3;
pub const SHAPE_CAPSULE: i32 = 4;

/// Sphere packing for a primitive shape in shape space (the caller applies the offset, over its
/// whole point list as the C++ does); returns the radius.
pub fn buoyancy_points(shape: i32, size: Vector3f, out: &mut Vec<Vector3f>) -> f32 {
    let mut radius = 0.0f32;
    if shape == SHAPE_BOX {
        let r = 0.5 * fmin(size.x, fmin(size.y, size.z));
        radius = r;
        let sz = [size.x, size.y, size.z];
        let mut num = [0i32; 3];
        let mut step = [0.0f32; 3];
        for ((n, s), len) in num.iter_mut().zip(step.iter_mut()).zip(sz) {
            *n = ((len / (2.0 * r) + 0.999) as i32).min(8);
            *s = (len - 2.0 * r) / (*n as f32 - 1.0);
        }
        let mut z = -0.5 * size.z + r;
        for _ in 0..num[2] {
            let mut y = -0.5 * size.y + r;
            for _ in 0..num[1] {
                let mut x = -0.5 * size.x + r;
                for _ in 0..num[0] {
                    out.push(v3(x, y, z));
                    x += step[0];
                }
                y += step[1];
            }
            z += step[2];
        }
    } else if shape == SHAPE_SPHERE {
        radius = size.x;
        out.push(ZERO);
    } else if shape == SHAPE_CAPSULE || (shape == SHAPE_CYLINDER && 2.0 * size.x <= size.y) {
        radius = size.x;
        let (r, len) = (size.x, size.y);
        let num = ((len / (2.0 * r) + 0.99) as i32).min(10);
        let step = (len - 2.0 * r) / (num as f32 - 1.0);
        let mut x = -0.5 * len + r;
        for _ in 0..num {
            out.push(v3(x, 0.0, 0.0));
            x += step;
        }
    } else if shape == SHAPE_CYLINDER {
        let (r, h) = (size.x, size.y);
        let pr = 0.5 * h;
        radius = pr;
        let rows = ((2.0 * r / h + 0.999) as i32).clamp(5, 8) | 1;
        let row_step = (2.0 * r - h) / (rows as f32 - 1.0);
        let mut z = pr - r;
        for _ in 0..rows {
            let w = -fma(
                sin(z / r * PI2).abs(),
                pr,
                -fmax(fma(-z, z, r * r), 0.0).sqrt(),
            );
            let cols = ((2.0 * w / h + 0.999) as i32).clamp(5, 8);
            let step = (2.0 * w - h) / (cols as f32 - 1.0);
            let mut y = pr - w;
            for _ in 0..cols {
                out.push(v3(0.0, y, z));
                y += step;
            }
            z += row_step;
        }
    }
    radius
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sphere_gets_one_point() {
        let mut pts = Vec::new();
        let r = buoyancy_points(SHAPE_SPHERE, v3(0.5, 0.0, 0.0), &mut pts);
        assert_eq!((r, pts.len()), (0.5, 1));
    }

    #[test]
    fn unit_box_packs_one_point() {
        let mut pts = Vec::new();
        buoyancy_points(SHAPE_BOX, v3(1.0, 1.0, 1.0), &mut pts);
        assert_eq!(pts.len(), 1);
    }

    #[test]
    fn speed_clamp_compares_length_with_square() {
        assert!(clamp_speed(v3(1.0, 0.0, 0.0), 2.0).is_none());
        assert!(clamp_speed(v3(5.0, 0.0, 0.0), 2.0).is_some());
        assert!(clamp_speed(v3(5.0, 0.0, 0.0), 0.0).is_none());
    }

    #[test]
    fn massless_body_gets_no_buoyancy() {
        let b = BuoyIn::default();
        assert!(hpl3_buoyancy(&b, &[]).is_none());
    }
}
