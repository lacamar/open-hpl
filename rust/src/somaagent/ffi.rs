//! C ABI of the SOMA agent layer, mirrored in `soma/src/game/RustSomaAgent.h`.

use super::agent::AgentState;
use super::charmover::{AvoidBody, CharMoverState, SpeedState, WallSample};
use super::host::{Event, Events, Host};
use super::misc::{ProxState, SoundListener, bark_due, proximity_update};
use super::pathfinder::{PathfinderState, TrackNodeInfo};
use super::statemachine::{Action, SmExtra, SmPod};
use crate::math::{Matrixf, Vector3f, v3};
use std::ffi::c_void;

type Ctx = *mut c_void;
type Out3 = *mut f32;
type In3 = *const f32;

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Event>() == 24 && offset_of!(Event, v) == 12);
    assert!(size_of::<ProxState>() == 16 && offset_of!(ProxState, enter) == 8);
    assert!(size_of::<SoundListener>() == 28);
    assert!(size_of::<AgentState>() == 144 && offset_of!(AgentState, fov) == 16);
};

#[repr(C)]
pub struct OhplGs2Host {
    pub ctx: Ctx,
    pub has_body: unsafe extern "C" fn(Ctx) -> bool,
    pub has_map: unsafe extern "C" fn(Ctx) -> bool,
    pub has_mesh: unsafe extern "C" fn(Ctx) -> bool,
    pub ent_pos: unsafe extern "C" fn(Ctx, Out3),
    pub ent_scale_y: unsafe extern "C" fn(Ctx) -> f32,
    pub ent_global_space_anim: unsafe extern "C" fn(Ctx) -> bool,
    pub body_pos: unsafe extern "C" fn(Ctx, Out3),
    pub body_feet: unsafe extern "C" fn(Ctx, Out3),
    pub body_size: unsafe extern "C" fn(Ctx, Out3),
    pub body_forward: unsafe extern "C" fn(Ctx, Out3),
    pub body_yaw: unsafe extern "C" fn(Ctx) -> f32,
    pub body_pitch: unsafe extern "C" fn(Ctx) -> f32,
    pub body_entity_pitch_amount: unsafe extern "C" fn(Ctx) -> f32,
    pub body_move_speed: unsafe extern "C" fn(Ctx) -> f32,
    pub body_on_ground: unsafe extern "C" fn(Ctx) -> bool,
    pub body_test_collision: unsafe extern "C" fn(Ctx) -> bool,
    pub body_gravity_active: unsafe extern "C" fn(Ctx) -> bool,
    pub body_max_step_size: unsafe extern "C" fn(Ctx) -> f32,
    pub set_body_yaw: unsafe extern "C" fn(Ctx, f32),
    pub set_body_pos: unsafe extern "C" fn(Ctx, In3),
    pub set_body_feet: unsafe extern "C" fn(Ctx, In3),
    pub set_body_gravity: unsafe extern "C" fn(Ctx, bool),
    pub set_body_collision: unsafe extern "C" fn(Ctx, bool),
    pub has_pos_bone: unsafe extern "C" fn(Ctx) -> bool,
    pub pos_bone_world_pos: unsafe extern "C" fn(Ctx, Out3),
    pub set_mesh_matrix: unsafe extern "C" fn(Ctx, *const Matrixf),
    pub player_body_present: unsafe extern "C" fn(Ctx) -> bool,
    pub player_dead: unsafe extern "C" fn(Ctx) -> bool,
    pub player_visibility_range_mul: unsafe extern "C" fn(Ctx) -> f32,
    pub player_visibility_max_range: unsafe extern "C" fn(Ctx) -> f32,
    pub player_pos: unsafe extern "C" fn(Ctx, Out3),
    pub player_feet: unsafe extern "C" fn(Ctx, Out3),
    pub player_size_y: unsafe extern "C" fn(Ctx) -> f32,
    pub line_of_sight: unsafe extern "C" fn(Ctx, In3, In3) -> bool,
    pub entity_in_player_los: unsafe extern "C" fn(Ctx) -> bool,
    pub floor_dist: unsafe extern "C" fn(Ctx, In3, In3) -> f32,
    pub body_last_pos: unsafe extern "C" fn(Ctx, Out3),
    pub body_right: unsafe extern "C" fn(Ctx, Out3),
    pub body_velocity: unsafe extern "C" fn(Ctx, f32, Out3),
    pub body_add_pitch: unsafe extern "C" fn(Ctx, f32),
    pub body_set_max_forward: unsafe extern "C" fn(Ctx, f32),
    pub body_set_move_acc: unsafe extern "C" fn(Ctx, f32),
    pub body_set_move_deacc: unsafe extern "C" fn(Ctx, f32),
    pub body_move_forward: unsafe extern "C" fn(Ctx),
    pub raycast: unsafe extern "C" fn(Ctx, In3, In3, *mut f32, Out3) -> bool,
    pub avoid_query: unsafe extern "C" fn(Ctx, In3, f32) -> usize,
    pub avoid_get: unsafe extern "C" fn(Ctx, usize, *mut AvoidBody),
    pub anim_current: unsafe extern "C" fn(Ctx) -> i32,
    pub anim_name_nonempty: unsafe extern "C" fn(Ctx, i32) -> bool,
    pub play_move_anim: unsafe extern "C" fn(Ctx, i32),
    pub set_move_anim_speed: unsafe extern "C" fn(Ctx, f32),
    pub idle_restart: unsafe extern "C" fn(Ctx),
    pub idle_extra_length: unsafe extern "C" fn(Ctx, i32, *mut f32) -> bool,
    pub play_idle_extra: unsafe extern "C" fn(Ctx, i32),
    pub pf_feet: unsafe extern "C" fn(Ctx, Out3),
    pub pf_path_len: unsafe extern "C" fn(Ctx) -> usize,
    pub pf_path_get: unsafe extern "C" fn(Ctx, usize, Out3),
    pub pf_build_path: unsafe extern "C" fn(Ctx, In3),
    pub pf_track_len: unsafe extern "C" fn(Ctx) -> usize,
    pub pf_track_info: unsafe extern "C" fn(Ctx, usize, *mut TrackNodeInfo),
    pub pf_track_node_pos: unsafe extern "C" fn(Ctx, usize, Out3) -> bool,
    pub door_count: unsafe extern "C" fn(Ctx) -> usize,
    pub door_hit: unsafe extern "C" fn(Ctx, usize, In3, In3, f32, Out3) -> bool,
}

