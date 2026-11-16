//! Per-component update, activation and teardown.

use super::super::host::Events;
use super::super::misc::{bark_due, proximity_update};
use super::super::{normalized, std_min};
use super::core::{cm_of, ent_active, pf_of, replay, sm_drive};
use super::host::H;
use super::io::{Io, op};
use super::model::{Agent, Atk, AtkType, Bt, Comp, Fe, K};
use crate::math::{
    Matrixf, Quaternion, TWO_PI, Vector3f, acos, clamp, dot3, fma, len3, matrix_mul_vec,
    matrix_quaternion, matrix_rotate, quat_dot, quat_mul, quat_slerp, rand_rectf, sub3, v3,
};

fn m_world_pos(ent: usize) -> Vector3f {
    Io::new().p(0, ent).go(op::M_WORLD_POS).out_v(0)
}

fn m_rot_quat(ent: usize) -> Quaternion {
    let io = Io::new().p(0, ent).go(op::M_ROT_QUAT);
    Quaternion {
        v: v3(io.fl(1), io.fl(2), io.fl(3)),
        w: io.fl(0),
    }
}

pub fn update(ent: usize, a: *mut Agent, c: *mut Comp, dt: f32) {
    // SAFETY: `c` is a component of the registry agent `a`.
    let comp = unsafe { &mut *c };
    match &mut comp.k {
        K::Sm(s) => {
            s.pod.begin_update(dt);
            sm_drive(ent, c);
        }
        K::Cm(_) => {
            let cm = cm_of(a);
            let mut ev = Events::default();
            // SAFETY: the mover of this agent; the host reads other fields through raw pointers.
            unsafe {
                let m = &mut *cm;
                m.st.update(
                    dt,
                    m.speeds.get(&m.st.speed_state),
                    &mut m.walls,
                    &mut H(a),
                    &mut ev,
                );
            }
            replay(ent, &ev);
        }
        K::Pf(_) => {
            let (pf, cm) = (pf_of(a), cm_of(a));
            let mut ev = Events::default();
            // SAFETY: live components; the host reads other fields through raw pointers.
            unsafe {
                let mover = cm.as_mut().map(|m| &mut m.st);
                (*pf).st.update(dt, mover, &mut H(a), &mut ev);
            }
            replay(ent, &ev);
        }
        K::Bark(b) => {
            let cur = b
                .states
                .get(&b.cur)
                .filter(|s| !s.sound.is_empty())
                .cloned();
            let has = cur.is_some();
            let (min, max) = cur.as_ref().map_or((0.0, 0.0), |s| (s.min, s.max));
            if !bark_due(&mut b.count, dt, comp.active, has, min, max) {
                return;
            }
            if let Some(s) = cur {
                Io::new().p(0, ent).s(0, &s.sound).go(op::W_BARK);
            }
        }
        K::Fe(f) => fe_update(f, dt),
        K::Bt(b) => bt_update(ent, b, dt),
        K::Prox(p) => {
            // SAFETY: a registry agent.
            let ai = unsafe { (*a).body } != 0;
            let mut ev = Events::default();
            // SAFETY: the agent's state is read through a raw pointer; the host does the same.
            unsafe {
                proximity_update(
                    &mut p.timer,
                    &mut p.states,
                    dt,
                    &(*a).st,
                    ai,
                    &mut H(a),
                    &mut ev,
                );
            }
            replay(ent, &ev);
        }
        K::Atk(k) => atk_update(ent, a, k, dt),
        K::User(shell) => {
            Io::new().p(0, *shell).f(0, dt).go(op::U_UPDATE);
        }
        _ => {}
    }
}

fn fe_update(f: &mut Fe, dt: f32) {
    if f.body == 0 {
        return;
    }
    let vel = |dt: f32| Io::new().p(1, f.body).f(0, dt).go(op::B_VEL).out_v(0);
    let fade = |t: f32, time: f32| {
        Io::new()
            .p(0, f.field)
            .f(0, t)
            .f(1, time)
            .i(0, 2)
            .go(op::FF_FADE);
    };
    let dead = || Io::new().p(0, f.field).go(op::FF_IS_DEAD).int(0) != 0;
    if f.max_speed > 0.0 {
        let speed = len3(vel(dt));
        if speed > f.min_speed {
            fade(clamp(speed / (f.max_speed - f.min_speed), 0.0, 1.0), 0.25);
        } else if speed < f.min_speed * 0.66 && !dead() {
            Io::new().p(0, f.field).f(0, 1.0).i(0, 1).go(op::FF_FADE);
        }
    }
    if dead() {
        return;
    }
    let get3 = |w: i32| Io::new().p(1, f.body).i(0, w).go(op::B_GET3).out_v(0);
    let mut pos = if f.at_foot { get3(1) } else { get3(0) };
    if f.offset.x != 0.0 || f.offset.y != 0.0 || f.offset.z != 0.0 {
        let getf = |w: i32| Io::new().p(1, f.body).i(0, w).go(op::B_GETF).fl(0);
        let rot = matrix_rotate(v3(getf(1), getf(0), 0.0), 4);
        let d = matrix_mul_vec(&rot, f.offset);
        pos = v3(pos.x + d.x, pos.y + d.y, pos.z + d.z);
    }
    Io::new().p(0, f.field).v(0, pos).go(op::FF_SET_POS);
}

