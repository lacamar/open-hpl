//! Agent registry, lifecycle, the per-update driver, event replay and message dispatch.

use super::super::host::Events;
use super::super::pathfinder::EV_TRACK_PLAY_ANIM;
use super::super::statemachine::{
    ACT_ENTER, ACT_LEAVE, ACT_MESSAGE, ACT_SUB_STATE_OVER, ACT_TIMER_UP, ACT_UPDATE,
};
use super::host::H;
use super::io::{Io, op};
use super::model::{
    Agent, COMP_CHAR_MOVER, COMP_PATHFINDER, COMP_STATE_MACHINE, Cm, Comp, K, Pf, Sm,
};
use crate::math::{Vector3f, v3};
use std::cell::UnsafeCell;

pub const MSG_SOUND_HEARD: i32 = 4;
pub const MSG_DOOR: i32 = 18;

pub enum Arg {
    Float(f32),
    QWord(u64),
    DWord(i32),
    Byte(bool),
    EntName,
    Addr(usize),
}

pub const CALL_ENT: i32 = 0;
pub const CALL_ENT_BOOL: i32 = 1;
pub const CALL_RUNTIME_MAP: i32 = 2;
pub const CALL_MAP: i32 = 3;

pub fn call(target: usize, mode: i32, default: bool, decl: &[u8], args: &[Arg]) -> bool {
    let mut io = Io::new()
        .p(0, target)
        .i(0, mode)
        .i(1, i32::from(default))
        .s(0, decl);
    for (k, a) in args.iter().take(3).enumerate() {
        let (kind, bits) = match a {
            Arg::Float(f) => (1, u64::from(f.to_bits())),
            Arg::QWord(q) => (2, *q),
            Arg::DWord(d) => (3, u64::from(*d as u32)),
            Arg::Byte(b) => (4, u64::from(*b)),
            Arg::EntName => (5, 0),
            Arg::Addr(p) => (6, *p as u64),
        };
        io = io.i(2 + k, kind).a(k, bits);
    }
    io.go(op::E_CALL).int(0) != 0
}

pub fn call_ent(ent: usize, decl: &[u8], args: &[Arg]) -> bool {
    call(ent, CALL_ENT, false, decl, args)
}

pub fn call_map(ent: usize, decl: &[u8], args: &[Arg]) -> bool {
    call(ent, CALL_MAP, false, decl, args)
}

pub fn log_err(s: &str) {
    Io::new().s(0, s.as_bytes()).go(op::LOG_ERR);
}

fn dump_on() -> bool {
    use std::sync::atomic::{AtomicI8, Ordering};
    static ON: AtomicI8 = AtomicI8::new(-1);
    let v = ON.load(Ordering::Relaxed);
    if v >= 0 {
        return v == 1;
    }
    let on = Io::new().go(op::DUMP_ON).int(0) != 0;
    ON.store(i8::from(on), Ordering::Relaxed);
    on
}

fn dump_call(ent: usize, decl: &[u8]) {
    if dump_on() {
        Io::new().p(0, ent).i(0, 0).s(0, decl).go(op::DUMP);
    }
}

fn dump_ev(ent: usize, kind: u32, a: i32, b: i32) {
    if dump_on() {
        Io::new()
            .p(0, ent)
            .i(0, 1)
            .i(1, kind as i32)
            .i(2, a)
            .i(3, b)
            .go(op::DUMP);
    }
}

fn dump_msg(ent: usize, id: i32, v: Vector3f, x: i32) {
    if dump_on() {
        Io::new()
            .p(0, ent)
            .i(0, 2)
            .i(1, id)
            .i(2, x)
            .v(0, v)
            .go(op::DUMP);
    }
}

#[allow(clippy::vec_box)] // components and scripts hold agent addresses
struct Reg(UnsafeCell<Vec<Box<Agent>>>);

// SAFETY: the registry is only touched from the game thread (a thread-local below).
thread_local! {
    static AGENTS: Reg = const { Reg(UnsafeCell::new(Vec::new())) };
}

fn with_reg<R>(f: impl FnOnce(&mut Vec<Box<Agent>>) -> R) -> R {
    // SAFETY: single-threaded; `f` never calls back into the registry.
    AGENTS.with(|r| f(unsafe { &mut *r.0.get() }))
}

pub fn find(ent: usize) -> *mut Agent {
    with_reg(|v| {
        v.iter_mut()
            .find(|a| a.ent == ent)
            .map_or(std::ptr::null_mut(), |a| {
                std::ptr::from_mut::<Agent>(&mut **a)
            })
    })
}

