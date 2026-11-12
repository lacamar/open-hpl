//! `cPhysicsWorldNewton`: world lifetime, stepping, queries and the ray/collision/debug callbacks.

use super::host::host;
use super::reg::{self, WorldRec, reg};
use super::transposed;
use crate::math::{dot3, fma, len3, matrix_inverse, matrix_mul_vec, sub3, v3};
use crate::obj::Handle;
use crate::sys::newton::{
    NewtonBody, NewtonCollision, NewtonCollisionCollide, NewtonCollisionForEachPolygonDo,
    NewtonCollisionRayCast, NewtonCreate, NewtonDestroy, NewtonGetThreadsCount,
    NewtonSetFrictionModel, NewtonSetSolverModel, NewtonSetThreadsCount, NewtonSetWorldSize,
    NewtonUpdate, NewtonWorld, NewtonWorldForEachBodyInAABBDo, NewtonWorldRayCast,
};
use crate::types::{Matrixf, Vector3f};
use std::ffi::{c_int, c_uint, c_void};
use std::ptr;

const ACCURACY_LOW: i32 = 0;
const ACCURACY_MEDIUM: i32 = 1;

/// Layout of `cCollidePoint`.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct CollPoint {
    pub point: Vector3f,
    pub normal: Vector3f,
    pub depth: f32,
}

