//! The message queue and state scheduler of `iLuxEnemy`. Handlers may re-enter (`SendMessage`
//! and `ChangeState` from C++), so the queue and state words are only touched through short
//! raw-pointer scopes, never held across a handler call.

use crate::math::Vector3f;

pub const STATE_LAST: i32 = 21;
pub const EV_ENTER: i32 = 0;
pub const EV_LEAVE: i32 = 1;
pub const EV_UPDATE: i32 = 2;
pub const EV_MESSAGE: i32 = 3;

pub const MSG_TIMEOUT: i32 = 0;
pub const MSG_TIMEOUT_2: i32 = 1;
pub const MSG_TIMEOUT_3: i32 = 2;
pub const MSG_ANIMATION_OVER: i32 = 5;
pub const MSG_ANIMATION_SPECIAL_EVENT: i32 = 6;
pub const MSG_END_OF_PATH: i32 = 7;
pub const MSG_PLAYER_SEEN: i32 = 9;
pub const MSG_PLAYER_UNSEEN: i32 = 10;
pub const MSG_PLAYER_DETECTED: i32 = 11;
pub const MSG_PLAYER_UNDETECTED: i32 = 12;
pub const MSG_SOUND_HEARD: i32 = 13;
pub const MSG_PLAYER_IN_RANGE: i32 = 14;
pub const MSG_PLAYER_OUT_OF_RANGE: i32 = 15;
pub const MSG_PLAYER_DEAD: i32 = 16;
pub const MSG_HELP_ME: i32 = 18;
pub const MSG_STUCK_AT_DOOR: i32 = 19;
pub const MSG_TAKE_HIT: i32 = 20;
pub const MSG_RESET: i32 = 22;

/// Layout of `cLuxStateMessage`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Msg {
    pub ty: i32,
    pub time: f32,
    pub count: f32,
    pub custom_v: [f32; 3],
    pub custom_f: f32,
    pub custom_i: i32,
    pub state: i32,
}

#[derive(Default)]
pub struct Queue {
    pub msgs: Vec<Msg>,
}

/// The enemy's three state words, which C++ subclasses read directly.
#[derive(Clone, Copy)]
#[repr(C)]
pub struct StatePtrs {
    pub cur: *mut i32,
    pub next: *mut i32,
    pub prev: *mut i32,
}

impl StatePtrs {
    pub fn cur(&self) -> i32 {
        // SAFETY: C++ keeps the three words alive for the call.
        unsafe { *self.cur }
    }
    pub fn next(&self) -> i32 {
        // SAFETY: as above.
        unsafe { *self.next }
    }
    pub fn prev(&self) -> i32 {
        // SAFETY: as above.
        unsafe { *self.prev }
    }
    pub fn change_state(&self, s: i32) {
        if s == STATE_LAST || self.cur() == s {
            return;
        }
        // SAFETY: as above.
        unsafe { *self.next = s };
    }
}

pub trait Handler {
    fn event(&mut self, state: i32, ev: i32, msg: Option<&Msg>) -> bool;
}

/// `iLuxEnemy::StateEvent`: the state's own handler, then the default section.
pub fn state_event(hd: &mut dyn Handler, state: i32, ev: i32, msg: Option<&Msg>) -> bool {
    hd.event(state, ev, msg) || hd.event(-1, ev, msg)
}

pub fn check_state_change(st: &StatePtrs, hd: &mut dyn Handler) {
    if st.next() == st.cur() {
        return;
    }
    if st.cur() != STATE_LAST {
        state_event(hd, st.cur(), EV_LEAVE, None);
    }
    // SAFETY: C++ keeps the three words alive for the call; nothing else holds a reference.
    unsafe {
        *st.prev = *st.cur;
        *st.cur = *st.next;
    }
    state_event(hd, st.cur(), EV_ENTER, None);
}

fn msg_at(q: *mut Queue, i: usize) -> Option<Msg> {
    // SAFETY: the queue outlives the call and no reference to it is live across a handler.
    unsafe { (&*q).msgs.get(i).copied() }
}

pub fn update_state_machine(st: &StatePtrs, q: *mut Queue, dt: f32, hd: &mut dyn Handler) {
    check_state_change(st, hd);
    state_event(hd, st.cur(), EV_UPDATE, None);
    check_state_change(st, hd);
    let mut i = 0usize;
    while let Some(m) = msg_at(q, i) {
        if m.state != STATE_LAST && m.state != st.cur() {
            queue_erase(q, i);
            continue;
        }
        let mut m = m;
        m.count -= dt;
        // SAFETY: as in `msg_at`.
        unsafe {
            if let Some(s) = (&mut *q).msgs.get_mut(i) {
                s.count = m.count;
            }
        }
        if m.count <= 0.0 {
            state_event(hd, st.cur(), EV_MESSAGE, Some(&m));
            queue_erase(q, i);
            continue;
        }
        i += 1;
    }
    check_state_change(st, hd);
}

fn queue_erase(q: *mut Queue, i: usize) {
    // SAFETY: as in `msg_at`.
    unsafe {
        let v = &mut *q;
        if i < v.msgs.len() {
            v.msgs.remove(i);
        }
    }
}

#[allow(clippy::too_many_arguments)]
pub fn send_message(
    q: *mut Queue,
    cur: i32,
    disabled: bool,
    triggers_disabled: bool,
    ty: i32,
    time: f32,
    local: bool,
    v: Vector3f,
    f: f32,
    i: i32,
) -> bool {
    if disabled {
        return false;
    }
    if triggers_disabled
        && ty > MSG_END_OF_PATH
        && ty != MSG_PLAYER_IN_RANGE
        && ty != MSG_PLAYER_OUT_OF_RANGE
    {
        return false;
    }
    let m = Msg {
        ty,
        time,
        count: time,
        custom_v: [v.x, v.y, v.z],
        custom_f: f,
        custom_i: i,
        state: if local { cur } else { STATE_LAST },
    };
    // SAFETY: as in `msg_at`.
    unsafe { (&mut *q).msgs.push(m) };
    true
}
