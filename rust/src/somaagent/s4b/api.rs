//! The `SomaAgent.h` surface the rest of the game calls: creation, save, debug and sound.

use super::super::s4::{
    DebugCm, DebugPf, DebugSm, PathBlob, Track, debug_text, path_load, path_save,
};
use super::core::MSG_SOUND_HEARD;
use super::core::{
    agent_pos, agent_ptrs, cm_of, ent_active, find, pf_of, send_message, sm_of, sync_mesh_of,
};
use super::host::H;
use super::io::{Io, op};
use super::model::{COMP_SOUND_LISTENER, K, TrackNode, Tstr};
use crate::math::{PI, Vector3f, atan2, matrix_mul, matrix_rotate, matrix_scale, to_rad, v3};

fn vars_of(ent: usize, which: i32) -> usize {
    Io::new().p(0, ent).i(0, which).go(op::E_VARS).ptr(0)
}

fn var_vec(vars: usize, name: &[u8], d: Vector3f) -> Vector3f {
    Io::new()
        .p(0, vars)
        .s(0, name)
        .v(0, d)
        .go(op::V_VEC)
        .out_v(0)
}

fn var_bool(vars: usize, name: &[u8], d: bool) -> bool {
    Io::new()
        .p(0, vars)
        .s(0, name)
        .i(0, i32::from(d))
        .go(op::V_BOOL)
        .int(0)
        != 0
}

pub fn create_agent(ent: usize) {
    if Io::new().p(0, ent).go(op::E_FLAGS).int(0) == 0 {
        return;
    }
    let ex = super::core::find(ent);
    // SAFETY: a registry agent or null.
    if unsafe { ex.as_ref() }.is_some_and(|a| a.body != 0) {
        return;
    }
    let a = super::core::agent_or_new(ent);
    let vars = vars_of(ent, 0);
    let inst = vars_of(ent, 1);
    let scale = Io::new().p(0, ent).go(op::E_SCALE3).out_v(0);
    let sz = var_vec(vars, b"CharBodySize", v3(0.9, 1.9, 0.9));
    let size = v3(sz.x * scale.x, sz.y * scale.y, sz.z * scale.z);
    let body = Io::new().p(0, ent).v(0, size).go(op::B_CREATE).ptr(1);
    let on = Io::new().p(0, ent).go(op::E_ONLOAD);
    Io::new()
        .p(1, body)
        .i(0, 1)
        .v(0, on.out_v(0))
        .go(op::B_SET3);
    Io::new()
        .p(1, body)
        .f(0, atan2(on.fl(3), on.fl(4)) - PI)
        .go(op::B_SET_YAW);
    let r = var_vec(vars, b"MeshRotationOffset", v3(0.0, 0.0, 0.0));
    let mut off = matrix_rotate(v3(to_rad(r.x), to_rad(r.y), to_rad(r.z)), 0);
    off = matrix_mul(
        &off,
        &matrix_scale(var_vec(vars, b"MeshScaleOffset", v3(1.0, 1.0, 1.0))),
    );
    let t = var_vec(vars, b"MeshPositionOffset", v3(0.0, 0.0, 0.0));
    off.m[0][3] = t.x;
    off.m[1][3] = t.y;
    off.m[2][3] = t.z;
    let pos_bone = {
        let name = Io::new()
            .p(0, vars)
            .s(0, b"CharBodyPosBone")
            .s(1, b"")
            .go(op::V_STR)
            .out_s(0)
            .to_vec();
        Io::new()
            .p(0, ent)
            .s(0, &name)
            .i(0, 1)
            .go(op::BN_BY_NAME)
            .ptr(0)
    };
    let feet = var_bool(vars, b"CharBodyBoneIsFeet", true);
    let y_off = Io::new()
        .p(0, vars)
        .s(0, b"CharBodyBoneYOffset")
        .f(0, 0.0)
        .go(op::V_FLOAT)
        .fl(0);
    let senses = var_bool(inst, b"SensesActive", true);
    let stat = var_bool(inst, b"StaticCollider", false);
    // SAFETY: a registry agent.
    unsafe {
        (*a).body = body;
        (*a).mesh_offset = matrix_mul(&matrix_scale(scale), &off);
        (*a).pos_bone = pos_bone;
        (*a).st.pos_bone_is_feet = feet;
        (*a).st.pos_bone_y_offset = y_off;
        (*a).st.senses_active = senses;
        (*a).st.static_collider = stat;
    }
    let g = i32::from(!stat);
    Io::new().p(1, body).i(0, 0).i(1, g).go(op::B_SETB);
    Io::new().p(1, body).i(0, 1).i(1, g).go(op::B_SETB);
    Io::new()
        .p(1, body)
        .i(0, 2)
        .i(1, i32::from(ent_active(ent)))
        .go(op::B_SETB);
    sync_mesh_of(ent);
}

