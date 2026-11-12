//! `cLuxProp_Button`, `_LevelDoor`, `_OilBarrel` and `_Photocell`: switch state, door lock timer,
//! oil amounts and the light-level thresholds. Engine calls stay in C++.

use crate::math::{clamp, fma, v3};
use crate::types::Vector3f;

/// `Button::SetSwitchedOn`: the connection state to send, or `None` when nothing changes.
pub fn button_set(on: &mut bool, new_on: bool) -> Option<i32> {
    if *on == new_on {
        return None;
    }
    *on = new_on;
    Some(if new_on { 1 } else { -1 })
}

pub fn button_can_interact(on: bool, can_on: bool, can_off: bool) -> bool {
    (can_off && on) || (can_on && !on)
}

/// `Button::OnConnectionStateChange`: the switch state to set, if any.
pub fn button_conn_state(state: i32) -> Option<bool> {
    if state > 0 {
        Some(true)
    } else if state < 0 {
        Some(false)
    } else {
        None
    }
}

/// `LevelDoor::UpdatePropSpecific`.
pub fn door_tick(count: f32, dt: f32) -> f32 {
    if count > 0.0 { count - dt } else { count }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct DoorAct {
    /// Locked: play the locked sound (and message) and restart the timer.
    pub locked_feedback: bool,
    /// Unlocked: change the map.
    pub change_map: bool,
    pub count: f32,
}

/// `LevelDoor::OnInteract`.
pub fn door_interact(locked: bool, count: f32) -> DoorAct {
    let mut out = DoorAct {
        locked_feedback: false,
        change_map: false,
        count,
    };
    if locked {
        if count <= 0.0 {
            out.locked_feedback = true;
            out.count = 2.0;
        }
    } else {
        out.change_map = true;
    }
    out
}

/// `OilBarrel::CalculateOilAmount` after the cached/hard-mode checks: the barrel gives what the
/// player is short of, between 10 and 100.
pub fn oil_amount(total_oil: f32) -> f32 {
    let add = (100.0f32 - total_oil).clamp(0.0, 90.0);
    10.0f32 + add
}

pub const OIL_EMPTY: i32 = 0;
pub const OIL_FULL: i32 = 1;
pub const OIL_FILLED: i32 = 2;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct OilFill {
    /// `OIL_EMPTY`, `OIL_FULL` (lantern full) or `OIL_FILLED`.
    pub result: i32,
    pub barrel: f32,
    pub lamp: f32,
    pub filled: f32,
    /// Message tier, 0 = lantern now full, 1..4 = refill amount below 25/50/75/more.
    pub tier: i32,
}

/// `OilBarrel::OnInteract` after the amount is calculated.
pub fn oil_fill(barrel: f32, lamp: f32) -> OilFill {
    let mut out = OilFill {
        result: OIL_EMPTY,
        barrel,
        lamp,
        filled: 0.0,
        tier: 0,
    };
    if barrel <= 0.0 {
        return out;
    }
    if lamp > 99.0 {
        out.result = OIL_FULL;
        return out;
    }
    let mut oil = lamp + barrel;
    let fill;
    if oil > 100.0 {
        fill = barrel - (oil - 100.0f32);
        out.barrel = oil - 100.0f32;
        if out.barrel < 1.0 {
            out.barrel = 0.0;
        }
        oil = 100.0;
    } else {
        fill = barrel;
        out.barrel = 0.0;
    }
    out.result = OIL_FILLED;
    out.lamp = oil;
    out.filled = fill;
    out.tier = if oil >= 100.0 {
        0
    } else if fill < 25.0 {
        1
    } else if fill < 50.0 {
        2
    } else if fill < 75.0 {
        3
    } else {
        4
    };
    out
}

/// `OilBarrel::GetFocusText` tier: 0 empty, 1..4 below 25/50/75/more.
pub fn oil_focus_tier(barrel: f32) -> i32 {
    if barrel <= 0.0 {
        0
    } else if barrel < 25.0 {
        1
    } else if barrel < 50.0 {
        2
    } else if barrel < 75.0 {
        3
    } else {
        4
    }
}

/// `Photocell::UpdateLightLevel` timer: true when a new sample is due.
pub fn photo_tick(count: &mut f32, dt: f32) -> bool {
    *count += dt;
    if *count < 1.0f32 / 30.0f32 {
        return false;
    }
    *count = 0.0;
    true
}

/// The six sample positions around the bounding volume.
pub fn photo_samples(size: Vector3f, center: Vector3f) -> [Vector3f; 6] {
    let a = v3(
        fma(size.x, 0.5, 0.02),
        fma(size.y, 0.5, 0.02),
        fma(size.z, 0.5, 0.02),
    );
    [
        v3(center.x + a.x, center.y + 0.0, center.z + 0.0),
        v3(center.x - a.x, center.y - 0.0, center.z - 0.0),
        v3(center.x + 0.0, center.y + a.y, center.z + 0.0),
        v3(center.x - 0.0, center.y - a.y, center.z - 0.0),
        v3(center.x + 0.0, center.y + 0.0, center.z + a.z),
        v3(center.x - 0.0, center.y - 0.0, center.z - a.z),
    ]
}

/// The brightest of the samples (starting from 0).
pub fn photo_level(samples: &[f32]) -> f32 {
    let mut level = 0.0f32;
    for &l in samples {
        if l > level {
            level = l;
        }
    }
    level
}

/// Lit/unlit switching: the connection state to send (1, -1) or 0.
pub fn photo_switch(level: f32, lit: &mut bool, on_limit: f32, off_limit: f32) -> i32 {
    if *lit {
        if level < off_limit {
            *lit = false;
            return -1;
        }
    } else if level > on_limit {
        *lit = true;
        return 1;
    }
    0
}

/// The illumination the mesh should have.
pub fn photo_goal(level: f32, on_limit: f32, off_limit: f32) -> f32 {
    clamp((level - off_limit) / (on_limit - off_limit), 0.0, 1.0)
}

/// One illumination fade step towards the goal.
pub fn photo_illum_step(current: f32, goal: f32, dt: f32) -> f32 {
    let mut cur = current;
    if cur < goal {
        cur += dt;
        if cur > goal {
            cur = goal;
        }
    } else if cur > goal {
        cur -= dt;
        if cur < goal {
            cur = goal;
        }
    }
    cur
}
