//! `iLuxEnemy` update, animation, sound, glow, damage and distance logic behind `OHPL_IMPL=rust`.

use super::a4_state::{
    Base, Ga3Via, H, Io, Kind, OP_ADD_GLOW, OP_AMBIENT_EMPTY, OP_ANIM_EVENT_TIME, OP_ANIM_FADE_IN,
    OP_ANIM_FADE_OUT, OP_ANIM_FIND, OP_ANIM_IS_ACTIVE, OP_ANIM_IS_FADING, OP_ANIM_IS_LOOPING,
    OP_ANIM_IS_OVER, OP_ANIM_LENGTH, OP_ANIM_NAME_EQ, OP_ANIM_PREV_TIME, OP_ANIM_REL_TIME,
    OP_ANIM_SET_ACTIVE, OP_ANIM_SET_LOOP, OP_ANIM_SET_REL_TIME, OP_ANIM_SET_SPEED,
    OP_ANIM_SET_TIME, OP_ANIM_SET_WEIGHT, OP_ANIM_TIME, OP_ATTACK_HIT_SOUND, OP_ATTACK_SHAPE,
    OP_BODY_EXISTS, OP_BODY_RIGHT_UP_FWD, OP_BODY_SET_FEET, OP_CALLBACK_DEATH, OP_CAM_COLLIDE_MESH,
    OP_CAM_POS, OP_DAMAGE_MUL, OP_DEBUG_DAMAGE, OP_FADE_TO_SMOKE, OP_FOOD_BEGIN, OP_FOOD_IS_FOOD,
    OP_FOOD_IS_OBJECT, OP_FOOD_LOWEST_Y, OP_FOOD_POS, OP_FRUSTUM_BODY, OP_FRUSTUM_BODY_AT,
    OP_GROUND_RAY, OP_LIGHT_LEVEL_AT, OP_MESH_EXISTS, OP_MESH_SET_ACTIVE, OP_MESH_VISIBLE,
    OP_ON_DAMAGE, OP_ON_RESET_PROPERTIES, OP_ON_SET_ACTIVE_SPECIFIC, OP_PLAYER_SANITY,
    OP_SET_ENTITY_OFFSET, OP_SOUND_CREATE, OP_SOUND_EXISTS, OP_SOUND_FADE_OUT, OP_SOUND_ID,
    OP_STEP_SIZE, OP_SUBMESH_NUM, OP_UPDATE_STEP, OP_WARN_NO_ANIM,
};
use super::ctx::{Ctx, Env, g, gv, s, sv};
use super::machine::{MSG_ANIMATION_OVER, MSG_ANIMATION_SPECIAL_EVENT, MSG_TAKE_HIT};
use super::{dist_sqr, dist3, normalized, rot_y, z3};
use crate::math::{Matrixf, Vector3f, fma, len3, matrix_mul_vec, matrix_rotate, rand_rectf, v3};
use std::ffi::c_void;

pub const STATE_IDLE: i32 = 0;
pub const STATE_PATROL: i32 = 3;
pub const STATE_SEARCH: i32 = 6;
pub const STATE_DEAD: i32 = 16;
const SOUND_SILENT: i32 = 0;
const MUSIC_SEARCH: i32 = 0;
const MUSIC_ATTACK: i32 = 1;
const MOVE_SPEED_WALK: i32 = 0;

const STEP_PF: i32 = 0;
const STEP_MOVER: i32 = 1;
const STEP_CAN_SEE: i32 = 2;
const STEP_DETECTED: i32 = 3;
const STEP_IN_RANGE: i32 = 4;
const STEP_STUCK_DOOR: i32 = 5;
const STEP_LAST_POS: i32 = 6;
const STEP_STATE_MACHINE: i32 = 7;
const STEP_SPECIFIC: i32 = 8;
const STEP_CHAR_BODY: i32 = 9;
const STEP_REGEN: i32 = 10;

fn ctx<'a>(b: &Base) -> &'a Ctx {
    // SAFETY: C++ sets `c` to the enemy's `OhplGa3Ctx` before any entry is called.
    unsafe { &*b.c }
}

fn mover<'a>(b: &Base) -> &'a mut super::mover::MoverState {
    // SAFETY: `mover` points at the enemy's mover state, set up in `SetupAfterLoad` before updates.
    unsafe { &mut *ctx(b).mover }
}

fn step(h: &H, which: i32, dt: f32) {
    let mut io = Io::new();
    io.i[0] = which;
    io.f[0] = dt;
    h.call(OP_UPDATE_STEP, &mut io);
}

