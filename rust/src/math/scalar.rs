//! Random numbers, float helpers, angles and 2D vectors.

use super::libm::{atan, c_rand, c_srand, c_time, cos, ln, pow_f64, sin};
use super::{PI, PI2, TWO_PI, Vector2f, fma};

pub(crate) fn rand_rectl(min: i32, max: i32) -> i32 {
    aarch64_rem(c_rand(), max.wrapping_sub(min).wrapping_add(1)).wrapping_add(min)
}

/// C `%` as gcc emits it on aarch64 (sdiv + msub): a zero divisor yields the dividend.
fn aarch64_rem(a: i32, b: i32) -> i32 {
    if b == 0 { a } else { a.wrapping_rem(b) }
}

pub(crate) fn rand_rectf(min: f32, max: f32) -> f32 {
    let r = c_rand() as f32 / i32::MAX as f32;
    fma(r, max - min, min)
}

pub(crate) fn fast_random_float(seed: i32) -> f32 {
    let s = seed as u32;
    let x = (s << 13) ^ s;
    let v = x
        .wrapping_mul(x.wrapping_mul(x).wrapping_mul(15731).wrapping_add(789221))
        .wrapping_add(1376312589)
        & 0x7fff_ffff;
    1.0f32 - v as f32 / 1073741824.0f32
}

pub(crate) fn randomize(seed: i32) {
    if seed == -1 {
        c_srand(c_time() as u32);
    } else {
        c_srand(seed as u32);
    }
}

pub(crate) fn get_flag_bit(bit: i32) -> i32 {
    1i32.wrapping_shl(bit as u32)
}

pub(crate) fn round_float_to_decimals(val: f32, precision: i32) -> f32 {
    let pw = pow_f64(10.0, f64::from(precision)) as f32;
    let inv = 1.0f32 / pw;
    let add = 0.5f32 * inv;
    let v = val + add;
    (pw * v).floor() * inv
}

pub(crate) fn get_fraction(val: f32) -> f32 {
    val - val.floor()
}

pub(crate) fn modulus(dividend: f32, divisor: f32) -> f32 {
    let num = (dividend / divisor).abs().floor();
    fma(-divisor.abs(), num, dividend.abs())
}

pub(crate) fn wrap(x: f32, min: f32, max: f32) -> f32 {
    if x >= min && x <= max {
        return x;
    }
    let max = max - min;
    let offset = min;
    let min = 0.0f32;
    let x = x - offset;
    let num = (x / max).abs().floor();
    let x = if x >= max {
        fma(-num, max, x)
    } else if x < min {
        fma(num + 1.0f32, max, x)
    } else {
        x
    };
    x + offset
}

pub(crate) fn clamp(x: f32, min: f32, max: f32) -> f32 {
    if x > max {
        max
    } else if x < min {
        min
    } else {
        x
    }
}

pub(crate) fn angle_distance(a1: f32, a2: f32, max_angle: f32) -> f32 {
    let a1 = wrap(a1, 0.0, max_angle);
    let a2 = wrap(a2, 0.0, max_angle);
    if a1 == a2 {
        return 0.0;
    }
    let d1 = a2 - a1;
    let mut d2 = max_angle - d1.abs();
    if d1 > 0.0 {
        d2 = -d2;
    }
    if d1.abs() < d2.abs() { d1 } else { d2 }
}

pub(crate) fn turn_angle(angle: f32, final_angle: f32, speed: f32, max_angle: f32) -> f32 {
    let mut angle = angle;
    if angle != final_angle {
        let dist = final_angle - angle;
        if (final_angle > angle && dist < max_angle) || (final_angle < angle && dist < -max_angle) {
            angle += speed;
        } else {
            angle += -speed;
        }
    }
    if f64::from(angle_distance(angle, final_angle, max_angle * 2.0).abs())
        <= f64::from(speed) * 1.5
    {
        angle = final_angle;
    }
    angle
}

pub(crate) fn angle_from_points_2d(start: Vector2f, goal: Vector2f) -> f32 {
    let mut dx = goal.x - start.x;
    let mut dy = goal.y - start.y;
    if dx == 0.0 {
        dx = 0.00001;
    }
    if dy == 0.0 {
        dy = 0.00001;
    }
    // NaN input leaves gcc's result uninitialised; 0 here.
    if dx >= 0.0 && dy < 0.0 {
        atan(dx / -dy)
    } else if dx >= 0.0 && dy >= 0.0 {
        atan(dy / dx) + PI2
    } else if dx < 0.0 && dy >= 0.0 {
        atan(-dx / dy) + PI
    } else if dx < 0.0 && dy < 0.0 {
        atan(-dy / -dx) + PI2 + PI
    } else {
        0.0
    }
}

pub(crate) fn to_rad(a: f32) -> f32 {
    (a / 360.0f32) * TWO_PI
}

pub(crate) fn to_deg(a: f32) -> f32 {
    (a / TWO_PI) * 360.0f32
}

pub(crate) fn log2_to_int(x: i32) -> i32 {
    match x {
        1 => 0,
        2 => 1,
        4 => 2,
        8 => 3,
        16 => 4,
        32 => 5,
        64 => 6,
        128 => 7,
        256 => 8,
        512 => 9,
        _ => ((ln(x as f32) / ln(2.0f32) + 0.5f32).floor()) as i32,
    }
}

pub(crate) fn is_pow2(x: i32) -> bool {
    matches!(
        x,
        0 | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | 512 | 1024 | 2048 | 4096 | 8192
    )
}

pub(crate) fn interpolate(a: f32, b: f32, t: f32) -> f32 {
    fma(a, 1.0f32 - t, b * t)
}

pub(crate) fn increase_to(x: f32, add: f32, dest: f32) -> f32 {
    let mut x = x;
    if x < dest {
        x += add;
        if x > dest {
            x = dest;
        }
    } else if x > dest {
        x -= add;
        if x < dest {
            x = dest;
        }
    }
    x
}

pub(crate) fn smooth_curve(x: f32) -> f32 {
    x * x * fma(-x, 2.0, 3.0)
}

pub(crate) fn vector2_dist_sqr(ax: f32, ay: f32, bx: f32, by: f32) -> f32 {
    let dx = ax - bx;
    let dy = ay - by;
    fma(dx, dx, dy * dy)
}

pub(crate) fn vector2_dist(ax: f32, ay: f32, bx: f32, by: f32) -> f32 {
    vector2_dist_sqr(ax, ay, bx, by).sqrt()
}

pub(crate) fn vector_from_angle_2d(angle: f32, length: f32) -> Vector2f {
    Vector2f {
        x: sin(angle) * length,
        y: -cos(angle) * length,
    }
}

pub(crate) fn angle_from_vector(v: Vector2f) -> (f32, f32) {
    let length = fma(v.x, v.x, v.y * v.y).sqrt();
    let angle = angle_from_points_2d(Vector2f { x: 0.0, y: 0.0 }, v);
    (angle, length)
}

pub(crate) fn project_vector_2d(src: Vector2f, dest: Vector2f) -> Vector2f {
    let t = fma(src.x, dest.x, src.y * dest.y) / fma(dest.x, dest.x, dest.y * dest.y);
    Vector2f {
        x: dest.x * t,
        y: dest.y * t,
    }
}
