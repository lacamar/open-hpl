//! The rest of `iLuxEntity`: callback scripts, the look-at update, connection fan-out, activation,
//! the collide-with-player test and the save/preload decisions. Scripts and engine calls stay in
//! C++ as ops; the script text is built there from the entity's own strings.

use super::entity::{self, LookIn, Los};
use super::opc as op_c;
use super::ops::{Op, Ops, Pod};

/// The flags of `iLuxEntity` the callbacks read and write; they sit in C++ next to the strings.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EntityCb {
    pub interact_remove: bool,
    pub lookat_remove: bool,
    pub lookat_count: f32,
    pub is_looked_at: bool,
}

/// `RunCallbackFunc`: the script runs when the callback name is set.
pub fn run_callback(has: bool, ops: &mut Ops) {
    if has {
        ops.event(Op::new(op_c::ENT_CALLBACK));
    }
}

/// `RunInteractCallbackFunc`: the script may re-arm the callback, which the removal then clears.
pub fn run_interact(cb: Pod<EntityCb>, has: bool, ops: &mut Ops) {
    if !has {
        return;
    }
    ops.event(Op::new(op_c::ENT_INTERACT));
    if cb.r().interact_remove {
        ops.push(Op::new(op_c::ENT_CLEAR_INTERACT));
    }
}

/// `SetActive`: true when `OnSetActive` runs.
pub fn set_active(active: &mut bool, x: bool) -> bool {
    if *active == x {
        return false;
    }
    *active = x;
    true
}

pub const ATTACH_NONE: i32 = 0;
pub const ATTACH_CHILD: i32 = 1;
pub const ATTACH_POSITION: i32 = 2;

/// `PlaySound` after the sound exists: attach it to the entity or only move it there.
pub fn play_sound_attach(sound: bool, attach_entity: bool, attach: bool) -> i32 {
    if !(sound && attach_entity) {
        ATTACH_NONE
    } else if attach {
        ATTACH_CHILD
    } else {
        ATTACH_POSITION
    }
}

pub trait LookHost {
    fn body_num(&mut self) -> i32;
    /// Camera and bounds of body `i`; false when the frustum misses it.
    fn body(&mut self, i: i32) -> Option<LookIn>;
}

/// `UpdatePlayerLookAt`.
pub fn update_look_at(
    cb: Pod<EntityCb>,
    has_callback: bool,
    dt: f32,
    host: &mut dyn LookHost,
    los: &mut dyn Los,
    ops: &mut Ops,
) {
    if !has_callback {
        return;
    }
    if !entity::look_timer(&mut cb.m().lookat_count, dt) {
        return;
    }
    let mut looking = false;
    let mut i = 0;
    while i < host.body_num() {
        if let Some(input) = host.body(i)
            && entity::look_body(&input, los)
        {
            looking = true;
            break;
        }
        i += 1;
    }
    let was = cb.r().is_looked_at;
    if looking && !was {
        ops.event(Op::new(op_c::ENT_LOOKAT_ENTER).i(i32::from(cb.r().lookat_remove)));
    } else if !looking && was {
        ops.event(Op::new(op_c::ENT_LOOKAT_LEAVE));
    }
    cb.m().is_looked_at = looking;
}

pub trait ConnHost {
    fn conn_num(&mut self) -> i32;
    /// `(states used, invert)` of connection `i`.
    fn conn(&mut self, i: i32) -> (i32, bool);
    fn conn_has_callback(&mut self, i: i32) -> bool;
}

/// `ConnectionStateChange`: the state callback, then every connection that passes the filter.
pub fn connection_state_change(
    state: i32,
    has_change_callback: bool,
    host: &mut dyn ConnHost,
    ops: &mut Ops,
) {
    if has_change_callback {
        ops.event(Op::new(op_c::ENT_CONN_CHANGE_CB).i(state));
    }
    let mut i = 0;
    while i < host.conn_num() {
        let (used, invert) = host.conn(i);
        if let Some(sent) = entity::conn_state(state, used, invert) {
            ops.event(Op::new(op_c::ENT_CONN_SEND).i(i).j(sent));
            if host.conn_has_callback(i) {
                ops.event(Op::new(op_c::ENT_CONN_SCRIPT).i(i).j(sent));
            }
        }
        i += 1;
    }
}

pub trait CollideHost {
    fn body_num(&mut self) -> i32;
    fn bv_intersects(&mut self, i: i32) -> bool;
    fn shape_collides(&mut self, i: i32) -> bool;
}

/// `CollidesWithPlayer`.
pub fn collides_with_player(host: &mut dyn CollideHost) -> bool {
    let mut i = 0;
    while i < host.body_num() {
        if host.bv_intersects(i) && host.shape_collides(i) {
            return true;
        }
        i += 1;
    }
    false
}

/// `PreloadEntityModel` after the document load: continue only with a document.
pub fn preload_has_doc(doc: bool) -> bool {
    doc
}

/// `PreloadEntityModel` after the mesh load: true keeps the mesh, false reports the error.
pub fn preload_has_mesh(mesh: bool) -> bool {
    mesh
}

/// `cLuxEntityConnection_SaveData::ToConnection`: true reports the missing entity.
pub fn connection_missing(found: bool) -> bool {
    !found
}