pub fn agent_or_new(ent: usize) -> *mut Agent {
    let p = find(ent);
    if !p.is_null() {
        return p;
    }
    with_reg(|v| {
        v.push(Agent::new(ent));
        v.last_mut().map_or(std::ptr::null_mut(), |a| {
            std::ptr::from_mut::<Agent>(&mut **a)
        })
    })
}

pub fn agent_ptrs() -> Vec<*mut Agent> {
    with_reg(|v| {
        v.iter_mut()
            .map(|a| std::ptr::from_mut::<Agent>(&mut **a))
            .collect()
    })
}

pub fn has_agents() -> bool {
    with_reg(|v| !v.is_empty())
}

pub fn add_comp(ent: usize, comp: Box<Comp>) -> *mut Comp {
    let a = agent_or_new(ent);
    // SAFETY: `agent_or_new` returns a live agent.
    let a = unsafe { &mut *a };
    a.comps.push(comp);
    a.comps.last_mut().map_or(std::ptr::null_mut(), |c| {
        std::ptr::from_mut::<Comp>(&mut **c)
    })
}

pub fn alive(ent: usize, c: *const Comp) -> bool {
    let a = find(ent);
    // SAFETY: a registry agent.
    !a.is_null()
        && unsafe { &(*a).comps }
            .iter()
            .any(|x| std::ptr::eq(&raw const **x, c))
}

pub fn comp_of(a: *mut Agent, ty: i32) -> *mut Comp {
    if a.is_null() {
        return std::ptr::null_mut();
    }
    // SAFETY: a registry agent; only the `comps` field is read.
    unsafe { &(*a).comps }
        .iter()
        .find(|c| c.ty == ty)
        .map_or(std::ptr::null_mut(), |c| {
            std::ptr::from_ref::<Comp>(c).cast_mut()
        })
}

pub fn cm_of(a: *mut Agent) -> *mut Cm {
    // SAFETY: a live component of the agent.
    match unsafe { comp_of(a, COMP_CHAR_MOVER).as_mut() }.map(|c| &mut c.k) {
        Some(K::Cm(c)) => std::ptr::from_mut::<Cm>(&mut **c),
        _ => std::ptr::null_mut(),
    }
}

pub fn pf_of(a: *mut Agent) -> *mut Pf {
    // SAFETY: as `cm_of`.
    match unsafe { comp_of(a, COMP_PATHFINDER).as_mut() }.map(|c| &mut c.k) {
        Some(K::Pf(c)) => std::ptr::from_mut::<Pf>(&mut **c),
        _ => std::ptr::null_mut(),
    }
}

pub fn sm_of(a: *mut Agent) -> *mut Sm {
    // SAFETY: as `cm_of`.
    match unsafe { comp_of(a, COMP_STATE_MACHINE).as_mut() }.map(|c| &mut c.k) {
        Some(K::Sm(c)) => std::ptr::from_mut::<Sm>(&mut **c),
        _ => std::ptr::null_mut(),
    }
}

pub fn ent_name(ent: usize) -> Vec<u8> {
    Io::new().p(0, ent).go(op::E_NAME).out_s(0).to_vec()
}

pub fn ent_active(ent: usize) -> bool {
    Io::new().p(0, ent).go(op::E_FLAGS).int(2) != 0
}

pub fn player_feet() -> Vector3f {
    Io::new().go(op::P_INFO).out_v(5)
}

pub fn agent_pos(a: *mut Agent, ent: usize) -> Vector3f {
    // SAFETY: a registry agent or null.
    if let Some(a) = unsafe { a.as_ref() }
        && a.body != 0
    {
        return Io::new().p(1, a.body).i(0, 1).go(op::B_GET3).out_v(0);
    }
    Io::new().p(0, ent).go(op::E_POS).out_v(0)
}

fn sync_mesh(a: *mut Agent) {
    let cm = cm_of(a);
    // SAFETY: a registry agent; the host reads other fields through raw pointers.
    unsafe {
        let roll = cm.as_ref().map_or(0.0, |c| c.st.roll);
        let offset = (*a).mesh_offset;
        (*a).st.sync_mesh(&offset, roll, &mut H(a));
    }
}

pub fn sync_mesh_of(ent: usize) {
    let a = find(ent);
    if !a.is_null() {
        sync_mesh(a);
    }
}

