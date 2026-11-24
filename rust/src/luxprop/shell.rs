//! State of the generic C++ prop shell (`cLuxPropRs`): button, photocell, emotion stone, oil barrel,
//! lever, wheel and multi slider. C++ keeps the engine side (effects, sounds, messages, joints, the
//! player state) behind the op sink and the host tables; everything else lives in [`PropRs`].
#![allow(clippy::missing_safety_doc)]

use super::a5_ffi::{OilHostC, PhotoHostC, with_ops};
use super::a5_small::{self as s, A5Button, A5Oil, A5Photocell};
use super::ffi::{Host, Sink, VarsRef};
use super::lever::{self, LeverParams, LeverState};
use super::multislider::{self, MultiParams, MultiState};
use super::object::MoveData;
use super::opc as op_c;
use super::ops::Pod;
use super::propsc;
use super::vars::Vars;
use super::wheel::{self, WheelParams, WheelState};
use crate::luxsave::a8_props as sv;
use crate::types::{Color, Vector3f};
use std::cell::RefCell;

const BUTTON: i32 = 0;
const PHOTOCELL: i32 = 1;
const EMOTION: i32 = 2;
const OIL: i32 = 3;
const LEVER: i32 = 4;
const WHEEL: i32 = 5;
const MULTI: i32 = 6;

type Map = &'static [(i32, usize)];

const EMOTION_MAP: Map = &[
    (op_c::SLOT_EMOTION_DESC_CAT, 0),
    (op_c::SLOT_EMOTION_DESC_ENTRY, 1),
    (op_c::SLOT_EMOTION_TEXT_CAT, 2),
    (op_c::SLOT_EMOTION_TEXT_ENTRY, 3),
    (op_c::SLOT_EMOTION_SOUND, 4),
];
const OIL_MAP: Map = &[(s::a5op::SLOT_OIL_FILL, 5), (s::a5op::SLOT_OIL_EMPTY, 6)];
const LEVER_MAP: Map = &[
    (lever::SLOT_MIN_LIMIT_SOUND, 0),
    (lever::SLOT_MAX_LIMIT_SOUND, 1),
    (lever::SLOT_STUCK_SOUND, 2),
];
const WHEEL_MAP: Map = &[
    (wheel::SLOT_MIN_LIMIT_SOUND, 0),
    (wheel::SLOT_MAX_LIMIT_SOUND, 1),
    (wheel::SLOT_STUCK_SOUND, 2),
];
const MULTI_MAP: Map = &[
    (multislider::SLOT_CHANGE_STATE_SOUND, 0),
    (multislider::SLOT_STUCK_SOUND, 1),
];
const MULTI_CALLBACK: Map = &[(multislider::SLOT_CHANGE_STATE_CALLBACK, 2)];

const NO_STUCK: i32 = i32::MIN;

#[derive(Default)]
struct Strs([Vec<u8>; 7]);

enum Kind {
    Button(A5Button),
    Photocell(A5Photocell),
    Emotion,
    Oil(A5Oil),
    Lever(LeverParams, LeverState),
    Wheel(WheelParams, WheelState),
    Multi(MultiParams, MultiState, Vec<f32>),
}

pub struct PropRs {
    kind: Kind,
    strs: Strs,
}

/// Loader variables, with the string slots kept here instead of in C++ members.
struct Capture<'a> {
    inner: &'a VarsRef,
    got: RefCell<&'a mut Strs>,
    map: Map,
}

impl<'a> Capture<'a> {
    fn new(inner: &'a VarsRef, got: &'a mut Strs, map: Map) -> Self {
        Self {
            inner,
            got: RefCell::new(got),
            map,
        }
    }
}