fn send(b: &Base, h: &H, ty: i32, v: Vector3f, f: f32, i: i32) {
    let mut via = Ga3Via(h);
    Env::new(ctx(b), &mut via).send(ty, 0.0, false, v, f, i);
}

pub fn on_update(b: &mut Base, h: &H, dt: f32) {
    let c = ctx(b);
    if g(c.disabled) {
        return;
    }
    if dt < h.qf(OP_STEP_SIZE) * 0.8 {
        return;
    }
    step(h, STEP_PF, dt);
    step(h, STEP_MOVER, dt);
    if g(c.health) > 0.0 {
        for w in [
            STEP_CAN_SEE,
            STEP_DETECTED,
            STEP_IN_RANGE,
            STEP_STUCK_DOOR,
            STEP_LAST_POS,
        ] {
            step(h, w, dt);
        }
    } else {
        s(c.can_see, false);
        s(c.detected, false);
        s(c.in_range, false);
    }
    step(h, STEP_STATE_MACHINE, dt);
    step(h, STEP_SPECIFIC, dt);
    step(h, STEP_CHAR_BODY, dt);
    update_animation(b, h, dt);
    update_sound_state(b, h, dt);
    update_darkness_glow(b, h, dt);
    step(h, STEP_REGEN, dt);
    update_hallucination(b, h);
    update_align_ground_ray(b, h, dt);
}

pub fn give_damage(b: &mut Base, h: &H, amount: f32, strength: i32) {
    let c = ctx(b);
    let mut amount = amount;
    if g(c.health) > 0.0 {
        if strength < b.toughness - 1 {
            amount = 0.0;
        } else if strength == b.toughness - 1 {
            amount *= 0.5;
        }
    }
    let mut io = Io::new();
    io.f[0] = amount;
    io.i[0] = strength;
    h.call(OP_DAMAGE_MUL, &mut io);
    amount *= io.f[0];
    s(c.health, g(c.health) - amount);
    let mut io = Io::new();
    io.f[0] = amount;
    h.call(OP_DEBUG_DAMAGE, &mut io);
    if g(c.health) <= 0.0 {
        change_sound_state(b, h, SOUND_SILENT);
        change_state(b, STATE_DEAD);
        h.q(OP_CALLBACK_DEATH);
    } else {
        send(b, h, MSG_TAKE_HIT, z3(), amount, strength);
        let mut io = Io::new();
        io.f[0] = amount;
        io.i[0] = strength;
        h.call(OP_ON_DAMAGE, &mut io);
    }
}

pub fn show_player_position(b: &mut Base, h: &H) {
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    sv(ctx(b).last_known, host.player_feet());
}

pub fn alert_of_player_presence(b: &mut Base, h: &H) {
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    if host.triggers_disabled() {
        return;
    }
    show_player_position(b, h);
    let cur = g(ctx(b).cur);
    if cur == STATE_IDLE || cur == STATE_PATROL {
        change_state(b, STATE_SEARCH);
    }
}

pub fn change_state(b: &Base, state: i32) {
    let c = ctx(b);
    super::machine::StatePtrs {
        cur: c.cur,
        next: c.next,
        prev: c.prev,
    }
    .change_state(state);
}

#[expect(clippy::too_many_arguments, reason = "mirrors iLuxEnemy::PlayAnimImpl")]
pub fn play_anim_impl(
    b: &mut Base,
    h: &H,
    name: *mut c_void,
    looped: bool,
    fade: f32,
    depends_on_speed: bool,
    speed_mul: f32,
    sync: bool,
    use_move_when_over: bool,
) -> bool {
    if !b.use_animations {
        return false;
    }
    let cur = b.cur_anim;
    if !cur.is_null() {
        let mut io = Io::new();
        io.p[0] = cur;
        io.p[1] = name;
        if h.call(OP_ANIM_NAME_EQ, &mut io) != 0
            && h.qp(OP_ANIM_IS_ACTIVE, cur) != 0
            && h.qp(OP_ANIM_IS_OVER, cur) == 0
        {
            return false;
        }
    }
    let mut io = Io::new();
    io.p[0] = name;
    h.call(OP_ANIM_FIND, &mut io);
    let new = io.p[1];
    if new.is_null() {
        h.qp(OP_WARN_NO_ANIM, name);
        return false;
    }
    h.pi(OP_ANIM_SET_ACTIVE, new, 1);
    if !cur.is_null() && cur != new {
        h.pf(OP_ANIM_FADE_OUT, cur, fade);
        if h.qp(OP_ANIM_IS_FADING, new) == 0 {
            h.pf(OP_ANIM_SET_WEIGHT, new, 0.0);
        }
        h.pf(OP_ANIM_FADE_IN, new, fade);
    } else {
        h.pf(OP_ANIM_SET_WEIGHT, new, 1.0);
    }
    h.pi(OP_ANIM_SET_LOOP, new, i32::from(looped));
    if sync && !cur.is_null() {
        let rel = h.pq(OP_ANIM_REL_TIME, cur);
        h.pf(OP_ANIM_SET_REL_TIME, new, rel);
    } else {
        h.pf(OP_ANIM_SET_TIME, new, 0.0);
    }
    b.cur_anim = new;
    b.anim_speed_dependant = depends_on_speed;
    b.anim_speed_mul = speed_mul;
    b.use_move_anim_when_over = use_move_when_over;
    true
}

