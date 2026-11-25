//! Prop, door, lamp, lever, wheel, button, NPC, sticky area and attachment functions.

use super::super::a1::Arg;
use super::entity::{MOVE_OBJECT, body, lossy, matrix_of, set_each, transform};
use super::{P, S, TYPE_ANY, TYPE_AREA, TYPE_PROP, call, each, entity, error, ops, s, sf, warning};
use crate::math::{self, Matrixf};

const SWING_DOOR: i32 = 1;
const LEVER: i32 = 2;
const WHEEL: i32 = 3;
const LAMP: i32 = 4;
const LEVEL_DOOR: i32 = 10;
const BUTTON: i32 = 11;
const NPC: i32 = 14;
const MULTI_SLIDER: i32 = 15;
const OBJECT: i32 = 0;
const STICKY: i32 = 4;

fn mat_arg(m: &Matrixf) -> Arg {
    Arg::ptr(std::ptr::from_ref(m).cast())
}

set_each!(
    PauseCurrentAnimation(fade: f32),
    TYPE_PROP,
    -1,
    ops::PROP_PAUSE_ANIM
);
set_each!(
    PlayCurrentAnimation(fade: f32, looped: bool),
    TYPE_PROP,
    -1,
    ops::PROP_PLAY_ANIM
);
set_each!(
    SetPropAnimationPosition(pos: f32),
    TYPE_PROP,
    -1,
    ops::PROP_ANIM_POS
);
set_each!(
    SetPropAnimationSpeed(speed: f32),
    TYPE_PROP,
    -1,
    ops::PROP_ANIM_SPEED
);
set_each!(StopPropAnimation(), TYPE_PROP, -1, ops::PROP_STOP_ANIM);
set_each!(
    PlayPropAnimation(anim: S, fade: f32, looped: bool, cb: S),
    TYPE_PROP,
    -1,
    ops::PROP_PLAY_ANIMATION
);
set_each!(
    SetPropEffectActive(on: bool, fade: bool),
    TYPE_PROP,
    -1,
    ops::PROP_EFFECTS_ACTIVE
);
set_each!(
    SetPropStaticPhysics(on: bool),
    TYPE_PROP,
    -1,
    ops::PROP_STATIC_PHYSICS
);
set_each!(StopPropMovement(), TYPE_PROP, -1, ops::PROP_STOP_MOVE);
set_each!(
    SetPropHealth(health: f32),
    TYPE_PROP,
    -1,
    ops::PROP_SET_HEALTH
);
set_each!(ResetProp(), TYPE_PROP, -1, ops::PROP_RESET);

set_each!(
    FadeLampTo(r: f32, g: f32, b: f32, a: f32, radius: f32, time: f32),
    TYPE_PROP,
    LAMP,
    ops::LAMP_FADE_TO
);
set_each!(
    SetLampCanBeLitByPlayer(on: bool),
    TYPE_PROP,
    LAMP,
    ops::LAMP_CAN_BE_LIT
);
set_each!(
    SetLampLit(on: bool, effects: bool),
    TYPE_PROP,
    LAMP,
    ops::LAMP_SET_LIT
);

set_each!(
    SetButtonCanBeSwitchedOff(on: bool),
    TYPE_PROP,
    BUTTON,
    ops::BUTTON_CAN_OFF
);
set_each!(
    SetButtonCanBeSwitchedOn(on: bool),
    TYPE_PROP,
    BUTTON,
    ops::BUTTON_CAN_ON
);
set_each!(
    SetButtonSwitchedOn(on: bool, effects: bool),
    TYPE_PROP,
    BUTTON,
    ops::BUTTON_SWITCHED_ON
);

