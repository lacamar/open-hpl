//! `cAgentStateMachine`: state/sub-state transitions and timers as a resumable stepper. Each
//! script call (Leave, Enter, Update, TimerUp, ...) is returned as an `Action`; C++ runs it and
//! asks for the next one, so effects of a script (a new sub-state, a timer) are seen by the
//! phases after it exactly as when the calls were made inline. A script may start a nested
//! update or message (`OnMessage`), so steppers live on a stack.

pub const ACT_LEAVE: u32 = 1;
pub const ACT_ENTER: u32 = 2;
pub const ACT_UPDATE: u32 = 3;
pub const ACT_TIMER_UP: u32 = 4;
pub const ACT_SUB_STATE_OVER: u32 = 5;
pub const ACT_MESSAGE: u32 = 6;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Action {
    pub kind: u32,
    pub sub: bool,
    pub state: i32,
    pub arg_i: i32,
    pub arg_f: f32,
    pub arg_u: u64,
}

#[derive(Clone, Copy, Debug)]
struct Timer {
    id: u64,
    time: f32,
}

#[derive(Clone, Copy, Debug, PartialEq)]
enum Phase {
    Idle,
    MainStart,
    MainSwitch,
    MainUpdate,
    MainUpdateDone,
    Timers,
    SubStart,
    SubSwitch,
    SubOver,
    SubUpdate,
    SubUpdateDone,
    MsgMain,
    MsgSub,
}

struct Frame {
    phase: Phase,
    after_timers: Phase,
    dt: f32,
    msg: i32,
    next_state: i32,
    old_sub: i32,
    due: Vec<u64>,
    due_pos: usize,
    timer_sub: bool,
    timer_state: i32,
}

impl Frame {
    fn new(phase: Phase, dt: f32, msg: i32) -> Self {
        Self {
            phase,
            after_timers: Phase::Idle,
            dt,
            msg,
            next_state: -1,
            old_sub: -1,
            due: Vec::new(),
            due_pos: 0,
            timer_sub: false,
            timer_state: -1,
        }
    }

    /// `UpdateTimers`: tick, drop and collect the due ones, then run `after` once they have fired.
    fn begin_timers(&mut self, timers: &mut [Vec<Timer>; 2], sub: bool, state: i32, after: Phase) {
        let dt = self.dt;
        self.due.clear();
        self.due_pos = 0;
        if let Some(t) = timers.get_mut(usize::from(sub)) {
            let due = &mut self.due;
            t.retain_mut(|t| {
                t.time -= dt;
                if t.time <= 0.0 {
                    due.push(t.id);
                    false
                } else {
                    true
                }
            });
        }
        self.timer_sub = sub;
        self.timer_state = state;
        self.after_timers = after;
        self.phase = Phase::Timers;
    }
}

/// Rust-owned half of the state machine; `SmPod::rs` points at it.
#[derive(Default)]
pub struct SmExtra {
    timers: [Vec<Timer>; 2],
    frames: Vec<Frame>,
}

impl SmExtra {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn timer_len(&self, sub: bool) -> usize {
        self.timers.get(usize::from(sub)).map_or(0, Vec::len)
    }

    pub fn clear_timers(&mut self, sub: bool) {
        if let Some(t) = self.timers.get_mut(usize::from(sub)) {
            t.clear();
        }
    }

    pub fn add_timer(&mut self, sub: bool, id: u64, time: f32) {
        if let Some(t) = self.timers.get_mut(usize::from(sub)) {
            t.push(Timer { id, time });
        }
    }

    pub fn stop_timer(&mut self, sub: bool, id: u64) {
        if let Some(t) = self.timers.get_mut(usize::from(sub)) {
            t.retain(|t| t.id != id);
        }
    }

    pub fn timer_exists(&self, sub: bool, id: u64) -> bool {
        self.timers
            .get(usize::from(sub))
            .is_some_and(|t| t.iter().any(|t| t.id == id))
    }
}

/// The part C++ reads and writes in place.
#[repr(C)]
pub struct SmPod {
    pub cur: i32,
    pub prev: i32,
    pub next: i32,
    pub sub_cur: i32,
    pub sub_prev: i32,
    pub sub_next: i32,
    pub in_sub: bool,
    pub rs: *mut SmExtra,
}

fn act(kind: u32, sub: bool, state: i32) -> Action {
    Action {
        kind,
        sub,
        state,
        ..Action::default()
    }
}

impl SmPod {
    pub fn reset(&mut self, has_states: bool) {
        self.cur = -1;
        self.prev = -1;
        self.sub_cur = -1;
        self.sub_prev = -1;
        self.sub_next = -1;
        self.next = if has_states { 0 } else { -1 };
        self.in_sub = false;
        // SAFETY: `rs` is null or the Rust half this POD owns.
        if let Some(x) = unsafe { self.rs.as_mut() } {
            x.clear_timers(false);
            x.clear_timers(true);
        }
    }