impl Vars for Capture<'_> {
    fn string(&self, name: &str, def: &[u8]) -> Vec<u8> {
        self.inner.string(name, def)
    }

    fn boolean(&self, name: &str, def: bool) -> bool {
        self.inner.boolean(name, def)
    }

    fn float(&self, name: &str, def: f32) -> f32 {
        self.inner.float(name, def)
    }

    fn int(&self, name: &str, def: i32) -> i32 {
        self.inner.int(name, def)
    }

    fn vec3(&self, name: &str, def: Vector3f) -> Vector3f {
        self.inner.vec3(name, def)
    }

    fn color(&self, name: &str, def: Color) -> Color {
        self.inner.color(name, def)
    }

    fn put(&self, slot: i32, v: &[u8]) {
        if let Some(&(_, i)) = self.map.iter().find(|m| m.0 == slot)
            && let Some(d) = self.got.borrow_mut().0.get_mut(i)
        {
            *d = v.to_vec();
        }
    }
}

fn rw<'a>(p: *mut PropRs) -> &'a mut PropRs {
    // SAFETY: caller contract: `p` comes from `ohpl_prs_new`; no borrow lives across a sink call.
    unsafe { &mut *p }
}

fn host<'a>(h: *const Host) -> &'a Host {
    // SAFETY: caller contract: the table lives for the call.
    unsafe { &*h }
}

fn snk<'a>(s: *const Sink) -> &'a Sink {
    // SAFETY: caller contract: the sink lives for the call.
    unsafe { &*s }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_prs_new(kind: i32) -> *mut PropRs {
    let kind = match kind {
        BUTTON => Kind::Button(A5Button::new()),
        PHOTOCELL => Kind::Photocell(A5Photocell::default()),
        EMOTION => Kind::Emotion,
        OIL => Kind::Oil(A5Oil::default()),
        LEVER => Kind::Lever(LeverParams::default(), LeverState::default()),
        WHEEL => Kind::Wheel(WheelParams::default(), WheelState::default()),
        MULTI => Kind::Multi(MultiParams::default(), MultiState::new(), Vec::new()),
        _ => return std::ptr::null_mut(),
    };
    Box::into_raw(Box::new(PropRs {
        kind,
        strs: Strs::default(),
    }))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_free(p: *mut PropRs) {
    if !p.is_null() {
        // SAFETY: caller contract: `p` came from `ohpl_prs_new` and is not used again.
        drop(unsafe { Box::from_raw(p) });
    }
}

/// `LoadVariables` (entity file).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_load_vars(p: *mut PropRs, v: *const VarsRef) {
    let PropRs { kind, strs } = rw(p);
    // SAFETY: caller contract.
    let inner = unsafe { &*v };
    match kind {
        Kind::Oil(_) => s::oil_load(&Capture::new(inner, strs, OIL_MAP)),
        Kind::Lever(pa, _) => lever::load(&Capture::new(inner, strs, LEVER_MAP), pa),
        Kind::Wheel(pa, _) => wheel::load(&Capture::new(inner, strs, WHEEL_MAP), pa),
        Kind::Multi(pa, ..) => multislider::load(&Capture::new(inner, strs, MULTI_MAP), pa),
        _ => {}
    }
}

/// Writes the interaction data (max speed, slow down, speed factor, throw impulse) of a lever,
/// wheel or multi slider; true when a multi slider has fewer than two states.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_move_data(p: *mut PropRs, out: *mut f32) -> bool {
    let (d, few): (MoveData, bool) = match &rw(p).kind {
        Kind::Lever(pa, _) => (pa.data, false),
        Kind::Wheel(pa, _) => (pa.data, false),
        Kind::Multi(pa, ..) => (pa.data, pa.few_states),
        _ => return false,
    };
    // SAFETY: caller contract: `out` holds four floats.
    unsafe {
        out.write(d.max_speed);
        out.add(1).write(d.slow_down_factor);
        out.add(2).write(d.speed_factor);
        out.add(3).write(d.throw_impulse);
    }
    few
}