set_each!(
    SetSwingDoorOpenAmount(amount: f32, duration: f32, towards_max: bool),
    TYPE_PROP,
    SWING_DOOR,
    ops::DOOR_OPEN_AMOUNT
);
set_each!(
    SetSwingDoorLocked(locked: bool, effects: bool),
    TYPE_PROP,
    SWING_DOOR,
    ops::DOOR_LOCKED
);
set_each!(
    SetSwingDoorClosed(closed: bool, effects: bool),
    TYPE_PROP,
    SWING_DOOR,
    ops::DOOR_CLOSED
);
set_each!(
    SetSwingDoorDisableAutoClose(off: bool),
    TYPE_PROP,
    SWING_DOOR,
    ops::DOOR_DISABLE_AUTO_CLOSE
);

set_each!(
    SetLevelDoorLocked(locked: bool),
    TYPE_PROP,
    LEVEL_DOOR,
    ops::LEVEL_DOOR_LOCKED
);
set_each!(
    SetLevelDoorLockedSound(sound: S),
    TYPE_PROP,
    LEVEL_DOOR,
    ops::LEVEL_DOOR_SOUND
);
set_each!(
    SetLevelDoorLockedText(cat: S, entry: S),
    TYPE_PROP,
    LEVEL_DOOR,
    ops::LEVEL_DOOR_TEXT
);

set_each!(
    SetPropObjectStuckState(state: i32),
    TYPE_PROP,
    OBJECT,
    ops::OBJECT_STUCK
);
set_each!(
    SetWheelAngle(angle: f32, auto_move: bool),
    TYPE_PROP,
    WHEEL,
    ops::WHEEL_ANGLE
);
set_each!(
    SetWheelStuckState(state: i32, effects: bool),
    TYPE_PROP,
    WHEEL,
    ops::WHEEL_STUCK
);
set_each!(
    SetLeverStuckState(state: i32, effects: bool),
    TYPE_PROP,
    LEVER,
    ops::LEVER_STUCK
);
set_each!(
    SetWheelInteractionDisablesStuck(on: bool),
    TYPE_PROP,
    WHEEL,
    ops::WHEEL_INTERACTION
);
set_each!(
    SetLeverInteractionDisablesStuck(on: bool),
    TYPE_PROP,
    LEVER,
    ops::LEVER_INTERACTION
);
set_each!(
    SetMultiSliderStuckState(state: i32, effects: bool),
    TYPE_PROP,
    MULTI_SLIDER,
    ops::MULTI_SLIDER_STUCK
);
set_each!(
    SetMultiSliderCallback(cb: S),
    TYPE_PROP,
    MULTI_SLIDER,
    ops::MULTI_SLIDER_CB
);

set_each!(
    SetNPCAwake(awake: bool, effects: bool),
    TYPE_PROP,
    NPC,
    ops::NPC_AWAKE
);
set_each!(
    SetNPCFollowPlayer(on: bool),
    TYPE_PROP,
    NPC,
    ops::NPC_FOLLOW
);

set_each!(
    SetMoveObjectState(state: f32),
    TYPE_PROP,
    MOVE_OBJECT,
    ops::MOVE_OBJECT_STATE
);
set_each!(
    SetMoveObjectStateExt(state: f32, acc: f32, max_speed: f32, slowdown: f32, reset: bool),
    TYPE_PROP,
    MOVE_OBJECT,
    ops::MOVE_OBJECT_STATE_EXT
);

fn get_one(name: &[u8], sub: i32, op: ops::Op) -> Option<Arg> {
    entity(name, TYPE_PROP, sub).map(|e| call(op, &[e.a()]))
}

sf!(GetSwingDoorLocked(name: S) -> bool {
    get_one(name.0, SWING_DOOR, ops::DOOR_GET_LOCKED).is_some_and(|a| a.as_b())
});

sf!(GetSwingDoorClosed(name: S) -> bool {
    get_one(name.0, SWING_DOOR, ops::DOOR_GET_CLOSED).is_some_and(|a| a.as_b())
});

sf!(GetSwingDoorState(name: S) -> i32 {
    get_one(name.0, SWING_DOOR, ops::DOOR_GET_STATE).map_or(0, |a| a.as_i())
});

