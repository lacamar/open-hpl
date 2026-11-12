//! `iLuxPlayerState_InteractRotateBase` and its lever, wheel and swing-door states: speed
//! damping, hinge torque, per-state push direction, wheel gesture detection and the door throw.

use super::hw;
use super::states_vec::{
    Pid3, cross, dist3, dot, fmad, hmul3, madd, mat_vec, max_length, msub3, nmul, normalize,
};
use crate::math::{div3, fma, len3, sub3, v3};
use crate::types::{Matrixf, Vector2f, Vector3f};

/// `OnEnterState` max distance: `dist(feet, pivot) * 1.2 + 0.5`.
pub fn rotate_max_distance(feet: Vector3f, pivot: Vector3f) -> f32 {
    fma(dist3(feet, pivot), 1.2, 0.5)
}

/// `Update` range test; the C++ keeps running the update after changing state.
pub fn rotate_far(feet: Vector3f, pivot: Vector3f, max: f32) -> bool {
    dist3(feet, pivot) > max
}

/// Speed damping and mouse reset before `GetSpeedAdd`.
pub fn rotate_slow(speed: &mut f32, mouse: Vector2f, slow_factor: f32, dt: f32) {
    let slow = hw::mul(speed.abs(), slow_factor);
    if *speed > 0.0 {
        *speed = hw::msub(slow, dt, *speed);
        if *speed < 0.0 {
            *speed = 0.0;
        }
    }
    if *speed < 0.0 {
        *speed = fmad(slow, dt, *speed);
        if *speed > 0.0 {
            *speed = 0.0;
        }
    }
    if mouse.x == 0.0 && mouse.y == 0.0 {
        *speed = 0.0;
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct RotateTorqueIn {
    pub pin: Vector3f,
    pub ang_vel: Vector3f,
    pub inertia: Matrixf,
    pub speed_add: f32,
    pub speed_factor: f32,
    pub max_speed: f32,
    pub max_torque: f32,
}

/// New speed from `speed_add`, then the hinge torque.
pub fn rotate_torque(i: &RotateTorqueIn, speed: &mut f32, pid: &mut Pid3, dt: f32) -> Vector3f {
    *speed = fmad(
        hw::mul(hw::mul(i.speed_add, 3000.0), i.speed_factor),
        dt,
        *speed,
    );
    if *speed > i.max_speed {
        *speed = i.max_speed;
    }
    if *speed < -i.max_speed {
        *speed = -i.max_speed;
    }
    let d = dot(i.pin, i.ang_vel);
    let wanted = hmul3(i.pin, *speed);
    let t = pid.output(msub3(d, i.pin, wanted), dt);
    let t = mat_vec(&i.inertia, t);
    max_length(t, i.max_torque)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct JointGeom {
    pub body_matrix: Matrixf,
    pub mass_centre: Vector3f,
    pub pivot: Vector3f,
    pub pin: Vector3f,
    pub up: Vector3f,
    pub right: Vector3f,
    pub forward: Vector3f,
    pub mouse: Vector2f,
}

fn joint_to_body(g: &JointGeom) -> Vector3f {
    let centre = mat_vec(&g.body_matrix, g.mass_centre);
    let mut v = sub3(centre, g.pivot);
    normalize(&mut v);
    v
}

fn norm(mut v: Vector3f) -> Vector3f {
    normalize(&mut v);
    v
}

fn push_dir(a: Vector3f, my: f32, b: Vector3f, mx: f32) -> Vector3f {
    v3(
        fma(a.x, my, nmul(mx, b.x)),
        fma(a.y, my, nmul(mx, b.y)),
        fma(a.z, my, nmul(mx, b.z)),
    )
}

/// `cLuxPlayerState_InteractLever::GetSpeedAdd`.
pub fn lever_speed_add(g: &JointGeom) -> f32 {
    let jtb = joint_to_body(g);
    let up_jf = cross(g.up, g.pin);
    let up = norm(madd(up_jf, dot(g.up, jtb), g.up));
    let right_jf = cross(g.right, g.pin);
    let right = norm(madd(right_jf, dot(g.right, jtb), g.right));
    let push = push_dir(up, g.mouse.y, right, g.mouse.x);
    dot(cross(jtb, push), g.pin)
}

/// `cLuxPlayerState_InteractSwingDoor::GetSpeedAdd`.
pub fn swing_door_speed_add(g: &JointGeom) -> f32 {
    let jtb = joint_to_body(g);
    let sum = v3(
        g.up.x + g.forward.x,
        g.up.y + g.forward.y,
        g.up.z + g.forward.z,
    );
    let nx = -g.mouse.x;
    let my = g.mouse.y;
    let push = v3(
        fmad(sum.x, my, hw::mul(g.right.x, nx)),
        fmad(sum.y, my, hw::mul(g.right.y, nx)),
        fmad(sum.z, my, hw::mul(nx, g.right.z)),
    );
    dot(cross(jtb, push), g.pin)
}

/// `cLuxPlayerState_InteractSwingDoor::OnThrow`: `(joint forward, impulse)`.
pub fn swing_door_throw(g: &JointGeom, impulse: f32) -> (Vector3f, Vector3f) {
    let jtb = joint_to_body(g);
    let jf = cross(g.pin, jtb);
    let mut v = v3(
        hw::mul(impulse, jf.x),
        hw::mul(impulse, jf.y),
        hw::mul(impulse, jf.z),
    );
    if dot(jf, g.forward) < 0.0 {
        v = v3(-v.x, -v.y, -v.z);
    }
    (jf, v)
}

/// Wheel history trim: whether the oldest entry is dropped after a push.
pub fn wheel_pop(size: usize) -> bool {
    size > 10
}

fn norm2(v: Vector2f) -> Vector2f {
    let l = fma(v.x, v.x, v.y * v.y).sqrt();
    if f64::from(l) > 1e-08 {
        let inv = 1.0f32 / l;
        return Vector2f {
            x: v.x * inv,
            y: v.y * inv,
        };
    }
    v
}

/// `cLuxPlayerState_InteractWheel::GetSpeedAdd` once the newest mouse delta is in `hist`.
pub fn wheel_speed_add(
    hist: &[Vector2f],
    mouse: Vector2f,
    pin: Vector3f,
    forward: Vector3f,
) -> f32 {
    if hist.len() < 4 {
        return 0.0;
    }
    let mut prev = Vector2f::default();
    let mut first = true;
    let mut proper = 0.0f32;
    let mut centre = [Vector3f::default(); 2];
    for h in hist {
        let n = norm2(*h);
        if first {
            first = false;
            prev = n;
            continue;
        }
        let diff = Vector2f {
            x: n.x - prev.x,
            y: n.y - prev.y,
        };
        if fma(diff.x, diff.x, diff.y * diff.y).sqrt() < 0.00001 {
            continue;
        }
        let tangents = [
            Vector2f {
                x: diff.y,
                y: -diff.x,
            },
            Vector2f {
                x: -diff.y,
                y: diff.x,
            },
        ];
        for (c, t) in centre.iter_mut().zip(tangents) {
            let t = norm2(t);
            *c = v3(c.x + (prev.x + t.x), c.y + (prev.y + t.y), c.z);
        }
        proper += 1.0;
    }
    let dist = |c: Vector3f| len3(div3(c, proper));
    let (d0, d1) = (dist(centre[0]), dist(centre[1]));
    let mut dir = 1.0f32;
    let mut min = d1;
    if d0 < d1 {
        dir = -1.0;
        min = d0;
    }
    if min > 0.75 {
        return 0.0;
    }
    if dot(pin, forward) < 0.0 {
        dir = -dir;
    }
    let length = fma(mouse.x, mouse.x, mouse.y * mouse.y).sqrt();
    (dir * length) * 1.0
}

/// Camera yaw and pitch of the mouse delta feeding `mvMouseAdd` (`OnMove`).
pub fn move_to_mouse(forward: bool, mul: f32, factor: f32, mouse: &mut Vector2f) {
    if forward {
        mouse.y = fma(mul, factor, mouse.y);
    } else {
        mouse.x = fma(mul, -factor, mouse.x);
    }
}