#[expect(clippy::too_many_arguments, reason = "mirrors iLuxEnemy::PlayAnim")]
pub fn play_anim(
    b: &mut Base,
    h: &H,
    name: *mut c_void,
    looped: bool,
    fade: f32,
    depends_on_speed: bool,
    speed_mul: f32,
    sync: bool,
    override_move: bool,
    use_move_when_over: bool,
) -> bool {
    let ok = play_anim_impl(
        b,
        h,
        name,
        looped,
        fade,
        depends_on_speed,
        speed_mul,
        sync,
        use_move_when_over,
    );
    if ok {
        mover(b).override_move = override_move;
    }
    ok
}

pub fn fade_out_current_anim_impl(b: &mut Base, h: &H, fade: f32) -> bool {
    let cur = b.cur_anim;
    if cur.is_null() || h.qp(OP_ANIM_IS_LOOPING, cur) != 0 {
        return false;
    }
    h.pf(OP_ANIM_FADE_OUT, cur, fade);
    b.cur_anim = std::ptr::null_mut();
    b.use_move_anim_when_over
}

pub fn fade_out_current_anim(b: &mut Base, h: &H, fade: f32) {
    if fade_out_current_anim_impl(b, h, fade) {
        mover(b).use_move_state_animations();
    }
}

pub fn anim_absolute_time(b: &Base, h: &H, rel: f32) -> f32 {
    if b.cur_anim.is_null() {
        return 0.0;
    }
    h.pq(OP_ANIM_LENGTH, b.cur_anim) * rel
}

fn at2(a: &[[f32; 2]; 2], pose: i32, speed: i32) -> f32 {
    usize::try_from(pose)
        .ok()
        .and_then(|p| a.get(p))
        .and_then(|row| row.get(usize::try_from(speed).ok()?))
        .copied()
        .unwrap_or(0.0)
}

fn ambient_rand(b: &Base, state: i32) -> f32 {
    let at = |a: &[f32; 4]| {
        usize::try_from(state)
            .ok()
            .and_then(|i| a.get(i))
            .copied()
            .unwrap_or(0.0)
    };
    rand_rectf(at(&b.ambient_min), at(&b.ambient_max))
}

pub fn set_move_speed(b: &Base, speed: i32) {
    let c = ctx(b);
    let (p, m) = (g(c.pose), speed);
    s(c.forward_speed, at2(&g(c.def_forward_speed), p, m));
    s(c.backward_speed, at2(&g(c.def_backward_speed), p, m));
    s(c.forward_acc, at2(&g(c.def_forward_acc), p, m));
    s(c.forward_deacc, at2(&g(c.def_forward_deacc), p, m));
}

pub fn reset_properties(b: &mut Base, h: &H) {
    let c = ctx(b);
    let start = gv(c.start_position);
    let mut io = Io::new();
    io.f[..3].copy_from_slice(&[start.x, start.y, start.z]);
    h.call(OP_BODY_SET_FEET, &mut io);
    change_state(b, STATE_IDLE);
    b.causes_sanity = b.causes_sanity_default;
    s(c.disabled, false);
    s(c.look_count, 0.0);
    s(c.can_see, false);
    s(c.los_count, 0);
    s(c.detected, false);
    s(c.in_range, false);
    s(c.check_door_count, 0.0);
    s(c.stuck_at_door, false);
    b.glow_alpha = 0.0;
    b.glow_update_count = 0.0;
    s(c.fov_mul, 1.0);
    s(c.damage_count, 0.0);
    sv(c.last_known, z3());
    s(c.last_pos_count, 0.0);
    s(c.cur_patrol, 0);
    s(c.attack_hit_counter, 0);
    s(c.is_seen_by_player, false);
    s(c.skip_visibility_handicaps, false);
    set_move_speed(b, MOVE_SPEED_WALK);
    h.q(OP_ON_RESET_PROPERTIES);
}