pub(super) struct CHost<'a>(pub &'a OhplGs2Host);

fn get3(f: impl FnOnce(Out3)) -> Vector3f {
    let mut o = [0.0f32; 3];
    f(o.as_mut_ptr());
    v3(o[0], o[1], o[2])
}

fn a3(v: Vector3f) -> [f32; 3] {
    [v.x, v.y, v.z]
}

// SAFETY (all calls below): the caller of the `ohpl_gs2_*` entry point guarantees the table's
// callbacks accept its `ctx` and the pointers handed to them.
impl Host for CHost<'_> {
    fn has_body(&mut self) -> bool {
        unsafe { (self.0.has_body)(self.0.ctx) }
    }
    fn has_map(&mut self) -> bool {
        unsafe { (self.0.has_map)(self.0.ctx) }
    }
    fn has_mesh(&mut self) -> bool {
        unsafe { (self.0.has_mesh)(self.0.ctx) }
    }
    fn ent_pos(&mut self) -> Vector3f {
        get3(|o| unsafe { (self.0.ent_pos)(self.0.ctx, o) })
    }
    fn ent_scale_y(&mut self) -> f32 {
        unsafe { (self.0.ent_scale_y)(self.0.ctx) }
    }
    fn ent_global_space_anim(&mut self) -> bool {
        unsafe { (self.0.ent_global_space_anim)(self.0.ctx) }
    }
    fn body_pos(&mut self) -> Vector3f {
        get3(|o| unsafe { (self.0.body_pos)(self.0.ctx, o) })
    }
    fn body_feet(&mut self) -> Vector3f {
        get3(|o| unsafe { (self.0.body_feet)(self.0.ctx, o) })
    }
    fn body_size(&mut self) -> Vector3f {
        get3(|o| unsafe { (self.0.body_size)(self.0.ctx, o) })
    }
    fn body_forward(&mut self) -> Vector3f {
        get3(|o| unsafe { (self.0.body_forward)(self.0.ctx, o) })
    }
    fn body_yaw(&mut self) -> f32 {
        unsafe { (self.0.body_yaw)(self.0.ctx) }
    }
    fn body_pitch(&mut self) -> f32 {
        unsafe { (self.0.body_pitch)(self.0.ctx) }
    }
    fn body_entity_pitch_amount(&mut self) -> f32 {
        unsafe { (self.0.body_entity_pitch_amount)(self.0.ctx) }
    }
    fn body_move_speed(&mut self) -> f32 {
        unsafe { (self.0.body_move_speed)(self.0.ctx) }
    }
    fn body_on_ground(&mut self) -> bool {
        unsafe { (self.0.body_on_ground)(self.0.ctx) }
    }
    fn body_test_collision(&mut self) -> bool {
        unsafe { (self.0.body_test_collision)(self.0.ctx) }
    }
    fn body_gravity_active(&mut self) -> bool {
        unsafe { (self.0.body_gravity_active)(self.0.ctx) }
    }
    fn body_max_step_size(&mut self) -> f32 {
        unsafe { (self.0.body_max_step_size)(self.0.ctx) }
    }
    fn set_body_yaw(&mut self, yaw: f32) {
        unsafe { (self.0.set_body_yaw)(self.0.ctx, yaw) }
    }
    fn set_body_pos(&mut self, p: Vector3f) {
        unsafe { (self.0.set_body_pos)(self.0.ctx, a3(p).as_ptr()) }
    }
    fn set_body_feet(&mut self, p: Vector3f) {
        unsafe { (self.0.set_body_feet)(self.0.ctx, a3(p).as_ptr()) }
    }
    fn set_body_gravity(&mut self, on: bool) {
        unsafe { (self.0.set_body_gravity)(self.0.ctx, on) }
    }
    fn set_body_collision(&mut self, on: bool) {
        unsafe { (self.0.set_body_collision)(self.0.ctx, on) }
    }
    fn has_pos_bone(&mut self) -> bool {
        unsafe { (self.0.has_pos_bone)(self.0.ctx) }
    }
    fn pos_bone_world_pos(&mut self) -> Vector3f {
        get3(|o| unsafe { (self.0.pos_bone_world_pos)(self.0.ctx, o) })
    }
    fn set_mesh_matrix(&mut self, m: &Matrixf) {
        unsafe { (self.0.set_mesh_matrix)(self.0.ctx, m) }
    }
    fn player_body_present(&mut self) -> bool {
        unsafe { (self.0.player_body_present)(self.0.ctx) }
    }
    fn player_dead(&mut self) -> bool {
        unsafe { (self.0.player_dead)(self.0.ctx) }
    }
    fn player_visibility_range_mul(&mut self) -> f32 {
        unsafe { (self.0.player_visibility_range_mul)(self.0.ctx) }
    }
    fn player_visibility_max_range(&mut self) -> f32 {
        unsafe { (self.0.player_visibility_max_range)(self.0.ctx) }
    }
    fn player_pos(&mut self) -> Vector3f {
        get3(|o| unsafe { (self.0.player_pos)(self.0.ctx, o) })
    }
    fn player_feet(&mut self) -> Vector3f {
        get3(|o| unsafe { (self.0.player_feet)(self.0.ctx, o) })
    }
    fn player_size_y(&mut self) -> f32 {
        unsafe { (self.0.player_size_y)(self.0.ctx) }
    }
    fn line_of_sight(&mut self, from: Vector3f, to: Vector3f) -> bool {
        unsafe { (self.0.line_of_sight)(self.0.ctx, a3(from).as_ptr(), a3(to).as_ptr()) }
    }
    fn entity_in_player_los(&mut self) -> bool {
        unsafe { (self.0.entity_in_player_los)(self.0.ctx) }
    }
    fn floor_dist(&mut self, start: Vector3f, end: Vector3f) -> f32 {
        unsafe { (self.0.floor_dist)(self.0.ctx, a3(start).as_ptr(), a3(end).as_ptr()) }
    }
    fn body_last_pos(&mut self) -> Vector3f {
        get3(|o| unsafe { (self.0.body_last_pos)(self.0.ctx, o) })
    }
    fn body_right(&mut self) -> Vector3f {
        get3(|o| unsafe { (self.0.body_right)(self.0.ctx, o) })
    }
    fn body_velocity(&mut self, dt: f32) -> Vector3f {
        get3(|o| unsafe { (self.0.body_velocity)(self.0.ctx, dt, o) })
    }
    fn body_add_pitch(&mut self, p: f32) {
        unsafe { (self.0.body_add_pitch)(self.0.ctx, p) }
    }
    fn body_set_max_forward(&mut self, v: f32) {
        unsafe { (self.0.body_set_max_forward)(self.0.ctx, v) }
    }
    fn body_set_move_acc(&mut self, v: f32) {
        unsafe { (self.0.body_set_move_acc)(self.0.ctx, v) }
    }
    fn body_set_move_deacc(&mut self, v: f32) {
        unsafe { (self.0.body_set_move_deacc)(self.0.ctx, v) }
    }
    fn body_move_forward(&mut self) {
        unsafe { (self.0.body_move_forward)(self.0.ctx) }
    }
    fn raycast(&mut self, start: Vector3f, end: Vector3f) -> Option<(f32, Vector3f)> {
        let (mut dist, mut n) = (0.0f32, [0.0f32; 3]);
        let hit = unsafe {
            (self.0.raycast)(
                self.0.ctx,
                a3(start).as_ptr(),
                a3(end).as_ptr(),
                &mut dist,
                n.as_mut_ptr(),
            )
        };
        hit.then(|| (dist, v3(n[0], n[1], n[2])))
    }
    fn avoid_query(&mut self, center: Vector3f, dist: f32) -> usize {
        unsafe { (self.0.avoid_query)(self.0.ctx, a3(center).as_ptr(), dist) }
    }
    fn avoid_get(&mut self, i: usize) -> AvoidBody {
        let mut b = AvoidBody::default();
        unsafe { (self.0.avoid_get)(self.0.ctx, i, &mut b) };
        b
    }
    fn anim_current(&mut self) -> i32 {
        unsafe { (self.0.anim_current)(self.0.ctx) }
    }
    fn anim_name_nonempty(&mut self, state: i32) -> bool {
        unsafe { (self.0.anim_name_nonempty)(self.0.ctx, state) }
    }
    fn play_move_anim(&mut self, state: i32) {
        unsafe { (self.0.play_move_anim)(self.0.ctx, state) }
    }
    fn set_move_anim_speed(&mut self, speed: f32) {
        unsafe { (self.0.set_move_anim_speed)(self.0.ctx, speed) }
    }
    fn idle_restart(&mut self) {
        unsafe { (self.0.idle_restart)(self.0.ctx) }
    }
    fn idle_extra_length(&mut self, idx: i32) -> Option<f32> {
        let mut len = 0.0f32;
        unsafe { (self.0.idle_extra_length)(self.0.ctx, idx, &mut len) }.then_some(len)
    }
    fn play_idle_extra(&mut self, idx: i32) {
        unsafe { (self.0.play_idle_extra)(self.0.ctx, idx) }
    }
    fn pf_feet(&mut self) -> Vector3f {
        get3(|o| unsafe { (self.0.pf_feet)(self.0.ctx, o) })
    }
    fn pf_path_len(&mut self) -> usize {
        unsafe { (self.0.pf_path_len)(self.0.ctx) }
    }
    fn pf_path_get(&mut self, i: usize) -> Vector3f {
        get3(|o| unsafe { (self.0.pf_path_get)(self.0.ctx, i, o) })
    }
    fn pf_build_path(&mut self, goal: Vector3f) {
        unsafe { (self.0.pf_build_path)(self.0.ctx, a3(goal).as_ptr()) }
    }
    fn pf_track_len(&mut self) -> usize {
        unsafe { (self.0.pf_track_len)(self.0.ctx) }
    }
    fn pf_track_info(&mut self, i: usize) -> TrackNodeInfo {
        let mut t = TrackNodeInfo::default();
        unsafe { (self.0.pf_track_info)(self.0.ctx, i, &mut t) };
        t
    }
    fn pf_track_node_pos(&mut self, i: usize) -> Option<Vector3f> {
        let mut o = [0.0f32; 3];
        unsafe { (self.0.pf_track_node_pos)(self.0.ctx, i, o.as_mut_ptr()) }
            .then(|| v3(o[0], o[1], o[2]))
    }
    fn door_count(&mut self) -> usize {
        unsafe { (self.0.door_count)(self.0.ctx) }
    }
    fn door_hit(
        &mut self,
        idx: usize,
        origin: Vector3f,
        dir: Vector3f,
        max: f32,
    ) -> Option<Vector3f> {
        let mut o = [0.0f32; 3];
        let hit = unsafe {
            (self.0.door_hit)(
                self.0.ctx,
                idx,
                a3(origin).as_ptr(),
                a3(dir).as_ptr(),
                max,
                o.as_mut_ptr(),
            )
        };
        hit.then(|| v3(o[0], o[1], o[2]))
    }
}

