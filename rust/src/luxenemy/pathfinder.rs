//! `cLuxEnemyPathfinder`: path following with node-reached and stuck decisions, node picking.

use super::ctx::{Env, g, gv, s};
use super::dist_sqr;
use super::host::Host;
use super::machine::MSG_END_OF_PATH;
use crate::math::{Vector3f, fma, rand_rectl, v3};

pub const WIN_CAP: usize = 152;

/// `mlstPathNodeDistances`: the last 150 squared goal distances.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct PfWin {
    pub len: i32,
    pub v: [f32; WIN_CAP],
}

impl Default for PfWin {
    fn default() -> Self {
        Self {
            len: 0,
            v: [0.0; WIN_CAP],
        }
    }
}

impl PfWin {
    fn slice(&self) -> &[f32] {
        self.v
            .get(..(self.len.max(0) as usize).min(WIN_CAP))
            .unwrap_or(&[])
    }

    fn push(&mut self, x: f32) {
        let n = (self.len.max(0) as usize).min(WIN_CAP - 1);
        if let Some(d) = self.v.get_mut(n) {
            *d = x;
        }
        self.len = n as i32 + 1;
    }

    fn pop_front(&mut self) {
        let n = (self.len.max(0) as usize).min(WIN_CAP);
        if n > 0 {
            self.v.copy_within(1..n, 0);
            self.len -= 1;
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub struct NodeQuery {
    pub min_dist: f32,
    pub max_dist: f32,
    pub get_closest: bool,
    pub pos_free_check: bool,
    pub enemy_free_check: bool,
    pub skip: i32,
    pub ray_num: i32,
    pub skip_used: bool,
}

impl NodeQuery {
    /// The defaults of `GetNodeAtPos`: no ray count, skip used nodes.
    pub fn new(
        min: f32,
        max: f32,
        closest: bool,
        pos_free: bool,
        enemy_free: bool,
        skip: i32,
    ) -> Self {
        Self {
            min_dist: min,
            max_dist: max,
            get_closest: closest,
            pos_free_check: pos_free,
            enemy_free_check: enemy_free,
            skip,
            ray_num: -1,
            skip_used: true,
        }
    }
}

/// `GetNodeAtPos`: the chosen iterator index and node position.
pub fn node_at_pos(h: &mut dyn Host, pos: Vector3f, q: &NodeQuery) -> Option<(i32, Vector3f)> {
    if !h.nc_present() {
        return None;
    }
    let max_sqr = q.max_dist * q.max_dist;
    let min_sqr = q.min_dist * q.min_dist;
    // The start index is drawn and never used; the draw still advances rand().
    let _ = rand_rectl(0, h.nc_count().wrapping_sub(1));
    let mut closest_sqr = max_sqr * 10.0;
    let mut ret = None;
    let mut cands: Vec<(i32, Vector3f)> = Vec::new();
    let n = h.nc_iter_begin(pos, q.max_dist, q.skip);
    for idx in 0..n {
        let Some(np) = h.nc_iter_node(idx, q.skip_used) else {
            continue;
        };
        let d = dist_sqr(np, pos);
        if d > max_sqr || d < min_sqr {
            continue;
        }
        if q.get_closest && d > closest_sqr {
            continue;
        }
        // The ray count is passed for the flags parameter too, as the original does.
        if q.pos_free_check && !h.nc_free_path(pos, np, q.ray_num, q.ray_num) {
            continue;
        }
        if q.enemy_free_check {
            let bp = h.body_pos();
            if !h.nc_free_path(bp, np, q.ray_num, q.ray_num) {
                continue;
            }
        }
        if !q.get_closest {
            cands.push((idx, np));
        }
        ret = Some((idx, np));
        closest_sqr = d;
    }
    if !q.get_closest && !cands.is_empty() {
        let i = rand_rectl(0, cands.len() as i32 - 1);
        return cands.get(i.max(0) as usize).copied();
    }
    ret
}

/// `GetNextGoalPos`.
pub fn next_goal_pos(h: &mut dyn Host, goal: Vector3f) -> Vector3f {
    if h.pf_path_len() == 0 {
        goal
    } else {
        h.pf_path_back()
    }
}

pub fn update_moving(e: &mut Env) {
    if !g(e.c.pf_moving) {
        return;
    }
    let goal_pos = gv(e.c.pf_goal);
    let body = e.h.body_size();
    let bp = e.h.body_pos();
    let size = v3(body.x * 1.3, fma(body.y, 1.2, 0.45), body.z * 1.3);
    let bvpos = v3(bp.x - 0.0, fma(-body.y, 0.1, bp.y), bp.z - 0.0);
    let goal = if e.h.pf_path_len() == 0 {
        goal_pos
    } else {
        e.h.pf_path_back()
    };
    // SAFETY: the mover state is only aliased by C++ inside `play_anim`, which never runs here.
    unsafe { (*e.c.mover).move_to_pos(goal, e.h) };
    let mut stuck_at_node = false;
    let feet = e.h.body_feet();
    let node_dist = dist_sqr(goal, feet);
    // SAFETY: the window belongs to the pathfinder and no host call touches it.
    let win = unsafe { &mut *e.c.pf_win };
    win.push(node_dist);
    if win.len > 150 {
        win.pop_front();
        let w = win.slice();
        let first = w.first().copied().unwrap_or(0.0);
        let mut avg = 0.0f32;
        for x in w.iter().skip(1) {
            avg += *x - first;
        }
        let sx = e.h.body_size().x;
        if avg > 0.0 && node_dist < sx * 1.5 {
            stuck_at_node = true;
        }
    }
    if stuck_at_node || e.h.bv_contains(goal, size, bvpos) {
        if e.h.pf_path_len() == 0 {
            s(e.c.pf_moving, false);
            e.send0(MSG_END_OF_PATH, 0.0, false);
        } else {
            e.h.pf_path_pop();
        }
        win.len = 0;
    } else if unsafe { (*e.c.mover).stuck_at_max() } {
        // SAFETY: as above.
        unsafe { (*e.c.mover).stuck_counter = 0.0 };
        e.h.pf_path_clear();
        win.len = 0;
        e.send(MSG_END_OF_PATH, 0.0, false, v3(0.0, 0.0, 0.0), 0.0, 1);
    }
}
