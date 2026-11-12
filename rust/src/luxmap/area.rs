//! `LuxArea*.cpp`: per-area update and interaction decisions.

// The negated comparisons keep the C++ NaN behaviour.
#![allow(clippy::neg_cmp_op_on_partial_ord)]

use crate::math::{
    Matrixf, Planef, Vector2f, Vector3f, angle_from_points_2d, dot3, fma, matrix_inverse,
    matrix_mul_vec, rand_rectf, sin, v3,
};
use crate::string;

use crate::math::matrix_slerp;

/// The `afTimeStep < GetStepSize()*0.8f` guard of the areas that only update on real game steps.
pub fn step_too_small(step: f32, engine_step: f32) -> bool {
    step < engine_step * 0.8
}

/// Liquid and sticky areas skip bodies that cannot take part.
pub fn body_skipped(collide: bool, active: bool, mass: f32, is_char: bool) -> bool {
    !collide || !active || (mass == 0.0 && !is_char)
}

pub fn liquid_surface_y(pos_y: f32, size_y: f32) -> f32 {
    pos_y + size_y / 2.0
}

fn up_plane(p: Vector3f) -> Planef {
    let n = v3(0.0, 1.0, 0.0);
    Planef {
        a: n.x,
        b: n.y,
        c: n.z,
        d: -dot3(n, p),
    }
}

pub fn liquid_surface_plane(pos_y: f32, size_y: f32) -> Planef {
    up_plane(v3(0.0 + 0.0, pos_y + size_y / 2.0, 0.0 + 0.0))
}