pub fn replay(ent: usize, ev: &Events) {
    for e in &ev.0 {
        let a = find(ent);
        if a.is_null() {
            return;
        }
        dump_ev(ent, e.kind, e.a, e.b);
        let (pf, cm) = (pf_of(a), cm_of(a));
        let name_arg = [Arg::EntName];
        match e.kind {
            super::super::host::EV_MESSAGE => {
                send_message(ent, e.a, v3(e.v[0], e.v[1], e.v[2]), e.b);
            }
            super::super::host::EV_SET_INACTIVE => {
                Io::new().p(0, ent).i(0, 0).go(op::E_SET_ACTIVE);
            }
            super::super::host::EV_AUTO_DISABLE_CALLBACK => {
                // SAFETY: a registry agent.
                let cb = unsafe { (*a).auto_disable_cb.clone() };
                if !cb.is_empty() && Io::new().p(0, ent).go(op::E_FLAGS).int(4) != 0 {
                    let decl = [b"void ".as_slice(), &cb, b"(const tString &in)"].concat();
                    call(ent, CALL_RUNTIME_MAP, false, &decl, &name_arg);
                }
            }
            super::super::host::EV_DOOR_MESSAGE => {
                // SAFETY: a registry agent.
                let dst = unsafe { (*a).msg.ptr() } as usize + 44;
                Io::new().p(0, ent).p(1, dst).i(0, e.a).go(op::E_ID_COPY);
                send_message(ent, MSG_DOOR, v3(e.v[0], e.v[1], e.v[2]), 0);
            }
            super::super::charmover::EV_TURNED_CALLBACK => {
                // SAFETY: a live mover.
                if let Some(cb) = unsafe { cm.as_ref() }.map(|c| c.turned_cb.clone())
                    && !cb.is_empty()
                {
                    let decl = [b"void ".as_slice(), &cb, b"(const tString &in)"].concat();
                    call_map(ent, &decl, &name_arg);
                }
            }
            super::super::pathfinder::EV_END_OF_PATH_CALLBACK => {
                // SAFETY: a live pathfinder.
                if let Some(p) = unsafe { pf.as_mut() }
                    && !p.end_cb.is_empty()
                {
                    let f = std::mem::take(&mut p.end_cb);
                    let decl = [b"void ".as_slice(), &f, b"(const tString &in, bool)"].concat();
                    call_map(ent, &decl, &[Arg::EntName, Arg::Byte(e.a != 0)]);
                }
            }
            super::super::pathfinder::EV_RESULT_CALLBACK => {
                if !pf.is_null() {
                    run_result_callback(ent, pf, e.a != 0);
                }
            }
            super::super::pathfinder::EV_TRACK_CALLBACK => {
                // SAFETY: a live pathfinder.
                if let Some(p) = unsafe { pf.as_ref() } {
                    let cb = p.track_cb.get().to_vec();
                    if !cb.is_empty() {
                        let decl = [b"void ".as_slice(), &cb, b"(const tString &in)"].concat();
                        call_map(ent, &decl, &name_arg);
                    }
                }
            }
            EV_TRACK_PLAY_ANIM => {
                // SAFETY: live components.
                if let (Some(p), false) = (unsafe { pf.as_ref() }, cm.is_null())
                    && let Some(n) = usize::try_from(e.a).ok().and_then(|i| p.track.get(i))
                {
                    let (anim, looped) = (n.anim.get().to_vec(), n.looped);
                    play_animation(ent, cm, &anim, 0.3, looped, b"");
                }
            }
            _ => {}
        }
    }
}

pub fn play_animation(
    ent: usize,
    cm: *mut Cm,
    name: &[u8],
    fade: f32,
    looped: bool,
    cb: &[u8],
) -> i32 {
    let idx = Io::new()
        .p(0, ent)
        .s(0, name)
        .s(1, cb)
        .f(0, fade)
        .i(0, i32::from(looped))
        .go(op::E_PLAY_ANIM)
        .int(0);
    if idx >= 0 {
        // SAFETY: a live mover.
        if let Some(c) = unsafe { cm.as_mut() } {
            c.st.anim_playing = true;
            c.st.use_move_state_anims = false;
        }
    }
    idx
}

pub fn run_result_callback(ent: usize, pf: *mut Pf, ok: bool) {
    // SAFETY: a live pathfinder.
    let Some(p) = (unsafe { pf.as_mut() }) else {
        return;
    };
    if p.result_cb.is_empty() {
        return;
    }
    let f = std::mem::take(&mut p.result_cb);
    let in_map = p.st.callback_in_map;
    if in_map && Io::new().p(0, ent).go(op::E_FLAGS).int(4) != 0 {
        let decl = [b"void ".as_slice(), &f, b"(const tString &in, bool)"].concat();
        call(
            ent,
            CALL_RUNTIME_MAP,
            false,
            &decl,
            &[Arg::EntName, Arg::Byte(ok)],
        );
    } else {
        let decl = [b"void ".as_slice(), &f, b"(bool)"].concat();
        call_ent(ent, &decl, &[Arg::Byte(ok)]);
    }
}

