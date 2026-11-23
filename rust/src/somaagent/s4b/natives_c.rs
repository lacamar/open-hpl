//! Script natives on the agent components.

use super::super::charmover::{WallSample, wrap, yaw_to};
use super::super::dist3;
use super::super::misc::{ProxState, SoundListener};
use super::super::statemachine::SmExtra;
use super::comps::bt_setup;
use super::core::{
    add_comp, agent_ptrs, cm_of, comp_of, ent_name, find, log_err, play_animation, replay, sm_of,
};
use super::host::H;
use super::io::{Io, op};
use super::model::{
    Agent, Atk, AtkType, Bark, BarkState, Bt, COMP_ATTACK_DAMAGE, COMP_BACKBONE_TAIL,
    COMP_BARK_MACHINE, COMP_CHAR_MOVER, COMP_EDGE_GLOW, COMP_FOOT_PLACEMENT, COMP_HAND_PLACEMENT,
    COMP_HEAD_TRACKER, COMP_LIGHT_SENSOR, COMP_PATHFINDER, COMP_PROXIMITY_SENSOR,
    COMP_SOUND_LISTENER, COMP_STATE_MACHINE, Cm, Comp, Glow, K, Pf, Prox, Shape, Sm, TrackNode,
    Tstr,
};
use super::natives::{AGENT_TYPES, comp_of_this, text, this, vec};
use crate::math::{Quaternion, Vector3f, rand_rectf, rand_rectl, sub3, to_rad, v3};
use crate::somalux::scriptable::hash64;
use crate::somascript::s2_as::{Engine, Frame, Soma};
use std::cell::{Cell, RefCell};
use std::collections::BTreeMap;
use std::ffi::c_void;

fn comp(f: &Frame) -> &mut Comp {
    comp_of_this(f)
}

fn ent_arg(f: &Frame, i: usize) -> usize {
    f.handle_arg(i) as usize
}

fn done(f: &Frame) {
    f.release_handles();
}

fn ret_comp(f: &Frame, c: *mut Comp) {
    f.ret_ref(c);
}

fn cm<'a>(f: &Frame) -> Option<&'a mut Cm> {
    // SAFETY: `this` is a live component the registry owns.
    match unsafe { &mut (*(f.this.cast::<Comp>())).k } {
        K::Cm(m) => Some(&mut **m),
        _ => None,
    }
}

fn pf<'a>(f: &Frame) -> Option<&'a mut Pf> {
    // SAFETY: as `cm`.
    match unsafe { &mut (*(f.this.cast::<Comp>())).k } {
        K::Pf(m) => Some(&mut **m),
        _ => None,
    }
}

fn sm<'a>(f: &Frame) -> Option<&'a mut Sm> {
    // SAFETY: as `cm`.
    match unsafe { &mut (*(f.this.cast::<Comp>())).k } {
        K::Sm(m) => Some(&mut **m),
        _ => None,
    }
}

fn agent_of(f: &Frame) -> *mut Agent {
    find(comp(f).ent)
}

fn vars_f(vars: usize, name: &[u8], d: f32) -> f32 {
    Io::new()
        .p(0, vars)
        .s(0, name)
        .f(0, d)
        .go(op::V_FLOAT)
        .fl(0)
}

fn vars_s(vars: usize, name: &[u8], d: &[u8]) -> Vec<u8> {
    Io::new()
        .p(0, vars)
        .s(0, name)
        .s(1, d)
        .go(op::V_STR)
        .out_s(0)
        .to_vec()
}

fn set_yaw_near(body: usize, yaw: f32) {
    let cur = Io::new().p(1, body).i(0, 0).go(op::B_GETF).fl(0);
    Io::new()
        .p(1, body)
        .f(0, cur + wrap(yaw - cur))
        .go(op::B_SET_YAW);
}

fn feet_of(body: usize) -> Vector3f {
    Io::new().p(1, body).i(0, 1).go(op::B_GET3).out_v(0)
}

fn setup_idle_extra(ent: usize, m: &mut Cm, name: &[u8], min: f32, max: f32) {
    m.idle_extra = name.to_vec();
    m.st.idle_extra_min = min;
    m.st.idle_extra_max = max;
    m.st.idle_extra_count = 0;
    loop {
        let mut n = name.to_vec();
        n.extend_from_slice((m.st.idle_extra_count + 1).to_string().as_bytes());
        if Io::new().p(0, ent).s(0, &n).go(op::E_ANIM_EXISTS).int(0) == 0 {
            break;
        }
        m.st.idle_extra_count += 1;
    }
    if m.st.idle_extra_count > 0 {
        m.st.idle_extra_wait = rand_rectf(min, max);
    } else {
        let mut e = b"Could not find any animation start with '".to_vec();
        e.extend_from_slice(name);
        e.extend_from_slice(b"' in entity '");
        e.extend_from_slice(&ent_name(ent));
        e.extend_from_slice(b"' to be used for idle extra!\n");
        Io::new().s(0, &e).go(op::LOG_ERR);
    }
}

fn pf_move_to(ent: usize, p: &mut Pf, goal: Vector3f, exact: bool, cb: &[u8], in_map: bool) {
    p.result_cb = cb.to_vec();
    let a = find(ent);
    if a.is_null() {
        return;
    }
    let cmp = cm_of(a);
    // SAFETY: live components; the host reads other fields through raw pointers.
    unsafe {
        p.st.move_to(
            goal,
            exact,
            in_map,
            cmp.as_mut().map(|m| &mut m.st),
            &mut H(a),
        );
    }
}

fn pf_stop(p: &mut Pf, a: *mut Agent) {
    let cmp = cm_of(a);
    // SAFETY: a live mover or null.
    p.st.stop(unsafe { cmp.as_mut() }.map(|m| &mut m.st));
}

fn pf_start_track_node(ent: usize, p: &mut Pf) {
    let a = find(ent);
    if a.is_null() {
        return;
    }
    let cmp = cm_of(a);
    // SAFETY: as `pf_move_to`.
    unsafe {
        p.st.start_track_node(cmp.as_mut().map(|m| &mut m.st), &mut H(a));
    }
}

fn pf_next_track_node(ent: usize, p: &mut Pf) {
    let a = find(ent);
    if a.is_null() {
        return;
    }
    let cmp = cm_of(a);
    let mut ev = super::super::host::Events::default();
    // SAFETY: as `pf_move_to`.
    unsafe {
        p.st.go_to_next_track_node(cmp.as_mut().map(|m| &mut m.st), &mut H(a), &mut ev);
    }
    replay(ent, &ev);
}

fn node_pos(node: usize) -> Vector3f {
    Io::new().p(0, node).go(op::ND_NODE_POS).out_v(0)
}

fn node_at_pos(
    ent: usize,
    pos: Vector3f,
    min: f32,
    max: f32,
    closest: bool,
    los: bool,
    skip: usize,
) -> usize {
    let a = find(ent);
    let nd = H(a).nodes();
    if nd == 0 {
        return 0;
    }
    let count = Io::new().p(0, nd).i(0, -1).go(op::ND_COUNT).int(0);
    let (mut best, mut best_d) = (0usize, 1e30f32);
    let mut cands = Vec::new();
    for i in 0..count {
        let at = Io::new().p(0, nd).i(0, i).go(op::ND_AT);
        let node = at.ptr(0);
        if node == skip {
            continue;
        }
        let np = at.out_v(0);
        let d = dist3(np, pos);
        if d < min || d > max {
            continue;
        }
        if los {
            let off = v3(0.0, 0.5, 0.0);
            let (s, e) = (
                v3(pos.x, pos.y + off.y, pos.z),
                v3(np.x, np.y + off.y, np.z),
            );
            if Io::new()
                .p(0, nd)
                .v(0, s)
                .v(3, e)
                .go(op::ND_FREE_PATH)
                .int(0)
                == 0
            {
                continue;
            }
        }
        cands.push(node);
        if d < best_d {
            best_d = d;
            best = node;
        }
    }
    if closest || cands.is_empty() {
        return best;
    }
    cands
        .get(rand_rectl(0, cands.len() as i32 - 1) as usize)
        .copied()
        .unwrap_or(0)
}

fn node_by_name(ent: usize, name: &[u8]) -> usize {
    let nd = H(find(ent)).nodes();
    if nd == 0 {
        return 0;
    }
    Io::new().p(0, nd).s(0, name).go(op::ND_BY_NAME).ptr(0)
}

pub fn register_creators(s: &Soma) {
    s.func(
        "cLuxStateMachine@ cLux_CreateEntityComponent_StateMachine(iLuxEntity @apEntity)",
        |f| {
            let p = ent_arg(f, 0);
            let k = K::Sm(Sm::new());
            ret_comp(f, add_comp(p, Comp::new(p, COMP_STATE_MACHINE, k)));
            done(f);
        },
    );
    s.func(
        "cLuxCharMover@ cLux_CreateEntityComponent_CharMover(iLuxEntity @apEntity, iCharacterBody @apCharBody)",
        |f| {
            let p = ent_arg(f, 0);
            let k = K::Cm(Cm::new(f.handle_arg(1) as usize));
            ret_comp(f, add_comp(p, Comp::new(p, COMP_CHAR_MOVER, k)));
            done(f);
        },
    );
    s.func(
        "cLuxPathfinder@ cLux_CreateEntityComponent_Pathfinder(iLuxEntity @apEntity)",
        |f| {
            let p = ent_arg(f, 0);
            let k = K::Pf(Pf::new());
            ret_comp(f, add_comp(p, Comp::new(p, COMP_PATHFINDER, k)));
            done(f);
        },
    );
    s.func(
        "cLuxBarkMachine@ cLux_CreateEntityComponent_BarkMachine(iLuxEntity @apEntity)",
        |f| {
            let p = ent_arg(f, 0);
            let k = K::Bark(Bark {
                states: BTreeMap::new(),
                edit: -1,
                cur: -1,
                count: 0.0,
            });
            ret_comp(f, add_comp(p, Comp::new(p, COMP_BARK_MACHINE, k)));
            done(f);
        },
    );
    s.func(
        "cLuxSoundListener@ cLux_CreateEntityComponent_SoundListener(iLuxEntity @apEntity)",
        |f| {
            let p = ent_arg(f, 0);
            let k = K::Sl(SoundListener::default());
            ret_comp(f, add_comp(p, Comp::new(p, COMP_SOUND_LISTENER, k)));
            done(f);
        },
    );
    s.func(
        "cLuxHeadTracker@ cLux_CreateEntityComponent_HeadTracker(iLuxEntity @apEntity)",
        |f| {
            let p = ent_arg(f, 0);
            let k = K::Ht {
                track: 0,
                max_angle: to_rad(70.0),
            };
            ret_comp(f, add_comp(p, Comp::new(p, COMP_HEAD_TRACKER, k)));
            done(f);
        },
    );
}