/// `LoadInstanceVariables` (map entry).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_load_instance(
    p: *mut PropRs,
    v: *const VarsRef,
    h: *const Host,
    sink: *const Sink,
) {
    // SAFETY: caller contract.
    let inner = unsafe { &*v };
    let sink = snk(sink);
    let PropRs { kind, strs } = rw(p);
    match kind {
        Kind::Button(b) => {
            let pod = Pod::new(b);
            with_ops(sink, |ops| s::button_load_instance(inner, pod, ops));
        }
        Kind::Photocell(c) => s::photocell_load_instance(inner, c),
        Kind::Emotion => propsc::emotion_load_instance(&Capture::new(inner, strs, EMOTION_MAP)),
        Kind::Oil(_) => {}
        Kind::Lever(pa, st) => {
            let (pa, st) = (Pod::new(pa), Pod::new(st));
            with_ops(sink, |ops| lever::load_instance(inner, st, pa, ops));
        }
        Kind::Wheel(pa, st) => {
            let (pa, st) = (Pod::new(pa), Pod::new(st));
            with_ops(sink, |ops| {
                wheel::load_instance(inner, st, pa, &mut host(h), ops);
            });
        }
        Kind::Multi(_, st, states) => {
            let st = Pod::new(st);
            let states = states.clone();
            let mut tmp = Strs::default();
            with_ops(sink, |ops| {
                let cap = Capture::new(inner, &mut tmp, MULTI_CALLBACK);
                multislider::load_instance(&cap, st, &states, ops);
            });
            if let (Some(d), Some(cb)) = (strs.0.get_mut(2), tmp.0.into_iter().nth(2)) {
                *d = cb;
            }
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_can_interact(p: *mut PropRs, mass: f32) -> bool {
    match &rw(p).kind {
        Kind::Button(b) => s::button_can_interact(b),
        Kind::Photocell(_) => false,
        Kind::Emotion | Kind::Oil(_) => true,
        Kind::Lever(pa, _) => lever::can_interact(pa, mass),
        Kind::Wheel(pa, _) => wheel::can_interact(pa, mass),
        Kind::Multi(pa, ..) => multislider::can_interact(pa, mass),
    }
}

/// 0 default, 1 grab, 2 grab when the body can be interacted with.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_crosshair(p: *mut PropRs) -> i32 {
    match &rw(p).kind {
        Kind::Photocell(_) => 0,
        Kind::Emotion | Kind::Oil(_) => 1,
        _ => 2,
    }
}

/// 0 can interact with static bodies, 1 lever auto move to angle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_flag(p: *mut PropRs, which: i32) -> bool {
    match (&rw(p).kind, which) {
        (Kind::Lever(pa, _), 0) => pa.can_interact_static,
        (Kind::Wheel(pa, _), 0) => pa.can_interact_static,
        (Kind::Multi(pa, ..), 0) => pa.can_interact_static,
        (Kind::Lever(pa, _), 1) => pa.auto_move_to_angle,
        _ => false,
    }
}

/// `OnInteract` of the button, photocell, emotion stone and oil barrel; `oil` is only read for
/// the oil barrel.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_interact(
    p: *mut PropRs,
    oil: *const OilHostC,
    sink: *const Sink,
) -> bool {
    let sink = snk(sink);
    match &mut rw(p).kind {
        Kind::Button(b) => {
            let pod = Pod::new(b);
            with_ops(sink, |ops| s::button_interact(pod, ops));
            true
        }
        Kind::Oil(o) if !oil.is_null() => {
            // SAFETY: the table lives for the call.
            let mut host = unsafe { &*oil };
            let o = Pod::new(o);
            with_ops(sink, |ops| s::oil_interact(o.m(), &mut host, ops))
        }
        _ => true,
    }
}

