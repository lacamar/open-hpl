//! The agent logic's `Host` over the host ops: engine objects through `Io`, agent data from the
//! Rust-owned `Agent`.

use super::super::charmover::AvoidBody;
use super::super::host::Host;
use super::super::pathfinder::TrackNodeInfo;
use super::io::{Io, op};
use super::model::{Agent, COMP_CHAR_MOVER, COMP_PATHFINDER, Cm, K, Pf};
use super::pod;
use crate::math::{Matrixf, Vector3f, v3};

pub struct H(pub *mut Agent);

impl H {
    fn a(&self) -> &Agent {
        // SAFETY: the agent outlives the logic call this host serves.
        unsafe { &*self.0 }
    }

    fn ent(&self) -> usize {
        self.a().ent
    }

    fn body(&self) -> usize {
        self.a().body
    }

    fn cm(&self) -> Option<&Cm> {
        let p = self.a().find_ptr(COMP_CHAR_MOVER);
        // SAFETY: a live component of this agent.
        match unsafe { p.as_ref() }.map(|c| &c.k) {
            Some(K::Cm(c)) => Some(c),
            _ => None,
        }
    }

    fn pf(&self) -> Option<&Pf> {
        let p = self.a().find_ptr(COMP_PATHFINDER);
        // SAFETY: as `cm`.
        match unsafe { p.as_ref() }.map(|c| &c.k) {
            Some(K::Pf(c)) => Some(c),
            _ => None,
        }
    }

    #[allow(clippy::mut_from_ref)]
    fn pf_mut(&self) -> Option<&mut Pf> {
        let p = self.a().find_ptr(COMP_PATHFINDER);
        // SAFETY: as `cm`; the logic holds only the pod fields, never the `Pf` as a whole.
        match unsafe { p.as_mut() }.map(|c| &mut c.k) {
            Some(K::Pf(c)) => Some(c),
            _ => None,
        }
    }

    fn b3(&self, w: i32) -> Vector3f {
        Io::new().p(1, self.body()).i(0, w).go(op::B_GET3).out_v(0)
    }

    fn bf(&self, w: i32) -> f32 {
        Io::new().p(1, self.body()).i(0, w).go(op::B_GETF).fl(0)
    }

    fn bflag(&self, k: usize) -> bool {
        Io::new().p(1, self.body()).go(op::B_FLAGS).int(k) != 0
    }

    fn eflag(&self, k: usize) -> bool {
        Io::new().p(0, self.ent()).go(op::E_FLAGS).int(k) != 0
    }

    fn pinfo(&self) -> Io {
        Io::new().go(op::P_INFO)
    }

    fn move_anim(&self, state: i32) -> Option<&[u8]> {
        self.cm().map(|m| match state {
            0 => m.idle.as_slice(),
            1 => m.walk.as_slice(),
            _ => m.run.as_slice(),
        })
    }

    pub fn feet(&self) -> Vector3f {
        let a = self.a();
        if a.body == 0 {
            return self.ent_pos_();
        }
        let at_center = self.pf().is_some_and(|p| p.at_center);
        if at_center { self.b3(0) } else { self.b3(1) }
    }

    fn ent_pos_(&self) -> Vector3f {
        Io::new().p(0, self.ent()).go(op::E_POS).out_v(0)
    }

    pub fn nodes(&self) -> usize {
        let Some(pf) = self.pf_mut() else { return 0 };
        if pf.nodes == 0 {
            pf.nodes = Io::new()
                .p(0, self.ent())
                .s(0, &pf.container)
                .f(0, pf.max_height)
                .f(1, pf.max_edge)
                .i(0, i32::from(pf.at_center))
                .go(op::ND_GET)
                .ptr(0);
        }
        pf.nodes
    }