pub fn send_message(ent: usize, msg: i32, v: Vector3f, x: i32) {
    let a = find(ent);
    if a.is_null() {
        return;
    }
    // SAFETY: a registry agent.
    let ptr = unsafe {
        if msg != MSG_DOOR {
            (*a).msg.clear_id();
        }
        dump_msg(ent, msg, v, x);
        (*a).msg.set(v, x);
        (*a).msg.ptr() as usize
    };
    call_ent(
        ent,
        b"void OnRecieveMessage(int, cLuxEntityMessageData@)",
        &[Arg::DWord(msg), Arg::Addr(ptr)],
    );
    let mut i = 0;
    loop {
        let a = find(ent);
        // SAFETY: a registry agent.
        let Some(c) = (unsafe { a.as_ref() }).and_then(|a| a.comps.get(i)) else {
            break;
        };
        let cp = std::ptr::from_ref::<Comp>(c).cast_mut();
        comp_message(ent, a, cp, msg);
        i += 1;
    }
}

fn comp_message(ent: usize, a: *mut Agent, c: *mut Comp, msg: i32) {
    // SAFETY: a registry component.
    let kind = unsafe { &mut (*c).k };
    match kind {
        K::Sm(_) => {
            // SAFETY: the state machine of this component.
            if let K::Sm(s) = unsafe { &mut (*c).k } {
                s.pod.begin_message(msg);
            }
            sm_drive(ent, c);
        }
        K::Pf(_) => {
            let (pf, cm) = (pf_of(a), cm_of(a));
            let mut ev = Events::default();
            // SAFETY: live components; the host reads other fields through raw pointers.
            unsafe {
                if let Some(p) = pf.as_mut() {
                    let mover = cm.as_mut().map(|m| &mut m.st);
                    p.st.on_message(msg, mover, &mut H(a), &mut ev);
                }
            }
            replay(ent, &ev);
        }
        K::User(shell) => {
            let ptr = unsafe { (*a).msg.ptr() } as usize;
            Io::new()
                .p(0, *shell)
                .s(
                    0,
                    b"void OnRecieveMessage(int alMessageId, cLuxEntityMessageData @apData)",
                )
                .i(2, 3)
                .a(0, msg as u32 as u64)
                .i(3, 6)
                .a(1, ptr as u64)
                .go(op::U_SCRIPT_CALL);
        }
        _ => {}
    }
}

fn sm_run(ent: usize, c: *mut Comp, sub: bool, id: i32, func: &str, args: &[Arg]) {
    // SAFETY: a registry state machine.
    let (name, bmsg) = unsafe {
        let K::Sm(s) = &mut (*c).k else { return };
        s.pod.in_sub = sub;
        let m = if sub { &s.subs } else { &s.states };
        (
            m.get(&id).cloned().unwrap_or_default(),
            func.starts_with("Message"),
        )
    };
    let pre = format!(
        "{} {}",
        if bmsg { "bool" } else { "void" },
        if sub { "SubState_" } else { "State_" }
    );
    let run = |st: &[u8]| {
        let decl = [pre.as_bytes(), st, b"_", func.as_bytes()].concat();
        dump_call(ent, &decl);
        if bmsg {
            call(ent, CALL_ENT_BOOL, false, &decl, args)
        } else {
            call_ent(ent, &decl, args)
        }
    };
    if !run(&name) {
        run(b"Default");
    }
    if alive(ent, c) {
        // SAFETY: still registered.
        if let K::Sm(s) = unsafe { &mut (*c).k } {
            s.pod.in_sub = false;
        }
    }
}

pub fn sm_drive(ent: usize, c: *mut Comp) {
    loop {
        if !alive(ent, c) {
            return;
        }
        // SAFETY: a registry state machine; `next` only reads the name maps.
        let act = unsafe {
            let K::Sm(s) = &mut (*c).k else { return };
            let (pod, states, subs) = (&mut s.pod, &s.states, &s.subs);
            pod.next(&|sub, id| (if sub { subs } else { states }).contains_key(&id))
        };
        let Some(a) = act else { return };
        match a.kind {
            ACT_LEAVE => sm_run(ent, c, a.sub, a.state, "Leave()", &[]),
            ACT_ENTER => sm_run(ent, c, a.sub, a.state, "Enter()", &[]),
            ACT_UPDATE => sm_run(
                ent,
                c,
                a.sub,
                a.state,
                "Update(float)",
                &[Arg::Float(a.arg_f)],
            ),
            ACT_TIMER_UP => sm_run(
                ent,
                c,
                a.sub,
                a.state,
                "TimerUp(uint64)",
                &[Arg::QWord(a.arg_u)],
            ),
            ACT_SUB_STATE_OVER => {
                sm_run(
                    ent,
                    c,
                    false,
                    a.state,
                    "SubStateOver(int)",
                    &[Arg::DWord(a.arg_i)],
                );
            }
            ACT_MESSAGE => sm_run(
                ent,
                c,
                a.sub,
                a.state,
                "Message(int)",
                &[Arg::DWord(a.arg_i)],
            ),
            _ => {}
        }
    }
}

