//! `cAgentPathfinder`: path following, arrival detection and track nodes. Node containers and
//! A* stay C++ (`build_path`); the path itself is read back through the host.

use super::charmover::CharMoverState;
use super::host::{Events, Host};
use crate::math::{Vector3f, dot3, fma, len3, rand_rectf, sub3, v3};

pub const MSG_END_OF_PATH: i32 = 2;
pub const MSG_AT_TRACK_NODE: i32 = 11;
pub const MSG_END_OF_TRACK: i32 = 12;
pub const EV_END_OF_PATH_CALLBACK: u32 = 6;
pub const EV_RESULT_CALLBACK: u32 = 7;
pub const EV_TRACK_CALLBACK: u32 = 8;
pub const EV_TRACK_PLAY_ANIM: u32 = 9;
pub const HISTORY_CAP: usize = 152;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct TrackNodeInfo {
    pub min_wait: f32,
    pub max_wait: f32,
    pub anim_nonempty: bool,
    pub loop_anim: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct PathfinderState {
    pub moving: bool,
    pub exact: bool,
    pub goal_pending: bool,
    pub callback_in_map: bool,
    pub track_active: bool,
    pub track_paused: bool,
    pub track_loop: bool,
    pub at_track_node: bool,
    pub goal: [f32; 3],
    pub path_idx: u64,
    pub track_idx: i32,
    pub track_wait: f32,
    pub track_freq: f32,
    pub history_len: i32,
    pub history: [f32; HISTORY_CAP],
}

impl Default for PathfinderState {
    fn default() -> Self {
        Self {
            moving: false,
            exact: false,
            goal_pending: false,
            callback_in_map: false,
            track_active: false,
            track_paused: false,
            track_loop: false,
            at_track_node: false,
            goal: [0.0; 3],
            path_idx: 0,
            track_idx: -1,
            track_wait: 0.0,
            track_freq: 1.0,
            history_len: 0,
            history: [0.0; HISTORY_CAP],
        }
    }
}

impl PathfinderState {
    fn history_slice(&self) -> &[f32] {
        let n = (self.history_len.max(0) as usize).min(HISTORY_CAP);
        self.history.get(..n).unwrap_or(&[])
    }

    fn history_push(&mut self, v: f32) {
        let n = (self.history_len.max(0) as usize).min(HISTORY_CAP - 1);
        if let Some(s) = self.history.get_mut(n) {
            *s = v;
        }
        self.history_len = n as i32 + 1;
    }

    fn history_pop_front(&mut self) {
        let n = self.history_len.max(0) as usize;
        if n > 0 {
            self.history.copy_within(1..n.min(HISTORY_CAP), 0);
            self.history_len -= 1;
        }
    }

    pub fn clear_history(&mut self) {
        self.history_len = 0;
    }

    /// `MoveTo` after the C++ side built the path (`path_len >= 1`) and stored the callbacks.
    pub fn move_to(
        &mut self,
        goal: Vector3f,
        exact: bool,
        in_map: bool,
        mover: Option<&mut CharMoverState>,
        h: &mut dyn Host,
    ) {
        self.goal = [goal.x, goal.y, goal.z];
        self.exact = exact;
        self.callback_in_map = in_map;
        h.pf_build_path(goal);
        self.path_idx = 0;
        self.moving = true;
        self.clear_history();
        if let Some(m) = mover {
            let first = h.pf_path_get(0);
            m.move_to_pos(first, exact && h.pf_path_len() == 1);
        }
    }

    pub fn stop(&mut self, mover: Option<&mut CharMoverState>) {
        self.moving = false;
        if let Some(m) = mover {
            m.stop();
        }
    }

    fn arrive_end(
        &mut self,
        ok: bool,
        mover: Option<&mut CharMoverState>,
        h: &mut dyn Host,
        ev: &mut Events,
    ) {
        self.moving = false;
        let has_mover = mover.is_some();
        if let Some(m) = mover {
            m.stop();
        }
        ev.message(MSG_END_OF_PATH, v3(0.0, 0.0, 0.0), i32::from(ok));
        ev.0.push(super::host::Event {
            kind: EV_END_OF_PATH_CALLBACK,
            a: i32::from(ok),
            b: 0,
            v: [0.0; 3],
        });
        let len = h.pf_track_len() as i64;
        if self.track_active && self.track_idx >= 0 && i64::from(self.track_idx) < len {
            self.at_track_node = true;
            self.track_wait = 0.0;
            if !ok {
                return;
            }
            let node = h.pf_track_info(self.track_idx as usize);
            self.track_wait = rand_rectf(node.min_wait, node.max_wait);
            if node.anim_nonempty && has_mover {
                ev.0.push(super::host::Event {
                    kind: EV_TRACK_PLAY_ANIM,
                    a: self.track_idx,
                    b: 0,
                    v: [0.0; 3],
                });
            }
            ev.message(MSG_AT_TRACK_NODE, v3(0.0, 0.0, 0.0), 0);
        } else {
            ev.0.push(super::host::Event {
                kind: EV_RESULT_CALLBACK,
                a: i32::from(ok),
                b: 0,
                v: [0.0; 3],
            });
        }
    }

    pub fn on_message(
        &mut self,
        msg: i32,
        mover: Option<&mut CharMoverState>,
        h: &mut dyn Host,
        ev: &mut Events,
    ) {
        if msg == super::charmover::MSG_STUCK {
            self.arrive_end(false, mover, h, ev);
        }
    }

    pub fn start_track_node(&mut self, mover: Option<&mut CharMoverState>, h: &mut dyn Host) {
        if self.track_idx < 0 || self.track_idx as usize >= h.pf_track_len() {
            return;
        }
        let Some(pos) = h.pf_track_node_pos(self.track_idx as usize) else {
            self.track_active = false;
            return;
        };
        self.at_track_node = false;
        self.move_to(pos, false, false, mover, h);
    }

    pub fn go_to_next_track_node(
        &mut self,
        mover: Option<&mut CharMoverState>,
        h: &mut dyn Host,
        ev: &mut Events,
    ) {
        let len = h.pf_track_len();
        if len == 0 {
            return;
        }
        self.track_idx = self.track_idx.wrapping_add(1);
        if self.track_idx >= len as i32 {
            ev.message(MSG_END_OF_TRACK, v3(0.0, 0.0, 0.0), 0);
            ev.plain(EV_TRACK_CALLBACK);
            if !self.track_loop {
                self.track_active = false;
                self.track_idx = len as i32 - 1;
                return;
            }
            self.track_idx = 0;
        }
        self.start_track_node(mover, h);
    }

    /// `cLuxPathfinder::UpdateMoving`: node inside the enlarged body box, or no progress for 150
    /// frames near it.
    fn arrived(&mut self, node: Vector3f, last: bool, h: &mut dyn Host) -> bool {
        if !h.has_body() {
            let mut delta = sub3(node, h.pf_feet());
            let height = delta.y.abs();
            delta.y = 0.0;
            let r = if last {
                if self.exact { 0.15 } else { 0.4 }
            } else {
                0.6
            };
            return len3(delta) < r && height < 2.0;
        }
        let (size, pos) = (h.body_size(), h.body_pos());
        let d = sub3(node, pos);
        let dist_sqr = dot3(d, d);
        self.history_push(dist_sqr);
        if self.history_len > 150 {
            self.history_pop_front();
            let front = self.history_slice().first().copied().unwrap_or(0.0);
            let mut growth = 0.0f32;
            for f in self.history_slice() {
                growth += f - front;
            }
            if growth > 0.0 && dist_sqr < size.y * 1.5 {
                return true;
            }
        }
        let d = sub3(node, pos);
        let d = v3(d.x + 0.0, fma(0.1, size.y, d.y), d.z + 0.0);
        d.x.abs() <= size.x * 0.65
            && d.y.abs() <= fma(size.y, 0.6, 0.225)
            && d.z.abs() <= size.z * 0.65
    }

    pub fn update(
        &mut self,
        dt: f32,
        mut mover: Option<&mut CharMoverState>,
        h: &mut dyn Host,
        ev: &mut Events,
    ) {
        if self.track_active && !self.track_paused && self.at_track_node && !self.moving {
            self.track_wait -= dt;
            if self.track_wait <= 0.0 {
                self.go_to_next_track_node(mover.as_deref_mut(), h, ev);
            }
        }
        let len = h.pf_path_len();
        if !self.moving || len == 0 {
            return;
        }
        let idx = self.path_idx as usize;
        let last = idx.wrapping_add(1) >= len;
        let node = h.pf_path_get(idx);
        if self.arrived(node, last, h) {
            self.clear_history();
            if last {
                self.arrive_end(true, mover, h, ev);
                return;
            }
            self.path_idx = self.path_idx.wrapping_add(1);
        }
        self.goal_pending = true;
    }

    /// Queued like the real goal message, so it lands after the state machine's `MoveToPos`.
    pub fn send_goal(&mut self, mover: Option<&mut CharMoverState>, h: &mut dyn Host) {
        let pending = std::mem::take(&mut self.goal_pending);
        let idx = self.path_idx as usize;
        let len = h.pf_path_len();
        if let (true, true, true, Some(m)) = (pending, self.moving, idx < len, mover) {
            let p = h.pf_path_get(idx);
            m.move_to_pos(p, self.exact && idx.wrapping_add(1) >= len);
        }
    }
}
