//! C ABI of the critter layer, mirrored in `soma/src/game/RustSomaUi.h`.

use super::critter::{self, CritterHost, CritterProps, CritterState, Flock, Member, arr, vec};
use crate::types::Vector3f;
use std::ffi::c_void;

type Ctx = *mut c_void;

#[repr(C)]
pub struct OhplGs7CritterHost {
    pub ctx: Ctx,
    /// Casts from `start` to `end`; false when there is no map. Distance -1 and a zero normal
    /// for a miss.
    pub cast: unsafe extern "C" fn(Ctx, *const f32, *const f32, *mut f32, *mut f32) -> bool,
}

struct CHost<'a>(&'a OhplGs7CritterHost);

impl CritterHost for CHost<'_> {
    fn cast(&mut self, start: Vector3f, end: Vector3f) -> Option<(f32, Vector3f)> {
        let (a, b) = (arr(start), arr(end));
        let (mut dist, mut n) = (-1.0f32, [0.0f32; 3]);
        // SAFETY: the entry point's caller guarantees the table accepts its `ctx`.
        unsafe {
            (self.0.cast)(
                self.0.ctx,
                a.as_ptr(),
                b.as_ptr(),
                &mut dist,
                n.as_mut_ptr(),
            )
        }
        .then(|| (dist, vec(n)))
    }
}

#[repr(C)]
pub struct OhplGs7FlockMember {
    pub pos: [f32; 3],
    pub vel: [f32; 3],
    pub is_self: bool,
}

#[repr(C)]
pub struct OhplGs7CritterStep {
    pub pos: [f32; 3],
    pub yaw: f32,
    pub pitch: f32,
    /// 0 none, 1 move, 2 idle
    pub play_anim: i32,
}

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<CritterState>() == 52 && offset_of!(CritterState, vel) == 8);
    assert!(size_of::<CritterProps>() == 88);
};

/// # Safety
/// `s` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_critter_state_default(s: *mut CritterState) {
    // SAFETY: caller contract.
    unsafe { *s = CritterState::default() }
}

/// # Safety
/// `s` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_critter_state_init(s: *mut CritterState, fx: f32, fz: f32) {
    // SAFETY: caller contract.
    critter::init_state(unsafe { &mut *s }, fx, fz);
}

/// # Safety
/// All pointers are valid; `pos` has 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_critter_update(
    s: *mut CritterState,
    p: *mut CritterProps,
    host: *const OhplGs7CritterHost,
    pos: *const f32,
    dt: f32,
    out: *mut OhplGs7CritterStep,
) {
    // SAFETY: caller contract.
    unsafe {
        let step = critter::update(
            &mut *s,
            &mut *p,
            &mut CHost(&*host),
            vec(pos.cast::<[f32; 3]>().read()),
            dt,
        );
        *out = OhplGs7CritterStep {
            pos: arr(step.pos),
            yaw: step.yaw,
            pitch: step.pitch,
            play_anim: step.play_anim,
        };
    }
}

unsafe fn v3(p: *const f32) -> Vector3f {
    // SAFETY: caller contract, 3 floats.
    vec(unsafe { p.cast::<[f32; 3]>().read() })
}

unsafe fn put(out: *mut f32, a: Vector3f) {
    // SAFETY: caller contract, 3 floats.
    unsafe { out.cast::<[f32; 3]>().write(arr(a)) }
}

/// # Safety
/// `a` and `out` have 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_critter_safe_normalize(a: *const f32, out: *mut f32) {
    // SAFETY: caller contract.
    unsafe { put(out, critter::safe_normalize(v3(a))) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_critter_change_max_speed(f: f32, goal: f32, acc: f32, dt: f32) -> f32 {
    critter::change_max_speed(f, goal, acc, dt)
}

/// # Safety
/// Vectors have 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_critter_toward_add(
    pos: *const f32,
    target: *const f32,
    normalize: bool,
    dt: f32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe { put(out, critter::toward_add(v3(pos), v3(target), normalize, dt)) }
}

/// # Safety
/// Vectors have 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_critter_stop_add(
    wanted: *const f32,
    amount: f32,
    dt: f32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe { put(out, critter::stop_add(v3(wanted), amount, dt)) }
}

/// # Safety
/// Vectors have 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_critter_toward_center_add(
    pos: *const f32,
    has_center: bool,
    center: *const f32,
    dt: f32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let c = has_center.then(|| v3(center));
        put(out, critter::toward_center_add(v3(pos), c, dt));
    }
}

#[repr(C)]
pub struct OhplGs7Flock {
    pub center_mul: f32,
    pub center_y_mul: f32,
    pub separation: f32,
    pub alignment: f32,
    pub cohesion: f32,
    pub max_checks: i32,
    pub dt: f32,
}

/// # Safety
/// Vectors have 3 floats; `members` has `n` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_critter_flocking_add(
    pos: *const f32,
    self_vel: *const f32,
    has_center: bool,
    center: *const f32,
    members: *const OhplGs7FlockMember,
    n: usize,
    f: *const OhplGs7Flock,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let ms: Vec<Member> = if n == 0 {
            Vec::new()
        } else {
            std::slice::from_raw_parts(members, n)
                .iter()
                .map(|m| Member {
                    is_self: m.is_self,
                    pos: vec(m.pos),
                    vel: vec(m.vel),
                })
                .collect()
        };
        let f = &*f;
        let flock = Flock {
            center_mul: f.center_mul,
            center_y_mul: f.center_y_mul,
            separation: f.separation,
            alignment: f.alignment,
            cohesion: f.cohesion,
            max_checks: f.max_checks,
            dt: f.dt,
        };
        let c = has_center.then(|| v3(center));
        put(
            out,
            critter::flocking_add(v3(pos), v3(self_vel), c, &ms, &flock),
        );
    }
}

/// # Safety
/// Vectors have 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_critter_wander_add(
    s: *mut CritterState,
    wanted: *const f32,
    length: f32,
    radius: f32,
    dt: f32,
    three_d: bool,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let a = critter::wander_add(&mut *s, v3(wanted), length, radius, dt, three_d);
        put(out, a);
    }
}

/// # Safety
/// Vectors have 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_critter_far_exceeded(
    player: *const f32,
    pos: *const f32,
    max_enabled_distance: f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { critter::far_exceeded(v3(player), v3(pos), max_enabled_distance) }
}

/// # Safety
/// Pointers valid; vectors have 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_critter_towards_ground_add(
    s: *mut CritterState,
    host: *const OhplGs7CritterHost,
    pos: *const f32,
    max_height: f32,
    has_mesh: bool,
    mesh_height: f32,
    dt: f32,
    far_exceeded: bool,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let a = critter::towards_ground_add(
            &mut *s,
            &mut CHost(&*host),
            v3(pos),
            max_height,
            has_mesh.then_some(mesh_height),
            dt,
            far_exceeded,
        );
        put(out, a);
    }
}

/// # Safety
/// Pointers valid; vectors have 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_critter_wall_avoid_add(
    s: *mut CritterState,
    p: *mut CritterProps,
    host: *const OhplGs7CritterHost,
    pos: *const f32,
    distance_forward: f32,
    dt: f32,
    far_exceeded: bool,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let a = critter::wall_avoid_add(
            &mut *s,
            &mut *p,
            &mut CHost(&*host),
            v3(pos),
            distance_forward,
            dt,
            far_exceeded,
        );
        put(out, a);
    }
}