    pub fn build_path(&self, goal: Vector3f) -> bool {
        let nd = self.nodes();
        let feet = self.feet();
        let Some(pf) = self.pf_mut() else {
            return false;
        };
        pf.path.clear();
        if nd == 0 {
            pf.path.push(goal);
            return true;
        }
        let io = Io::new().p(0, nd).v(0, feet).v(3, goal).go(op::ND_PATH);
        let n = io.ret();
        if n < 0 {
            return false;
        }
        for k in (0..n).rev() {
            let at = Io::new().p(0, nd).i(0, k).go(op::ND_PATH_AT);
            pf.path.push(at.out_v(0));
        }
        pf.path.push(goal);
        true
    }
}

impl Host for H {
    fn has_body(&mut self) -> bool {
        self.body() != 0
    }
    fn has_map(&mut self) -> bool {
        self.eflag(0)
    }
    fn has_mesh(&mut self) -> bool {
        self.eflag(1)
    }
    fn ent_pos(&mut self) -> Vector3f {
        self.ent_pos_()
    }
    fn ent_scale_y(&mut self) -> f32 {
        pod::scale(self.ent()).y
    }
    fn ent_global_space_anim(&mut self) -> bool {
        pod::global_space_anim(self.ent())
    }
    fn body_pos(&mut self) -> Vector3f {
        self.b3(0)
    }
    fn body_feet(&mut self) -> Vector3f {
        self.b3(1)
    }
    fn body_size(&mut self) -> Vector3f {
        self.b3(2)
    }
    fn body_forward(&mut self) -> Vector3f {
        self.b3(3)
    }
    fn body_yaw(&mut self) -> f32 {
        self.bf(0)
    }
    fn body_pitch(&mut self) -> f32 {
        self.bf(1)
    }
    fn body_entity_pitch_amount(&mut self) -> f32 {
        self.bf(2)
    }
    fn body_move_speed(&mut self) -> f32 {
        self.bf(3)
    }
    fn body_on_ground(&mut self) -> bool {
        self.bflag(0)
    }
    fn body_test_collision(&mut self) -> bool {
        self.bflag(1)
    }
    fn body_gravity_active(&mut self) -> bool {
        self.bflag(2)
    }
    fn body_max_step_size(&mut self) -> f32 {
        self.bf(4)
    }
    fn set_body_yaw(&mut self, yaw: f32) {
        Io::new().p(1, self.body()).f(0, yaw).go(op::B_SET_YAW);
    }
    fn set_body_pos(&mut self, p: Vector3f) {
        Io::new().p(1, self.body()).i(0, 0).v(0, p).go(op::B_SET3);
    }
    fn set_body_feet(&mut self, p: Vector3f) {
        Io::new().p(1, self.body()).i(0, 1).v(0, p).go(op::B_SET3);
    }
    fn set_body_gravity(&mut self, on: bool) {
        Io::new()
            .p(1, self.body())
            .i(0, 0)
            .i(1, i32::from(on))
            .go(op::B_SETB);
    }
    fn set_body_collision(&mut self, on: bool) {
        Io::new()
            .p(1, self.body())
            .i(0, 1)
            .i(1, i32::from(on))
            .go(op::B_SETB);
    }
    fn has_pos_bone(&mut self) -> bool {
        self.a().pos_bone != 0
    }
    fn pos_bone_world_pos(&mut self) -> Vector3f {
        Io::new()
            .p(0, self.a().pos_bone)
            .go(op::BN_WORLD_POS)
            .out_v(0)
    }
    fn set_mesh_matrix(&mut self, m: &Matrixf) {
        Io::new()
            .p(0, self.ent())
            .p(1, std::ptr::from_ref(m) as usize)
            .go(op::E_MESH_SET_MATRIX);
    }
    fn player_body_present(&mut self) -> bool {
        self.pinfo().int(0) != 0
    }
    fn player_dead(&mut self) -> bool {
        self.pinfo().int(1) != 0
    }
    fn player_visibility_range_mul(&mut self) -> f32 {
        self.pinfo().fl(0)
    }
    fn player_visibility_max_range(&mut self) -> f32 {
        self.pinfo().fl(1)
    }
    fn player_pos(&mut self) -> Vector3f {
        self.pinfo().out_v(2)
    }
    fn player_feet(&mut self) -> Vector3f {
        self.pinfo().out_v(5)
    }
    fn player_size_y(&mut self) -> f32 {
        self.pinfo().fl(8)
    }
    fn line_of_sight(&mut self, from: Vector3f, to: Vector3f) -> bool {
        Io::new()
            .p(0, self.ent())
            .v(0, from)
            .v(3, to)
            .go(op::W_LOS)
            .int(0)
            != 0
    }
    fn entity_in_player_los(&mut self) -> bool {
        Io::new()
            .p(0, self.ent())
            .i(0, 1)
            .go(op::W_PLAYER_LOS)
            .int(0)
            != 0
    }
    fn floor_dist(&mut self, start: Vector3f, end: Vector3f) -> f32 {
        Io::new()
            .p(0, self.ent())
            .v(0, start)
            .v(3, end)
            .go(op::W_FLOOR)
            .fl(0)
    }
    fn door_count(&mut self) -> usize {
        Io::new().p(0, self.ent()).go(op::E_MAP_ENT_COUNT).int(0) as usize
    }
    fn body_last_pos(&mut self) -> Vector3f {
        self.b3(5)
    }
    fn body_right(&mut self) -> Vector3f {
        self.b3(4)
    }
    fn body_velocity(&mut self, dt: f32) -> Vector3f {
        Io::new().p(1, self.body()).f(0, dt).go(op::B_VEL).out_v(0)
    }
    fn body_add_pitch(&mut self, p: f32) {
        Io::new().p(1, self.body()).i(0, 0).f(0, p).go(op::B_SETF);
    }
    fn body_set_max_forward(&mut self, v: f32) {
        Io::new().p(1, self.body()).i(0, 1).f(0, v).go(op::B_SETF);
    }
    fn body_set_move_acc(&mut self, v: f32) {
        Io::new().p(1, self.body()).i(0, 2).f(0, v).go(op::B_SETF);
    }
    fn body_set_move_deacc(&mut self, v: f32) {
        Io::new().p(1, self.body()).i(0, 3).f(0, v).go(op::B_SETF);
    }
    fn body_move_forward(&mut self) {
        Io::new().p(1, self.body()).go(op::B_MOVE_FWD);
    }
    fn raycast(&mut self, start: Vector3f, end: Vector3f) -> Option<(f32, Vector3f)> {
        let io = Io::new()
            .p(0, self.ent())
            .v(0, start)
            .v(3, end)
            .go(op::W_RAYCAST);
        (io.int(0) != 0).then(|| (io.fl(6), io.out_v(7)))
    }
    fn avoid_query(&mut self, center: Vector3f, dist: f32) -> usize {
        Io::new()
            .p(0, self.ent())
            .v(0, center)
            .f(3, dist)
            .go(op::W_AVOID_QUERY)
            .int(0) as usize
    }
    fn avoid_get(&mut self, i: usize) -> AvoidBody {
        let io = Io::new()
            .p(1, self.body())
            .i(0, i as i32)
            .go(op::W_AVOID_GET);
        AvoidBody {
            center: [io.fl(0), io.fl(1), io.fl(2)],
            radius: io.fl(3),
            mass: io.fl(4),
            active: io.int(1) != 0,
            collide: io.int(2) != 0,
            character: io.int(3) != 0,
            player: io.int(4) != 0,
            own: io.int(5) != 0,
        }
    }
    fn anim_current(&mut self) -> i32 {
        pod::current_anim(self.ent())
    }
    fn anim_name_nonempty(&mut self, state: i32) -> bool {
        self.move_anim(state).is_some_and(|n| !n.is_empty())
    }
    fn play_move_anim(&mut self, state: i32) {
        if let Some(n) = self.move_anim(state) {
            Io::new()
                .p(0, self.ent())
                .s(0, n)
                .f(0, 0.3)
                .i(0, 1)
                .go(op::E_PLAY_ANIM);
        }
    }
    fn set_move_anim_speed(&mut self, speed: f32) {
        Io::new()
            .p(0, self.ent())
            .f(0, speed)
            .go(op::E_ANIM_SET_SPEED);
    }
    fn idle_restart(&mut self) {
        if let Some(m) = self.cm() {
            Io::new()
                .p(0, self.ent())
                .s(0, &m.idle)
                .go(op::E_ANIM_RESTART_RAND);
        }
    }
    fn idle_extra_length(&mut self, idx: i32) -> Option<f32> {
        let m = self.cm()?;
        let mut name = m.idle_extra.clone();
        name.extend_from_slice(idx.to_string().as_bytes());
        let io = Io::new().p(0, self.ent()).s(0, &name).go(op::E_ANIM_LEN);
        (io.int(0) != 0).then_some(io.fl(0))
    }
    fn play_idle_extra(&mut self, idx: i32) {
        if let Some(m) = self.cm() {
            let mut name = m.idle_extra.clone();
            name.extend_from_slice(idx.to_string().as_bytes());
            Io::new()
                .p(0, self.ent())
                .s(0, &name)
                .f(0, 0.3)
                .go(op::E_PLAY_ANIM);
        }
    }
    fn pf_feet(&mut self) -> Vector3f {
        if self.pf().is_some() {
            self.feet()
        } else {
            v3(0.0, 0.0, 0.0)
        }
    }
    fn pf_path_len(&mut self) -> usize {
        self.pf().map_or(0, |p| p.path.len())
    }
    fn pf_path_get(&mut self, i: usize) -> Vector3f {
        self.pf()
            .and_then(|p| p.path.get(i).copied())
            .unwrap_or(v3(0.0, 0.0, 0.0))
    }
    fn pf_build_path(&mut self, goal: Vector3f) {
        if self.pf().is_none() {
            return;
        }
        if !self.build_path(goal)
            && let Some(p) = self.pf_mut()
        {
            p.path.clear();
            p.path.push(goal);
        }
    }
    fn pf_track_len(&mut self) -> usize {
        self.pf().map_or(0, |p| p.track.len())
    }
    fn pf_track_info(&mut self, i: usize) -> TrackNodeInfo {
        self.pf()
            .and_then(|p| p.track.get(i))
            .map_or_else(TrackNodeInfo::default, |n| TrackNodeInfo {
                min_wait: n.min_wait,
                max_wait: n.max_wait,
                anim_nonempty: !n.anim.get().is_empty(),
                loop_anim: n.looped,
            })
    }
    fn pf_track_node_pos(&mut self, i: usize) -> Option<Vector3f> {
        let nd = self.nodes();
        let name = self
            .pf()
            .and_then(|p| p.track.get(i))
            .map(|n| n.node.get().to_vec())?;
        let io = if nd == 0 {
            Io::new()
        } else {
            Io::new().p(0, nd).s(0, &name).go(op::ND_BY_NAME)
        };
        if nd == 0 || io.ptr(0) == 0 {
            let mut msg = b"SOMA agent '".to_vec();
            msg.extend_from_slice(&pod::name(self.ent()));
            msg.extend_from_slice(b"': track node '");
            msg.extend_from_slice(&name);
            msg.extend_from_slice(b"' not found\n");
            Io::new().s(0, &msg).go(op::LOG_WARN);
            return None;
        }
        Some(io.out_v(0))
    }
    fn door_hit(
        &mut self,
        idx: usize,
        origin: Vector3f,
        dir: Vector3f,
        max: f32,
    ) -> Option<Vector3f> {
        let io = Io::new()
            .p(0, self.ent())
            .i(0, idx as i32)
            .v(0, origin)
            .v(3, dir)
            .f(6, max)
            .go(op::E_DOOR_HIT);
        (io.int(1) != 0).then(|| io.out_v(0))
    }
}