pub fn change_sound_state(b: &mut Base, h: &H, state: i32) {
    if b.sound_state == state {
        return;
    }
    b.sound_state = state;
    if !b.cur_sound.is_null() {
        let mut io = Io::new();
        io.p[0] = b.cur_sound;
        io.i[0] = b.cur_sound_id;
        if h.call(OP_SOUND_EXISTS, &mut io) != 0 {
            h.pf(OP_SOUND_FADE_OUT, b.cur_sound, 3.0);
            b.cur_sound = std::ptr::null_mut();
        }
    }
    b.ambient_count = ambient_rand(b, state);
}

pub fn update_sound_state(b: &mut Base, h: &H, dt: f32) {
    let st = b.sound_state;
    if st == SOUND_SILENT || h.qi(OP_AMBIENT_EMPTY, st) != 0 {
        return;
    }
    if !b.cur_sound.is_null() {
        let mut io = Io::new();
        io.p[0] = b.cur_sound;
        io.i[0] = b.cur_sound_id;
        if h.call(OP_SOUND_EXISTS, &mut io) != 0 {
            return;
        }
    }
    b.cur_sound = std::ptr::null_mut();
    if b.ambient_count <= 0.0 {
        let mut io = Io::new();
        io.i[0] = st;
        h.call(OP_SOUND_CREATE, &mut io);
        b.cur_sound = io.p[1];
        if !b.cur_sound.is_null() {
            b.cur_sound_id = h.qp(OP_SOUND_ID, b.cur_sound);
        }
        b.ambient_count = ambient_rand(b, st);
    } else {
        b.ambient_count -= dt;
    }
}

pub fn update_animation(b: &mut Base, h: &H, dt: f32) {
    if !b.use_animations {
        return;
    }
    let cur = b.cur_anim;
    if !mover(b).override_move || cur.is_null() {
        return;
    }
    if h.pq(OP_ANIM_PREV_TIME, cur) <= h.pq(OP_ANIM_EVENT_TIME, cur)
        && h.pq(OP_ANIM_TIME, cur) > h.pq(OP_ANIM_EVENT_TIME, cur)
    {
        send(b, h, MSG_ANIMATION_SPECIAL_EVENT, z3(), 0.0, 0);
    }
    if h.qp(OP_ANIM_IS_OVER, cur) != 0 {
        if b.use_move_anim_when_over {
            mover(b).use_move_state_animations();
        }
        send(b, h, MSG_ANIMATION_OVER, z3(), 0.0, 0);
    }
    if b.anim_speed_dependant {
        let mut via = Ga3Via(h);
        let host: &mut dyn super::host::Host = &mut via;
        let mut speed = len3(host.body_velocity(dt));
        if host.body_move_speed_fwd() < 0.0 {
            speed = -speed;
        }
        h.pf(OP_ANIM_SET_SPEED, cur, speed.abs() * b.move_speed_anim_mul);
    }
}

pub fn update_darkness_glow(b: &mut Base, h: &H, dt: f32) {
    if b.glow_alpha != b.glow_goal {
        if b.glow_alpha > b.glow_goal {
            b.glow_alpha -= dt;
            if b.glow_alpha < b.glow_goal {
                b.glow_alpha = b.glow_goal;
            }
        } else {
            b.glow_alpha += dt;
            if b.glow_alpha > b.glow_goal {
                b.glow_alpha = b.glow_goal;
            }
        }
    }
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    if b.glow_alpha > 0.0 && h.q(OP_CAM_COLLIDE_MESH) != 0 {
        let dist_sqr_ = dist_sqr(h.q3(OP_CAM_POS), host.body_pos());
        let max = b.glow_max_dist;
        if dist_sqr_ < max * max {
            let dist = dist_sqr_.sqrt();
            let mut mul = 1.0f32;
            if dist > max * 0.5 {
                mul = 1.0 - ((dist - max * 0.5) / (max * 0.5));
                if mul < 0.0 {
                    mul = 0.0;
                }
            }
            let sanity = (h.qf(OP_PLAYER_SANITY) / 100.0).sqrt();
            let alpha = b.glow_alpha * b.glow_alpha * mul * sanity;
            for i in 0..h.q(OP_SUBMESH_NUM) {
                let mut io = Io::new();
                io.i[0] = i;
                io.f[0] = alpha;
                h.call(OP_ADD_GLOW, &mut io);
            }
        }
    }
    if h.q(OP_MESH_VISIBLE) == 0 {
        return;
    }
    if b.glow_update_count > 0.0 {
        b.glow_update_count -= dt;
    }
    b.glow_update_count = 0.3;
    let (pos, feet) = (host.body_pos(), host.body_feet());
    let mut goal = h.at(OP_LIGHT_LEVEL_AT, pos).1.f[0];
    goal += h
        .at(
            OP_LIGHT_LEVEL_AT,
            v3(feet.x + 0.0, feet.y + 0.1, feet.z + 0.0),
        )
        .1
        .f[0];
    goal /= 2.0;
    goal *= 2.0;
    if goal > 1.0 {
        goal = 1.0;
    }
    b.glow_goal = 1.0 - goal;
}

