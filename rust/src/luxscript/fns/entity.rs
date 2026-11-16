//! Generic entity functions, bodies and forces, entity creation, callbacks and connections.

use super::super::a1::{Arg, raw_bytes};
use super::super::{lookup, tables};
use super::{
    Ent, P, S, TYPE_ANY, TYPE_AREA, TYPE_ENEMY, TYPE_PROP, call, each, entity, error, ops, s, sf,
};
use crate::math::{self, Matrixf, Vector3f, v3};
use std::ffi::c_void;

pub(super) fn lossy(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

pub(super) fn ent_name<'a>(e: Ent) -> &'a [u8] {
    // SAFETY: the host returns the entity's own name string.
    unsafe { raw_bytes(call(ops::ENT_NAME, &[e.a()]).p) }
}

pub(super) fn body_num(e: Ent) -> i32 {
    call(ops::ENT_BODY_NUM, &[e.a()]).as_i()
}

pub(super) fn body(e: Ent, i: i32) -> *const c_void {
    call(ops::ENT_BODY, &[e.a(), Arg::i(i)]).p
}

pub(super) fn matrix_of(op: ops::Op, target: *const c_void) -> Matrixf {
    let p = call(op, &[Arg::ptr(target)]).p.cast::<Matrixf>();
    // SAFETY: the host returns a pointer to a 4x4 float matrix valid until its next call.
    unsafe { p.read_unaligned() }
}

pub(super) fn vec_of(op: ops::Op, target: *const c_void) -> Vector3f {
    let at = |i: i32| call(op, &[Arg::ptr(target), Arg::i(i)]).as_f();
    v3(at(0), at(1), at(2))
}

fn mat_arg(m: &Matrixf) -> Arg {
    Arg::ptr(std::ptr::from_ref(m).cast())
}

/// `MatrixRotate(Vector3ToRad(rot), XYZ)` with the translation set.
pub(super) fn transform(pos: [f32; 3], rot: [f32; 3]) -> Matrixf {
    let mut m = math::matrix_rotate(
        v3(
            math::to_rad(rot[0]),
            math::to_rad(rot[1]),
            math::to_rad(rot[2]),
        ),
        0,
    );
    m.m[0][3] = pos[0];
    m.m[1][3] = pos[1];
    m.m[2][3] = pos[2];
    m
}

/// `GetBodyInEntity`.
pub(super) fn body_in(e: Option<Ent>, want: &[u8]) -> Option<*const c_void> {
    let e = e?;
    let n = body_num(e);
    if n == 0 {
        error(&format!(
            "Entity '{}' contains no bodies!\n",
            lossy(ent_name(e))
        ));
        return None;
    }
    let mut found = None;
    if lookup::searches_bodies(want, n) {
        let en = ent_name(e).len() as i32;
        for i in 0..n {
            let b = body(e, i);
            // SAFETY: the host returns the body's own name string.
            let bn = unsafe { raw_bytes(call(ops::BODY_NAME, &[Arg::ptr(b)]).p) };
            if lookup::body_matches(bn, en, want) {
                found = Some(b);
                break;
            }
        }
    } else {
        found = Some(body(e, 0));
    }
    if found.is_none() {
        error(&format!(
            "Body '{}' could not be found in entity '{}'!\n",
            lossy(want),
            lossy(ent_name(e))
        ));
    }
    found
}

fn create_entity(name: &[u8], file: &[u8], m: &Matrixf, full_save: bool) {
    let latest = Ent(call(ops::MAP_CREATE_ENTITY, &[s(name), s(file), mat_arg(m)]).p);
    if !latest.0.is_null() && ent_name(latest) == name {
        call(ops::ENT_FULL_SAVE, &[latest.a(), Arg::b(full_save)]);
    } else {
        error(&format!(
            "Could not create entity '{}' from file '{}'!\n",
            lossy(name),
            lossy(file)
        ));
    }
}

sf!(SetEntityActive(name: S, on: bool) {
    each(name.0, TYPE_ANY, -1, |e| { call(ops::ENT_ACTIVE, &[e.a(), Arg::b(on)]); });
});

sf!(SetEntityVisible(name: S, on: bool) {
    each(name.0, TYPE_ANY, -1, |e| {
        if call(ops::ENT_HAS_MESH, &[e.a()]).as_b() {
            call(ops::ENT_MESH_VISIBLE, &[e.a(), Arg::b(on)]);
        }
    });
});

sf!(GetEntityExists(name: S) -> bool {
    super::map_exists() && !call(ops::MAP_ENTITY, &[s(name.0)]).p.is_null()
});

sf!(SetEntityPos(name: S, x: f32, y: f32, z: f32) {
    each(name.0, TYPE_ANY, -1, |e| {
        if call(ops::ENT_TYPE, &[e.a()]).as_i() == TYPE_ENEMY {
            call(ops::ENEMY_SET_FEET, &[e.a(), Arg::f(x), Arg::f(y), Arg::f(z)]);
        } else if body_num(e) != 0 {
            call(ops::BODY_SET_WORLD_POS, &[Arg::ptr(body(e, 0)), Arg::f(x), Arg::f(y), Arg::f(z)]);
        }
    });
});

fn entity_pos(name: &[u8], axis: i32) -> f32 {
    let Some(e) = entity(name, TYPE_ANY, -1) else {
        return 0.0;
    };
    if call(ops::ENT_TYPE, &[e.a()]).as_i() == TYPE_ENEMY {
        return call(ops::ENEMY_FEET, &[e.a(), Arg::i(axis)]).as_f();
    }
    if body_num(e) == 0 {
        error(&format!(
            "Could not get position of entity '{}' because it has no physics body!\n",
            lossy(name)
        ));
        return 0.0;
    }
    call(ops::BODY_WORLD_POS, &[Arg::ptr(body(e, 0)), Arg::i(axis)]).as_f()
}

sf!(GetEntityPosX(name: S) -> f32 {
    entity_pos(name.0, 0)
});

sf!(GetEntityPosY(name: S) -> f32 {
    entity_pos(name.0, 1)
});

sf!(GetEntityPosZ(name: S) -> f32 {
    entity_pos(name.0, 2)
});

sf!(SetEntityCustomFocusCrossHair(name: S, cross: S) {
    each(name.0, TYPE_ANY, -1, |e| {
        let mut c = tables::crosshair(cross.0);
        if c < 0 {
            error(&format!("CrossHair type '{}' does not exist!\n", lossy(cross.0)));
            c = 0;
        }
        call(ops::ENT_CROSSHAIR, &[e.a(), Arg::i(c)]);
    });
});

sf!(CreateEntityAtArea(name: S, file: S, area: S, full_save: bool) {
    let a = entity(area.0, TYPE_AREA, -1);
    let Some(a) = a else { return };
    let m = matrix_of(ops::AREA_MATRIX, a.0);
    create_entity(name.0, file.0, &m, full_save);
});

sf!(CreateEntityAtAreaExt(
    name: S, file: S, area: S, full_save: bool, px: f32, py: f32, pz: f32, rx: f32, ry: f32, rz: f32
) {
    let Some(a) = entity(area.0, TYPE_AREA, -1) else { return };
    let local = transform([px, py, pz], [rx, ry, rz]);
    let m = math::matrix_mul(&matrix_of(ops::AREA_MATRIX, a.0), &local);
    create_entity(name.0, file.0, &m, full_save);
});

sf!(ReplaceEntity(name: S, body_name: S, new_name: S, new_file: S, full_save: bool) {
    let e = entity(name.0, TYPE_ANY, -1);
    let Some(e) = e else { return };
    let Some(b) = body_in(Some(e), body_name.0) else { return };
    let m = matrix_of(ops::BODY_MATRIX, b);
    call(ops::MAP_DESTROY_ENTITY, &[e.a()]);
    create_entity(new_name.0, new_file.0, &m, full_save);
});

sf!(PlaceEntityAtEntity(name: S, target: S, target_body: S, use_rotation: bool) {
    let Some(e) = entity(name.0, TYPE_ANY, -1) else { return };
    if body_num(e) == 0 {
        error(&format!(
            "Entity '{}' has no body that can be placed at '{}'!\n",
            lossy(name.0),
            lossy(target.0)
        ));
        return;
    }
    let Some(b) = body_in(Some(e), b"") else { return };
    let t = entity(target.0, TYPE_ANY, -1);
    let Some(tb) = body_in(t, target_body.0) else { return };
    let m = if use_rotation {
        matrix_of(ops::BODY_MATRIX, tb)
    } else {
        let mut m = matrix_of(ops::BODY_MATRIX, b);
        let p = vec_of(ops::BODY_WORLD_POS, tb);
        m.m[0][3] = p.x;
        m.m[1][3] = p.y;
        m.m[2][3] = p.z;
        m
    };
    call(ops::BODY_SET_MATRIX, &[Arg::ptr(b), mat_arg(&m)]);
});

macro_rules! set_each {
    ($name:ident($($a:ident: $t:ty),*), $ty:expr, $sub:expr, $op:expr) => {
        sf!($name(name: S $(, $a: $t)*) {
            each(name.0, $ty, $sub, |e| { call($op, &[e.a() $(, <$t as P>::arg(&$a))*]); });
        });
    };
}
pub(super) use set_each;

set_each!(SetEntityPlayerLookAtCallback(cb: S, remove: bool), TYPE_ANY, -1, ops::ENT_LOOK_AT_CB);
set_each!(SetEntityPlayerInteractCallback(cb: S, remove: bool), TYPE_ANY, -1, ops::ENT_INTERACT_CB);
set_each!(SetEntityCallbackFunc(cb: S), TYPE_ANY, -1, ops::ENT_CALLBACK_FUNC);
set_each!(SetEntityConnectionStateChangeCallback(cb: S), TYPE_ANY, -1, ops::ENT_CONNECTION_CB);
set_each!(SetEntityInteractionDisabled(off: bool), TYPE_PROP, -1, ops::PROP_INTERACTION_DISABLED);

sf!(GetEntitiesCollide(a: S, b: S) -> bool {
    let Some(ea) = entity(a.0, TYPE_ANY, -1) else { return false };
    let Some(eb) = entity(b.0, TYPE_ANY, -1) else { return false };
    call(ops::ENT_COLLIDE, &[ea.a(), eb.a()]).as_b()
});

sf!(AddEntityCollideCallback(name: S, child: S, func: S, delete: bool, states: i32) {
    let Some(c) = entity(child.0, TYPE_ANY, -1) else { return };
    if name.0 == b"Player" {
        call(ops::P_ADD_COLLIDE_CB, &[c.a(), s(func.0), Arg::b(delete), Arg::i(states)]);
    } else {
        each(name.0, TYPE_ANY, -1, |e| {
            call(ops::ENT_ADD_COLLIDE_CB, &[e.a(), c.a(), s(func.0), Arg::b(delete), Arg::i(states)]);
        });
    }
});

sf!(RemoveEntityCollideCallback(name: S, child: S) {
    entity(child.0, TYPE_ANY, -1);
    if name.0 == b"Player" {
        call(ops::P_REMOVE_COLLIDE_CB, &[s(child.0)]);
    } else {
        each(name.0, TYPE_ANY, -1, |e| { call(ops::ENT_REMOVE_COLLIDE_CB, &[e.a(), s(child.0)]); });
    }
});

fn to_coord(b: *const c_void, v: Vector3f, coord: &[u8]) -> Vector3f {
    if !tables::coord_is_local(coord) {
        return v;
    }
    let mut m = matrix_of(ops::BODY_LOCAL_MATRIX, b);
    m.m[0][3] = 0.0;
    m.m[1][3] = 0.0;
    m.m[2][3] = 0.0;
    m.m[3] = [0.0, 0.0, 0.0, 1.0];
    math::matrix_mul_vec(&m, v)
}

fn push_body(op: ops::Op, b: *const c_void, v: Vector3f) {
    call(op, &[Arg::ptr(b), Arg::f(v.x), Arg::f(v.y), Arg::f(v.z)]);
}

fn prop_bodies(name: &[u8], op: ops::Op, v: Vector3f, coord: &[u8]) {
    let Some(p) = entity(name, TYPE_PROP, -1) else {
        return;
    };
    for i in 0..body_num(p) {
        let b = body(p, i);
        push_body(op, b, to_coord(b, v, coord));
    }
}

sf!(AddPropForce(name: S, x: f32, y: f32, z: f32, coord: S) {
    prop_bodies(name.0, ops::BODY_ADD_FORCE, v3(x, y, z), coord.0);
});

sf!(AddPropImpulse(name: S, x: f32, y: f32, z: f32, coord: S) {
    prop_bodies(name.0, ops::BODY_ADD_IMPULSE, v3(x, y, z), coord.0);
});

fn named_body(name: &[u8]) -> Option<*const c_void> {
    let b = call(ops::MAP_PHYS_BODY, &[s(name)]).p;
    if b.is_null() {
        error(&format!("Could not find body '{}'!\n", lossy(name)));
        return None;
    }
    Some(b)
}

sf!(AddBodyForce(name: S, x: f32, y: f32, z: f32, coord: S) {
    if let Some(b) = named_body(name.0) {
        push_body(ops::BODY_ADD_FORCE, b, to_coord(b, v3(x, y, z), coord.0));
    }
});

sf!(AddBodyImpulse(name: S, x: f32, y: f32, z: f32, coord: S) {
    if let Some(b) = named_body(name.0) {
        push_body(ops::BODY_ADD_IMPULSE, b, to_coord(b, v3(x, y, z), coord.0));
    }
});

sf!(BreakJoint(name: S) {
    let j = call(ops::MAP_PHYS_JOINT, &[s(name.0)]).p;
    if j.is_null() {
        error(&format!("Could not find joint '{}'!\n", lossy(name.0)));
        return;
    }
    call(ops::JOINT_BREAK, &[Arg::ptr(j)]);
});

sf!(SetBodyMass(name: S, mass: f32) {
    if let Some(b) = named_body(name.0) {
        call(ops::BODY_SET_MASS, &[Arg::ptr(b), Arg::f(mass)]);
    }
});

sf!(GetBodyMass(name: S) -> f32 {
    named_body(name.0).map_or(0.0, |b| call(ops::BODY_MASS, &[Arg::ptr(b)]).as_f())
});

sf!(InteractConnectPropWithRope(
    name: S, prop: S, rope: S, only: bool, speed_mul: f32, min_speed: f32, max_speed: f32, invert: bool, states: i32
) {
    let Some(p) = entity(prop.0, TYPE_PROP, -1) else { return };
    let r = call(ops::MAP_PHYS_ROPE, &[s(rope.0)]).p;
    if r.is_null() {
        error(&format!("Could not find rope '{}'!\n", lossy(rope.0)));
        return;
    }
    call(ops::PROP_CONNECT_ROPE, &[
        p.a(), Arg::ptr(r), s(name.0), Arg::f(speed_mul), Arg::f(-min_speed.abs()), Arg::f(max_speed.abs()),
        Arg::b(invert), Arg::i(states), Arg::b(only),
    ]);
});

sf!(InteractConnectPropWithMoveObject(name: S, prop: S, move_obj: S, only: bool, invert: bool, states: i32) {
    let Some(p) = entity(prop.0, TYPE_PROP, -1) else { return };
    let Some(m) = entity(move_obj.0, TYPE_PROP, MOVE_OBJECT) else { return };
    call(ops::PROP_CONNECT_MOVE_OBJECT, &[p.a(), m.a(), s(name.0), Arg::b(invert), Arg::i(states), Arg::b(only)]);
});

sf!(ConnectEntities(name: S, main: S, connect: S, invert: bool, states: i32, cb: S) {
    let Some(m) = entity(main.0, TYPE_ANY, -1) else { return };
    let Some(c) = entity(connect.0, TYPE_ANY, -1) else { return };
    call(ops::ENT_ADD_CONNECTION, &[m.a(), s(name.0), c.a(), Arg::b(invert), Arg::i(states), s(cb.0)]);
});

pub(super) const MOVE_OBJECT: i32 = 6;
