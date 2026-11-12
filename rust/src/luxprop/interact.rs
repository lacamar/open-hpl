//! `cLuxInteractConnection_Rope` and `_MoveObject`: how a turned wheel, a limit or a moving object
//! maps onto the rope length, the wheel angle and the move state.

use crate::math::fma;

fn flip(t: f32, invert: bool) -> f32 {
    if invert { 1.0f32 - t } else { t }
}

/// `Rope::UpdateProp`: the wheel angle for the current rope length.
pub fn rope_wheel_angle(
    length: f32,
    min_len: f32,
    max_len: f32,
    invert: bool,
    min_limit: f32,
    max_limit: f32,
) -> f32 {
    let t = flip((length - min_len) / (max_len - min_len), invert);
    fma(t, max_limit - min_limit, min_limit)
}

/// `Rope::OnTurn`: the wanted rope length.
pub fn rope_turn_length(t: f32, invert: bool, min_len: f32, max_len: f32) -> f32 {
    fma(max_len - min_len, flip(t, invert), min_len)
}

/// The state a limit message acts on, or `None` when it is ignored.
fn limit_state(is_wheel: bool, state: i32, used: i32, invert: bool) -> Option<i32> {
    if is_wheel || state == 0 || (used != 0 && used != state) {
        return None;
    }
    Some(if invert { state.wrapping_neg() } else { state })
}

/// `Rope::OnLimit`: the wanted rope length.
pub fn rope_limit_length(
    is_wheel: bool,
    state: i32,
    used: i32,
    invert: bool,
    min_len: f32,
    max_len: f32,
) -> Option<f32> {
    let s = limit_state(is_wheel, state, used, invert)?;
    Some(match s {
        1 => max_len,
        -1 => min_len,
        _ => 0.0,
    })
}

/// `MoveObject::UpdateProp` gate: whether the prop is updated, clearing the start-up flag.
pub fn move_update_gate(interaction_only: bool, moving: bool, needs_update: &mut bool) -> bool {
    if !interaction_only || (!moving && !*needs_update) {
        return false;
    }
    *needs_update = false;
    true
}

/// `MoveObject::UpdateProp`: the wheel angle for the move state.
pub fn move_wheel_angle(t: f32, invert: bool, min_limit: f32, max_limit: f32) -> f32 {
    fma(flip(t, invert), max_limit - min_limit, min_limit)
}

/// `MoveObject::OnTurn`: the state to move to.
pub fn move_turn_state(t: f32, invert: bool) -> f32 {
    flip(t, invert)
}

/// `MoveObject::OnLimit`: the state to move to.
pub fn move_limit_state(is_wheel: bool, state: i32, used: i32, invert: bool) -> Option<f32> {
    let s = limit_state(is_wheel, state, used, invert)?;
    Some(if s > 0 { 1.0 } else { 0.0 })
}