sf!(GetLeverState(name: S) -> i32 {
    get_one(name.0, LEVER, ops::LEVER_GET_STATE).map_or(0, |a| a.as_i())
});

sf!(GetPropIsInteractedWith(name: S) -> bool {
    get_one(name.0, -1, ops::PROP_INTERACTED_WITH).is_some_and(|a| a.as_b())
});

sf!(GetPropHealth(name: S) -> f32 {
    get_one(name.0, -1, ops::PROP_HEALTH).map_or(0.0, |a| a.as_f())
});

sf!(AddPropHealth(name: S, health: f32) {
    each(name.0, TYPE_PROP, -1, |e| {
        let cur = call(ops::PROP_HEALTH, &[e.a()]).as_f();
        call(ops::PROP_SET_HEALTH, &[e.a(), Arg::f(cur + health)]);
    });
});

sf!(SetPropActiveAndFade(name: S, on: bool, fade: f32) {
    each(name.0, TYPE_PROP, -1, |e| {
        call(ops::ENT_ACTIVE, &[e.a(), Arg::b(on)]);
        if on {
            call(ops::PROP_FADE_IN_MESH, &[e.a(), Arg::f(fade)]);
        } else if call(ops::ENT_HAS_MESH, &[e.a()]).as_b() {
            call(ops::PROP_DISSOLVE, &[e.a(), Arg::f(fade)]);
        }
    });
});

sf!(RotatePropToSpeed(
    name: S, acc: f32, goal: f32, ax: f32, ay: f32, az: f32, reset: bool, offset_area: S
) {
    let area = if offset_area.0.is_empty() { None } else { entity(offset_area.0, TYPE_AREA, -1) };
    each(name.0, TYPE_PROP, -1, |e| {
        call(ops::ENT_FULL_SAVE, &[e.a(), Arg::b(true)]);
        let axis = [Arg::f(ax), Arg::f(ay), Arg::f(az)];
        if let Some(a) = area {
            let inv = math::matrix_inverse(&matrix_of(ops::PROP_MAIN_BODY_MATRIX, e.0));
            let apos = super::entity::vec_of(ops::AREA_POSITION, a.0);
            let local = math::matrix_mul_vec(&inv, apos);
            call(ops::PROP_ROTATE, &[
                e.a(), Arg::f(acc), Arg::f(goal), axis[0], axis[1], axis[2], Arg::b(reset), Arg::b(true),
                Arg::f(apos.x), Arg::f(apos.y), Arg::f(apos.z), Arg::f(local.x), Arg::f(local.y), Arg::f(local.z),
            ]);
        } else {
            call(ops::PROP_ROTATE, &[
                e.a(), Arg::f(acc), Arg::f(goal), axis[0], axis[1], axis[2], Arg::b(reset), Arg::b(false),
                Arg::f(0.0), Arg::f(0.0), Arg::f(0.0), Arg::f(0.0), Arg::f(0.0), Arg::f(0.0),
            ]);
        }
    });
});

fn attach_prop(prop: &[u8], attach: &[u8], file: &[u8], pos: [f32; 3], rot: [f32; 3]) {
    let m = transform(pos, rot);
    each(prop, TYPE_PROP, -1, |e| {
        call(
            ops::PROP_ADD_ATTACH,
            &[e.a(), s(attach), s(file), mat_arg(&m)],
        );
    });
}

sf!(AttachPropToProp(
    prop: S, attach: S, file: S, px: f32, py: f32, pz: f32, rx: f32, ry: f32, rz: f32
) {
    attach_prop(prop.0, attach.0, file.0, [px, py, pz], [rx, ry, rz]);
});

// the original forwards the Z rotation as the Z position
sf!(AddAttachedPropToProp(
    prop: S, attach: S, file: S, px: f32, py: f32, pz: f32, rx: f32, ry: f32, rz: f32
) {
    let _ = pz;
    warning("AddAttachedPropToProp is deprectated, use AttachPropToProp instead!\n");
    attach_prop(prop.0, attach.0, file.0, [px, py, rz], [rx, ry, rz]);
});

