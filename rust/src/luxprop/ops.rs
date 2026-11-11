//! The op list shared by every prop: C++ applies each op in order, as it is issued. Re-entrant ops
//! (scripts, entity creation) go through `Ops::event`, which hands the state over and re-reads it.

use crate::types::{Matrixf, Vector3f};

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Op {
    pub code: i32,
    pub i: i32,
    pub j: i32,
    pub f: [f32; 4],
    pub v: Vector3f,
    pub m: Matrixf,
}

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Op>() == 104 && offset_of!(Op, v) == 28 && offset_of!(Op, m) == 40);
};

impl Op {
    pub fn new(code: i32) -> Self {
        Self {
            code,
            ..Self::default()
        }
    }

    pub fn i(mut self, i: i32) -> Self {
        self.i = i;
        self
    }

    pub fn j(mut self, j: i32) -> Self {
        self.j = j;
        self
    }

    pub fn f(mut self, a: f32) -> Self {
        self.f[0] = a;
        self
    }

    pub fn f2(mut self, a: f32, b: f32) -> Self {
        self.f[0] = a;
        self.f[1] = b;
        self
    }

    pub fn v(mut self, v: Vector3f) -> Self {
        self.v = v;
        self
    }

    pub fn m(mut self, m: Matrixf) -> Self {
        self.m = m;
        self
    }
}

pub struct Ops<'a>(pub &'a mut dyn FnMut(&Op));

impl Ops<'_> {
    pub fn push(&mut self, op: Op) {
        (self.0)(&op);
    }

    /// A re-entrant op (script, entity creation): C++ runs it right here and the script may call
    /// back into this prop. No reference to the prop's state may be live across it; callers use
    /// `Pod` handles and borrow only between events.
    pub fn event(&mut self, op: Op) {
        (self.0)(&op);
    }
}

/// A handle to prop state owned by C++. `m` gives a short-lived borrow, valid only until the next
/// event; no borrow is ever held across one.
pub struct Pod<T>(std::ptr::NonNull<T>);

impl<T> Clone for Pod<T> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<T> Copy for Pod<T> {}

impl<T> Pod<T> {
    pub fn new(r: &mut T) -> Self {
        Self(std::ptr::NonNull::from(r))
    }

    /// # Safety
    /// `p` is non-null and points to live state for the whole call.
    pub unsafe fn from_ptr(p: *mut T) -> Self {
        // SAFETY: caller contract.
        Self(unsafe { std::ptr::NonNull::new_unchecked(p) })
    }

    /// A shared borrow of the state, for read-only data such as the settings.
    pub fn r(&self) -> &T {
        // SAFETY: as in `m`; C++ only reads this data while a script runs.
        unsafe { &*self.0.as_ptr() }
    }