/// # Safety
/// `out_nw` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntw_create(out_nw: *mut *mut NewtonWorld) -> u64 {
    // SAFETY: plain Newton call.
    let nw = unsafe { NewtonCreate() };
    // SAFETY: caller contract.
    if let Some(out) = unsafe { out_nw.as_mut() } {
        *out = nw;
    }
    let Some(mut r) = reg() else { return 0 };
    r.worlds
        .insert(WorldRec {
            nw,
            max_step: 1.0 / 60.0,
        })
        .map_or(0, Handle::to_bits)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntw_destroy(w: u64) {
    let Some(h) = Handle::from_bits(w) else {
        return;
    };
    let rec = {
        let Some(mut r) = reg() else { return };
        r.forced.retain(|(wh, _)| *wh != h);
        r.worlds.remove(h)
    };
    if let Some(rec) = rec {
        // SAFETY: the world is destroyed exactly once, after its record left the registry.
        unsafe { NewtonDestroy(rec.nw) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntw_simulate(w: u64, time_step: f32) {
    let Some((h, rec)) = reg::world(w) else {
        return;
    };
    let mut ts = time_step;
    while ts > rec.max_step {
        // SAFETY: no registry borrow is held; Newton callbacks re-enter through `reg()`.
        unsafe { NewtonUpdate(rec.nw, rec.max_step) };
        ts -= rec.max_step;
    }
    // SAFETY: as above.
    unsafe { NewtonUpdate(rec.nw, ts) };
    let Some(mut r) = reg() else { return };
    let mut cleared = Vec::new();
    r.forced.retain(|(wh, bh)| {
        if *wh == h {
            cleared.push(*bh);
            false
        } else {
            true
        }
    });
    for bh in cleared {
        if let Some(b) = r.bodies.get_mut(bh) {
            b.force = v3(0.0, 0.0, 0.0);
            b.torque = v3(0.0, 0.0, 0.0);
            b.forced = false;
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntw_set_max_step(w: u64, step: f32) {
    if let Some(mut r) = reg()
        && let Some(h) = Handle::from_bits(w)
        && let Some(rec) = r.worlds.get_mut(h)
    {
        rec.max_step = step;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntw_max_step(w: u64) -> f32 {
    reg::world(w).map_or(0.0, |(_, r)| r.max_step)
}

/// # Safety
/// `min` and `max` point to three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntw_set_size(w: u64, min: *const f32, max: *const f32) {
    if let Some((_, r)) = reg::world(w) {
        // SAFETY: caller contract.
        unsafe { NewtonSetWorldSize(r.nw, min, max) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntw_set_accuracy(w: u64, accuracy: i32) {
    let Some((_, r)) = reg::world(w) else { return };
    let (solver, friction) = match accuracy {
        ACCURACY_LOW => (1, 1),
        ACCURACY_MEDIUM => (2, 1),
        2 => (0, 0),
        _ => return,
    };
    // SAFETY: plain Newton calls.
    unsafe {
        NewtonSetSolverModel(r.nw, solver);
        NewtonSetFrictionModel(r.nw, friction);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntw_set_threads(w: u64, n: i32) {
    if let Some((_, r)) = reg::world(w) {
        // SAFETY: plain Newton call.
        unsafe { NewtonSetThreadsCount(r.nw, n) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntw_threads(w: u64) -> i32 {
    // SAFETY: plain Newton call.
    reg::world(w).map_or(0, |(_, r)| unsafe { NewtonGetThreadsCount(r.nw) })
}

struct VecCtx {
    vec: usize,
}

unsafe extern "C" fn push_body(body: *const NewtonBody, user: *mut c_void) {
    let Some(host) = host() else { return };
    // SAFETY: Newton hands back the user data given to `NewtonWorldForEachBodyInAABBDo`.
    let Some(ctx) = (unsafe { user.cast::<VecCtx>().as_ref() }) else {
        return;
    };
    if let Some((_, rec)) = super::body::of_newton(body) {
        // SAFETY: `host` callbacks are live for the process.
        unsafe { (host.body_vec_push)(ctx.vec, rec.cpp) };
    }
}

/// # Safety
/// `min` and `max` point to three floats; `vec` is the C++ output vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntw_bodies_in_aabb(
    w: u64,
    min: *const f32,
    max: *const f32,
    vec: usize,
) {
    if let Some((_, r)) = reg::world(w) {
        let mut ctx = VecCtx { vec };
        // SAFETY: `ctx` outlives the synchronous call.
        unsafe {
            NewtonWorldForEachBodyInAABBDo(
                r.nw,
                min,
                max,
                Some(push_body),
                ptr::from_mut(&mut ctx).cast(),
            );
        }
    }
}

struct RayCtx {
    cb: usize,
    origin: Vector3f,
    delta: Vector3f,
    length: f32,
    box_min: Vector3f,
    box_max: Vector3f,
    calc_dist: bool,
    calc_normal: bool,
    calc_point: bool,
}

fn aabb_intersection(min1: Vector3f, max1: Vector3f, min2: Vector3f, max2: Vector3f) -> bool {
    !(max1.x < min2.x
        || max1.y < min2.y
        || max1.z < min2.z
        || max2.x < min1.x
        || max2.y < min1.y
        || max2.z < min1.z)
}

unsafe extern "C" fn ray_prefilter(
    body: *const NewtonBody,
    _collision: *const NewtonCollision,
    user: *mut c_void,
) -> c_uint {
    let (Some(host), Some((_, rec))) = (host(), super::body::of_newton(body)) else {
        return 0;
    };
    // SAFETY: `user` is the `RayCtx` of the cast in progress.
    let Some(ctx) = (unsafe { user.cast::<RayCtx>().as_ref() }) else {
        return 0;
    };
    // SAFETY: host callbacks are live for the process; out-pointers are valid.
    unsafe {
        if !(host.body_is_active)(rec.cpp) {
            return 0;
        }
        let (mut min, mut max) = (Vector3f::default(), Vector3f::default());
        (host.body_aabb)(rec.cpp, &mut min, &mut max);
        if !aabb_intersection(ctx.box_min, ctx.box_max, min, max) {
            return 0;
        }
        c_uint::from((host.ray_before_intersect)(ctx.cb, rec.cpp))
    }
}

unsafe extern "C" fn ray_filter(
    body: *const NewtonBody,
    normal: *const f32,
    _collision_id: c_int,
    user: *mut c_void,
    t: f32,
) -> f32 {
    let (Some(host), Some((_, rec))) = (host(), super::body::of_newton(body)) else {
        return 1.0;
    };
    // SAFETY: `user` is the `RayCtx` of the cast in progress.
    let Some(ctx) = (unsafe { user.cast::<RayCtx>().as_ref() }) else {
        return 1.0;
    };
    // SAFETY: host callbacks are live for the process.
    if !unsafe { (host.body_is_active)(rec.cpp) } {
        return 1.0;
    }
    let dist = ctx.length * t;
    // SAFETY: Newton passes a three-float normal.
    let n = unsafe { normal.cast::<Vector3f>().as_ref() }
        .copied()
        .unwrap_or_default();
    let p = v3(
        fma(ctx.delta.x, t, ctx.origin.x),
        fma(ctx.delta.y, t, ctx.origin.y),
        fma(ctx.delta.z, t, ctx.origin.z),
    );
    // SAFETY: the pointers are valid for the call.
    let keep = unsafe {
        (host.ray_on_intersect)(
            ctx.cb,
            rec.cpp,
            t,
            if ctx.calc_dist { &dist } else { ptr::null() },
            if ctx.calc_normal { &n } else { ptr::null() },
            if ctx.calc_point { &p } else { ptr::null() },
        )
    };
    if keep { 1.0 } else { 0.0 }
}

/// # Safety
/// `origin` and `end` are valid vectors.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntw_cast_ray(
    w: u64,
    cb: usize,
    origin: *const Vector3f,
    end: *const Vector3f,
    calc_dist: bool,
    calc_normal: bool,
    calc_point: bool,
    prefilter: bool,
) {
    // SAFETY: caller contract.
    let (Some((_, r)), Some(origin), Some(end)) =
        (reg::world(w), unsafe { origin.as_ref() }, unsafe {
            end.as_ref()
        })
    else {
        return;
    };
    let (origin, end) = (*origin, *end);
    let delta = sub3(end, origin);
    let lo = |a: f32, b: f32| if a > b { b } else { a };
    let hi = |a: f32, b: f32| if a > b { a } else { b };
    let mut ctx = RayCtx {
        cb,
        origin,
        delta,
        length: len3(delta),
        box_min: v3(
            lo(origin.x, end.x),
            lo(origin.y, end.y),
            lo(origin.z, end.z),
        ),
        box_max: v3(
            hi(origin.x, end.x),
            hi(origin.y, end.y),
            hi(origin.z, end.z),
        ),
        calc_dist,
        calc_normal,
        calc_point,
    };
    // SAFETY: `ctx` outlives the synchronous cast; no registry borrow is held.
    unsafe {
        NewtonWorldRayCast(
            r.nw,
            ptr::from_ref(&origin).cast(),
            ptr::from_ref(&end).cast(),
            Some(ray_filter),
            ptr::from_mut(&mut ctx).cast(),
            if prefilter { Some(ray_prefilter) } else { None },
        );
    }
}

/// # Safety
/// All pointers are valid; `normal` receives three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntw_cast_ray_on_shape(
    coll: *const NewtonCollision,
    transform: *const Matrixf,
    origin: *const Vector3f,
    end: *const Vector3f,
    normal: *mut Vector3f,
    local_positions: bool,
) -> f32 {
    // SAFETY: caller contract.
    let (Some(m), Some(o), Some(e)) = (
        unsafe { transform.as_ref() },
        unsafe { origin.as_ref() },
        unsafe { end.as_ref() },
    ) else {
        return 0.0;
    };
    let (mut start, mut stop) = (*o, *e);
    if !local_positions {
        let inv = matrix_inverse(m);
        start = matrix_mul_vec(&inv, *o);
        stop = matrix_mul_vec(&inv, *e);
    }
    let mut attribute: c_int = 0;
    // SAFETY: caller contract on `normal`; locals are valid.
    unsafe {
        NewtonCollisionRayCast(
            coll,
            ptr::from_ref(&start).cast(),
            ptr::from_ref(&stop).cast(),
            normal.cast(),
            &mut attribute,
        )
    }
}

fn correct_normal(normal: Vector3f, point: Vector3f, center: Vector3f) -> Vector3f {
    let to_point = sub3(point, center);
    if dot3(to_point, normal) > 0.0 {
        v3(-normal.x, -normal.y, -normal.z)
    } else {
        normal
    }
}

struct Scratch {
    points: Vec<f32>,
    normals: Vec<f32>,
    depths: Vec<f32>,
}

/// Collision of two shapes (or their compound sub-shapes) into `out`; returns whether anything hit.
///
/// # Safety
/// `subs_a`/`subs_b` point to `na`/`nb` collisions, matrices are valid, `out` has `cap` points.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntw_check_collision(
    w: u64,
    subs_a: *const *mut NewtonCollision,
    na: usize,
    subs_b: *const *mut NewtonCollision,
    nb: usize,
    compound: bool,
    mtx_a: *const Matrixf,
    mtx_b: *const Matrixf,
    max_points: i32,
    correct: bool,
    a_is_mesh: bool,
    out: *mut CollPoint,
    cap: usize,
    out_count: *mut i32,
) -> bool {
    // SAFETY: caller contract.
    let (Some((_, r)), Some(ma), Some(mb)) = (reg::world(w), unsafe { mtx_a.as_ref() }, unsafe {
        mtx_b.as_ref()
    }) else {
        return false;
    };
    // SAFETY: caller contract.
    let (list_a, list_b, out) = unsafe {
        (
            std::slice::from_raw_parts(subs_a, na),
            std::slice::from_raw_parts(subs_b, nb),
            std::slice::from_raw_parts_mut(out, cap),
        )
    };
    let (ta, tb) = (transposed(ma), transposed(mb));
    let center = v3(ma.m[0][3], ma.m[1][3], ma.m[2][3]);
    let scratch = usize::try_from(max_points).unwrap_or(0).max(500);
    let mut sc = Scratch {
        points: vec![0.0f32; scratch * 3],
        normals: vec![0.0f32; scratch * 3],
        depths: vec![0.0f32; scratch],
    };
    let mut remaining = max_points;
    let mut start = 0usize;
    let mut total = 0i32;
    let mut collided = false;

    let collide =
        |sc: &mut Scratch, a: *mut NewtonCollision, b: *mut NewtonCollision, max: i32| -> usize {
            // SAFETY: the scratch buffers hold at least `max` entries; shapes are live Newton collisions.
            let n = unsafe {
                NewtonCollisionCollide(
                    r.nw,
                    max,
                    a,
                    ta.as_ptr(),
                    b,
                    tb.as_ptr(),
                    sc.points.as_mut_ptr(),
                    sc.normals.as_mut_ptr(),
                    sc.depths.as_mut_ptr(),
                    0,
                )
            };
            usize::try_from(n).unwrap_or(0)
        };
    let fill = |dst: &mut CollPoint, i: usize, sc: &Scratch| {
        let get = |s: &[f32], k: usize| s.get(i * 3 + k).copied().unwrap_or(0.0);
        dst.depth = sc.depths.get(i).copied().unwrap_or(0.0);
        dst.normal = v3(
            get(&sc.normals, 0),
            get(&sc.normals, 1),
            get(&sc.normals, 2),
        );
        dst.point = v3(get(&sc.points, 0), get(&sc.points, 1), get(&sc.points, 2));
        if correct && !a_is_mesh {
            dst.normal = correct_normal(dst.normal, dst.point, center);
        }
    };

    if compound {
        'outer: for &sa in list_a {
            for &sb in list_b {
                let n = collide(&mut sc, sa, sb, remaining);
                let Some(n) = (if n < 1 {
                    None
                } else {
                    Some(n.min(usize::try_from(remaining).unwrap_or(0)))
                }) else {
                    continue;
                };
                collided = true;
                remaining -= i32::try_from(n).unwrap_or(0);
                for i in 0..n {
                    if let Some(dst) = out.get_mut(start + i) {
                        fill(dst, i, &sc);
                    }
                }
                start += n;
                total += i32::try_from(n).unwrap_or(0);
                if remaining <= 0 {
                    break 'outer;
                }
            }
        }
        // SAFETY: caller contract.
        if let Some(c) = unsafe { out_count.as_mut() } {
            *c = total;
        }
        return collided;
    }

    let (Some(&sa), Some(&sb)) = (list_a.first(), list_b.first()) else {
        return false;
    };
    let n = collide(&mut sc, sa, sb, max_points);
    if n < 1 {
        return false;
    }
    let n = n.min(usize::try_from(max_points).unwrap_or(0));
    for i in 0..n {
        if let Some(dst) = out.get_mut(i) {
            fill(dst, i, &sc);
        }
    }
    // SAFETY: caller contract.
    if let Some(c) = unsafe { out_count.as_mut() } {
        *c = i32::try_from(n).unwrap_or(0);
    }
    true
}

struct DebugCtx {
    low: usize,
    color: usize,
}

unsafe extern "C" fn debug_polygon(
    user: *mut c_void,
    count: c_int,
    face: *const f32,
    _face_id: c_int,
) {
    let Some(host) = host() else { return };
    // SAFETY: `user` is the `DebugCtx` of the iteration in progress.
    let Some(ctx) = (unsafe { user.cast::<DebugCtx>().as_ref() }) else {
        return;
    };
    let Ok(n) = usize::try_from(count) else {
        return;
    };
    if n == 0 || face.is_null() {
        return;
    }
    // SAFETY: Newton passes `count` vertices of three floats.
    let verts = unsafe { std::slice::from_raw_parts(face.cast::<Vector3f>(), n) };
    let mut p0 = verts.last().copied().unwrap_or_default();
    for p1 in verts {
        // SAFETY: host callbacks are live for the process.
        unsafe { (host.draw_line)(ctx.low, &p0, p1, ctx.color) };
        p0 = *p1;
    }
}

/// # Safety
/// `transform` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntw_debug_shape(
    coll: *const NewtonCollision,
    transform: *const Matrixf,
    low: usize,
    color: usize,
) {
    // SAFETY: caller contract.
    let Some(m) = (unsafe { transform.as_ref() }) else {
        return;
    };
    let t = transposed(m);
    let mut ctx = DebugCtx { low, color };
    // SAFETY: `ctx` outlives the synchronous iteration.
    unsafe {
        NewtonCollisionForEachPolygonDo(
            coll,
            t.as_ptr(),
            Some(debug_polygon),
            ptr::from_mut(&mut ctx).cast(),
        );
    }
}
