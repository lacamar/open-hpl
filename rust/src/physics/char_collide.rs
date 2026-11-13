//! `cCharacterBodyCollideGravity` and `cCharacterBodyCollidePush`: what a character's gravity and push
//! shape queries do to the bodies they hit (`OHPL_IMPL=rust`). Called from inside world collision
//! loops; the states are read through raw pointers around every host op.

use super::char_conn::CharExt;
use super::character::CharState;
use super::host::H;
use super::wreg::{Wa, WorldOp};
use crate::math::{div3, normalize3, v3};
use crate::types::Vector3f;

pub const CC_BODY_MATERIAL: i32 = 508;
pub const CC_BODY_FLAGS: i32 = 509;
pub const CC_BODY_MASS: i32 = 510;
pub const CC_CD_NUM: i32 = 511;
pub const CC_CD_POINT: i32 = 512;
pub const CC_CALLBACK_GRAVITY: i32 = 514;
pub const CC_WORLD_GRAVITY_Y: i32 = 515;
pub const CC_BODY_ADD_FORCE_AT: i32 = 516;
pub const CC_CHAR_STATE: i32 = 519;

const FLAG_CAN_ATTACH: usize = 1;
const FLAG_IS_CHARACTER: usize = 2;
const FLAG_PUSHED_BY_GRAVITY: usize = 4;

fn call(op: WorldOp, code: i32, p: [usize; 6]) -> Wa {
    let mut a = Wa {
        p,
        f: [0.0; 2],
        i: [0, 0],
        s: std::ptr::null(),
        n: 0,
        rf: 0.0,
        rv: [0.0; 3],
    };
    // SAFETY: the op belongs to the live character and `a` is a live block.
    a.p[5] = unsafe { op(code, &mut a) };
    a
}

fn mass(op: WorldOp, body: H) -> f32 {
    call(op, CC_BODY_MASS, [body, 0, 0, 0, 0, 0]).rf
}

fn point(op: WorldOp, cd: H, i: usize) -> (Vector3f, f32) {
    let a = call(op, CC_CD_POINT, [cd, i, 0, 0, 0, 0]);
    (v3(a.rv[0], a.rv[1], a.rv[2]), a.rf)
}

fn point_pos(op: WorldOp, cd: H, i: usize) -> Vector3f {
    let mut p = [0.0f32; 3];
    call(op, CC_CD_POINT, [cd, i, 1, p.as_mut_ptr() as usize, 0, 0]);
    v3(p[0], p[1], p[2])
}

fn force_at(op: WorldOp, body: H, force: Vector3f, pos: Vector3f) {
    let (f, p) = ([force.x, force.y, force.z], [pos.x, pos.y, pos.z]);
    call(
        op,
        CC_BODY_ADD_FORCE_AT,
        [body, f.as_ptr() as usize, p.as_ptr() as usize, 0, 0, 0],
    );
}

fn add3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x + b.x, a.y + b.y, a.z + b.z)
}

fn sub3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x - b.x, a.y - b.y, a.z - b.z)
}

fn mul3(a: Vector3f, s: f32) -> Vector3f {
    v3(a.x * s, a.y * s, a.z * s)
}