pub fn update_hallucination(b: &mut Base, h: &H) {
    let c = ctx(b);
    if !b.hallucination || g(c.health) <= 0.0 {
        return;
    }
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    let d = dist_sqr(host.body_pos(), host.player_pos());
    if d < b.hallucination_end_dist * b.hallucination_end_dist {
        h.qi(OP_FADE_TO_SMOKE, 1);
    }
}

pub fn update_align_ground_ray(b: &mut Base, h: &H, dt: f32) {
    if !b.align_ground_ray {
        return;
    }
    b.ground_ray_count -= dt;
    if b.ground_ray_count > 0.0 {
        return;
    }
    b.ground_ray_count = 0.02;
    let start_add = 0.05f32;
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    let dir = matrix_mul_vec(&rot_y(host.body_yaw()), v3(0.0, 0.0, -1.0));
    let (feet, size) = (host.body_feet(), host.body_size());
    let sx = size.x;
    let start = v3(
        feet.x + 0.0 + dir.x * sx * 0.5,
        feet.y + start_add + dir.y * sx * 0.5,
        feet.z + 0.0 + dir.z * sx * 0.5,
    );
    let (hit, io) = h.at(OP_GROUND_RAY, start);
    if hit == 0 {
        return;
    }
    let dist = io.f[3] - start_add;
    if let Some(slot) = usize::try_from(b.ground_n)
        .ok()
        .and_then(|n| b.ground.get_mut(n))
    {
        *slot = dist;
        b.ground_n += 1;
    }
    if b.ground_n > 3 {
        b.ground.copy_within(1..4, 0);
        b.ground_n -= 1;
    }
    let mut total = 0.0f32;
    for d in b.ground.iter().take(b.ground_n as usize) {
        total += *d;
    }
    total /= b.ground_n as f32;
    let mut io = Io::new();
    io.f[0] = total;
    h.call(OP_SET_ENTITY_OFFSET, &mut io);
}

pub fn attack(b: &Base, h: &H, size: *mut c_void, damage: *mut c_void, mul: f32) -> bool {
    let mut io = Io::new();
    io.p[0] = size;
    io.p[1] = damage;
    io.f[0] = mul;
    let r = h.call(OP_ATTACK_SHAPE, &mut io);
    if r & 1 != 0 {
        let c = ctx(b);
        s(c.attack_hit_counter, g(c.attack_hit_counter) + 1);
        h.qp(OP_ATTACK_HIT_SOUND, damage);
    }
    r & 2 != 0
}

pub fn damage_shape_matrix(h: &H, offset: Vector3f) -> Matrixf {
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    let pos = host.body_pos();
    let yaw = host.body_yaw();
    let mut io = Io::new();
    h.call(OP_BODY_RIGHT_UP_FWD, &mut io);
    let [rx, ry, rz, ux, uy, uz, fx, fy, fz, ..] = io.f;
    let at = |r: f32, u: f32, f: f32, base: f32| {
        fma(f, offset.z, fma(u, offset.y, fma(r, offset.x, base)))
    };
    let mut m = matrix_rotate(v3(0.0, yaw, 0.0), 0);
    let [r0, r1, r2, ..] = &mut m.m;
    r0[3] = at(rx, ux, fx, pos.x);
    r1[3] = at(ry, uy, fy, pos.y);
    r2[3] = at(rz, uz, fz, pos.z);
    m
}