pub fn pre_update(ent: usize, dt: f32) {
    let a = find(ent);
    if a.is_null() {
        return;
    }
    let mut ev = Events::default();
    // SAFETY: a registry agent; the host reads other fields through raw pointers.
    unsafe { (*a).st.pre_update(dt, &mut H(a), &mut ev) };
    replay(ent, &ev);
}

pub fn set_senses_active(ent: usize, on: bool) {
    let a = find(ent);
    if a.is_null() {
        return;
    }
    let mut ev = Events::default();
    // SAFETY: a registry agent.
    unsafe { (*a).st.set_senses_active(on, &mut ev) };
    replay(ent, &ev);
}

pub fn update_components(ent: usize, dt: f32) {
    let mut i = 0;
    loop {
        let a = find(ent);
        // SAFETY: a registry agent.
        let Some(c) = (unsafe { a.as_ref() }).and_then(|a| a.comps.get(i)) else {
            return;
        };
        let cp = std::ptr::from_ref::<Comp>(c).cast_mut();
        // SAFETY: a registry component.
        if unsafe { (*cp).active } {
            super::comps::update(ent, a, cp, dt);
        }
        i += 1;
    }
}

pub fn update_agent(ent: usize, dt: f32) {
    if find(ent).is_null() || !ent_active(ent) {
        return;
    }
    pre_update(ent, dt);
    if !ent_active(ent) {
        return;
    }
    update_components(ent, dt);
    let a = find(ent);
    if a.is_null() {
        return;
    }
    let (pf, cm) = (pf_of(a), cm_of(a));
    // SAFETY: live components of a registry agent.
    unsafe {
        if let Some(p) = pf.as_mut() {
            p.st.send_goal(cm.as_mut().map(|m| &mut m.st), &mut H(a));
        }
        (*a).st.step_up(&mut H(a));
        (*a).st.align_with_ground(dt, &mut H(a));
    }
    sync_mesh(a);
}

pub fn set_active(ent: usize, on: bool) {
    let a = find(ent);
    if a.is_null() {
        return;
    }
    // SAFETY: a registry agent.
    let body = unsafe { (*a).body };
    if body != 0 {
        Io::new()
            .p(1, body)
            .i(0, 2)
            .i(1, i32::from(on))
            .go(op::B_SETB);
    }
    let mut i = 0;
    loop {
        let a = find(ent);
        // SAFETY: a registry agent.
        let Some(c) = (unsafe { a.as_ref() }).and_then(|a| a.comps.get(i)) else {
            return;
        };
        let cp = std::ptr::from_ref::<Comp>(c).cast_mut();
        super::comps::on_set_active(ent, cp);
        i += 1;
    }
}

pub fn destroy_agent(ent: usize) {
    let a = find(ent);
    if a.is_null() {
        return;
    }
    let has_map = Io::new().p(0, ent).go(op::E_FLAGS).int(0) != 0;
    if has_map {
        // SAFETY: a registry agent.
        let (comps, body) = unsafe {
            (
                (*a).comps
                    .iter()
                    .map(|c| std::ptr::from_ref::<Comp>(c).cast_mut())
                    .collect::<Vec<_>>(),
                (*a).body,
            )
        };
        for c in comps {
            super::comps::on_destroy(ent, c);
        }
        if body != 0 {
            Io::new().p(0, ent).p(1, body).go(op::B_DESTROY);
        }
    }
    let shells = with_reg(|v| {
        let Some(i) = v.iter().position(|x| x.ent == ent) else {
            return Vec::new();
        };
        let a = v.remove(i);
        a.comps
            .iter()
            .filter_map(|c| if let K::User(s) = c.k { Some(s) } else { None })
            .collect::<Vec<_>>()
    });
    for s in shells {
        Io::new().p(0, s).go(op::U_FREE);
    }
    if !has_agents() {
        Io::new().go(op::ND_CLEAR);
    }
}