pub struct OhplGs2Events(Events);

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs2_events_new() -> *mut OhplGs2Events {
    Box::into_raw(Box::new(OhplGs2Events(Events::default())))
}

/// # Safety
/// `e` comes from `ohpl_gs2_events_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_events_free(e: *mut OhplGs2Events) {
    if !e.is_null() {
        drop(unsafe { Box::from_raw(e) });
    }
}

/// # Safety
/// `e` is a live events handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_events_len(e: *const OhplGs2Events) -> usize {
    unsafe { e.as_ref() }.map_or(0, |e| e.0.0.len())
}

/// # Safety
/// `e` is a live events handle; the pointer is valid until the next step call on it.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_events_data(e: *const OhplGs2Events) -> *const Event {
    unsafe { e.as_ref() }.map_or(std::ptr::null(), |e| e.0.0.as_ptr())
}

/// # Safety
/// `e` is a live events handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_events_clear(e: *mut OhplGs2Events) {
    if let Some(e) = unsafe { e.as_mut() } {
        e.0.0.clear();
    }
}

fn parts<'a>(
    host: *const OhplGs2Host,
    ev: *mut OhplGs2Events,
) -> Option<(CHost<'a>, &'a mut Events)> {
    // SAFETY: the C++ caller passes a live host table and events handle.
    let host = unsafe { host.as_ref() }?;
    let ev = unsafe { ev.as_mut() }?;
    Some((CHost(host), &mut ev.0))
}

