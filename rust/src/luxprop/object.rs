//! `cLuxProp_Object`: variables, interaction rules, impact breaking, stuck joints, life length,
//! food and insanity-vision timers.

use super::ops::{Op, Ops, Probe, op};
use super::vars::{Vars, put_var, zero3};
use crate::math::{dot3, fma, len3, normalize3, to_rad, v3};
use crate::types::Vector3f;

pub const SLOT_BREAK_ENTITY: i32 = 10;
pub const SLOT_BREAK_ALIGN_BODY: i32 = 11;
pub const SLOT_BREAK_SOUND: i32 = 12;
pub const SLOT_BREAK_PS: i32 = 13;
pub const SLOT_CONTAINED_ITEM: i32 = 14;

pub const TYPE_STATIC: i32 = 0;
pub const TYPE_GRAB: i32 = 1;
pub const TYPE_PUSH: i32 = 2;
pub const TYPE_SLIDE: i32 = 3;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GrabData {
    pub use_depth: bool,
    pub depth: f32,
    pub depth_inc: f32,
    pub min_depth: f32,
    pub max_depth: f32,
    pub use_offset: bool,
    pub position_offset: Vector3f,
    pub rotation_offset: Vector3f,
    pub throw_impulse: f32,
    pub mass_mul: f32,
    pub force_mul: f32,
    pub torque_mul: f32,
    pub use_rotation: bool,
}

pub(crate) fn load_grab(v: &dyn Vars, use_offset_default: bool, g: &mut GrabData) {
    g.use_depth = v.boolean("GrabUseDepth", false);
    g.depth = v.float("GrabDepth", 1.5);
    g.depth_inc = v.float("GrabDepthInc", 0.1);
    g.min_depth = v.float("GrabMinDepth", 1.0);
    g.max_depth = v.float("GrabMaxDepth", 2.0);
    g.position_offset = v.vec3("GrabPositionOffset", zero3());
    let r = v.vec3("GrabRotationOffset", zero3());
    g.rotation_offset = v3(to_rad(r.x), to_rad(r.y), to_rad(r.z));
    g.throw_impulse = v.float("GrabThrowImpulse", 10.0);
    g.use_offset = v.boolean("GrabUseOffset", use_offset_default);
    g.mass_mul = v.float("GrabMassMul", 0.1);
    g.force_mul = v.float("GrabForceMul", 1.0);
    g.torque_mul = v.float("GrabTorqueMul", 1.0);
    g.use_rotation = v.boolean("GrabUseRotation", true);
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct PushData {
    pub at_point: bool,
    pub force_mul: f32,
    pub impulse: f32,
}

/// Slide data and the rotate-base data of doors, wheels and levers share a shape.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MoveData {
    pub max_speed: f32,
    pub slow_down_factor: f32,
    pub speed_factor: f32,
    pub throw_impulse: f32,
}

pub(crate) fn load_slide(v: &dyn Vars, d: &mut MoveData) {
    d.max_speed = v.float("SlideMaxSpeed", 5.0);
    d.slow_down_factor = v.float("SlideSlowDownFactor", 1.0);
    d.speed_factor = v.float("SlideSpeedFactor", 1.0);
    d.throw_impulse = v.float("SlideThrowImpulse", 3.0);
}