pub fn wave_plane(surface_y: f32) -> Planef {
    up_plane(v3(0.0, surface_y, 0.0))
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct BuoyIn {
    pub inside: bool,
    pub allowed: bool,
    pub active: bool,
    pub has_waves: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct BuoyOut {
    pub deactivate: bool,
    /// Set surface, density and viscosities, splash, activate.
    pub setup: bool,
    pub waves: bool,
}

pub fn liquid_body(i: BuoyIn, dist_sqr: f32, max_wave_dist_sqr: f32) -> BuoyOut {
    if !i.inside || !i.allowed {
        return BuoyOut {
            deactivate: true,
            ..BuoyOut::default()
        };
    }
    BuoyOut {
        deactivate: false,
        setup: !i.active,
        waves: i.has_waves && dist_sqr < max_wave_dist_sqr,
    }
}

pub fn wave_surface_y(time: f32, freq: f32, px: f32, pz: f32, amp: f32, surface_y: f32) -> f32 {
    let wave_x = sin(fma(time, freq, px * 15.0)) * amp;
    let wave_z = sin(fma(time, freq, pz * 15.0)) * amp;
    fma(wave_z, 0.3, fma(wave_x, 0.3, surface_y))
}

pub const CHAR_NONE: i32 = 0;
pub const CHAR_ENTER: i32 = 1;
pub const CHAR_LEAVE: i32 = 2;

/// Player: enter once, leave every update while outside. Enemy: same, but only with an owner.
pub fn liquid_char(is_player: bool, has_enemy: bool, inside: bool, in_water: bool) -> i32 {
    if !is_player && !has_enemy {
        return CHAR_NONE;
    }
    match (inside, in_water) {
        (true, false) => CHAR_ENTER,
        (true, true) => CHAR_NONE,
        (false, _) => CHAR_LEAVE,
    }
}

pub const INSIDE_PROCEED: i32 = 0;
pub const INSIDE_SKIP: i32 = 1;
/// The last attached body left the area: forget it, then skip.
pub const INSIDE_FORGET_LAST: i32 = 2;

pub fn sticky_inside(inside: bool, is_last: bool) -> i32 {
    match (inside, is_last) {
        (false, true) => INSIDE_FORGET_LAST,
        (false, false) | (true, true) => INSIDE_SKIP,
        (true, false) => INSIDE_PROCEED,
    }
}

/// `(skip, allow_attachment)` after the attachable-name check (`allow` starts false).
pub fn sticky_name(attach_name: &[u8], body_name: &[u8]) -> (bool, bool) {
    if attach_name.is_empty() {
        return (false, false);
    }
    if string::first_pos(body_name, attach_name) < 0 {
        return (true, false);
    }
    (false, true)
}

/// The attach script ran (if any); the body is skipped when the static flag is still false.
pub fn sticky_after_callback(has_callback: bool, allow: bool) -> bool {
    has_callback && !allow
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct StickyAttachIn {
    pub start: Matrixf,
    pub area_world: Matrixf,
    pub area_pos: Vector3f,
    pub mass_centre: Vector3f,
    pub attached_pos: Vector3f,
    pub pose_time: f32,
    pub step: f32,
    pub set_time: f32,
    pub move_body: bool,
    pub rotate_body: bool,
}

pub const ATTACH_NONE: i32 = 0;
pub const ATTACH_SET_MATRIX: i32 = 1;
pub const ATTACH_SET_POSITION: i32 = 2;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct StickyAttachOut {
    pub mtx: Matrixf,
    pub pos: Vector3f,
    pub set_time: f32,
    pub mode: i32,
    pub done: bool,
}

fn advance(i: &StickyAttachIn) -> f32 {
    if i.pose_time == 0.0 {
        1.0
    } else {
        i.set_time + i.step / i.pose_time
    }
}

fn set_translation(m: &mut Matrixf, t: Vector3f) {
    m.m[0][3] = t.x;
    m.m[1][3] = t.y;
    m.m[2][3] = t.z;
}

pub fn sticky_attach(i: &StickyAttachIn) -> StickyAttachOut {
    let mut o = StickyAttachOut {
        set_time: i.set_time,
        ..StickyAttachOut::default()
    };
    let goal_pos = v3(
        i.area_pos.x - i.mass_centre.x,
        i.area_pos.y - i.mass_centre.y,
        i.area_pos.z - i.mass_centre.z,
    );
    if i.move_body && i.rotate_body {
        o.set_time = advance(i);
        let mut goal = i.area_world;
        set_translation(&mut goal, goal_pos);
        o.mtx = matrix_slerp(o.set_time, &i.start, &goal);
        o.mode = ATTACH_SET_MATRIX;
    } else if i.move_body {
        o.set_time = advance(i);
        let s = v3(i.start.m[0][3], i.start.m[1][3], i.start.m[2][3]);
        let k = 1.0 - o.set_time;
        let t = o.set_time;
        o.pos = v3(
            fma(s.x, k, goal_pos.x * t),
            fma(s.y, k, goal_pos.y * t),
            fma(s.z, k, goal_pos.z * t),
        );
        o.mode = ATTACH_SET_POSITION;
    } else if i.rotate_body {
        o.set_time = advance(i);
        o.mtx = matrix_slerp(o.set_time, &i.start, &i.area_world);
        set_translation(&mut o.mtx, i.attached_pos);
        o.mode = ATTACH_SET_MATRIX;
    } else {
        o.set_time = 1.0;
    }
    o.done = o.set_time >= 1.0;
    o
}

/// `count -= step`; true once it has run out (a NaN count has) (the caller then does its check).
pub fn tick(count: &mut f32, step: f32) -> bool {
    *count -= step;
    !(*count > 0.0)
}

/// Slime and insanity areas: tick, then draw the next interval.
pub fn tick_random(count: &mut f32, step: f32, min: f32, max: f32) -> bool {
    if !tick(count, step) {
        return false;
    }
    *count = rand_rectf(min, max);
    true
}

pub fn tick_fixed(count: &mut f32, step: f32, next: f32) -> bool {
    if !tick(count, step) {
        return false;
    }
    *count = next;
    true
}

pub fn examine_tick(count: &mut f32, step: f32) {
    if *count > 0.0 {
        *count -= step;
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ExamineIn {
    pub has_cat: bool,
    pub has_insane_cat: bool,
    pub has_insane_entry: bool,
    pub has_sound: bool,
    pub has_insane_sound: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ExamineOut {
    pub show_text: bool,
    pub insane_text: bool,
    pub play_sound: bool,
    pub insane_sound: bool,
}

pub fn examine_interact(
    i: ExamineIn,
    sanity: f32,
    insane_limit: f32,
    sound_count: f32,
) -> ExamineOut {
    let insane = sanity < insane_limit;
    ExamineOut {
        show_text: i.has_cat,
        insane_text: i.has_cat && insane && i.has_insane_cat && i.has_insane_entry,
        play_sound: sound_count <= 0.0 && i.has_sound,
        insane_sound: insane && i.has_insane_sound,
    }
}

pub fn script_crosshair(has_callback: bool, custom: i32, default: i32, grab: i32) -> i32 {
    if !has_callback {
        default
    } else if custom == default {
        grab
    } else {
        custom
    }
}

pub fn script_can_interact(item_interaction: bool, has_callback: bool) -> bool {
    item_interaction || has_callback
}

pub fn follow_offset(offset: Vector3f, parent: Vector3f, body: Vector3f) -> Vector3f {
    v3(
        offset.x + parent.x - body.x,
        offset.y + parent.y - body.y,
        offset.z + parent.z - body.z,
    )
}

pub fn attach_offset(body: Vector3f, parent: Vector3f) -> Vector3f {
    v3(body.x - parent.x, body.y - parent.y, body.z - parent.z)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LadderSetup {
    pub forward: Vector3f,
    pub max_y: f32,
    pub min_y: f32,
}

pub fn ladder_setup(world: &Matrixf, pos_y: f32, size_y: f32) -> LadderSetup {
    let inv = matrix_inverse(world);
    LadderSetup {
        forward: v3(inv.m[2][0], inv.m[2][1], inv.m[2][2]),
        max_y: pos_y + size_y / 2.0,
        min_y: pos_y - size_y / 2.0,
    }
}

pub fn ladder_start_pos(
    area_pos: Vector3f,
    forward: Vector3f,
    char_size: Vector3f,
    char_y: f32,
    max_y: f32,
    min_y: f32,
) -> Vector3f {
    let f = |a: f32, p: f32| fma(a * char_size.x, 0.6, p);
    let mut v = v3(
        f(forward.x, area_pos.x),
        f(forward.y, area_pos.y),
        f(forward.z, area_pos.z),
    );
    v.y = char_y + 0.05;
    let top = fma(-char_size.y, 0.3, max_y);
    if v.y > top {
        v.y = top;
    }
    if v.y - char_size.y / 2.0 < min_y {
        v.y = min_y + char_size.y / 2.0 + 0.1;
    }
    v
}

/// Tries ever higher start positions until `fits`; a hit after the first try still moves one step
/// up (kept).
pub fn ladder_find_start(
    mut start: Vector3f,
    max_y: f32,
    fits: &mut dyn FnMut(Vector3f) -> bool,
) -> Option<Vector3f> {
    let up = |v: &mut Vector3f| {
        v.x += 0.0;
        v.y += 0.1;
        v.z += 0.0;
    };
    let mut first = true;
    loop {
        if fits(start) {
            if !first {
                up(&mut start);
            }
            return Some(start);
        }
        first = false;
        up(&mut start);
        if !(start.y <= max_y - 0.2) {
            return None;
        }
    }
}

pub const ROPE_RETURN: i32 = 0;
pub const ROPE_RELINK: i32 = 1;
pub const ROPE_CREATE: i32 = 2;

pub fn rope_after_world(created: bool, nothing_linked: bool) -> i32 {
    match (created, nothing_linked) {
        (true, true) => ROPE_RELINK,
        (true, false) => ROPE_RETURN,
        (false, _) => ROPE_CREATE,
    }
}

/// `(int)(max / segment) + 4`: float-to-int saturates and the add wraps on aarch64.
pub fn rope_max_segments(max_len: f32, segment: f32) -> i32 {
    ((max_len / segment) as i32).wrapping_add(4)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct PlayerStart {
    pub pos: Vector3f,
    pub angle: f32,
}

pub fn player_start(m: &Matrixf) -> PlayerStart {
    let mut rot = *m;
    for row in &mut rot.m[..3] {
        row[3] = 0.0;
    }
    let fwd = matrix_mul_vec(&rot, v3(0.0, 0.0, 1.0));
    PlayerStart {
        pos: node_up(v3(m.m[0][3], m.m[1][3], m.m[2][3])),
        angle: -angle_from_points_2d(Vector2f { x: 0.0, y: 0.0 }, Vector2f { x: fwd.x, y: fwd.z }),
    }
}

/// Player start and path nodes sit 5 cm above the area.
pub fn node_up(t: Vector3f) -> Vector3f {
    v3(t.x + 0.0, t.y + 0.05, t.z + 0.0)
}

/// `"<func>(\"<name>\")"`.
pub fn call1(func: &[u8], name: &[u8]) -> Vec<u8> {
    let mut v = func.to_vec();
    v.extend_from_slice(b"(\"");
    v.extend_from_slice(name);
    v.extend_from_slice(b"\")");
    v
}

/// `"<func>(\"<a>\",\"<b>\")"`, the sticky-area callback form.
pub fn call2(func: &[u8], a: &[u8], b: &[u8]) -> Vec<u8> {
    let mut v = func.to_vec();
    v.extend_from_slice(b"(\"");
    v.extend_from_slice(a);
    v.extend_from_slice(b"\",\"");
    v.extend_from_slice(b);
    v.extend_from_slice(b"\")");
    v
}
