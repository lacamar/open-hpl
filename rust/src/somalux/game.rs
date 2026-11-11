//! `SomaLuxGame.cpp`: action routing, GUI pointer mode and text duration.

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Route {
    /// Paused: only user modules hear the action (the menu).
    ModulesOnly,
    All,
}

pub fn route_action(paused: bool) -> Route {
    if paused {
        Route::ModulesOnly
    } else {
        Route::All
    }
}

pub fn analog_allowed(paused: bool) -> bool {
    !paused
}

/// `ForEach` visits the handlers, the active effects (all when `active_only` is off), then the
/// modules.
pub fn visits_effect(effect_active: bool, active_only: bool) -> bool {
    effect_active || !active_only
}

/// `max(min_time, start_time + chars * char_time)`.
pub fn string_duration(min_time: f32, start_time: f32, char_time: f32, chars: usize) -> f32 {
    let t = (chars as f32).mul_add(char_time, start_time);
    if min_time < t { t } else { min_time }
}

/// `-1` unchanged; `Some(relative)` when the pointer mode flips.
pub fn pointer_mode(current: i32, focus_present: bool, focus_is_3d: bool) -> Option<bool> {
    let want = i32::from(!focus_present || focus_is_3d);
    (want != current).then_some(want != 0)
}

/// The pointer stays centred until the OS reports motion.
pub fn pointer_moved(moved_before: bool, rel_x: i32, rel_y: i32) -> bool {
    moved_before || rel_x != 0 || rel_y != 0
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum Edge {
    #[default]
    None,
    Down,
    Up,
}

/// `BecameDown`/`BecameUp` for one action in range of the table.
pub fn action_edge(id: i32, max_actions: i32, down: bool, prev_down: bool) -> Edge {
    if id < 0 || id >= max_actions {
        Edge::None
    } else if down && !prev_down {
        Edge::Down
    } else if !down && prev_down {
        Edge::Up
    } else {
        Edge::None
    }
}

/// Analog sample of an action: `Some((axis, mul))` when it fires this frame.
pub fn analog_fire(analog_id: i32, axis: i32, mul: f32, triggered: bool) -> Option<(usize, f32)> {
    (analog_id >= 0 && (0..3).contains(&axis) && triggered && mul != 0.0)
        .then_some((axis as usize, mul))
}