struct CompIter {
    comps: Vec<*mut Comp>,
    pos: usize,
}

thread_local! {
    #[allow(clippy::vec_box)] // the engine keeps iterator addresses
    static ITERS: RefCell<Vec<Box<CompIter>>> = const { RefCell::new(Vec::new()) };
    static ITER_NEXT: Cell<usize> = const { Cell::new(0) };
    static ATK_INSTANCES: Cell<i32> = const { Cell::new(0) };
}

fn user_by_shell(shell: usize) -> *mut Comp {
    for a in agent_ptrs() {
        // SAFETY: registry agents.
        for c in unsafe { &(*a).comps } {
            if matches!(c.k, K::User(s) if s == shell) {
                return std::ptr::from_ref::<Comp>(c).cast_mut();
            }
        }
    }
    std::ptr::null_mut()
}

fn user_script(c: *mut Comp) -> *mut c_void {
    // SAFETY: a registry component or null.
    match unsafe { c.as_ref() }.map(|c| &c.k) {
        Some(K::User(s)) => super::natives::script_of(*s),
        _ => std::ptr::null_mut(),
    }
}

fn prox_get(c: &mut Comp, id: i32) -> Option<&mut ProxState> {
    let name = ent_name(c.ent);
    let K::Prox(p) = &mut c.k else { return None };
    if let Some(s) = usize::try_from(id)
        .ok()
        .and_then(|i| p.states.get_mut(i))
        .filter(|s| s.exists)
    {
        return Some(s);
    }
    let mut m = format!("BarkMachine state {id} called by entity ").into_bytes();
    m.extend_from_slice(&name);
    m.extend_from_slice(b" does not exist.\n");
    Io::new().s(0, &m).go(op::LOG_ERR);
    None
}

fn atk_get(c: &mut Comp, id: i32) -> Option<&mut AtkType> {
    let name = ent_name(c.ent);
    let K::Atk(a) = &mut c.k else { return None };
    if let Some(t) = usize::try_from(id)
        .ok()
        .and_then(|i| a.types.get_mut(i))
        .filter(|t| t.exists)
    {
        return Some(t);
    }
    let mut m = format!("AttackDamage type {id} called by entity ").into_bytes();
    m.extend_from_slice(&name);
    m.extend_from_slice(b" does not exist.\n");
    Io::new().s(0, &m).go(op::LOG_ERR);
    None
}

fn atk_cur(c: &mut Comp) -> Option<&mut AtkType> {
    let K::Atk(a) = &mut c.k else { return None };
    let i = usize::try_from(a.cur).ok()?;
    a.types.get_mut(i)
}

fn component_pair(s: &Soma, ty: &str) {
    s.method(ty, "iLuxEntity@ GetEntity()", |f| {
        f.ret_ref(comp(f).ent as *mut c_void);
    });
    s.method(ty, "eLuxEntityComponentType GetType()", |f| {
        f.ret(comp(f).ty)
    });
}

pub fn register_components(s: &Soma, e: Engine) {
    s.func(
        "cLuxEdgeGlow@ cLux_CreateEntityComponent_EdgeGlow(iLuxEntity @apEntity)",
        |f| {
            let p = ent_arg(f, 0);
            Io::new().go(op::G_REGISTER);
            let k = K::Glow(Glow {
                color: [0.0, 0.0, 1.0, 1.0],
                thickness: 0.0,
                alpha: 1.0,
                light_limit: 1.0,
            });
            ret_comp(f, add_comp(p, Comp::new(p, COMP_EDGE_GLOW, k)));
            done(f);
        },
    );
    s.func(
        "cLuxForceEmitter@ cLux_CreateEntityComponent_ForceEmitter(iLuxEntity @apEntity)",
        |f| {
            let p = ent_arg(f, 0);
            ret_comp(f, add_comp(p, super::comps::new_fe(p)));
            done(f);
        },
    );
    if e.type_by_name(b"cLuxProximitySensor").is_some() {
        let t = "cLuxProximitySensor";
        s.func(
            "cLuxProximitySensor@ cLux_CreateEntityComponent_ProximitySensor(iLuxEntity@ apEntity)",
            |f| {
                let p = ent_arg(f, 0);
                if p == 0 {
                    f.ret_ref(std::ptr::null_mut::<c_void>());
                } else {
                    let k = K::Prox(Prox {
                        states: Vec::new(),
                        timer: 0.0,
                    });
                    ret_comp(f, add_comp(p, Comp::new(p, COMP_PROXIMITY_SENSOR, k)));
                }
                done(f);
            },
        );
        s.method(
            t,
            "void AddState(int alId, float afEnterDistance, float afLeaveDistance, bool abLineOfSightCheck, bool abPlayerDetectedCheck)",
            |f| {
                let id = f.arg::<i32>(0);
                if id < 0 {
                    return;
                }
                if let K::Prox(p) = &mut comp(f).k {
                    let id = id as usize;
                    if p.states.len() <= id {
                        p.states.resize(id + 1, ProxState::default());
                    }
                    if let Some(s) = p.states.get_mut(id) {
                        *s = ProxState {
                            exists: true,
                            active: true,
                            inside: false,
                            los: f.arg::<bool>(3),
                            detected_check: f.arg::<bool>(4),
                            enter: f.arg::<f32>(1),
                            leave: f.arg::<f32>(2),
                        };
                    }
                }
            },
        );
        s.method(t, "bool GetPlayerInsideState(int alId)", |f| {
            f.ret(prox_get(comp(f), f.arg::<i32>(0)).is_some_and(|s| s.inside));
        });
        s.method(t, "void SetStateActive(int alId, bool abX)", |f| {
            if let Some(s) = prox_get(comp(f), f.arg::<i32>(0)) {
                s.active = f.arg::<bool>(1);
            }
        });
        s.method(t, "bool GetStateActive(int alId)", |f| {
            f.ret(prox_get(comp(f), f.arg::<i32>(0)).is_some_and(|s| s.active));
        });
        s.method(t, "void ResetState(int alId)", |f| {
            if let Some(s) = prox_get(comp(f), f.arg::<i32>(0)) {
                s.inside = false;
            }
        });
        s.method(t, "void ResetAllStates()", |f| {
            if let K::Prox(p) = &mut comp(f).k {
                for s in &mut p.states {
                    s.inside = false;
                }
            }
        });
        component_pair_short(s, t);
    }
    if e.type_by_name(b"cLuxAttackDamage").is_some() {
        register_attack(s);
    }
    s.func(
        "cLuxLightSensor@ cLux_CreateEntityComponent_LightSensor(iLuxEntity @apEntity)",
        |f| {
            let p = ent_arg(f, 0);
            let k = K::Ls { sensitivity: 1.0 };
            ret_comp(f, add_comp(p, Comp::new(p, COMP_LIGHT_SENSOR, k)));
            done(f);
        },
    );
    s.method(
        "cLuxLightSensor",
        "void LoadFromInstanceVariables(cResourceVarsObject@ apInstanceVars)",
        done,
    );
    s.method(
        "cLuxLightSensor",
        "void SetSensitivityLevel(float afX)",
        |f| {
            if let K::Ls { sensitivity } = &mut comp(f).k {
                *sensitivity = f.arg::<f32>(0);
            }
        },
    );
    s.method("cLuxLightSensor", "float GetSensitivityLevel()", |f| {
        f.ret(match comp(f).k {
            K::Ls { sensitivity } => sensitivity,
            _ => 0.0,
        });
    });
    s.method("cLuxLightSensor", "bool IsSensoring()", |f| {
        let c = comp(f);
        let a = find(c.ent);
        let active = super::pod::active(c.ent);
        // SAFETY: a registry agent or null.
        f.ret(active && unsafe { a.as_ref() }.is_none_or(|a| a.st.senses_active));
    });
    s.method(
        "cLuxMap",
        "cLuxEntityComponentIterator@ GetEntityComponentIterator(eLuxEntityComponentType aType)",
        |f| {
            let (m, t) = (this(f), f.arg::<i32>(0));
            let mut comps = Vec::new();
            for a in agent_ptrs() {
                // SAFETY: registry agents.
                let ent = unsafe { (*a).ent };
                if Io::new().p(0, ent).p(1, m).go(op::E_SAME_MAP).int(0) != 0 {
                    for c in unsafe { &(*a).comps } {
                        if c.ty == t {
                            comps.push(std::ptr::from_ref::<Comp>(c).cast_mut());
                        }
                    }
                }
            }
            let ptr = ITERS.with(|r| {
                let mut r = r.borrow_mut();
                if r.is_empty() {
                    r.extend((0..16).map(|_| {
                        Box::new(CompIter {
                            comps: Vec::new(),
                            pos: 0,
                        })
                    }));
                }
                let k = ITER_NEXT.with(|n| {
                    let v = n.get();
                    n.set(v + 1);
                    v.checked_rem(16).unwrap_or(0)
                });
                r.get_mut(k).map_or(std::ptr::null_mut(), |it| {
                    it.comps = comps;
                    it.pos = 0;
                    std::ptr::from_mut::<CompIter>(&mut **it)
                })
            });
            f.ret_ref(ptr);
        },
    );
    s.method("cLuxEntityComponentIterator", "bool HasNext()", |f| {
        let it = f.me::<CompIter>();
        f.ret(it.pos < it.comps.len());
    });
    s.method(
        "cLuxEntityComponentIterator",
        "iLuxEntityComponent@ Next()",
        |f| {
            let it = f.me::<CompIter>();
            let r = it
                .comps
                .get(it.pos)
                .copied()
                .unwrap_or(std::ptr::null_mut());
            if it.pos < it.comps.len() {
                it.pos += 1;
            }
            f.ret_ref(r);
        },
    );
    s.method(
        "cLuxEntityComponentIterator",
        "iLuxEntityComponent@ PeekNext()",
        |f| {
            let it = f.me::<CompIter>();
            f.ret_ref(
                it.comps
                    .get(it.pos)
                    .copied()
                    .unwrap_or(std::ptr::null_mut()),
            );
        },
    );
    if e.type_by_name(b"cLuxFootPlacement").is_some() {
        s.func(
            "cLuxFootPlacement@ cLux_CreateEntityComponent_FootPlacement(iLuxEntity @apEntity)",
            |f| {
                let p = ent_arg(f, 0);
                ret_comp(
                    f,
                    add_comp(p, Comp::new(p, COMP_FOOT_PLACEMENT, K::Generic)),
                );
                done(f);
            },
        );
        s.func(
            "cLuxHandPlacement@ cLux_CreateEntityComponent_HandPlacement(iLuxEntity @apEntity)",
            |f| {
                let p = ent_arg(f, 0);
                ret_comp(
                    f,
                    add_comp(p, Comp::new(p, COMP_HAND_PLACEMENT, K::Generic)),
                );
                done(f);
            },
        );
        s.method(
            "cLuxFootPlacement",
            "void SetFootActive(bool abX, bool abRight, float afInactiveLength = -1.f, bool abFront = false)",
            |f| {
                let k = usize::from(f.arg::<bool>(1)) + 2 * usize::from(f.arg::<bool>(3));
                if let Some(x) = comp(f).foot.get_mut(k) {
                    *x = f.arg::<bool>(0);
                }
            },
        );
        s.method(
            "cLuxFootPlacement",
            "bool GetFootActive(bool abRight, bool abFront = false)",
            |f| {
                let k = usize::from(f.arg::<bool>(0)) + 2 * usize::from(f.arg::<bool>(1));
                f.ret(comp(f).foot.get(k).copied().unwrap_or(false));
            },
        );
        for ty in ["cLuxFootPlacement", "cLuxHandPlacement"] {
            component_pair(s, ty);
            s.method(ty, "void SetActive(bool abX)", |f| {
                comp(f).active = f.arg::<bool>(0)
            });
            s.method(ty, "bool IsActive()", |f| f.ret(comp(f).active));
            s.method(ty, "void FadeIn(float afTime)", |f| comp(f).active = true);
            s.method(ty, "void FadeOut(float afTime)", |f| comp(f).active = false);
        }
        for ty in AGENT_TYPES {
            s.method(
                ty,
                "iLuxEntityComponent@ GetComponentByType(eLuxEntityComponentType aType) const",
                |f| f.ret_ref(comp_of(find(this(f)), f.arg::<i32>(0))),
            );
        }
    }
    s.func(
        "cLuxBackboneTail@ cLux_CreateEntityComponent_BackboneTail(iLuxEntity @apEntity)",
        |f| {
            let p = ent_arg(f, 0);
            let k = K::Bt(Box::new(Bt {
                bones: Vec::new(),
                trail: vec![(Quaternion::default(), 0.0)],
                count: 0,
                head: 0,
                length: 0.0,
                timer: 0.0,
                interval: 0.1,
                stiffness: 1.0,
                last_pos: v3(0.0, 0.0, 0.0),
                last: Quaternion::default(),
            }));
            ret_comp(f, add_comp(p, Comp::new(p, COMP_BACKBONE_TAIL, k)));
            done(f);
        },
    );
    for ty in [
        "iLuxEntityComponent",
        "cLuxStateMachine",
        "cLuxCharMover",
        "cLuxPathfinder",
        "cLuxBarkMachine",
        "cLuxSoundListener",
        "cLuxHeadTracker",
        "cLuxEdgeGlow",
        "cLuxForceEmitter",
        "cLuxLightSensor",
        "cLuxBackboneTail",
    ] {
        component_pair(s, ty);
    }
    if e.type_by_name(b"cLuxUserComponent").is_some() {
        register_user(s, e);
    }
    s.method(
        "cLuxMap",
        "iLuxEntityComponent@ GetEntityComponent(eLuxEntityComponentType aType, const tString&in asName)",
        |f| {
            let ent = Io::new()
                .p(1, this(f))
                .s(0, text(f, 1))
                .go(op::E_MAP_FIND)
                .ptr(0);
            let a = if ent == 0 { std::ptr::null_mut() } else { find(ent) };
            f.ret_ref(comp_of(a, f.arg::<i32>(0)));
        },
    );
    register_glow(s);
    register_force_emitter(s);
    register_backbone(s);
    register_state_machine(s);
    register_char_mover(s);
    register_pathfinder(s);
    register_misc(s);
}