pub fn get_pos(ent: usize) -> Option<Vector3f> {
    let a = find(ent);
    // SAFETY: a registry agent or null.
    let a = unsafe { a.as_ref() }?;
    (a.body != 0).then(|| Io::new().p(1, a.body).i(0, 0).go(op::B_GET3).out_v(0))
}

pub fn set_pos(ent: usize, p: Vector3f) -> bool {
    let a = find(ent);
    // SAFETY: a registry agent or null.
    let Some(a) = (unsafe { a.as_ref() }) else {
        return false;
    };
    if a.body == 0 {
        return false;
    }
    Io::new().p(1, a.body).i(0, 0).v(0, p).go(op::B_SET3);
    sync_mesh_of(ent);
    true
}

pub fn body_of(ent: usize) -> usize {
    // SAFETY: a registry agent or null.
    unsafe { find(ent).as_ref() }.map_or(0, |a| a.body)
}

pub fn get_state(ent: usize) -> i32 {
    // SAFETY: a registry state machine or null.
    unsafe { sm_of(find(ent)).as_ref() }.map_or(-1, |s| s.pod.cur)
}

pub fn save_extra(ent: usize) -> (f32, bool, bool) {
    let a = find(ent);
    // SAFETY: a registry agent or null.
    let Some(a) = (unsafe { a.as_ref() }) else {
        return (0.0, false, false);
    };
    let yaw = Io::new().p(1, a.body).i(0, 0).go(op::B_GETF).fl(0);
    (yaw, a.st.senses_active, a.st.update_detection)
}

pub fn load_extra(ent: usize, yaw: f32, senses: bool, detection: bool) {
    let a = find(ent);
    // SAFETY: a registry agent or null.
    let Some(a) = (unsafe { a.as_mut() }) else {
        return;
    };
    Io::new().p(1, a.body).f(0, yaw).go(op::B_SET_YAW);
    a.st.senses_active = senses;
    a.st.update_detection = detection;
    sync_mesh_of(ent);
}

pub fn save_path(ent: usize) -> Vec<u8> {
    // SAFETY: a registry pathfinder or null.
    let Some(p) = (unsafe { pf_of(find(ent)).as_ref() }) else {
        return Vec::new();
    };
    path_save(&PathBlob {
        moving: p.st.moving,
        goal: p.st.goal,
        exact: p.st.exact,
        result: p.result_cb.clone(),
        in_map: p.st.callback_in_map,
        end_cb: p.end_cb.clone(),
        tracks: p
            .track
            .iter()
            .map(|n| Track {
                node: n.node.get().to_vec(),
                min_wait: n.min_wait,
                max_wait: n.max_wait,
                anim: n.anim.get().to_vec(),
                looped: n.looped,
            })
            .collect(),
        track_idx: p.st.track_idx,
        track_active: p.st.track_active,
        track_paused: p.st.track_paused,
        track_loop: p.st.track_loop,
        at_node: p.st.at_track_node,
        track_wait: p.st.track_wait,
        track_freq: p.st.track_freq,
        track_cb: p.track_cb.get().to_vec(),
    })
}

