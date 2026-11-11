//! Small decisions of the entity: damage, break, animation controller, interact connections, GUI
//! update gating and body-name lookup.

use crate::math::{clamp, fma};
use crate::string;

pub fn damage_amount(amount: f32, strength: i32, toughness: i32) -> f32 {
    let limit = toughness.wrapping_sub(1);
    if strength < limit {
        0.0
    } else if strength == limit {
        amount * 0.5f32
    } else {
        amount
    }
}

pub fn health_breaks(health: f32, disable_breakable: bool) -> bool {
    health <= 0.0 && !disable_breakable
}

#[expect(
    clippy::neg_cmp_op_on_partial_ord,
    reason = "NaN speed and time pass the C++ `<= limit` early-outs"
)]
/// First gate of `cBreakBodyCallback::OnBodyCollide` (`time` and `start` are doubles).
pub fn break_gate(
    breakable: bool,
    broken: bool,
    active: bool,
    speed: f32,
    has_map: bool,
    time: f64,
    start: f64,
) -> bool {
    breakable && !broken && active && !(speed <= 0.0) && has_map && !(time - start <= 0.5)
}

pub fn break_energy_exceeds(
    speed: f32,
    mass: f32,
    other_mass: f32,
    v1: f32,
    v2: f32,
    min_energy: f32,
) -> bool {
    let mass2 = if other_mass > 0.0 { other_mass } else { 1e6f32 };
    let m = if v1 + v2 > 0.0 {
        fma(mass2 - mass, v2 / (v1 + v2), mass)
    } else {
        mass
    };
    0.5f32 * speed * speed * m > min_energy
}

pub const ANIM_NONE: i32 = 0;
pub const ANIM_NEXT: i32 = 1;
pub const ANIM_FINISH: i32 = 2;
pub const ANIM_FINISH_CALLBACK: i32 = 3;

pub fn anim_update_action(
    has_mesh: bool,
    current: i32,
    playing: bool,
    queued: bool,
    has_callback: bool,
) -> i32 {
    if !has_mesh || current < 0 || playing {
        ANIM_NONE
    } else if queued {
        ANIM_NEXT
    } else if has_callback {
        ANIM_FINISH_CALLBACK
    } else {
        ANIM_FINISH
    }
}

pub const PLAY_NONE: i32 = 0;
pub const PLAY_MISSING_ERROR: i32 = 1;
pub const PLAY_FADE: i32 = 2;
pub const PLAY_NOW: i32 = 3;

pub fn play_plan(has_mesh: bool, idx: i32, name_empty: bool, num_states: i32, fade: f32) -> i32 {
    if !has_mesh {
        PLAY_NONE
    } else if idx < 0 {
        if !name_empty && num_states > 0 {
            PLAY_MISSING_ERROR
        } else {
            PLAY_NONE
        }
    } else if fade > 0.0 {
        PLAY_FADE
    } else {
        PLAY_NOW
    }
}

pub const STOP_CLEAR_CURRENT: i32 = 1;
pub const STOP_FADE: i32 = 2;
pub const STOP_OFF: i32 = 4;

pub fn stop_plan(
    has_mesh: bool,
    idx: i32,
    num_states: i32,
    current: i32,
    active: bool,
    fade: f32,
) -> i32 {
    if !has_mesh || idx < 0 || idx >= num_states {
        return 0;
    }
    let mut flags = if idx == current {
        STOP_CLEAR_CURRENT
    } else {
        0
    };
    if active {
        flags |= if fade > 0.0 { STOP_FADE } else { STOP_OFF };
    }
    flags
}

pub fn max_interact_distance(own: f32, default: f32) -> f32 {
    if own > 0.0 { own } else { default }
}

/// `ChangeConnectionState` filter: the state sent to a connection, if it takes this one.
pub fn connection_state(state: i32, states_used: i32, invert: bool) -> Option<i32> {
    if states_used != 0 && state != states_used {
        return None;
    }
    Some(if invert { state.wrapping_neg() } else { state })
}

pub fn interact_connection_input(t: f32, invert: bool) -> f32 {
    clamp(if invert { 1.0f32 - t } else { t }, 0.0, 1.0)
}

pub fn body_name_matches(body: &[u8], want: &[u8]) -> bool {
    !want.is_empty()
        && (body == want || string::file_name(body) == want || {
            let mut suffix = vec![b'_'];
            suffix.extend_from_slice(want);
            body.ends_with(&suffix)
        })
}

/// `UpdateGui` fade: the fade value, speed and active flag after one step.
pub fn gui_fade_step(fade: &mut f32, speed: &mut f32, active: &mut bool, dt: f32) {
    if *speed == 0.0 {
        return;
    }
    *fade = fma(*speed, dt, *fade);
    if *fade <= 0.0 || *fade >= 1.0 {
        *active = *fade > 0.0;
        *fade = if *active { 1.0 } else { 0.0 };
        *speed = 0.0;
    }
}

pub struct GuiGate {
    pub focused: bool,
    pub fps: f32,
    pub update_out_of_view: bool,
    pub has_sub_mesh: bool,
    pub sub_mesh_rendered: bool,
}

/// The step to run the GUI with this frame, or none when the frame is skipped.
pub fn gui_gate(g: &GuiGate, acc: &mut f32, dirty: &mut bool, dt: f32) -> Option<f32> {
    if g.focused {
        return Some(dt);
    }
    *acc += dt;
    if *acc < 1.0f32 / clamp(g.fps, 1.0, 30.0) && !*dirty {
        return None;
    }
    let step = *acc;
    *acc = 0.0;
    *dirty = false;
    if !g.update_out_of_view && g.has_sub_mesh && !g.sub_mesh_rendered {
        return None;
    }
    Some(step)
}