fn component_pair_short(s: &Soma, ty: &str) {
    s.method(ty, "iLuxEntity@ GetEntity()", |f| {
        f.ret_ref(comp(f).ent as *mut c_void);
    });
    s.method(ty, "int GetType()", |f| f.ret(comp(f).ty));
}

fn register_attack(s: &Soma) {
    let t = "cLuxAttackDamage";
    s.func(
        "cLuxAttackDamage@ cLux_CreateEntityComponent_AttackDamage(iLuxEntity @apEntity)",
        |f| {
            let p = ent_arg(f, 0);
            if p == 0 {
                f.ret_ref(std::ptr::null_mut::<c_void>());
            } else {
                let k = K::Atk(Atk {
                    types: Vec::new(),
                    cur: -1,
                });
                ret_comp(f, add_comp(p, Comp::new(p, COMP_ATTACK_DAMAGE, k)));
            }
            done(f);
        },
    );
    s.method(t, "void AddType(int alId, const tString&in asName)", |f| {
        let id = f.arg::<i32>(0);
        if id < 0 {
            return;
        }
        if let K::Atk(a) = &mut comp(f).k {
            let i = id as usize;
            if a.types.len() <= i {
                a.types.resize(i + 1, AtkType::default());
            }
            if let Some(t) = a.types.get_mut(i) {
                *t = AtkType {
                    exists: true,
                    ..AtkType::default()
                };
            }
            a.cur = id;
        }
    });
    s.method(
        t,
        "void SetType_Damage(float afMinDamage, float afMaxDamage=-1, bool abLethal=true)",
        |f| {
            let (mn, mx, l) = (f.arg::<f32>(0), f.arg::<f32>(1), f.arg::<bool>(2));
            if let Some(t) = atk_cur(comp(f)) {
                t.min = mn;
                t.max = if mx <= 0.0 { mn } else { mx };
                t.lethal = l;
            }
        },
    );
    s.method(t, "void SetType_HitSpeed(float afX)", |_| {});
    s.method(t, "void SetType_DamageType(int alX)", |f| {
        let x = f.arg::<i32>(0);
        if let Some(t) = atk_cur(comp(f)) {
            t.damage_type = x;
        }
    });
    s.method(t, "void SetType_Strength(int alX)", |_| {});
    s.method(
        t,
        "void SetType_Force(float afForce, float afMaxImpulse)",
        |f| {
            let x = f.arg::<f32>(0);
            if let Some(t) = atk_cur(comp(f)) {
                t.force = x;
                t.max_impulse = 10.0;
            }
        },
    );
    s.method(
        t,
        "void SetType_Affects(bool abProps, bool abAgents, bool abPlayer)",
        |f| {
            let x = f.arg::<bool>(2);
            if let Some(t) = atk_cur(comp(f)) {
                t.player = x;
            }
        },
    );
    s.method(
        t,
        "void SetType_AddShapeFromCenter(const cVector3f&in avOffset, float afRadius)",
        |f| {
            let (v, r) = (vec(f, 0), f.arg::<f32>(1));
            if let Some(t) = atk_cur(comp(f)) {
                t.shapes.push(Shape {
                    socket: -1,
                    offset: v,
                    radius: r,
                });
            }
        },
    );
    s.method(
        t,
        "void SetType_AddShapeAtSocket(const tString&in asSocket, const cVector3f&in avOffset, float afRadius)",
        |f| {
            let (n, v, r) = (text(f, 0).to_vec(), vec(f, 1), f.arg::<f32>(2));
            let c = comp(f);
            let ent = c.ent;
            if atk_cur(c).is_none() {
                return;
            }
            let count = Io::new().p(0, ent).go(op::E_SOCKET_COUNT).int(0);
            if count == 0 {
                let mut m = b"Couldn't add shape to attack damage component for entity '".to_vec();
                m.extend_from_slice(&ent_name(ent));
                m.extend_from_slice(b"' because mesh or sockets are missing.\n");
                log_err_b(&m);
                return;
            }
            let idx = Io::new().p(0, ent).s(0, &n).go(op::E_SOCKET_FIND).int(0);
            if idx >= 0 {
                if let Some(t) = atk_cur(comp(f)) {
                    t.shapes.push(Shape {
                        socket: idx,
                        offset: v,
                        radius: r,
                    });
                }
                return;
            }
            let mut m = b"Shape attack damage component for entity '".to_vec();
            m.extend_from_slice(&ent_name(ent));
            m.extend_from_slice(b"' failed because socket '");
            m.extend_from_slice(&n);
            m.extend_from_slice(b"' doesn't exist\n");
            log_err_b(&m);
        },
    );
    s.method(
        t,
        "void Start(int alId, float afDuration, float afCheckFreq=8.0f)",
        |f| {
            let (id, d, fr) = (f.arg::<i32>(0), f.arg::<f32>(1), f.arg::<f32>(2));
            if let Some(t) = atk_get(comp(f), id) {
                t.active = true;
                t.duration = d;
                t.freq = fr;
                t.timer = 0.0;
                t.instance = ATK_INSTANCES.with(|c| {
                    let v = c.get();
                    c.set(v + 1);
                    v
                });
            }
        },
    );
    s.method(t, "void Stop(int alId)", |f| {
        if let Some(t) = atk_get(comp(f), f.arg::<i32>(0)) {
            t.active = false;
        }
    });
    s.method(t, "void StopAll()", |f| {
        if let K::Atk(a) = &mut comp(f).k {
            for t in &mut a.types {
                t.active = false;
            }
        }
    });
    s.method(t, "int GetTypeInstanceID(int alId)", |f| {
        let r = atk_get(comp(f), f.arg::<i32>(0))
            .map_or(-1, |t| if t.active { t.instance } else { -1 });
        f.ret(r);
    });
    s.method(
        t,
        "void SetDamage(int alId, float afMinDamage, float afMaxDamage, bool abLethal)",
        |f| {
            let (id, mn, mx, l) = (
                f.arg::<i32>(0),
                f.arg::<f32>(1),
                f.arg::<f32>(2),
                f.arg::<bool>(3),
            );
            if let Some(t) = atk_get(comp(f), id) {
                t.min = mn;
                t.max = if mx <= 0.0 { mn } else { mx };
                t.lethal = l;
            }
        },
    );
    component_pair_short(s, t);
}

