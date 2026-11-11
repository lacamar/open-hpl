//! LuxTypes logic: alpha fader, collide-callback transitions, enemy sound gate.

use crate::math;
use crate::string;

#[repr(C)]
pub struct AlphaFader {
    pub alpha: f32,
    pub active: bool,
    pub fade_in: f32,
    pub fade_out: f32,
}

impl AlphaFader {
    pub(crate) fn reset(&mut self) {
        self.active = false;
        self.alpha = 0.0;
    }

    pub(crate) fn update(&mut self, dt: f32) {
        if self.active {
            self.alpha = math::fma(self.fade_in, dt, self.alpha);
            if self.alpha > 1.0 {
                self.alpha = 1.0;
            }
        } else {
            self.alpha = math::fma(-self.fade_out, dt, self.alpha);
            if self.alpha < 0.0 {
                self.alpha = 0.0;
            }
        }
    }
}

#[repr(C)]
pub struct CollideStep {
    pub transition: bool,
    pub fire: bool,
    pub state: i32,
}

/// A callback fires on a colliding change whose direction matches `states` (0 = both).
pub(crate) fn collide_step(colliding: bool, states: i32, collide: bool) -> CollideStep {
    if collide == colliding {
        return CollideStep {
            transition: false,
            fire: false,
            state: 0,
        };
    }
    let state = if collide { 1 } else { -1 };
    CollideStep {
        transition: true,
        fire: state == states || states == 0,
        state,
    }
}

pub(crate) fn collide_command(func: &[u8], name: &[u8], entity: &[u8], state: i32) -> Vec<u8> {
    let mut c = func.to_vec();
    c.extend_from_slice(b"(\"");
    c.extend_from_slice(name);
    c.extend_from_slice(b"\", \"");
    c.extend_from_slice(entity);
    c.extend_from_slice(b"\", ");
    c.extend(string::int_to_string(state, 0));
    c.push(b')');
    c
}

pub(crate) fn enemy_sound_update(count: &mut f32, dt: f32) {
    if *count > 0.0 {
        *count -= dt;
    }
}

pub(crate) fn enemy_sound_allowed(count: f32) -> bool {
    count <= 0.0
}

pub(crate) const ENEMY_SOUND_COOLDOWN: f32 = 5.0;