impl Bt {
    fn slot(&self, i: usize) -> usize {
        let n = self.trail.len();
        (self.head + n)
            .wrapping_sub(1)
            .wrapping_sub(i)
            .checked_rem(n)
            .unwrap_or(0)
    }

    fn get(&self, i: usize) -> (Quaternion, f32) {
        self.trail.get(self.slot(i)).copied().unwrap_or_default()
    }

    fn get_mut(&mut self, i: usize) -> Option<&mut (Quaternion, f32)> {
        let k = self.slot(i);
        self.trail.get_mut(k)
    }
}

pub fn bt_setup(ent: usize, b: &mut Bt, names: &[Vec<u8>]) {
    b.bones.clear();
    if Io::new().p(0, ent).go(op::E_FLAGS).int(1) == 0 {
        return;
    }
    for name in names {
        let bone = Io::new()
            .p(0, ent)
            .s(0, name)
            .i(0, 0)
            .go(op::BN_BY_NAME)
            .ptr(0);
        if bone == 0 {
            let mut m = b"Could not find bone '".to_vec();
            m.extend_from_slice(name);
            m.extend_from_slice(b"' in '");
            m.extend_from_slice(&super::core::ent_name(ent));
            m.extend_from_slice(b"' to be used as backbone!\n");
            Io::new().s(0, &m).go(op::LOG_ERR);
        } else {
            b.bones.push((bone, 0.0));
        }
    }
    b.last_pos = m_world_pos(ent);
    b.last = m_rot_quat(ent);
    let wp = |bone: usize| Io::new().p(0, bone).go(op::BN_WORLD_POS).out_v(0);
    let (mut length, mut prev_bone) = (0.0f32, 0usize);
    for (i, bone) in b.bones.iter_mut().enumerate() {
        Io::new().p(0, bone.0).go(op::BN_USE_PRE);
        if i > 0 {
            length += len3(sub3(wp(bone.0), wp(prev_bone)));
        }
        bone.1 = length;
        prev_bone = bone.0;
    }
    b.length = length;
}

fn bt_update(ent: usize, b: &mut Bt, dt: f32) {
    if Io::new().p(0, ent).go(op::E_FLAGS).int(1) == 0 || b.bones.is_empty() {
        return;
    }
    let pos = m_world_pos(ent);
    let rot = m_rot_quat(ent);
    if b.count > 0 {
        let angle = 2.0 * acos(quat_dot(&b.last, &rot).abs().min(1.0));
        let add = fma(angle / TWO_PI, b.length, len3(sub3(pos, b.last_pos))) * b.stiffness;
        for i in 0..b.count {
            if let Some(t) = b.get_mut(i) {
                t.1 += add;
            }
        }
    }
    b.last_pos = pos;
    b.last = rot;
    b.timer += dt;
    if b.timer >= b.interval || b.count == 0 {
        b.timer = 0.0;
        let h = b.head;
        if let Some(t) = b.trail.get_mut(h) {
            *t = (rot, 0.0);
        }
        b.count = (b.count + 1).min(b.trail.len());
        b.head = (b.head + 1).checked_rem(b.trail.len()).unwrap_or(0);
    }
    if Io::new().p(0, ent).go(op::M_VISIBLE).int(0) == 0 {
        return;
    }
    let inv = Quaternion {
        v: v3(-rot.v.x, -rot.v.y, -rot.v.z),
        w: rot.w,
    };
    for (bone, dist) in b.bones.clone() {
        let mut prev: i32 = -1;
        while ((prev + 1) as usize) < b.count && b.get((prev + 1) as usize).1 < dist {
            prev += 1;
        }
        let (qa0, prev_dist) = if prev < 0 {
            (rot, 0.0)
        } else {
            let t = b.get(prev as usize);
            (t.0, t.1)
        };
        let qa = quat_mul(&inv, &qa0);
        let mut qb = qa;
        if ((prev + 1) as usize) < b.count {
            let next = b.get((prev + 1) as usize);
            let seg = next.1 - prev_dist;
            if seg >= 1e-5 {
                qb = quat_slerp(
                    (dist - prev_dist) / seg,
                    &qa,
                    &quat_mul(&inv, &next.0),
                    true,
                );
            }
        }
        let m: Matrixf = matrix_quaternion(&qb);
        Io::new()
            .p(0, bone)
            .p(1, std::ptr::from_ref(&m) as usize)
            .go(op::BN_SET_PRE);
    }
}