fn log_err_b(m: &[u8]) {
    Io::new().s(0, m).go(op::LOG_ERR);
}

fn register_user(s: &Soma, e: Engine) {
    s.func(
        "iScrUserComponent_Interface@ cLux_CreateUserComponent(iLuxEntity@ apEntity, const tString&in asComponentName, const tString&in asScriptFile, const tString&in asScriptClass, int alID)",
        |f| {
            let p = ent_arg(f, 0);
            if p == 0 {
                f.ret_ref(std::ptr::null_mut::<c_void>());
                done(f);
                return;
            }
            let (name, file, class, id) = (text(f, 1), text(f, 2), text(f, 3), f.arg::<i32>(4));
            let shell = Io::new()
                .p(0, p)
                .s(0, file)
                .s(1, class)
                .i(0, id)
                .go(op::U_NEW)
                .ptr(0);
            if shell == 0 {
                let mut m = b"Could not load user component script for '".to_vec();
                m.extend_from_slice(name);
                m.extend_from_slice(b"' '");
                m.extend_from_slice(file);
                m.extend_from_slice(b"' of class '");
                m.extend_from_slice(class);
                m.extend_from_slice(b"'\n");
                log_err(&String::from_utf8_lossy(&m));
                f.ret_ref(std::ptr::null_mut::<c_void>());
                done(f);
                return;
            }
            let mut c = Comp::new(p, id, K::User(shell));
            c.name.set(name);
            let cp = add_comp(p, c);
            f.ret_ref(user_script(cp));
            done(f);
        },
    );
    s.func(
        "iScrUserComponent_Interface@ cLux_GetUserComponent(iLuxEntity@ apEntity, int alID)",
        |f| {
            let p = ent_arg(f, 0);
            let a = if p == 0 {
                std::ptr::null_mut()
            } else {
                find(p)
            };
            f.ret_ref(user_script(comp_of(a, f.arg::<i32>(1))));
            done(f);
        },
    );
    s.func(
        "iScrUserComponent_Interface@ cLux_GetUserComponentScriptInterface(iLuxEntityComponent@ apComponent)",
        |f| {
            let c = f.handle_arg(0).cast::<Comp>();
            f.ret_ref(user_script(c));
            done(f);
        },
    );
    s.method("cLuxUserComponent", "iLuxEntity@ GetEntity()", |f| {
        // SAFETY: a registry component or null.
        let c = user_by_shell(this(f));
        f.ret_ref(unsafe { c.as_ref() }.map_or(0, |c| c.ent) as *mut c_void);
    });
    s.method("cLuxUserComponent", "int GetType()", |f| {
        let c = user_by_shell(this(f));
        // SAFETY: as above.
        f.ret(unsafe { c.as_ref() }.map_or(0, |c| c.ty));
    });
    s.method(
        "cLuxUserComponent",
        "const tString& GetComponentName()",
        |f| {
            let c = user_by_shell(this(f));
            // SAFETY: as above.
            f.ret_ref(unsafe { c.as_ref() }.map_or(std::ptr::null_mut(), |c| c.name.mirror()));
        },
    );
    Io::new().p(0, e.0 as usize).go(op::U_TIMERS);
}

fn register_glow(s: &Soma) {
    let t = "cLuxEdgeGlow";
    s.method(t, "void SetColor(const cColor&in aColor)", |f| {
        let c = *f.rf::<[f32; 4]>(0);
        if let K::Glow(g) = &mut comp(f).k {
            g.color = c;
        }
    });
    s.method(t, "cColor GetColor()", |f| {
        if let K::Glow(g) = &comp(f).k {
            f.ret(g.color);
        }
    });
    macro_rules! glow_f {
        ($set:literal, $get:literal, $field:ident) => {
            s.method(t, $set, |f| {
                let x = f.arg::<f32>(0);
                if let K::Glow(g) = &mut comp(f).k {
                    g.$field = x;
                }
            });
            s.method(t, $get, |f| {
                if let K::Glow(g) = &comp(f).k {
                    f.ret(g.$field);
                }
            });
        };
    }
    glow_f!("void SetAlpha(float afX)", "float GetAlpha()", alpha);
    glow_f!(
        "void SetEdgeThickness(float afX)",
        "float GetEdgeThickness()",
        thickness
    );
    glow_f!(
        "void SetLightLimit(float afX)",
        "float GetLightLimit()",
        light_limit
    );
    s.method(t, "void SetActive(bool abX)", |f| {
        comp(f).active = f.arg::<bool>(0)
    });
    s.method(t, "bool IsActive()", |f| f.ret(comp(f).active));
}

fn fe_field(f: &Frame, which: i32, v: f32) {
    if let K::Fe(x) = &comp(f).k {
        Io::new().p(0, x.field).i(0, which).f(0, v).go(op::FF_SET);
    }
}

fn fe_fade(f: &Frame, which: i32, v: f32) {
    if let K::Fe(x) = &comp(f).k {
        Io::new().p(0, x.field).i(0, which).f(0, v).go(op::FF_FADE);
    }
}

fn register_force_emitter(s: &Soma) {
    let t = "cLuxForceEmitter";
    s.method(t, "void SetActive(bool abX)", |f| {
        let c = comp(f);
        c.active = f.arg::<bool>(0);
        super::comps::on_set_active(c.ent, c);
    });
    s.method(t, "bool IsActive()", |f| f.ret(comp(f).active));
    s.method(
        t,
        "void SetCharacterBody(iCharacterBody @apCharBody, const cVector3f &in avOffset, bool abAtFoot)",
        |f| {
            let (b, o, a) = (f.handle_arg(0) as usize, vec(f, 1), f.arg::<bool>(2));
            if let K::Fe(x) = &mut comp(f).k {
                x.body = b;
                x.offset = o;
                x.at_foot = a;
            }
            done(f);
        },
    );
    s.method(t, "void SetRadius(float afX)", |f| {
        fe_field(f, 0, f.arg::<f32>(0))
    });
    s.method(t, "void SetForce(float afX)", |f| {
        fe_field(f, 1, f.arg::<f32>(0))
    });
    s.method(t, "void SetFreq(float afX)", |f| {
        fe_field(f, 2, f.arg::<f32>(0))
    });
    s.method(t, "void FadeIn(float afTime)", |f| {
        fe_fade(f, 0, f.arg::<f32>(0))
    });
    s.method(t, "void FadeOut(float afTime)", |f| {
        fe_fade(f, 1, f.arg::<f32>(0))
    });
    s.method(t, "void SetMinForceSpeed(float afX)", |f| {
        if let K::Fe(x) = &mut comp(f).k {
            x.min_speed = f.arg::<f32>(0);
        }
    });
    s.method(t, "void SetMaxForceSpeed(float afX)", |f| {
        if let K::Fe(x) = &mut comp(f).k {
            x.max_speed = f.arg::<f32>(0);
        }
    });
}

fn bt_names(arr: usize) -> Vec<Vec<u8>> {
    let n = Io::new().p(0, arr).i(0, -1).go(op::ARR_LEN).int(0);
    (0..n)
        .map(|i| Io::new().p(0, arr).i(0, i).go(op::ARR_AT).out_s(0).to_vec())
        .collect()
}

fn bt_set_max_trail(b: &mut Bt, n: i32) {
    if n <= 0 {
        return;
    }
    b.trail.resize(n as usize, (Quaternion::default(), 0.0));
    b.count = 0;
    b.head = 0;
}

fn register_backbone(s: &Soma) {
    let t = "cLuxBackboneTail";
    s.method(t, "void Setup(array<tString> &in avBoneNames)", |f| {
        let names = bt_names(f.ptr_arg(0) as usize);
        let c = comp(f);
        let ent = c.ent;
        if let K::Bt(b) = &mut c.k {
            bt_setup(ent, b, &names);
        }
    });
    s.method(
        t,
        "void LoadFromVariables(cResourceVarsObject@ apVars)",
        |f| {
            let v = f.handle_arg(0) as usize;
            let c = comp(f);
            let ent = c.ent;
            if let K::Bt(b) = &mut c.k {
                b.interval = 1.0 / (vars_f(v, b"Backbone_AddTrailFreq", 10.0) + 1e-5);
                b.stiffness = vars_f(v, b"Backbone_Stiffness", 1.0);
                let n = Io::new()
                    .p(0, v)
                    .s(0, b"Backbone_MaxTrailSize")
                    .i(0, 13)
                    .go(op::V_INT)
                    .int(0);
                bt_set_max_trail(b, n);
                let names: Vec<Vec<u8>> =
                    crate::string::split(&vars_s(v, b"Backbone_Bones", b""), None, true)
                        .map(<[u8]>::to_vec)
                        .collect();
                bt_setup(ent, b, &names);
            }
            done(f);
        },
    );
    s.method(t, "void SetStiffness(float afX)", |f| {
        if let K::Bt(b) = &mut comp(f).k {
            b.stiffness = f.arg::<f32>(0);
        }
    });
    s.method(t, "void SetMaxTrailSize(int alX)", |f| {
        if let K::Bt(b) = &mut comp(f).k {
            bt_set_max_trail(b, f.arg::<i32>(0));
        }
    });
    s.method(t, "void SetAddTrailFreq(float afX)", |f| {
        if let K::Bt(b) = &mut comp(f).k {
            b.interval = 1.0 / f.arg::<f32>(0);
        }
    });
}

