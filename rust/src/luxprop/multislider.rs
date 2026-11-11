//! `cLuxProp_MultiSlider`: evenly spaced slider states, stuck state and the pull to the current one.

use super::object::{MoveData, load_slide};
use super::ops::{Op, Ops, Probe, op};
use super::vars::{Vars, put_var};
use crate::math::dot3;

pub const SLOT_CHANGE_STATE_SOUND: i32 = 80;
pub const SLOT_STUCK_SOUND: i32 = 81;
pub const SLOT_CHANGE_STATE_CALLBACK: i32 = 82;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MultiParams {
    pub data: MoveData,
    pub num_of_states: i32,
    pub few_states: bool,
    pub stick_to_state_max_dist: f32,
    pub can_interact_static: bool,
    pub auto_move_to_current_state: bool,
    pub auto_move_speed_factor: f32,
    pub auto_move_max_speed: f32,
}

pub fn load(v: &dyn Vars, out: &mut MultiParams) {
    load_slide(v, &mut out.data);
    out.num_of_states = v.int("NumOfStates", 2);
    out.few_states = out.num_of_states < 2;
    out.stick_to_state_max_dist = v.float("StickToStateMaxDist", 0.1);
    out.can_interact_static = v.boolean("CanInteractWithStaticBody", true);
    out.auto_move_to_current_state = v.boolean("AutoMoveToCurrentState", true);
    out.auto_move_speed_factor = v.float("AutoMoveSpeedFactor", 2.0);
    out.auto_move_max_speed = v.float("AutoMoveMaxSpeed", 8.0);
    put_var(v, SLOT_CHANGE_STATE_SOUND, "ChangeStateSound");
    put_var(v, SLOT_STUCK_SOUND, "StuckSound");
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MultiState {
    pub current_state: i32,
    pub stuck_state: i32,
    pub stuck_sound_timer: f32,
    pub interaction_disables_stuck: bool,
    pub default_min_dist: f32,
    pub default_max_dist: f32,
}

impl MultiState {
    pub fn new() -> Self {
        Self {
            current_state: -1,
            stuck_state: -1,
            ..Self::default()
        }
    }
}

pub fn can_interact(p: &MultiParams, mass: f32) -> bool {
    !(mass == 0.0 && !p.can_interact_static)
}

/// The slider position of each state. A negative state count is a C++ `resize` of a huge
/// size, a length error; here it gives no states.
pub fn compute_states(min: f32, max: f32, n: i32) -> Vec<f32> {
    let count = usize::try_from(n).unwrap_or(0);
    let add = (max - min) / ((n.wrapping_sub(1)) as f32);
    let mut pos = min;
    let mut out = Vec::with_capacity(count);
    for _ in 0..count {
        out.push(pos);
        pos += add;
    }
    out
}

fn change_state(s: &mut MultiState, state: i32, effects: bool, ops: &mut Ops) {
    if s.current_state == state {
        return;
    }
    s.current_state = state;
    ops.push(Op::new(op::CHANGE_SOUND).j(i32::from(effects)));
    ops.push(Op::new(op::RUN_CHANGE_CALLBACK).i(state));
}

/// An out-of-range stuck state indexes past the C++ vector; here the position reads 0.
pub fn set_stuck_state(
    s: &mut MultiState,
    states: &[f32],
    state: i32,
    effects: bool,
    ops: &mut Ops,
) {
    if s.stuck_state == state {
        return;
    }
    s.stuck_state = state;
    s.stuck_sound_timer = 0.0;
    if state >= 0 {
        change_state(s, state, effects, ops);
        let pos = usize::try_from(state)
            .ok()
            .and_then(|i| states.get(i))
            .copied()
            .unwrap_or(0.0);
        let last = states.len() as i32 - 1;
        let min = if state == 0 { pos } else { pos - 0.05f32 };
        let max = if state == last { pos } else { pos + 0.05f32 };
        ops.push(Op::new(op::SLIDER_MIN).f(min));
        ops.push(Op::new(op::SLIDER_MAX).f(max));
    } else {
        ops.push(Op::new(op::SLIDER_MIN).f(s.default_min_dist));
        ops.push(Op::new(op::SLIDER_MAX).f(s.default_max_dist));
    }
    ops.push(Op::new(op::SLIDER_WAKE));
}

pub fn on_interact(s: &mut MultiState, states: &[f32], ops: &mut Ops) {
    if s.stuck_state != 0 && s.interaction_disables_stuck {
        set_stuck_state(s, states, -1, true, ops);
    }
}

pub fn update(
    s: &mut MultiState,
    p: &MultiParams,
    states: &[f32],
    dt: f32,
    interacted: bool,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    let sl = probe.slider();
    let dist = sl.distance;
    if s.stuck_state >= 0 && interacted {
        if s.stuck_sound_timer > 0.0 {
            s.stuck_sound_timer -= dt;
        } else if super::base::sqr_len(sl.child_lin_vel) > 0.01f32 {
            ops.push(Op::new(op::PLAY_SOUND).i(op::SND_MULTI_STUCK));
            s.stuck_sound_timer = 1.5;
        }
    }

    if s.stuck_state < 0 {
        for (i, pos) in states.iter().enumerate() {
            if i as i32 == s.current_state {
                continue;
            }
            if (dist - pos).abs() <= p.stick_to_state_max_dist {
                change_state(s, i as i32, true, ops);
                break;
            }
        }
    }

    if interacted || !p.auto_move_to_current_state || s.stuck_state != -1 || s.current_state == -1 {
        return;
    }
    let goal = usize::try_from(s.current_state)
        .ok()
        .and_then(|i| states.get(i))
        .copied()
        .unwrap_or(0.0);
    let mut wanted = p.auto_move_speed_factor * (goal - dist);
    if wanted.abs() < 0.003f32 {
        return;
    }
    if wanted > p.auto_move_max_speed {
        wanted = p.auto_move_max_speed;
    }
    if wanted < -p.auto_move_max_speed {
        wanted = -p.auto_move_max_speed;
    }
    let sl = probe.slider();
    let speed = dot3(sl.pin_dir, sl.child_lin_vel);
    ops.push(
        Op::new(op::SLIDER_FORCE)
            .v(sl.pin_dir)
            .f2(wanted - speed, dt),
    );
}

pub fn load_instance(v: &dyn Vars, s: &mut MultiState, states: &[f32], ops: &mut Ops) {
    let stuck = v.int("StuckState", -1);
    set_stuck_state(s, states, stuck, false, ops);
    s.interaction_disables_stuck = v.boolean("InteractionDisablesStuck", false);
    put_var(v, SLOT_CHANGE_STATE_CALLBACK, "ChangeStateCallback");
}