pub(crate) fn load_rotate(v: &dyn Vars, d: &mut MoveData) {
    d.max_speed = v.float("MoveMaxSpeed", 13.5);
    d.slow_down_factor = v.float("MoveSlowDownFactor", 3.0);
    d.speed_factor = v.float("MoveSpeedFactor", 1.0);
    d.throw_impulse = v.float("MoveThrowImpulse", 6.0);
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ObjectParams {
    pub object_type: i32,
    pub bad_sub_type: bool,
    pub life_length: f32,
    pub is_food: bool,
    pub break_active: bool,
    pub break_destroy_joints: bool,
    pub break_min_energy: f32,
    pub break_impulse: f32,
    pub hit_damage_amount: f32,
    pub hit_damage_strength: i32,
    pub min_hit_damage_speed: f32,
    pub grab_skip_non_outer: bool,
    pub grab: GrabData,
    pub push: PushData,
    pub slide: MoveData,
}

/// `cLuxPropLoader_Object::LoadVariables`. `focus` is the prop's `mfMaxFocusDistance`, and the
/// three defaults are the `Player_Interaction` config values.
pub fn load(
    v: &dyn Vars,
    sub_type: &[u8],
    focus: &mut f32,
    defaults: [f32; 3],
    out: &mut ObjectParams,
) {
    let low = crate::string::to_lower(sub_type);
    out.bad_sub_type = false;
    out.object_type = match low.as_slice() {
        b"static" => TYPE_STATIC,
        b"grab" => TYPE_GRAB,
        b"push" => TYPE_PUSH,
        b"slide" => TYPE_SLIDE,
        _ => {
            out.bad_sub_type = true;
            TYPE_GRAB
        }
    };
    out.life_length = v.float("LifeLength", 0.0);
    out.is_food = v.boolean("IsFood", false);
    out.break_active = v.boolean("BreakActive", false);
    out.break_destroy_joints = v.boolean("BreakDestroyJoints", false);
    out.break_min_energy = v.float("BreakMinEnergy", 1000.0);
    put_var(v, SLOT_BREAK_ENTITY, "BreakEntity");
    put_var(v, SLOT_BREAK_ALIGN_BODY, "BreakEntityAlignBody");
    put_var(v, SLOT_BREAK_SOUND, "BreakSound");
    put_var(v, SLOT_BREAK_PS, "BreakParticleSystem");
    out.break_impulse = v.float("BreakImpulse", 3.0);
    out.hit_damage_amount = v.float("HitDamageAmount", 5.0);
    out.hit_damage_strength = v.int("HitDamageStrength", 1);
    out.min_hit_damage_speed = v.float("MinHitDamageSpeed", 3.5);
    match out.object_type {
        TYPE_GRAB => {
            load_grab(v, false, &mut out.grab);
            out.grab_skip_non_outer = v.boolean("GrabSkipNonOuterBodies", false);
            if *focus <= 0.0 {
                *focus = defaults[0];
            }
        }
        TYPE_PUSH => {
            out.push.at_point = v.boolean("PushAtPoint", true);
            out.push.force_mul = v.float("PushForceMul", 1.0);
            out.push.impulse = v.float("PushImpulse", 2.0);
            if *focus <= 0.0 {
                *focus = defaults[1];
            }
        }
        TYPE_SLIDE => {
            load_slide(v, &mut out.slide);
            if *focus <= 0.0 {
                *focus = defaults[2];
            }
        }
        _ => {}
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ObjectInstance {
    pub disable_breakable: bool,
    pub is_insanity_vision: bool,
    pub vision_max_sanity: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ObjectState {
    pub broken: bool,
    pub life_length_count: f32,
    pub stuck_state: i32,
    pub food_attract_count: f32,
    pub insanity_vision_count: f32,
    pub insanity_vision_active: bool,
}

pub fn load_instance(v: &dyn Vars, s: &mut ObjectState, out: &mut ObjectInstance, ops: &mut Ops) {
    out.disable_breakable = v.boolean("DisableBreakable", false);
    out.is_insanity_vision = v.boolean("IsInsanityVision", false);
    out.vision_max_sanity = v.float("VisionMaxSanity", 30.0);
    if out.is_insanity_vision {
        set_insanity_visible(s, false, ops);
    }
    let item = v.string("ContainedItem", b"");
    v.put(
        SLOT_CONTAINED_ITEM,
        if item == b"None" { b"" } else { &item },
    );
}

pub fn set_insanity_visible(s: &mut ObjectState, visible: bool, ops: &mut Ops) {
    s.insanity_vision_active = visible;
    ops.push(Op::new(op::INSANITY_VISIBLE).i(i32::from(visible)));
}

/// `CanInteract`.
pub fn can_interact(
    p: &ObjectParams,
    joint_num: i32,
    mass: f32,
    in_sticky_area: bool,
    has_interact_callback: bool,
) -> bool {
    if p.object_type == TYPE_GRAB && p.grab_skip_non_outer && joint_num >= 2 {
        return false;
    }
    !((mass == 0.0 && !in_sticky_area && !has_interact_callback)
        || (p.object_type == TYPE_STATIC && !has_interact_callback))
}

/// `GetFocusCrosshair`: 0 default, 1 grab, 2 push, 3 the custom one, 4 `LastEnum`.
pub fn focus_crosshair(p: &ObjectParams, can_interact: bool, custom_is_default: bool) -> i32 {
    if !can_interact {
        return 0;
    }
    let base = match p.object_type {
        TYPE_STATIC | TYPE_GRAB | TYPE_SLIDE => 1,
        TYPE_PUSH => 2,
        _ => return 4,
    };
    if custom_is_default { base } else { 3 }
}

pub fn show_outlines_on_connected(p: &ObjectParams) -> bool {
    p.object_type == TYPE_GRAB
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ImpactBody {
    pub mass: f32,
    pub center: Vector3f,
    pub vel_at_impact: Vector3f,
}

/// The energy `OnBodyCollide` sums over the two bodies.
pub fn impact_energy(bodies: &[ImpactBody; 2], point: Vector3f, normal: Vector3f) -> f32 {
    let mut total = 0.0f32;
    for b in bodies {
        if b.mass == 0.0 {
            continue;
        }
        let to_point = v3(
            point.x - b.center.x,
            point.y - b.center.y,
            point.z - b.center.z,
        );
        let n = if dot3(to_point, normal) > 0.0 {
            normal
        } else {
            v3(-normal.x, -normal.y, -normal.z)
        };
        let k = dot3(n, b.vel_at_impact);
        let towards = v3(n.x * k, n.y * k, n.z * k);
        total = fma(len3(towards), b.mass, total);
    }
    total
}

/// A body hit by a character: does it hurt?
pub fn hit_hurts(p: &ObjectParams, speed: f32) -> bool {
    speed >= p.min_hit_damage_speed
}

pub fn on_health_change(p: &ObjectParams, health: f32, ops: &mut Ops) {
    if health <= 0.0 && p.break_active {
        ops.push(Op::new(op::BREAK));
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct UpdateIn {
    pub dt: f32,
    pub has_mesh: bool,
    pub sanity: f32,
    pub interacted: bool,
}

pub fn update(
    s: &mut ObjectState,
    p: &ObjectParams,
    i: &ObjectInstance,
    a: &UpdateIn,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    let dt = a.dt;
    if p.is_food {
        if s.food_attract_count > 0.0 {
            s.food_attract_count -= dt;
        } else {
            s.food_attract_count = 2.0;
            ops.push(Op::new(op::FOOD_ATTRACT));
        }
    }
    update_insanity(s, i, a, probe, ops);
    if p.life_length > 0.0 {
        s.life_length_count += dt;
        let left = p.life_length - s.life_length_count;
        let dist = if left > 0.0 { left } else { 0.0 };
        if dist < 1.0 {
            ops.push(Op::new(op::COVERAGE).f(dist));
        }
        if s.life_length_count >= p.life_length {
            ops.push(Op::new(op::BREAK));
        }
    }
}

fn update_insanity(
    s: &mut ObjectState,
    i: &ObjectInstance,
    a: &UpdateIn,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    if !i.is_insanity_vision || !a.has_mesh {
        return;
    }
    let active = s.insanity_vision_active;
    if (a.sanity <= i.vision_max_sanity && active) || (a.sanity > i.vision_max_sanity && !active) {
        return;
    }
    if s.insanity_vision_count > 0.0 {
        s.insanity_vision_count -= a.dt;
        return;
    }
    s.insanity_vision_count = 2.0;
    let inside = a.interacted || probe.insanity_in_fov();
    if !inside {
        set_insanity_visible(s, !active, ops);
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct JointData {
    pub max_limit: f32,
    pub min_limit: f32,
    pub locked_range: f32,
    /// 0 hinge, 1 slider, 2 screw, -1 anything else.
    pub kind: i32,
}

pub fn joint_data(kind: i32, min: f32, max: f32) -> JointData {
    let locked = match kind {
        0 => to_rad(5.0),
        1 | 2 => to_rad(0.1f32),
        _ => 0.0,
    };
    JointData {
        max_limit: max,
        min_limit: min,
        locked_range: locked,
        kind: if (0..=2).contains(&kind) { kind } else { -1 },
    }
}

fn set_joint_min_max(i: usize, d: &JointData, min: f32, max: f32, ops: &mut Ops) {
    // C++ sets a hinge's max angle to `min` as well
    match d.kind {
        0 => ops.push(Op::new(op::SET_JOINT_MINMAX).i(i as i32).f2(min, min)),
        1 | 2 => ops.push(Op::new(op::SET_JOINT_MINMAX).i(i as i32).f2(min, max)),
        _ => {}
    }
}

pub fn set_stuck_state(s: &mut ObjectState, state: i32, joints: &[JointData], ops: &mut Ops) {
    if s.stuck_state == state {
        return;
    }
    s.stuck_state = state;
    for (i, d) in joints.iter().enumerate() {
        match state {
            0 => set_joint_min_max(i, d, d.min_limit, d.max_limit, ops),
            1 => set_joint_min_max(i, d, d.max_limit - d.locked_range, d.max_limit, ops),
            -1 => set_joint_min_max(i, d, d.min_limit, d.min_limit + d.locked_range, ops),
            _ => {}
        }
        ops.push(Op::new(op::JOINT_WAKE).i(i as i32));
    }
}

/// `BeforePropDestruction`: the impulse a spawned or freed body gets,
/// `Normalize(center - origin) * impulse (+ base velocity)`.
pub fn break_impulse(
    center: Vector3f,
    origin: Vector3f,
    impulse: f32,
    add: Option<Vector3f>,
) -> Vector3f {
    let mut dir = v3(
        center.x - origin.x,
        center.y - origin.y,
        center.z - origin.z,
    );
    normalize3(&mut dir);
    match add {
        Some(a) => v3(
            fma(dir.x, impulse, a.x),
            fma(dir.y, impulse, a.y),
            fma(dir.z, impulse, a.z),
        ),
        None => v3(dir.x * impulse, dir.y * impulse, dir.z * impulse),
    }
}

/// The mean of the body positions, as `BeforePropDestruction` sums and divides them.
pub fn bodies_center(pos: &[Vector3f]) -> Vector3f {
    let mut c = zero3();
    for p in pos {
        c = v3(c.x + p.x, c.y + p.y, c.z + p.z);
    }
    let n = pos.len() as f32;
    v3(c.x / n, c.y / n, c.z / n)
}