sf!(RemoveAttachedPropFromProp(prop: S, attach: S) {
    each(prop.0, TYPE_PROP, -1, |e| {
        if !call(ops::PROP_DESTROY_ATTACHED, &[e.a(), s(attach.0)]).as_b() {
            error(&format!(
                "Could not find attached prop '{}' in '{}'\n",
                lossy(prop.0),
                lossy(attach.0)
            ));
        }
    });
});

sf!(AttachAreaToProp(area: S, prop: S, body_idx: i32) {
    let Some(a) = entity(area.0, TYPE_AREA, -1) else {
        error(&format!("WARNING: AttachAreaToProp Area Not Found:'{}'\n", lossy(area.0)));
        return;
    };
    call(ops::AREA_ATTACH_TO_BODY, &[a.a(), s(prop.0), Arg::i(body_idx)]);
});

sf!(AttachPropToBone(
    child: S, parent: S, bone: S, px: f32, py: f32, pz: f32, rx: f32, ry: f32, rz: f32
) {
    let Some(c) = entity(child.0, TYPE_PROP, -1) else {
        error(&format!("Could not find child prop '{}'\n", lossy(child.0)));
        return;
    };
    let Some(p) = entity(parent.0, TYPE_ANY, -1) else {
        error(&format!("Could not find parent entity '{}'\n", lossy(parent.0)));
        return;
    };
    if !call(ops::ENT_HAS_MESH, &[c.a()]).as_b() {
        error(&format!("Child entity '{}' does not have a mesh entity\n", lossy(child.0)));
        return;
    }
    if !call(ops::ENT_HAS_MESH, &[p.a()]).as_b() {
        error(&format!("Parent entity '{}' does not have a mesh entity\n", lossy(parent.0)));
        return;
    }
    let b = call(ops::ENT_BONE, &[p.a(), s(bone.0)]).p;
    if b.is_null() {
        error(&format!(
            "Did not find bone '{}' in parent entity '{}'\n",
            lossy(bone.0),
            lossy(parent.0)
        ));
        return;
    }
    let m = transform([px, py, pz], [rx, ry, rz]);
    call(ops::PROP_SET_PARENT_BONE, &[c.a(), Arg::ptr(b), mat_arg(&m), s(parent.0), s(bone.0)]);
});

sf!(DetachPropFromBone(child: S) {
    let Some(c) = entity(child.0, TYPE_PROP, -1) else {
        error(&format!("Could not find child prop '{}'\n", lossy(child.0)));
        return;
    };
    call(ops::PROP_DETACH_BONE, &[c.a()]);
});

sf!(SetAllowStickyAreaAttachment(on: bool) {
    call(ops::STICKY_ALLOW, &[Arg::b(on)]);
});

sf!(AttachPropToStickyArea(area: S, prop: S) {
    let sticky = entity(area.0, TYPE_AREA, STICKY);
    let p = entity(prop.0, TYPE_PROP, -1);
    let (Some(p), Some(sticky)) = (p, sticky) else { return };
    let main = call(ops::PROP_MAIN_BODY, &[p.a()]).p;
    let b = if main.is_null() { body(p, 0) } else { main };
    call(ops::STICKY_ATTACH_BODY, &[sticky.a(), Arg::ptr(b)]);
});

sf!(AttachBodyToStickyArea(area: S, body_name: S) {
    let Some(sticky) = entity(area.0, TYPE_AREA, STICKY) else { return };
    let b = call(ops::MAP_PHYS_BODY, &[s(body_name.0)]).p;
    if b.is_null() {
        error(&format!("Could not find body '{}'\n", lossy(body_name.0)));
        return;
    }
    call(ops::STICKY_ATTACH_BODY, &[sticky.a(), Arg::ptr(b)]);
});

sf!(DetachFromStickyArea(area: S) {
    if let Some(sticky) = entity(area.0, TYPE_AREA, STICKY) {
        call(ops::STICKY_DETACH, &[sticky.a()]);
    }
});