fn atk_update(ent: usize, a: *mut Agent, k: &mut Atk, dt: f32) {
    for ti in 0..k.types.len() {
        let Some(t) = k.types.get_mut(ti) else {
            continue;
        };
        if !t.exists || !t.active {
            continue;
        }
        if t.duration > 0.0 {
            t.duration -= dt;
            if t.duration <= 0.0 {
                t.active = false;
            }
        }
        t.timer -= dt;
        if t.timer > 0.0 {
            continue;
        }
        t.timer = if t.freq > 0.0001 {
            1.0 / t.freq
        } else {
            10000.0
        };
        let (tc, shapes) = (t.clone(), t.shapes.clone());
        for s in &shapes {
            shape_damage(ent, a, &tc, s.socket, s.offset, s.radius);
        }
    }
}

fn shape_damage(ent: usize, a: *mut Agent, t: &AtkType, socket: i32, off: Vector3f, radius: f32) {
    let info = Io::new().go(op::P_INFO);
    if !t.player || info.int(0) == 0 {
        return;
    }
    let m = Io::new().p(0, ent).i(0, socket).go(op::E_ATTACK_MATRIX);
    let mtx = Matrixf {
        m: [
            [m.fl(0), m.fl(1), m.fl(2), m.fl(3)],
            [m.fl(4), m.fl(5), m.fl(6), m.fl(7)],
            [m.fl(8), m.fl(9), m.fl(10), m.fl(11)],
            [m.fl(12), m.fl(13), m.fl(14), m.fl(15)],
        ],
    };
    let pos = matrix_mul_vec(&mtx, off);
    let (body, size) = (info.out_v(2), v3(info.fl(9), info.fl(8), info.fl(10)));
    let closest = v3(
        body.x,
        clamp(pos.y, fma(-size.y, 0.5, body.y), fma(size.y, 0.5, body.y)),
        body.z,
    );
    if len3(sub3(closest, pos)) > fma(size.x, 0.5, radius) {
        return;
    }
    // SAFETY: a registry agent.
    let origin = if unsafe { (*a).body } != 0 {
        Io::new()
            .p(1, unsafe { (*a).body })
            .i(0, 0)
            .go(op::B_GET3)
            .out_v(0)
    } else {
        Io::new().p(0, ent).go(op::E_POS).out_v(0)
    };
    if Io::new()
        .p(0, ent)
        .v(0, origin)
        .v(3, closest)
        .go(op::W_LOS)
        .int(0)
        == 0
    {
        return;
    }
    let dmg = rand_rectf(t.min, t.max);
    let ok = Io::new()
        .p(0, ent)
        .f(0, dmg)
        .f(1, if t.lethal { 0.0 } else { 0.01 })
        .i(0, t.damage_type)
        .i(1, t.instance)
        .go(op::P_DAMAGE)
        .int(0);
    if ok == 0 {
        return;
    }
    let mut dir = sub3(closest, origin);
    dir.y = 0.0;
    if dot3(dir, dir) > 0.0001 {
        let n = normalized(dir);
        let s = std_min(t.force / info.fl(11), t.max_impulse);
        Io::new()
            .v(0, v3(n.x * s, n.y * s, n.z * s))
            .go(op::P_FORCE);
    }
}

pub fn on_set_active(ent: usize, c: *mut Comp) {
    // SAFETY: a registry component.
    let comp = unsafe { &mut *c };
    let active = comp.active;
    match &mut comp.k {
        K::Sm(s) => s.pod.reset(!s.states.is_empty()),
        K::Bark(b) => {
            b.cur = if b.states.is_empty() { -1 } else { 0 };
            b.count = 0.0;
        }
        K::Fe(f) => {
            Io::new()
                .p(0, f.field)
                .i(0, i32::from(ent_active(ent) && active))
                .go(op::FF_SET_ACTIVE);
        }
        K::User(shell) => {
            let on = ent_active(ent);
            Io::new()
                .p(0, *shell)
                .s(0, b"void OnSetActive(bool abX)")
                .i(2, 4)
                .a(0, u64::from(on))
                .go(op::U_SCRIPT_CALL);
        }
        _ => {}
    }
}

pub fn on_destroy(ent: usize, c: *mut Comp) {
    // SAFETY: a registry component.
    if let K::Fe(f) = unsafe { &(*c).k } {
        Io::new().p(0, ent).p(1, f.field).go(op::FF_DESTROY);
    }
}

pub fn new_fe(ent: usize) -> Box<Comp> {
    let field = Io::new().p(0, ent).go(op::FF_NEW).ptr(0);
    Comp::new(
        ent,
        super::model::COMP_FORCE_EMITTER,
        K::Fe(Fe {
            field,
            body: 0,
            offset: v3(0.0, 0.0, 0.0),
            at_foot: false,
            min_speed: 0.0,
            max_speed: 0.0,
        }),
    )
}
