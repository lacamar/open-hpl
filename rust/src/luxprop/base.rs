//! `iLuxProp`: base variables, damage, effect fading, the scripted static move and the
//! "ignore the player until outside" check.

use super::ops::{Op, Ops, Pod, Probe, op};
use super::vars::{Vars, put_var, zero3};
use crate::math::{
    IDENTITY, dot3, fma, len3, matrix_mul, matrix_mul_vec, matrix_rotate, normalize3, v3,
};
use crate::types::{Color, Matrixf, Vector3f};

unsafe extern "C" {
    safe fn ohpl_math_matrix_euler_angle_distance(a: &Matrixf, b: &Matrixf, out: &mut Vector3f);
    safe fn ohpl_math_matrix_slerp(t: f32, a: &Matrixf, b: &Matrixf, out: &mut Matrixf);
}

pub(crate) fn euler_angle_distance(a: &Matrixf, b: &Matrixf) -> Vector3f {
    let mut o = zero3();
    ohpl_math_matrix_euler_angle_distance(a, b, &mut o);
    o
}

pub(crate) fn matrix_slerp(t: f32, a: &Matrixf, b: &Matrixf) -> Matrixf {
    let mut o = IDENTITY;
    ohpl_math_matrix_slerp(t, a, b, &mut o);
    o
}

pub const SLOT_MAIN_BODY: i32 = 0;
pub const SLOT_EFFECTS_ON_SOUND: i32 = 1;
pub const SLOT_EFFECTS_OFF_SOUND: i32 = 2;
pub const SLOT_MOVE_START_SOUND: i32 = 3;
pub const SLOT_MOVE_STOP_SOUND: i32 = 4;
pub const SLOT_MOVE_LOOP_SOUND: i32 = 5;
pub const SLOT_CALLBACK_FUNC: i32 = 20;
pub const SLOT_CONN_CHANGE: i32 = 21;
pub const SLOT_LOOK_AT: i32 = 22;
pub const SLOT_INTERACT: i32 = 23;
pub const SLOT_CONNECTED_PROP: i32 = 24;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BaseParams {
    pub health: f32,
    pub toughness: i32,
    pub max_focus_distance: f32,
    pub dissolve_on_destruction: bool,
    pub dissolve_time: f32,
    pub show_mesh: bool,
    pub show_hints: bool,
    pub effects_on_time: f32,
    pub effects_off_time: f32,
    pub effects_off_light_color: Color,
    pub effects_off_light_radius: f32,
    pub move_check_collision: bool,
    pub randomize_anim_start: bool,
}