/// `cCharacterBodyCollideGravity::OnCollision`.
///
/// # Safety
/// `ext` and `st` are the live embedded states of character `ch`; `cd` is the live collide data.
pub unsafe fn collide_gravity(
    op: WorldOp,
    ch: H,
    ext: *mut CharExt,
    st: *const CharState,
    body: H,
    cd: H,
) {
    // SAFETY: caller contract for every deref below.
    let e = || unsafe { ext.as_mut() };
    let Some(x) = e() else { return };
    let mat = call(op, CC_BODY_MATERIAL, [body, 0, 0, 0, 0, 0]).p[5];
    x.gravity_collide_material = mat;

    let flags = call(op, CC_BODY_FLAGS, [body, 0, 0, 0, 0, 0]).p[5];
    let num = call(op, CC_CD_NUM, [cd, 0, 0, 0, 0, 0]).p[5];
    if flags & FLAG_CAN_ATTACH != 0
        && flags & FLAG_IS_CHARACTER == 0
        && point(op, cd, 0).0.y > 0.001
        && let Some(x) = e()
        && usize::try_from(x.gravity_attachment_contacts).is_ok_and(|c| c < num)
    {
        x.gravity_attachment_contacts = i32::try_from(num).unwrap_or(i32::MAX);
        x.gravity_attached_body = body;
    }

    call(op, CC_CALLBACK_GRAVITY, [ch, body, cd, 0, 0, 0]);

    if mass(op, body) == 0.0 || flags & FLAG_PUSHED_BY_GRAVITY == 0 {
        return;
    }

    let mut push_down = false;
    let mut median = v3(0.0, 0.0, 0.0);
    let mut count = 0.0f32;
    for i in 0..num {
        let (normal, _) = point(op, cd, i);
        if normal.y > 0.001 {
            push_down = true;
            count += 1.0;
            median = add3(median, point_pos(op, cd, i));
        }
    }
    if push_down {
        median = div3(median, count);
        // SAFETY: caller contract.
        let Some(s) = (unsafe { st.as_ref() }) else {
            return;
        };
        let force_mul = if s.custom_gravity_active {
            s.custom_gravity.y
        } else {
            call(op, CC_WORLD_GRAVITY_Y, [ch, 0, 0, 0, 0, 0]).rf
        };
        if force_mul < 0.0 {
            force_at(op, body, v3(0.0, s.mass * force_mul, 0.0), median);
        }
    }
}

/// `cCharacterBodyCollidePush::OnCollision`.
///
/// # Safety
/// `ext` and `st` are the live embedded states of character `ch`; `cd` is the live collide data.
pub unsafe fn collide_push(op: WorldOp, ext: *const CharExt, st: *const CharState, body: H, cd: H) {
    // SAFETY: caller contract.
    let (Some(x), Some(s)) = (unsafe { (ext.as_ref(), st.as_ref()) }) else {
        return;
    };
    let flags = call(op, CC_BODY_FLAGS, [body, 0, 0, 0, 0, 0]).p[5];
    let body_mass = mass(op, body);
    let is_char = flags & FLAG_IS_CHARACTER != 0;
    if body_mass == 0.0 && !is_char {
        return;
    }
    if body_mass > x.max_push_mass {
        return;
    }

    if is_char {
        let hit = call(op, CC_CHAR_STATE, [body, 0, 0, 0, 0, 0]).p[5] as *mut CharState;
        // SAFETY: the host returns the embedded state of the hit body's character; it is not `ch`'s own.
        let Some(h) = (unsafe { hit.as_mut() }) else {
            return;
        };
        if h.mass > x.char_max_push_mass {
            return;
        }
        let hit_top = h.position.y + h.size.y / 2.0;
        let bottom = s.position.y - (s.size.y / 2.0 - s.size.y * 0.1);
        if hit_top < bottom {
            return;
        }
        let mut dir = sub3(h.position, s.position);
        if x.char_push_in_2d {
            dir.y = 0.0;
        }
        // gcc contracts this inlined Normalize as fma(y, y, x * x) + z * z.
        let len = (dir.y.mul_add(dir.y, dir.x * dir.x) + dir.z * dir.z).sqrt();
        if f64::from(len) > 1e-08 {
            let inv = 1.0f32 / len;
            dir.x *= inv;
            dir.y *= inv;
            dir.z *= inv;
        }
        // gcc inlines AddForce here and fuses `force += dir * pushForce`.
        h.force.x = dir.x.mul_add(x.char_push_force, h.force.x);
        h.force.y = dir.y.mul_add(x.char_push_force, h.force.y);
        h.force.z = dir.z.mul_add(x.char_push_force, h.force.z);
    } else {
        let num = call(op, CC_CD_NUM, [cd, 0, 0, 0, 0, 0]).p[5];
        let mut median = v3(0.0, 0.0, 0.0);
        let mut count = 0.0f32;
        for i in 0..num {
            count += 1.0;
            median = add3(median, point_pos(op, cd, i));
        }
        median = div3(median, count);

        let mut min_y = s.position.y - s.size.y / 2.0;
        min_y += 0.01;
        if median.y < min_y {
            return;
        }

        let mut force = x.push_force;
        if x.push_impulse > 0.0 {
            let cap = body_mass * x.push_impulse;
            force = if force < cap { force } else { cap };
        }
        let mut dir = sub3(median, s.position);
        if x.push_in_2d {
            dir.y = 0.0;
        }
        normalize3(&mut dir);
        force_at(op, body, mul3(dir, force), median);
    }
}