macro_rules! agent_fn {
    ($name:ident ($($arg:ident: $t:ty),*) |$s:ident, $h:ident, $ev:ident| $body:expr) => {
        /// # Safety
        /// All pointers are live; `st` is the agent's scalar state.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(
            st: *mut AgentState,
            host: *const OhplGs2Host,
            ev: *mut OhplGs2Events,
            $($arg: $t),*
        ) {
            let (Some($s), Some(($h, $ev))) = (unsafe { st.as_mut() }, parts(host, ev)) else {
                return;
            };
            let mut $h = $h;
            $body
        }
    };
}

agent_fn!(ohpl_gs2_agent_pre_update(dt: f32) |s, h, ev| s.pre_update(dt, &mut h, ev));
agent_fn!(ohpl_gs2_agent_step_up() | s, h, _ev | s.step_up(&mut h));
agent_fn!(ohpl_gs2_agent_align(dt: f32) |s, h, _ev| s.align_with_ground(dt, &mut h));
agent_fn!(ohpl_gs2_agent_set_senses_active(on: bool) |s, _h, ev| s.set_senses_active(on, ev));
agent_fn!(ohpl_gs2_agent_sync_mesh(offset: *const Matrixf, roll: f32) |s, h, _ev| {
    if let Some(m) = unsafe { offset.as_ref() } {
        s.sync_mesh(m, roll, &mut h);
    }
});