/// `iLuxPropLoader::AfterLoad` base properties.
pub fn load_base(v: &dyn Vars, out: &mut BaseParams) {
    out.health = v.float("Health", 100.0);
    out.toughness = v.int("Toughness", 0);
    out.max_focus_distance = v.float("MaxFocusDistance", 0.0);
    put_var(v, SLOT_MAIN_BODY, "MainPhysicsBody");
    out.dissolve_on_destruction = v.boolean("DissolveOnDestruction", false);
    out.dissolve_time = v.float("DissolveTime", 1.0);
    out.show_mesh = v.boolean("ShowMesh", true);
    out.show_hints = v.boolean("ShowHints", true);
    put_var(v, SLOT_EFFECTS_ON_SOUND, "EffectsOnSound");
    put_var(v, SLOT_EFFECTS_OFF_SOUND, "EffectsOffSound");
    out.effects_on_time = v.float("EffectsOnTime", 1.0);
    out.effects_off_time = v.float("EffectsOffTime", 1.0);
    out.effects_off_light_color = v.color(
        "EffectsOffLightColor",
        Color {
            r: 0.0,
            g: 0.0,
            b: 0.0,
            a: 0.0,
        },
    );
    out.effects_off_light_radius = v.float("EffectsOffLightRadius", 1.0);
    out.move_check_collision = v.boolean("StaticMoveCheckCollision", true);
    put_var(v, SLOT_MOVE_START_SOUND, "StaticMoveStartSound");
    put_var(v, SLOT_MOVE_STOP_SOUND, "StaticMoveStopSound");
    put_var(v, SLOT_MOVE_LOOP_SOUND, "StaticMoveLoopSound");
    out.randomize_anim_start = v.boolean("RandomizeAnimationStart", true);
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BaseInstance {
    pub cast_shadows: bool,
    pub full_game_save: bool,
    pub look_at_auto_remove: bool,
    pub interact_auto_remove: bool,
    pub static_physics: bool,
}

/// The instance variables of `AfterLoad`, in the order C++ read them.
pub fn load_base_instance(inst: &dyn Vars, force_full_game_save: bool, out: &mut BaseInstance) {
    out.cast_shadows = inst.boolean("CastShadows", true);
    let connected = inst.string("ConnectedProps", b"");
    for tok in crate::string::split(&connected, None, true) {
        inst.put(SLOT_CONNECTED_PROP, tok);
    }
    put_var(inst, SLOT_CALLBACK_FUNC, "CallbackFunc");
    put_var(inst, SLOT_CONN_CHANGE, "ConnectionStateChangeCallback");
    out.full_game_save = if force_full_game_save {
        true
    } else {
        inst.boolean("FullGameSave", false)
    };
    put_var(inst, SLOT_LOOK_AT, "PlayerLookAtCallback");
    out.look_at_auto_remove = inst.boolean("PlayerLookAtCallbackAutoRemove", false);
    put_var(inst, SLOT_INTERACT, "PlayerInteractCallback");
    out.interact_auto_remove = inst.boolean("PlayerInteractCallbackAutoRemove", false);
    out.static_physics = inst.boolean("StaticPhysics", false);
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LightData {
    pub color: Color,
    pub radius: f32,
    pub flickering: bool,
}

const _: () = assert!(std::mem::size_of::<LightData>() == 24);

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Linear {
    pub max_speed: f32,
    pub acc: f32,
    pub speed: f32,
    pub slowdown: f32,
    pub goal: Vector3f,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Angular {
    pub max_speed: f32,
    pub acc: f32,
    pub speed: f32,
    pub slowdown: f32,
    pub goal: Matrixf,
    pub no_goal: bool,
    pub no_goal_dir: Vector3f,
    pub use_offset: bool,
    pub world_offset: Vector3f,
    pub local_offset: Vector3f,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BaseState {
    pub effects_active: bool,
    pub effects_fading: bool,
    pub effects_alpha: f32,
    pub check_outside_player: bool,
    pub moving: bool,
    pub moving_linear: bool,
    pub moving_angular: bool,
    pub moving_volume: f32,
    pub move_start_count: f32,
    pub linear: Linear,
    pub angular: Angular,
    pub last_body_matrix: Matrixf,
    pub fade_in_alpha: f32,
    pub fade_in_speed: f32,
}

impl BaseState {
    pub fn new() -> Self {
        Self {
            effects_active: true,
            effects_fading: true,
            effects_alpha: 1.0,
            fade_in_alpha: 1.0,
            last_body_matrix: IDENTITY,
            ..Self::default()
        }
    }
}

pub(crate) fn translation(m: &Matrixf) -> Vector3f {
    v3(m.m[0][3], m.m[1][3], m.m[2][3])
}

pub(crate) fn set_translation(m: &mut Matrixf, t: Vector3f) {
    m.m[0][3] = t.x;
    m.m[1][3] = t.y;
    m.m[2][3] = t.z;
}

pub(crate) fn rotation(m: &Matrixf) -> Matrixf {
    let mut r = *m;
    r.m[0][3] = 0.0;
    r.m[1][3] = 0.0;
    r.m[2][3] = 0.0;
    r.m[3] = [0.0, 0.0, 0.0, 1.0];
    r
}

pub(crate) fn sqr_len(v: Vector3f) -> f32 {
    dot3(v, v)
}

/// `cMath::Min` (float overload): the second operand wins on NaN.
pub(crate) fn cmin(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

pub(crate) fn cmax(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

fn is_zero(v: Vector3f) -> bool {
    v.x == 0.0 && v.y == 0.0 && v.z == 0.0
}

/// `GiveDamage`: the damage that applies and the resulting health.
pub fn give_damage(health: f32, toughness: i32, amount: f32, strength: i32) -> (f32, f32) {
    let mut amount = amount;
    if strength < toughness.wrapping_sub(1) {
        amount = 0.0;
    } else if strength == toughness.wrapping_sub(1) {
        amount *= 0.5;
    }
    (health - amount, amount)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EffectsIn {
    pub active: bool,
    pub fade: bool,
    pub n_ps: i32,
    pub n_sounds: i32,
    pub on_time: f32,
    pub off_time: f32,
    pub off_radius: f32,
    pub on_sound: bool,
    pub off_sound: bool,
}

pub fn effects_set_active(s: &mut BaseState, a: &EffectsIn, lights: &[LightData], ops: &mut Ops) {
    if a.active == s.effects_active {
        return;
    }
    s.effects_active = a.active;
    s.effects_fading = true;
    if a.active {
        if a.n_ps > 0 {
            ops.push(Op::new(op::FX_PS_CREATE_ALL));
        }
        for i in 0..lights.len() {
            let i = i as i32;
            if a.fade {
                ops.push(Op::new(op::FX_LIGHT_FADE_ON).i(i).f(a.on_time));
            } else {
                ops.push(Op::new(op::FX_LIGHT_SET_ON).i(i));
            }
        }
        for i in 0..a.n_sounds {
            if a.fade {
                ops.push(Op::new(op::FX_SOUND_FADE_IN).i(i).f(1.0f32 / a.on_time));
            } else {
                ops.push(Op::new(op::FX_SOUND_PLAY).i(i));
            }
        }
        if !a.fade {
            s.effects_alpha = 1.0;
        }
    } else {
        if a.n_ps > 0 {
            ops.push(Op::new(op::FX_PS_KILL_ALL).j(i32::from(a.fade)));
        }
        for (i, l) in lights.iter().enumerate() {
            let r = if a.off_radius >= 0.0 {
                a.off_radius
            } else {
                l.radius
            };
            ops.push(
                Op::new(op::FX_LIGHT_OFF)
                    .i(i as i32)
                    .j(i32::from(a.fade))
                    .f2(r, a.off_time),
            );
        }
        for i in 0..a.n_sounds {
            if a.fade {
                ops.push(Op::new(op::FX_SOUND_FADE_OUT).i(i).f(1.0f32 / a.off_time));
            } else {
                ops.push(Op::new(op::FX_SOUND_STOP).i(i));
            }
        }
        if !a.fade {
            s.effects_alpha = 0.0;
        }
    }
    if a.fade && (if a.active { a.on_sound } else { a.off_sound }) {
        ops.push(Op::new(op::FX_CHANGE_SOUND).i(i32::from(a.active)));
    }
}

pub fn effects_update(
    s: &mut BaseState,
    dt: f32,
    on_time: f32,
    off_time: f32,
    lights: &[LightData],
    has_mesh: bool,
    ops: &mut Ops,
) {
    if !s.effects_fading {
        return;
    }
    if s.effects_active {
        s.effects_alpha = fma(1.0f32 / on_time, dt, s.effects_alpha);
        if s.effects_alpha > 1.0 {
            s.effects_alpha = 1.0;
            for (i, _) in lights.iter().enumerate() {
                ops.push(Op::new(op::FX_LIGHT_FLICKER).i(i as i32));
            }
            s.effects_fading = false;
        }
    } else {
        s.effects_alpha = fma(-(1.0f32 / off_time), dt, s.effects_alpha);
        if s.effects_alpha < 0.0 {
            s.effects_alpha = 0.0;
            s.effects_fading = false;
        }
    }
    ops.push(Op::new(op::FX_BILLBOARDS).f(s.effects_alpha));
    if has_mesh {
        ops.push(Op::new(op::FX_MESH_ILLUM).f(s.effects_alpha));
    }
}

pub fn fade_in_mesh(s: &mut BaseState, time: f32, has_mesh: bool, ops: &mut Ops) {
    s.fade_in_alpha = 0.0;
    s.fade_in_speed = 1.0f32 / time;
    if has_mesh {
        ops.push(Op::new(op::COVERAGE).f(0.0));
    }
}

pub fn mesh_fading(s: &mut BaseState, dt: f32, has_mesh: bool, ops: &mut Ops) {
    if s.fade_in_alpha >= 1.0 || !has_mesh {
        return;
    }
    s.fade_in_alpha = fma(s.fade_in_speed, dt, s.fade_in_alpha);
    ops.push(Op::new(op::COVERAGE).f(s.fade_in_alpha));
}

/// `UpdateAnimation`: run the stored callback when the non-looping animation finished.
pub fn anim_update(index: Pod<i32>, over: bool, has_callback: bool, ops: &mut Ops) {
    if *index.m() < 0 {
        return;
    }
    if over {
        *index.m() = -1;
        if has_callback {
            ops.event(Op::new(op::RUN_ANIM_CALLBACK));
        }
    }
}

/// `InteractConnectionTurn`: the turn fraction handed to every connection.
pub fn connection_turn(angle: f32, min: f32, max: f32) -> f32 {
    (angle - min) / (max - min)
}

impl BaseState {
    pub fn reset_move(&mut self) {
        self.moving = false;
        self.moving_angular = false;
        self.moving_linear = false;
        self.angular.speed = 0.0;
    }

    fn start_sound(&mut self, ops: &mut Ops) {
        if !self.moving && self.move_start_count <= 0.0 {
            ops.push(Op::new(op::SND_MOVE_START));
        }
        self.move_start_count = 1.0;
        self.moving = true;
    }

    pub fn move_linear_to(
        &mut self,
        goal: Vector3f,
        acc: f32,
        max_speed: f32,
        slowdown: f32,
        reset: bool,
        ops: &mut Ops,
    ) {
        self.start_sound(ops);
        self.moving_linear = true;
        self.linear.goal = goal;
        self.linear.acc = acc;
        self.linear.max_speed = max_speed;
        self.linear.slowdown = slowdown;
        if reset {
            self.linear.speed = 0.0;
        }
        ops.push(Op::new(op::ON_START_MOVE));
    }

    #[expect(clippy::too_many_arguments, reason = "mirrors MoveAngularTo")]
    pub fn move_angular_to(
        &mut self,
        goal: &Matrixf,
        acc: f32,
        max_speed: f32,
        slowdown: f32,
        reset: bool,
        use_offset: bool,
        world_offset: Vector3f,
        local_offset: Vector3f,
        ops: &mut Ops,
    ) {
        self.start_sound(ops);
        self.moving_angular = true;
        self.angular.goal = *goal;
        self.angular.acc = acc;
        self.angular.max_speed = max_speed;
        self.angular.slowdown = slowdown;
        if reset {
            self.angular.speed = 0.0;
        }
        self.angular.use_offset = use_offset;
        self.angular.world_offset = world_offset;
        self.angular.local_offset = local_offset;
        self.angular.no_goal = false;
        ops.push(Op::new(op::ON_START_MOVE));
    }

    #[expect(clippy::too_many_arguments, reason = "mirrors RotateAtSpeed")]
    pub fn rotate_at_speed(
        &mut self,
        acc: f32,
        goal_speed: f32,
        axis: Vector3f,
        reset: bool,
        use_offset: bool,
        world_offset: Vector3f,
        local_offset: Vector3f,
        ops: &mut Ops,
    ) {
        self.start_sound(ops);
        self.moving_angular = true;
        self.angular.acc = acc;
        self.angular.max_speed = goal_speed;
        if reset {
            self.angular.speed = 0.0;
        }
        self.angular.no_goal = true;
        let mut dir = axis;
        normalize3(&mut dir);
        self.angular.no_goal_dir = dir;
        self.angular.use_offset = use_offset;
        self.angular.world_offset = world_offset;
        self.angular.local_offset = local_offset;
        ops.push(Op::new(op::ON_START_MOVE));
    }

    pub fn stop_move(&mut self, ops: &mut Ops) {
        self.moving_linear = false;
        self.moving_angular = false;
        self.moving = false;
        if self.move_start_count <= 0.0 {
            ops.push(Op::new(op::SND_MOVE_STOP));
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MoveIn {
    pub dt: f32,
    pub body: Matrixf,
    pub check_collision: bool,
    pub loop_present: bool,
    pub loop_alive: bool,
    pub loop_sound: bool,
}

/// `UpdateMoving`.
pub fn update_moving(s: &mut BaseState, a: &MoveIn, probe: &mut dyn Probe, ops: &mut Ops) {
    let dt = a.dt;
    if s.move_start_count > 0.0 {
        s.move_start_count -= dt;
    }
    if !s.moving {
        if s.moving_volume > 0.0 {
            s.moving_volume -= dt;
            if s.moving_volume < 0.0 {
                s.moving_volume = 0.0;
                if a.loop_present {
                    ops.push(Op::new(op::MOVE_LOOP_KILL));
                }
            } else {
                ops.push(Op::new(op::MOVE_LOOP_VOLUME).f(s.moving_volume));
            }
        }
        return;
    }

    let body_pos = translation(&a.body);
    let mut has_speed = false;
    if s.moving_linear {
        let min = 0.001f32 * (1.0f32 / 60.0f32);
        let last = translation(&s.last_body_matrix);
        let (dx, dy, dz) = (
            body_pos.x - last.x,
            body_pos.y - last.y,
            body_pos.z - last.z,
        );
        let sqr = fma(dz, dz, fma(dx, dx, dy * dy));
        if sqr > min * min {
            has_speed = true;
        }
    }
    if !has_speed && s.moving_angular {
        let min = 0.001f32 * (1.0f32 / 60.0f32);
        let vel = euler_angle_distance(&rotation(&s.last_body_matrix), &rotation(&a.body));
        if sqr_len(vel) > min * min {
            has_speed = true;
        }
    }

    if has_speed && s.moving_volume < 1.0 {
        if !a.loop_present && a.loop_sound {
            ops.push(Op::new(op::MOVE_LOOP_START));
        }
        s.moving_volume = fma(dt, 1.2f32, s.moving_volume);
        if s.moving_volume > 1.0 {
            s.moving_volume = 1.0;
        }
        ops.push(Op::new(op::MOVE_LOOP_VOLUME).f(s.moving_volume));
    } else if !has_speed && s.moving_volume > 0.0 {
        s.moving_volume -= dt;
        if s.moving_volume < 0.0 {
            s.moving_volume = 0.0;
            if a.loop_present && a.loop_alive {
                ops.push(Op::new(op::MOVE_LOOP_KILL).j(1));
            }
        } else {
            ops.push(Op::new(op::MOVE_LOOP_VOLUME).f(s.moving_volume));
        }
    }

    s.last_body_matrix = a.body;
    update_linear(s, a, probe, ops);
    update_angular(s, a, ops);
    if !s.moving_linear && !s.moving_angular {
        s.stop_move(ops);
    }
}

fn update_linear(s: &mut BaseState, a: &MoveIn, probe: &mut dyn Probe, ops: &mut Ops) {
    if !s.moving_linear {
        return;
    }
    let dt = a.dt;
    let pos = translation(&a.body);
    let l = &mut s.linear;
    let delta = v3(l.goal.x - pos.x, l.goal.y - pos.y, l.goal.z - pos.z);
    let dist = len3(delta);
    let mut vel;
    if dist <= l.speed * dt * 1.05f32 || f64::from(dist) < 0.01 {
        l.speed = 0.0;
        vel = v3(delta.x / dt, delta.y / dt, delta.z / dt);
        s.moving_linear = false;
    } else {
        let dir = v3(delta.x / dist, delta.y / dist, delta.z / dist);
        if dist < l.slowdown {
            l.speed = (dist / l.slowdown) * l.max_speed;
        } else {
            l.speed = fma(l.acc, dt, l.speed);
            if l.speed > l.max_speed {
                l.speed = l.max_speed;
            }
        }
        vel = v3(dir.x * l.speed, dir.y * l.speed, dir.z * l.speed);
    }
    if a.check_collision && !is_zero(vel) {
        let add = v3(vel.x * dt, vel.y * dt, vel.z * dt);
        let push = probe.move_push(add);
        if !is_zero(push) {
            let add_len_sqr = sqr_len(add);
            let neg = v3(-add.x, -add.y, -add.z);
            let mut amount = 1.0f32 - dot3(push, neg) / add_len_sqr;
            if amount < 0.0 {
                amount = 0.0;
            }
            s.linear.speed *= amount;
            vel = v3(vel.x * amount, vel.y * amount, vel.z * amount);
        }
    }
    if !is_zero(vel) {
        ops.push(Op::new(op::STATIC_LINEAR_MOVE).v(vel));
    }
}

fn update_angular(s: &mut BaseState, a: &MoveIn, ops: &mut Ops) {
    if !s.moving_angular {
        return;
    }
    let dt = a.dt;
    let an = &mut s.angular;
    let vel;
    if an.no_goal {
        let acc = if an.max_speed < an.speed {
            -an.acc
        } else {
            an.acc
        };
        an.speed = fma(acc, dt, an.speed);
        if (acc < 0.0 && an.speed < an.max_speed) || (acc > 0.0 && an.speed > an.max_speed) {
            an.speed = an.max_speed;
            if an.speed == 0.0 {
                s.moving_angular = false;
            }
        }
        let d = an.no_goal_dir;
        vel = v3(d.x * an.speed, d.y * an.speed, d.z * an.speed);
    } else {
        let delta = euler_angle_distance(&a.body, &an.goal);
        let dist = len3(delta);
        if dist <= an.speed * dt * 1.05f32 || f64::from(dist) < 0.01 {
            an.speed = 0.0;
            vel = v3(delta.x / dt, delta.y / dt, delta.z / dt);
            s.moving_angular = false;
        } else {
            let dir = v3(delta.x / dist, delta.y / dist, delta.z / dist);
            if dist < an.slowdown {
                an.speed = (dist / an.slowdown) * an.max_speed;
            } else {
                an.speed = fma(an.acc, dt, an.speed);
                if an.speed > an.max_speed {
                    an.speed = an.max_speed;
                }
            }
            vel = v3(dir.x * an.speed, dir.y * an.speed, dir.z * an.speed);
        }
    }
    ops.push(Op::new(op::STATIC_ANGULAR_MOVE).v(vel));
    if s.angular.use_offset {
        let rot = matrix_rotate(v3(vel.x * dt, vel.y * dt, vel.z * dt), 0);
        let mut m = matrix_mul(&rot, &rotation(&a.body));
        set_translation(&mut m, translation(&a.body));
        let off = matrix_mul_vec(&m, s.angular.local_offset);
        let w = s.angular.world_offset;
        let diff = v3(w.x - off.x, w.y - off.y, w.z - off.z);
        ops.push(Op::new(op::STATIC_LINEAR_MOVE).v(v3(diff.x / dt, diff.y / dt, diff.z / dt)));
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct OutsideBody {
    pub collide_character: bool,
    pub default_collide_character: bool,
    pub pos: Vector3f,
    pub vel: Vector3f,
}

/// `UpdateCheckIfOutsidePlayer`; returns whether the check is over.
pub fn outside_player(
    bodies: &[OutsideBody],
    player_pos: Vector3f,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) -> bool {
    let mut not_colliding = true;
    for (i, b) in bodies.iter().enumerate() {
        if b.collide_character || !b.default_collide_character {
            continue;
        }
        if probe.outside_collides(i as i32) {
            not_colliding = false;
            let mut dir = v3(
                b.pos.x - player_pos.x,
                b.pos.y - player_pos.y,
                b.pos.z - player_pos.z,
            );
            dir.y *= 0.1f32;
            normalize3(&mut dir);
            let k = dot3(b.vel, dir);
            let rel = v3(dir.x * k, dir.y * k, dir.z * k);
            if len3(rel) < 0.5f32 {
                ops.push(Op::new(op::OUTSIDE_IMPULSE).i(i as i32).v(v3(
                    dir.x * 0.5f32,
                    dir.y * 0.5f32,
                    dir.z * 0.5f32,
                )));
            }
        } else {
            ops.push(Op::new(op::OUTSIDE_COLLIDE_CHAR).i(i as i32));
        }
    }
    not_colliding
}
