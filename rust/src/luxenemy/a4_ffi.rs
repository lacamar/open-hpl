//! C ABI of the A4 enemy logic, mirrored in `amnesia/src/game/RustLuxEnemyA4.h`.
#![allow(
    clippy::missing_safety_doc,
    reason = "every entry shares the contract below"
)]

use super::a4_enemy as e;
use super::a4_state::{Base, H, Io, Table};
use super::ctx::Ctx;
use crate::math::{Matrixf, Vector3f, v3};
use std::ffi::{c_char, c_void};

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<Base>() == 136);
    assert!(size_of::<Io>() == 88);
};

fn name(p: *const c_char) -> *mut c_void {
    p.cast_mut().cast()
}

unsafe fn vec3(p: *const f32) -> Vector3f {
    // SAFETY: `p` points at three floats.
    unsafe { v3(*p, *p.add(1), *p.add(2)) }
}

unsafe fn put3(p: *mut f32, v: Vector3f) {
    // SAFETY: `p` points at three writable floats.
    unsafe {
        *p = v.x;
        *p.add(1) = v.y;
        *p.add(2) = v.z;
    }
}

/// # Safety
/// `b` is the enemy's POD, `c` its `OhplGa3Ctx`, `h` a live table; applies to every entry below.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a4_init(b: *mut Base, c: *mut Ctx, kind: i32) {
    // SAFETY: caller contract.
    unsafe { e::init(&mut *b, c, kind) };
}

macro_rules! entry {
    ($(fn $n:ident($b:ident, $h:ident $(, $a:ident : $t:ty)*) $(-> $r:ty)? $body:block)*) => {$(
        /// # Safety
        /// See `ohpl_a4_init`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $n($b: *mut Base, $h: *const Table $(, $a: $t)*) $(-> $r)? {
            // SAFETY: caller contract.
            let ($b, $h) = unsafe { (&mut *$b, H(&*$h)) };
            $body
        }
    )*};
}

entry! {
    fn ohpl_a4_on_update(b, h, dt: f32) { e::on_update(b, &h, dt) }
    fn ohpl_a4_give_damage(b, h, amount: f32, strength: i32) { e::give_damage(b, &h, amount, strength) }
    fn ohpl_a4_alert_of_player_presence(b, h) { e::alert_of_player_presence(b, &h) }
    fn ohpl_a4_show_player_position(b, h) { e::show_player_position(b, &h) }
    fn ohpl_a4_play_anim(b, h, n: *const c_char, looped: bool, fade: f32, speed_dep: bool, speed_mul: f32, sync: bool, override_move: bool, use_move: bool) -> bool {
        e::play_anim(b, &h, name(n), looped, fade, speed_dep, speed_mul, sync, override_move, use_move)
    }
    fn ohpl_a4_play_anim_impl(b, h, n: *const c_char, looped: bool, fade: f32, speed_dep: bool, speed_mul: f32, sync: bool, use_move: bool) -> bool {
        e::play_anim_impl(b, &h, name(n), looped, fade, speed_dep, speed_mul, sync, use_move)
    }
    fn ohpl_a4_fade_out_current_anim(b, h, fade: f32) { e::fade_out_current_anim(b, &h, fade) }
    fn ohpl_a4_fade_out_current_anim_impl(b, h, fade: f32) -> bool { e::fade_out_current_anim_impl(b, &h, fade) }
    fn ohpl_a4_anim_absolute_time(b, h, rel: f32) -> f32 { e::anim_absolute_time(b, &h, rel) }
    fn ohpl_a4_reset_properties(b, h) { e::reset_properties(b, &h) }
    fn ohpl_a4_change_sound_state(b, h, state: i32) { e::change_sound_state(b, &h, state) }
    fn ohpl_a4_update_sound_state(b, h, dt: f32) { e::update_sound_state(b, &h, dt) }
    fn ohpl_a4_update_animation(b, h, dt: f32) { e::update_animation(b, &h, dt) }
    fn ohpl_a4_update_darkness_glow(b, h, dt: f32) { e::update_darkness_glow(b, &h, dt) }
    fn ohpl_a4_update_hallucination(b, h) { e::update_hallucination(b, &h) }
    fn ohpl_a4_update_align_ground_ray(b, h, dt: f32) { e::update_align_ground_ray(b, &h, dt) }
    fn ohpl_a4_attack(b, h, size: *const c_void, damage: *const c_void, mul: f32) -> bool {
        e::attack(b, &h, size.cast_mut(), damage.cast_mut(), mul)
    }
    fn ohpl_a4_damage_shape_matrix(b, h, offset: *const f32, out: *mut Matrixf) {
        let _ = b;
        // SAFETY: caller contract; `offset` is a vector and `out` a matrix.
        unsafe { *out = e::damage_shape_matrix(&h, vec3(offset)) }
    }
    fn ohpl_a4_closest_food(b, h, max_dist: f32, max_height: f32) -> *mut c_void {
        let _ = b;
        e::closest_food(&h, max_dist, max_height)
    }
    fn ohpl_a4_dist_2d(b, h, pos: *const f32) -> f32 {
        let _ = b;
        // SAFETY: caller contract.
        e::dist_2d(&h, unsafe { vec3(pos) })
    }
    fn ohpl_a4_dist_to_char(b, h, feet: *const f32) -> f32 {
        let _ = b;
        // SAFETY: caller contract.
        e::dist_to_char(&h, unsafe { vec3(feet) })
    }
    fn ohpl_a4_dist_to_char_2d(b, h, feet: *const f32) -> f32 {
        let _ = b;
        // SAFETY: caller contract.
        e::dist_2d(&h, unsafe { vec3(feet) })
    }
    fn ohpl_a4_abs_height_dist(b, h, feet: *const f32) -> f32 {
        let _ = b;
        // SAFETY: caller contract.
        e::abs_height_dist(&h, unsafe { vec3(feet) })
    }
    fn ohpl_a4_abs_height_dist_to_player(b, h) -> f32 {
        let _ = b;
        e::abs_height_dist_to_player(&h)
    }
    fn ohpl_a4_direction_2d(b, h, pos: *const f32, out: *mut f32) {
        let _ = b;
        // SAFETY: caller contract.
        unsafe { put3(out, e::direction_2d(&h, vec3(pos))) }
    }
    fn ohpl_a4_is_seen_by_player(b, h) -> bool { e::is_seen_by_player(b, &h) }
    fn ohpl_a4_in_player_fov_at_feet(b, h, feet: *const f32) -> bool {
        let _ = b;
        // SAFETY: caller contract.
        e::in_player_fov_at_feet(&h, unsafe { vec3(feet) })
    }
    fn ohpl_a4_visible_to_player_at_feet(b, h, feet: *const f32) -> bool {
        // SAFETY: caller contract.
        e::visible_to_player_at_feet(b, &h, unsafe { vec3(feet) })
    }
    fn ohpl_a4_outside_start_radius(b, h) -> bool { e::outside_start_radius(b, &h) }
    fn ohpl_a4_on_set_active(b, h, active: bool) { e::on_set_active(b, &h, active) }
}

/// # Safety
/// See `ohpl_a4_init`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a4_change_state(b: *mut Base, state: i32) {
    // SAFETY: caller contract.
    unsafe { e::change_state(&*b, state) };
}

/// # Safety
/// See `ohpl_a4_init`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a4_set_move_speed(b: *mut Base, speed: i32) {
    // SAFETY: caller contract.
    unsafe { e::set_move_speed(&*b, speed) };
}