/// # Safety
/// `st` is a live agent state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_agent_reset_detection(st: *mut AgentState) {
    if let Some(s) = unsafe { st.as_mut() } {
        s.reset_detection();
    }
}

/// # Safety
/// `l` is a live listener state, `agent_pos` and `pos` point at three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_sound_heard(
    l: *const SoundListener,
    active: bool,
    ent_active: bool,
    agent_pos: In3,
    pos: In3,
    radius: f32,
    prio: i32,
) -> bool {
    let (Some(l), false, false) = (unsafe { l.as_ref() }, agent_pos.is_null(), pos.is_null())
    else {
        return false;
    };
    let rd = |p: In3| {
        // SAFETY: caller passes three floats.
        let p = unsafe { std::slice::from_raw_parts(p, 3) };
        v3(
            p.first().copied().unwrap_or(0.0),
            p.get(1).copied().unwrap_or(0.0),
            p.get(2).copied().unwrap_or(0.0),
        )
    };
    l.hears(active, ent_active, rd(agent_pos), rd(pos), radius, prio)
}

/// # Safety
/// `timer` is live, `states` points at `n` states, `agent` is the agent state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_proximity_update(
    timer: *mut f32,
    states: *mut ProxState,
    n: usize,
    agent: *const AgentState,
    ai: bool,
    dt: f32,
    host: *const OhplGs2Host,
    ev: *mut OhplGs2Events,
) {
    let (Some(timer), Some(agent), Some((mut h, ev))) = (
        unsafe { timer.as_mut() },
        unsafe { agent.as_ref() },
        parts(host, ev),
    ) else {
        return;
    };
    let states = if n == 0 || states.is_null() {
        &mut []
    } else {
        // SAFETY: caller guarantees `n` states.
        unsafe { std::slice::from_raw_parts_mut(states, n) }
    };
    proximity_update(timer, states, dt, agent, ai, &mut h, ev);
}