fn sm_error(m: String) {
    Io::new().s(0, m.as_bytes()).go(op::LOG_ERR);
}

fn sm_change_state(ent: usize, s: &mut Sm, st: i32) {
    if s.pod.in_sub {
        let sub = s.subs.get(&s.pod.sub_cur).cloned().unwrap_or_default();
        sm_error(format!(
            "Entity '{}' cannot change to state {} inside a sub state ('{}').\n",
            String::from_utf8_lossy(&ent_name(ent)),
            st,
            String::from_utf8_lossy(&sub)
        ));
    } else if !s.states.contains_key(&st) {
        sm_error(format!(
            "State {} does not exist in Entity '{}'\n",
            st,
            String::from_utf8_lossy(&ent_name(ent))
        ));
    } else {
        s.pod.next = st;
    }
}

pub fn change_state(ent: usize, st: i32) {
    let a = find(ent);
    let p = sm_of(a);
    // SAFETY: a live state machine or null.
    if let (Some(s), true) = (unsafe { p.as_mut() }, st >= 0) {
        sm_change_state(ent, s, st);
    }
}

fn register_state_machine(s: &Soma) {
    let t = "cLuxStateMachine";
    s.method(t, "void AddState(const tString&in asName, int alId)", |f| {
        let (n, id) = (text(f, 0).to_vec(), f.arg::<i32>(1));
        if let Some(m) = sm(f) {
            m.states.insert(id, n);
            if m.states.len() == 1 {
                m.pod.next = 0;
            }
        }
    });
    s.method(
        t,
        "void AddSubState(const tString&in asName, int alId)",
        |f| {
            let (n, id) = (text(f, 0).to_vec(), f.arg::<i32>(1));
            if let Some(m) = sm(f) {
                m.subs.insert(id, n);
            }
        },
    );
    s.method(t, "void ChangeState(int alState)", |f| {
        let ent = comp(f).ent;
        if let Some(m) = sm(f) {
            sm_change_state(ent, m, f.arg::<i32>(0));
        }
    });
    s.method(t, "void ChangeSubState(int alState)", |f| {
        let (ent, st) = (comp(f).ent, f.arg::<i32>(0));
        if let Some(m) = sm(f) {
            if st > 0 && !m.subs.contains_key(&st) {
                sm_error(format!(
                    "State {} does not exist in Entity '{}'\n",
                    st,
                    String::from_utf8_lossy(&ent_name(ent))
                ));
            } else {
                m.pod.sub_next = st;
            }
        }
    });
    macro_rules! sm_int {
        ($decl:literal, $field:ident) => {
            s.method(t, $decl, |f| f.ret(sm(f).map_or(0, |m| m.pod.$field)));
        };
    }
    sm_int!("int GetNextState()", next);
    sm_int!("int GetPrevState()", prev);
    sm_int!("int GetCurrentState()", cur);
    s.method(t, "tString GetCurrentStateName()", |f| {
        let n = sm(f)
            .and_then(|m| m.states.get(&m.pod.cur).cloned())
            .unwrap_or_default();
        f.ret_string(&n);
    });
    sm_int!("int GetNextSubState()", sub_next);
    sm_int!("int GetPrevSubState()", sub_prev);
    sm_int!("int GetCurrentSubState()", sub_cur);
    s.method(t, "void AddTimer(uint64 alId, float afTime)", |f| {
        sm_timer(f, f.arg::<u64>(0), |x, sub, id| {
            x.add_timer(sub, id, f.arg::<f32>(1))
        });
    });
    s.method(t, "void StopTimer(uint64 alId)", |f| {
        sm_timer(f, f.arg::<u64>(0), |x, sub, id| x.stop_timer(sub, id));
    });
    s.method(t, "bool TimerExists(uint64 alId)", |f| {
        let mut r = false;
        sm_timer(f, f.arg::<u64>(0), |x, sub, id| r = x.timer_exists(sub, id));
        f.ret(r);
    });
    s.method(
        t,
        "void AddTimer(const tString& in asId, float afTime)",
        |f| {
            sm_timer(f, hash64(text(f, 0)), |x, sub, id| {
                x.add_timer(sub, id, f.arg::<f32>(1))
            });
        },
    );
    s.method(t, "void StopTimer(const tString& in asId)", |f| {
        sm_timer(f, hash64(text(f, 0)), |x, sub, id| x.stop_timer(sub, id));
    });
    s.method(t, "bool TimerExists(const tString& in asId)", |f| {
        let mut r = false;
        sm_timer(f, hash64(text(f, 0)), |x, sub, id| {
            r = x.timer_exists(sub, id)
        });
        f.ret(r);
    });
    s.method(t, "cLuxEntityMessageData@ GetCurrentMessageData()", |f| {
        let a = agent_of(f);
        // SAFETY: a registry agent or null.
        f.ret_ref(unsafe { a.as_ref() }.map_or(std::ptr::null_mut(), |a| a.msg.ptr()));
    });
}

fn sm_timer(f: &Frame, id: u64, g: impl FnOnce(&mut SmExtra, bool, u64)) {
    if let Some(m) = sm(f) {
        let sub = m.pod.in_sub;
        g(&mut m.x, sub, id);
    }
}

macro_rules! cm_f {
    ($s:ident, $t:ident, $decl:literal, $field:ident) => {
        $s.method($t, $decl, |f| {
            if let Some(m) = cm(f) {
                m.st.$field = f.arg::<f32>(0);
            }
        });
    };
}

macro_rules! cm_b {
    ($s:ident, $t:ident, $decl:literal, $field:ident) => {
        $s.method($t, $decl, |f| {
            if let Some(m) = cm(f) {
                m.st.$field = f.arg::<bool>(0);
            }
        });
    };
}

macro_rules! cm_speed {
    ($s:ident, $t:ident, $decl:literal, $field:ident) => {
        $s.method($t, $decl, |f| {
            if let Some(m) = cm(f) {
                if m.edit >= 0 {
                    let e = m.edit;
                    m.speeds.entry(e).or_default().$field = f.arg::<f32>(0);
                }
            }
        });
    };
}