    #[expect(
        clippy::mut_from_ref,
        reason = "short borrow of C++-owned state, see the type docs"
    )]
    pub fn m(&self) -> &mut T {
        // SAFETY: the handle points to live state; callers keep the borrow within one statement
        // and never across an event.
        unsafe { &mut *self.0.as_ptr() }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Hinge {
    pub angle: f32,
    pub min_angle: f32,
    pub max_angle: f32,
    pub pin_dir: Vector3f,
    pub pivot: Vector3f,
    pub child: Matrixf,
    pub child_ang_vel: Vector3f,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Slider {
    pub distance: f32,
    pub pin_dir: Vector3f,
    pub child_lin_vel: Vector3f,
    pub child_mass: f32,
}

/// Pure engine queries Rust needs mid-update. They never call back into a prop.
pub trait Probe {
    /// `CheckShapeWorldCollision` push vector for the main body moved by `add`.
    fn move_push(&mut self, add: Vector3f) -> Vector3f;
    /// Does body `i` overlap the player's body?
    fn outside_collides(&mut self, i: i32) -> bool;
    fn insanity_in_fov(&mut self) -> bool;
    /// The `i`th hinge joint (of the door's hinge list, or the single hinge of a wheel/lever).
    fn hinge(&mut self, i: i32) -> Hinge;
    fn slider(&mut self) -> Slider;
}

pub mod op {
    pub const FX_PS_CREATE_ALL: i32 = 1;
    pub const FX_LIGHT_FADE_ON: i32 = 2;
    pub const FX_LIGHT_SET_ON: i32 = 3;
    pub const FX_SOUND_FADE_IN: i32 = 4;
    pub const FX_SOUND_PLAY: i32 = 5;
    pub const FX_PS_KILL_ALL: i32 = 6;
    pub const FX_LIGHT_OFF: i32 = 7;
    pub const FX_SOUND_FADE_OUT: i32 = 8;
    pub const FX_SOUND_STOP: i32 = 9;
    pub const FX_CHANGE_SOUND: i32 = 10;
    pub const FX_LIGHT_FLICKER: i32 = 11;
    pub const FX_BILLBOARDS: i32 = 12;
    pub const FX_MESH_ILLUM: i32 = 13;
    pub const COVERAGE: i32 = 14;
    pub const SND_MOVE_START: i32 = 15;
    pub const SND_MOVE_STOP: i32 = 16;
    pub const MOVE_LOOP_START: i32 = 17;
    pub const MOVE_LOOP_VOLUME: i32 = 18;
    pub const MOVE_LOOP_KILL: i32 = 19;
    pub const STATIC_LINEAR_MOVE: i32 = 20;
    pub const STATIC_ANGULAR_MOVE: i32 = 21;
    pub const ON_START_MOVE: i32 = 22;
    pub const OUTSIDE_IMPULSE: i32 = 23;
    pub const OUTSIDE_COLLIDE_CHAR: i32 = 24;
    /// Re-entrant: `InteractConnectionLimit(i)`.
    pub const CONN_LIMIT: i32 = 25;
    /// Re-entrant: `InteractConnectionTurn` with `f = [angle, prev, min, max]`.
    pub const CONN_TURN: i32 = 26;
    /// Re-entrant: `ConnectionStateChange(i)`.
    pub const CONN_STATE: i32 = 27;
    /// Re-entrant: run the animation-over callback.
    pub const RUN_ANIM_CALLBACK: i32 = 28;
    /// `PlaySound` of the named slot `i` (see `SND_*`).
    pub const PLAY_SOUND: i32 = 29;
    /// Re-entrant: `Break()`.
    pub const BREAK: i32 = 30;
    pub const SET_JOINT_MINMAX: i32 = 31;
    pub const JOINT_WAKE: i32 = 32;
    pub const INSANITY_VISIBLE: i32 = 33;
    /// Re-entrant: enemies react to the food.
    pub const FOOD_ATTRACT: i32 = 34;
    pub const HINGE_MIN: i32 = 35;
    pub const HINGE_MAX: i32 = 36;
    pub const HINGE_STICKY: i32 = 37;
    pub const BODIES_WAKE: i32 = 38;
    pub const DOOR_BODY_MATRIX: i32 = 39;
    pub const DOOR_DAMAGE_LEVEL: i32 = 40;
    pub const DOOR_DISABLE_BODIES: i32 = 41;
    /// Re-entrant: spawn the broken entity.
    pub const DOOR_BROKEN_ENTITY: i32 = 42;
    pub const DOOR_EFFECTS: i32 = 43;
    pub const PID_TORQUE: i32 = 44;
    pub const PID_RESET: i32 = 45;
    pub const BODY_ANG_VEL: i32 = 46;
    pub const SLIDER_MIN: i32 = 47;
    pub const SLIDER_MAX: i32 = 48;
    pub const SLIDER_WAKE: i32 = 49;
    pub const SLIDER_FORCE: i32 = 50;
    /// Re-entrant: `RunScript(callback(name, state))`.
    pub const RUN_CHANGE_CALLBACK: i32 = 51;
    pub const CHANGE_SOUND: i32 = 52;
    pub const LIMIT_SOUND: i32 = 53;
    pub const SET_EFFECTS: i32 = 54;
    /// Re-entrant: `RunCallbackFunc("Break")`.
    pub const RUN_BREAK_CALLBACK: i32 = 55;
    pub const DOOR_DESTROY_BROKEN: i32 = 56;
    pub const BODY_ZERO_VEL: i32 = 57;
    pub const MOVE_STATE_DONE: i32 = 58;

    pub const SND_CLOSE_ON: i32 = 0;
    pub const SND_CLOSE_OFF: i32 = 1;
    pub const SND_LOCK_ON: i32 = 2;
    pub const SND_LOCK_OFF: i32 = 3;
    pub const SND_INTERACT_LOCKED: i32 = 4;
    pub const SND_WHEEL_STUCK: i32 = 5;
    pub const SND_LEVER_STUCK: i32 = 6;
    pub const SND_MULTI_STUCK: i32 = 7;
}