/// # Safety
/// `count` is a live float.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_bark_due(
    count: *mut f32,
    dt: f32,
    active: bool,
    has_sound: bool,
    min: f32,
    max: f32,
) -> bool {
    unsafe { count.as_mut() }.is_some_and(|c| bark_due(c, dt, active, has_sound, min, max))
}

/// # Safety
/// `cm` is the mover state; `walls` points at `n` samples (or is null with `n == 0`); `speed` is
/// the current speed state or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_cm_update(
    cm: *mut CharMoverState,
    speed: *const SpeedState,
    walls: *mut WallSample,
    n: usize,
    dt: f32,
    host: *const OhplGs2Host,
    ev: *mut OhplGs2Events,
) {
    let (Some(cm), Some((mut h, ev))) = (unsafe { cm.as_mut() }, parts(host, ev)) else {
        return;
    };
    let walls: &mut [WallSample] = if n == 0 || walls.is_null() {
        &mut []
    } else {
        // SAFETY: caller guarantees `n` samples.
        unsafe { std::slice::from_raw_parts_mut(walls, n) }
    };
    cm.update(dt, unsafe { speed.as_ref() }, walls, &mut h, ev);
}

fn read3(p: In3) -> Vector3f {
    // SAFETY: callers pass a pointer to three floats.
    let s = unsafe { std::slice::from_raw_parts(p, 3) };
    let g = |i: usize| s.get(i).copied().unwrap_or(0.0);
    v3(g(0), g(1), g(2))
}