fn register_char_mover(s: &Soma) {
    let t = "cLuxCharMover";
    s.method(t, "iCharacterBody@ GetCharBody()", |f| {
        f.ret_ref(cm(f).map_or(0, |m| m.body) as *mut c_void);
    });
    s.method(
        t,
        "void LoadFromVariables(cResourceVarsObject@ apVars)",
        |f| {
            let v = f.handle_arg(0) as usize;
            let ent = comp(f).ent;
            if let (true, Some(m)) = (v != 0, cm(f)) {
                m.idle = vars_s(v, b"CharMover_IdleAnim", &m.idle);
                m.walk = vars_s(v, b"CharMover_WalkAnim", &m.walk);
                m.run = vars_s(v, b"CharMover_RunAnim", &m.run);
                m.backward = vars_s(v, b"CharMover_BackwardAnim", &m.backward);
                let extra = vars_s(v, b"CharMover_IdleExtraAnim", b"");
                if !extra.is_empty() {
                    let (mn, mx) = (
                        vars_f(v, b"CharMover_IdleExtraMinWait", 0.0),
                        vars_f(v, b"CharMover_IdleExtraMaxWait", 0.0),
                    );
                    setup_idle_extra(ent, m, &extra, mn, mx);
                }
            }
            done(f);
        },
    );
    s.method(
        t,
        "void MoveToPos(const cVector3f&in avFeetPos, bool abSlowDownAndStopAtGoal=false)",
        |f| {
            if let Some(m) = cm(f) {
                m.st.move_to_pos(vec(f, 0), f.arg::<bool>(1));
            }
        },
    );
    s.method(t, "void TurnToPos(const cVector3f&in avFeetPos)", |f| {
        if let Some(m) = cm(f) {
            let mut d = sub3(vec(f, 0), feet_of(m.body));
            d.y = 0.0;
            m.st.turn_to(yaw_to(d));
        }
    });
    s.method(t, "void TurnToAngle(float afAngle)", |f| {
        if let Some(m) = cm(f) {
            m.st.turn_to(f.arg::<f32>(0));
        }
    });
    s.method(t, "void TurnToAngles(float afYaw, float afPitch)", |f| {
        if let Some(m) = cm(f) {
            m.st.turn_to(f.arg::<f32>(0));
        }
    });
    s.method(
        t,
        "void TurnInstantlyToPos(const cVector3f&in avGoalPos)",
        |f| {
            if let Some(m) = cm(f) {
                let mut d = sub3(vec(f, 0), feet_of(m.body));
                d.y = 0.0;
                set_yaw_near(m.body, yaw_to(d));
                m.st.turning = false;
            }
        },
    );
    s.method(t, "void TurnInstantlyToAngle(float afAngle)", |f| {
        if let Some(m) = cm(f) {
            set_yaw_near(m.body, f.arg::<f32>(0));
            m.st.turning = false;
        }
    });
    s.method(
        t,
        "void TurnInstantlyToAngle(float afYaw, float afPitch)",
        |f| {
            if let Some(m) = cm(f) {
                set_yaw_near(m.body, f.arg::<f32>(0));
                m.st.turning = false;
            }
        },
    );
    s.method(t, "void StopTurning()", |f| {
        if let Some(m) = cm(f) {
            m.st.turning = false;
        }
    });
    s.method(
        t,
        "int PlayAnimation(const tString&in asName, float afFadeTime=0.3f, bool abLoop=false, bool abPlayTransition=true, const tString&in asCallback=\"\")",
        |f| {
            let ent = comp(f).ent;
            let r = match cm(f) {
                Some(m) => play_animation(
                    ent,
                    std::ptr::from_mut(m),
                    text(f, 0),
                    f.arg::<f32>(1),
                    f.arg::<bool>(2),
                    text(f, 4),
                ),
                None => 0,
            };
            f.ret(r);
        },
    );
    s.method(t, "void SetUseMoveStateAnimations(bool abX)", |f| {
        if let Some(m) = cm(f) {
            m.st.set_use_move_state_anims(f.arg::<bool>(0));
        }
    });
    s.method(t, "bool GetUseMoveStateAnimations()", |f| {
        f.ret(cm(f).is_some_and(|m| m.st.use_move_state_anims));
    });
    s.method(
        t,
        "void SetTurnedToGoalCallbackFunc(const tString &in asFunc)",
        |f| {
            let n = text(f, 0).to_vec();
            if let Some(m) = cm(f) {
                m.turned_cb = n;
            }
        },
    );
    s.method(t, "float GetMoveSpeed()", |f| {
        let r = cm(f).map_or(0.0, |m| {
            if m.body == 0 {
                0.0
            } else {
                Io::new().p(1, m.body).i(0, 3).go(op::B_GETF).fl(0)
            }
        });
        f.ret(r);
    });
    s.method(t, "float GetWantedSpeedAmount()", |f| {
        f.ret(if cm(f).is_some_and(|m| m.st.moving) {
            1.0f32
        } else {
            0.0
        });
    });
    s.method(t, "float GetStuckCounter()", |f| {
        f.ret(cm(f).map_or(0.0, |m| m.st.stuck))
    });
    s.method(t, "float GetMaxStuckCounter()", |f| f.ret(2.0f32));
    s.method(t, "void ResetStuckCounter()", |f| {
        if let Some(m) = cm(f) {
            m.st.stuck = 0.0;
        }
    });
    cm_f!(s, t, "void SetMaxForwardSpeed(float afX)", max_forward);
    cm_f!(s, t, "void SetMaxBackwardSpeed(float afX)", max_backward);
    cm_f!(
        s,
        t,
        "void SetTurnMinBreakAngle(float afX)",
        turn_min_break_angle
    );
    cm_f!(s, t, "void SetTurnBreakMul(float afX)", turn_break_mul);
    cm_f!(s, t, "void SetTurnSpeedMul(float afX)", turn_speed_mul);
    cm_f!(s, t, "void SetTurnMaxSpeed(float afX)", turn_max_speed);
    cm_f!(
        s,
        t,
        "void SetStoppedToWalkSpeed(float afX)",
        stopped_to_walk
    );
    cm_f!(s, t, "void SetWalkToRunSpeed(float afX)", walk_to_run);
    cm_f!(
        s,
        t,
        "void SetWalkToStoppedSpeed(float afX)",
        walk_to_stopped
    );
    cm_f!(s, t, "void SetRunToWalkSpeed(float afX)", run_to_walk);
    cm_f!(
        s,
        t,
        "void SetMoveSpeedAnimMul(float afX)",
        move_speed_anim_mul
    );
    for (decl, which) in [
        ("void SetIdleAnimName(const tString&in asName)", 0),
        ("void SetWalkAnimName(const tString&in asName)", 1),
        ("void SetRunAnimName(const tString&in asName)", 2),
    ] {
        s.method(t, decl, move |f| {
            let n = text(f, 0).to_vec();
            if let Some(m) = cm(f) {
                match which {
                    0 => m.idle = n,
                    1 => m.walk = n,
                    _ => m.run = n,
                }
                m.st.anim_state = -1;
            }
        });
    }
    s.method(
        t,
        "void SetBackwardAnimName(const tString&in asName)",
        |f| {
            let n = text(f, 0).to_vec();
            if let Some(m) = cm(f) {
                m.backward = n;
            }
        },
    );
    s.method(t, "void AddSpeedState(int alId)", |f| {
        if let Some(m) = cm(f) {
            let id = f.arg::<i32>(0);
            m.speeds.entry(id).or_default();
            m.edit = id;
        }
    });
    s.method(t, "void SetSpeedState(int alId)", |f| {
        if let Some(m) = cm(f) {
            m.st.speed_state = f.arg::<i32>(0);
        }
    });
    cm_speed!(s, t, "void SetSpeedState_Forward(float afX)", forward);
    cm_speed!(s, t, "void SetSpeedState_Backward(float afX)", backward);
    cm_speed!(s, t, "void SetSpeedState_Sideways(float afX)", sideways);
    cm_speed!(
        s,
        t,
        "void SetSpeedState_TurnBreakMul(float afX)",
        turn_break_mul
    );
    cm_speed!(
        s,
        t,
        "void SetSpeedState_TurnSpeedMul(float afX)",
        turn_speed_mul
    );
    cm_speed!(
        s,
        t,
        "void SetSpeedState_TurnMaxSpeed(float afX)",
        turn_max_speed
    );
    cm_speed!(
        s,
        t,
        "void SetSpeedState_ForwardAcc(float afX)",
        forward_acc
    );
    cm_speed!(
        s,
        t,
        "void SetSpeedState_ForwardDeacc(float afX)",
        forward_deacc
    );
    cm_speed!(
        s,
        t,
        "void SetSpeedState_SidewayAcc(float afX)",
        sideway_acc
    );
    cm_speed!(
        s,
        t,
        "void SetSpeedState_SidewayDeacc(float afX)",
        sideway_deacc
    );
    cm_b!(s, t, "void SetWallAvoidanceActive(bool abX)", wall_avoid);
    cm_b!(
        s,
        t,
        "void SetIdleExtraAnimActive(bool abX)",
        idle_extra_active
    );
    cm_b!(
        s,
        t,
        "void SetDynamicObjectAvoidanceActive(bool abX)",
        object_avoid
    );
    cm_b!(s, t, "void SetBankingActive(bool abX)", banking);
    cm_f!(s, t, "void SetBankingAngleMul(float afX)", bank_angle_mul);
    cm_f!(s, t, "void SetBankingMaxAngle(float afX)", bank_max_angle);
    cm_f!(s, t, "void SetBankingMaxSpeed(float afX)", bank_max_speed);
    cm_b!(s, t, "void SetUse3DMovement(bool abX)", is_3d);
    for decl in [
        "void SetTurnStoppedToWalkSpeed(float afX)",
        "void SetTurnWalkToStoppedSpeed(float afX)",
        "void SetVerticalMoveSpeedExtraAnimMul(float afX)",
        "void SetBankingSpeedMul(float afX)",
    ] {
        s.method(t, decl, |_| {});
    }
    s.method(
        t,
        "void SetIdleExtraAnimName(const tString&in asName)",
        |f| {
            let ent = comp(f).ent;
            if let Some(m) = cm(f) {
                let (mn, mx) = (m.st.idle_extra_min, m.st.idle_extra_max);
                setup_idle_extra(ent, m, text(f, 0), mn, mx);
            }
        },
    );
    s.method(
        t,
        "void SetupWallAvoidance(float afRadius, float afSteerAmount, int alSamples)",
        |f| {
            if let Some(m) = cm(f) {
                m.st.wall_radius = f.arg::<f32>(0);
                m.st.wall_steer = f.arg::<f32>(1);
                m.walls = vec![WallSample::default(); f.arg::<i32>(2).max(0) as usize];
            }
        },
    );
    s.method(
        t,
        "void SetupDynamicObjectAvoidance(float afMaxDistance, float afMinMass, float afSteerAmount)",
        |f| {
            if let Some(m) = cm(f) {
                m.st.object_avoid_dist = f.arg::<f32>(0);
                m.st.object_avoid_min_mass = f.arg::<f32>(1);
                m.st.object_avoid_steer = f.arg::<f32>(2);
            }
        },
    );
    s.method(
        t,
        "void SetupIdleExtra(const tString&in asAnimName, float afMinWait, float afMaxWait, bool abPauseProceduralAnims)",
        |f| {
            let ent = comp(f).ent;
            if let Some(m) = cm(f) {
                setup_idle_extra(ent, m, text(f, 0), f.arg::<f32>(1), f.arg::<f32>(2));
            }
        },
    );
    s.method(t, "bool GetIdleExtraAnimActive()", |f| {
        f.ret(cm(f).is_some_and(|m| m.st.idle_extra_active));
    });
}

