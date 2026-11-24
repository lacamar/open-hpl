//! Script natives on the agent entity types, in the order `SomaRegisterAgentNatives` registered them.

use super::super::charmover::{wrap, yaw_to};
use super::super::{angle3, cmax, dist3, normalized};
use super::core::{Arg, agent_pos, call_ent, find, player_feet, send_message, set_senses_active};
use super::host::H;
use super::io::{Io, op};
use super::model::{Comp, zero3};
use crate::math::{Vector3f, dot3, fma, len3, sub3, v3};
use crate::somalux::scriptable::hash64;
use crate::somascript::s2_as::{Engine, Frame, Soma, str_bytes};
use std::cell::Cell;
use std::ffi::c_void;

pub const AGENT_TYPES: [&str; 6] = [
    "iLuxEntity",
    "cLuxProp",
    "cLuxArea",
    "cLuxAgent",
    "cLuxCritter",
    "cLuxLiquidArea",
];

pub fn this(f: &Frame) -> usize {
    f.this as usize
}

pub fn text<'a>(f: &Frame, i: usize) -> &'a [u8] {
    // SAFETY: the declaration passes a `const tString &in` in slot `i`.
    unsafe { str_bytes(f.ptr_arg(i)) }
}

pub fn vec(f: &Frame, i: usize) -> Vector3f {
    *f.rf::<Vector3f>(i)
}

thread_local! {
    static VEC_BUF: Cell<[f32; 3]> = const { Cell::new([0.0; 3]) };
}

pub fn ret_vec_ref(f: &Frame, v: Vector3f) {
    VEC_BUF.with(|b| {
        b.set([v.x, v.y, v.z]);
        f.ret_ref(b.as_ptr());
    });
}

pub fn pinfo() -> Io {
    Io::new().go(op::P_INFO)
}

pub fn player_body() -> bool {
    pinfo().int(0) != 0
}

fn los(ent: usize, a: Vector3f, b: Vector3f) -> bool {
    Io::new().p(0, ent).v(0, a).v(3, b).go(op::W_LOS).int(0) != 0
}

fn eye(ent: usize) -> Vector3f {
    let a = find(ent);
    if a.is_null() {
        return Io::new().p(0, ent).go(op::E_POS).out_v(0);
    }
    // SAFETY: a registry agent.
    unsafe { (*a).st.eye(&mut H(a)) }
}

fn ground_dist(ent: usize, f: &Frame, o: usize) -> f32 {
    let a = find(ent);
    let d = f.arg::<f32>(o);
    if a.is_null() {
        return d;
    }
    // SAFETY: a registry agent.
    let body = unsafe { (*a).body };
    Io::new()
        .p(0, ent)
        .p(1, body)
        .f(0, d)
        .i(0, i32::from(f.arg::<bool>(o + 1)))
        .i(1, f.arg::<i32>(o + 2))
        .f(1, f.arg::<f32>(o + 3))
        .i(2, i32::from(f.arg::<bool>(o + 4)))
        .go(op::W_GROUND)
        .fl(0)
}

fn on_screen(ent: usize, ray: bool) -> bool {
    Io::new()
        .p(0, ent)
        .i(0, i32::from(ray))
        .go(op::W_ON_SCREEN)
        .int(0)
        != 0
}

fn in_player_los(ent: usize, fov: bool) -> bool {
    Io::new()
        .p(0, ent)
        .i(0, i32::from(fov))
        .go(op::W_PLAYER_LOS)
        .int(0)
        != 0
}

fn with_agent(f: &Frame, g: impl FnOnce(&mut super::model::Agent)) {
    let a = find(this(f));
    // SAFETY: a registry agent.
    if let Some(a) = unsafe { a.as_mut() } {
        g(a);
    }
}

fn from_agent<R>(f: &Frame, d: R, g: impl FnOnce(&mut super::model::Agent) -> R) -> R {
    let a = find(this(f));
    // SAFETY: a registry agent.
    unsafe { a.as_mut() }.map_or(d, g)
}