/// `OnInteract` of the lever, wheel and multi slider before the player state change; true when
/// the hint is due.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_joint_interact(
    p: *mut PropRs,
    show_hints: bool,
    h: *const Host,
    sink: *const Sink,
) -> bool {
    let sink = snk(sink);
    match &mut rw(p).kind {
        Kind::Lever(pa, st) => {
            let (pa, st) = (Pod::new(pa), Pod::new(st));
            with_ops(sink, |ops| lever::on_interact(st, pa.r(), show_hints, ops))
        }
        Kind::Wheel(pa, st) => {
            let (pa, st) = (Pod::new(pa), Pod::new(st));
            with_ops(sink, |ops| {
                let disables = st.r().interaction_disables_stuck;
                wheel::on_interact(st, pa.r(), show_hints, disables, &mut host(h), ops)
            })
        }
        Kind::Multi(_, st, states) => {
            let st = Pod::new(st);
            let states = states.clone();
            with_ops(sink, |ops| multislider::on_interact(st, &states, ops));
            false
        }
        _ => false,
    }
}

/// Focus text: emotion stone 1 when it has a description, oil barrel its tier 0 to 4.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_focus(p: *mut PropRs, oil: *const OilHostC) -> i32 {
    let me = rw(p);
    match &mut me.kind {
        Kind::Emotion => {
            let [cat, entry, ..] = &me.strs.0;
            i32::from(propsc::emotion_has_text(cat.is_empty(), entry.is_empty()))
        }
        Kind::Oil(o) if !oil.is_null() => {
            // SAFETY: the table lives for the call.
            s::oil_focus_tier(o, &mut unsafe { &*oil })
        }
        _ => 0,
    }
}

