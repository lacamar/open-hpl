//! Sound listener, proximity sensor and bark machine timing.

use super::agent::AgentState;
use super::dist3;
use super::host::{Events, Host};
use crate::math::{Vector3f, clamp, rand_rectf, v3};

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct SoundListener {
    pub hear_radius: f32,
    pub radius_mul: f32,
    pub min_radius: f32,
    pub max_radius: f32,
    pub ignore_radius: f32,
    pub max_player_interact_time: f32,
    pub min_prio: i32,
}

impl Default for SoundListener {
    fn default() -> Self {
        Self {
            hear_radius: 30.0,
            radius_mul: 1.0,
            min_radius: 0.0,
            max_radius: 1000.0,
            ignore_radius: 0.0,
            max_player_interact_time: 0.0,
            min_prio: 0,
        }
    }
}

impl SoundListener {
    /// `SomaBroadcastSoundHeard` for one agent; true means `eMsg_SoundHeard` goes to it.
    pub fn hears(
        &self,
        active: bool,
        ent_active: bool,
        agent_pos: Vector3f,
        pos: Vector3f,
        radius: f32,
        prio: i32,
    ) -> bool {
        if !active || !ent_active || prio < self.min_prio {
            return false;
        }
        let r = clamp(radius * self.radius_mul, self.min_radius, self.max_radius);
        let d = dist3(agent_pos, pos);
        d < r && d < self.hear_radius + r && d >= self.ignore_radius
    }
}

pub const MSG_PROXIMITY_ENTER: i32 = 0x15;
pub const MSG_PROXIMITY_LEAVE: i32 = 0x16;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ProxState {
    pub exists: bool,
    pub active: bool,
    pub inside: bool,
    pub los: bool,
    pub detected_check: bool,
    pub enter: f32,
    pub leave: f32,
}

/// `cAgentProximitySensor::Update`; `ai` is "the agent has a character body".
pub fn proximity_update(
    timer: &mut f32,
    states: &mut [ProxState],
    dt: f32,
    agent: &AgentState,
    ai: bool,
    h: &mut dyn Host,
    ev: &mut Events,
) {
    *timer += dt;
    if *timer < 0.15 || !h.player_body_present() {
        return;
    }
    *timer = 0.0;
    for (i, s) in states.iter_mut().enumerate() {
        if !s.exists || !s.active {
            continue;
        }
        let from = if h.has_body() {
            h.body_feet()
        } else {
            h.ent_pos()
        };
        let player = h.player_pos();
        let dist = dist3(from, player);
        let qualifies = !ai
            || ((!s.detected_check || agent.detected)
                && (!s.los || {
                    let eye = agent.eye(h);
                    h.line_of_sight(eye, player)
                }));
        let inside = if s.inside {
            dist <= s.leave && qualifies
        } else {
            dist <= s.enter && qualifies
        };
        if inside == s.inside {
            continue;
        }
        s.inside = inside;
        let id = if inside {
            MSG_PROXIMITY_ENTER
        } else {
            MSG_PROXIMITY_LEAVE
        };
        ev.message(id, v3(0.0, 0.0, 0.0), i as i32);
    }
}

/// `cAgentBarkMachine::Update` up to the sound creation; true when a bark is due.
pub fn bark_due(
    count: &mut f32,
    dt: f32,
    active: bool,
    has_sound: bool,
    min: f32,
    max: f32,
) -> bool {
    if !active || !has_sound {
        return false;
    }
    *count -= dt;
    if *count > 0.0 {
        return false;
    }
    *count = rand_rectf(min, max);
    true
}