/// # Safety
/// `cm` is the mover state, `pos` three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_cm_move_to_pos(
    cm: *mut CharMoverState,
    pos: In3,
    slow_down: bool,
) {
    if let (Some(cm), false) = (unsafe { cm.as_mut() }, pos.is_null()) {
        cm.move_to_pos(read3(pos), slow_down);
    }
}

/// # Safety
/// `cm` is the mover state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_cm_stop(cm: *mut CharMoverState) {
    if let Some(cm) = unsafe { cm.as_mut() } {
        cm.stop();
    }
}

/// # Safety
/// `cm` is the mover state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_cm_turn_to(cm: *mut CharMoverState, yaw: f32) {
    if let Some(cm) = unsafe { cm.as_mut() } {
        cm.turn_to(yaw);
    }
}

/// # Safety
/// `cm` is the mover state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_cm_set_use_move_state_anims(cm: *mut CharMoverState, on: bool) {
    if let Some(cm) = unsafe { cm.as_mut() } {
        cm.set_use_move_state_anims(on);
    }
}

/// # Safety
/// `pf` is the pathfinder state, `cm` the mover state or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_pf_update(
    pf: *mut PathfinderState,
    cm: *mut CharMoverState,
    dt: f32,
    host: *const OhplGs2Host,
    ev: *mut OhplGs2Events,
) {
    let (Some(pf), Some((mut h, ev))) = (unsafe { pf.as_mut() }, parts(host, ev)) else {
        return;
    };
    pf.update(dt, unsafe { cm.as_mut() }, &mut h, ev);
}

/// # Safety
/// As `ohpl_gs2_pf_update`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_pf_send_goal(
    pf: *mut PathfinderState,
    cm: *mut CharMoverState,
    host: *const OhplGs2Host,
) {
    let (Some(pf), Some(host)) = (unsafe { pf.as_mut() }, unsafe { host.as_ref() }) else {
        return;
    };
    pf.send_goal(unsafe { cm.as_mut() }, &mut CHost(host));
}

/// # Safety
/// As `ohpl_gs2_pf_update`; `goal` is three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_pf_move_to(
    pf: *mut PathfinderState,
    cm: *mut CharMoverState,
    host: *const OhplGs2Host,
    goal: In3,
    exact: bool,
    in_map: bool,
) {
    let (Some(pf), Some(host), false) = (
        unsafe { pf.as_mut() },
        unsafe { host.as_ref() },
        goal.is_null(),
    ) else {
        return;
    };
    pf.move_to(
        read3(goal),
        exact,
        in_map,
        unsafe { cm.as_mut() },
        &mut CHost(host),
    );
}

/// # Safety
/// `pf` is the pathfinder state, `cm` the mover state or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_pf_stop(pf: *mut PathfinderState, cm: *mut CharMoverState) {
    if let Some(pf) = unsafe { pf.as_mut() } {
        pf.stop(unsafe { cm.as_mut() });
    }
}

/// # Safety
/// As `ohpl_gs2_pf_update`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_pf_on_message(
    pf: *mut PathfinderState,
    cm: *mut CharMoverState,
    msg: i32,
    host: *const OhplGs2Host,
    ev: *mut OhplGs2Events,
) {
    let (Some(pf), Some((mut h, ev))) = (unsafe { pf.as_mut() }, parts(host, ev)) else {
        return;
    };
    pf.on_message(msg, unsafe { cm.as_mut() }, &mut h, ev);
}

/// # Safety
/// `pod` is the state machine's POD half; it must not already own a Rust half.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_sm_init(pod: *mut SmPod) {
    if let Some(p) = unsafe { pod.as_mut() } {
        p.rs = Box::into_raw(Box::new(SmExtra::new()));
    }
}