    fn push(&mut self, f: Frame) {
        // SAFETY: as in `reset`.
        if let Some(x) = unsafe { self.rs.as_mut() } {
            x.frames.push(f);
        }
    }

    pub fn begin_update(&mut self, dt: f32) {
        self.push(Frame::new(Phase::MainStart, dt, 0));
    }

    pub fn begin_message(&mut self, msg: i32) {
        self.push(Frame::new(Phase::MsgMain, 0.0, msg));
    }

    /// The next script call of the innermost update or message, or `None` once it is complete.
    pub fn next(&mut self, exists: &dyn Fn(bool, i32) -> bool) -> Option<Action> {
        self.in_sub = false;
        // SAFETY: as in `reset`; `self`'s plain fields are read and written alongside.
        let x = unsafe { self.rs.as_mut() }?;
        let (timers, frames) = (&mut x.timers, &mut x.frames);
        loop {
            let f = frames.last_mut()?;
            match f.phase {
                Phase::Idle => {
                    frames.pop();
                    return None;
                }
                Phase::MainStart => {
                    let next = self.next;
                    f.next_state = next;
                    if if next >= 0 {
                        self.cur != next
                    } else {
                        self.cur >= 0
                    } {
                        self.prev = self.cur;
                        f.phase = Phase::MainSwitch;
                        if self.cur >= 0 {
                            return Some(act(ACT_LEAVE, false, self.cur));
                        }
                    } else {
                        f.phase = Phase::MainUpdate;
                    }
                }
                Phase::MainSwitch => {
                    self.cur = if exists(false, f.next_state) {
                        f.next_state
                    } else {
                        -1
                    };
                    self.sub_next = -1;
                    if let Some(t) = timers.first_mut() {
                        t.clear();
                    }
                    f.phase = Phase::MainUpdate;
                    if self.cur >= 0 {
                        return Some(act(ACT_ENTER, false, self.cur));
                    }
                }
                Phase::MainUpdate => {
                    f.phase = Phase::MainUpdateDone;
                    if self.cur >= 0 {
                        return Some(Action {
                            arg_f: f.dt,
                            ..act(ACT_UPDATE, false, self.cur)
                        });
                    }
                }
                Phase::MainUpdateDone => {
                    if self.cur >= 0 {
                        f.begin_timers(timers, false, self.cur, Phase::SubStart);
                    } else {
                        f.phase = Phase::SubStart;
                    }
                }
                Phase::Timers => {
                    if let Some(&id) = f.due.get(f.due_pos) {
                        f.due_pos += 1;
                        return Some(Action {
                            arg_u: id,
                            ..act(ACT_TIMER_UP, f.timer_sub, f.timer_state)
                        });
                    }
                    f.phase = f.after_timers;
                }
                Phase::SubStart => {
                    let next = self.sub_next;
                    f.next_state = next;
                    if if next >= 0 {
                        self.sub_cur != next
                    } else {
                        self.sub_cur >= 0
                    } {
                        f.old_sub = self.sub_cur;
                        self.sub_prev = self.sub_cur;
                        f.phase = Phase::SubSwitch;
                        if f.old_sub >= 0 {
                            return Some(act(ACT_LEAVE, true, f.old_sub));
                        }
                    } else {
                        f.phase = Phase::SubUpdate;
                    }
                }
                Phase::SubSwitch => {
                    self.sub_cur = if exists(true, f.next_state) {
                        f.next_state
                    } else {
                        -1
                    };
                    f.phase = Phase::SubOver;
                    if f.old_sub >= 0 && self.cur >= 0 {
                        return Some(Action {
                            arg_i: f.old_sub,
                            ..act(ACT_SUB_STATE_OVER, false, self.cur)
                        });
                    }
                }
                Phase::SubOver => {
                    if let Some(t) = timers.get_mut(1) {
                        t.clear();
                    }
                    f.phase = Phase::SubUpdate;
                    if self.sub_cur >= 0 {
                        return Some(act(ACT_ENTER, true, self.sub_cur));
                    }
                }
                Phase::SubUpdate => {
                    f.phase = Phase::SubUpdateDone;
                    if self.sub_cur >= 0 {
                        return Some(Action {
                            arg_f: f.dt,
                            ..act(ACT_UPDATE, true, self.sub_cur)
                        });
                    }
                }
                Phase::SubUpdateDone => {
                    f.phase = Phase::Idle;
                    if self.sub_cur >= 0 {
                        f.begin_timers(timers, true, self.sub_cur, Phase::Idle);
                    }
                }
                Phase::MsgMain => {
                    f.phase = Phase::MsgSub;
                    if self.cur >= 0 {
                        return Some(Action {
                            arg_i: f.msg,
                            ..act(ACT_MESSAGE, false, self.cur)
                        });
                    }
                }
                Phase::MsgSub => {
                    f.phase = Phase::Idle;
                    if self.sub_cur >= 0 {
                        return Some(Action {
                            arg_i: f.msg,
                            ..act(ACT_MESSAGE, true, self.sub_cur)
                        });
                    }
                }
            }
        }
    }
}