pub fn load_path(ent: usize, data: &[u8]) {
    let a = find(ent);
    let pf = pf_of(a);
    // SAFETY: a registry pathfinder or null.
    let Some(p) = (unsafe { pf.as_mut() }) else {
        return;
    };
    if data.is_empty() {
        return;
    }
    let cur = PathBlob {
        track_idx: p.st.track_idx,
        track_active: p.st.track_active,
        track_paused: p.st.track_paused,
        track_loop: p.st.track_loop,
        at_node: p.st.at_track_node,
        track_wait: p.st.track_wait,
        track_freq: p.st.track_freq,
        ..PathBlob::default()
    };
    p.track.clear();
    let o = path_load(data, &cur);
    p.end_cb = o.end_cb.clone();
    p.track_cb.set(&o.track_cb);
    for t in &o.tracks {
        p.track.push(Box::new(TrackNode {
            node: Tstr::new(&t.node),
            min_wait: t.min_wait,
            max_wait: t.max_wait,
            anim: Tstr::new(&t.anim),
            looped: t.looped,
        }));
    }
    p.st.track_idx = o.track_idx;
    p.st.track_active = o.track_active;
    p.st.track_paused = o.track_paused;
    p.st.track_loop = o.track_loop;
    p.st.at_track_node = o.at_node;
    p.st.track_wait = o.track_wait;
    p.st.track_freq = o.track_freq;
    if o.moving {
        p.result_cb = o.result.clone();
        let cmp = cm_of(a);
        // SAFETY: live components; the host reads other fields through raw pointers.
        unsafe {
            p.st.move_to(
                v3(o.goal[0], o.goal[1], o.goal[2]),
                o.exact,
                o.in_map,
                cmp.as_mut().map(|m| &mut m.st),
                &mut H(a),
            );
        }
    } else {
        let cmp = cm_of(a);
        // SAFETY: a live mover or null.
        p.st.stop(unsafe { cmp.as_mut() }.map(|m| &mut m.st));
    }
}

pub fn debug(ent: usize) -> Vec<u8> {
    let a = find(ent);
    if a.is_null() {
        return Vec::new();
    }
    let sm = sm_of(a);
    let (pf, cm) = (pf_of(a), cm_of(a));
    // SAFETY: registry components or null.
    let (sm, pf, cm) = unsafe { (sm.as_ref(), pf.as_ref(), cm.as_ref()) };
    let name = |s: &super::model::Sm, id: i32| s.states.get(&id).cloned().unwrap_or_default();
    let (cur, prev, next) = sm.map_or((Vec::new(), Vec::new(), Vec::new()), |s| {
        (name(s, s.pod.cur), name(s, s.pod.prev), name(s, s.pod.next))
    });
    let dsm = sm.map(|s| DebugSm {
        cur: &cur,
        prev: &prev,
        next: &next,
        timers: s.x.timer_len(false) as i32,
    });
    let dpf = pf.map(|p| DebugPf {
        moving: p.st.moving,
        path_idx: p.st.path_idx as i32,
        path_len: p.path.len() as i32,
        goal: p.st.goal,
        track_idx: p.st.track_idx,
        track_len: p.track.len() as i32,
        track_active: p.st.track_active,
        paused: p.st.track_paused,
        at_node: p.st.at_track_node,
        wait: p.st.track_wait,
    });
    let anim = Io::new()
        .p(0, ent)
        .go(op::E_ANIM_CUR_NAME)
        .out_s(0)
        .to_vec();
    let dcm = cm.map(|m| DebugCm {
        moving: m.st.moving,
        goal: m.st.goal,
        speed_state: m.st.speed_state,
        fwd: m
            .speeds
            .get(&m.st.speed_state)
            .map_or(m.st.max_forward, |s| s.forward),
        stuck: m.st.stuck,
        turning: m.st.turning,
        anim: &anim,
        idle_extra_count: m.st.idle_extra_count,
        idle_extra_wait: m.st.idle_extra_wait,
        wall: m.st.wall_avoid,
        wall_hits: m.walls.iter().filter(|w| w.hit).count() as i32,
        roll: m.st.roll,
        object_avoid: m.st.object_avoid,
    });
    debug_text(dsm.as_ref(), dpf.as_ref(), dcm.as_ref())
}

pub fn broadcast_sound(pos: Vector3f, radius: f32, prio: i32) {
    for a in agent_ptrs() {
        // SAFETY: registry agents; re-found below because a message can remove agents.
        let ent = unsafe { (*a).ent };
        let a = find(ent);
        if a.is_null() {
            continue;
        }
        let c = super::core::comp_of(a, COMP_SOUND_LISTENER);
        // SAFETY: a registry component or null.
        let Some(c) = (unsafe { c.as_ref() }) else {
            continue;
        };
        let K::Sl(l) = &c.k else { continue };
        let at = agent_pos(a, ent);
        if l.hears(c.active, ent_active(ent), at, pos, radius, prio) {
            send_message(ent, MSG_SOUND_HEARD, pos, prio);
        }
    }
}