/// # Safety
/// `pod` was initialised with `ohpl_gs2_sm_init` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_sm_free(pod: *mut SmPod) {
    if let Some(p) = unsafe { pod.as_mut() } {
        if !p.rs.is_null() {
            drop(unsafe { Box::from_raw(p.rs) });
        }
        p.rs = std::ptr::null_mut();
    }
}

/// # Safety
/// `pod` is an initialised state machine POD.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_sm_reset(pod: *mut SmPod, has_states: bool) {
    if let Some(p) = unsafe { pod.as_mut() } {
        p.reset(has_states);
    }
}

/// # Safety
/// `pod` is an initialised state machine POD.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_sm_begin_update(pod: *mut SmPod, dt: f32) {
    if let Some(p) = unsafe { pod.as_mut() } {
        p.begin_update(dt);
    }
}

/// # Safety
/// `pod` is an initialised state machine POD.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_sm_begin_message(pod: *mut SmPod, msg: i32) {
    if let Some(p) = unsafe { pod.as_mut() } {
        p.begin_message(msg);
    }
}

/// # Safety
/// `pod` is an initialised state machine POD, `out` an action; `exists(ctx, sub, id)` must not
/// re-enter the state machine.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_sm_next(
    pod: *mut SmPod,
    ctx: Ctx,
    exists: unsafe extern "C" fn(Ctx, bool, i32) -> bool,
    out: *mut Action,
) -> bool {
    let (Some(p), Some(out)) = (unsafe { pod.as_mut() }, unsafe { out.as_mut() }) else {
        return false;
    };
    // SAFETY: the callback only reads the C++ state maps.
    match p.next(&|sub, id| unsafe { exists(ctx, sub, id) }) {
        Some(a) => {
            *out = a;
            true
        }
        None => false,
    }
}

fn sm_extra<'a>(pod: *mut SmPod) -> Option<&'a mut SmExtra> {
    // SAFETY: `pod` is an initialised POD whose `rs` is owned by it.
    unsafe { pod.as_mut().and_then(|p| p.rs.as_mut()) }
}

/// # Safety
/// `pod` is an initialised state machine POD.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_sm_add_timer(pod: *mut SmPod, sub: bool, id: u64, time: f32) {
    if let Some(x) = sm_extra(pod) {
        x.add_timer(sub, id, time);
    }
}

/// # Safety
/// `pod` is an initialised state machine POD.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_sm_stop_timer(pod: *mut SmPod, sub: bool, id: u64) {
    if let Some(x) = sm_extra(pod) {
        x.stop_timer(sub, id);
    }
}

/// # Safety
/// `pod` is an initialised state machine POD.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_sm_timer_exists(pod: *mut SmPod, sub: bool, id: u64) -> bool {
    sm_extra(pod).is_some_and(|x| x.timer_exists(sub, id))
}

/// # Safety
/// `pod` is an initialised state machine POD.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_sm_timer_len(pod: *mut SmPod, sub: bool) -> usize {
    sm_extra(pod).map_or(0, |x| x.timer_len(sub))
}

/// # Safety
/// As `ohpl_gs2_pf_update`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_pf_start_track_node(
    pf: *mut PathfinderState,
    cm: *mut CharMoverState,
    host: *const OhplGs2Host,
) {
    let (Some(pf), Some(host)) = (unsafe { pf.as_mut() }, unsafe { host.as_ref() }) else {
        return;
    };
    pf.start_track_node(unsafe { cm.as_mut() }, &mut CHost(host));
}

/// # Safety
/// As `ohpl_gs2_pf_update`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs2_pf_go_to_next_track_node(
    pf: *mut PathfinderState,
    cm: *mut CharMoverState,
    host: *const OhplGs2Host,
    ev: *mut OhplGs2Events,
) {
    let (Some(pf), Some((mut h, ev))) = (unsafe { pf.as_mut() }, parts(host, ev)) else {
        return;
    };
    pf.go_to_next_track_node(unsafe { cm.as_mut() }, &mut h, ev);
}