fn in_fov(dir: Vector3f, fwd: Vector3f, fov: f32) -> bool {
    angle3(normalized(dir), normalized(fwd)) < fov * 0.5
}

macro_rules! agent_field {
    ($s:ident, $set:literal, $get:literal, $ty:ty, $d:expr, $($path:tt)+) => {
        $s.method("cLuxAgent", $set, |f| with_agent(f, |a| a.$($path)+ = f.arg::<$ty>(0)));
        $s.method("cLuxAgent", $get, |f| f.ret(from_agent(f, $d, |a| a.$($path)+)));
    };
}

fn glow_add(f: &Frame, col: [f32; 4], y: f32) {
    let p = f.handle_arg(0) as usize;
    if p != 0 {
        Io::new()
            .p(0, p)
            .f(0, col[0])
            .f(1, col[1])
            .f(2, col[2])
            .f(3, col[3])
            .f(4, y)
            .go(op::G_ADD);
    }
    f.release_handles();
}

fn user_script(p: usize) -> *mut c_void {
    if p == 0 {
        return std::ptr::null_mut();
    }
    Io::new().p(0, p).go(op::U_SCRIPT).ptr(0) as *mut c_void
}

pub fn register(e: Engine) {
    let s = Soma(e);
    s.func("uint64 H64(const tString&in asStr)", |f| {
        f.ret(hash64(text(f, 0)));
    });
    let a = "cLuxAgent";
    s.method(a, "iCharacterBody@ GetCharBody()", |f| {
        f.ret_ref(from_agent(f, 0usize, |a| a.body) as *mut c_void);
    });
    s.method(
        a,
        "float GetDistanceToGround(float afMaxTestDistance, bool abCheckDynamic, int alNumOfRays=1, float afRadius=0.25, bool abGetShortest=true)",
        |f| f.ret(ground_dist(this(f), f, 0)),
    );
    s.method(
        a,
        "void GetDistanceToGround(const tString&in asCallbackFunc, float afMaxTestDistance, bool abCheckDynamic, int alNumOfRays=1, float afRadius=0.25, bool abGetClosest=true)",
        |f| {
            let d = ground_dist(this(f), f, 1);
            let decl = [b"void ".as_slice(), text(f, 0), b"(float)"].concat();
            call_ent(this(f), &decl, &[Arg::Float(d)]);
        },
    );
    s.method(a, "float GetDistanceToPlayer()", |f| {
        let p = this(f);
        f.ret(dist3(agent_pos(find(p), p), player_feet()));
    });
    s.method(a, "float GetDistanceToPlayer2D()", |f| {
        let p = this(f);
        let mut d = sub3(agent_pos(find(p), p), player_feet());
        d.y = 0.0;
        f.ret(len3(d));
    });
    s.method(a, "float GetPlayerMovementTowardEntity()", |f| {
        if !player_body() {
            f.ret(-1.0f32);
            return;
        }
        let p = this(f);
        let mut v = Io::new().go(op::P_VEL).out_v(0);
        let mut d = sub3(agent_pos(find(p), p), pinfo().out_v(2));
        v.y = 0.0;
        d.y = 0.0;
        if dot3(v, v) < 0.01 {
            f.ret(-1.0f32);
            return;
        }
        f.ret(dot3(normalized(v), normalized(d)));
    });
    s.method(a, "float GetDistanceToPos(const cVector3f&in avPos)", |f| {
        let p = this(f);
        f.ret(dist3(agent_pos(find(p), p), vec(f, 0)));
    });
    s.method(
        a,
        "float GetDistanceToPos2D(const cVector3f&in avPos)",
        |f| {
            let p = this(f);
            let mut d = sub3(agent_pos(find(p), p), vec(f, 0));
            d.y = 0.0;
            f.ret(len3(d));
        },
    );
    s.method(a, "const cVector3f& GetPlayerPos()", |f| {
        let i = pinfo();
        ret_vec_ref(f, if i.int(0) != 0 { i.out_v(2) } else { zero3() });
    });
    s.method(a, "cVector3f GetPlayerFeetPos()", |f| f.ret(player_feet()));
    s.method(a, "cVector3f GetPlayerHeadPos()", |f| {
        let i = pinfo();
        let p = i.out_v(2);
        f.ret(if i.int(0) != 0 {
            v3(p.x + 0.0, fma(i.fl(8), 0.4, p.y), p.z + 0.0)
        } else {
            zero3()
        });
    });
    s.method(a, "cVector3f GetEyePostion()", |f| f.ret(eye(this(f))));
    s.method(a, "void SetRelativeEyeHeight(float afX)", |f| {
        with_agent(f, |a| a.st.eye_height = f.arg::<f32>(0));
    });
    s.method(a, "float GetRelativeEyeHeight()", |f| {
        f.ret(from_agent(f, 0.0, |a| a.st.eye_height));
    });
    s.method(
        a,
        "bool GetPointIsInFOV(const cVector3f&in avPoint, float afFOV, const cVector3f &in avForward)",
        |f| {
            let p = this(f);
            f.ret(in_fov(sub3(vec(f, 0), eye(p)), vec(f, 2), f.arg::<f32>(1)));
        },
    );
    s.method(
        a,
        "bool GetPlayerIsInFOV(float afFOV, const cVector3f &in avForward)",
        |f| {
            let p = this(f);
            let d = sub3(player_feet(), agent_pos(find(p), p));
            f.ret(in_fov(d, vec(f, 1), f.arg::<f32>(0)));
        },
    );
    s.method(a, "bool GetPlayerIsInLineOfSight()", |f| {
        let p = this(f);
        let ok = !find(p).is_null() && player_body();
        f.ret(ok && los(p, eye(p), pinfo().out_v(2)));
    });
    s.method(
        a,
        "bool GetPlayerIsInLineOfSight(float afFOV, const cVector3f &in avForward, bool abCheckFOV)",
        |f| f.ret(los_fov(this(f), f.arg::<f32>(0), vec(f, 1), f.arg::<bool>(2), false)),
    );
    s.method_new(
        a,
        "void GetPlayerIsInLineOfSight(const tString&in asCallbackFunc)",
        |f| {
            let p = this(f);
            let ok = !find(p).is_null()
                && player_body()
                && pinfo().int(1) == 0
                && los(p, eye(p), pinfo().out_v(2));
            let decl = [b"void ".as_slice(), text(f, 0), b"(bool)"].concat();
            call_ent(p, &decl, &[Arg::Byte(ok)]);
        },
    );
    s.method_new(
        a,
        "void GetPlayerIsInLineOfSight(const tString&in asCallbackFunc, float afFOV, const cVector3f &in avForward, bool abCheckFOV)",
        |f| {
            let ok = los_fov(this(f), f.arg::<f32>(1), vec(f, 2), f.arg::<bool>(3), true);
            let decl = [b"void ".as_slice(), text(f, 0), b"(bool)"].concat();
            call_ent(this(f), &decl, &[Arg::Byte(ok)]);
        },
    );
    s.method(a, "float GetAngleToPos2D(const cVector3f&in avPos)", |f| {
        let p = this(f);
        let ag = find(p);
        let mut d = sub3(vec(f, 0), agent_pos(ag, p));
        d.y = 0.0;
        f.ret(angle_to(ag, d));
    });
    s.method(a, "float GetAngleToPlayer2D()", |f| {
        let p = this(f);
        let ag = find(p);
        let mut d = sub3(player_feet(), agent_pos(ag, p));
        d.y = 0.0;
        f.ret(angle_to(ag, d));
    });
    s.method(a, "void SetSensesActive(bool abX)", |f| {
        set_senses_active(this(f), f.arg::<bool>(0));
    });
    s.method(a, "void ResetPlayerDetectionState()", |f| {
        with_agent(f, |a| a.st.reset_detection());
    });
    s.method(a, "bool GetSensesActive()", |f| {
        f.ret(from_agent(f, false, |a| a.st.senses_active));
    });
    s.method(a, "void SetPlayerDetectedMinTime(float afX)", |f| {
        with_agent(f, |a| a.st.detect_min_time = f.arg::<f32>(0));
    });
    s.method(a, "float GetPlayerDetectedCount()", |f| {
        f.ret(from_agent(f, 0.0, |a| a.st.detect_count));
    });
    s.method(a, "void SetUpdatePlayerDetection(bool abX)", |f| {
        with_agent(f, |a| a.st.update_detection = f.arg::<bool>(0));
    });
    s.method(a, "bool SetUpdatePlayerDetection()", |f| {
        f.ret(from_agent(f, false, |a| a.st.update_detection));
    });
    s.method(a, "bool PlayerIsSeen()", |f| {
        f.ret(from_agent(f, false, |a| a.st.seen));
    });
    s.method(a, "bool PlayerIsDetected()", |f| {
        f.ret(from_agent(f, false, |a| a.st.detected));
    });
    s.method(a, "void SetFOV(float afX)", |f| {
        with_agent(f, |a| a.st.fov = f.arg::<f32>(0));
    });
    s.method(a, "float GetFOV()", |f| {
        f.ret(from_agent(f, 0.0, |a| a.st.fov))
    });
    s.method(a, "void SetFOVMul(float afX)", |f| {
        with_agent(f, |a| a.st.fov_mul = f.arg::<f32>(0));
    });
    s.method(a, "float GetFOVMul()", |f| {
        f.ret(from_agent(f, 1.0, |a| a.st.fov_mul))
    });
    s.method(a, "void SetSightRange(float afX)", |f| {
        with_agent(f, |a| a.st.sight_range = f.arg::<f32>(0));
    });
    s.method(a, "float GetSightRange()", |f| {
        f.ret(from_agent(f, 0.0, |a| a.st.sight_range));
    });
    s.method(a, "void SetSightRangeMul(float afX)", |f| {
        with_agent(f, |a| a.st.sight_range_mul = f.arg::<f32>(0));
    });
    s.method(a, "float GetSightRangeMul()", |f| {
        f.ret(from_agent(f, 1.0, |a| a.st.sight_range_mul));
    });
    s.method(a, "const cVector3f& GetLastKnownPlayerPos()", |f| {
        let p = from_agent(f, [0.0; 3], |a| a.st.last_known_player_pos);
        ret_vec_ref(f, v3(p[0], p[1], p[2]));
    });
    s.method(a, "float GetDistFromLastKnownToActualPlayerPos()", |f| {
        let p = from_agent(f, None, |a| Some(a.st.last_known_player_pos));
        f.ret(p.map_or(0.0, |p| dist3(v3(p[0], p[1], p[2]), player_feet())));
    });
    s.method(a, "void RevealPlayerPos()", |f| {
        let pf = player_feet();
        with_agent(f, |a| {
            a.st.last_known_player_pos = [pf.x, pf.y, pf.z];
            a.st.seen = true;
            a.st.detected = true;
            a.st.detect_count = cmax(a.st.detect_count, a.st.detect_min_time);
        });
    });
    s.method(a, "float GetCurrentPlayerSightDistance()", |f| {
        f.ret(from_agent(f, 0.0, |a| a.st.current_sight_dist));
    });
    s.method(a, "void SetSightRangeAffectedByModifiers(bool abX)", |f| {
        with_agent(f, |a| {
            a.st.sight_range_affected_by_modifiers = f.arg::<bool>(0)
        });
    });
    s.method(a, "bool GetSightRangeAffectedByModifiers()", |f| {
        f.ret(from_agent(f, false, |a| {
            a.st.sight_range_affected_by_modifiers
        }));
    });
    for ty in AGENT_TYPES {
        s.method(ty, "bool CheckIsOnScreen(bool abUseRayCast)", |f| {
            f.ret(on_screen(this(f), f.arg::<bool>(0)));
        });
        s.method_new(
            ty,
            "cParticleSystem@ CreateParticleSystem(const tString&in asName, const tString&in asFile, bool abRemoveWhenDone, bool abAttach)",
            |f| {
                let io = Io::new()
                    .p(0, this(f))
                    .s(0, text(f, 0))
                    .s(1, text(f, 1))
                    .i(0, i32::from(f.arg::<bool>(2)))
                    .i(1, i32::from(f.arg::<bool>(3)))
                    .go(op::E_PARTICLE);
                f.ret_ref(io.ptr(0) as *mut c_void);
            },
        );
    }
    for ty in ["cLuxAgent", "cLuxCritter"] {
        s.method(ty, "bool GetEntityIsInPlayerFOV()", |f| {
            f.ret(on_screen(this(f), false));
        });
        s.method(
            ty,
            "bool GetEntityIsInPlayerLineOfSight(bool abCheckFOV)",
            |f| {
                f.ret(in_player_los(this(f), f.arg::<bool>(0)));
            },
        );
        s.method(
            ty,
            "void GetEntityIsInPlayerLineOfSight(const tString&in asCallbackFunc, bool abCheckFOV)",
            |f| {
                let ok = in_player_los(this(f), f.arg::<bool>(1));
                let decl = [b"void ".as_slice(), text(f, 0), b"(bool)"].concat();
                call_ent(this(f), &decl, &[Arg::Byte(ok)]);
            },
        );
    }
    s.func(
        "bool Entity_IsInPlayerFOV(const tString &in asEntity)",
        |f| {
            let ent = Io::new().s(0, text(f, 0)).go(op::E_MAP_FIND).ptr(0);
            f.ret(on_screen(ent, false));
        },
    );
    s.method(a, "void SetCheckForDoorsCount(float afX)", |f| {
        with_agent(f, |a| a.st.check_doors_count = f.arg::<f32>(0));
    });
    for ty in AGENT_TYPES {
        s.method(ty, "void SetIsDoor(bool abX)", |f| door_flag(f, 0, true));
        s.method(ty, "bool GetIsDoor()", |f| door_flag(f, 0, false));
        s.method(ty, "void SetIsClosedDoor(bool abX)", |f| {
            door_flag(f, 1, true)
        });
        s.method(ty, "bool GetIsClosedDoor()", |f| door_flag(f, 1, false));
    }
    s.method(a, "void SetStaticCollider(bool abX)", |f| {
        let b = f.arg::<bool>(0);
        with_agent(f, |a| {
            a.st.static_collider = b;
            if a.body != 0 {
                let on = i32::from(!b);
                Io::new().p(1, a.body).i(0, 0).i(1, on).go(op::B_SETB);
                Io::new().p(1, a.body).i(0, 1).i(1, on).go(op::B_SETB);
            }
        });
    });
    s.method(a, "bool GetStaticCollider()", |f| {
        f.ret(from_agent(f, false, |a| a.st.static_collider));
    });
    s.method(
        a,
        "void SetAutoDisableWhenOutOfSightActive(bool abX, float afMinDist)",
        |f| {
            with_agent(f, |a| {
                a.st.auto_disable = f.arg::<bool>(0);
                a.st.auto_disable_min_dist = f.arg::<f32>(1);
            });
        },
    );
    s.method(
        a,
        "void SetAutoDisableCallback(const tString&in asCallback)",
        |f| {
            let t = text(f, 0).to_vec();
            with_agent(f, |a| a.auto_disable_cb = t);
        },
    );
    agent_field!(
        s,
        "void SetCheckForDoors(bool abX)",
        "bool GetCheckForDoors()",
        bool,
        false,
        st.check_for_doors
    );
    agent_field!(
        s,
        "void SetMaxCheckDoorDistance(float afX)",
        "float GetMaxCheckDoorDistance()",
        f32,
        0.0,
        st.max_door_dist
    );
    s.method(a, "void SetAlignEntityWithGroundRay(bool abX)", |f| {
        with_agent(f, |a| {
            a.st.align_ground = f.arg::<bool>(0);
            a.st.align_count = 0;
            a.st.align_idx = 0;
        });
    });
    s.method(a, "bool GetAlignEntityWithGroundRay()", |f| {
        f.ret(from_agent(f, false, |a| a.st.align_ground));
    });
    s.method(a, "const cVector3f& GetGroundAlignPosition()", |f| {
        let p = from_agent(f, [0.0; 3], |a| a.st.ground_align_pos);
        ret_vec_ref(f, v3(p[0], p[1], p[2]));
    });
    s.method(
        a,
        "void SetAlignEntityWithGroundRelativeRayStart(float afX)",
        |f| with_agent(f, |a| a.st.align_ray_start = f.arg::<f32>(0)),
    );
    s.method(a, "float GetAlignEntityWithGroundRelativeRayStart()", |f| {
        f.ret(from_agent(f, 0.5, |a| a.st.align_ray_start));
    });
    s.method(
        a,
        "void SetAlignEntityWithGroundMaxRayDistance(float afX)",
        |f| with_agent(f, |a| a.st.align_ray_max = f.arg::<f32>(0)),
    );
    s.method(a, "float GetAlignEntityWithGroundMaxRayDistance()", |f| {
        f.ret(from_agent(f, 0.5, |a| a.st.align_ray_max));
    });
    s.method(
        a,
        "void BroadcastMessage(int alMessageId, iLuxEntityComponent@ apSource, const cVector3f &in avData, int alData)",
        |f| {
            send_message(this(f), f.arg::<i32>(0), vec(f, 2), f.arg::<i32>(3));
            f.release_handles();
        },
    );
    s.method(
        a,
        "void SetRecieveMessageCallback(const tString&in asCallbackFunc)",
        |f| {
            let t = text(f, 0).to_vec();
            with_agent(f, |a| a.message_cb = t);
        },
    );

    super::natives_c::register_creators(&s);
    s.method(
        "cLuxEffectHandler",
        "void AddGlowObject(iLuxEntity@ apEntity, float afAlpha, float afY)",
        |f| glow_add(f, [0.2, 0.2, 1.0, f.arg::<f32>(1)], f.arg::<f32>(2)),
    );
    s.method(
        "cLuxEffectHandler",
        "void AddGlowObject(iLuxEntity@ apEntity, const cColor&in aColor, float afY)",
        |f| glow_add(f, *f.rf::<[f32; 4]>(1), f.arg::<f32>(2)),
    );
    super::natives_c::register_components(&s, e);
}