/// Photocell light sampling.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_update(
    p: *mut PropRs,
    dt: f32,
    photo: *const PhotoHostC,
    sink: *const Sink,
) {
    if let Kind::Photocell(c) = &mut rw(p).kind
        && !photo.is_null()
    {
        let c = Pod::new(c);
        // SAFETY: the table lives for the call.
        let mut hst = unsafe { &*photo };
        with_ops(snk(sink), |ops| s::photocell_update(c, dt, &mut hst, ops));
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_joint_update(
    p: *mut PropRs,
    dt: f32,
    interacted: bool,
    h: *const Host,
    sink: *const Sink,
) {
    let sink = snk(sink);
    match &mut rw(p).kind {
        Kind::Lever(pa, st) => {
            let (pa, st) = (Pod::new(pa), Pod::new(st));
            with_ops(sink, |ops| {
                lever::update(st, pa.r(), dt, interacted, &mut host(h), ops);
            });
        }
        Kind::Wheel(pa, st) => {
            let (pa, st) = (Pod::new(pa), Pod::new(st));
            with_ops(sink, |ops| {
                wheel::update(st, pa.r(), dt, interacted, &mut host(h), ops);
            });
        }
        Kind::Multi(pa, st, states) => {
            let (pa, st) = (Pod::new(pa), Pod::new(st));
            let states = states.clone();
            with_ops(sink, |ops| {
                multislider::update(st, pa.r(), &states, dt, interacted, &mut host(h), ops);
            });
        }
        _ => {}
    }
}

/// `OnSetupAfterLoad`: the joint limits (lever) or distances (multi slider) of the joint.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_setup(p: *mut PropRs, min: f32, max: f32, sink: *const Sink) {
    let sink = snk(sink);
    match &mut rw(p).kind {
        Kind::Lever(pa, st) => {
            let (pa, st) = (Pod::new(pa), Pod::new(st));
            with_ops(sink, |ops| lever::setup(st.m(), pa.r(), min, max, ops));
        }
        Kind::Wheel(pa, _) => {
            let pa = Pod::new(pa);
            with_ops(sink, |ops| wheel::setup(pa.r(), ops));
        }
        Kind::Multi(pa, st, states) => {
            st.default_min_dist = min;
            st.default_max_dist = max;
            *states = multislider::compute_states(min, max, pa.num_of_states);
        }
        _ => {}
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_conn(
    p: *mut PropRs,
    state: i32,
    h: *const Host,
    sink: *const Sink,
) {
    let sink = snk(sink);
    match &mut rw(p).kind {
        Kind::Button(b) => {
            let b = Pod::new(b);
            with_ops(sink, |ops| s::button_conn(b, state, ops));
        }
        Kind::Lever(pa, st) => {
            let (pa, st) = (Pod::new(pa), Pod::new(st));
            with_ops(sink, |ops| {
                lever::on_connection_state(st, pa.r(), state, ops)
            });
        }
        Kind::Wheel(pa, st) => {
            let (pa, st) = (Pod::new(pa), Pod::new(st));
            with_ops(sink, |ops| {
                wheel::on_connection_state(st, pa.r(), state, &mut host(h), ops);
            });
        }
        _ => {}
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_set_on(
    p: *mut PropRs,
    on: bool,
    effects: bool,
    sink: *const Sink,
) {
    if let Kind::Button(b) = &mut rw(p).kind {
        let b = Pod::new(b);
        with_ops(snk(sink), |ops| s::button_set_on(b, on, effects, ops));
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_set_can(p: *mut PropRs, off: bool, v: bool) {
    if let Kind::Button(b) = &mut rw(p).kind {
        if off {
            b.can_off = v;
        } else {
            b.can_on = v;
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_set_stuck(
    p: *mut PropRs,
    state: i32,
    effects: bool,
    h: *const Host,
    sink: *const Sink,
) {
    let sink = snk(sink);
    match &mut rw(p).kind {
        Kind::Lever(pa, st) => {
            let (pa, st) = (Pod::new(pa), Pod::new(st));
            with_ops(sink, |ops| {
                lever::set_stuck_state(st, pa.r(), state, effects, ops)
            });
        }
        Kind::Wheel(pa, st) => {
            let (pa, st) = (Pod::new(pa), Pod::new(st));
            with_ops(sink, |ops| {
                wheel::set_stuck_state(st, pa.r(), state, effects, &mut host(h), ops);
            });
        }
        Kind::Multi(_, st, states) => {
            let st = Pod::new(st);
            let states = states.clone();
            with_ops(sink, |ops| {
                multislider::set_stuck_state(st, &states, state, effects, ops);
            });
        }
        _ => {}
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_stuck_state(p: *mut PropRs) -> i32 {
    match &rw(p).kind {
        Kind::Lever(_, st) => st.stuck_state,
        Kind::Wheel(_, st) => st.stuck_state,
        Kind::Multi(_, st, _) => st.stuck_state,
        _ => 0,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_lever_state(p: *mut PropRs) -> i32 {
    match &rw(p).kind {
        Kind::Lever(_, st) => st.current_state,
        _ => 0,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_set_disables(p: *mut PropRs, v: bool) {
    match &mut rw(p).kind {
        Kind::Lever(_, st) => st.interaction_disables_stuck = v,
        Kind::Wheel(_, st) => st.interaction_disables_stuck = v,
        Kind::Multi(_, st, _) => st.interaction_disables_stuck = v,
        _ => {}
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_set_angle(
    p: *mut PropRs,
    x: f32,
    auto_move: bool,
    sink: *const Sink,
) {
    if let Kind::Wheel(pa, st) = &mut rw(p).kind {
        let (pa, st) = (Pod::new(pa), Pod::new(st));
        with_ops(snk(sink), |ops| {
            wheel::set_angle(st.m(), pa.r(), x, auto_move, ops)
        });
    }
}

/// Wheel angle, minimum limit and maximum limit.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_wheel(p: *mut PropRs, out: *mut f32) {
    if let Kind::Wheel(pa, st) = &rw(p).kind {
        // SAFETY: caller contract: `out` holds three floats.
        unsafe {
            out.write(st.angle);
            out.add(1).write(pa.min_limit);
            out.add(2).write(pa.max_limit);
        }
    }
}

/// String slot `i`: emotion desc cat, desc entry, text cat, text entry, sound, oil fill, oil
/// empty; lever and wheel min sound, max sound, stuck sound; multi change sound, stuck sound,
/// change callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_str(p: *mut PropRs, i: i32, n: *mut usize) -> *const u8 {
    let v = usize::try_from(i)
        .ok()
        .and_then(|i| rw(p).strs.0.get(i))
        .map_or(&[][..], Vec::as_slice);
    // SAFETY: caller contract: `n` is writable.
    unsafe { *n = v.len() };
    v.as_ptr()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_set_str(p: *mut PropRs, i: i32, s: *const u8, n: usize) {
    let v = if n == 0 {
        &[][..]
    } else {
        // SAFETY: caller contract: `s` holds `n` bytes.
        unsafe { std::slice::from_raw_parts(s, n) }
    };
    if let Some(d) = usize::try_from(i)
        .ok()
        .and_then(|i| rw(p).strs.0.get_mut(i))
    {
        *d = v.to_vec();
    }
}

const LEVER_SAVE: &[u8] = b"cLuxProp_Lever_SaveData";
const WHEEL_SAVE: &[u8] = b"cLuxProp_Wheel_SaveData";
const MULTI_SAVE: &[u8] = b"cLuxProp_MultiSlider_SaveData";

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_save(p: *mut PropRs, d: *mut u8) {
    // SAFETY: caller contract: `d` is the save data of the prop's kind.
    unsafe {
        match &rw(p).kind {
            Kind::Button(b) => sv::ohpl_a8_button_save(b, d),
            Kind::Photocell(c) => sv::ohpl_a8_photocell_save(c, d),
            Kind::Oil(o) => sv::ohpl_a8_oil_save(o, d),
            Kind::Emotion => {}
            Kind::Lever(pa, st) => {
                sv::put(d, LEVER_SAVE, b"mlCurrentState", st.current_state);
                sv::put(d, LEVER_SAVE, b"mlStuckState", st.stuck_state);
                sv::put(
                    d,
                    LEVER_SAVE,
                    b"mbInteractionDisablesStuck",
                    st.interaction_disables_stuck,
                );
                sv::put(
                    d,
                    LEVER_SAVE,
                    b"mfMiddleAngleAmount",
                    pa.middle_angle_amount,
                );
                sv::put(d, LEVER_SAVE, b"mbAutoMoveToAngle", pa.auto_move_to_angle);
                sv::put(d, LEVER_SAVE, b"mlAutoMoveGoal", pa.auto_move_goal);
            }
            Kind::Wheel(pa, st) => {
                sv::put(d, WHEEL_SAVE, b"mfAngle", st.angle);
                sv::put(d, WHEEL_SAVE, b"mfPrevAngle", st.prev_angle);
                sv::put(d, WHEEL_SAVE, b"mfLastToMin", st.last_to_min);
                sv::put(d, WHEEL_SAVE, b"mfLastToMax", st.last_to_max);
                sv::put(d, WHEEL_SAVE, b"mlCurrentState", st.current_state);
                sv::put(d, WHEEL_SAVE, b"mlStuckState", st.stuck_state);
                sv::put(d, WHEEL_SAVE, b"mfJointAngle", st.joint_angle);
                sv::put(d, WHEEL_SAVE, b"mfPrevJointAngle", st.prev_joint_angle);
                sv::put(d, WHEEL_SAVE, b"mbAutoMoving", st.auto_moving);
                sv::put(d, WHEEL_SAVE, b"mfAutoMoveGoal", st.auto_move_goal);
                sv::put(d, WHEEL_SAVE, b"mfMinLimit", pa.min_limit);
                sv::put(d, WHEEL_SAVE, b"mfMaxLimit", pa.max_limit);
                sv::put(d, WHEEL_SAVE, b"mbMinLimitStuck", pa.min_limit_stuck);
                sv::put(d, WHEEL_SAVE, b"mbMaxLimitStuck", pa.max_limit_stuck);
                sv::put(d, WHEEL_SAVE, b"mlSpinDir", pa.spin_dir);
                sv::put(
                    d,
                    WHEEL_SAVE,
                    b"mbInteractionDisablesStuck",
                    st.interaction_disables_stuck,
                );
            }
            Kind::Multi(_, st, _) => {
                sv::put(d, MULTI_SAVE, b"mlCurrentState", st.current_state);
                sv::put(d, MULTI_SAVE, b"mlStuckState", st.stuck_state);
                sv::put(
                    d,
                    MULTI_SAVE,
                    b"mbInteractionDisablesStuck",
                    st.interaction_disables_stuck,
                );
            }
        }
    }
}

/// `LoadFromSaveData` in two phases around the stuck state, which C++ sets through
/// `ohpl_prs_set_stuck` (it runs ops). Phase 0 returns the stored stuck state of the lever,
/// wheel and multi slider, else `i32::MIN`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_prs_load(p: *mut PropRs, d: *mut u8, phase: i32) -> i32 {
    // SAFETY: caller contract: `d` is the save data of the prop's kind.
    unsafe {
        match (&mut rw(p).kind, phase) {
            (Kind::Button(b), 0) => sv::ohpl_a8_button_load(b, d),
            (Kind::Photocell(c), 0) => sv::ohpl_a8_photocell_load(c, d),
            (Kind::Oil(o), 0) => sv::ohpl_a8_oil_load(o, d),
            (Kind::Lever(_, st), 0) => {
                st.current_state = sv::get(d, LEVER_SAVE, b"mlCurrentState");
                return sv::get(d, LEVER_SAVE, b"mlStuckState");
            }
            (Kind::Lever(pa, st), _) => {
                st.interaction_disables_stuck =
                    sv::get(d, LEVER_SAVE, b"mbInteractionDisablesStuck");
                pa.middle_angle_amount = sv::get(d, LEVER_SAVE, b"mfMiddleAngleAmount");
                pa.auto_move_to_angle = sv::get(d, LEVER_SAVE, b"mbAutoMoveToAngle");
                pa.auto_move_goal = sv::get(d, LEVER_SAVE, b"mlAutoMoveGoal");
            }
            (Kind::Wheel(pa, st), 0) => {
                st.angle = sv::get(d, WHEEL_SAVE, b"mfAngle");
                st.prev_angle = sv::get(d, WHEEL_SAVE, b"mfPrevAngle");
                st.last_to_min = sv::get(d, WHEEL_SAVE, b"mfLastToMin");
                st.last_to_max = sv::get(d, WHEEL_SAVE, b"mfLastToMax");
                st.current_state = sv::get(d, WHEEL_SAVE, b"mlCurrentState");
                st.joint_angle = sv::get(d, WHEEL_SAVE, b"mfJointAngle");
                st.prev_joint_angle = sv::get(d, WHEEL_SAVE, b"mfPrevJointAngle");
                st.auto_moving = sv::get(d, WHEEL_SAVE, b"mbAutoMoving");
                st.auto_move_goal = sv::get(d, WHEEL_SAVE, b"mfAutoMoveGoal");
                pa.min_limit = sv::get(d, WHEEL_SAVE, b"mfMinLimit");
                pa.max_limit = sv::get(d, WHEEL_SAVE, b"mfMaxLimit");
                pa.min_limit_stuck = sv::get(d, WHEEL_SAVE, b"mbMinLimitStuck");
                pa.max_limit_stuck = sv::get(d, WHEEL_SAVE, b"mbMaxLimitStuck");
                pa.spin_dir = sv::get(d, WHEEL_SAVE, b"mlSpinDir");
                st.interaction_disables_stuck =
                    sv::get(d, WHEEL_SAVE, b"mbInteractionDisablesStuck");
                return sv::get(d, WHEEL_SAVE, b"mlStuckState");
            }
            (Kind::Multi(_, st, _), 0) => {
                st.current_state = sv::get(d, MULTI_SAVE, b"mlCurrentState");
                return sv::get(d, MULTI_SAVE, b"mlStuckState");
            }
            (Kind::Multi(_, st, _), _) => {
                st.interaction_disables_stuck =
                    sv::get(d, MULTI_SAVE, b"mbInteractionDisablesStuck");
            }
            _ => {}
        }
    }
    NO_STUCK
}