fn register_pathfinder(s: &Soma) {
    let t = "cLuxPathfinder";
    s.method(
        t,
        "void MoveTo(const cVector3f&in avPos, float afUpdateFreq, bool abExactStopAtEnd, const tString&in asResultCallback=\"\", bool abCallbackInMap=false)",
        |f| {
            let ent = comp(f).ent;
            if let Some(p) = pf(f) {
                pf_move_to(ent, p, vec(f, 0), f.arg::<bool>(2), text(f, 3), f.arg::<bool>(4));
            }
        },
    );
    s.method(
        t,
        "void MoveToNode(const tString&in asNodeName, float afUpdateFreq, bool abExactStopAtEnd, const tString&in asResultCallback=\"\", bool abCallbackInMap=false)",
        |f| {
            let ent = comp(f).ent;
            let Some(p) = pf(f) else { return };
            let node = node_by_name(ent, text(f, 0));
            let goal = if node != 0 {
                Some(node_pos(node))
            } else {
                let e = Io::new().s(0, text(f, 0)).go(op::E_MAP_FIND).ptr(0);
                (e != 0).then(|| Io::new().p(0, e).go(op::E_POS).out_v(0))
            };
            if let Some(g) = goal {
                pf_move_to(ent, p, g, f.arg::<bool>(2), text(f, 3), f.arg::<bool>(4));
            }
        },
    );
    s.method(t, "void Stop()", |f| {
        let a = agent_of(f);
        if let Some(p) = pf(f) {
            pf_stop(p, a);
        }
    });
    s.method(
        t,
        "void SetEndOfPathCallbackFunc(const tString &in asCallbackFunc)",
        |f| {
            let n = text(f, 0).to_vec();
            if let Some(p) = pf(f) {
                p.end_cb = n;
            }
        },
    );
    s.method(t, "bool IsMoving()", |f| {
        f.ret(pf(f).is_some_and(|p| p.st.moving))
    });
    s.method(t, "cVector3f GetNextGoalPos()", |f| {
        let ent = comp(f).ent;
        let Some(p) = pf(f) else { return };
        let r = if p.path.is_empty() {
            H(find(ent)).feet()
        } else {
            let i = (p.st.path_idx as usize).min(p.path.len() - 1);
            p.path.get(i).copied().unwrap_or_default()
        };
        f.ret(r);
    });
    s.method(
        t,
        "cAINode@ GetNodeFromName(const tString&in asName)",
        |f| {
            f.ret_ref(node_by_name(comp(f).ent, text(f, 0)) as *mut c_void);
        },
    );
    s.method(t, "cAINodeContainer@ GetNodeContainer()", |f| {
        let nd = H(find(comp(f).ent)).nodes();
        let c = if nd == 0 {
            0
        } else {
            Io::new().p(0, nd).go(op::ND_CONTAINER).ptr(0)
        };
        f.ret_ref(c as *mut c_void);
    });
    s.method(
        "cAINodeContainer",
        "cAINodeIterator@ GetNodeIterator(const cVector3f &in avPosition, float afRadius)",
        |f| {
            let it = Io::new()
                .p(0, this(f))
                .v(0, vec(f, 0))
                .f(3, f.arg::<f32>(1))
                .go(op::ND_ITER_NEW)
                .ptr(0);
            f.ret_ref(it as *mut c_void);
        },
    );
    s.method("cAINodeIterator", "bool HasNext()", |f| {
        f.ret(Io::new().p(0, this(f)).go(op::ND_ITER_HAS).int(0) != 0);
    });
    s.method("cAINodeIterator", "cAINode@ Next()", |f| {
        f.ret_ref(Io::new().p(0, this(f)).go(op::ND_ITER_NEXT).ptr(0) as *mut c_void);
    });
    s.method(
        t,
        "cAINode@ GetNodeAtPos(const cVector3f &in avPos,float afMinDistance,float afMaxDistance, bool abGetClosest, bool abPosToNodeFreeDirectPathCheck,bool abAgentToNodeFreeDirectPathCheck, cAINode@ apSkipNode, int alFreePathRayNum, uint alFreePathFlags, bool abSkipUsedNodes)",
        |f| {
            let ent = comp(f).ent;
            if pf(f).is_some() {
                let n = node_at_pos(
                    ent,
                    vec(f, 0),
                    f.arg::<f32>(1),
                    f.arg::<f32>(2),
                    f.arg::<bool>(3),
                    f.arg::<bool>(4),
                    f.handle_arg(6) as usize,
                );
                f.ret_ref(n as *mut c_void);
            }
            done(f);
        },
    );
    s.method(
        t,
        "cAINode@ GetNodeAtPos(const cVector3f &in avPos,float afMinDistance,float afMaxDistance, bool abGetClosest, bool abPosToNodeFreeDirectPathCheck,bool abAgentToNodeFreeDirectPathCheck, cAINode@ apSkipNode)",
        |f| {
            let ent = comp(f).ent;
            if pf(f).is_some() {
                let n = node_at_pos(
                    ent,
                    vec(f, 0),
                    f.arg::<f32>(1),
                    f.arg::<f32>(2),
                    f.arg::<bool>(3),
                    f.arg::<bool>(4),
                    f.handle_arg(6) as usize,
                );
                f.ret_ref(n as *mut c_void);
            }
            done(f);
        },
    );
    s.method(
        t,
        "cAINode@ GetNodeInPosLOS(const cVector3f &in avPos, float afMinDistance,float afMaxDistance,bool abAgentToNodeFreeDirectPathCheck=false)",
        |f| {
            let ent = comp(f).ent;
            if pf(f).is_some() {
                let n = node_at_pos(ent, vec(f, 0), f.arg::<f32>(1), f.arg::<f32>(2), true, true, 0);
                f.ret_ref(n as *mut c_void);
            }
        },
    );
    s.method(
        t,
        "bool CheckFreePath(const cVector3f &in avStartPos, const cVector3f &in avTargetPos)",
        |f| {
            let ent = comp(f).ent;
            let nd = H(find(ent)).nodes();
            let (a, b) = (vec(f, 0), vec(f, 1));
            let r = if nd != 0 {
                Io::new()
                    .p(0, nd)
                    .v(0, a)
                    .v(3, b)
                    .go(op::ND_FREE_PATH)
                    .int(0)
                    != 0
            } else {
                Io::new().p(0, ent).v(0, a).v(3, b).go(op::W_LOS).int(0) != 0
            };
            f.ret(r);
        },
    );
    s.method(
        t,
        "bool BuildPathNodeArrayToPos(const cVector3f &in avPos)",
        |f| {
            let ent = comp(f).ent;
            let Some(p) = pf(f) else { return };
            p.node_array.clear();
            p.node_array_dist.clear();
            let h = H(find(ent));
            let nd = h.nodes();
            if nd == 0 {
                f.ret(false);
                return;
            }
            let feet = h.feet();
            let n = Io::new()
                .p(0, nd)
                .v(0, feet)
                .v(3, vec(f, 0))
                .go(op::ND_PATH)
                .ret();
            if n < 0 {
                f.ret(false);
                return;
            }
            let (mut total, mut prev) = (0.0f32, feet);
            for k in (0..n).rev() {
                let at = Io::new().p(0, nd).i(0, k).go(op::ND_PATH_AT);
                let pos = at.out_v(0);
                total += dist3(prev, pos);
                prev = pos;
                p.node_array.push(at.ptr(0));
                p.node_array_dist.push(total);
            }
            f.ret(true);
        },
    );
    s.method(t, "int GetPathNodeArraySize()", |f| {
        f.ret(pf(f).map_or(0, |p| p.node_array.len() as i32));
    });
    s.method(t, "cAINode@ GetPathNodeArrayNode(int alIdx)", |f| {
        let i = f.arg::<i32>(0);
        let r = pf(f)
            .and_then(|p| {
                usize::try_from(i)
                    .ok()
                    .and_then(|i| p.node_array.get(i).copied())
            })
            .unwrap_or(0);
        f.ret_ref(r as *mut c_void);
    });
    s.method(t, "float GetPathNodeArrayDist(int alIdx)", |f| {
        let i = f.arg::<i32>(0);
        let r = pf(f)
            .and_then(|p| {
                usize::try_from(i)
                    .ok()
                    .and_then(|i| p.node_array_dist.get(i).copied())
            })
            .unwrap_or(0.0);
        f.ret(r);
    });
    s.method(t, "float GetPathNodeArrayFullLength()", |f| {
        f.ret(
            pf(f)
                .and_then(|p| p.node_array_dist.last().copied())
                .unwrap_or(0.0),
        );
    });
    s.method(t, "void ClearTrackNodes()", |f| {
        if let Some(p) = pf(f) {
            p.track.clear();
            p.st.track_idx = -1;
        }
    });
    s.method(
        t,
        "void AddTrackNode(const tString&in asNodeName, float afMinWaitTime, float afMaxWaitTime, const tString&in asAnimName, bool abLoopAnim)",
        |f| {
            let n = TrackNode {
                node: Tstr::new(text(f, 0)),
                min_wait: f.arg::<f32>(1),
                max_wait: f.arg::<f32>(2),
                anim: Tstr::new(text(f, 3)),
                looped: f.arg::<bool>(4),
            };
            if let Some(p) = pf(f) {
                p.track.push(Box::new(n));
            }
        },
    );
    s.method(
        t,
        "void StartTrack(bool abLoop, float afUpdateFreq, const tString &in asEndOfTrackCallback)",
        |f| {
            let ent = comp(f).ent;
            if let Some(p) = pf(f) {
                p.st.track_loop = f.arg::<bool>(0);
                p.st.track_freq = f.arg::<f32>(1);
                p.track_cb.set(text(f, 2));
                p.st.track_active = true;
                p.st.track_paused = false;
                p.st.track_idx = 0;
                pf_start_track_node(ent, p);
            }
        },
    );
    s.method(t, "void StopTrack()", |f| {
        let a = agent_of(f);
        if let Some(p) = pf(f) {
            p.st.track_active = false;
            pf_stop(p, a);
        }
    });
    s.method(t, "void ResetCurrentTrackNode()", |f| {
        if let Some(p) = pf(f) {
            p.st.track_idx = 0;
        }
    });
    s.method(t, "int GetTrackNodeNum()", |f| {
        f.ret(pf(f).map_or(0, |p| p.track.len() as i32));
    });
    s.method(t, "cLuxTrackNode@ GetTrackNode(int alIdx)", |f| {
        let i = f.arg::<i32>(0);
        let r = pf(f)
            .and_then(|p| usize::try_from(i).ok().and_then(|i| p.track.get_mut(i)))
            .map_or(std::ptr::null_mut(), |n| {
                std::ptr::from_mut::<TrackNode>(&mut **n)
            });
        f.ret_ref(r);
    });
    s.method(t, "int GetCurrentTrackNode()", |f| {
        f.ret(pf(f).map_or(0, |p| p.st.track_idx))
    });
    s.method(t, "cLuxTrackNode@ GetCurrentTrackNodeData()", |f| {
        let r = pf(f)
            .and_then(|p| {
                usize::try_from(p.st.track_idx)
                    .ok()
                    .and_then(|i| p.track.get_mut(i))
            })
            .map_or(std::ptr::null_mut(), |n| {
                std::ptr::from_mut::<TrackNode>(&mut **n)
            });
        f.ret_ref(r);
    });
    s.method(t, "void SetCurrentTrackWaitTime(float afX)", |f| {
        if let Some(p) = pf(f) {
            p.st.track_wait = f.arg::<f32>(0);
        }
    });
    s.method(t, "float GetCurrentTrackWaitTime()", |f| {
        f.ret(pf(f).map_or(0.0, |p| p.st.track_wait));
    });
    s.method(t, "void GoToNextTrackNode()", |f| {
        let ent = comp(f).ent;
        if let Some(p) = pf(f) {
            pf_next_track_node(ent, p);
        }
    });
    s.method(t, "bool GetTrackActive()", |f| {
        f.ret(pf(f).is_some_and(|p| p.st.track_active))
    });
    s.method(t, "void SetTrackPaused(bool abX)", |f| {
        let (ent, a) = (comp(f).ent, agent_of(f));
        let b = f.arg::<bool>(0);
        if let Some(p) = pf(f) {
            if p.st.track_paused == b {
                return;
            }
            p.st.track_paused = b;
            if b {
                pf_stop(p, a);
            } else if p.st.track_active && !p.st.at_track_node {
                pf_start_track_node(ent, p);
            }
        }
    });
    s.method(t, "bool GetTrackPaused()", |f| {
        f.ret(pf(f).is_some_and(|p| p.st.track_paused))
    });
    s.method(t, "void SetTrackLoop(bool abX)", |f| {
        if let Some(p) = pf(f) {
            p.st.track_loop = f.arg::<bool>(0);
        }
    });
    s.method(t, "bool GetTrackLoop()", |f| {
        f.ret(pf(f).is_some_and(|p| p.st.track_loop))
    });
    s.method(t, "const tString& GetTrackCallback()", |f| {
        f.ret_ref(pf(f).map_or(std::ptr::null_mut(), |p| p.track_cb.mirror()));
    });
    s.method(t, "float GetTrackUpdateFreq()", |f| {
        f.ret(pf(f).map_or(0.0, |p| p.st.track_freq));
    });
    s.method(
        t,
        "void SetNodeContainerName(const tString &in asName)",
        |f| {
            let n = text(f, 0).to_vec();
            if let Some(p) = pf(f) {
                p.container = n;
                p.nodes = 0;
            }
        },
    );
    s.method(t, "void SetMaxHeight(float afX)", |f| {
        if let Some(p) = pf(f) {
            p.max_height = f.arg::<f32>(0);
            p.nodes = 0;
        }
    });
    s.method(t, "void SetNodeName(const tString &in asName)", |_| {});
    s.method(t, "void SetNodeIsAtCenter(bool abX)", |f| {
        if let Some(p) = pf(f) {
            p.at_center = f.arg::<bool>(0);
            p.nodes = 0;
        }
    });
    s.method(t, "void SetMinEdges(int alX)", |_| {});
    s.method(t, "void SetMaxEdges(int alX)", |_| {});
    s.method(t, "void SetMaxEdgeDistance(float afX)", |f| {
        if let Some(p) = pf(f) {
            p.max_edge = f.arg::<f32>(0);
            p.nodes = 0;
        }
    });
    let t = "cLuxTrackNode";
    s.method(t, "const tString& GetNodeName()", |f| {
        f.ret_ref(f.me::<TrackNode>().node.mirror());
    });
    s.method(t, "float GetMinWaitTime()", |f| {
        f.ret(f.me::<TrackNode>().min_wait)
    });
    s.method(t, "float GetMaxWaitTime()", |f| {
        f.ret(f.me::<TrackNode>().max_wait)
    });
    s.method(t, "const tString& GetAnimName()", |f| {
        f.ret_ref(f.me::<TrackNode>().anim.mirror());
    });
    s.method(t, "bool GetLoopAnim()", |f| {
        f.ret(f.me::<TrackNode>().looped)
    });
}