fn los_fov(ent: usize, fov: f32, fwd: Vector3f, check_fov: bool, cb: bool) -> bool {
    let a = find(ent);
    let info = pinfo();
    if a.is_null() || info.int(0) == 0 || (cb && info.int(1) != 0) {
        return false;
    }
    let pos = info.out_v(2);
    let e = eye(ent);
    if check_fov && angle3(normalized(sub3(pos, e)), normalized(fwd)) > fov * 0.5 {
        return false;
    }
    los(ent, e, pos)
}

fn angle_to(ag: *mut super::model::Agent, d: Vector3f) -> f32 {
    // SAFETY: a registry agent or null.
    match unsafe { ag.as_ref() } {
        Some(a) if a.body != 0 => {
            let yaw = Io::new().p(1, a.body).i(0, 0).go(op::B_GETF).fl(0);
            wrap(yaw_to(d) - yaw).abs()
        }
        _ => 0.0,
    }
}

fn door_flag(f: &Frame, which: i32, set: bool) {
    let v = set.then(|| f.arg::<bool>(0));
    let flag = super::pod::door_flag(this(f), which, v);
    if !set {
        f.ret(flag);
    }
}

pub fn comp_of_this(f: &Frame) -> &mut Comp {
    f.me::<Comp>()
}

pub fn script_of(p: usize) -> *mut c_void {
    user_script(p)
}