pub fn closest_food(h: &H, max_dist: f32, max_height_dist: f32) -> *mut c_void {
    let mut out: *mut c_void = std::ptr::null_mut();
    let mut shortest = -1.0f32;
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    for i in 0..h.q(OP_FOOD_BEGIN) {
        if h.qi(OP_FOOD_IS_OBJECT, i) == 0 {
            continue;
        }
        let mut io = Io::new();
        io.i[0] = i;
        if h.call(OP_FOOD_IS_FOOD, &mut io) == 0 {
            continue;
        }
        let food = io.p[1];
        let mut io = Io::new();
        io.i[0] = i;
        h.call(OP_FOOD_LOWEST_Y, &mut io);
        if (io.f[0] - host.body_feet().y).abs() > max_height_dist {
            continue;
        }
        let mut io = Io::new();
        io.i[0] = i;
        h.call(OP_FOOD_POS, &mut io);
        let d = dist_sqr(host.body_pos(), v3(io.f[0], io.f[1], io.f[2]));
        if d > max_dist * max_dist {
            continue;
        }
        if shortest < 0.0 || d < shortest {
            shortest = d;
            out = food;
        }
    }
    out
}

fn flat(v: Vector3f) -> Vector3f {
    v3(v.x, 0.0, v.z)
}

pub fn dist_2d(h: &H, pos: Vector3f) -> f32 {
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    dist3(flat(host.body_feet()), flat(pos))
}

pub fn dist_to_char(h: &H, feet: Vector3f) -> f32 {
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    dist3(host.body_feet(), feet)
}

pub fn abs_height_dist(h: &H, feet: Vector3f) -> f32 {
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    (host.body_feet().y - feet.y).abs()
}

pub fn abs_height_dist_to_player(h: &H) -> f32 {
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    if host.player_dead() {
        return 100_000.0;
    }
    let feet = host.player_feet();
    (host.body_feet().y - feet.y).abs()
}

pub fn direction_2d(h: &H, pos: Vector3f) -> Vector3f {
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    let bp = host.body_pos();
    normalized(v3(pos.x - bp.x, 0.0, pos.z - bp.z))
}

pub fn is_seen_by_player(b: &Base, h: &H) -> bool {
    if h.q(OP_FRUSTUM_BODY) == 0 {
        return false;
    }
    let eye = h.q3(OP_CAM_POS);
    let mut via = Ga3Via(h);
    let mut env = Env::new(ctx(b), &mut via);
    let (pos, size) = (env.h.body_pos(), env.h.body_size());
    env.line_of_sight(pos, size, false, Some(eye))
}

pub fn in_player_fov_at_feet(h: &H, feet: Vector3f) -> bool {
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    let size = host.body_size();
    let center = v3(feet.x + 0.0, feet.y + size.y / 2.0, feet.z + 0.0);
    h.at(OP_FRUSTUM_BODY_AT, center).0 != 0
}

pub fn visible_to_player_at_feet(b: &Base, h: &H, feet: Vector3f) -> bool {
    let eye = h.q3(OP_CAM_POS);
    let mut via = Ga3Via(h);
    let mut env = Env::new(ctx(b), &mut via);
    let size = env.h.body_size();
    let pos = v3(feet.x, feet.y + size.y / 2.0, feet.z);
    env.line_of_sight(pos, size, false, Some(eye))
}

pub fn outside_start_radius(b: &Base, h: &H) -> bool {
    let c = ctx(b);
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    let feet = host.body_feet();
    dist3(feet, gv(c.start_position)) > fma(g(c.activation_distance), 1.2, 2.0)
}

pub fn on_set_active(b: &mut Base, h: &H, active: bool) {
    let mut via = Ga3Via(h);
    let host: &mut dyn super::host::Host = &mut via;
    if h.q(OP_BODY_EXISTS) != 0 {
        host.body_set_active(active);
    }
    if h.q(OP_MESH_EXISTS) != 0 {
        h.qi(OP_MESH_SET_ACTIVE, i32::from(active));
    }
    if !active {
        host.music(false, MUSIC_ATTACK);
        host.music(false, MUSIC_SEARCH);
        host.player_remove_terror_enemy();
        change_state(b, STATE_IDLE);
    }
    h.qi(OP_ON_SET_ACTIVE_SPECIFIC, i32::from(active));
}

pub fn init(b: &mut Base, c: *mut Ctx, kind: i32) {
    b.c = c;
    b.kind = Kind::from_i32(kind).map_or(-1, |k| k as i32);
}