fn bark<'a>(f: &Frame) -> Option<&'a mut Bark> {
    // SAFETY: as `cm`.
    match unsafe { &mut (*(f.this.cast::<Comp>())).k } {
        K::Bark(b) => Some(b),
        _ => None,
    }
}

fn sl<'a>(f: &Frame) -> Option<&'a mut SoundListener> {
    // SAFETY: as `cm`.
    match unsafe { &mut (*(f.this.cast::<Comp>())).k } {
        K::Sl(b) => Some(b),
        _ => None,
    }
}

fn register_misc(s: &Soma) {
    let t = "cLuxBarkMachine";
    s.method(t, "void AddState(int alId)", |f| {
        if let Some(b) = bark(f) {
            let id = f.arg::<i32>(0);
            b.states.entry(id).or_default();
            b.edit = id;
        }
    });
    s.method(t, "void ChangeState(int alId)", |f| {
        if let Some(b) = bark(f) {
            let id = f.arg::<i32>(0);
            if b.cur != id {
                b.cur = id;
                b.count = 0.0;
                if let Some(s) = b.states.get(&id) {
                    b.count = rand_rectf(s.min, s.max);
                }
            }
        }
    });
    s.method(
        t,
        "void SetState_SoundBark(const tString&in asSound, float afMinBetweenTime, float afMaxBetweenTime, bool abWaitForSoundToBeDone)",
        |f| {
            let st = BarkState {
                sound: text(f, 0).to_vec(),
                min: f.arg::<f32>(1),
                max: f.arg::<f32>(2),
            };
            if let Some(b) = bark(f)
                && b.edit >= 0
            {
                b.states.insert(b.edit, st);
            }
        },
    );
    s.method(
        t,
        "void SetState_VoiceBark(const tString&in asSubject, float afMinBetweenTime, float afMaxBetweenTime, bool abWaitForSoundToBeDone,int alPrio=0,float afMinDistance=-1, float afMaxDistance=-1, float afMaxPlayerListeningRange=-1)",
        |_| {},
    );
    s.method(
        t,
        "void SetupVoice(const tString&in asCharacter, bool abUse3D, float afDefaultMinDistance,float afDefaultMaxDistance ,float afDefaultMaxPlayerListeningRange)",
        |_| {},
    );
    s.method(
        t,
        "void PlayVoice(const tString&in asSubject, int alPrio, float afMinDistance=-1, float afMaxDistance=-1, float afMaxPlayerListeningRange=-1)",
        |_| {},
    );
    s.method(t, "void SetActive(bool abX)", |f| {
        comp(f).active = f.arg::<bool>(0)
    });
    s.method(t, "bool IsActive()", |f| f.ret(comp(f).active));

    let t = "cLuxSoundListener";
    s.method(
        t,
        "void LoadFromInstanceVariables(cResourceVarsObject@ apInstanceVars)",
        done,
    );
    macro_rules! sl_f {
        ($set:literal, $get:literal, $field:ident) => {
            s.method(t, $set, |f| {
                if let Some(l) = sl(f) {
                    l.$field = f.arg::<f32>(0);
                }
            });
            s.method(t, $get, |f| f.ret(sl(f).map_or(0.0, |l| l.$field)));
        };
    }
    sl_f!(
        "void SetHearRadius(float afX)",
        "float GetHearRadius()",
        hear_radius
    );
    s.method(t, "void SetMinHearPrio(int alX)", |f| {
        if let Some(l) = sl(f) {
            l.min_prio = f.arg::<i32>(0);
        }
    });
    s.method(t, "int GetMinHearPrio()", |f| {
        f.ret(sl(f).map_or(0, |l| l.min_prio))
    });
    s.method(t, "float GetSoundRadiusMul()", |f| {
        f.ret(sl(f).map_or(0.0, |l| l.radius_mul))
    });
    s.method(t, "float GetSoundMinRadius()", |f| {
        f.ret(sl(f).map_or(0.0, |l| l.min_radius))
    });
    s.method(t, "float GetSoundMaxRadius()", |f| {
        f.ret(sl(f).map_or(0.0, |l| l.max_radius))
    });
    for (decl, w) in [
        ("void SetSoundRadiusMul(float afX)", 0),
        ("void SetSoundMinRadius(float afX)", 1),
        ("void SetSoundMaxRadius(float afX)", 2),
    ] {
        s.method(t, decl, move |f| {
            if let Some(l) = sl(f) {
                let x = f.arg::<f32>(0);
                match w {
                    0 => l.radius_mul = x,
                    1 => l.min_radius = x,
                    _ => l.max_radius = x,
                }
            }
        });
    }
    s.method(t, "void SetActive(bool bX)", |f| {
        comp(f).active = f.arg::<bool>(0)
    });
    s.method(t, "bool IsActive()", |f| f.ret(comp(f).active));
    s.method(t, "bool IsListening()", |f| f.ret(comp(f).active));
    sl_f!(
        "void SetIgnoreSoundRadius(float afX)",
        "float GetIgnoreSoundRadius()",
        ignore_radius
    );
    sl_f!(
        "void SetMaxPlayerPhysicsInteractTime(float afX)",
        "float GetMaxPlayerPhysicsInteractTime()",
        max_player_interact_time
    );

    let t = "cLuxHeadTracker";
    s.method(t, "void SetTrackEntity(iLuxEntity @apEntity)", |f| {
        let p = ent_arg(f, 0);
        if let K::Ht { track, .. } = &mut comp(f).k {
            *track = p;
        }
        done(f);
    });
    s.method(t, "void SetActive(bool abX)", |f| {
        comp(f).active = f.arg::<bool>(0)
    });
    s.method(t, "bool IsActive()", |f| f.ret(comp(f).active));
    s.method(t, "float GetMaxAngle()", |f| {
        f.ret(match comp(f).k {
            K::Ht { max_angle, .. } => max_angle,
            _ => 0.0,
        });
    });
    s.method(t, "void SetMaxAngle(float afX)", |f| {
        if let K::Ht { max_angle, .. } = &mut comp(f).k {
            *max_angle = f.arg::<f32>(0);
        }
    });
    s.method(
        t,
        "void LoadFromVariables(cResourceVarsObject@ apVars)",
        done,
    );
    s.method(t, "void SetMoveSpeedMul(float afX)", |_| {});
}
